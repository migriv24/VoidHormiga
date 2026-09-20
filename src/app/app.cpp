/* app.cpp — the Hormiga SHELL, platform-free.
 *
 * The workspace is SECTIONS over one core (okf/concepts/sections/workspace-and-sections.md):
 *   Data    — kind sidebar → rune list → detail form: the management UI,
 *             built on the widget protocol (voidmaiz/widget.hpp, drafted for
 *             us 2026-07-16). The connections canvas is a toggle away.
 *   Builder — categorized palette + the block canvas + inspector + an HTML
 *             preview (EN/ES) through `effect render` — phase D's seam, live
 *             early so the builder is usable, not just technically present.
 *   Antfarm — placeholder cards until the holiday registry exists (phase C).
 * One dispatcher, one undo history, one log strip + command bar under all of
 * it. Every widget's edits compile to commands — the CLI stays complete.
 */

#include "app/app_internal.hpp" // the shared includes, helpers and structs
#include "app/lan_share.hpp"     // LanRuntime: sharing, joining, presence
#include "app/paths.hpp"        // find_key_file

// Heavy headers only the SHELL needs, kept out of app_internal.hpp so the six
// section units do not pay for them (see the note there — this is what the
// 65,535-section ceiling was actually made of).
#include "domain/date_query.hpp"     // date: predicates in the block grammar
#include "domain/seed.hpp"           // the shipped demo transcript (pulls nlohmann/json)
#include "domain/rescue_import.hpp"  // recovering a damaged org file (ditto)
#include "domain/allomone_legacy.hpp" // the frozen dialect: still DERIVES, never shipped as a tab
#include "sodium.h"           // sodium_memzero - wipe passphrase buffers after use
#include "voidmaiz/headless.hpp" // Session: to CLAIM THE FLOOR (see g_floor)
#define STB_IMAGE_WRITE_IMPLEMENTATION // the ONE unit that compiles the writer
#include "stb_image_write.h"  // vendored, public domain (PNG writer, no GL)


// ── host seams on the core (survive core replacement) ───────────────────────

/* ── THE FLOOR: a person outranks an agent ───────────────────────────────────
 *
 * Void Maiz's headless step 5, adopted 2026-08-18. `voidhormiga-cli` can drive
 * this same document from a terminal, so two processes can now want it at once,
 * and the author's rule is that the person wins: *"a person should always have
 * more power than an agent, even in the headless mode."*
 *
 * The mechanism is one flag. A `SessionKind::Human` session may take the
 * advisory lock from an Agent session; nothing else preempts anything. So the
 * GUI holds a Human session for its whole life, purely as a claim — an agent
 * starting afterwards is refused, and an agent already running loses the floor,
 * fails its next command and drops its unsaved work rather than overwriting a
 * document somebody is editing right now.
 *
 * IT OWNS A THROWAWAY CORE AND WE IGNORE IT. `Session` loads the state document
 * into its own core; ours is loaded separately below. That duplicated parse at
 * boot is the price of not restructuring the GUI around a Session — which would
 * mean making `HormigaApp::core` an alias, and it is reassigned in three places
 * (new / open / init), so it cannot be one.
 *
 * NEVER FATAL. If the lock cannot be taken — a stale file, a read-only
 * directory, another window already open — we carry on unlocked. Being unable
 * to claim the floor is a reason to warn, never a reason to refuse somebody
 * their own application. */
static std::unique_ptr<maiz::Session> g_floor;

static bool claim_the_floor(const fs::path& state_file) {
    maiz::HostApp claim;           // deliberately bare: no glyphs, no effects —
    claim.id = "hormiga";          // this session exists only to hold a lock.
    claim.label = "Void Hormiga (window)";
    maiz::SessionOptions o;
    o.state_path = state_file.string();
    o.kind = maiz::SessionKind::Human;  // <- the whole point
    o.actor = "human:hormiga";
    o.save_on_close = false;           // the GUI persists through its own path
    o.journal = false;                 // ...and keeps its own log
    auto s = std::make_unique<maiz::Session>(claim, o);
    if (!s->start()) return false;
    g_floor = std::move(s);
    return true;
}

fs::path HormigaApp::org_file() const { return base_dir / state_name; }
/* The SQLite mirror is named after the document it mirrors. It was the literal
 * "demo-org.db", so pointing the app at `org_01.state.json` would have mirrored
 * it into a file named after a different database — two documents, one mirror,
 * and the mirror silently belonging to whichever ran last. It is a DERIVED
 * artifact (`effect save` regenerates it), so renaming it costs nothing and the
 * default is unchanged: demo-org.json -> demo-org.db. */
fs::path HormigaApp::db_file() const {
    return base_dir / (fs::path(state_name).stem().string() + ".db");
}
fs::path HormigaApp::assets_dir() const { return data_dir("assets"); }
fs::path HormigaApp::vault_path() const { return base_dir / "org.miga"; }

/* Populate secrets from the unlocked vault if present, else the plaintext
 * imgbb.key (off-by-default encryption; security.md §2). */
void HormigaApp::load_secrets() {
    if (vault.unlocked()) {
        imgbb_key = vault.get("imgbb_key");
        return;
    }
    if (const fs::path kp = hormiga::find_key_file("imgbb.key", key_dirs()); fs::exists(kp)) {
        std::ifstream kf(kp);
        std::getline(kf, imgbb_key);
        while (!imgbb_key.empty() &&
               (imgbb_key.back() == '\r' || imgbb_key.back() == ' '))
            imgbb_key.pop_back();
    }
}

/* The passphrase prompt: Unlock an existing vault, or Create one from the
 * current plaintext key. Crypto lives in the Vault; this is just the modal. */
void HormigaApp::draw_vault_modal() {
    if (vault_modal == VaultModal::None) return;
    const char* id = vault_modal == VaultModal::Unlock ? "Unlock credentials"
                                                       : "Encrypt credentials";
    ImGui::OpenPopup(id);
    ImGui::SetNextWindowSize(ImVec2(380, 0));
    if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (vault_modal == VaultModal::Unlock) {
            ImGui::TextWrapped("This org has an encrypted credential vault "
                               "(org.miga). Enter its passphrase to enable "
                               "cloud holidays (ImgBB). You can skip and work "
                               "offline.");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1);
            bool go = ImGui::InputTextWithHint("##pass", "passphrase", pass_buf,
                                               sizeof pass_buf,
                                               ImGuiInputTextFlags_Password |
                                                   ImGuiInputTextFlags_EnterReturnsTrue);
            if (!vault_msg.empty())
                ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1), "%s", vault_msg.c_str());
            if (ImGui::Button("Unlock", ImVec2(120, 0)) || go) {
                if (vault.unlock(vault_path().string(), pass_buf)) {
                    secrets_locked = false;
                    load_secrets();
                    toast("credentials unlocked");
                    sodium_memzero(pass_buf, sizeof pass_buf);
                    vault_modal = VaultModal::None;
                } else {
                    vault_msg = "wrong passphrase";
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Skip (offline)", ImVec2(140, 0))) {
                sodium_memzero(pass_buf, sizeof pass_buf);
                vault_msg.clear();
                vault_modal = VaultModal::None;
            }
        } else { // Create
            ImGui::TextWrapped("Encrypt this org's credentials under a "
                               "passphrase (argon2id + XChaCha20-Poly1305). "
                               "There is NO recovery — lose the passphrase, "
                               "lose the secrets.");
            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##p1", "new passphrase", pass_buf,
                                     sizeof pass_buf, ImGuiInputTextFlags_Password);
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##p2", "confirm passphrase", pass_buf2,
                                     sizeof pass_buf2, ImGuiInputTextFlags_Password);
            if (!vault_msg.empty())
                ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1), "%s", vault_msg.c_str());
            bool ok = std::strlen(pass_buf) >= 6 &&
                      std::strcmp(pass_buf, pass_buf2) == 0;
            ImGui::BeginDisabled(!ok);
            if (ImGui::Button("Encrypt", ImVec2(120, 0))) {
                vault.create(pass_buf);
                if (!imgbb_key.empty()) vault.set("imgbb_key", imgbb_key);
                if (vault.save(vault_path().string())) {
                    // the secret now lives encrypted — retire the plaintext
                    std::error_code ec;
                    fs::remove(base_dir / "imgbb.key", ec);
                    toast("credentials encrypted to org.miga (plaintext removed)");
                    sodium_memzero(pass_buf, sizeof pass_buf);
                    sodium_memzero(pass_buf2, sizeof pass_buf2);
                    vault_modal = VaultModal::None;
                } else {
                    vault_msg = "save failed: " + vault.error();
                }
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                sodium_memzero(pass_buf, sizeof pass_buf);
                sodium_memzero(pass_buf2, sizeof pass_buf2);
                vault_msg.clear();
                vault_modal = VaultModal::None;
            }
            if (!ok && std::strlen(pass_buf2) > 0)
                ImGui::TextDisabled("passphrases must match, min 6 chars");
        }
        ImGui::EndPopup();
    }
}

/* Every mantle's projection (the `mantles` verb enumerates; lines may carry
 * an active marker — strip decorations, keep the name). */
std::vector<maiz::Scene> HormigaApp::project_all_mantles() {
    std::vector<maiz::Scene> out;
    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' '))
            line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (line.empty() || line == "(no mantles)") continue;
        maiz::ProjectOptions po;
        po.mantle = line;
        out.push_back(maiz::project_scene(core, po));
    }
    return out;
}

void HormigaApp::install_host() {
    core.set_log_sink([this](std::string_view level, std::string_view op,
                             std::string_view msg) {
        log.push_back({std::string(level), std::string(op), std::string(msg)});
    });
    on_register_glyphs = [](maiz::Core& c) {
        hormiga::register_glyphs(c);
        hormiga::register_block_glyphs(c);
        hormiga::register_antfarm_glyphs(c);
    };
    // the effect seam: world-facing ops the core can't do itself. `save`
    // writes the exported state; `render` walks the issue chain into HTML
    // and hands the file to the OS (phase D's seam, minimally alive).
    core.set_effect_handler([this](std::string_view op, std::string_view args)
                                -> std::string {
        if (op == "save") {
            // primary: the SQLite Data holiday (state + normalized rows);
            // beside it: the JSON snapshot mirror (fallback principle)
            bool db_ok = storage && storage->ok() &&
                         storage->save(std::string(args), project_all_mantles());
            if (storage && !db_ok)
                log.push_back({"error", "save", "db: " + storage->error()});
            std::ofstream out(org_file(), std::ios::binary | std::ios::trunc);
            out << args;
            if (!out) {
                log.push_back({"error", "save", "write failed: " + org_file().string()});
                return {};
            }
            return json_str(db_ok ? db_file().string() : org_file().string());
        }
        if (op == "render") {
            std::string lang = args.find("es") != std::string_view::npos ? "es" : "en";
            std::string path = render_preview(lang);
            if (!path.empty() && on_open) on_open(path);
            return json_str(path);
        }
        if (op == "render-site") { // the web domain: a deployable static site
            std::string lang = args.find("es") != std::string_view::npos ? "es" : "en";
            std::string path = render_site(lang);
            if (path.empty()) {
                toast("site build failed - see log", true);
                return {};
            }
            if (on_open) on_open(path);
            toast("built site/ (index + style.css + app.js + assets) - deploy "
                  "this folder to your domain");
            return json_str(path);
        }
        /* SYNC REACHES THE COMMAND BAR TOO, for the same reason publishing does
         * (below): a verb that only one front-end can call is a broken surface.
         * The body is in `sync_ops.cpp` beside the rest of sync. */
        if (op == "sync-version" || op == "sync-merge" || op == "lan-peers" ||
            op == "lan-serve" || op == "lan-sync") {
            std::string result;
            return gui_sync_effect(op, args, result) ? result : std::string();
        }
        /* THE COMMAND BAR REACHES PUBLISHING TOO. The panel is an affordance;
         * these are the verbs. A person who types `effect deploy-site` into the
         * console inside the app gets exactly what the button does and exactly
         * what an agent gets — which is the "one interaction surface, three
         * callers" commitment being true rather than asserted. */
        if (op == "deploy-site" || op == "deploy") {
            std::string node;
            try {
                auto aj = nlohmann::json::parse(args);
                if (aj.contains("args") && !aj["args"].empty())
                    node = aj["args"][0].get<std::string>();
            } catch (...) {}
            maiz::ProjectOptions o;
            o.mantle = kAntfarmMantle;
            const maiz::Scene farm = maiz::project_scene(core, o);
            const std::string url = deploy_site(farm, node);
            if (url.empty()) {
                toast("publish failed - see log", true);
                return {};
            }
            for (const auto& c : deployment_record(farm, node, url, cur_doc,
                                                   preview_lang ? "es" : "en"))
                core.dispatch(c);
            toast("published: " + url);
            return json_str(url);
        }
        if (op == "rollback-site") {
            std::string node, dep;
            try {
                auto aj = nlohmann::json::parse(args);
                if (aj.contains("args") && aj["args"].size() > 0)
                    node = aj["args"][0].get<std::string>();
                if (aj.contains("args") && aj["args"].size() > 1)
                    dep = aj["args"][1].get<std::string>();
            } catch (...) {}
            maiz::ProjectOptions o;
            o.mantle = kAntfarmMantle;
            const std::string done =
                rollback_site(maiz::project_scene(core, o), node, dep);
            if (done.empty()) {
                toast("rollback failed - see log", true);
                return {};
            }
            toast("restored " + done);
            return json_str(done);
        }
        /* HOST IT ONLINE (2026-09-15): `effect host-online <image|missing>
         * [host-node]`, through the Antfarm's image host (domain/hosting.hpp).
         * `publish` is the old ImgBB-only name, kept so a script using it works;
         * it also received the raw `{"args":[...]}` as a rune name, and found
         * nothing. */
        if (op == "host-online" || op == "publish") {
            std::vector<std::string> a;
            try {
                auto aj = nlohmann::json::parse(args);
                if (aj.contains("args"))
                    for (const auto& x : aj["args"]) a.push_back(x.get<std::string>());
            } catch (...) {
                if (!args.empty()) a.push_back(std::string(args)); // a bare rune name
            }
            if (a.empty()) {
                toast("host-online: name an image, or `missing`", true);
                return {};
            }
            const std::string prefer = a.size() > 1 ? a[1] : std::string();
            if (a[0] == "missing") return std::to_string(host_missing_images(prefer));
            return host_image(a[0], prefer) ? json_str(a[0]) : std::string();
        }
        if (op == "import-rescue") { // the Supabase Import node's button
            import_rescue_effect();
            return {};
        }
        if (op == "mirror-images") { // the ImgBB node's cloud→local twin
            mirror_images_effect();
            return {};
        }
        if (op == "export-map") { // static PNG of a view at the current camera
            std::string v; // args arrive as {"args":[...]} json
            try {
                auto aj = nlohmann::json::parse(args);
                if (aj.contains("args") && !aj["args"].empty())
                    v = aj["args"][0].get<std::string>();
            } catch (...) {}
            if (v.empty()) v = map_sel;
            if (v.empty()) // fall back to the first view
                for (const auto& nn : scene.nodes)
                    if (nn.glyph == "map") { v = nn.name; break; }
            if (!v.empty()) export_map_png(v);
            else toast("no view to export - create one in the Map tab", true);
            return {};
        }
        if (op == "export-calendar") { export_calendar_png(); return {}; }
        // What the operator is looking at, filter included. The CLI reaches the
        // same function via main/headless.cpp -- unlike `export-calendar`, which
        // lives only here and so writes nothing there. The PNG is desktop-only:
        // it blits a baked ImGui font atlas.
        if (op == "export-calendar-ics") { export_calendar_ics(); return {}; }
        if (op == "preview-live") { // start the live preview (agent-reachable)
            preview_start();
            return {};
        }
        if (op == "serve-site") { // start the local web host (hol_localhost)
            host_start();
            return {};
        }
        if (op == "export-doc") { // save the current document to a file
            export_document();
            return {};
        }
        if (op == "new-database") { // a fresh empty database (agent/test seam)
            new_database();
            return {};
        }
        if (op == "save-database") { // save the bundle to a path (args = path)
            try {
                auto aj = nlohmann::json::parse(args);
                if (aj.contains("args") && !aj["args"].empty())
                    save_database_as(aj["args"][0].get<std::string>());
            } catch (...) {}
            return {};
        }
        if (op == "open-database") { // open a .miga bundle (args = path)
            try {
                auto aj = nlohmann::json::parse(args);
                if (aj.contains("args") && !aj["args"].empty())
                    open_database(aj["args"][0].get<std::string>());
            } catch (...) {}
            return {};
        }
        if (op == "apply-template") { // apply a built-in template by index
            int idx = 0;
            try {
                auto aj = nlohmann::json::parse(args);
                if (aj.contains("args") && !aj["args"].empty())
                    idx = std::stoi(aj["args"][0].get<std::string>());
            } catch (...) {}
            auto tpls = hormiga::builtin_templates();
            if (idx >= 0 && idx < (int)tpls.size()) apply_template(tpls[idx]);
            return {};
        }
        if (op == "derive-date-tags") { // the temper pass, dispatcher-reachable
            derive_date_tags();
            return {};
        }
        if (op == "query") {
            // the blessed interim for spatial reads (Core's 2026-07-21 ruling:
            // `effect query` routes a whole query to the host until composable
            // where-predicates land). `effect query near <lat,lon> <radius_m>`
            // → located runes within radius, nearest first, as JSON data —
            // an agent's "what's around this point?"
            std::vector<std::string> tok = tokenize(std::string(args));
            if (tok.size() >= 3 && tok[0] == "near") {
                double lat, lon;
                if (!hormiga::parse_geo(tok[1], lat, lon))
                    return std::string("\"bad geo (want lat,lon)\"");
                double radius = std::atof(tok[2].c_str());
                maiz::ProjectOptions po;
                po.mantle = kDataMantle;
                maiz::Scene data = maiz::project_scene(core, po);
                std::vector<std::pair<double, std::string>> hits;
                for (const auto& n : data.nodes) {
                    std::string g = field_value(n, "geo");
                    double la, lo;
                    if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
                    double d = hormiga::geo_distance_m(lat, lon, la, lo);
                    if (d <= radius) hits.push_back({d, n.name});
                }
                std::sort(hits.begin(), hits.end());
                std::string j = "[";
                for (size_t i = 0; i < hits.size(); ++i) {
                    char m[32];
                    std::snprintf(m, sizeof m, "%.0f", hits[i].first);
                    if (i) j += ",";
                    j += "{\"name\":\"" + hits[i].second + "\",\"m\":" + m + "}";
                    log.push_back({"info", "near", hits[i].second + "  (" +
                                                       std::string(m) + " m)"});
                }
                if (hits.empty())
                    log.push_back({"info", "near", "(nothing located in radius)"});
                return j + "]";
            }
            return {};
        }
        return {};
    });
}

