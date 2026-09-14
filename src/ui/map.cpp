/* ui/map.cpp — Territory: web-mercator math, the tile fetcher, the
 * per-view position channels, map creation, and the slippy map itself. Split
 * out of app.cpp 2026-08-17 (Q30a); see app_internal.hpp.
 *
 * NOTE THE TWO HALVES. In app.cpp these were separated by the entire calendar,
 * for no reason other than the order they were written in. They are one subject
 * and they are now one file, which is most of what a split is for: the mercator
 * helpers sit next to their only callers, and `static` says so.
 *
 * No geographic type touches the core. Positions are a VIEW model written
 * through the dispatcher's `place` verb (the view slice, SPEC §6) — which is
 * also why Reyna's `Site` locus is named to stay clear of it.
 */
#include "app/app_internal.hpp"
#include "render/mercator.hpp"
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

// ── Territory placeholder (the map — concept: okf/concepts/sections/territory.md) ────

// web-mercator math now lives in render/mercator.hpp — the PNG export needs the
// SAME projection as this canvas, which only became visible when it moved out
using hormiga::merc_lat;
using hormiga::merc_lon;
using hormiga::merc_x;
using hormiga::merc_y;

// ── the background tile fetcher (pure I/O; the core is never touched) ───────

void HormigaApp::TileFetcher::start(std::function<std::string(const std::string&)> sh) {
    shell = std::move(sh);
    for (int w = 0; w < 4; ++w) // parallel: fetches are latency-bound
        workers.emplace_back([this, w] {
            (void)w;
            for (;;) {
                std::string path, url;
                {
                    std::unique_lock<std::mutex> lk(mu);
                    cv.wait(lk, [this] { return stop.load() || !queue.empty(); });
                    if (stop.load()) return;
                    path = queue.front();
                    queue.pop_front();
                    url = url_of[path];
                }
                ++in_flight;
                if (!std::filesystem::exists(path)) {
                    std::filesystem::create_directories(
                        std::filesystem::path(path).parent_path());
                    // download to a temp name, then rename: the final path
                    // only ever exists COMPLETE (a half-written file would
                    // cache a permanent decode failure on the main thread)
                    std::string tmp = path + ".part" + std::to_string(w);
                    shell("curl -s -A \"Hormiga/0.1 (dev; local-first outreach "
                          "app)\" -o \"" + tmp + "\" \"" + url + "\"");
                    std::error_code ec;
                    auto size = std::filesystem::file_size(tmp, ec);
                    if (!ec && size > 100) { // a real tile, not an error stub
                        std::filesystem::rename(tmp, path, ec);
                    } else {
                        std::filesystem::remove(tmp, ec); // retry next session
                    }
                }
                --in_flight;
            }
        });
}
void HormigaApp::TileFetcher::want(const std::string& url, const std::string& path) {
    std::lock_guard<std::mutex> lk(mu);
    if (queued.count(path)) return;
    queued.insert(path);
    url_of[path] = url;
    queue.push_back(path);
    cv.notify_one();
}
int HormigaApp::TileFetcher::pending() {
    std::lock_guard<std::mutex> lk(mu);
    return (int)queue.size() + in_flight.load();
}
HormigaApp::TileFetcher::~TileFetcher() {
    stop = true;
    cv.notify_all();
    for (auto& w : workers)
        if (w.joinable()) w.join();
}

// ── position channels (the per-view location model) ─────────────────────────
// pos(rune, view) = geo_<channel> if set, else geo (main). Views sharing a
// channel are locked BY CONSTRUCTION (one field, no sync); a forked view
// stores only its divergences (copy-on-write). See okf/concepts/sections/territory.md.

std::string HormigaApp::geo_field_for(const std::string& ch) {
    return (ch.empty() || ch == "main") ? "geo" : "geo_" + ch;
}
std::string HormigaApp::view_geo(const maiz::SceneNode& n, const std::string& ch) {
    if (!ch.empty() && ch != "main") {
        std::string v = hormiga::temper::field_value(n, ("geo_" + ch).c_str());
        if (!v.empty()) return v; // this view's own position
    }
    return hormiga::temper::field_value(n, "geo"); // fallback: main
}

/* #4: fan-out layout. For every entity whose `ref` field names a REFPOINT,
 * compute its display position = the refpoint's geo + a pixel offset so
 * co-located siblings don't overlap. Siblings ring around the point (6 per
 * ring, spiralling outward for more). The offset is in SCREEN pixels, so it
 * stays spread at any zoom. Returns entity name → {ref geo, dx, dy}. */
std::map<std::string, HormigaApp::RefFan> HormigaApp::ref_fans(
    const maiz::Scene& s, [[maybe_unused]] const std::string& ch) const {
    std::map<std::string, std::pair<double, double>> rp; // refpoint → geo
    for (const auto& n : s.nodes)
        if (n.glyph == "refpoint") {
            double la, lo;
            if (hormiga::parse_geo(hormiga::temper::field_value(n, "geo"), la, lo))
                rp[n.name] = {la, lo};
        }
    std::map<std::string, std::vector<std::string>> kids; // ref → child names
    for (const auto& n : s.nodes) {
        if (n.glyph == "refpoint" || n.glyph == "mapshape" || n.glyph == "map")
            continue;
        std::string r = hormiga::temper::field_value(n, "ref");
        if (!r.empty() && rp.count(r)) kids[r].push_back(n.name);
    }
    std::map<std::string, RefFan> out;
    for (auto& [r, names] : kids) {
        int N = (int)names.size();
        auto [la, lo] = rp[r];
        for (int i = 0; i < N; ++i) {
            // a child that was hand-dragged carries a MANUAL pixel offset
            // (`ref_off` = "dx,dy") — that overrides the auto-fan. The offset is
            // relative to the refpoint, so moving the refpoint carries it along
            // and the drag is a re-arrangement, never a new absolute position.
            const maiz::SceneNode* cn = s.find(names[i]);
            float mdx, mdy;
            std::string off = cn ? hormiga::temper::field_value(*cn, "ref_off") : "";
            if (!off.empty() && std::sscanf(off.c_str(), "%f,%f", &mdx, &mdy) == 2) {
                out[names[i]] = {la, lo, mdx, mdy};
                continue;
            }
            int ring = i / 6, in_ring = i % 6;
            int ring_n = std::min(6, N - ring * 6);
            float radius = 22.0f + ring * 22.0f;
            float ang = (float)in_ring * 6.2831853f / std::max(1, ring_n) - 1.5708f;
            out[names[i]] = {la, lo, std::cos(ang) * radius, std::sin(ang) * radius};
        }
    }
    return out;
}

std::string HormigaApp::rules_to_json(const std::vector<MapRule>& rules) {
    nlohmann::json rj = nlohmann::json::array();
    for (const auto& r : rules)
        rj.push_back({{"name", r.name},
                      {"tags", r.tags},
                      {"icon", r.icon},
                      {"color", r.color},
                      {"shape", r.shape}});
    return rj.dump();
}
void HormigaApp::save_rules(const std::string& view_rune) {
    pending_cmds.push_back("setjson " + view_rune + " rules " +
                           json_arg(rules_to_json(active_rules)));
}

/* The RULE EDITOR — a real window, not a popup (author: give it room to
 * grow). Edits one rule of the ACTIVE view: name, tag CHIPS (visibly added
 * and removable; matching = all tags, AND-joined through the one grammar),
 * icon + color dropdowns. Save = ONE setjson on the view rune. */
void HormigaApp::draw_rule_editor() {
    if (!show_rule_editor) return;
    ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(rule_edit_idx < 0 ? "New rule###ruleedit" : "Edit rule###ruleedit",
                     &show_rule_editor)) {
        ImGui::TextDisabled("entities matching ALL tags get this style");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##rname", "rule name (e.g. \"volunteers\")",
                                 rule_name, sizeof rule_name);
        // tag chips: clear add, clear remove
        for (size_t i = 0; i < rule_tags.size(); ++i) {
            ImGui::PushID((int)i);
            if (ImGui::SmallButton(("@" + rule_tags[i] + "  x").c_str()))
                rule_tags.erase(rule_tags.begin() + i--);
            ImGui::PopID();
            ImGui::SameLine();
        }
        // tag entry searches EXISTING tags as you type (author #4) — pick one or
        // create a new one; either way it lands as a chip
        std::string chose = tag_picker("##rtag", rule_tag_input, sizeof rule_tag_input,
                                       "+ tag (searches existing)");
        if (!chose.empty() &&
            std::find(rule_tags.begin(), rule_tags.end(), chose) == rule_tags.end())
            rule_tags.push_back(chose);
        ImGui::Spacing();
        ImGui::SetNextItemWidth(170);
        if (ImGui::BeginCombo("icon", rule_icon == 0
                                          ? "(none)"
                                          : kMarkerIcons[rule_icon - 1].label)) {
            if (ImGui::Selectable("(none)", rule_icon == 0)) rule_icon = 0;
            for (int ii = 0; ii < (int)(sizeof kMarkerIcons / sizeof kMarkerIcons[0]);
                 ++ii) {
                std::string lbl = std::string(kMarkerIcons[ii].glyph) + "  " +
                                  kMarkerIcons[ii].label;
                if (ImGui::Selectable(lbl.c_str(), rule_icon == ii + 1))
                    rule_icon = ii + 1;
            }
            ImGui::EndCombo();
        }
        ImGui::SetNextItemWidth(170);
        if (ImGui::BeginCombo("color", rule_color == 0
                                           ? "(glyph default)"
                                           : kMarkerColors[rule_color - 1].tag)) {
            if (ImGui::Selectable("(glyph default)", rule_color == 0)) rule_color = 0;
            for (int ci = 0; ci < (int)(sizeof kMarkerColors / sizeof kMarkerColors[0]);
                 ++ci) {
                ImGui::PushStyleColor(
                    ImGuiCol_Text,
                    ImGui::ColorConvertU32ToFloat4(kMarkerColors[ci].col));
                bool pick =
                    ImGui::Selectable(kMarkerColors[ci].tag, rule_color == ci + 1);
                ImGui::PopStyleColor();
                if (pick) rule_color = ci + 1;
            }
            ImGui::EndCombo();
        }
        // shape: circle (default) / pin / square / diamond
        ImGui::SetNextItemWidth(170);
        if (ImGui::BeginCombo("shape", kMarkerShapes[rule_shape])) {
            for (int si = 0; si < (int)(sizeof kMarkerShapes / sizeof kMarkerShapes[0]);
                 ++si)
                if (ImGui::Selectable(kMarkerShapes[si], rule_shape == si))
                    rule_shape = si;
            ImGui::EndCombo();
        }
        ImGui::Spacing();
        ImGui::BeginDisabled(rule_tags.empty());
        if (ImGui::Button(rule_edit_idx < 0 ? "Add rule" : "Save rule",
                          ImVec2(110, 0))) {
            MapRule r{rule_name[0] ? rule_name : "rule",
                      rule_tags,
                      rule_icon ? kMarkerIcons[rule_icon - 1].tag : "",
                      rule_color ? kMarkerColors[rule_color - 1].tag : "",
                      rule_shape ? kMarkerShapes[rule_shape] : ""};
            if (rule_edit_idx >= 0 && rule_edit_idx < (int)active_rules.size())
                active_rules[rule_edit_idx] = r;
            else
                active_rules.push_back(r);
            if (!map_sel.empty()) save_rules(map_sel);
            show_rule_editor = false;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110, 0))) show_rule_editor = false;
        if (rule_tags.empty())
            ImGui::TextDisabled("add at least one tag");
    }
    ImGui::End();
}

