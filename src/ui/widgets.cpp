/* ui/widgets.cpp — reusable GUI pieces, owned by no section.
 *
 * The search picker and the tag picker are used by Data, the Builder, the map
 * and the rule editor, and lived in the map file because that is where the
 * first caller was. A widget every section uses belongs to none of them. */

#include "app/app_internal.hpp"
#include "domain/bestow.hpp" // givers: the tag vocabulary and the redirect
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

std::string HormigaApp::search_picker(
    const char* id, char* buf, size_t bufsz,
    const std::function<bool(const maiz::SceneNode&)>& keep, const char* hint) {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint(id, hint, buf, (int)bufsz);
    if (!buf[0]) return {};
    std::string picked;
    int shown = 0;
    for (const auto& n : scene.nodes) {
        if (keep && !keep(n)) continue;
        if (!contains_ci(n.name, buf)) continue;
        if (++shown > 8) { ImGui::TextDisabled("(keep typing...)"); break; }
        /* ONE ID PER ROW, BY RUNE NAME (2026-09-19). The label used to end in
         * "##" + id, and every caller's `id` already starts with "##" -- ImGui
         * reads from the FIRST "##", so all eight rows shared the id
         * "##linksearch" and Dear ImGui's conflict detector painted them red. */
        ImGui::PushID(n.name.c_str());
        std::string lbl = n.name + "  (" + n.glyph + ")";
        if (ImGui::Selectable(lbl.c_str())) {
            picked = n.name;
            buf[0] = 0;
        }
        ImGui::PopID();
        std::string sub = subtitle(n);
        if (!sub.empty()) {
            ImGui::Indent(12);
            ImGui::TextDisabled("%s", sub.c_str());
            ImGui::Unindent(12);
        }
    }
    if (shown == 0) ImGui::TextDisabled("no matches");
    return picked;
}

/* The TAG PICKER (author #4): a tag entry that behaves like the search bar but
 * searches over tags ALREADY in the database — type-ahead over the existing
 * vocabulary, so tags stay consistent instead of drifting into near-duplicates.
 * A brand-new tag can still be committed with Enter (the "+ create" row).
 * Returns the chosen/typed tag WITHOUT its `+`/`-` sigil, or "" ; clears buf on
 * commit. Namespaced tags (icon:, color:, month:) are hidden — those have their
 * own dedicated UI and shouldn't be hand-typed here. */
std::string HormigaApp::tag_picker(const char* id, char* buf, size_t bufsz,
                                   const char* hint) {
    // gather the existing tag vocabulary once (deduped, sorted, de-namespaced)
    std::set<std::string> vocab;
    for (const auto& n : scene.nodes)
        for (const auto& t : n.tags) {
            // namespaced tags (kw:food, clearance:public) are offered too since
            // 2026-09-13 - they were skipped, which made them impossible to find
            vocab.insert(t);
        }
    /* A TAG CAN EXIST BEFORE ANYTHING CARRIES IT (2026-09-15): a map shape that
     * bestows `apple` makes `apple` a tag of this database even while nothing is
     * inside the shape, so it is searchable here (domain/bestow.hpp). */
    if (scene.mantle == kDataMantle)
        for (const auto& b : hormiga::bestow::bestowers(scene)) vocab.insert(b.tag);
    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputTextWithHint(
        id, hint, buf, (int)bufsz, ImGuiInputTextFlags_EnterReturnsTrue);
    std::string typed = buf;
    std::string chosen;
    if (!typed.empty()) {
        /* ONE ID PER ROW (2026-09-15). The label was `"@" + tag + "##" + id`,
         * and every caller's id begins with `##`, so a row read
         * `@color:blue####noteaddtag`. ImGui treats `###` as "the id is what
         * follows", which made every row in the list the SAME id: the
         * "2 visible items with conflicting ID" popup the author hit on Windows,
         * and a click that could land on a different row than the one pressed.
         * The id is pushed as scope instead, and each row is its own tag. */
        ImGui::PushID(id);
        int shown = 0;
        bool exact = false;
        for (const auto& t : vocab) {
            if (!contains_ci(t, typed.c_str())) continue;
            if (t == typed) exact = true;
            if (++shown > 8) { ImGui::TextDisabled("(keep typing...)"); break; }
            ImGui::PushID(t.c_str());
            if (ImGui::Selectable(("@" + t).c_str())) chosen = t;
            ImGui::PopID();
        }
        if (shown == 0) ImGui::TextDisabled("no existing tag matches");
        // offer to create the typed tag when it isn't already an exact match
        if (!exact) {
            std::string mk = "+ create tag \"" + typed + "\"";
            if (ImGui::Selectable((mk + "##create").c_str()) || enter)
                chosen = typed;
        } else if (enter) {
            chosen = typed;
        }
        ImGui::PopID();
    }
    if (!chosen.empty()) buf[0] = 0;
    return chosen;
}