void HormigaApp::apply_theme() {
    if (light_mode) {
        ImGui::StyleColorsLight();
        canvas_style.theme = maiz::CanvasTheme::light();
    } else {
        ImGui::StyleColorsDark();
        canvas_style.theme = maiz::CanvasTheme::dark();
    }
    // roomier, more legible chrome (author's ask, 2026-07-20). Bigger text and
    // more breathing room until Void Maiz's `apply_touch_metrics` / a real
    // theme pass (or Allmusely) supersedes this. ScaleAllSizes must run on a
    // FRESH style each call or padding compounds on every theme toggle.
    ImGuiStyle& s = ImGui::GetStyle();
    s.FramePadding = ImVec2(8, 6);
    s.ItemSpacing = ImVec2(9, 7);
    s.ItemInnerSpacing = ImVec2(7, 5);
    s.CellPadding = ImVec2(7, 5);
    s.WindowPadding = ImVec2(10, 10);
    s.FrameRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.ScrollbarSize = 15.0f;
    // UI/UX phase (2026-08-03): softer, rounder panels — the modern-desktop feel
    s.ChildRounding = 6.0f;
    s.PopupRounding = 6.0f;
    s.WindowRounding = 6.0f;
    s.TabRounding = 5.0f;
    // accent-tinted hover/selection (fx_highlights): interactive rows glow toward
    // the org's accent instead of ImGui's neutral grey. Off ⇒ keep grey (a hair
    // cheaper, and the low-distraction choice). Re-applied whenever the toggle
    // flips (draw_settings calls apply_theme).
    if (fx_highlights) {
        ImVec4 a(theme_accent[0], theme_accent[1], theme_accent[2], 1.0f);
        auto tint = [&](ImGuiCol c, float alpha) {
            s.Colors[c] = ImVec4(a.x, a.y, a.z, alpha);
        };
        tint(ImGuiCol_Header, 0.36f);
        tint(ImGuiCol_HeaderHovered, 0.55f);
        tint(ImGuiCol_HeaderActive, 0.72f);
        tint(ImGuiCol_ButtonHovered, 0.48f);
        tint(ImGuiCol_ButtonActive, 0.68f);
        tint(ImGuiCol_FrameBgHovered, 0.26f);
        tint(ImGuiCol_SeparatorHovered, 0.60f);
        tint(ImGuiCol_TabHovered, 0.55f);
        tint(ImGuiCol_TextSelectedBg, 0.35f);
    }
    ImGui::GetIO().FontGlobalScale = ui_scale; // Settings > UI scale
}

// A soft drop shadow behind a rounded rectangle (UI/UX phase; gated by
// fx_shadows). Layered translucent rects, offset slightly down for depth —
// drawn on the CURRENT window's draw list, so call BEFORE the child/content it
// sits under. Cheap (a handful of AddRectFilled) but honestly opt-out.
void HormigaApp::panel_shadow(const ImVec2& mn, const ImVec2& mx, float rounding) {
    if (!fx_shadows) return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int layers = 7;
    for (int i = layers; i >= 1; --i) {
        float sp = (float)i * 1.6f;                 // outward spread
        float oy = 2.0f;                            // downward offset (light above)
        int alpha = (int)(9.0f * (1.0f - (float)(i - 1) / layers));
        dl->AddRectFilled(ImVec2(mn.x - sp, mn.y - sp + oy),
                          ImVec2(mx.x + sp, mx.y + sp + oy),
                          IM_COL32(0, 0, 0, alpha), rounding + sp);
    }
}

void HormigaApp::read_view_config() {
    ed.cam = {-10, -10, 1.0f};
    if (maiz::Camera saved;
        maiz::parse_camera(core.dispatch("config get view.camera").data, saved))
        ed.cam = saved;
    float a, b;
    if (parse_two(core.dispatch("config get view.panels").data, a, b)) {
        canvas_frac = std::clamp(a, 0.4f, 0.92f);
        log_frac = std::clamp(b, 0.3f, 0.92f);
    }
    if (parse_two(core.dispatch("config get view.data_panels").data, a, b)) {
        data_side_frac = std::clamp(a, 0.08f, 0.4f);
        data_list_frac = std::clamp(b, 0.15f, 0.75f);
    }
    // branding rides the database (refresh on open, not just boot)
    auto rd = [&](const char* key, char* out, size_t n) {
        std::string v = core.dispatch(std::string("config get ") + key).data;
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size() - 2);
        std::snprintf(out, n, "%s", (v == "null") ? "" : v.c_str());
    };
    rd("org.name", org_name, sizeof org_name);
    rd("org.logo", org_logo_buf, sizeof org_logo_buf);
    org_logo = org_logo_buf;
}

void HormigaApp::flush_panels() {
    char buf[96];
    std::snprintf(buf, sizeof buf, "config set view.panels \"%.3f %.3f\"",
                  canvas_frac, log_frac);
    core.dispatch(buf);
}

void HormigaApp::flush_data_panels() {
    char buf[96];
    std::snprintf(buf, sizeof buf, "config set view.data_panels \"%.3f %.3f\"",
                  data_side_frac, data_list_frac);
    core.dispatch(buf);
}

// ── projection + feedback ────────────────────────────────────────────────────

void HormigaApp::reproject() {
    scene = maiz::project_scene(core);
    // per-view position channels: projection only fills DECLARED glyph
    // fields, so every unlocked view's geo_<channel> must be registered on
    // the placeable glyphs. Detect channel changes here (the one seam all
    // model changes pass through) and re-register + re-project when the set
    // moves — unlocking a view makes its positions visible the same frame.
    std::vector<std::string> chans;
    for (const auto& n : scene.nodes) {
        if (n.glyph != "map") continue;
        std::string ch = hormiga::temper::field_value(n, "channel");
        if (!ch.empty() && ch != "main") chans.push_back("geo_" + ch);
    }
    std::sort(chans.begin(), chans.end());
    chans.erase(std::unique(chans.begin(), chans.end()), chans.end());
    if (chans != channel_fields) {
        channel_fields = chans;
        hormiga::register_glyphs(core, channel_fields);
        scene = maiz::project_scene(core);
    }
    maiz::Result h = core.dispatch("history");
    undo_depth = (h.lines.size() == 1 && h.lines[0] == "(no history)")
                     ? 0
                     : (int)h.lines.size();
    refresh_allo_rules(); // Allomone: cache the rule set (its own mantle)
}

// Attach the mantle's wires to the projected runes (both directions), so
// Allomone can traverse the graph STRUCTURALLY — neighbours, clusters, degree —
// rather than by naming specific runes (okf/concepts/allomone/traversal.md).
static void allo_attach_links(std::vector<allo::Thing>& things,
                              const maiz::Scene& scene) {
    std::unordered_map<std::string, size_t> idx;
    for (size_t i = 0; i < things.size(); ++i) idx[things[i].name] = i;
    for (const auto& w : scene.wires) {
        auto a = idx.find(w.from), b = idx.find(w.to);
        if (a == idx.end() || b == idx.end()) continue;
        things[a->second].links.push_back({w.relation, w.to});
        things[b->second].links.push_back({w.relation, w.from});
    }
}

// Compute the GLOBAL graph measures ONCE (budgeted) and stamp each rune, so
// Allomone reads `centrality(rune)` / `community(rune)` as O(1) lookups. This is
// the "expensive, opt-in, host-computed" path (okf/concepts/allomone/inputs.md
// §4): eigenvector centrality by power iteration, community by deterministic
// label propagation. Both are cheap at the org scale (10²–10³ runes).
static void allo_compute_measures(std::vector<allo::Thing>& r) {
    size_t n = r.size();
    if (n == 0) return;
    std::unordered_map<std::string, size_t> idx;
    for (size_t i = 0; i < n; ++i) idx[r[i].name] = i;
    std::vector<std::vector<size_t>> adj(n);
    for (size_t i = 0; i < n; ++i) {
        std::set<size_t> nb;
        for (const auto& lk : r[i].links) {
            auto it = idx.find(lk.second);
            if (it != idx.end() && it->second != i) nb.insert(it->second);
        }
        adj[i].assign(nb.begin(), nb.end());
    }
    // eigenvector centrality: c ← normalize(A·c), a few power-iterations
    std::vector<double> c(n, 1.0);
    for (int iter = 0; iter < 40; ++iter) {
        std::vector<double> nc(n, 0.0);
        for (size_t i = 0; i < n; ++i)
            for (size_t j : adj[i]) nc[i] += c[j];
        double norm = 0;
        for (double v : nc) norm += v * v;
        norm = std::sqrt(norm);
        if (norm < 1e-9) break;
        for (size_t i = 0; i < n; ++i) nc[i] /= norm;
        c.swap(nc);
    }
    double mx = 0;
    for (double v : c) mx = std::max(mx, v);
    for (size_t i = 0; i < n; ++i) r[i].centrality = mx > 0 ? c[i] / mx : 0.0;
    // community by label propagation (deterministic: lowest label breaks ties)
    std::vector<int> lbl(n);
    for (size_t i = 0; i < n; ++i) lbl[i] = (int)i;
    for (int iter = 0; iter < 20; ++iter) {
        bool changed = false;
        for (size_t i = 0; i < n; ++i) {
            std::map<int, int> cnt;
            for (size_t j : adj[i]) cnt[lbl[j]]++;
            if (cnt.empty()) continue;
            int best = lbl[i], bestc = -1;
            for (const auto& [l, cc] : cnt)
                if (cc > bestc) { best = l; bestc = cc; } // map is label-ordered → lowest wins ties
            if (best != lbl[i]) { lbl[i] = best; changed = true; }
        }
        if (!changed) break;
    }
    for (size_t i = 0; i < n; ++i) r[i].community = lbl[i];
}

// Run the FROZEN legacy interpreter as ONE PRODUCER among several: instead of
// writing a colour straight into the cache, each enabled `script` rune yields a
// ConstraintMap that merges alongside the Void Maiz scripts
// (okf/concepts/allomone/adoption.md; the handover's step 2).
//
// This is the whole reason the old engine can keep shipping while the new one
// takes over. It also means an old script and a new one disagreeing about a
// card is now a SURFACED CONFLICT rather than whichever ran last — which is the
// behaviour the adoption was for, applied to the migration itself.
static std::vector<maiz::ConstraintMap>
allo_legacy_sources(const maiz::Scene& scripts, const maiz::Scene& data,
                    std::map<std::string, std::vector<std::string>>& errors) {
    std::vector<allo::Thing> things;
    things.reserve(data.nodes.size());
    for (const auto& n : data.nodes) {
        if (!hormiga::allomone::is_subject_glyph(n.glyph)) continue;
        allo::Rune r{n.name, n.glyph, n.tags, {}, {}};
        for (const auto& f : n.fields)
            r.fields.push_back({f.key, hormiga::temper::field_value(n, f.key.c_str())});
        things.push_back(std::move(r));
    }
    allo_attach_links(things, data); // graph edges → structural traversal
    allo_compute_measures(things);   // centrality + community, host-computed once

    std::vector<maiz::ConstraintMap> out;
    for (const auto& s : scripts.nodes) {
        if (s.glyph != "script") continue; // the legacy dialect's glyph
        if (hormiga::temper::field_value(s, "enabled") == "0") continue;
        std::string nm = hormiga::temper::field_value(s, "name");
        if (nm.empty()) nm = s.name;
        allo::Script sc = allo::parse(hormiga::temper::field_value(s, "body"));
        if (!sc.ok()) { errors[nm] = sc.errors; continue; }
        maiz::ConstraintMap m;
        m.id = "legacy:" + nm;
        // Strength 1, uniformly: the legacy dialect is imperative and has no
        // notion of specificity to report, so claiming one would be a lie. It
        // beats a `when all` default and ties with any single-term rule, which
        // is the honest reading of "this script decided to colour this thing".
        auto sink = [&m](const std::string& thing, const std::string& hex) {
            m.set(thing, "color", hex, 1, "legacy");
        };
        std::vector<std::string> rt;
        allo::run(sc, things, sink, &rt);
        if (!rt.empty()) errors[nm] = rt;
        out.push_back(std::move(m));
    }
    return out;
}

// DERIVE (okf/concepts/allomone/): compose every source's opinion about the
// data into one annotated state, and cache the render seam (a colour per rune).
//
// Independent of the active tab — it projects the allomone mantle (scripts and
// resolutions) and the data mantle (subjects) itself. Nothing here writes: a
// derivation is a projection, so re-deriving is always safe and disabling a
// script removes its effects with nothing to undo.
void HormigaApp::refresh_allo_rules() {
    allo_colors.clear();
    allo_errors.clear();
    maiz::ProjectOptions po;
    po.mantle = kAlloMantle;
    maiz::Scene rs;
    try {
        rs = maiz::project_scene(core, po);
    } catch (...) {
        allo_derived = {}; // mantle not present yet
        return;
    }
    maiz::ProjectOptions dp;
    dp.mantle = kDataMantle;
    maiz::Scene ds;
    try {
        ds = maiz::project_scene(core, dp);
    } catch (...) {}

    hormiga::allomone::Options o;
    { // `today` is read ONCE, here, and frozen into the frame. A predicate that
      // consulted the clock itself would make the merge depend on WHEN it was
      // asked and silently destroy order-independence.
        int y = 0, m = 0, d = 0;
        cal_today(y, m, d);
        char buf[16];
        std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", y, m, d);
        o.inputs.today = buf;
    }
    o.inputs.published = allo_published_runes(); // the Builder's references
    o.resolutions = hormiga::allomone::resolutions_from(rs);
    o.extra_sources = allo_legacy_sources(rs, ds, allo_errors);
    allo_derived = hormiga::allomone::derive(ds, rs, o);

    for (const auto& s : allo_derived.scripts)
        if (!s.diagnostics.empty()) {
            auto& into = allo_errors[s.label];
            for (const auto& x : s.diagnostics)
                into.push_back("line " + std::to_string(x.line + 1) + ": " + x.message);
        }
    // The render seam, cached per SURFACE. `value_for` refuses a conflicted
    // cell, so a contested card simply keeps its default — never an arbitrary
    // winner. Caching here rather than resolving per draw call keeps the map's
    // marker loop O(1) per node and, more importantly, means every surface
    // reads the same derivation: the tab cannot disagree with the cards.
    for (const auto& subj : allo_derived.subjects) {
        for (const auto& dom : hormiga::allomone::domains()) {
            AlloStyle st;
            hormiga::allomone::color_for(allo_derived.merged, subj.id, dom.prefix,
                                         st.rgba);
            st.has_color = st.rgba != 0;
            st.icon = hormiga::allomone::value_for(allo_derived.merged, subj.id,
                                                   dom.prefix, "icon");
            st.label = hormiga::allomone::value_for(allo_derived.merged, subj.id,
                                                    dom.prefix, "label");
            st.badges = hormiga::allomone::list_for(allo_derived.merged, subj.id,
                                                    dom.prefix, "badge");
            st.notes = hormiga::allomone::list_for(allo_derived.merged, subj.id,
                                                   dom.prefix, "note");
            st.weight = hormiga::allomone::number_for(allo_derived.merged, subj.id,
                                                      dom.prefix, "weight");
            st.priority = hormiga::allomone::number_for(allo_derived.merged, subj.id,
                                                        dom.prefix, "priority");
            // A CONFLICTED `hide` reads as "" and therefore does NOT hide —
            // deliberately. Two scripts disagreeing about whether something may
            // be published is when a human should decide, and silently
            // suppressing content nobody agreed to suppress looks exactly like
            // the export being broken.
            std::string h = hormiga::allomone::value_for(allo_derived.merged, subj.id,
                                                         dom.prefix, "hide");
            st.hide = !h.empty() && h != "0" && h != "false";
            if (st.has_color || !st.icon.empty() || !st.label.empty() ||
                !st.badges.empty() || !st.notes.empty() || st.weight ||
                st.priority || st.hide)
                allo_style[dom.prefix][subj.id] = std::move(st);
        }
        unsigned rgba = 0; // the legacy one-colour cache the card view still uses
        if (hormiga::allomone::color_for(allo_derived.merged, subj.id, "card", rgba))
            allo_colors[subj.id] = rgba;
    }
}

