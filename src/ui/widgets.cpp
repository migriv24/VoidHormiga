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
            if (t.find(':') != std::string::npos) continue; // skip icon:/color:/…
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