/* Map CONFIG (author #6): view-level display knobs that are stored as ORDINARY
 * fields on the view rune (label_scale, show_labels) — "internally rules" in
 * the author's framing, surfaced like settings. They shape both the live canvas
 * and the PNG export, so an exported map shows readable labels at a chosen size.
 * Defaults: scale 1.0, labels ON. */
float HormigaApp::view_label_scale(const maiz::SceneNode* v) const {
    if (!v) return 1.0f;
    std::string s = hormiga::temper::field_value(*v, "label_scale");
    if (s.empty()) return 1.0f;
    float f = (float)std::atof(s.c_str());
    return (f >= 0.4f && f <= 4.0f) ? f : 1.0f;
}

/* ── THE THREE BELOW ARRIVED 2026-09-02, AND THEY BELONG HERE FOR THE REASON
 *    THE HEADER ALREADY GIVES ────────────────────────────────────────────────
 *
 * They were written in `ui/data.cpp` because that is where the first caller was
 * — the same way the search and tag pickers were written in the map file — and
 * `tools/find_long.py` caught it the same afternoon. Each of them is used by a
 * section that is not Data:
 *
 *   `rune_rename_control` — the Data detail pane AND the Notes tab. A note is
 *       a rune the Data tab deliberately skips, which is exactly why it was the
 *       one kind of rune in the application that could not be renamed.
 *   `org_image_picker`   — the Style tab's branding, and the next block that
 *       wants an image the organization already has.
 *   `ingest_image_rune`  — every "bring a file in" affordance, because the
 *       author's rule is general: an upload becomes an image ASSET, not a path.
 */
/* ── ONE RENAME CONTROL, FOR EVERY TAB THAT SHOWS A RUNE (2026-09-02) ────────
 *
 * The author, from a hands-on session: *"cant rename notes"*. They could not,
 * and the reason is structural rather than a missing widget. The Data tab's
 * detail pane has had a rename box since it was written — and `draw_data_body`
 * skips `note` and `rule` runes entirely, because both have their own tab. So
 * the one surface that could rename a rune was the one surface that never
 * showed the runes in question, and the Notes tab printed the name with
 * `TextDisabled`, which is ImGui for "this is not yours to touch".
 *
 * Lifting it out rather than copying it into `draw_notes_body` is the point: a
 * second copy would be a second place to forget that a rename has to reselect
 * the new name, restage the buffer, and — the part that is easy to get wrong —
 * stop touching `sel`, which points into a projection the dispatch just
 * invalidated.
 *
 * Returns TRUE when a rename landed, which means the caller's `sel` is dangling
 * and it must return immediately. Making that the return value rather than a
 * comment is the only way a third caller cannot get it wrong.
 */
bool HormigaApp::rune_rename_control(const maiz::SceneNode& sel, float width) {
    if (rename_for != sel.name) {
        rename_for = sel.name;
        std::snprintf(rename_buf, sizeof rename_buf, "%s", sel.name.c_str());
    }
    ImGui::SetNextItemWidth(width);
    const bool entered =
        ImGui::InputText("##rename", rename_buf, sizeof rename_buf,
                         ImGuiInputTextFlags_EnterReturnsTrue);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("edit + Enter to rename (undoable; every link "
                          "and reference repoints)");
    if (!entered) return false;
    const std::string nn = hormiga::detail::slug(rename_buf);
    if (nn.empty() || nn == sel.name) return false;
    /* THE NAME IS COPIED BEFORE THE DISPATCH. `sel` is a reference into the
     * scene the reprojection below replaces, so reading `sel.name` afterwards —
     * which the error path does — is a use-after-free waiting for a failing
     * rename to find it. */
    const std::string was = sel.name;
    const maiz::Result r = dispatch_and_reproject("rune rename " + was + " " + nn);
    if (!r.ok) {
        toast("rename failed: " + r.text(), true);
        return false;
    }
    ed.selection = {nn};
    rename_for.clear(); // restage from the new name
    toast("renamed to " + nn + " (references repointed)");
    return true;
}