// Every rune the Builder's documents currently reference — what `published ""`
// answers. Computed here rather than inside the derivation because the Builder
// lives in another mantle and a derivation must not go looking for one; the
// caller knows what it has.
//
// Two ways a document reaches a rune: a block naming it directly (`ref`), and a
// query-backed block whose expression selects it. The second is the one that
// matters — a query grid is how most content is published — so it is resolved
// through the same `node_matches` the preview uses, not approximated.
std::vector<std::string> HormigaApp::allo_published_runes() {
    std::vector<std::string> out;
    maiz::ProjectOptions ep;
    ep.mantle = kDataMantle;
    maiz::Scene data;
    try { data = maiz::project_scene(core, ep); } catch (...) { return out; }

    std::vector<std::string> queries;
    for (const std::string& doc : list_documents()) { // a document IS a mantle
        maiz::ProjectOptions dp;
        dp.mantle = doc;
        maiz::Scene blocks;
        try { blocks = maiz::project_scene(core, dp); } catch (...) { continue; }
        for (const auto& n : blocks.nodes) {
            std::string ref = hormiga::temper::field_value(n, "ref");
            if (!ref.empty()) out.push_back(ref);
            std::string q = hormiga::temper::field_value(n, "query");
            if (!q.empty()) queries.push_back(q);
        }
    }
    /* Date-aware, like the renderer: a rune reachable only by `date:future`
     * is genuinely published today, and one whose event has passed is
     * genuinely not — so "what does a document reach" answers with the same
     * set the page will actually contain. */
    for (const auto& q : queries)
        for (const auto& e : data.nodes)
            if (hormiga::query_matches(q, data, e)) out.push_back(e.name);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

// Run a rule command in the ALLOMONE mantle, restoring the active mantle after —
// so editing rules from the Allomone tab never disturbs the visible section.
void HormigaApp::allo_cmd(const std::string& c) {
    std::string prev = scene.mantle;
    pending_cmds.push_back(std::string("use ") + kAlloMantle);
    pending_cmds.push_back(c);
    if (!prev.empty() && prev != kAlloMantle)
        pending_cmds.push_back(std::string("use ") + prev);
}

maiz::Result HormigaApp::dispatch_and_reproject(const std::string& cmd) {
    maiz::Result r = core.dispatch(cmd);
    if (!r.ok) {
        /* THE STRIP GETS IT TOO (2026-09-16). A toast is gone in four seconds and
         * was the ONLY trace of a failure: the author hit "no host effect handler"
         * and found nothing in the console to copy. The core's own log sink cannot
         * cover this one - a core that lost its host seams lost the sink with them. */
        const std::string why = r.text().empty() ? std::string("failed") : r.text();
        toast(why == "failed" ? "failed: " + cmd : why, true);
        log.push_back({"error", cmd.substr(0, cmd.find(0x20)), why + "  (" + cmd + ")"});
    }
    reproject();
    if (r.ok && preview_live) { // live preview: any landed edit re-renders
        preview_dirty = true;   // (debounced in frame(); render is VIEW-side,
        preview_edit_t = ImGui::GetTime(); // not a logged effect)
    }
    /* ── UNSAVED WORK, COUNTED AT THE ONE DOOR (2026-09-02) ──────────────────
     *
     * The author asked for a Save button in the Builder. Writing was never the
     * missing part — Ctrl+S and File > Save have always done it — so what the
     * button needs is the thing the writing did not provide: an answer to *is
     * there anything to save*. This is the only place a GUI edit reaches the
     * model, so it is the only place a count cannot drift.
     *
     * DELIBERATELY BIASED TOWARD OVER-REPORTING. `maiz::Result` carries no
     * "did this change anything" flag, and comparing exported state per edit is
     * not worth it, so anything off the short read-only list counts. The two
     * failure directions are not symmetric: an extra "1 unsaved" makes somebody
     * press a free, idempotent button, while a missed one lets them close the
     * app believing their afternoon is on disk. A list of MUTATING verbs would
     * go stale in the dangerous direction; this one goes stale in the harmless
     * one.
     *
     * `use` is on the list and is the judgement call: switching mantle is a
     * state change, but the Builder dispatches it on every document switch, and
     * a counter that ticks when somebody looks at another page is one they
     * learn to ignore. */
    if (r.ok) {
        static const char* kReadOnly[] = {"use ",  "ls",   "find",  "cat ",
                                          "mantles", "glyphs", "links ",
                                          "related ", "status", "diff",
                                          "verbs", "tags"};
        if (cmd == "save") {
            edits_since_save = 0;
        } else {
            bool reads_only = false;
            for (const char* v : kReadOnly) {
                const size_t n = std::strlen(v);
                if (cmd.compare(0, n, v) == 0 &&
                    (cmd.size() == n || v[n - 1] == ' ')) {
                    reads_only = true;
                    break;
                }
            }
            if (!reads_only) ++edits_since_save;
        }
    }
    return r;
}

void HormigaApp::do_save() {
    if (dispatch_and_reproject("save").ok)
        toast("saved " + org_file().filename().string());
}

// ── the .miga v3 database bundle: Save / Save As / Open (miga-format.md) ─────

/* Clear the working copy's local files so two databases never bleed together
 * (author 2026-07-24: "different databases... have their own local storage").
 * Always drops the re-derivable caches (they rebuild from protocols); on a
 * database SWITCH also drops assets/ (each bundle carries its own). */
void HormigaApp::reset_working_copy(bool clear_assets) {
    std::error_code ec;
    tex_cache.clear(); // freed image paths must not return stale textures
    fs::remove_all(data_dir("tiles"), ec);
    fs::remove_all(data_dir("site"), ec);
    fs::remove_all(data_dir("exports"), ec);
    for (auto& e : fs::directory_iterator(base_dir, ec)) {
        std::string fn = e.path().filename().string();
        if (fn.rfind("preview-", 0) == 0 && e.path().extension() == ".html")
            fs::remove(e.path(), ec);
    }
    if (clear_assets) fs::remove_all(data_dir("assets"), ec);
}


/* Pack the working copy into the current bundle (backing up the old one), or
 * fall through to Save As when there's no bundle yet. Save ≈ commit. */
void HormigaApp::save_database() {
    if (cur_miga.empty()) { // no bundle yet → behave as Save As (author #1)
        if (on_save_file) {
            std::string p = on_save_file("my-database.miga");
            if (!p.empty()) save_database_as(p);
        } else {
            toast("no save dialog available", true);
        }
        return;
    }
    do_save(); // flush the live state into the working .db first
    std::error_code ec;
    // back up the existing bundle before overwriting (author #5: backups)
    if (fs::exists(cur_miga)) {
        fs::create_directories(data_dir("backups"), ec);
        char stamp[32];
        std::time_t t = std::time(nullptr);
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&t));
        std::string stem = fs::path(cur_miga).stem().string();
        fs::copy_file(cur_miga, data_dir("backups") / (stem + "-" + stamp + ".miga"),
                      fs::copy_options::overwrite_existing, ec);
        // retention: keep the newest ~10 backups for this database
        std::vector<fs::path> mine;
        for (auto& e : fs::directory_iterator(data_dir("backups"), ec))
            if (e.path().filename().string().rfind(stem + "-", 0) == 0)
                mine.push_back(e.path());
        std::sort(mine.begin(), mine.end());
        for (size_t i = 0; i + 10 < mine.size(); ++i) fs::remove(mine[i], ec);
    }
    auto r = hormiga::miga::pack(core.export_state(), base_dir, cur_miga,
                                 fs::path(cur_miga).stem().string(), assets_dir(),
                                 referenced_files(core.export_state()));
    if (r.ok)
        toast("saved database " + fs::path(cur_miga).filename().string() + " (" +
              std::to_string(r.assets) + " assets, " +
              std::to_string(r.bytes / 1024) + " KB)");
    else
        toast("save database failed: " + r.error, true);
}

/* Save the working copy as a NEW .miga at a user-CHOSEN path (author
 * 2026-07-24: "choose where we are saving"). The path comes from the OS save
 * dialog; .miga is ensured. This bundle becomes the current database. */
void HormigaApp::save_database_as(const std::string& path) {
    if (path.empty()) return;
    fs::path out = path;
    if (out.extension() != ".miga") out += ".miga";
    do_save(); // flush live state into the working .db first
    auto r = hormiga::miga::pack(core.export_state(), base_dir, out,
                                 out.stem().string(), assets_dir(),
                                 referenced_files(core.export_state()));
    if (r.ok) {
        cur_miga = out.string();
        remember_bundle(cur_miga);
        toast("saved database to " + out.string() + " (" +
              std::to_string(r.assets) + " assets, " +
              std::to_string(r.bytes / 1024) + " KB)");
    } else {
        toast("save database failed: " + r.error, true);
    }
}

/* Start a COMPLETELY NEW, empty database (author 2026-07-24): a fresh working
 * copy — empty data, one blank newsletter document, the Antfarm topology (the
 * protocol layer) — and NO bundle yet (Save database as... names & places its
 * .miga). This is the multi-database "new project" entry the app was missing. */
void HormigaApp::new_database() {
    reset_working_copy(true); // a fresh db owns nothing from the old working copy
    core = maiz::Core(); // a fresh, empty state (no demo data)
    install_host();
    channel_fields.clear();
    hormiga::register_glyphs(core);
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
    core.dispatch("config set actor human:hormiga");
    // minimal structure so every section has a home to open into
    core.dispatch(std::string("mantle new ") + kDataMantle);   // empty org data
    core.dispatch("mantle new issue-demo");                    // one blank document
    core.dispatch(std::string("mantle new ") + kAlloMantle);   // Allomone rules
    core.dispatch("rune new hero masthead");
    core.dispatch("set masthead title_en \"New Newsletter\"");
    core.dispatch("set masthead row \"0\"");
    for (const auto& c : hormiga::seed_antfarm_transcript()) core.dispatch(c);
    core.dispatch(std::string("use ") + kDataMantle);
    reproject();
    read_view_config();
    apply_theme();
    cur_miga.clear();      // unsaved: a new database has no file until Save As
    remember_bundle("");
    cur_doc = "issue-demo";
    cur_page.clear();
    ed.selection.clear();
    do_save();             // flush the fresh structure into the working .db
    toast("new database (empty) - use 'Save database as...' to name & place it");
}

/* Replace the running core with a freshly-loaded state, re-establishing the
 * host seams and glyphs (the same setup init() does after a state load), and
 * persist it into the working .db so the app runs off local storage. */
void HormigaApp::reload_from_state(const std::string& state) {
    core = maiz::Core(state);
    install_host();
    channel_fields.clear();
    hormiga::register_glyphs(core);
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
    core.dispatch("config set actor human:hormiga");
    reproject();
    read_view_config();
    apply_theme();
    do_save(); // write the opened database into the working .db
    cur_page.clear();
    cur_doc = "issue-demo";
    ed.selection.clear();
}

void HormigaApp::open_database(const std::string& path) {
    // isolate: drop the current working copy's assets + caches BEFORE the
    // bundle extracts its own into base_dir/assets/ (databases don't bleed)
    reset_working_copy(true);
    auto r = hormiga::miga::open(path, base_dir, assets_dir());
    if (!r.ok) {
        toast(r.version == 2 ? "that's a legacy secrets-only .miga (v2), not a "
                               "full database"
                             : "open failed: " + r.error,
              true);
        return;
    }
    reload_from_state(r.state);
    cur_miga = path;
    remember_bundle(path);
    load_secrets(); // an imgbb.key beside THIS bundle
    toast("opened database " + fs::path(path).filename().string() + " (" +
          std::to_string(r.assets) + " assets restored)");
}

void HormigaApp::toast(std::string msg, bool error) {
    toasts.push_back({std::move(msg), 4.0f, error});
    if (toasts.size() > 4) toasts.erase(toasts.begin());
}

void HormigaApp::draw_toasts() {
    float dt = ImGui::GetIO().DeltaTime;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float y = vp->WorkPos.y + vp->WorkSize.y - 12.0f;
    int i = 0;
    for (auto it = toasts.begin(); it != toasts.end(); ++i) {
        it->ttl -= dt;
        if (it->ttl <= 0) {
            it = toasts.erase(it);
            continue;
        }
        float alpha = std::min(1.0f, it->ttl);
        ImGui::SetNextWindowBgAlpha(0.85f * alpha);
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - 12.0f, y),
                                ImGuiCond_Always, ImVec2(1, 1));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::Begin(("##toast" + std::to_string(i)).c_str(), nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        if (it->error)
            ImGui::TextColored(ImVec4(0.90f, 0.35f, 0.30f, 1.0f), "%s", it->msg.c_str());
        else
            ImGui::TextUnformatted(it->msg.c_str());
        y -= ImGui::GetWindowSize().y + 6.0f;
        ImGui::End();
        ImGui::PopStyleVar();
        ++it;
    }
}

void HormigaApp::switch_section(int s) {
    section = s;
    ed.selection.clear();
    // Map overlays the org's data, so it rides the data mantle for now
    std::string mantle = (s == Builder)   ? cur_doc
                         : (s == Antfarm) ? std::string(kAntfarmMantle)
                                          : std::string(kDataMantle);
    if (scene.mantle != mantle)
        dispatch_and_reproject(std::string("use ") + mantle);
}

// ── init / shutdown ──────────────────────────────────────────────────────────

