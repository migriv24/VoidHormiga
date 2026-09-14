/* ui/widgets.cpp — reusable GUI pieces, owned by no section.
 *
 * The search picker and the tag picker are used by Data, the Builder, the map
 * and the rule editor, and lived in the map file because that is where the
 * first caller was. A widget every section uses belongs to none of them. */

#include "app/app_internal.hpp"
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
        std::string lbl = n.name + "  (" + n.glyph + ")##" + std::string(id);
        if (ImGui::Selectable(lbl.c_str())) {
            picked = n.name;
            buf[0] = 0;
        }
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
    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputTextWithHint(
        id, hint, buf, (int)bufsz, ImGuiInputTextFlags_EnterReturnsTrue);
    std::string typed = buf;
    std::string chosen;
    if (!typed.empty()) {
        int shown = 0;
        bool exact = false;
        for (const auto& t : vocab) {
            if (!contains_ci(t, typed.c_str())) continue;
            if (t == typed) exact = true;
            if (++shown > 8) { ImGui::TextDisabled("(keep typing...)"); break; }
            if (ImGui::Selectable(("@" + t + "##" + std::string(id)).c_str()))
                chosen = t;
        }
        if (shown == 0) ImGui::TextDisabled("no existing tag matches");
        // offer to create the typed tag when it isn't already an exact match
        if (!exact) {
            std::string mk = "+ create tag \"" + typed + "\"";
            if (ImGui::Selectable((mk + "##new" + std::string(id)).c_str()) || enter)
                chosen = typed;
        } else if (enter) {
            chosen = typed;
        }
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

    /* A name derived from the file, made unique against what is already there.
     * The content hash in the FILENAME already makes re-ingesting the same
     * picture a no-op; this is about two different pictures called `logo.png`
     * from two different folders. */
    std::string base = hormiga::detail::slug(fs::path(src).stem().string());
    if (base.empty()) base = "image";
    std::string name = base;
    for (int i = 2; scene.find(name); ++i) name = base + "-" + std::to_string(i);

    const std::string was = scene.mantle;
    dispatch_and_reproject(std::string("use ") + kDataMantle);
    if (!dispatch_and_reproject("rune new image " + name).ok) {
        if (!was.empty()) dispatch_and_reproject("use " + was);
        return rel; // the bytes are in assets/; say nothing more than that
    }
    dispatch_and_reproject("set " + name + " path " + json_arg(json_str(rel)));
    dispatch_and_reproject("tag " + name + " +type:image");
    if (!was.empty() && was != kDataMantle) dispatch_and_reproject("use " + was);
    toast("added image '" + name + "' - it is in Data now, taggable and "
          "queryable like every other image");
    return rel;
}