/* ── CHOOSING AN IMAGE THE ORGANIZATION ALREADY HAS (2026-09-02) ─────────────
 *
 * A popup listing every `image` rune in the data mantle that resolves to a file
 * on this machine, with a thumbnail each, returning the chosen rune's `path`.
 * Empty means nothing was picked this frame.
 *
 * It is a shared control rather than a block of code in the Style tab because
 * the author's sentence was general — *"In general, the images we use should be
 * from the image assets"* — and the next place that needs it (a hero's banner,
 * a band background, a video poster) should not get a second, differently
 * behaved picker. Those already pick from the org's images; this brings
 * branding in line with them.
 *
 * ONLY RUNES WHOSE FILE IS ACTUALLY HERE. An `image` rune may carry a remote
 * `url` and no local `path` — that is the ordinary state of a database whose
 * assets have not been mirrored yet — and offering one as a logo would set
 * `org.logo` to a blank and put nothing in the header. The count of what was
 * skipped is shown rather than swallowed, because "my images are not in this
 * list" needs an answer and the answer is `effect mirror-images`.
 */
std::string HormigaApp::org_image_picker(const char* popup_id,
                                         const char* heading) {
    std::string chosen;
    if (!ImGui::BeginPopup(popup_id)) return chosen;
    ImGui::TextDisabled("%s", heading);
    ImGui::Separator();

    maiz::ProjectOptions po;
    po.mantle = kDataMantle;
    const maiz::Scene data = maiz::project_scene(core, po);

    ImGui::SetNextItemWidth(220);
    ImGui::InputTextWithHint("##imgpicksearch", "search images...",
                             img_pick_search, sizeof img_pick_search);
    ImGui::BeginChild("##imgpicklist", ImVec2(360, 260));
    int shown = 0, remote_only = 0;
    for (const auto& n : data.nodes) {
        if (n.glyph != "image") continue;
        const std::string path = hormiga::temper::field_value(n, "path");
        if (path.empty()) { ++remote_only; continue; }
        if (!contains_ci(n.name, img_pick_search)) continue;
        ++shown;
        ImGui::PushID(n.name.c_str());
        const HostTexture t = texture_for(path);
        if (t.id) {
            const float hh = 34.0f;
            ImGui::Image((ImTextureID)(intptr_t)t.id,
                         ImVec2(hh * (float)t.w / std::max(1, t.h), hh));
            ImGui::SameLine();
        }
        if (ImGui::Selectable(n.name.c_str(), false, 0, ImVec2(0, 34))) {
            chosen = path;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", path.c_str());
        ImGui::PopID();
    }
    if (shown == 0)
        ImGui::TextDisabled(img_pick_search[0]
                                ? "no image matches"
                                : "this organization has no images with a local "
                                  "file yet");
    ImGui::EndChild();
    if (remote_only)
        ImGui::TextDisabled("%d image(s) hidden: they have a URL but no local\n"
                            "file yet. `effect mirror-images` brings them down.",
                            remote_only);
    ImGui::EndPopup();
    return chosen;
}

/* ── AN UPLOAD IS A NEW ASSET, NOT A PATH (2026-09-02) ───────────────────────
 *
 * `ingest_asset` content-hashes a file into `assets/` and hands back a
 * site-relative path — which is exactly half of what an upload should do. The
 * other half is that the organization now OWNS an image, and owning it in this
 * application means an `image` rune: something that can be tagged, queried by a
 * gallery, linked to the event it advertises, seen by the clearance seam, and
 * carried in the `.miga`.
 *
 * Without the rune, a logo browsed in from the Style tab was a path in config
 * and a file on disk, and nothing in the Data section knew it existed.
 *
 * `+type:image` and nothing else, on purpose. Tagging is the operator's
 * vocabulary and guessing at it is how a database fills with tags nobody chose;
 * the one namespaced tag every query in the application relies on is the
 * exception, and it is the exception the guide's first trap is about.
 *
 * Returns the site-relative path, or "" if the file could not be read (which
 * `ingest_asset` has already reported).
 */
std::string HormigaApp::ingest_image_rune(const std::string& src) {
    const std::string rel = ingest_asset(src);
    if (rel.empty()) return {};
    /* The naming, the `+type:image` tag and (since 2026-09-15) the upload are
     * `adopt_image`'s, so a file browsed into a block's image field and a logo
     * browsed into branding become the same kind of thing in the same way. */
    adopt_image(rel, fs::path(src).stem().string());
    return rel;
}

/* ── IMAGES COME FROM THE GALLERY AND GO THROUGH THE ANTFARM (2026-09-15) ────
 *
 * The author: *"for selecting images, we should be choosing from our image
 * gallery within the database. I guess there should be 2 ways we choose an
 * image. 1 is by browsing for files to upload a new image (we already do),
 * however, that process should include the process of using the antfarm to
 * actually get the image in our database. if something like an imgbb node is
 * set up, then it should automatically be uploaded."*
 *
 * What was broken underneath was worse than a missing button. Browsing a file
 * into a BLOCK's image field — a hero's banner, an image + text block — copied
 * it into assets/ and minted no `image` rune. So the picture was in no gallery,
 * had no `url`, could not be published, and the newsletter, which can only use
 * a public address, had nothing to draw: the "images not really appearing"
 * half of the same report.
 *
 * Now every image a person brings in becomes an image rune (`adopt_image`), and
 * when the Antfarm has an image host that can answer (domain/hosting.hpp: ImgBB,
 * an object store, or the website itself), it is put online and its `url` set in
 * the same step. From inside the inspector that work is handed to
 * `run_busy`, which runs it at the start of the next frame: it is a network
 * call, and dispatching mid-draw would invalidate the very node being drawn.
 */
#include "domain/image_presets.hpp"

#include <map>
#include <sstream>

namespace {
struct ImgState {
    std::string rune, url;
    bool ready = false;
    double at = -100.0;
};
std::map<std::string, ImgState> g_img_state; // path -> the gallery's answer, re-read every 2 s

} // namespace

/* The ImgBB transport, shared by `effect publish` and the automatic upload.
 * Permanent (no expiration parameter: a newsletter image must outlive the
 * send). The key rides only in the process invocation, never in the command
 * log. Returns the public url, or "" with the reason in the log. */
std::string HormigaApp::upload_to_imgbb(const std::string& path, const std::string& name) {
    if (imgbb_key.empty() || !on_shell_capture) return {};
    const fs::path abs = fs::path(path).is_absolute() ? fs::path(path) : base_dir / path;
    std::error_code ec;
    if (path.empty() || !fs::exists(abs, ec)) {
        log.push_back({"error", "publish", "no local image file at " + abs.string()});
        return {};
    }
    const std::string resp = on_shell_capture(
        "curl -s -F \"image=@" + abs.string() + "\" \"https://api.imgbb.com/1/upload?key=" +
        imgbb_key + "&name=" + name + "\"");
    const size_t pos = resp.find("\"url\":\"");
    if (pos == std::string::npos) {
        log.push_back({"error", "publish", resp.substr(0, 300)});
        return {};
    }
    std::string url;
    for (size_t i = pos + 7; i < resp.size() && resp[i] != '"'; ++i) {
        if (resp[i] == '\\' && i + 1 < resp.size() && resp[i + 1] == '/') continue;
        url += resp[i];
    }
    return url;
}

/* The file at `path` becomes an image rune in the data mantle if it is not one
 * already, and is uploaded if the Antfarm can and it has no `url`. Synchronous
 * dispatches: call it between frames (`run_busy`) or from a control that returns
 * straight afterwards, never from inside a widget still drawing a node. */
std::string HormigaApp::adopt_image(const std::string& path, const std::string& stem) {
    if (path.empty()) return {};
    maiz::ProjectOptions po;
    po.mantle = kDataMantle;
    maiz::Scene data = maiz::project_scene(core, po);
    std::string name, url;
    auto unquoted = [](std::string v) {
        if (v.size() >= 2 && v.front() == 0x22 && v.back() == 0x22)
            v = v.substr(1, v.size() - 2);
        return v;
    };
    for (const auto& n : data.nodes)
        if (n.glyph == "image" && unquoted(hormiga::temper::field_value(n, "path")) == path) {
            name = n.name;
            url = hormiga::temper::field_value(n, "url");
            break;
        }
    const std::string was = scene.mantle;
    const bool away = !was.empty() && was != kDataMantle;
    if (away) dispatch_and_reproject(std::string("use ") + kDataMantle);
    if (name.empty()) {
        /* A name from the file, unique against what is there. The content hash
         * in the asset's FILENAME makes re-adding the same picture find this
         * rune above rather than mint a second one. */
        std::string base =
            hormiga::detail::slug(stem.empty() ? fs::path(path).stem().string() : stem);
        if (base.empty()) base = "image";
        name = base;
        for (int i = 2; data.find(name); ++i) name = base + "-" + std::to_string(i);
        if (dispatch_and_reproject("rune new image " + name).ok) {
            dispatch_and_reproject("set " + name + " path " + json_str(path));
            dispatch_and_reproject("tag " + name + " +type:image");
            toast("added image '" + name + "' to the gallery - taggable and queryable "
                  "like every other image");
        } else {
            name.clear();
        }
    }
    if (away) dispatch_and_reproject("use " + was);
    // online through whichever image host the Antfarm has, if one can answer now
    if (!name.empty() && url.empty() && image_host_ready()) host_image(name);
    g_img_state.erase(path);
    return name;
}

/* + Flier, + Banner (domain/image_presets.hpp). The file first, because the
 * preset means a picture; cancelling the dialog adds nothing. The work runs
 * between frames: adopting can upload, and the button that asked is mid-draw. */
void HormigaApp::new_image_preset(const hormiga::ImagePreset& p) {
    if (!on_pick_file) {
        toast("no file dialog on this front-end", true);
        return;
    }
    const std::string picked = on_pick_file("");
    if (picked.empty()) return;
    const std::string managed = ingest_asset(picked);
    if (managed.empty()) return;
    const std::string stem = fs::path(picked).stem().string();
    const std::string id = p.id, tags = p.tags;
    run_busy(std::string("Adding the ") + p.id,
             [this, managed, stem, id, tags] {
                 const std::string name = adopt_image(managed, stem.empty() ? id : stem);
                 if (name.empty()) return;
                 std::vector<std::string> cmds;
                 std::istringstream ts(tags);
                 for (std::string t; ts >> t;) cmds.push_back("tag " + name + " +" + t);
                 if (!cmds.empty()) dispatch_and_reproject(maiz::compile_commit(cmds));
                 ed.selection = {name};
                 kind_sel = "image";
                 toast("added " + id + " '" + name + "' - tagged " + tags);
             });
}

void HormigaApp::register_image_editors() {
    /* The "path" editor kind: the library owns the box and the commit, this
     * owns the OS dialog. An asset-bearing field's pick is INGESTED (copied into
     * assets/, content-hash deduplicated). An image rune's own file is uploaded
     * once the path lands; a BLOCK's image is adopted into the gallery, and
     * uploaded, after this frame. */
    browse_ingest = [this](const maiz::SceneNode& n, const char* field,
                           std::string_view cur) -> std::string {
        if (!on_pick_file) return {};
        std::string picked = on_pick_file(cur);
        if (picked.empty()) return picked;
        const std::string_view fk(field);
        const bool image_rune = n.glyph == "image";
        const bool asset_field = image_rune || n.glyph == "resource" || fk == "image" ||
                                 fk == "portrait" || fk == "cover";
        if (!asset_field) return picked;
        std::string managed = ingest_asset(picked);
        if (managed.empty()) return picked; // ingest failed; keep the original
        if (image_rune || n.glyph == "resource") {
            bool has_month = false;
            for (const auto& t : n.tags)
                if (t.rfind("month:", 0) == 0) has_month = true;
            if (!has_month)
                pending_cmds.push_back("tag " + n.name + " +month:" + month_name_now());
        }
        if (image_rune) {
            const std::string rune = n.name;
            if (image_host_ready())
                run_busy("Putting the image online", [this, rune] { host_image(rune); });
        } else if (n.glyph != "resource") {
            const std::string stem = fs::path(picked).stem().string();
            run_busy("Adding the image to the gallery",
                     [this, managed, stem] { adopt_image(managed, stem); });
        }
        return managed;
    };
    if (on_pick_file) widgets.add_path(browse_ingest);

    /* One line under an image field: is this picture in the gallery, and can
     * an inbox load it? The two questions a person needs answered before
     * sending. A member lambda, because the answers are HormigaApp's. */
    auto state_line = [this](const std::string& path) {
        ImgState& s = g_img_state[path];
        const double now = ImGui::GetTime();
        if (now - s.at > 2.0) {
            s = ImgState{};
            s.at = now;
            maiz::ProjectOptions po;
            po.mantle = kDataMantle;
            const maiz::Scene data = maiz::project_scene(core, po);
            for (const auto& n : data.nodes)
                if (n.glyph == "image" && hormiga::temper::field_value(n, "path") == path) {
                    s.rune = n.name;
                    s.url = hormiga::temper::field_value(n, "url");
                    break;
                }
            s.ready = image_host_ready();
        }
        const ImVec4 green(0.35f, 0.70f, 0.40f, 1.0f), amber(0.85f, 0.60f, 0.15f, 1.0f);
        auto adopt_later = [this, path](const char* label) {
            run_busy(label, [this, path] { adopt_image(path, ""); });
        };
        if (!s.url.empty()) {
            ImGui::TextColored(green, ICON_FA_CIRCLE_CHECK "  online - shows in email");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", s.url.c_str());
        } else if (s.rune.empty()) {
            ImGui::TextColored(amber, "not in the image gallery yet");
            ImGui::SameLine();
            if (ImGui::SmallButton(s.ready ? "Add + host online" : "Add to gallery"))
                adopt_later("Adding the image to the gallery");
        } else if (s.ready) {
            ImGui::TextColored(amber, "not online yet");
            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_CLOUD_ARROW_UP "  Host it online"))
                adopt_later("Putting the image online");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("through the Antfarm's image host - see Antfarm >\n"
                                  "Hosting images online for which one, and why");
        } else {
            ImGui::TextDisabled("not online yet - an inbox cannot load it. No image\n"
                                "host in the Antfarm can answer: add ImgBB, an object\n"
                                "store with a public address, or your website's host.");
        }
    };

    /* The "image" editor kind: the path box, a thumbnail, the GALLERY — the
     * second way in, an image the organization already has — and a line saying
     * whether an inbox can load it. */
    widgets.editors["image"] = [this, state_line](maiz::WidgetContext& ctx,
                                                  const maiz::SceneNode& n,
                                      const maiz::SceneField& f, std::string_view) {
        // a phone has no file dialog: the path box and its "..." are the
        // desktop's; the gallery below is the way in on both
        bool committed = false;
        if (!phone)
            committed = maiz::widget_field_path(ctx, n, f.key.c_str(), browse_ingest,
                                                f.label.empty() ? nullptr : f.label.c_str());
        std::string p = f.value_json;
        if (f.is_string && p.size() >= 2) p = p.substr(1, p.size() - 2);
        if (p == "null") p.clear();
        ImGui::PushID(f.key.c_str());
        // a phone's own photos, through the system's photo picker (the author,
        // 2026-09-25: "images can't be shared or uploaded" from the phone)
        if (phone && ImGui::Button(ICON_FA_CAMERA "  Choose a photo")) phone_pick_photo(n.name, f.key);
        if (phone ? ImGui::Button(ICON_FA_IMAGES "  One this database has")
                  : ImGui::SmallButton(ICON_FA_IMAGES "  Choose from the gallery"))
            ImGui::OpenPopup("##gallery");
        const std::string chosen = org_image_picker("##gallery", "this organization's images");
        if (!chosen.empty()) {
            ctx.commands.push_back("set " + n.name + " " + f.key + " " + json_str(chosen));
            committed = true;
        }
        if (!p.empty()) {
            HostTexture t = texture_for(p);
            if (t.id) {
                float w = std::min(220.0f, (float)t.w);
                ImGui::Image((ImTextureID)(intptr_t)t.id,
                             ImVec2(w, w * (float)t.h / (float)t.w));
            } else {
                ImGui::TextDisabled("(image not found: %s)", p.c_str());
            }
            if (n.glyph != "image" && !phone) state_line(p); // email hosting: the desktop's business
        }
        ImGui::PopID();
        return committed;
    };
}