void HormigaApp::init() {
    // boot order (snapshot-fallback principle): the .db is where the org
    // lives; the .json mirror catches a missing/unreadable database
    /* Claim the floor before anything else touches the document, so a running
     * agent is preempted rather than racing us (see claim_the_floor). */
    if (!claim_the_floor(org_file()))
        toast("another session holds this database - your changes may be "
              "overwritten by it", true);

    storage = std::make_unique<hormiga::Storage>(db_file());
    std::string state;
    if (storage->ok()) state = storage->load_state();
    else toast("database unavailable (" + storage->error() + ") - using snapshot", true);

    /* A HEADLESS RUN MAY HAVE MOVED AHEAD OF US (2026-08-18). `voidhormiga-cli`
     * writes the JSON state document and does not touch SQLite, because Void
     * Maiz's Session persists the document and deliberately does not dispatch
     * `save` — that is what leaves an agent's work visible as unsaved changes
     * against the baseline, which is the entire cross-process review story.
     *
     * So the database is authoritative only until something else writes the
     * document. Preferring whichever is NEWER is what makes the author's "when
     * I open up the app, it should be updated" true, and it generalizes past
     * our own CLI to any external writer. The next save refills the database.
     *
     * Deliberately a timestamp comparison and not a content one: we want to
     * notice a change we did not make, and we cannot diff against a document we
     * never saw. */
    std::error_code tec;
    if (!state.empty() && fs::exists(org_file(), tec) && fs::exists(db_file(), tec)) {
        const auto jt = fs::last_write_time(org_file(), tec);
        const auto dt = fs::last_write_time(db_file(), tec);
        if (!tec && jt > dt) {
            std::ifstream in(org_file(), std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            if (!ss.str().empty()) {
                state = ss.str();
                toast("loaded newer changes from the state document "
                      "(a headless run?) - saving will refresh the database");
            }
        }
    }
    if (state.empty() && fs::exists(org_file())) {
        std::ifstream in(org_file(), std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        state = ss.str();
        if (storage->ok()) toast("opened from snapshot; next save fills the db");
    }
    if (!state.empty()) core = maiz::Core(state); // replay the state document
    install_host();
    hormiga::register_glyphs(core); // host config, per-session, never exported
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
    core.dispatch("config set actor human:hormiga");
    if (state.empty()) { // truly fresh: seed the shipped default (the Cat Colony)
        for (const auto& cmd : hormiga::seed_cat_transcript()) core.dispatch(cmd);
        for (const auto& cmd : hormiga::seed_cat_issue_transcript()) core.dispatch(cmd);
        // The LEGACY example library is no longer seeded (2026-08-11): its tab
        // is unshipped, so twelve uneditable `script` runes in every new
        // database would be scaffolding a user cannot act on. The eight
        // `allo-script` examples below replace them. Existing databases keep
        // theirs, the interpreter still derives them, and the Allomone tab can
        // enable, disable and delete them.
        // for (const auto& cmd : hormiga::seed_cat_scripts_transcript()) core.dispatch(cmd);
        for (const auto& cmd : hormiga::seed_allomone_scripts_transcript())
            core.dispatch(cmd);
        // A small CIVIC RECORD in its own mantle, so the window opens onto
        // something. Synthetic council, structure from the real Springfield
        // document — see the note on the transcript.
        for (const auto& cmd : hormiga::seed_civic_transcript()) core.dispatch(cmd);
        core.dispatch("use demo-org"); // the civic seed leaves the mantle alone
    }
    { // migration: orgs saved before the Antfarm became a mantle get one now
        maiz::ProjectOptions po;
        po.mantle = kAntfarmMantle;
        if (maiz::project_scene(core, po).nodes.empty())
            for (const auto& cmd : hormiga::seed_antfarm_transcript())
                core.dispatch(cmd);
    }
    { // the Allomone rules mantle (created for every org that predates it)
        if (core.dispatch("mantles --json").data.find("\"" + std::string(kAlloMantle) +
                                                      "\"") == std::string::npos)
            core.dispatch(std::string("mantle new ") + kAlloMantle);
        core.dispatch(std::string("use ") + kDataMantle); // leave data active
    }

    // settings (config tier — ride the saved org)
    {
        std::string v = core.dispatch("config get ui.scale").data;
        float f = (float)std::atof(v.c_str() + (v.empty() ? 0 : (v[0] == '"' ? 1 : 0)));
        if (f >= 1.0f && f <= 2.0f) ui_scale = f;
        map_show_prox =
            core.dispatch("config get ui.map_proximity").data.find('1') !=
            std::string::npos;
        v = core.dispatch("config get ui.map_proximity_m").data;
        f = (float)std::atof(v.c_str() + (v.empty() ? 0 : (v[0] == '"' ? 1 : 0)));
        if (f >= 50.0f && f <= 10000.0f) map_prox_m = f;
        time_prox = core.dispatch("config get ui.time_proximity").data.find('1') !=
                    std::string::npos;
        v = core.dispatch("config get ui.time_proximity_d").data;
        f = (float)std::atof(v.c_str() + (v.empty() ? 0 : (v[0] == '"' ? 1 : 0)));
        if (f >= 1.0f && f <= 365.0f) time_prox_days = f;
        // animations default ON; only an explicit "0" turns them off
        v = core.dispatch("config get ui.animations").data;
        if (v.find('0') != std::string::npos && v.find('1') == std::string::npos)
            gui_anim = false;
        // visual-effects toggles (UI/UX phase) — each defaults ON, an explicit
        // "0" disables. Granular so a weak machine can shed effects one by one.
        auto read_fx = [&](const char* key, bool& out) {
            std::string s = core.dispatch(std::string("config get ") + key).data;
            if (s.find('0') != std::string::npos && s.find('1') == std::string::npos)
                out = false;
        };
        read_fx("ui.fx.shadows", fx_shadows);
        read_fx("ui.fx.highlights", fx_highlights);
        read_fx("ui.fx.blur", fx_blur);
        // Data list pane: list (0, default) vs cards (1)
        if (core.dispatch("config get ui.data_view").data.find('1') !=
            std::string::npos)
            data_view_mode = 1;
        { // card size 0/1/2 (small/med/large)
            std::string s = core.dispatch("config get ui.data_card_size").data;
            for (char ch : s)
                if (ch >= '0' && ch <= '2') { data_card_size = ch - '0'; break; }
        }
        show_legacy = core.dispatch("config get ui.show_legacy").data.find('1') !=
                      std::string::npos; // legacy block canvas off by default
        { // tag-recommendation mode (0 similar / 1 distinct / 2 comprehensive)
            std::string s =
                core.dispatch("config get ui.tags.recommend_mode").data;
            for (char ch : s)
                if (ch >= '0' && ch <= '2') { tag_rec_mode = ch - '0'; break; }
        }
        // base map source + color treatment (global; the base map is singular)
        v = core.dispatch("config get ui.basemap").data;
        for (int i = 0; i < kBaseSourceCount; ++i)
            if (v.find(kBaseSources[i].key) != std::string::npos) basemap_src = i;
        v = core.dispatch("config get ui.basemap_brightness").data;
        f = (float)std::atof(v.c_str() + (v.empty() ? 0 : (v[0] == '"' ? 1 : 0)));
        if (f >= 0.3f && f <= 1.0f) basemap_brightness = f;
        v = core.dispatch("config get ui.basemap_fade").data;
        f = (float)std::atof(v.c_str() + (v.empty() ? 0 : (v[0] == '"' ? 1 : 0)));
        if (f >= 0.0f && f <= 1.0f) basemap_fade = f;
        // the THEME (Style tab socket): "#rrggbb" strings in config
        auto read_hex = [&](const char* key, float out[3]) {
            std::string s = core.dispatch(std::string("config get ") + key).data;
            size_t hp = s.find('#');
            unsigned r2, g2, b2;
            if (hp != std::string::npos &&
                std::sscanf(s.c_str() + hp, "#%02x%02x%02x", &r2, &g2, &b2) == 3) {
                out[0] = r2 / 255.0f; out[1] = g2 / 255.0f; out[2] = b2 / 255.0f;
            }
        };
        read_hex("theme.accent", theme_accent);
        for (int i = 0; i < 3; ++i) theme_accent2[i] = theme_accent[i]; // default: mirror
        read_hex("theme.accent2", theme_accent2); // override iff configured
        read_hex("theme.bg", theme_bg);
        read_hex("theme.ink", theme_ink);
        auto read_int = [&](const char* key, int& out, int lo, int hi) {
            std::string s = core.dispatch(std::string("config get ") + key).data;
            for (char c : s)
                if (c >= '0' && c <= '9') {
                    int val = c - '0';
                    if (val >= lo && val <= hi) out = val;
                    break;
                }
        };
        read_int("theme.preset", theme_preset, 0, 4);
        read_int("theme.font", theme_font, 0, kNumFonts - 1);
        read_int("theme.bodyfont", theme_bodyfont, 0, kNumFonts - 1);
        read_int("theme.scale", theme_scale, 0, 4);
        read_int("theme.radius", theme_radius, 0, 4);
        read_int("theme.texture", theme_texture, 0, 3);
        theme_dark = core.dispatch("config get theme.dark").data.find('0') ==
                     std::string::npos; // default ON; explicit "0" disables
        auto read_str = [&](const char* key, char* out, size_t n) {
            std::string v = core.dispatch(std::string("config get ") + key).data;
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                v = v.substr(1, v.size() - 2);
            if (v != "null" && !v.empty())
                std::snprintf(out, n, "%s", v.c_str());
        };
        read_str("site.base_url", site_base_url, sizeof site_base_url);
        read_str("site.desc", site_desc_buf, sizeof site_desc_buf);
        read_str("site.colophon", site_colophon_buf, sizeof site_colophon_buf);
        /* The axes added 2026-08-20. `read_int` above stops at the first digit,
         * which is right for a 0-4 enum and wrong for a 0-100 scrim, so the
         * multi-digit ones get their own reader. Loading these matters more
         * than it looks: a tab that shows defaults while the site renders
         * something else is a tab a person stops trusting. */
        read_int("theme.contrast", theme_contrast, 0, 2);
        read_int("theme.banner_filter", theme_banner_filter, 0, 5);
        read_int("theme.grid_gap", theme_grid_gap, 0, 4);
        {
            std::string v = core.dispatch("config get theme.banner_dim").data;
            size_t d = v.find_first_of("0123456789");
            if (d != std::string::npos)
                theme_banner_dim = std::clamp(std::atoi(v.c_str() + d), 0, 100);
            theme_grid_even =
                core.dispatch("config get theme.grid_even").data.find('0') ==
                std::string::npos;
            theme_icons = core.dispatch("config get theme.icons").data.find('0') ==
                          std::string::npos;
        }
        read_str("theme.accent_lite", theme_accent_lite, sizeof theme_accent_lite);
        read_str("theme.accent_dark", theme_accent_dark, sizeof theme_accent_dark);
        read_str("theme.font_custom", theme_font_custom, sizeof theme_font_custom);
    }

    read_view_config();
    apply_theme();

    // the "path" and "image" editor kinds, the gallery and the ImgBB upload
    // live in ui/widgets.cpp (register_image_editors)
    register_image_editors();

    // a "hidden" editor kind: renders nothing, so a field marked editor:hidden
    // drops out of the generic inspector. Used for builder-internal fields
    // (row/col/span/page/link_to/band_*) that the Builder manages with its own
    // dropdowns — the widget protocol's extension point, no upstream change.
    widgets.editors["hidden"] = [](maiz::WidgetContext&, const maiz::SceneNode&,
                                   const maiz::SceneField&, std::string_view) {
        return false;
    };

    // "enum" = a dropdown of the DISTINCT existing values of this field (across
    // same-glyph runes) plus a "+ add new" row — turns a free-text field like
    // `role` into pick-or-add, so the vocabulary self-organizes and nobody
    // retypes "Volunteer" three ways. A registered editor (the widget protocol
    // used as designed), so it also lights up faces + the future table. One
    // `set` on commit. (Author 2026-08-03: "dropdowns … with add new role".)
    widgets.editors["enum"] = [](maiz::WidgetContext& ctx, const maiz::SceneNode& n,
                                 const maiz::SceneField& f,
                                 std::string_view) -> bool {
        std::string cur = hormiga::temper::field_value(n, f.key.c_str());
        std::vector<std::string> opts;
        std::set<std::string> seen;
        for (const auto& m : ctx.scene.nodes) {
            if (m.glyph != n.glyph) continue;
            std::string v = hormiga::temper::field_value(m, f.key.c_str());
            if (!v.empty() && seen.insert(v).second) opts.push_back(v);
        }
        std::sort(opts.begin(), opts.end());
        if (!f.label.empty()) ImGui::TextUnformatted(f.label.c_str());
        ImGui::SetNextItemWidth(ctx.width > 0 ? ctx.width : -1);
        bool committed = false;
        auto commit = [&](const std::string& v) {
            ctx.commands.push_back("set " + n.name + " " + f.key + " \"" + v + "\"");
            committed = true;
        };
        std::string id = "##enum_" + f.key;
        if (ImGui::BeginCombo(id.c_str(), cur.empty() ? "(none)" : cur.c_str())) {
            if (ImGui::Selectable("(none)", cur.empty())) commit("");
            for (const auto& o : opts)
                if (ImGui::Selectable(o.c_str(), o == cur)) commit(o);
            ImGui::Separator();
            ImGui::TextDisabled("add new:");
            static char nb[64] = "";
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputTextWithHint("##enum_new", "type + Enter", nb, sizeof nb,
                                         ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string v = nb;
                while (!v.empty() && v.front() == ' ') v.erase(v.begin());
                while (!v.empty() && v.back() == ' ') v.pop_back();
                if (!v.empty()) {
                    commit(v);
                    nb[0] = 0;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndCombo();
        }
        return committed;
    };

    // "color" = a color-swatch editor over a "#rrggbb" string field (Allomone's
    // set-color action, and reusable anywhere). One `set` on release.
    widgets.editors["color"] = [](maiz::WidgetContext& ctx, const maiz::SceneNode& n,
                                  const maiz::SceneField& f, std::string_view) -> bool {
        std::string cur = hormiga::temper::field_value(n, f.key.c_str());
        float col[3] = {0.5f, 0.5f, 0.5f};
        unsigned rr, gg, bb;
        if (cur.size() >= 7 &&
            std::sscanf(cur.c_str(), "#%02x%02x%02x", &rr, &gg, &bb) == 3) {
            col[0] = rr / 255.f; col[1] = gg / 255.f; col[2] = bb / 255.f;
        }
        if (!f.label.empty()) ImGui::TextUnformatted(f.label.c_str());
        ImGui::SetNextItemWidth(ctx.width > 0 ? ctx.width : -1);
        ImGui::ColorEdit3(("##col_" + f.key).c_str(), col);
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            char hex[10];
            std::snprintf(hex, sizeof hex, "#%02x%02x%02x", (int)(col[0] * 255),
                          (int)(col[1] * 255), (int)(col[2] * 255));
            ctx.commands.push_back("set " + n.name + " " + f.key + " \"" + hex +
                                   "\"");
            return true;
        }
        return false;
    };

    // Secrets (the ImgBB Output holiday's key today): if an encrypted vault
    // exists, it is the source of truth and needs the passphrase (prompt on
    // first frame); otherwise the plaintext imgbb.key beside the app is used
    // (encryption is off by default, loudly offered — security.md §2). The
    // key never enters the command log or exported state either way.
    hormiga::Vault::global_init();
    if (hormiga::Vault::exists(vault_path().string())) {
        secrets_locked = true;
        vault_modal = VaultModal::Unlock; // ImgBB dormant until unlocked
    } else {
        load_secrets();
    }

    // Territory's action vocabulary — the command bar's `map …` verbs today,
    // the map canvas's gestures tomorrow; ONE compile serves both
    map_actions = hormiga::make_map_actions();
    doc_actions = hormiga::make_doc_actions(); // the Builder's verbs (B1)
    if (on_shell_capture) tiles.start(on_shell_capture); // OSM tile downloads

    palette.entries = {{"contact", "Contact", "People"},
                       {"organization", "Organization", "People"},
                       {"event", "Event", "Events"},
                       {"incident", "Incident", "Events"},
                       {"job", "Job", "Events"},
                       {"image", "Image", "Assets"},
                       {"resource", "Resource", "Assets"}};
    // note: `note` is intentionally NOT in the Data palette — notes have their
    // own tab now (draw_notes_body); the glyph is still registered in the model.
    /* THE BLOCK PALETTE IS NOT WRITTEN HERE. It comes from the glyph
     * declarations themselves (`hormiga::block_palette()`, filled by `block()`
     * in domain/glyphs_blocks.hpp), because the hand-maintained copy that used
     * to live here fell three blocks behind — `directory`, `event_feature` and
     * `event_flier` were all registered and renderable and could not be placed
     * from the GUI. Declare once; the palette follows. */
    palette_blocks.entries.clear();
    for (const auto& e : hormiga::block_palette())
        palette_blocks.entries.push_back({e.glyph, e.label, e.category});
    /* SORTED BY GROUP (2026-09-15), because the palette draws a heading whenever
     * the category changes and registration order gave "Content, Data, Content,
     * Data". Stable, so within a group the declaration order still decides. */
    auto cat_rank = [](const std::string& c) {
        return c == "Content" ? 0 : c == "Data" ? 1 : c == "Media" ? 2
               : c == "Interactive" ? 3 : 4;
    };
    std::stable_sort(palette_blocks.entries.begin(), palette_blocks.entries.end(),
                     [&](const auto& a, const auto& b) {
                         return cat_rank(a.category) < cat_rank(b.category);
                     });
    // grouped by PAYLOAD (Records / Assets / Publish), each entry local or cloud
    palette_antfarm.entries = {{"hol_sqlite", "SQLite - local store", "Records"},
                               {"hol_csv", "CSV - local source", "Records"},
                               {"hol_supabase", "Supabase - cloud store", "Records"},
                               {"hol_sheets", "Google Sheets - cloud source", "Records"},
                               {"hol_fs_assets", "Local files - asset store", "Assets"},
                               {"hol_imgbb", "ImgBB - cloud image host", "Assets"},
                               {"hol_html", "HTML site - publisher", "Publish"},
                               {"hol_localhost", "Localhost - local server", "Publish"},
                               {"hol_github", "GitHub Pages - cloud deploy", "Publish"},
                               /* registered for weeks and missing here, so they
                                * could not be placed from the GUI (2026-09-15) */
                               {"hol_static_host", "Static host - cloud deploy", "Publish"},
                               {"hol_dns", "Domain / DNS - registrar", "Publish"},
                               {"hol_object_store", "Object store - S3 or R2", "Assets"},
                               {"hol_uploads", "Uploads - visitor object store", "Assets"},
                               {"hol_lan_peer", "LAN peer - another device", "Records"},
                               {"hol_lan_share", "Share over LAN", "Records"},
                               {"hol_membership", "Members", "Records"},
                               {"hol_auth", "Sign-in - identity provider", "Visitors"},
                               {"hol_accounts", "Accounts + submissions", "Visitors"}};
    palette_allomone.entries = {{"allo_when", "When (a rule)", "Allomone"},
                                {"allo_hastag", "has tag", "Allomone"},
                                {"allo_setcolor", "set card color", "Allomone"}};

    // faces (block bodies): staged widgets, one `set` per completed edit
    // Allomone blocks show their VALUE inline (A2): the tag on `has tag`, a
    // swatch on `set card color`. (Editing is in the inspector for A2.1; in-block
    // widgets are A2.2.)
    faces.by_glyph["allo_hastag"] = [](maiz::FaceContext& ctx) {
        std::string t = hormiga::temper::field_value(ctx.node, "tag");
        ImGui::SetCursorScreenPos(ImVec2(ctx.pos.x + 8, ctx.pos.y + 2));
        if (t.empty()) ImGui::TextDisabled("(pick a tag)");
        else ImGui::Text("%s", t.c_str());
    };
    faces.by_glyph["allo_setcolor"] = [](maiz::FaceContext& ctx) {
        std::string c = hormiga::temper::field_value(ctx.node, "color");
        unsigned rr = 120, gg = 120, bb = 120;
        if (c.size() >= 7) std::sscanf(c.c_str(), "#%02x%02x%02x", &rr, &gg, &bb);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(ImVec2(ctx.pos.x + 8, ctx.pos.y + 3),
                          ImVec2(ctx.pos.x + 30, ctx.pos.y + ctx.size.y - 3),
                          IM_COL32(rr, gg, bb, 255), 3.0f);
        ImGui::SetCursorScreenPos(ImVec2(ctx.pos.x + 36, ctx.pos.y + 2));
        ImGui::TextDisabled("%s", c.empty() ? "(pick a color)" : c.c_str());
    };
    faces.by_glyph["event"] = [](maiz::FaceContext& ctx) {
        maiz::face_date(ctx, "date");
    };
    faces.by_glyph["note"] = [](maiz::FaceContext& ctx) {
        maiz::face_text_multiline(ctx, "text");
    };
    faces.by_glyph["narrative"] = [](maiz::FaceContext& ctx) {
        maiz::face_text_multiline(ctx, "text_en");
    };
    faces.by_glyph["footer"] = [](maiz::FaceContext& ctx) {
        maiz::face_text_multiline(ctx, "text_en");
    };
    faces.by_glyph["event_grid"] = [](maiz::FaceContext& ctx) {
        maiz::face_text_multiline(ctx, "query"); // resolves in the preview
    };
    faces.by_glyph["image_grid"] = [](maiz::FaceContext& ctx) {
        maiz::face_text_multiline(ctx, "query"); // query-backed, auto-updating
    };
    faces.by_glyph["job_grid"] = [](maiz::FaceContext& ctx) {
        maiz::face_text_multiline(ctx, "query");
    };
    faces.by_glyph["section_header"] = [](maiz::FaceContext& ctx) {
        maiz::face_text_multiline(ctx, "title_en");
    };
    // images render ON the canvas: the face shows the actual picture (the
    // old app's image library, as runes you can see and wire)
    faces.by_glyph["image"] = [this](maiz::FaceContext& ctx) {
        std::string p = field_value(ctx.node, "path");
        HostTexture t = p.empty() ? HostTexture{} : texture_for(p);
        maiz::face_image(ctx, (ImTextureID)(intptr_t)t.id, (float)t.w, (float)t.h,
                         p.empty() ? "no image - pick one in the inspector"
                                   : ("missing: " + p).c_str());
    };
    faces.by_glyph["hero"] = [this](maiz::FaceContext& ctx) {
        std::string p = field_value(ctx.node, "image");
        if (p.empty()) return; // banner optional; title lives in the inspector
        HostTexture t = texture_for(p);
        maiz::face_image(ctx, (ImTextureID)(intptr_t)t.id, (float)t.w, (float)t.h,
                         "banner");
    };

    // Antfarm faces: live describe() on each holiday node. Buttons push
    // commands into ctx.commands — the same door as every gesture.
    auto face_status = [](maiz::FaceContext& ctx, const std::string& l1,
                          const std::string& l2 = "") {
        ImGui::SetCursorScreenPos(ctx.pos);
        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(ctx.pos.x + ctx.size.x);
        ImGui::TextDisabled("%s", l1.c_str());
        if (!l2.empty()) ImGui::TextDisabled("%s", l2.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndGroup();
    };
    faces.by_glyph["org_core"] = [face_status](maiz::FaceContext& ctx) {
        face_status(ctx, "wiring IS configuration");
    };
    faces.by_glyph["hol_sqlite"] = [this, face_status](maiz::FaceContext& ctx) {
        if (storage && storage->ok()) {
            auto s = storage->stats();
            face_status(ctx,
                        std::to_string(s.runes) + " runes, " +
                            std::to_string(s.links) + " links, " +
                            std::to_string(s.bytes / 1024) + " KB",
                        s.saved_at.empty() ? "not saved yet" : "saved " + s.saved_at);
        } else {
            face_status(ctx, "UNAVAILABLE - running on snapshot");
        }
    };
    faces.by_glyph["hol_fs_assets"] = [this, face_status](maiz::FaceContext& ctx) {
        int files = 0;
        std::error_code ec;
        if (fs::exists(assets_dir()))
            for (const auto& e : fs::directory_iterator(assets_dir(), ec))
                if (e.is_regular_file(ec)) ++files;
        face_status(ctx, std::to_string(files) + " files in assets/",
                    "content-hash deduplicated");
    };
    faces.by_glyph["hol_html"] = [face_status](maiz::FaceContext& ctx) {
        face_status(ctx, "render live (Builder > Preview)");
    };
    faces.by_glyph["hol_localhost"] = [this, face_status](maiz::FaceContext& ctx) {
        if (host_srv.running()) {
            face_status(ctx, "SERVING the website like a domain",
                        "http://127.0.0.1:" + std::to_string(host_srv.port()) +
                            "/");
            ImGui::SetCursorScreenPos(
                ImVec2(ctx.pos.x, ctx.pos.y + ctx.size.y - 48.0f));
            if (ImGui::Button("Open", ImVec2(ctx.size.x, 0)) && on_open)
                on_open("http://127.0.0.1:" + std::to_string(host_srv.port()) +
                        "/");
            ImGui::SetCursorScreenPos(
                ImVec2(ctx.pos.x, ctx.pos.y + ctx.size.y - 24.0f));
            if (ImGui::Button("Stop host", ImVec2(ctx.size.x, 0)))
                host_srv.stop();
        } else {
            face_status(ctx, "the website as a LOCAL SITE (not a file)",
                        "a stand-in for your future domain");
            ImGui::SetCursorScreenPos(
                ImVec2(ctx.pos.x, ctx.pos.y + ctx.size.y - 24.0f));
            if (ImGui::Button("Serve site locally", ImVec2(ctx.size.x, 0)))
                host_start();
        }
    };
    faces.by_glyph["hol_imgbb"] = [this, face_status](maiz::FaceContext& ctx) {
        if (mirror_count < 0) { // lazy first read of the mirror cache
            mirror_count = 0;
            std::ifstream in(assets_dir() / "mirror.json");
            if (in) {
                try {
                    mirror_count = (int)nlohmann::json::parse(in).size();
                } catch (...) {}
            }
        }
        face_status(ctx,
                    imgbb_key.empty() ? "no key - holiday dormant"
                                      : "key present - Publish is live",
                    "mirror: " + std::to_string(mirror_count) + " urls local");
        ImGui::SetCursorScreenPos(ImVec2(ctx.pos.x, ctx.pos.y + ctx.size.y - 26.0f));
        if (ImGui::Button("Mirror cloud -> local", ImVec2(ctx.size.x, 0)))
            ctx.commands.push_back("effect mirror-images");
    };
    faces.by_glyph["hol_supabase"] = [this, face_status](maiz::FaceContext& ctx) {
        std::string dir = field_value(ctx.node, "dump_dir");
        bool found = !dir.empty() && fs::exists(base_dir / dir / "manifest.json");
        face_status(ctx, found ? "rescue dump found (copy-only, verified)"
                               : "dump not found at " + dir);
        if (found) {
            ImGui::SetCursorScreenPos(
                ImVec2(ctx.pos.x, ctx.pos.y + ctx.size.y - 26.0f));
            if (ImGui::Button("Import now", ImVec2(ctx.size.x, 0)))
                ctx.commands.push_back("effect import-rescue");
        }
    };
    faces.by_glyph["hol_sheets"] = [face_status](maiz::FaceContext& ctx) {
        face_status(ctx, "not configured", "import-only lean (Q4)");
    };
    faces.by_glyph["hol_csv"] = [face_status](maiz::FaceContext& ctx) {
        face_status(ctx, "live: Data > Import CSV...");
    };
    register_hosting_faces(); // image hosts: can it answer, and "Use for images"

    // the seed references assets/flyer-taller.png — make it REAL so image
    // display works out of the box (procedural demo flyer, gitignored like
    // everything under assets/; regenerated only when absent)
    {
        fs::path flyer = assets_dir() / "flyer-taller.png";
        if (!fs::exists(flyer)) {
            std::error_code ec;
            fs::create_directories(assets_dir(), ec);
            const int W = 400, H = 520;
            std::vector<unsigned char> px(W * H * 3);
            for (int y = 0; y < H; ++y)
                for (int x = 0; x < W; ++x) {
                    unsigned char* p = &px[(y * W + x) * 3];
                    bool band = (y > 60 && y < 120) || (y > H - 90 && y < H - 50);
                    p[0] = band ? 212 : (unsigned char)(40 + 60 * y / H);
                    p[1] = band ? 160 : (unsigned char)(90 + 40 * x / W);
                    p[2] = band ? 23 : (unsigned char)(110 + 80 * y / H);
                }
            stbi_write_png(flyer.string().c_str(), W, H, 3, px.data(), W * 3);
        }
    }

    switch_section(Data);
    reproject();
    // dev affordances for screenshot-driven verification (harmless otherwise):
    // HORMIGA_SECTION=data|builder|antfarm|map, HORMIGA_SELECT=<rune>
    if (const char* sec = std::getenv("HORMIGA_SECTION")) {
        std::string s2(sec);
        if (s2 == "builder") switch_section(Builder);
        else if (s2 == "antfarm") switch_section(Antfarm);
        else if (s2 == "map") switch_section(Map);
        else if (s2 == "calendar") boot_focus_cal = true; // a window, not a section
        else if (s2 == "notes") boot_focus_notes = true;  // a window, not a section
        else if (s2 == "allomone") boot_focus_allomone = win_allomone = true; // a window, off by default
#if HORMIGA_LEGACY_ALLOMONE
        else if (s2 == "allodev") boot_focus_allodev = true;   // a window
#endif
        boot_section = section;
    }
    if (const char* sel = std::getenv("HORMIGA_SELECT"))
        if (scene.find(sel)) ed.selection = {sel};
    if (std::getenv("HORMIGA_CONNECTIONS")) data_show_connections = true;
    if (std::getenv("HORMIGA_DATATOOLS")) win_data_tools = true; // dev/test seam
    if (const char* w = std::getenv("HORMIGA_WINDOWS")) { // dev/test seam: "profile,share,discover"
        const std::string ws = w;
        win_profile = ws.find("profile") != std::string::npos;
        win_share = ws.find("share") != std::string::npos;
        win_discover = ws.find("discover") != std::string::npos;
    }
    if (std::getenv("HORMIGA_DATACARDS")) data_view_mode = 1;    // dev/test seam
    if (const char* df = std::getenv("HORMIGA_DATAFILTER")) {    // dev/test seam
        std::string s = df, one;                                // comma-separated
        auto flush = [&] {
            while (!one.empty() && one.front() == ' ') one.erase(one.begin());
            if (!one.empty()) data_filter_terms.push_back({false, one});
            one.clear();
        };
        for (char ch : s) { if (ch == ',') flush(); else one += ch; }
        flush();
        std::string e = compile_filter(data_filter_terms, data_filter_join);
        std::snprintf(filter, sizeof filter, "%s", e.c_str());
    }
    // HORMIGA_BOOT_CMD=<command>: dispatched on the first frame — the same
    // dispatcher door as the command bar. ` && `-separated for multi-step
    // dev flows (a mini transcript without a script file).
    if (const char* cmd = std::getenv("HORMIGA_BOOT_CMD")) {
        std::string s = cmd, one;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s.compare(i, 4, " && ") == 0) {
                pending_cmds.push_back(one);
                one.clear();
                i += 3;
            } else one += s[i];
        }
        if (!one.empty()) pending_cmds.push_back(one);
    }
    /* ── THE TITLE BAR ANSWERS "WHICH DOCUMENT IS THIS?" ─────────────────────
     *
     * It said "Hormiga" and nothing else, so the one question that mattered —
     * which of two identically-named databases am I looking at — had no answer
     * visible anywhere in the application. The Publish panel already shows the
     * resolved absolute `token_file` for exactly this reason; this is that
     * instinct applied to the thing everything else hangs off.
     *
     * The FOLDER is what disambiguates (every org's document is `<name>.json`
     * in a different place), so it leads, and the full path follows for the
     * case where two folders share a name. */
    if (on_title) {
        std::error_code tec;
        const fs::path abs = fs::absolute(org_file(), tec);
        on_title(tec ? std::string("Hormiga")
                     : "Hormiga - " + abs.parent_path().filename().string() +
                           " - " + abs.string());
    }

    /* LAST, AND ONLY WHERE A PERSON IS LOOKING. `updates_boot` reads a
     * preferences file and then either asks a question, starts a check, or does
     * nothing -- it never decides to go to the network on its own
     * (ui/updates.cpp says why). It is at the END of init because a check must
     * not be able to delay or interfere with opening somebody's database, which
     * is the thing they actually came here to do.
     *
     * `offer_updates` IS AN EXPLICIT FLAG RATHER THAN A TEST OF SOMETHING ELSE,
     * and the near-miss is the reason. The obvious guard was `on_shell_capture`
     * -- "do we have a transport?" -- and the headless front-end sets that too:
     * it builds a throwaway `HormigaApp` and calls `init()` to render a
     * newsletter, which would have made a network request during a command that
     * asked for a newsletter. An agent's `render` is not consent to check for
     * updates. Only the desktop shell sets this. */
    if (offer_updates) updates_boot();
}

void HormigaApp::shutdown() {
    do_save();
    // Release the floor AFTER the save, so an agent waiting to start cannot
    // begin against a half-written document.
    if (g_floor) { g_floor->close(); g_floor.reset(); }
}

// ── + New (mint, tag, select — the predecessor's "add" buttons, as commands) ─

void HormigaApp::new_rune(const std::string& glyph) {
    const std::string name = mint_name(glyph);
    // mint + tag as ONE undo frame (compile_commit, the batch helper that
    // landed on our bruise report)
    dispatch_and_reproject(maiz::compile_commit(
        {"rune new " + glyph + " " + name, "tag " + name + " +type:" + glyph}));
    ed.selection = {name};
    kind_sel = glyph;
    search[0] = 0;
    toast("added " + name + " - fill in its details");
}

// ── asset ingestion + CSV import (phase C) ──────────────────────────────────

/* Copy a picked file into assets/ under a content-hashed, deduplicated name;
 * return the repo-relative path ("" on failure). Same bytes = same name, so
 * re-ingesting a file is free. */
std::string HormigaApp::ingest_asset(const std::string& src, bool quiet) {
    std::ifstream in(src, std::ios::binary);
    if (!in) {
        if (!quiet) toast("cannot read " + src, true);
        return {};
    }
    std::stringstream ss;
    ss << in.rdbuf();
    char hex[20];
    std::snprintf(hex, sizeof hex, "%08llx", (unsigned long long)(fnv1a64(ss.str()) & 0xffffffffull));
    fs::path p(src);
    std::string name = hormiga::detail::slug(p.stem().string());
    if (name.empty()) name = "asset";
    std::string ext = p.extension().string();
    fs::path dest = assets_dir() / (name + "-" + hex + ext);
    std::error_code ec;
    fs::create_directories(assets_dir(), ec);
    if (!fs::exists(dest)) {
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            if (!quiet) toast("copy failed: " + ec.message(), true);
            return {};
        }
        if (!quiet) toast("ingested into assets/" + dest.filename().string());
    } else if (!quiet) {
        toast("already in assets/ (same content) - reused");
    }
    return "assets/" + dest.filename().string();
}

/* The MIRROR pipeline — publish's twin, cloud → local (the concept:
 * okf/concepts/platform/antfarm.md "publish & mirror"). Walk every rune field that
 * references a remote URL; make the bytes local by the cheapest honest
 * route: mirror cache hit → skip; the rescue's media/ has it → copy;
 * else download. Results land in two tiers:
 *   - bytes in content-hashed assets/ + assets/mirror.json (url → path):
 *     holiday CACHE, reconstructible, deliberately not model truth;
 *   - image runes missing a local path get ONE `set path` command each —
 *     model change, logged, undoable.
 * Dead URLs are recorded as "" — evidence, and no re-hammering the host. */
void HormigaApp::mirror_images_effect() {
    using nlohmann::json;
    if (job) { toast("a background job is already running", true); return; }

    // ── MAIN THREAD: gather everything the worker needs (all core access is
    // here; the worker below touches no dispatcher, scene, or shared UI state)
    maiz::ProjectOptions po;
    po.mantle = kDataMantle;
    maiz::Scene data = maiz::project_scene(core, po);

    std::vector<std::string> urls;              // every distinct remote ref
    std::map<std::string, std::string> url_rune; // image-rune url → name (relink)
    std::set<std::string> seen;
    for (const auto& n : data.nodes) {
        if (n.glyph == "image") {
            std::string u = field_value(n, "url"), p = field_value(n, "path");
            if (p.empty() && !u.empty()) url_rune[u] = n.name;
        }
        for (const auto& f : n.fields) {
            bool url_field = f.key == "image_url" || f.key == "icon_url" ||
                             (n.glyph == "image" && f.key == "url");
            if (!url_field) continue;
            std::string v = field_value(n, f.key);
            if (v.rfind("http", 0) == 0 && seen.insert(v).second) urls.push_back(v);
        }
    }

    json map = json::object();
    fs::path mpath = assets_dir() / "mirror.json";
    if (fs::exists(mpath)) {
        try { std::ifstream in(mpath); map = json::parse(in); } catch (...) {}
    }

    std::map<std::string, fs::path> rescue; // rescue media already on disk
    {
        maiz::ProjectOptions fpo;
        fpo.mantle = kAntfarmMantle;
        maiz::Scene farm = maiz::project_scene(core, fpo);
        std::string dir = "../Hormiga/supabase_rescue";
        if (const maiz::SceneNode* n = farm.find("import-supabase")) {
            std::string d = field_value(*n, "dump_dir");
            if (!d.empty()) dir = d;
        }
        fs::path manifest = base_dir / dir / "media_manifest.json";
        if (fs::exists(manifest)) {
            try {
                std::ifstream in(manifest);
                json mm = json::parse(in);
                for (const auto& f : mm["files"])
                    if (!f.contains("error") && f.contains("file"))
                        rescue[f["url"].get<std::string>()] =
                            base_dir / dir / "media" / f["file"].get<std::string>();
            } catch (...) {}
        }
    }

    // ── hand off to a WORKER: pure file/network I/O, no core, no UI state.
    // Model change (the `set path` relinks) comes back through `results` and
    // is dispatched on the main thread — the dispatcher stays the one door.
    auto j = std::make_unique<Job>();
    j->label = "Mirroring images (cloud -> local)";
    j->total = (int)urls.size();
    Job* jp = j.get(); // valid after the move; the thread uses this, not `job`
    jp->worker = std::thread([this, jp, urls, rescue, url_rune, map, mpath]() mutable {
        int from_rescue = 0, downloaded = 0, cached = 0, dead = 0;
        std::error_code ec;
        fs::create_directories(assets_dir(), ec);
        int idx = 0;
        for (const auto& url : urls) {
            std::string local;
            bool settled = false;
            if (map.contains(url)) {
                std::string rel = map[url].get<std::string>();
                if (rel.empty()) { // known-dead: no bytes, no relink, move on
                    ++dead;
                    jp->done = ++idx;
                    continue;
                }
                if (fs::exists(base_dir / rel)) { local = rel; ++cached; settled = true; }
            }
            if (!settled) { // not cached: copy from rescue media, else download
                auto rit = rescue.find(url);
                if (rit != rescue.end() && fs::exists(rit->second)) {
                    local = ingest_asset(rit->second.string(), true);
                    if (!local.empty()) ++from_rescue;
                } else if (on_shell_capture) {
                    std::string ext =
                        fs::path(url.substr(url.rfind('/') + 1)).extension().string();
                    if (ext.empty() || ext.size() > 6) ext = ".bin";
                    fs::path tmp = assets_dir() / ("mirror-dl" + ext);
                    fs::remove(tmp, ec);
                    on_shell_capture("curl -s -L -o \"" + tmp.string() + "\" \"" + url +
                                     "\"");
                    if (fs::exists(tmp) && fs::file_size(tmp, ec) > 100) {
                        local = ingest_asset(tmp.string(), true);
                        if (!local.empty()) ++downloaded;
                    }
                    fs::remove(tmp, ec);
                }
                map[url] = local; // "" when dead — evidence, never re-hammered
                if (local.empty()) ++dead;
            }
            // relink the rune whether the bytes were cached OR fetched — a
            // fresh import has empty paths but a populated cache, and those
            // runes still need their `path` set (the bug that shipped 1/72)
            auto rn = url_rune.find(url);
            if (!local.empty() && rn != url_rune.end())
                jp->results.push_back("set " + rn->second + " path " +
                                      json_str(local));
            jp->done = ++idx;
        }
        { std::ofstream out(mpath, std::ios::binary | std::ios::trunc);
          out << map.dump(1); }
        jp->summary = "mirror: " + std::to_string(from_rescue) + " from rescue, " +
                      std::to_string(downloaded) + " downloaded, " +
                      std::to_string(cached) + " settled, " + std::to_string(dead) +
                      " dead, " + std::to_string(jp->results.size()) + " relinked";
        jp->finished = true;
    });
    job = std::move(j);
}

/* The progress panel for a running background job — a centered modal-style
 * window with a determinate bar. Drawn every frame while a job lives. */
void HormigaApp::draw_job_overlay() {
    if (!job) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->GetCenter().x, vp->GetCenter().y),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360, 0));
    ImGui::Begin("##job", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_AlwaysAutoResize);
    int done = job->done.load(), total = job->total.load();
    ImGui::TextUnformatted(job->label.c_str());
    float frac = total > 0 ? (float)done / (float)total : 0.0f;
    char ov[32];
    std::snprintf(ov, sizeof ov, "%d / %d", done, total);
    ImGui::ProgressBar(frac, ImVec2(-1, 0), ov);
    ImGui::TextDisabled("downloads run in the background; the org stays "
                        "responsive.");
    ImGui::End();
}