/* MANAGE VIEWS — list, rename, delete, and the LOCK story: a view's channel
 * names its position set; channel "main" = locked to main (the default);
 * "Unlock" gives it its own channel (copy-on-write: nothing is copied until
 * something is actually moved there); "Lock to main" rejoins (its own
 * positions stay stored but dormant). */
void HormigaApp::draw_manage_views() {
    if (!show_manage_views) return;
    ImGui::SetNextWindowSize(ImVec2(430, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Manage views", &show_manage_views)) {
        ImGui::TextDisabled("a view = camera + rules + a position channel;\n"
                            "views on the SAME channel move together (locked)");

        // ── the BASE MAP row (author #3): the shared foundation every view
        // draws over ("for us, google maps"). Its source (labeled / no-labels)
        // and color treatment are GLOBAL config — one base map, many views. ──
        ImGui::SeparatorText("Base map (shared by all views)");
        int bsi = std::clamp(basemap_src, 0, kBaseSourceCount - 1);
        ImGui::SetNextItemWidth(240);
        if (ImGui::BeginCombo("##basesrc", kBaseSources[bsi].label)) {
            for (int i = 0; i < kBaseSourceCount; ++i)
                if (ImGui::Selectable(kBaseSources[i].label, i == bsi)) {
                    basemap_src = i;
                    dispatch_and_reproject(std::string("config set ui.basemap \"") +
                                           kBaseSources[i].key + "\"");
                }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("raster tiles bake labels/icons into the pixels -\n"
                              "a 'no labels' base is a different tile SOURCE,\n"
                              "not a filter. Pick a label-free style to quiet\n"
                              "the map (author: 'turn off base map text/icons')");
        ImGui::SameLine();
        if (ImGui::SmallButton("Adjust##base")) ImGui::OpenPopup("##baseadjust");
        if (ImGui::BeginPopup("##baseadjust")) {
            ImGui::TextDisabled("calm a busy base map (markers stay crisp)");
            // sliders edit the MEMBER directly (persists across frames — the
            // snap-back fix) and preview LIVE; the config-set lands on release
            ImGui::SetNextItemWidth(160);
            ImGui::SliderFloat("Brightness", &basemap_brightness, 0.3f, 1.0f,
                               "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                char b[56];
                std::snprintf(b, sizeof b,
                              "config set ui.basemap_brightness \"%.2f\"",
                              basemap_brightness);
                pending_cmds.push_back(b);
            }
            ImGui::SetNextItemWidth(160);
            ImGui::SliderFloat("Fade (desaturate)", &basemap_fade, 0.0f, 1.0f,
                               "%.2f");
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                char b[56];
                std::snprintf(b, sizeof b, "config set ui.basemap_fade \"%.2f\"",
                              basemap_fade);
                pending_cmds.push_back(b);
            }
            ImGui::TextDisabled("hue/contrast/true-saturation need a tile shader\n"
                                "(a noted future ask) - these cover 'too busy'");
            ImGui::EndPopup();
        }

        ImGui::SeparatorText("Views (layers, top draws last)");
        int vi = 0;
        for (const auto& n : scene.nodes) {
            if (n.glyph != "map") continue;
            ImGui::PushID(++vi);
            bool active = (n.name == map_sel);
            // layer visibility (art-program style): the eye checkbox
            bool vis = hormiga::temper::field_value(n, "visible") != "0";
            if (ImGui::Checkbox("##vis", &vis))
                pending_cmds.push_back("set " + n.name + " visible \"" +
                                       (vis ? "1" : "0") + "\"");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("layer visibility: shown views composite\n"
                                  "on the map (ghosted); the active view is\n"
                                  "the one you edit");
            ImGui::SameLine();
            if (ImGui::RadioButton(n.name.c_str(), active)) map_sel = n.name;
            std::string ch = hormiga::temper::field_value(n, "channel");
            if (ch.empty()) ch = "main";
            ImGui::SameLine(215);
            if (ch == "main") {
                ImGui::TextDisabled("locked");
                ImGui::SameLine(280);
                if (ImGui::SmallButton("Unlock"))
                    pending_cmds.push_back("set " + n.name + " channel \"" +
                                           n.name + "\"");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("own positions (copy-on-write: entities\n"
                                      "stay where main has them until you move\n"
                                      "them IN this view)");
            } else {
                ImGui::TextDisabled("own ch.");
                ImGui::SameLine(280);
                if (ImGui::SmallButton("Lock"))
                    pending_cmds.push_back("set " + n.name + " channel \"main\"");
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("rejoin main's positions (this view's own\n"
                                      "positions stay stored, dormant)");
            }
            ImGui::SameLine();
            // per-view LAYER treatment (author #3): how this view looks as a
            // ghosted layer under another. Stored on the view rune.
            if (ImGui::SmallButton("Adjust")) ImGui::OpenPopup("##layeradjust");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("layer look when this view shows UNDER another\n"
                                  "(opacity + brightness of its ghosted markers)");
            if (ImGui::BeginPopup("##layeradjust")) {
                ImGui::TextDisabled("%s as a ghosted layer", n.name.c_str());
                float op, lb;
                ImGui::SetNextItemWidth(150);
                if (slider_field("Opacity", view_layer_opacity(&n), 0.05f, 1.0f,
                                 "%.2f", &op)) {
                    char b[48];
                    std::snprintf(b, sizeof b, "%.2f", op);
                    pending_cmds.push_back("set " + n.name + " layer_opacity \"" +
                                           b + "\"");
                }
                ImGui::SetNextItemWidth(150);
                if (slider_field("Brightness", view_layer_brightness(&n), 0.3f,
                                 1.5f, "%.2f", &lb)) {
                    char b[48];
                    std::snprintf(b, sizeof b, "%.2f", lb);
                    pending_cmds.push_back("set " + n.name + " layer_brightness \"" +
                                           b + "\"");
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("PNG")) export_map_png(n.name);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("export this view at the CURRENT camera as a\n"
                                  "PNG (exports/ folder) - for newsletters,\n"
                                  "the website, or anywhere");
            ImGui::SameLine();
            if (ImGui::SmallButton("Delete")) {
                pending_cmds.push_back("rm " + n.name);
                if (active) map_sel.clear();
            }
            ImGui::PopID();
        }
        if (vi == 0) ImGui::TextDisabled("no views yet - use \"New view\"");
        ImGui::Separator();
        ImGui::TextDisabled("rename a view: select it in Data and edit its name");
    }
    ImGui::End();
}

// ── map creation + placement (all through the dispatcher) ───────────────────

void HormigaApp::map_new_earth() {
    std::string name;
    for (int i = 1;; ++i) {
        name = "earth-map" + std::string(i > 1 ? "-" + std::to_string(i) : "");
        if (!scene.find(name)) break;
    }
    char center[64];
    std::snprintf(center, sizeof center, "%.5f,%.5f", (double)map_cam.y,
                  (double)map_cam.x);
    // default STYLE RULES (named, v2): kinds read at a glance out of the box;
    // edit or remove them in the map panel / Rule Editor
    const char* default_rules =
        R"([{"name":"people","tags":["type:contact"],"icon":"user","color":""},)"
        R"({"name":"organizations","tags":["type:organization"],"icon":"house","color":""},)"
        R"({"name":"events","tags":["type:event"],"icon":"calendar","color":""},)"
        R"({"name":"incidents","tags":["type:incident"],"icon":"warning","color":"red"}])";
    pending_cmds.push_back(maiz::compile_commit(
        {"rune new map " + name, "set " + name + " source \"osm\"",
         "set " + name + " center " + json_str(center),
         "set " + name + " zoom \"" + std::to_string((int)map_cam.zoom) + "\"",
         "set " + name + " channel \"main\"", // new views start LOCKED to main
         "setjson " + name + " rules " + json_arg(default_rules),
         "tag " + name + " +type:map"}));
    map_sel = name;
    toast("created view " + name + " - locked to main positions; Manage views "
          "to unlock");
}

void HormigaApp::map_place_new(const char* glyph) {
    std::string name;
    for (int i = 1;; ++i) {
        name = std::string(glyph) + "-" + std::to_string(i);
        if (!scene.find(name)) break;
    }
    char geo[64];
    std::snprintf(geo, sizeof geo, "%.7g,%.7g", map_ctx_lat, map_ctx_lon);
    auto cmds = map_actions.run("place", scene,
                                {{"glyph", glyph}, {"name", name}, {"geo", geo},
                                 {"field", geo_field_for(active_channel)}});
    if (cmds.empty()) { toast("place declined", true); return; }
    pending_cmds.push_back(maiz::compile_commit(cmds));
    ed.selection = {name};
    toast("placed " + name + " - fill in its details in the inspector");
}

void HormigaApp::map_place_existing(const std::string& name) {
    char geo[64];
    std::snprintf(geo, sizeof geo, "%.7g,%.7g", map_ctx_lat, map_ctx_lon);
    auto cmds = map_actions.run("move", scene,
                                {{"name", name}, {"geo", geo},
                                 {"field", geo_field_for(active_channel)}});
    if (cmds.empty()) { toast("move declined", true); return; }
    pending_cmds.push_back(maiz::compile_commit(cmds));
    ed.selection = {name};
    toast("placed existing " + name + " on the map");
}

/* The marker action menu, ITEMS ONLY (caller owns Begin/EndPopup). Shared by
 * the map right-click AND the "On this map" list (author: the list should act
 * like the map). Everything queues to pending_cmds — this may be called from
 * inside a frame that still holds SceneNode pointers, so it never dispatches
 * directly (the dangling-pointer bug class). */
void HormigaApp::marker_menu_items(const maiz::SceneNode& mk) {
    ImGui::TextDisabled("%s (%s)", mk.name.c_str(), mk.glyph.c_str());
    ImGui::Separator();
    if (ImGui::MenuItem("Select (edit in inspector)")) ed.selection = {mk.name};
    if (ImGui::MenuItem("Center map here")) {
        double la, lo;
        if (hormiga::parse_geo(view_geo(mk, active_channel), la, lo)) {
            map_cam.x = (float)lo;
            map_cam.y = (float)la;
            pending_cmds.push_back(maiz::compile_camera(map_cam, "view.map.camera"));
        }
    }
    std::string cur_icon = tag_value(mk, "icon");
    std::string cur_color = tag_value(mk, "color");
    if (ImGui::BeginMenu("Icon")) {
        if (ImGui::MenuItem("(none)", nullptr, cur_icon.empty()) && !cur_icon.empty())
            pending_cmds.push_back("tag " + mk.name + " -icon:" + cur_icon);
        for (const auto& ic : kMarkerIcons) {
            std::string lbl = std::string(ic.glyph) + "  " + ic.label;
            if (ImGui::MenuItem(lbl.c_str(), nullptr, cur_icon == ic.tag)) {
                std::string cmd = "tag " + mk.name;
                if (!cur_icon.empty()) cmd += " -icon:" + cur_icon;
                cmd += " +icon:" + std::string(ic.tag);
                pending_cmds.push_back(cmd);
            }
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Color")) {
        if (ImGui::MenuItem("(glyph default)", nullptr, cur_color.empty()) &&
            !cur_color.empty())
            pending_cmds.push_back("tag " + mk.name + " -color:" + cur_color);
        for (const auto& c : kMarkerColors) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(c.col));
            bool picked = ImGui::MenuItem(c.tag, nullptr, cur_color == c.tag);
            ImGui::PopStyleColor();
            if (picked) {
                std::string cmd = "tag " + mk.name;
                if (!cur_color.empty()) cmd += " -color:" + cur_color;
                cmd += " +color:" + std::string(c.tag);
                pending_cmds.push_back(cmd);
            }
        }
        ImGui::EndMenu();
    }
    std::string cur_shape = tag_value(mk, "shape");
    if (ImGui::BeginMenu("Shape")) {
        if (ImGui::MenuItem("circle (default)", nullptr, cur_shape.empty()) &&
            !cur_shape.empty())
            pending_cmds.push_back("tag " + mk.name + " -shape:" + cur_shape);
        for (const char* sh : kMarkerShapes) {
            if (std::string(sh) == "circle") continue; // = default (no tag)
            if (ImGui::MenuItem(sh, nullptr, cur_shape == sh)) {
                std::string cmd = "tag " + mk.name;
                if (!cur_shape.empty()) cmd += " -shape:" + cur_shape;
                cmd += " +shape:" + std::string(sh);
                pending_cmds.push_back(cmd);
            }
        }
        ImGui::EndMenu();
    }
    // #4: attach this marker to a reference point — it will fan out around the
    // gizmo's shared location instead of stacking on its own coordinate
    {
        std::string cur_ref = hormiga::temper::field_value(mk, "ref");
        std::vector<const maiz::SceneNode*> refs;
        for (const auto& en : scene.nodes)
            if (en.glyph == "refpoint") refs.push_back(&en);
        if (ImGui::BeginMenu("Attach to reference point", !refs.empty())) {
            for (const auto* rf : refs) {
                std::string lbl = hormiga::temper::field_value(*rf, "label");
                std::string item = (lbl.empty() ? rf->name : lbl) + "##" + rf->name;
                bool on = (cur_ref == rf->name);
                if (ImGui::MenuItem(item.c_str(), nullptr, on))
                    pending_cmds.push_back("set " + mk.name + " ref \"" + rf->name + "\"");
            }
            ImGui::EndMenu();
        }
        if (!cur_ref.empty()) {
            if (ImGui::MenuItem("Detach from reference point"))
                pending_cmds.push_back("set " + mk.name + " ref \"\"");
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Remove from map (keep the rune)")) {
        // clear THIS view's channel position (copy-on-write aware); if that was
        // the only position, drop the located tag too
        std::string fld = geo_field_for(active_channel);
        std::vector<std::string> cmds = {"set " + mk.name + " " + fld + " \"\""};
        if (fld != "geo" && hormiga::temper::field_value(mk, "geo").empty())
            cmds.push_back("tag " + mk.name + " -located");
        else if (fld == "geo")
            cmds.push_back("tag " + mk.name + " -located");
        pending_cmds.push_back(maiz::compile_commit(cmds));
    }
    if (ImGui::MenuItem("Delete rune (undoable)")) {
        pending_cmds.push_back("rm " + mk.name);
        ed.selection.clear();
    }
}

/* Static PNG export of a map view (author #5): compose the CACHED tiles +
 * the view's markers (its channel, its rules) around the CURRENT camera into
 * exports/<view>-<stamp>.png — CPU-side (stb decode → blit → circle fill →
 * stb write), no GL. What you see is what exports. Missing tiles render as
 * flat ground — pan the area once to warm the cache first. */
void HormigaApp::draw_map_section() {
    // restore the persisted viewport once (config tier, undo-exempt — the
    // blessed compile_camera(key) pattern upstream generalized for us)
    if (!map_cam_loaded) {
        map_cam_loaded = true;
        maiz::Camera saved;
        if (maiz::parse_camera(core.dispatch("config get view.map.camera").data,
                               saved))
            map_cam = saved;
    }

    // the database's maps (one db, many maps — each map is a rune)
    std::vector<const maiz::SceneNode*> maps;
    for (const auto& n : scene.nodes)
        if (n.glyph == "map") maps.push_back(&n);
    const maiz::SceneNode* cur = nullptr;
    for (auto* m : maps)
        if (m->name == map_sel) cur = m;
    if (!cur && !maps.empty()) { cur = maps[0]; map_sel = cur->name; }

    // ── toolbar: views (a view = camera + rules + a position channel) ───────
    if (ImGui::SmallButton("New view")) map_new_earth();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("a fresh view over the base map (Settings > Base map).\n"
                          "New views start LOCKED to main's positions; unlock\n"
                          "in Manage views for view-specific placement");
    ImGui::SameLine();
    if (ImGui::SmallButton("Manage views")) show_manage_views = true;
    ImGui::SameLine();
    // #3: the shape DRAW tool — arm rect/ellipse, then drag on the map
    if (map_draw_shape) ImGui::PushStyleColor(ImGuiCol_Button,
                                              ImVec4(0.18f, 0.42f, 0.30f, 1));
    if (ImGui::SmallButton(map_draw_shape ? "Drawing… (Esc)" : "Draw shape"))
        ImGui::OpenPopup("##drawshape");
    if (map_draw_shape) ImGui::PopStyleColor();
    if (ImGui::BeginPopup("##drawshape")) {
        ImGui::TextDisabled("drag on the map to draw an annotation");
        if (ImGui::MenuItem("Rectangle")) map_draw_shape = 1;
        if (ImGui::MenuItem("Ellipse")) map_draw_shape = 2;
        if (map_draw_shape && ImGui::MenuItem("Stop drawing")) map_draw_shape = 0;
        ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("draw a rectangle/ellipse over the map - it's a rune,\n"
                          "colored by the same rules/tags as markers; can bestow\n"
                          "a tag on entities inside it");
    ImGui::SameLine();
    if (!maps.empty()) {
        ImGui::SetNextItemWidth(180);
        if (ImGui::BeginCombo("##mapsel", map_sel.c_str())) {
            for (auto* m : maps)
                if (ImGui::Selectable(m->name.c_str(), m->name == map_sel))
                    map_sel = m->name;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (active_channel != "main") {
            ImGui::TextColored(ImVec4(0.55f, 0.4f, 0.75f, 1), "[own positions]");
            ImGui::SameLine();
        }
    }
    // database search (LOCAL, not geocoding): find any rune, see whether it
    // has a location; located → jump to it, unlocated → arm click-to-place
    ImGui::SetNextItemWidth(210);
    ImGui::InputTextWithHint("##mapsearch", "search the database...", map_search,
                             sizeof map_search);
    ImVec2 srch_min = ImGui::GetItemRectMin(), srch_max = ImGui::GetItemRectMax();
    ImGui::SameLine();
    int located = 0;
    for (const auto& n : scene.nodes)
        for (const auto& t : n.tags)
            if (t == "located") { ++located; break; }
    ImGui::TextDisabled("%d located | drag: pan | wheel: zoom | right-click: place",
                        located);
    if (map_search[0]) {
        ImGui::SetNextWindowPos(ImVec2(srch_min.x, srch_max.y + 4));
        ImGui::SetNextWindowSize(ImVec2(320, 0));
        ImGui::Begin("##map-search-results", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing);
        int shown = 0;
        for (const auto& node : scene.nodes) {
            if (node.glyph == "map" || node.glyph == "image") continue;
            if (!contains_ci(node.name, map_search)) continue;
            if (++shown > 8) { ImGui::TextDisabled("(keep typing...)"); break; }
            std::string g = hormiga::temper::field_value(node, "geo");
            std::string lbl = node.name + "  (" + node.glyph + ")";
            if (ImGui::Selectable(lbl.c_str())) {
                double la, lo;
                if (!g.empty() && hormiga::parse_geo(g, la, lo)) {
                    map_cam.x = (float)lo;      // jump to it
                    map_cam.y = (float)la;
                    if (map_cam.zoom < 15) map_cam.zoom = 15;
                    pending_cmds.push_back(
                        maiz::compile_camera(map_cam, "view.map.camera"));
                    ed.selection = {node.name};
                } else {
                    map_place_arm = node.name;  // click-to-place mode
                }
                map_search[0] = 0;
            }
            ImGui::Indent(10);
            if (!g.empty())
                ImGui::TextDisabled("at %s", g.c_str());
            else
                ImGui::TextColored(ImVec4(0.75f, 0.45f, 0.25f, 1),
                                   "no spatial location - click to place");
            ImGui::Unindent(10);
        }
        if (shown == 0) ImGui::TextDisabled("no matches");
        ImGui::End();
    }

    if (!cur) {
        ImGui::Spacing();
        ImGui::TextWrapped(
            "No maps yet. \"New Earth map\" creates one over OpenStreetMap "
            "tiles. A database can hold many maps; the base is a SOURCE "
            "(okf/concepts/sections/territory.md - not assumed Earth: an image source "
            "renders a floor plan or a fantasy world the same way).");
        return;
    }

    // ── layout: canvas | splitter | inspector ───────────────────────────────
    float body_h = ImGui::GetContentRegionAvail().y;
    ImVec2 area = ImGui::GetContentRegionAvail();
    const float th = 6.0f;
    float main_w = std::max(160.0f, (area.x - th) * canvas_frac);

    ImGui::BeginChild("map-canvas", ImVec2(main_w, body_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 16 || sz.y < 16) { ImGui::EndChild(); return; } // collapsed pane
    ImVec2 ctr(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
    int z = std::clamp((int)std::lround(map_cam.zoom), 3, 19);
    double cx = merc_x(map_cam.x, z), cy = merc_y(map_cam.y, z); // center tile
    auto to_screen = [&](double lat, double lon) {
        return ImVec2((float)(ctr.x + (merc_x(lon, z) - cx) * 256.0),
                      (float)(ctr.y + (merc_y(lat, z) - cy) * 256.0));
    };
    auto to_geo = [&](ImVec2 s, double& lat, double& lon) {
        lon = merc_lon(cx + (s.x - ctr.x) / 256.0, z);
        lat = merc_lat(cy + (s.y - ctr.y) / 256.0, z);
    };

    // an input surface over the whole canvas (also clips the draw)
    dl->PushClipRect(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), true);
    ImGui::InvisibleButton("##map-surface", sz,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImVec2 mouse = ImGui::GetIO().MousePos;

    // ── tiles (with ancestor/child FALLBACK so zooming never goes gray) ─────
    // While the exact tile downloads, draw the best cached stand-in: a parent
    // tile's quarter (zooming in) or the four child tiles (zooming out) —
    // the slippy-map trick that makes zoom feel instant; the crisp tile
    // replaces it the frame it lands (fetcher renames only complete files).
    int n = 1 << z;
    const BaseSource& bsrc =
        kBaseSources[std::clamp(basemap_src, 0, kBaseSourceCount - 1)];
    auto tile_tex = [&](int zz, int tx, int ty) -> HostTexture {
        char rel[96];
        std::snprintf(rel, sizeof rel, "tiles/%s/%d/%d/%d.png", bsrc.key, zz, tx, ty);
        if (!fs::exists(base_dir / rel)) return {};
        return texture_for(rel);
    };
    int tx0 = (int)std::floor(cx - sz.x * 0.5 / 256.0) - 1;
    int tx1 = (int)std::floor(cx + sz.x * 0.5 / 256.0) + 1;
    int ty0 = std::max(0, (int)std::floor(cy - sz.y * 0.5 / 256.0) - 1);
    int ty1 = std::min(n - 1, (int)std::floor(cy + sz.y * 0.5 / 256.0) + 1);
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int txr = tx0; txr <= tx1; ++txr) {
            /* THE WORLD DECIDES WHETHER IT WRAPS. This was
             * `((txr % n) + n) % n` — correct for Earth, and silently wrong for
             * anything else: an authored map has EDGES, and wrapping one tiles a
             * fictional city into an infinite plane. `tile_column` returns false
             * off the edge of a non-wrapping world so we draw nothing there
             * rather than drawing the far side. (gis/source.hpp) */
            int tx = 0;
            if (!bsrc.tile_column(txr, z, tx)) continue;
            ImVec2 a((float)(ctr.x + (txr - cx) * 256.0),
                     (float)(ctr.y + (ty - cy) * 256.0));
            ImVec2 b(a.x + 256.0f, a.y + 256.0f);
            HostTexture t = tile_tex(z, tx, ty);
            if (t.id) {
                dl->AddImage((ImTextureID)(intptr_t)t.id, a, b);
                continue;
            }
            { // not here yet: request it…
                char url[160];
                std::snprintf(url, sizeof url, bsrc.url, z, tx, ty);
                char rel[96];
                std::snprintf(rel, sizeof rel, "tiles/%s/%d/%d/%d.png", bsrc.key,
                              z, tx, ty);
                tiles.want(url, (base_dir / rel).string());
            }
            // …and draw the best cached stand-in meanwhile
            bool drew = false;
            for (int k = 1; k <= 4 && !drew; ++k) { // ancestors (zoom-in case)
                HostTexture pa = tile_tex(z - k, tx >> k, ty >> k);
                if (!pa.id) continue;
                float f = 1.0f / (float)(1 << k);
                ImVec2 uv0((tx & ((1 << k) - 1)) * f, (ty & ((1 << k) - 1)) * f);
                ImVec2 uv1(uv0.x + f, uv0.y + f);
                dl->AddImage((ImTextureID)(intptr_t)pa.id, a, b, uv0, uv1);
                drew = true;
            }
            if (!drew && z < 19) { // children (zoom-out case)
                for (int q = 0; q < 4; ++q) {
                    HostTexture ch = tile_tex(z + 1, tx * 2 + (q & 1), ty * 2 + (q >> 1));
                    if (!ch.id) continue;
                    ImVec2 qa(a.x + (q & 1) * 128.0f, a.y + (q >> 1) * 128.0f);
                    dl->AddImage((ImTextureID)(intptr_t)ch.id, qa,
                                 ImVec2(qa.x + 128.0f, qa.y + 128.0f));
                    drew = true;
                }
            }
            if (!drew) {
                dl->AddRectFilled(a, b, IM_COL32(228, 226, 220, 255));
                dl->AddRect(a, b, IM_COL32(210, 208, 200, 255));
            }
        }
    // ── base-map color treatment (author #3): cheap full-canvas overlays drawn
    // OVER the tiles but UNDER the data, so the base map calms down while
    // markers/labels stay crisp. Brightness = a black veil (multiply); Fade =
    // a mid-gray veil (pulls color+contrast toward gray). Raster tiles can't be
    // hue/contrast-adjusted per-pixel without a shader (a noted future ask);
    // brightness + fade + the label-free source cover the "too busy" need. ───
    ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    if (basemap_brightness < 0.999f)
        dl->AddRectFilled(p0, p1,
                          IM_COL32(0, 0, 0, (int)((1.0f - basemap_brightness) * 255)));
    if (basemap_fade > 0.001f)
        dl->AddRectFilled(p0, p1, IM_COL32(150, 150, 150, (int)(basemap_fade * 200)));
    dl->AddText(ImVec2(p0.x + 6, p0.y + sz.y - 20), IM_COL32(90, 90, 90, 200),
                bsrc.attribution);
    if (int pend = tiles.pending(); pend > 0) { // the loading indicator
        char msg[48];
        std::snprintf(msg, sizeof msg, "loading %d tile%s...", pend,
                      pend == 1 ? "" : "s");
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImVec2 ta(p0.x + sz.x - ts.x - 14, p0.y + 8);
        dl->AddRectFilled(ImVec2(ta.x - 6, ta.y - 3),
                          ImVec2(ta.x + ts.x + 6, ta.y + ts.y + 3),
                          IM_COL32(255, 255, 255, 210), 4.0f);
        dl->AddText(ta, IM_COL32(60, 60, 60, 255), msg);
    }

    // ── the view's RULES (shared parser — the rules engine is cross-view)
    // and its position CHANNEL ──────────────────────────────────────────────
    auto parse_view_rules = [](const maiz::SceneNode& view) {
        return parse_view_rules_of(view);
    };
    active_rules.clear();
    active_channel = "main";
    if (cur) {
        std::string ch = hormiga::temper::field_value(*cur, "channel");
        if (!ch.empty()) active_channel = ch;
        active_rules = parse_view_rules(*cur);
    }
    bool show_labels = view_show_labels(cur);   // map config (#6)
    float label_scale = view_label_scale(cur);
    bool no_overlap = view_no_overlap(cur);     // labels dodge each other
    ImU32 label_col = view_label_color(cur);
    std::vector<ImVec4> placed_labels;          // x0,y0,x1,y1 of drawn labels
    auto rule_filter_expr = [](const MapRule& r) { return rule_expr_of(r); };

    // ── collect located runes once (markers, proximity, the side panel) —
    // positions come from the ACTIVE VIEW'S CHANNEL (fallback: main) ────────
    struct Located {
        const maiz::SceneNode* node;
        double lat, lon;
        ImVec2 s;
        bool on_screen;
    };
    // #4: ref-children are FANNED around their refpoint (they share coords)
    auto fans = ref_fans(scene, active_channel);
    std::vector<Located> located_nodes;
    for (const auto& node : scene.nodes) {
        if (node.glyph == "refpoint") continue; // gizmos drawn separately
        double la, lo;
        ImVec2 s;
        auto fit = fans.find(node.name);
        if (fit != fans.end()) { // fanned child: refpoint geo + pixel offset
            la = fit->second.lat; lo = fit->second.lon;
            s = to_screen(la, lo);
            s.x += fit->second.dx; s.y += fit->second.dy;
        } else {
            std::string g = view_geo(node, active_channel);
            if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
            s = to_screen(la, lo);
        }
        bool on = !(s.x < p0.x - 30 || s.x > p0.x + sz.x + 30 || s.y < p0.y - 30 ||
                    s.y > p0.y + sz.y + 30);
        located_nodes.push_back({&node, la, lo, s, on});
    }

    // #4: REFERENCE-POINT gizmos (editor-only — never in exports). A hollow
    // ring + crosshair at the shared location, with thin connectors out to each
    // fanned child, so the parent-child grouping is visible while editing.
    const maiz::SceneNode* ref_hit = nullptr;
    for (const auto& node : scene.nodes) {
        if (node.glyph != "refpoint") continue;
        double la, lo;
        if (!hormiga::parse_geo(hormiga::temper::field_value(node, "geo"), la, lo))
            continue;
        // while THIS gizmo is being dragged it follows the cursor live, and its
        // fanned children preview at the new anchor (they snap on release)
        bool being_dragged = (map_drag_ref == node.name);
        ImVec2 c = being_dragged ? mouse : to_screen(la, lo);
        ImU32 gc = IM_COL32(120, 100, 70, 220);
        for (const auto& [nm, f] : fans) // connectors to children
            if (std::abs(f.lat - la) < 1e-9 && std::abs(f.lon - lo) < 1e-9) {
                ImVec2 kid(c.x + f.dx, c.y + f.dy);
                dl->AddLine(c, kid, IM_COL32(120, 100, 70, 120), 1.2f);
                if (being_dragged) // ghost the child at its previewed spot
                    dl->AddCircleFilled(kid, 4.0f, IM_COL32(120, 100, 70, 150));
            }
        dl->AddCircle(c, 9.0f, gc, 0, 2.0f);
        dl->AddLine(ImVec2(c.x - 13, c.y), ImVec2(c.x + 13, c.y), gc, 1.2f);
        dl->AddLine(ImVec2(c.x, c.y - 13), ImVec2(c.x, c.y + 13), gc, 1.2f);
        std::string lbl = hormiga::temper::field_value(node, "label");
        if (lbl.empty()) lbl = node.name;
        dl->AddText(ImVec2(c.x + 12, c.y - 20), gc,
                    ("\xE2\x97\x87 " + lbl).c_str()); // ◇ prefix = gizmo
        float dx = mouse.x - c.x, dy = mouse.y - c.y;
        if (hovered && map_drag_marker.empty() && dx * dx + dy * dy < 144)
            ref_hit = &node;
    }

    // ── the HIDDEN CONNECTION, revealed: spatial proximity (Settings toggle).
    // Derived every frame from distance — never stored, never dispatched;
    // weight (alpha/width) fades with distance (Scry-style: derive, don't
    // materialize) ──────────────────────────────────────────────────────────
    if (map_show_prox) {
        for (size_t i = 0; i < located_nodes.size(); ++i)
            for (size_t j = i + 1; j < located_nodes.size(); ++j) {
                if (!located_nodes[i].on_screen && !located_nodes[j].on_screen)
                    continue;
                double d = hormiga::geo_distance_m(
                    located_nodes[i].lat, located_nodes[i].lon,
                    located_nodes[j].lat, located_nodes[j].lon);
                if (d > map_prox_m) continue;
                float w = 1.0f - (float)(d / map_prox_m); // closer = stronger
                dl->AddLine(located_nodes[i].s, located_nodes[j].s,
                            IM_COL32(70, 110, 160, (int)(40 + 140 * w)),
                            1.0f + 2.0f * w);
            }
    }

    // ── LAYERS: other VISIBLE views composite underneath, ghosted (like an
    // art program's layers — author, 2026-07-23). Each layer renders its own
    // channel's positions with its own rules; only the ACTIVE view receives
    // interaction. Toggle visibility in Manage views. ───────────────────────
    for (auto* v : maps) {
        if (v == cur) continue;
        if (hormiga::temper::field_value(*v, "visible") == "0") continue;
        std::string vch = hormiga::temper::field_value(*v, "channel");
        if (vch.empty()) vch = "main";
        auto vrules = parse_view_rules(*v);
        float lop = view_layer_opacity(v);     // per-view layer treatment (#3)
        float lbr = view_layer_brightness(v);
        int la8 = (int)(lop * 255);
        int ring8 = (int)(lop * 0.6f * 255); // the ring fades with the fill
        auto dim = [&](ImU32 c) {              // apply layer brightness to RGB
            int r = (int)((c & 0xFF) * lbr), g = (int)(((c >> 8) & 0xFF) * lbr);
            int b = (int)(((c >> 16) & 0xFF) * lbr);
            return IM_COL32(std::min(r, 255), std::min(g, 255), std::min(b, 255),
                            la8);
        };
        for (const auto& node : scene.nodes) {
            std::string g = view_geo(node, vch);
            double la, lo;
            if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
            ImVec2 s = to_screen(la, lo);
            if (s.x < p0.x - 20 || s.x > p0.x + sz.x + 20 || s.y < p0.y - 20 ||
                s.y > p0.y + sz.y + 20)
                continue;
            ImU32 col = IM_COL32(120, 120, 130, 255); // ghosted default (pre-dim)
            for (const auto& r : vrules) {
                if (r.tags.empty() || !maiz::node_matches(rule_filter_expr(r), node))
                    continue;
                for (const auto& c : kMarkerColors)
                    if (r.color == c.tag) col = c.col;
                break;
            }
            dl->AddCircleFilled(s, 5.0f, dim(col));
            dl->AddCircle(s, 5.0f, IM_COL32(255, 255, 255, ring8), 0, 1.5f);
        }
    }

    // ── #3 MAP SHAPES (drawn annotations): rect/ellipse runes over the map,
    // colored by the same rules/tags as markers, under the markers. Click a
    // shape to select it; right-click for its menu. ────────────────────────
    const maiz::SceneNode* shape_hit = nullptr;
    for (const auto& node : scene.nodes) {
        if (node.glyph != "mapshape") continue;
        double la1, lo1, la2, lo2;
        if (!hormiga::parse_geo(hormiga::temper::field_value(node, "geo1"), la1, lo1) ||
            !hormiga::parse_geo(hormiga::temper::field_value(node, "geo2"), la2, lo2))
            continue;
        ImVec2 a = to_screen(la1, lo1), b = to_screen(la2, lo2);
        ImVec2 mn(std::min(a.x, b.x), std::min(a.y, b.y));
        ImVec2 mx(std::max(a.x, b.x), std::max(a.y, b.y));
        // color: rules → explicit color: tag → a default green
        ImU32 col = IM_COL32(46, 107, 79, 255);
        for (const auto& r : active_rules) {
            if (r.tags.empty() || !maiz::node_matches(rule_filter_expr(r), node))
                continue;
            for (const auto& c : kMarkerColors)
                if (r.color == c.tag) col = c.col;
            break;
        }
        std::string ctag = tag_value(node, "color");
        for (const auto& c : kMarkerColors)
            if (ctag == c.tag) col = c.col;
        // ALLOMONE, the `map` surface (roadmap phase F's first step): a derived
        // colour beats the view's own rules and the explicit tag, because it is
        // the only one of the three that COMPOSES — several scripts' opinions
        // already met in the merge, and a conflict among them reached this point
        // as "nothing derived" rather than as a winner. Layering it under a
        // first-match-wins rule list would throw that away.
        if (const AlloStyle* st = allo_style_for("map", node.name))
            if (st->has_color) col = st->rgba;
        bool sel = ed.selected(node.name);
        ImU32 fill = (col & 0x00FFFFFF) | 0x33000000; // ~20% fill
        ImU32 line = (col & 0x00FFFFFF) | (sel ? 0xFF000000 : 0xBB000000);
        bool ellipse = hormiga::temper::field_value(node, "kind") == "ellipse";
        if (ellipse) {
            ImVec2 c((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
            dl->AddEllipseFilled(c, ImVec2((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f),
                                 fill);
            dl->AddEllipse(c, ImVec2((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f), line,
                           0, 0, sel ? 2.5f : 1.5f);
        } else {
            dl->AddRectFilled(mn, mx, fill, 4.0f);
            dl->AddRect(mn, mx, line, 4.0f, 0, sel ? 2.5f : 1.5f);
        }
        std::string lbl = hormiga::temper::field_value(node, "label");
        if (!lbl.empty())
            dl->AddText(ImVec2(mn.x + 4, mn.y + 3), line, lbl.c_str());
        if (hovered && mouse.x >= mn.x && mouse.x <= mx.x && mouse.y >= mn.y &&
            mouse.y <= mx.y)
            shape_hit = &node;
    }

    // ── markers (click = select, drag = the `move` action) ─────────────────
    const maiz::SceneNode* hit = nullptr;
    for (const auto& L : located_nodes) {
        if (!L.on_screen) continue;
        const maiz::SceneNode& node = *L.node;
        ImVec2 s = L.s;
        bool selected = ed.selected(node.name);
        ImU32 col = node.glyph == "incident"       ? IM_COL32(200, 50, 50, 255)
                    : node.glyph == "organization" ? IM_COL32(138, 109, 59, 255)
                    : node.glyph == "event"        ? IM_COL32(63, 111, 174, 255)
                                                   : IM_COL32(179, 89, 46, 255);
        const char* icon = nullptr;
        std::string shape;
        for (const auto& r : active_rules) { // first matching rule styles it…
            if (r.tags.empty() || !maiz::node_matches(rule_filter_expr(r), node))
                continue;
            for (const auto& c : kMarkerColors)
                if (r.color == c.tag) col = c.col;
            for (const auto& ic : kMarkerIcons)
                if (r.icon == ic.tag) icon = ic.glyph;
            if (!r.shape.empty()) shape = r.shape;
            break;
        }
        std::string ctag = tag_value(node, "color"); // …explicit tags win
        for (const auto& c : kMarkerColors)
            if (ctag == c.tag) col = c.col;
        std::string itag = tag_value(node, "icon");
        for (const auto& ic : kMarkerIcons)
            if (itag == ic.tag) icon = ic.glyph;
        std::string stag = tag_value(node, "shape");
        if (!stag.empty()) shape = stag;
        // ALLOMONE, the `map` surface. Derived styling beats both the view's
        // rules and the explicit tags: it is the only one of the three that
        // COMPOSES, and a disagreement among scripts already resolved to
        // "nothing derived" rather than to a winner, so nothing is being
        // overridden here that anybody chose.
        const AlloStyle* mst = allo_style_for("map", node.name);
        if (mst) {
            if (mst->has_color) col = mst->rgba;
            for (const auto& ic : kMarkerIcons)
                if (mst->icon == ic.tag) icon = ic.glyph;
        }
        MShape msh = shape_from(shape);
        float r = (icon ? 11.0f : 6.5f) + (selected ? 2.5f : 0.0f);
        if (mst && mst->weight > 0) r += std::min((float)mst->weight, 6.0f);
        ImVec2 ic_at = draw_marker_shape(dl, s, r, col, IM_COL32(255, 255, 255, 230),
                                         msh);
        if (icon) {
            ImFont* f = ImGui::GetFont();
            float isz = r * 1.15f;
            ImVec2 is = f->CalcTextSizeA(isz, FLT_MAX, 0, icon);
            dl->AddText(f, isz, ImVec2(ic_at.x - is.x * 0.5f, ic_at.y - is.y * 0.5f),
                        IM_COL32(255, 255, 255, 255), icon);
        }
        if (show_labels) { // map config: labels + size/color/no-overlap
            ImFont* lf = ImGui::GetFont();
            float lsz = ImGui::GetFontSize() * label_scale;
            // A derived `map-label` shadows the marker's caption for display
            // only — the rune's name is untouched.
            const std::string& mlabel =
                (mst && !mst->label.empty()) ? mst->label : node.name;
            ImVec2 tsz = lf->CalcTextSizeA(lsz, FLT_MAX, 0, mlabel.c_str());
            // candidate anchors: right, left, above, below the marker — the
            // first that doesn't collide with an already-placed label wins;
            // none fit => this label yields (markers always draw)
            ImVec2 cand[4] = {{s.x + r + 4, s.y - lsz * 0.5f},
                              {s.x - r - 4 - tsz.x, s.y - lsz * 0.5f},
                              {s.x - tsz.x * 0.5f, s.y - r - 4 - tsz.y},
                              {s.x - tsz.x * 0.5f, s.y + r + 4}};
            int pick = no_overlap ? -1 : 0;
            for (int c = 0; c < 4 && pick < 0; ++c) {
                bool clear = true;
                for (const auto& pl : placed_labels)
                    if (cand[c].x < pl.z && cand[c].x + tsz.x > pl.x &&
                        cand[c].y < pl.w && cand[c].y + tsz.y > pl.y) {
                        clear = false;
                        break;
                    }
                if (clear) pick = c;
            }
            if (pick >= 0) {
                ImVec2 tp = cand[pick];
                placed_labels.push_back(
                    {tp.x, tp.y, tp.x + tsz.x, tp.y + tsz.y});
                // a soft halo keeps any label color legible over tiles
                ImU32 halo = IM_COL32(255, 255, 255, 170);
                dl->AddText(lf, lsz, ImVec2(tp.x + 1, tp.y + 1), halo,
                            mlabel.c_str());
                dl->AddText(lf, lsz, ImVec2(tp.x - 1, tp.y - 1), halo,
                            mlabel.c_str());
                dl->AddText(lf, lsz, tp, label_col, mlabel.c_str());
            }
        }
        float dx = mouse.x - s.x, dy = mouse.y - s.y;
        float hr = std::max(12.0f, r + 3);
        if (hovered && dx * dx + dy * dy < hr * hr) hit = &node;
    }

    // ── #3: DRAW-SHAPE mode takes the drag — press starts a corner, release
    // creates the mapshape rune (geo1/geo2 = the two corners). Esc cancels.
    // `was_drawing` (captured now) suppresses the marker/pan input this frame,
    // even on the release frame that clears the draw arm. ────────────────────
    bool was_drawing = (map_draw_shape != 0) || map_shape_dragging;
    if (map_draw_shape) {
        if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape)) map_draw_shape = 0;
        if (ImGui::IsItemActivated() && hovered) {
            to_geo(mouse, map_shape_la0, map_shape_lo0);
            map_shape_dragging = true;
        }
        if (map_shape_dragging) { // live preview
            ImVec2 a = to_screen(map_shape_la0, map_shape_lo0), b = mouse;
            ImVec2 mn(std::min(a.x, b.x), std::min(a.y, b.y));
            ImVec2 mx(std::max(a.x, b.x), std::max(a.y, b.y));
            if (map_draw_shape == 2) {
                ImVec2 c((mn.x + mx.x) / 2, (mn.y + mx.y) / 2);
                dl->AddEllipse(c, ImVec2((mx.x - mn.x) / 2, (mx.y - mn.y) / 2),
                               IM_COL32(46, 107, 79, 220), 0, 0, 2);
            } else {
                dl->AddRect(mn, mx, IM_COL32(46, 107, 79, 220), 4, 0, 2);
            }
        }
        if (ImGui::IsItemDeactivated() && map_shape_dragging) {
            double la, lo;
            to_geo(mouse, la, lo);
            if (std::abs(la - map_shape_la0) > 1e-6 ||
                std::abs(lo - map_shape_lo0) > 1e-6) {
                std::string name;
                for (int i = 1;; ++i) {
                    name = "shape-" + std::to_string(i);
                    if (!scene.find(name)) break;
                }
                char g1[64], g2[64];
                std::snprintf(g1, sizeof g1, "%.7g,%.7g", map_shape_la0, map_shape_lo0);
                std::snprintf(g2, sizeof g2, "%.7g,%.7g", la, lo);
                pending_cmds.push_back(maiz::compile_commit(
                    {"rune new mapshape " + name,
                     "set " + name + " kind \"" +
                         (map_draw_shape == 2 ? "ellipse" : "rect") + "\"",
                     "set " + name + " geo1 \"" + g1 + "\"",
                     "set " + name + " geo2 \"" + g2 + "\"",
                     "tag " + name + " +type:mapshape"}));
                ed.selection = {name};
            }
            map_shape_dragging = false;
            map_draw_shape = 0; // one shape per arm (re-arm from the toolbar)
        }
    }

    // ── input: shift = SELECT (box on empty, toggle on a marker); plain drag on
    // a marker MOVES it; plain drag on empty PANS; wheel zooms (author #2) ────
    bool shift = ImGui::GetIO().KeyShift;
    if (!was_drawing && ImGui::IsItemActivated()) {
        if (hit && !shift) {
            map_drag_marker = hit->name; // plain press on a marker → move it
        } else if (ref_hit && !shift) {  // plain press on a gizmo → MOVE it (#4)
            map_drag_ref = ref_hit->name;
            ed.selection = {ref_hit->name};
        } else if (shape_hit && !hit && !shift) { // a shape (no marker) → select
            ed.selection = {shape_hit->name};
        } else if (hit && shift) {       // shift-click a marker → toggle it
            if (ed.selected(hit->name))
                ed.selection.erase(std::remove(ed.selection.begin(),
                                               ed.selection.end(), hit->name),
                                   ed.selection.end());
            else
                ed.selection.push_back(hit->name);
        } else if (!hit && shift) {      // shift-drag on empty → box select
            map_box_active = true;
            map_box_start = mouse;
        }
    }
    if (!was_drawing && ImGui::IsItemDeactivated()) {
        if (!map_drag_ref.empty()) { // #4: a reference point was MOVED — its whole
            double la, lo;           // fan follows, since children are relative
            to_geo(mouse, la, lo);
            char geo[64];
            std::snprintf(geo, sizeof geo, "%.7g,%.7g", la, lo);
            pending_cmds.push_back("set " + map_drag_ref + " geo \"" + geo + "\"");
            map_drag_ref.clear();
        } else if (!map_drag_marker.empty()) {
            const maiz::SceneNode* mn = scene.find(map_drag_marker);
            std::string pref = mn ? hormiga::temper::field_value(*mn, "ref") : "";
            const maiz::SceneNode* rp =
                pref.empty() ? nullptr : scene.find(pref);
            if (rp && rp->glyph == "refpoint") {
                // #4: a CHILD of a reference point moved — that's an OFFSET from
                // the shared point (a re-arrangement), never a new absolute geo.
                double rla, rlo;
                if (hormiga::parse_geo(
                        hormiga::temper::field_value(*rp, "geo"), rla, rlo)) {
                    ImVec2 rc = to_screen(rla, rlo);
                    char off[48];
                    std::snprintf(off, sizeof off, "%.1f,%.1f", mouse.x - rc.x,
                                  mouse.y - rc.y);
                    pending_cmds.push_back("set " + map_drag_marker + " ref_off \"" +
                                           off + "\"");
                }
            } else {
                double la, lo;
                to_geo(mouse, la, lo);
                char geo[64];
                std::snprintf(geo, sizeof geo, "%.7g,%.7g", la, lo);
                auto cmds = map_actions.run(
                    "move", scene,
                    {{"name", map_drag_marker}, {"geo", geo},
                     {"field", geo_field_for(active_channel)}}); // this view's chan
                if (!cmds.empty()) pending_cmds.push_back(maiz::compile_commit(cmds));
            }
            map_drag_marker.clear();
        } else if (map_box_active) { // box select done: gather everything inside
            ImVec2 a = map_box_start, b = mouse;
            float x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x);
            float y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
            if (!shift || (x1 - x0 < 3 && y1 - y0 < 3)) {} // tiny box: keep sel
            std::vector<std::string> picked;
            for (const auto& L : located_nodes)
                if (L.on_screen && L.s.x >= x0 && L.s.x <= x1 && L.s.y >= y0 &&
                    L.s.y <= y1)
                    picked.push_back(L.node->name);
            if (!picked.empty()) ed.selection = picked;
            map_box_active = false;
        } else { // pan ended: flush the viewport (config tier, undo-exempt)
            pending_cmds.push_back(maiz::compile_camera(map_cam, "view.map.camera"));
        }
    }
    if (!was_drawing && ImGui::IsItemActive() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 d = ImGui::GetIO().MouseDelta;
        if (!map_drag_marker.empty() || !map_drag_ref.empty() || map_box_active) {
            // staged: marker/gizmo follows cursor / box grows — nothing to pan
        } else if (d.x != 0 || d.y != 0) {
            map_cam.x = (float)merc_lon(cx - d.x / 256.0, z);
            map_cam.y = (float)merc_lat(cy - d.y / 256.0, z);
        }
    }
    if (map_box_active) { // the selection rectangle
        ImVec2 a = map_box_start, b = mouse;
        dl->AddRectFilled(a, b, IM_COL32(70, 130, 200, 40));
        dl->AddRect(a, b, IM_COL32(70, 130, 200, 200), 0, 0, 1.5f);
    }
    // drag-and-drop feel (author #3): the picked-up marker GROWS toward the
    // cursor rather than teleporting. marker_anim eases 0->1 while dragging and
    // decays after release. Gated by the "GUI animations" advanced setting —
    // off => it just tracks the cursor at full size, no easing.
    float dt = ImGui::GetIO().DeltaTime;
    bool dragging = !map_drag_marker.empty();
    if (!gui_anim) {
        marker_anim = dragging ? 1.0f : 0.0f;
    } else {
        float target = dragging ? 1.0f : 0.0f;
        marker_anim += (target - marker_anim) * std::min(1.0f, dt * 12.0f);
        if (std::fabs(marker_anim - target) < 0.01f) marker_anim = target;
    }
    if (dragging || marker_anim > 0.01f) { // staged ghost while dragging
        float grow = 9.0f + 7.0f * marker_anim; // 9 -> 16 px as it lifts
        ImU32 shadow = IM_COL32(0, 0, 0, (int)(60 * marker_anim));
        dl->AddCircleFilled(ImVec2(mouse.x + 2, mouse.y + 3), grow, shadow);
        dl->AddCircleFilled(mouse, grow, IM_COL32(255, 255, 255, 150));
        dl->AddCircle(mouse, grow, IM_COL32(60, 60, 60, 210), 0, 2.0f);
        if (dragging)
            dl->AddText(ImVec2(mouse.x + grow + 4, mouse.y - 8),
                        IM_COL32(40, 40, 40, 255), map_drag_marker.c_str());
    }
    if (hovered && ImGui::GetIO().MouseWheel != 0) {
        int nz = std::clamp(z + (ImGui::GetIO().MouseWheel > 0 ? 1 : -1), 3, 19);
        if (nz != z) {
            // zoom about the cursor: keep the geo under the mouse fixed
            double mlat, mlon;
            to_geo(mouse, mlat, mlon);
            double fx = (mouse.x - ctr.x) / 256.0, fy = (mouse.y - ctr.y) / 256.0;
            map_cam.zoom = (float)nz;
            map_cam.x = (float)merc_lon(merc_x(mlon, nz) - fx, nz);
            map_cam.y = (float)merc_lat(merc_y(mlat, nz) - fy, nz);
            pending_cmds.push_back(maiz::compile_camera(map_cam, "view.map.camera"));
        }
    }
    if (hit && !shift && ImGui::IsItemClicked(ImGuiMouseButton_Left) &&
        map_drag_marker.empty())
        ed.selection = {hit->name}; // plain click = replace; shift handled above

    // armed placement (from the search bar): a plain click places the chosen
    // rune right here — one `move` action, one undo frame
    if (!map_place_arm.empty()) {
        dl->AddText(ImVec2(p0.x + 8, p0.y + 8), IM_COL32(180, 60, 40, 255),
                    ("click the map to place: " + map_place_arm +
                     "   (Esc cancels)").c_str());
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) map_place_arm.clear();
        if (hovered && !hit && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            to_geo(mouse, map_ctx_lat, map_ctx_lon);
            map_place_existing(map_place_arm);
            map_place_arm.clear();
        }
    } else if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ed.selection.clear(); // easy deselect → the panel shows the map list
    }

    // right-click: a MARKER gets its own menu (act on the thing); empty map
    // gets the place menu (create here)
    if (!was_drawing && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        to_geo(mouse, map_ctx_lat, map_ctx_lon);
        if (hit) {
            map_ctx_marker = hit->name;
            ImGui::OpenPopup("map-marker-menu");
        } else if (ref_hit) {
            map_ctx_ref = ref_hit->name;
            ImGui::OpenPopup("map-ref-menu");
        } else if (shape_hit) {
            map_ctx_shape = shape_hit->name;
            ImGui::OpenPopup("map-shape-menu");
        } else {
            ImGui::OpenPopup("map-place-menu");
        }
    }
    if (ImGui::BeginPopup("map-ref-menu")) { // #4: a reference point's actions
        const maiz::SceneNode* rf = scene.find(map_ctx_ref);
        if (!rf) ImGui::CloseCurrentPopup();
        else {
            std::string lbl = hormiga::temper::field_value(*rf, "label");
            ImGui::TextDisabled("reference point: %s",
                                lbl.empty() ? rf->name.c_str() : lbl.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Edit (inspector)")) ed.selection = {rf->name};
            // how many children point here
            int kids = 0;
            for (const auto& en : scene.nodes)
                if (hormiga::temper::field_value(en, "ref") == rf->name) ++kids;
            ImGui::TextDisabled("%d child marker(s) fan out here", kids);
            ImGui::Separator();
            if (ImGui::MenuItem("Delete (detaches children)")) {
                // detach children (clear their ref) then remove the gizmo
                std::vector<std::string> cmds;
                for (const auto& en : scene.nodes)
                    if (hormiga::temper::field_value(en, "ref") == rf->name)
                        cmds.push_back("set " + en.name + " ref \"\"");
                cmds.push_back("rm " + rf->name);
                pending_cmds.push_back(maiz::compile_commit(cmds));
                ed.selection.clear();
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("map-shape-menu")) { // #3: a drawn shape's actions
        const maiz::SceneNode* sp = scene.find(map_ctx_shape);
        if (!sp) ImGui::CloseCurrentPopup();
        else {
            ImGui::TextDisabled("%s (%s)", sp->name.c_str(),
                                hormiga::temper::field_value(*sp, "kind").c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("Edit (inspector)")) ed.selection = {sp->name};
            std::string cur_color = tag_value(*sp, "color");
            if (ImGui::BeginMenu("Color")) {
                if (ImGui::MenuItem("(default)", nullptr, cur_color.empty()) &&
                    !cur_color.empty())
                    pending_cmds.push_back("tag " + sp->name + " -color:" + cur_color);
                for (const auto& c : kMarkerColors) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImGui::ColorConvertU32ToFloat4(c.col));
                    bool pick = ImGui::MenuItem(c.tag, nullptr, cur_color == c.tag);
                    ImGui::PopStyleColor();
                    if (pick) {
                        std::string cmd = "tag " + sp->name;
                        if (!cur_color.empty()) cmd += " -color:" + cur_color;
                        pending_cmds.push_back(cmd + " +color:" + std::string(c.tag));
                    }
                }
                ImGui::EndMenu();
            }
            // bestow a tag on entities inside (materialize the spatial relation)
            static char bestow_buf[48];
            std::string cur_bestow = hormiga::temper::field_value(*sp, "bestows");
            ImGui::TextDisabled("bestows tag: %s",
                                cur_bestow.empty() ? "(none)" : cur_bestow.c_str());
            ImGui::SetNextItemWidth(120);
            if (ImGui::InputTextWithHint("##bestow", "tag to bestow", bestow_buf,
                                         sizeof bestow_buf,
                                         ImGuiInputTextFlags_EnterReturnsTrue) &&
                bestow_buf[0]) {
                pending_cmds.push_back("set " + sp->name + " bestows \"" +
                                       bestow_buf + "\"");
                bestow_buf[0] = 0;
            }
            if (!cur_bestow.empty() &&
                ImGui::MenuItem("Apply tag to entities inside now")) {
                // materialize: point-in-bbox → tag each contained entity
                double la1, lo1, la2, lo2;
                if (hormiga::parse_geo(hormiga::temper::field_value(*sp, "geo1"),
                                       la1, lo1) &&
                    hormiga::parse_geo(hormiga::temper::field_value(*sp, "geo2"),
                                       la2, lo2)) {
                    double laL = std::min(la1, la2), laH = std::max(la1, la2);
                    double loL = std::min(lo1, lo2), loH = std::max(lo1, lo2);
                    std::vector<std::string> cmds;
                    for (const auto& en : scene.nodes) {
                        if (en.glyph == "mapshape" || en.glyph == "map") continue;
                        double ela, elo;
                        std::string eg = view_geo(en, active_channel);
                        if (eg.empty() || !hormiga::parse_geo(eg, ela, elo)) continue;
                        if (ela >= laL && ela <= laH && elo >= loL && elo <= loH)
                            cmds.push_back("tag " + en.name + " +" + cur_bestow);
                    }
                    if (!cmds.empty()) {
                        pending_cmds.push_back(maiz::compile_commit(cmds));
                        toast("bestowed @" + cur_bestow + " on " +
                              std::to_string(cmds.size()) + " entities inside");
                    } else {
                        toast("no entities inside this shape", true);
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete shape")) {
                pending_cmds.push_back("rm " + sp->name);
                ed.selection.clear();
            }
        }
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("map-marker-menu")) {
        const maiz::SceneNode* mk = scene.find(map_ctx_marker);
        if (!mk) ImGui::CloseCurrentPopup();
        else marker_menu_items(*mk);
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("map-place-menu")) {
        ImGui::TextDisabled("place at %.5f, %.5f", map_ctx_lat, map_ctx_lon);
        ImGui::Separator();
        if (ImGui::MenuItem("New contact here")) map_place_new("contact");
        if (ImGui::MenuItem("New organization here")) map_place_new("organization");
        if (ImGui::MenuItem("New event here")) map_place_new("event");
        if (ImGui::MenuItem("New incident here")) map_place_new("incident");
        // #4: a reference point — a gizmo children fan out around
        if (ImGui::MenuItem("New reference point here")) {
            std::string name;
            for (int i = 1;; ++i) {
                name = "refpoint-" + std::to_string(i);
                if (!scene.find(name)) break;
            }
            char geo[64];
            std::snprintf(geo, sizeof geo, "%.7g,%.7g", map_ctx_lat, map_ctx_lon);
            pending_cmds.push_back(maiz::compile_commit(
                {"rune new refpoint " + name, "set " + name + " geo \"" + geo + "\"",
                 "set " + name + " label \"Reference\""}));
            ed.selection = {name};
        }
        ImGui::Separator();
        ImGui::TextDisabled("place existing (search):");
        std::string ctx_pick = search_picker(
            "##ctxplace", ctx_search, sizeof ctx_search,
            [this](const maiz::SceneNode& n) {
                return n.glyph != "map" && n.glyph != "image" &&
                       n.glyph != "note" &&
                       view_geo(n, active_channel).empty(); // unplaced here
            },
            "type a name...");
        if (!ctx_pick.empty()) {
            map_place_existing(ctx_pick);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    dl->PopClipRect();
    ImGui::EndChild();

    ImGui::SameLine(0, 0);
    auto vs = maiz::splitter("##map-vsplit", true, canvas_frac, area.x - th, 0.4f,
                             0.92f, th, body_h);
    ImGui::SameLine(0, 0);
    ImGui::BeginChild("map-inspector", ImVec2(0, body_h));
    bool have_sel = !ed.selection.empty() && scene.find(ed.selection.front());
    if (ed.selection.size() > 1) {
        // ── MULTI-SELECT (box/shift, author #2): act on the whole set at once ─
        if (ImGui::SmallButton("< back to map list")) ed.selection.clear();
        ImGui::SameLine();
        ImGui::Text("%d selected", (int)ed.selection.size());
        ImGui::Separator();
        for (const auto& nm : ed.selection) {
            const maiz::SceneNode* n = scene.find(nm);
            ImGui::BulletText("%s%s", nm.c_str(),
                              n ? ("  (" + n->glyph + ")").c_str() : "");
        }
        ImGui::Spacing();
        ImGui::TextDisabled("apply to all selected:");
        // tag every selected entity (the reusable tag picker, #4)
        std::string t = tag_picker("##multitag", tag_pick_input, sizeof tag_pick_input,
                                   "tag all selected...");
        if (!t.empty())
            for (const auto& nm : ed.selection)
                pending_cmds.push_back("tag " + nm + " +" + t);
        // color/icon submenus via a small popup
        if (ImGui::Button("Color all")) ImGui::OpenPopup("##multicolor");
        ImGui::SameLine();
        if (ImGui::Button("Icon all")) ImGui::OpenPopup("##multiicon");
        ImGui::SameLine();
        if (ImGui::Button("Remove from map")) {
            std::string fld = geo_field_for(active_channel);
            for (const auto& nm : ed.selection) {
                std::vector<std::string> c = {"set " + nm + " " + fld + " \"\""};
                if (fld == "geo") c.push_back("tag " + nm + " -located");
                pending_cmds.push_back(maiz::compile_commit(c));
            }
            ed.selection.clear();
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete all")) {
            for (const auto& nm : ed.selection) pending_cmds.push_back("rm " + nm);
            ed.selection.clear();
        }
        if (ImGui::BeginPopup("##multicolor")) {
            for (const auto& c : kMarkerColors) {
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::ColorConvertU32ToFloat4(c.col));
                bool pick = ImGui::MenuItem(c.tag);
                ImGui::PopStyleColor();
                if (pick)
                    for (const auto& nm : ed.selection) {
                        const maiz::SceneNode* n = scene.find(nm);
                        std::string cur = n ? tag_value(*n, "color") : "";
                        std::string cmd = "tag " + nm;
                        if (!cur.empty()) cmd += " -color:" + cur;
                        cmd += " +color:" + std::string(c.tag);
                        pending_cmds.push_back(cmd);
                    }
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##multiicon")) {
            for (const auto& ic : kMarkerIcons) {
                std::string lbl = std::string(ic.glyph) + "  " + ic.label;
                if (ImGui::MenuItem(lbl.c_str()))
                    for (const auto& nm : ed.selection) {
                        const maiz::SceneNode* n = scene.find(nm);
                        std::string cur = n ? tag_value(*n, "icon") : "";
                        std::string cmd = "tag " + nm;
                        if (!cur.empty()) cmd += " -icon:" + cur;
                        cmd += " +icon:" + std::string(ic.tag);
                        pending_cmds.push_back(cmd);
                    }
            }
            ImGui::EndPopup();
        }
    } else if (have_sel) {
        if (ImGui::SmallButton("< back to map list")) ed.selection.clear();
        ImGui::Separator();
        maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
        for (const auto& cmd : iio.commands) pending_cmds.push_back(cmd);
    } else {
        // ── nothing selected: the map PANEL — search, everything placed,
        // and this map's style RULES ────────────────────────────────────────
        std::string picked = search_picker(
            "##mappanelsearch", panel_search, sizeof panel_search,
            [](const maiz::SceneNode& n) {
                return n.glyph != "map" && n.glyph != "image";
            },
            "search this map / the database...");
        if (!picked.empty()) {
            const maiz::SceneNode* pn = scene.find(picked);
            std::string g = pn ? hormiga::temper::field_value(*pn, "geo") : "";
            double la, lo;
            if (!g.empty() && hormiga::parse_geo(g, la, lo)) {
                map_cam.x = (float)lo;
                map_cam.y = (float)la;
                if (map_cam.zoom < 15) map_cam.zoom = 15;
                pending_cmds.push_back(
                    maiz::compile_camera(map_cam, "view.map.camera"));
                ed.selection = {picked};
            } else {
                map_place_arm = picked; // click-to-place
            }
        }
        ImGui::Spacing();
        ImGui::SeparatorText(
            ("On this map (" + std::to_string(located_nodes.size()) + ")").c_str());
        ImGui::BeginChild("##maplist", ImVec2(0, ImGui::GetContentRegionAvail().y *
                                                     0.55f));
        for (const auto& L : located_nodes) {
            ImGui::PushID(L.node->name.c_str());
            std::string lbl = L.node->name + "  (" + L.node->glyph + ")";
            bool sel = ed.selected(L.node->name);
            if (ImGui::Selectable(lbl.c_str(), sel)) {
                map_cam.x = (float)L.lon;
                map_cam.y = (float)L.lat;
                pending_cmds.push_back(
                    maiz::compile_camera(map_cam, "view.map.camera"));
                ed.selection = {L.node->name};
            }
            // author #1: right-click a list row → the SAME marker menu as the map
            if (ImGui::BeginPopupContextItem("##rowmenu"))
                marker_menu_items(*L.node), ImGui::EndPopup();
            ImGui::PopID();
        }
        if (located_nodes.empty())
            ImGui::TextDisabled("nothing placed yet - right-click the map");
        ImGui::EndChild();

        // ── this view's RULES: named, listed with LIVE match counts + conflict
        // flags, click-to-EDIT, right-click for the full menu, x to delete.
        // Rules apply automatically every frame (the marker styling loop reads
        // active_rules) — there is no separate "apply" step; adding a rule
        // restyles the map immediately (author #5). ─────────────────────────
        ImGui::SeparatorText("Rules (tags -> style)");
        if (cur) {
            bool rules_dirty = false;
            auto stage_edit = [&](int i) { // load a rule into the editor window
                const auto& r = active_rules[i];
                rule_edit_idx = i;
                std::snprintf(rule_name, sizeof rule_name, "%s", r.name.c_str());
                rule_tags = r.tags;
                rule_tag_input[0] = 0;
                rule_icon = 0;
                for (int ii = 0; ii < (int)(sizeof kMarkerIcons /
                                            sizeof kMarkerIcons[0]); ++ii)
                    if (r.icon == kMarkerIcons[ii].tag) rule_icon = ii + 1;
                rule_color = 0;
                for (int ci = 0; ci < (int)(sizeof kMarkerColors /
                                            sizeof kMarkerColors[0]); ++ci)
                    if (r.color == kMarkerColors[ci].tag) rule_color = ci + 1;
                rule_shape = 0;
                for (int si = 0; si < (int)(sizeof kMarkerShapes /
                                            sizeof kMarkerShapes[0]); ++si)
                    if (r.shape == kMarkerShapes[si]) rule_shape = si;
                show_rule_editor = true;
            };
            // live audit: for each data entity, the FIRST matching rule wins;
            // a later rule that also matches is SHADOWED (never styles it).
            std::vector<int> match_count(active_rules.size(), 0);
            std::vector<int> shadowed_by(active_rules.size(), -1);
            for (const auto& node : scene.nodes) {
                if (node.glyph == "map" || node.glyph == "image" ||
                    node.glyph == "note")
                    continue;
                int first = -1;
                for (size_t i = 0; i < active_rules.size(); ++i) {
                    if (active_rules[i].tags.empty() ||
                        !maiz::node_matches(rule_filter_expr(active_rules[i]), node))
                        continue;
                    if (first < 0) { first = (int)i; ++match_count[i]; }
                    else if (shadowed_by[i] < 0) shadowed_by[i] = first;
                }
            }
            for (size_t i = 0; i < active_rules.size(); ++i) {
                ImGui::PushID((int)i);
                if (ImGui::SmallButton("x")) {
                    active_rules.erase(active_rules.begin() + i--);
                    rules_dirty = true;
                    ImGui::PopID();
                    continue;
                }
                ImGui::SameLine();
                const auto& r = active_rules[i];
                const char* ig = nullptr;
                for (const auto& ic : kMarkerIcons)
                    if (r.icon == ic.tag) ig = ic.glyph;
                ImU32 rc = IM_COL32(60, 60, 60, 255);
                for (const auto& c : kMarkerColors)
                    if (r.color == c.tag) rc = c.col;
                std::string lbl = std::string(ig ? ig : "") + " " + r.name + "  [" +
                                  rule_filter_expr(r) + "]  (" +
                                  std::to_string(match_count[i]) + ")";
                if (shadowed_by[i] >= 0) lbl = ICON_FA_TRIANGLE_EXCLAMATION " " + lbl;
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImGui::ColorConvertU32ToFloat4(rc));
                bool open_edit = ImGui::Selectable(lbl.c_str());
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) {
                    if (shadowed_by[i] >= 0)
                        ImGui::SetTooltip(
                            "conflict: '%s' matches first and wins for some\n"
                            "entities (first rule wins). Reorder or narrow tags.\n"
                            "click to edit - right-click for more",
                            active_rules[shadowed_by[i]].name.c_str());
                    else
                        ImGui::SetTooltip("styles %d on the map\n"
                                          "click to edit - right-click for more",
                                          match_count[i]);
                }
                if (open_edit) stage_edit((int)i);
                if (ImGui::BeginPopupContextItem("##rulemenu")) { // author #5
                    if (ImGui::MenuItem("Edit...")) stage_edit((int)i);
                    if (ImGui::MenuItem("Duplicate")) {
                        MapRule dup = r;
                        dup.name += " copy";
                        active_rules.insert(active_rules.begin() + i + 1, dup);
                        rules_dirty = true;
                    }
                    ImGui::BeginDisabled(i == 0);
                    if (ImGui::MenuItem("Move up (higher priority)")) {
                        std::swap(active_rules[i], active_rules[i - 1]);
                        rules_dirty = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled(i + 1 >= active_rules.size());
                    if (ImGui::MenuItem("Move down (lower priority)")) {
                        std::swap(active_rules[i], active_rules[i + 1]);
                        rules_dirty = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete")) {
                        active_rules.erase(active_rules.begin() + i);
                        rules_dirty = true;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }
            if (ImGui::Button("+ Rule")) {
                rule_edit_idx = -1;
                rule_name[0] = 0;
                rule_tags.clear();
                rule_tag_input[0] = 0;
                rule_icon = 0;
                rule_color = 0;
                rule_shape = 0;
                show_rule_editor = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(n) = how many it styles; " ICON_FA_TRIANGLE_EXCLAMATION
                                " = shadowed");
            if (rules_dirty) save_rules(cur->name);

            // ── MAP CONFIG (author #6): view display knobs, stored as fields on
            // the view rune ("internally rules"), surfaced like settings. They
            // shape BOTH the live canvas and the PNG export so the exported
            // image shows the labels/sizes you set. ─────────────────────────
            ImGui::SeparatorText("Map config (this view)");
            bool show_lbl = view_show_labels(cur);
            if (ImGui::Checkbox("Show labels", &show_lbl))
                pending_cmds.push_back("set " + cur->name + " show_labels \"" +
                                       (show_lbl ? "1" : "0") + "\"");
            float lscale;
            ImGui::SetNextItemWidth(160);
            if (slider_field("Label size", view_label_scale(cur), 0.6f, 2.5f,
                             "%.2fx", &lscale)) {
                char b[48];
                std::snprintf(b, sizeof b, "%.2f", lscale);
                pending_cmds.push_back("set " + cur->name + " label_scale \"" +
                                       b + "\"");
            }
            bool nol = view_no_overlap(cur);
            if (ImGui::Checkbox("No overlap (labels dodge)", &nol))
                pending_cmds.push_back("set " + cur->name + " no_overlap \"" +
                                       (nol ? "1" : "0") + "\"");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("labels try right/left/above/below; when\n"
                                  "nothing fits, a label yields (markers\n"
                                  "always draw)");
            // label COLOR (author: "changes to font color, fonts in general")
            std::string cur_lc = hormiga::temper::field_value(*cur, "label_color");
            if (cur_lc.empty()) cur_lc = "dark";
            ImGui::SetNextItemWidth(160);
            if (ImGui::BeginCombo("Label color", cur_lc.c_str())) {
                const char* fixed[] = {"dark", "black", "white"};
                for (const char* f : fixed)
                    if (ImGui::Selectable(f, cur_lc == f))
                        pending_cmds.push_back("set " + cur->name +
                                               " label_color \"" + f + "\"");
                for (const auto& c : kMarkerColors) {
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImGui::ColorConvertU32ToFloat4(c.col));
                    bool pick = ImGui::Selectable(c.tag, cur_lc == c.tag);
                    ImGui::PopStyleColor();
                    if (pick)
                        pending_cmds.push_back("set " + cur->name +
                                               " label_color \"" + c.tag + "\"");
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled("applies to the map and its PNG export\n"
                                "(font FAMILY choice needs vendored fonts -\n"
                                "a noted future)");
        }
    }
    ImGui::EndChild();
    if (vs.released) flush_panels();
}

/* The `map` verb-macro front-end (Core's 2026-07-21 ruling made real): the
 * command bar calls the SAME ActionDescriptor.compile a canvas gesture will,
 * and dispatches the result as ONE `batch` — atomic, undoable, attributed.
 * `map actions` prints the manifest: an agent's discovery surface. Returns
 * true when the line was a map verb (handled + logged here). */
bool HormigaApp::view_show_labels(const maiz::SceneNode* v) const {
    if (!v) return true;
    std::string s = hormiga::temper::field_value(*v, "show_labels");
    return s != "0"; // default ON; only explicit "0" hides
}
float HormigaApp::view_layer_opacity(const maiz::SceneNode* v) const {
    if (!v) return 0.43f;
    std::string s = hormiga::temper::field_value(*v, "layer_opacity");
    if (s.empty()) return 0.43f;
    float f = (float)std::atof(s.c_str());
    return (f >= 0.05f && f <= 1.0f) ? f : 0.43f;
}
float HormigaApp::view_layer_brightness(const maiz::SceneNode* v) const {
    if (!v) return 1.0f;
    std::string s = hormiga::temper::field_value(*v, "layer_brightness");
    if (s.empty()) return 1.0f;
    float f = (float)std::atof(s.c_str());
    return (f >= 0.3f && f <= 1.5f) ? f : 1.0f;
}
bool HormigaApp::view_no_overlap(const maiz::SceneNode* v) const {
    if (!v) return true; // labels dodge each other by default (author ask)
    return hormiga::temper::field_value(*v, "no_overlap") != "0";
}
unsigned HormigaApp::view_label_color(const maiz::SceneNode* v) const {
    unsigned dark = IM_COL32(40, 40, 40, 255);
    if (!v) return dark;
    std::string s = hormiga::temper::field_value(*v, "label_color");
    if (s == "white") return IM_COL32(245, 245, 245, 255);
    if (s == "black") return IM_COL32(10, 10, 10, 255);
    for (const auto& c : kMarkerColors)
        if (s == c.tag) return c.col;
    return dark;
}

/* Parse a view rune's style rules — shared by the map canvas, the layer
 * compositor, both PNG exports, and the CALENDAR (the rules engine is
 * cross-view: okf/concepts/sections/territory.md). v1 {filter} read for compat. */
std::vector<HormigaApp::MapRule> HormigaApp::parse_view_rules_of(
    const maiz::SceneNode& view) {
    std::vector<MapRule> out;
    try {
        std::string rjs = hormiga::temper::field_value(view, "rules");
        auto rj = nlohmann::json::parse(rjs.empty() ? "[]" : rjs);
        for (const auto& r : rj) {
            MapRule mr;
            mr.name = r.value("name", "rule");
            mr.icon = r.value("icon", "");
            mr.color = r.value("color", "");
            mr.shape = r.value("shape", "");
            if (r.contains("tags"))
                for (const auto& t : r["tags"]) mr.tags.push_back(t);
            else if (!r.value("filter", "").empty()) // v1 compat
                mr.tags.push_back(r.value("filter", ""));
            out.push_back(std::move(mr));
        }
    } catch (...) {}
    return out;
}
std::string HormigaApp::rule_expr_of(const MapRule& r) {
    std::string e;
    for (const auto& t : r.tags) e += (e.empty() ? "" : " AND ") + t;
    return e;
}

/* The STYLE tab — the PLACEHOLDER for the full theming workspace (builder.md
 * QE, author-decided): the Builder stays the daily drag-drop tool; making the
 * newsletter/website "look your own" (themes, fonts, logos, banners, shapes)
 * lives HERE, later. Today: two live theme colors proving the socket — both
 * render packs read them, so the real tab grows into an existing seam. */
/* The STYLE tab — the theme editor (builder.md QE). It and the Builder are one
 * system (author, 2026-07-23): the Builder arranges components, the Style tab
 * decides how they look. Every knob is config-tier (theme.*), lands as a
 * pending_cmd, and — with Live preview on — re-renders the site instantly
 * (dispatch_and_reproject marks the preview dirty). Three of the author's
 * seven style axes live here now: presets (aesthetic modes), heading font
 * (bold typography), and browser dark/light reactivity. */
/* ── THE STYLE TAB (rebuilt 2026-08-20) ──────────────────────────────────────
 *
 * The author: *"revamp the style tab, before it was more of an afterthought …
 * give it some more functionality … mostly think about websites right now.
 * always remember that now we are developing for a headless mode as well."*
 *
 * Both halves of that shaped this.
 *
 * ── EVERY KNOB IS A `config set`, AND THAT IS THE WHOLE DESIGN ───────────────
 *
 * Nothing here holds state of its own. Each control emits `config set theme.*`,
 * which is a dispatcher command — so the tab is a *view* of the theme rather
 * than an editor with its own copy, and an agent setting `theme.contrast` from
 * a script and a person moving this slider are the same change in the same log.
 * That is founding commitment 1 at the level of a colour picker, and it is why
 * this file contains no theme logic: the arithmetic lives in `app_shared.cpp`
 * where the RENDERER calls it, so the number shown here is by construction the
 * number the website will use.
 *
 * ── THE CONTRAST READOUT IS THE POINT OF THE REVAMP ──────────────────────────
 *
 * A colour picker that lets you choose unreadable text and says nothing is not
 * a design tool, it is a trap with a nice widget. This one computes the WCAG
 * ratio for every pairing the site actually generates, shows it, and says which
 * ones the renderer is going to override — so a person can see WHY their yellow
 * accent produced dark button text instead of watching it change under them.
 */