/* A message from the platform shell (main/desktop.cpp) - today, that no file
 * dialog could open on this computer - shown the way the app shows its own. */
void HormigaApp::host_notice(const std::string& msg) {
    toast(msg, true);
    log.push_back({"warn", "shell", msg});
}

/* ── REDIRECTION: SHOW A THING WHERE IT LIVES (2026-09-15) ───────────────────
 *
 * The author: *"this also brings up the concept of 'redirection'. this is almost
 * purely a GUI thing, so theoretically there should be no need for a CLI thing.
 * However, i do think it is worth logging that the view has changed. The
 * redirecting to view certain aspects of the application is an important feature
 * to get down right."*
 *
 * A redirect names a rune and its mantle. It happens at the start of the next
 * frame, never mid-draw: it changes the mantle, the selection and the focused
 * window, and doing any of that inside the widget that asked would invalidate
 * the very scene that widget is drawing. Which window a rune belongs to is read
 * from what it is: a map shape opens the Map centred on it, a note the Notes tab,
 * a rule Allomone, an Antfarm node the Antfarm, a block the Builder on its
 * document, and anything else the Data tab.
 *
 * Nothing about the model changes, so it is not a dispatcher command and has no
 * undo. It is still RECORDED: a `view` entry in the log strip saying why the view
 * moved and to what, because an application whose view jumps without a trace is
 * one where "how did I get here" has no answer. */