/* The date-tag temper pass: derive month:/season: tags from event dates
 * (temper.hpp). Idempotent — re-running adds nothing — so it's safe to run
 * on a button or automatically after an import. One batch, one undo frame. */
void HormigaApp::derive_date_tags() {
    maiz::ProjectOptions po;
    po.mantle = kDataMantle;
    maiz::Scene data = maiz::project_scene(core, po);
    auto cmds = hormiga::temper::compile_date_tags(data);
    if (cmds.empty()) {
        toast("date tags already up to date");
        return;
    }
    if (scene.mantle != kDataMantle)
        dispatch_and_reproject(std::string("use ") + kDataMantle);
    dispatch_and_reproject(maiz::compile_commit(cmds));
    toast("tagged " + std::to_string(cmds.size()) +
          " events by month + season - now @season:summer filters them");
}

/* Pick a CSV, compile it to commands (import.hpp), land it as ONE batch =
 * one undo frame. The file is read once; the org keeps only commands. */
void HormigaApp::run_csv_import(const std::string& glyph) {
    if (!on_pick_file) return;
    std::string path = on_pick_file("");
    if (path.empty()) return;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        toast("cannot read " + path, true);
        return;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    auto res = hormiga::compile_csv_import(
        ss.str(), glyph, hormiga::glyph_fields(core, glyph),
        [this](const std::string& n) { return scene.find(n) != nullptr; });
    if (!res.error.empty()) {
        toast("import failed: " + res.error, true);
        return;
    }
    for (const auto& note : res.notes) log.push_back({"warn", "import", note});
    if (res.commands.empty()) {
        toast("nothing to import", true);
        return;
    }
    dispatch_and_reproject(maiz::compile_commit(res.commands));
    kind_sel = glyph;
    toast("imported " + std::to_string(res.rows) + " " + glyph + "(s) - one undo frame");
}

// ── images: texture cache + the opt-in ImgBB publish holiday ────────────────

HormigaApp::HostTexture HormigaApp::texture_for(const std::string& path) {
    auto it = tex_cache.find(path);
    if (it != tex_cache.end()) return it->second;
    // per-frame decode budget: a list showing dozens of not-yet-loaded images
    // (the Data section after a rescue import) must not decode+upload them all
    // in one frame — that was the multi-second boot stall. Over budget, return
    // "not ready yet" WITHOUT caching, so it retries next frame; the app
    // repaints continuously, so a full library fills in over a few frames.
    if (tex_decodes_this_frame >= 4) return {};
    ++tex_decodes_this_frame;
    HostTexture t{};
    if (on_load_texture) {
        fs::path p(path);
        t = on_load_texture(resolve_file(path).string()); // demo-assets travel with the program
    }
    tex_cache[path] = t; // an actual attempt (incl. failure) is cached
    return t;
}

// The best LOCAL photo for an entity: the avatar field, else (for an image
// rune) its own path, else the legacy image_url — but only if it names a local
// file, since http URLs can't be textured on the desktop (they're for the web
// pack). "" ⇒ no photo, callers draw the procedural placeholder.
std::string HormigaApp::avatar_path(const maiz::SceneNode& n) const {
    auto local = [](const std::string& s) {
        return !s.empty() && s.rfind("http", 0) != 0;
    };
    std::string a = hormiga::temper::field_value(n, "avatar");
    if (local(a)) return a;
    if (n.glyph == "image") {
        std::string p = hormiga::temper::field_value(n, "path");
        if (local(p)) return p;
    }
    std::string iu = hormiga::temper::field_value(n, "image_url");
    if (local(iu)) return iu;
    return "";
}

// Draw a circular avatar at `center` with `radius`: the photo clipped to a
// disc if one resolves, otherwise a hash-colored disc with the entity's
// initials — so every row/card has a face, never a blank (UI/UX phase #1).
void HormigaApp::draw_avatar(const maiz::SceneNode& n, ImVec2 c, float r) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    std::string p = avatar_path(n);
    HostTexture t = p.empty() ? HostTexture{} : texture_for(p);
    if (t.id) { // photo → disc (square + full rounding = circle)
        dl->AddImageRounded((ImTextureID)(intptr_t)t.id, ImVec2(c.x - r, c.y - r),
                            ImVec2(c.x + r, c.y + r), ImVec2(0, 0), ImVec2(1, 1),
                            IM_COL32_WHITE, r);
        dl->AddCircle(c, r, IM_COL32(0, 0, 0, 45), 0, 1.5f); // hairline ring
        return;
    }
    // placeholder: a distinct colour per entity (FNV hash of the name → hue),
    // desaturated so white initials stay legible
    unsigned h = 2166136261u;
    for (char ch : n.name) h = (h ^ (unsigned char)ch) * 16777619u;
    ImVec4 rgb = (ImVec4)ImColor::HSV((h % 360) / 360.0f, 0.42f, 0.62f);
    dl->AddCircleFilled(c, r, ImGui::GetColorU32(rgb));
    std::string ini; // up to two initials from the name's words (slug: split '-')
    bool at_start = true;
    for (char ch : n.name) {
        if (ch == '-' || ch == '_' || std::isspace((unsigned char)ch)) {
            at_start = true;
            continue;
        }
        if (at_start && ini.size() < 2) ini += (char)std::toupper((unsigned char)ch);
        at_start = false;
    }
    if (ini.empty()) ini = "?";
    ImVec2 ts = ImGui::CalcTextSize(ini.c_str());
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32_WHITE,
                ini.c_str());
}

/* `effect publish <rune>` lands here: upload the rune's local asset to ImgBB
 * (permanent — NO expiration param; newsletter images must outlive the send)
 * and queue ONE `set <rune> url ...` so the result is logged and undoable.
 * The key rides only in the process invocation, never in the command log. */
void HormigaApp::publish_image(const std::string& rune) {
    host_image(rune); // since 2026-09-15: whichever image host the Antfarm has
}

/* The rescue import, now TWO independently-guarded passes sharing one
 * button — phase one (clean tables) and phase two (json_store's freeform
 * connections, image library, jobs board, per-row tags). Each lands as its
 * own compile_commit batch (its own undo frame — semantically different
 * bulk operations), and each checks its own config flag, so a click after
 * phase one already ran (a prior session, say) still delivers phase two. */
void HormigaApp::import_rescue_effect() {
    maiz::ProjectOptions po;
    po.mantle = kAntfarmMantle;
    maiz::Scene farm = maiz::project_scene(core, po);
    std::string dir = "../Hormiga/supabase_rescue";
    if (const maiz::SceneNode* n = farm.find("import-supabase")) {
        std::string d = field_value(*n, "dump_dir");
        if (!d.empty()) dir = d;
    }
    auto slurp = [&](const char* name) -> std::string {
        std::ifstream in(base_dir / dir / "tables" / name, std::ios::binary);
        if (!in) return {};
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    };
    maiz::ProjectOptions dpo;
    dpo.mantle = kDataMantle;
    maiz::Scene data = maiz::project_scene(core, dpo);
    bool did_something = false;

    std::string done1 = core.dispatch("config get import.rescue_done").data;
    if (done1.find('1') == std::string::npos) {
        auto res = hormiga::compile_rescue_import(
            slurp("contacts.json"), slurp("organizations.json"), slurp("events.json"),
            slurp("presenters.json"),
            [&](const std::string& n) { return data.find(n) != nullptr; });
        if (!res.error.empty()) {
            toast("rescue import (tables): " + res.error, true);
        } else if (res.commands.empty()) {
            toast("rescue import: nothing found under " + dir, true);
        } else {
            for (const auto& note : res.notes) log.push_back({"warn", "import", note});
            // synchronous + ok-checked (see phase two's comment below for
            // why this matters — a deferred, unconditional flag write would
            // mark this "done" even if the batch rolled back)
            core.dispatch(std::string("use ") + kDataMantle);
            maiz::Result r = dispatch_and_reproject(maiz::compile_commit(res.commands));
            if (r.ok) {
                core.dispatch("config set import.rescue_done 1");
                toast("imported " + std::to_string(res.contacts) + " contacts, " +
                      std::to_string(res.organizations) + " orgs, " +
                      std::to_string(res.events) + " events, " +
                      std::to_string(res.presenters) + " presenters, " +
                      std::to_string(res.links) + " links - one undo frame");
                did_something = true;
                // re-project so phase two below sees phase one's runes if
                // both fire in the same click (a truly fresh org)
                data = maiz::project_scene(core, dpo);
            } else {
                log.push_back({"error", "import", r.text()});
                toast("rescue import (tables) FAILED: " + r.text(), true);
            }
        }
    }

    std::string done2 = core.dispatch("config get import.rescue_media_done").data;
    if (done2.find('1') == std::string::npos) {
        auto slurp_meta = [&](const char* key) -> std::string {
            std::ifstream in(base_dir / dir / "tables" / "json_store.json",
                             std::ios::binary);
            if (!in) return {};
            std::stringstream ss;
            ss << in.rdbuf();
            try {
                auto j = nlohmann::json::parse(ss.str());
                for (const auto& row : j)
                    if (row.value("key", "") == key) return row["data"].dump();
            } catch (...) {}
            return {};
        };
        auto res2 = hormiga::compile_json_store_import(
            slurp_meta("graph"), slurp_meta("images"), slurp_meta("jobs"),
            slurp_meta("contacts_meta"), slurp_meta("events_meta"),
            slurp_meta("presenter_meta"), slurp("contacts.json"),
            slurp("events.json"), slurp("presenters.json"),
            [&](const std::string& n) { return data.find(n) != nullptr; },
            [&](const std::string& n) {
                const maiz::SceneNode* node = data.find(n);
                return node ? node->glyph : std::string();
            });
        if (!res2.error.empty()) {
            toast("rescue import (extras): " + res2.error, true);
        } else if (res2.commands.empty()) {
            toast("rescue import (extras): nothing new found", true);
        } else {
            for (const auto& note : res2.notes) log.push_back({"warn", "import", note});
            // dispatched HERE, synchronously — not deferred through
            // pending_cmds — so we can check r.ok before ever claiming
            // success. A prior version queued the "done" flag unconditionally
            // and a batch rollback (name collision across glyphs — Void
            // Core enforces mantle-wide uniqueness, not per-glyph) silently
            // marked phase two done with nothing actually imported.
            core.dispatch(std::string("use ") + kDataMantle);
            maiz::Result r = dispatch_and_reproject(maiz::compile_commit(res2.commands));
            if (r.ok) {
                core.dispatch("config set import.rescue_media_done 1");
                toast("imported " + std::to_string(res2.images) + " images, " +
                      std::to_string(res2.jobs) + " jobs, " +
                      std::to_string(res2.graph_edges) + " connections, " +
                      std::to_string(res2.meta_tags) +
                      " tag batches - one undo frame. Try Mirror cloud -> local next.");
                did_something = true;
            } else {
                log.push_back({"error", "import", r.text()});
                toast("rescue import (extras) FAILED: " + r.text(), true);
            }
        }
    }

    pending_cmds.push_back(std::string("use ") + kAntfarmMantle);
    if (did_something)
        run_temper_next = true; // date-tag the freshly-imported events
    if (!did_something && done1.find('1') != std::string::npos &&
        done2.find('1') != std::string::npos)
        toast("rescue already fully imported - undo or start a fresh org to redo", true);
}