void HormigaApp::redirect_to(const std::string& rune, const std::string& mantle,
                             const std::string& why) {
    redirect_next = {rune, mantle, why};
}

void HormigaApp::apply_redirect() {
    if (redirect_next.rune.empty()) return;
    const Redirect r = redirect_next;
    redirect_next = {};
    maiz::ProjectOptions po;
    po.mantle = r.mantle.empty() ? std::string(kDataMantle) : r.mantle;
    maiz::Scene target = maiz::project_scene(core, po);
    const maiz::SceneNode* n = target.find(r.rune);
    if (!n) {
        toast("cannot find " + r.rune + " any more", true);
        return;
    }
    const std::string glyph = n->glyph;
    std::string where = "Data";
    if (glyph == "mapshape" || glyph == "map" || glyph == "refpoint") {
        where = "Map";
        sec_open[Map] = true;
        double la1, lo1, la2, lo2;
        if (glyph == "mapshape" &&
            hormiga::parse_geo(hormiga::temper::field_value(*n, "geo1"), la1, lo1) &&
            hormiga::parse_geo(hormiga::temper::field_value(*n, "geo2"), la2, lo2)) {
            map_cam.x = (float)((lo1 + lo2) / 2.0);
            map_cam.y = (float)((la1 + la2) / 2.0);
        } else if (hormiga::parse_geo(hormiga::temper::field_value(*n, "geo"), la1, lo1)) {
            map_cam.x = (float)lo1;
            map_cam.y = (float)la1;
        }
    } else if (glyph == "note") {
        where = "Notes";
        win_notes = true;
    } else if (glyph.rfind("allo_", 0) == 0 || glyph == "rule") {
        where = "Allomone";
        win_allomone = true;
    } else if (glyph.rfind("hol_", 0) == 0 || po.mantle == kAntfarmMantle) {
        where = "Antfarm";
        sec_open[Antfarm] = true;
    } else if (po.mantle != kDataMantle) {
        where = "Builder";
        sec_open[Builder] = true;
        cur_doc = po.mantle;
        cur_page.clear();
    } else {
        sec_open[Data] = true;
    }
    if (scene.mantle != po.mantle) dispatch_and_reproject("use " + po.mantle);
    ed.selection = {r.rune};
    ImGui::SetWindowFocus(where.c_str());
    log.push_back({"view", "redirect", r.why + " -> " + where + ": " + r.rune});
}