// ── the physics connections view (force-directed, Gephi-class) ──────────────
// NOT the node editor: a visualization. Fruchterman–Reingold forces (their
// layout list's classic), nodes sized by degree, spatial PROXIMITY folded in
// as weak springs (things near each other on the map pull together here too),
// a settling progress bar, pan/zoom/drag/click-select. Positions are VIEW
// state — never dispatched, never stored.

void HormigaApp::phys_reset() {
    phys_pos.clear();
    phys_total_iters = 260;
    phys_done_iters = 0;
    phys_temp = 1.0f;
    unsigned seed = 12345;
    auto rnd = [&seed] { // deterministic layout for a given org
        seed = seed * 1664525u + 1013904223u;
        return (float)(seed >> 8) / (float)(1u << 24);
    };
    for (const auto& n : scene.nodes) {
        float a = rnd() * 6.2831853f, r = 60.0f + 240.0f * rnd();
        phys_pos[n.name] = ImVec2(std::cos(a) * r, std::sin(a) * r);
    }
}

void HormigaApp::draw_physics_view(float /*body_h*/) {
    // toolbar: the layout family (their Gephi screenshot honored honestly)
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("##physlayout", "Fruchterman-Reingold")) {
        ImGui::Selectable("Fruchterman-Reingold", true);
        ImGui::TextDisabled("ForceAtlas 2 (planned)");
        ImGui::TextDisabled("Yifan Hu (planned)");
        ImGui::TextDisabled("OpenOrd (planned)");
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Re-run layout")) phys_reset();
    ImGui::SameLine();
    ImGui::TextDisabled("size = connections | click: select | drag: move | "
                        "wheel: zoom");

    // degree + edge list (proximity folds in as weak springs)
    std::map<std::string, int> degree;
    struct PEdge { std::string a, b; float w; };
    std::vector<PEdge> pedges;
    for (const auto& w : scene.wires) {
        ++degree[w.from];
        ++degree[w.to];
        pedges.push_back({w.from, w.to, 1.0f});
    }
    if (map_show_prox) { // the map's derived SPATIAL proximity, here too
        std::vector<std::pair<std::string, std::pair<double, double>>> loc;
        for (const auto& n : scene.nodes) {
            std::string g = hormiga::temper::field_value(n, "geo");
            double la, lo;
            if (!g.empty() && hormiga::parse_geo(g, la, lo))
                loc.push_back({n.name, {la, lo}});
        }
        for (size_t i = 0; i < loc.size(); ++i)
            for (size_t j = i + 1; j < loc.size(); ++j) {
                double d = hormiga::geo_distance_m(loc[i].second.first,
                                                   loc[i].second.second,
                                                   loc[j].second.first,
                                                   loc[j].second.second);
                if (d <= map_prox_m)
                    pedges.push_back({loc[i].first, loc[j].first,
                                      0.25f * (1.0f - (float)(d / map_prox_m))});
            }
    }
    if (time_prox) { // derived TEMPORAL proximity: dated things cluster by
                     // when they happen (events near events, incidents near
                     // the events they disrupted) — computed, never stored
        std::vector<std::pair<std::string, long>> dated;
        for (const auto& n : scene.nodes) {
            std::string d = hormiga::temper::field_value(n, "date");
            int y, mo, dd;
            if (std::sscanf(d.c_str(), "%d-%d-%d", &y, &mo, &dd) == 3)
                dated.push_back({n.name, (long)y * 372 + mo * 31 + dd});
        }
        for (size_t i = 0; i < dated.size(); ++i)
            for (size_t j = i + 1; j < dated.size(); ++j) {
                long dt = std::labs(dated[i].second - dated[j].second);
                if (dt <= (long)time_prox_days)
                    pedges.push_back(
                        {dated[i].first, dated[j].first,
                         0.3f * (1.0f - (float)dt / (float)time_prox_days)});
            }
    }
    // participants: connected nodes (a pure isolate cloud isn't a graph)
    std::vector<const maiz::SceneNode*> nodes;
    for (const auto& n : scene.nodes)
        if (degree.count(n.name) || ed.selected(n.name)) nodes.push_back(&n);
    if ((int)phys_pos.size() != (int)scene.nodes.size()) phys_reset();

    // settle a few iterations per frame (the loading bar the author wanted)
    const float K = 70.0f; // ideal spacing
    if (phys_done_iters < phys_total_iters) {
        int steps = 4;
        while (steps-- > 0 && phys_done_iters < phys_total_iters) {
            float t = 1.0f - (float)phys_done_iters / (float)phys_total_iters;
            float temp = 4.0f + 60.0f * t * t; // cools quadratically
            std::map<std::string, ImVec2> disp;
            for (auto* a : nodes) // repulsion
                for (auto* b : nodes) {
                    if (a == b) continue;
                    ImVec2 pa = phys_pos[a->name], pb = phys_pos[b->name];
                    float dx = pa.x - pb.x, dy = pa.y - pb.y;
                    float d2 = dx * dx + dy * dy + 0.01f;
                    float f = K * K / d2;
                    disp[a->name].x += dx * f;
                    disp[a->name].y += dy * f;
                }
            for (const auto& e : pedges) { // attraction (weighted springs)
                ImVec2 pa = phys_pos[e.a], pb = phys_pos[e.b];
                float dx = pa.x - pb.x, dy = pa.y - pb.y;
                float d = std::sqrt(dx * dx + dy * dy) + 0.01f;
                float f = (d * d / K) * e.w / d;
                disp[e.a].x -= dx * f;
                disp[e.a].y -= dy * f;
                disp[e.b].x += dx * f;
                disp[e.b].y += dy * f;
            }
            for (auto* a : nodes) { // apply, clamped by temperature + gravity
                if (a->name == phys_drag) continue; // pinned while dragged
                ImVec2& p = phys_pos[a->name];
                ImVec2 d = disp[a->name];
                float len = std::sqrt(d.x * d.x + d.y * d.y) + 0.001f;
                float lim = std::min(len, temp);
                p.x += d.x / len * lim;
                p.y += d.y / len * lim;
                p.x *= 0.995f; // gentle gravity to origin
                p.y *= 0.995f;
            }
            ++phys_done_iters;
        }
    }

    // canvas (guard: a collapsed pane must not feed ImGui a <=0 button size)
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 16 || sz.y < 16) return;
    ImVec2 ctr(p0.x + sz.x * 0.5f + phys_cam.x, p0.y + sz.y * 0.5f + phys_cam.y);
    dl->PushClipRect(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), true);
    ImGui::InvisibleButton("##phys-surface", sz,
                           ImGuiButtonFlags_MouseButtonLeft);
    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;
    auto to_s = [&](ImVec2 p) {
        return ImVec2(ctr.x + p.x * phys_zoom, ctr.y + p.y * phys_zoom);
    };

    for (const auto& e : pedges) { // edges (proximity springs drawn fainter)
        ImVec2 a = to_s(phys_pos[e.a]), b = to_s(phys_pos[e.b]);
        dl->AddLine(a, b,
                    IM_COL32(110, 125, 145, e.w >= 1.0f ? 120 : 45),
                    e.w >= 1.0f ? 1.4f : 1.0f);
    }
    const maiz::SceneNode* hit = nullptr;
    for (auto* n : nodes) {
        ImVec2 s = to_s(phys_pos[n->name]);
        int deg = degree.count(n->name) ? degree[n->name] : 0;
        float r = (3.5f + 2.0f * std::sqrt((float)deg)) * phys_zoom;
        bool selected = ed.selected(n->name);
        ImU32 col = n->glyph == "incident"       ? IM_COL32(200, 50, 50, 255)
                    : n->glyph == "organization" ? IM_COL32(138, 109, 59, 255)
                    : n->glyph == "event"        ? IM_COL32(63, 111, 174, 255)
                    : n->glyph == "image"        ? IM_COL32(125, 91, 176, 255)
                    : n->glyph == "job"          ? IM_COL32(93, 125, 59, 255)
                                                 : IM_COL32(179, 89, 46, 255);
        dl->AddCircleFilled(s, r + (selected ? 2.0f : 0.0f), col);
        if (selected) dl->AddCircle(s, r + 3.5f, IM_COL32(30, 30, 30, 255), 0, 2.0f);
        if (deg >= 3 || selected || phys_zoom > 1.4f)
            dl->AddText(ImVec2(s.x + r + 3, s.y - 8), IM_COL32(40, 40, 40, 255),
                        n->name.c_str());
        float dx = mouse.x - s.x, dy = mouse.y - s.y;
        if (hovered && dx * dx + dy * dy < (r + 4) * (r + 4)) hit = n;
    }
    if (hit && ImGui::IsItemHovered())
        ImGui::SetTooltip("%s (%s) - %d connection(s)", hit->name.c_str(),
                          hit->glyph.c_str(), degree[hit->name]);

    if (phys_done_iters < phys_total_iters) { // settling progress
        float frac = (float)phys_done_iters / (float)phys_total_iters;
        ImVec2 ba(p0.x + 10, p0.y + 8), bb(p0.x + 190, p0.y + 22);
        dl->AddRectFilled(ba, bb, IM_COL32(255, 255, 255, 220), 4);
        dl->AddRectFilled(ba, ImVec2(ba.x + 180 * frac, bb.y),
                          IM_COL32(90, 140, 200, 200), 4);
        char msg[48];
        std::snprintf(msg, sizeof msg, "computing layout %d%%", (int)(frac * 100));
        dl->AddText(ImVec2(ba.x + 8, ba.y), IM_COL32(40, 40, 40, 255), msg);
    }

    // input: drag node / pan / zoom / select
    if (ImGui::IsItemActivated() && hit) phys_drag = hit->name;
    if (ImGui::IsItemDeactivated()) phys_drag.clear();
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        if (!phys_drag.empty()) {
            phys_pos[phys_drag].x += d.x / phys_zoom;
            phys_pos[phys_drag].y += d.y / phys_zoom;
        } else {
            phys_cam.x += d.x;
            phys_cam.y += d.y;
        }
    }
    if (hit && ImGui::IsItemClicked(ImGuiMouseButton_Left) && phys_drag.empty())
        ed.selection = {hit->name};
    if (hovered && ImGui::GetIO().MouseWheel != 0)
        phys_zoom = std::clamp(
            phys_zoom * (ImGui::GetIO().MouseWheel > 0 ? 1.15f : 0.87f), 0.3f, 3.0f);
    dl->PopClipRect();
}


// ── the console: log strip + command bar, its own dockable window ───────────

void HormigaApp::draw_console() {
    if (ImGui::SmallButton("copy condensed"))
        ImGui::SetClipboardText(maiz::log_to_text(log, true).c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton("copy all"))
        ImGui::SetClipboardText(maiz::log_to_text(log, false).c_str());
    ImGui::SameLine(0, 16);
    ImGui::TextDisabled("the transcript IS the session");
    float footer = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("##lines", ImVec2(0, -footer));
    maiz::draw_log_strip(log);
    ImGui::EndChild();
    maiz::CanvasIO bio = maiz::draw_command_bar(cmdbar);
    for (const auto& cmd : bio.commands) {
        if (try_map_verb(cmd)) continue; // `map …` verb macros (one batch)
        if (try_doc_verb(cmd)) continue; // `doc …` — the Builder's verbs
        maiz::Result r = dispatch_and_reproject(cmd);
        log.push_back({">", cmd, r.text().empty() ? (r.ok ? "ok" : "failed") : r.text()});
    }
}

/* One workflow window. A VISIBLE window (its tab is up front, or it's floated)
 * is the active context: it makes its mantle active and draws its content.
 * Deterministic under the default tabbing (exactly one section visible → it's
 * active); if the user floats two at once they share the one `scene`/`ed`, so
 * `section` follows whichever draws last — an accepted edge, not the common
 * case. No selection-clear here (that's only for an explicit switch). */
void HormigaApp::section_window(const char* title, int which) {
    if (!sec_open[which]) return; // closed — restore via Windows menu
    if (boot_section == which) ImGui::SetNextWindowFocus(); // dev seam
    if (ImGui::Begin(title, &sec_open[which])) {
        if (section != which) { // this visible window becomes the active context
            section = which;
            std::string mantle = (which == Builder)   ? cur_doc
                                 : (which == Antfarm) ? std::string(kAntfarmMantle)
                                                      : std::string(kDataMantle);
            if (scene.mantle != mantle)
                dispatch_and_reproject(std::string("use ") + mantle);
        }
        switch (which) {
        case Data: draw_data_section(0); break;
        case Builder: draw_builder_section(0); break;
        case Antfarm: draw_antfarm_section(); break;
        case Map: draw_map_section(); break;
        }
    }
    ImGui::End();
}

// ── frame ────────────────────────────────────────────────────────────────────

/* Defer a heavy, blocking operation by ONE frame so the "working…" overlay
 * paints before the main thread stalls on it. The label shows immediately;
 * the action runs at the start of the next frame, then the overlay clears. */
void HormigaApp::run_busy(const std::string& label, std::function<void()> action) {
    busy_label = label;
    busy_progress = -1.0f;
    busy_action = std::move(action);
}

/* The blocking-op overlay: a label + a spinner (busy_progress < 0) or a
 * progress bar, over a dimmed screen. Shown for deferred heavy ops (save /
 * open / new database). Async work (tile downloads, import jobs) shows its own
 * indicators in place; this is the modal "the app is working" curtain. */
void HormigaApp::draw_busy_overlay() {
    if (busy_label.empty()) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##busydim", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        ImVec2 c(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                 vp->WorkPos.y + vp->WorkSize.y * 0.5f);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddRectFilled(ImVec2(c.x - 170, c.y - 40), ImVec2(c.x + 170, c.y + 40),
                          IM_COL32(30, 30, 34, 240), 10.0f);
        // a spinning arc for the indeterminate case
        if (busy_progress < 0) {
            float t = (float)ImGui::GetTime() * 6.0f;
            for (int i = 0; i < 12; ++i) {
                float a = t + i * 0.5236f;
                int alpha = 40 + (i * 215 / 12);
                dl->AddCircleFilled(ImVec2(c.x - 130 + std::cos(a) * 14,
                                           c.y + std::sin(a) * 14),
                                    2.6f, IM_COL32(255, 255, 255, alpha));
            }
        } else {
            dl->AddRect(ImVec2(c.x - 148, c.y + 14), ImVec2(c.x + 148, c.y + 26),
                        IM_COL32(255, 255, 255, 160), 3.0f);
            dl->AddRectFilled(
                ImVec2(c.x - 146, c.y + 16),
                ImVec2(c.x - 146 + 292 * std::clamp(busy_progress, 0.0f, 1.0f),
                       c.y + 24),
                IM_COL32(120, 190, 140, 230), 2.0f);
        }
        ImVec2 ts = ImGui::CalcTextSize(busy_label.c_str());
        dl->AddText(ImVec2(c.x - 100, c.y - 22), IM_COL32(240, 240, 245, 255),
                    busy_label.c_str());
        (void)ts;
    }
    ImGui::End();
}

void HormigaApp::frame() {
    ImGuiIO& io = ImGui::GetIO();
    /* EVERY VIEW REDECLARES WHAT IT SHOWS, this frame (Void Maiz's Surfaces is
     * immediate mode on purpose: a registry with a lifecycle goes stale the
     * moment a window closes on one device and not another). */
    surfaces.begin_frame();
    share_now = share_filter();  // read once a frame, not once a row
    tex_decodes_this_frame = 0; // reset the per-frame texture-decode budget

    /* A MERGED DOCUMENT LANDS HERE, not where it was computed (app.hpp,
     * `pending_state`): the sync effect runs inside a handler that `core` owns,
     * so swapping the core there would free the lambda mid-call. By now the
     * dispatch has returned and nothing is in flight. */
    if (!pending_state.empty()) {
        const std::string merged = pending_state;
        pending_state.clear();
        reload_from_state(merged);
        toast("sync applied - the merged database is open; check the log for "
              "conflicts");
    }
    LanRuntime::apply_incoming(*this); // members' changes, merged by the replica (lan_sync.cpp)

    // a DEFERRED heavy op runs now — the "working…" overlay painted last frame
    if (busy_action) {
        auto a = busy_action;
        busy_action = nullptr;
        a();               // blocks (save/open/pack) — the overlay already showed
        busy_label.clear();
        busy_progress = -1.0f;
    }

    // a finished background job: join it, land its model changes through the
    // dispatcher (main thread), and report — then release it
    if (job && job->finished.load()) {
        std::vector<std::string> cmds = std::move(job->results);
        std::string summary = job->summary;
        job.reset(); // joins the worker in ~Job
        if (!cmds.empty())
            dispatch_and_reproject(maiz::compile_commit(cmds));
        mirror_count = -1; // force the ImgBB face to re-read the fresh count
        toast(summary);
    }

    apply_redirect(); // a redirect asked for during the last frame (ui/widgets.cpp)

    // deferred commands (tag suggestions, the import's pre-batched landing)
    // — each entry is dispatched as-is; batching happened where it was built.
    // `map …` verbs route through the action registry here too, so agents
    // driving HORMIGA_BOOT_CMD get the same vocabulary as the command bar.
    if (!pending_cmds.empty()) {
        std::vector<std::string> cmds;
        cmds.swap(pending_cmds);
        for (const auto& c : cmds)
            if (!try_map_verb(c) && !try_doc_verb(c)) dispatch_and_reproject(c);
    }
    // a date-tag temper pass runs once the import's commands have all landed
    if (run_temper_next && pending_cmds.empty()) {
        run_temper_next = false;
        derive_date_tags();
    }
    // LIVE PREVIEW (B2): re-render 0.45s after the last edit lands, then bump
    // the version so every open live page reloads itself. Direct render calls
    // — the auto-loop is a view convenience, not a logged effect (the manual
    // buttons stay the transcript's renders).
    if (preview_live && preview_dirty &&
        ImGui::GetTime() - preview_edit_t > 0.45) {
        preview_dirty = false;
        render_site(preview_lang ? "es" : "en");
        render_preview(preview_lang ? "es" : "en");
        preview_srv.bump();
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) do_save();
            // "Preview newsletter" left this menu (2026-09-15, the author: it "doesn't
            // belong there"). File is the database; the preview is the Builder's.
            ImGui::Separator();
            // ── the DATABASE bundle (.miga v3): the whole org, portable ──────
            ImGui::TextDisabled("%s", cur_miga.empty()
                                    ? "database: (unsaved working copy)"
                                    : ("database: " +
                                       fs::path(cur_miga).filename().string())
                                          .c_str());
            if (ImGui::MenuItem("New database"))
                run_busy("Creating new database…", [this] { new_database(); });
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("start a fresh, empty database (a new\n"
                                  "working copy); Save database as... names it");
            if (ImGui::MenuItem("Save database", "Ctrl+Shift+S")) {
                // Save As if there's no file yet (author #1); the dialog must
                // run NOW (not deferred) so the path is chosen before the pack
                if (cur_miga.empty() && on_save_file) {
                    std::string p = on_save_file("my-database.miga");
                    if (!p.empty())
                        run_busy("Saving database…",
                                 [this, p] { save_database_as(p); });
                } else if (!cur_miga.empty()) {
                    run_busy("Saving database…", [this] { save_database(); });
                }
            }
            if (ImGui::MenuItem("Save database as...")) {
                if (on_save_file) {
                    std::string suggest = cur_miga.empty()
                                              ? "my-database.miga"
                                              : fs::path(cur_miga).filename().string();
                    std::string p = on_save_file(suggest); // OS save dialog
                    if (!p.empty())
                        run_busy("Saving database…",
                                 [this, p] { save_database_as(p); });
                } else {
                    toast("no save dialog available", true);
                }
            }
            if (ImGui::MenuItem("Open database...")) {
                if (on_pick_file) {
                    std::string p = on_pick_file(""); // OS open dialog (.miga)
                    if (!p.empty())
                        run_busy("Opening database…",
                                 [this, p] { open_database(p); });
                } else {
                    toast("no file picker available", true);
                }
            }
            if (ImGui::MenuItem("Share database...")) win_share = true;
            if (ImGui::MenuItem("Discover databases...")) win_discover = true;
            if (ImGui::MenuItem("Profile...")) win_profile = true;
            ImGui::Separator();
            // credential vault (security.md §2: off by default, loudly offered)
            if (secrets_locked) {
                if (ImGui::MenuItem("Unlock credentials..."))
                    vault_modal = VaultModal::Unlock;
            } else if (vault.unlocked()) {
                ImGui::MenuItem("Credentials encrypted", nullptr, false, false);
            } else if (ImGui::MenuItem("Encrypt credentials...")) {
                vault_msg.clear();
                vault_modal = VaultModal::Create;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit") && on_quit) on_quit();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem(light_mode ? "Dark mode" : "Light mode")) {
                light_mode = !light_mode;
                apply_theme();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows")) { // closed panels come back here
            ImGui::MenuItem("Data", nullptr, &sec_open[Data]);
            ImGui::MenuItem("Builder", nullptr, &sec_open[Builder]);
            ImGui::MenuItem("Antfarm", nullptr, &sec_open[Antfarm]);
            ImGui::MenuItem("Map", nullptr, &sec_open[Map]);
            ImGui::MenuItem("Publish", nullptr, &win_publish);
            ImGui::MenuItem("Calendar", nullptr, &win_calendar);
            ImGui::MenuItem("Notes", nullptr, &win_notes);
            ImGui::MenuItem("Civic Record", nullptr, &win_civic);
            ImGui::MenuItem("Allomone", nullptr, &win_allomone);
#if HORMIGA_LEGACY_ALLOMONE // unshipped 2026-08-11 — see the note in app.cpp
            ImGui::MenuItem("Allomone (legacy dialect)", nullptr, &win_allomone_legacy);
            ImGui::MenuItem("Allo Dev (experimental editor)", nullptr, &win_allomone_dev);
#endif
            ImGui::MenuItem("Data Tools", nullptr, &win_data_tools);
            ImGui::MenuItem("Niche Tools", nullptr, &win_niche_tools);
            ImGui::MenuItem("Settings", nullptr, &show_settings);
            ImGui::MenuItem("Style", nullptr, &win_style);
            ImGui::MenuItem("Console", nullptr, &win_console);
            ImGui::EndMenu();
        }
        ImGui::SameLine(0, 24);
        ImGui::TextDisabled("mantle: %s | undo: %d", scene.mantle.c_str(), undo_depth);
        draw_source_tree_banner();
        ImGui::SameLine();
        if (ImGui::SmallButton("undo")) dispatch_and_reproject("undo");
        ImGui::SameLine();
        if (ImGui::SmallButton("redo")) dispatch_and_reproject("redo");
        LanRuntime::draw_presence_strip(*this); // who else is here, and you
        ImGui::EndMainMenuBar();
    }
    // Ctrl+S saves the working copy; Ctrl+Shift+S saves the database bundle
    if (io.KeyCtrl && !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (io.KeyShift && !cur_miga.empty())
            run_busy("Saving database…", [this] { save_database(); });
        else if (io.KeyShift)
            save_database(); // no file yet → its own Save-As dialog
        else
            do_save();
    }
    // undo/redo shortcuts (author: "ctrl+z and ctrl+y don't work") — deletion
    // and every edit is undoable (rm snapshots the mantle slice). Ctrl+Z undo;
    // Ctrl+Y or Ctrl+Shift+Z redo. Guarded off text fields so typing is safe.
    if (io.KeyCtrl && !io.WantTextInput) {
        bool shift = io.KeyShift;
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false))
            dispatch_and_reproject(shift ? "redo" : "undo");
        else if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
            dispatch_and_reproject("redo");
    }

    // The dockspace host — the four workflows + the console are windows the
    // user can drag, float, tab, and re-dock. Driven directly via ImGui
    // (which Void Maiz vendors/exposes) because their begin_dockspace wrapper
    // has an always-false `#ifdef ImGuiConfigFlags_DockingEnable` guard (an
    // enum, not a macro — reported in MESSAGE_FOR_VOIDMAIZ.md 2026-07-20). The
    // seed-BEFORE-DockSpace order is what keeps this from crashing.
#ifdef IMGUI_HAS_DOCK
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##hormiga-dockhost", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoDocking);
    ImGui::PopStyleVar(3);
    ImGuiID dock = ImGui::GetID("hormiga-dock");
    // seed the default arrangement ONLY when there is no saved layout for this
    // node — so the user's own rearrangement (persisted in imgui.ini) wins on
    // later launches. Seeded BEFORE DockSpace() submits the node.
    if (!dock_seeded && ImGui::DockBuilderGetNode(dock) == nullptr) {
        dock_seeded = true;
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
        ImGuiID center = dock, bottom;
        bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.26f, nullptr,
                                             &center);
        ImGui::DockBuilderDockWindow("Data", center);
        ImGui::DockBuilderDockWindow("Builder", center);
        ImGui::DockBuilderDockWindow("Antfarm", center);
        ImGui::DockBuilderDockWindow("Map", center);
        ImGui::DockBuilderDockWindow("Publish", center);
        ImGui::DockBuilderDockWindow("Calendar", center);
        ImGui::DockBuilderDockWindow("Notes", center);
        ImGui::DockBuilderDockWindow("Allomone", center);
        ImGui::DockBuilderDockWindow("Allo Dev", center);
        ImGui::DockBuilderDockWindow("Settings", center);
        ImGui::DockBuilderDockWindow("Style", center);
        ImGui::DockBuilderDockWindow("Console", bottom);
        ImGui::DockBuilderFinish(dock);
    }
    ImGui::DockSpace(dock, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
#endif

    /* ── NO ICONS ON WINDOW TITLES, AND THIS IS THE NOTE THAT SAYS WHY ───────
     *
     * They were added on 2026-09-02 as `ICON " Data###Data"`, on the reasoning
     * that ImGui hashes what follows `###` so the window id would not move and
     * nobody's dock layout would either. **The id was right and the reasoning
     * was wrong**, and it cost the author a working application for an evening.
     *
     * `imgui.ini` keys a window's saved settings by a DIFFERENT hash than the
     * live window uses. `ImGui::CreateNewWindowSettings` skips to the `###`
     * marker and hashes from there, so a saved `[Window][Data]` has the id
     * `hash("Data")` while a window named `"X Data###Data"` looks itself up as
     * `hash("###Data")`. Those are not equal. Every existing entry was orphaned:
     * position, size and dock assignment all lost, silently, for anybody who
     * upgraded.
     *
     * That alone would have been a rude but survivable one-time reset. What it
     * actually did was hand the Data section a window narrow enough to cross
     * the bounds of a `std::clamp` — which is UB, which libstdc++ turns into
     * `abort()`. The application opened and closed again before it could draw a
     * frame. Both halves are fixed; only one of them was mine.
     *
     * So: **the titles are the plain names they have always been.** Icons live
     * everywhere they cost nothing — the Builder palette and its drag ghost,
     * the document toolbar, the Data "+ New" menu, the Notes tab. Putting one
     * on a window title needs an `imgui.ini` migration first, and that is a
     * deliberate piece of work rather than a decoration. */
    section_window("Data", Data);
    section_window("Builder", Builder);
    section_window("Antfarm", Antfarm);
    section_window("Map", Map);
    /* PUBLISH is a dockable tab rather than a fifth main Section, deliberately.
     * The four sections are the four things this application IS (data, builder,
     * antfarm, map); publishing is something you DO to one of them, occasionally,
     * and it is the operator's surface over Antfarm nodes rather than a
     * workflow of its own. The author's framing: a website is one of the things
     * Hormiga does, not what it is — so this stays a tab until hosting grows
     * into something bigger, at which point promoting it is one line. */
    if (win_publish) {
        ImGui::SetNextWindowSize(ImVec2(660, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Publish", &win_publish)) draw_publish_body();
        ImGui::End();
    }
    if (win_calendar) {
        if (boot_focus_cal && boot_focus_frames > 0) ImGui::SetNextWindowFocus();
        if (ImGui::Begin("Calendar", &win_calendar)) draw_calendar_body();
        ImGui::End();
    }
    if (win_notes) {
        ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
        if (boot_focus_notes && boot_focus_frames > 0) ImGui::SetNextWindowFocus();
        if (ImGui::Begin("Notes", &win_notes)) draw_notes_body();
        ImGui::End();
    }
    if (win_civic) {
        ImGui::SetNextWindowSize(ImVec2(760, 520), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Civic Record", &win_civic)) draw_civic_body();
        ImGui::End();
    }
    if (win_allomone) {
        ImGui::SetNextWindowSize(ImVec2(620, 460), ImGuiCond_FirstUseEver);
        if (boot_focus_allomone && boot_focus_frames > 0) ImGui::SetNextWindowFocus();
        if (ImGui::Begin("Allomone", &win_allomone)) draw_allomone_body();
        ImGui::End();
    }
    if (win_niche_tools) { // once-in-a-while utilities; off by default
        ImGui::SetNextWindowSize(ImVec2(660, 580), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Niche Tools", &win_niche_tools)) draw_niche_tools_body();
        ImGui::End();
    }
#if HORMIGA_LEGACY_ALLOMONE // unshipped 2026-08-11
    if (win_allomone_legacy) {
        ImGui::SetNextWindowSize(ImVec2(620, 460), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Allomone (legacy)", &win_allomone_legacy))
            draw_allomone_legacy_body();
        ImGui::End();
    }
    if (win_allomone_dev) {
        ImGui::SetNextWindowSize(ImVec2(640, 480), ImGuiCond_FirstUseEver);
        if (boot_focus_allodev && boot_focus_frames > 0) ImGui::SetNextWindowFocus();
        if (ImGui::Begin("Allo Dev", &win_allomone_dev)) draw_allomone_dev_body();
        ImGui::End();
    }
#endif
    draw_settings();
    draw_style_tab();
    draw_share_window();
    draw_data_tools_window();
    draw_templates_window();
    draw_rule_editor();
    draw_manage_views();
    if (win_console) {
        if (ImGui::Begin("Console", &win_console)) draw_console();
        ImGui::End();
    }
    if (boot_section >= 0 && --boot_focus_frames <= 0) boot_section = -1;

    draw_vault_modal();
    /* The update client's main-thread half: join a finished worker, then draw.
     * Drained here rather than beside the `job` drain above because it lands no
     * dispatcher commands -- an update is a fact about the installation, not a
     * change to the organization's data. */
    updates_drain();
    draw_update_modal();
    draw_job_overlay();
    draw_busy_overlay();
    draw_toasts();
}
