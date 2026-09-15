/* ui/tags.cpp - the tag editor and the tag recommender, out of ui/data.cpp.
 *
 * Moved 2026-09-13, when the author asked for what this editor could not do:
 * *"there should still be easier ways to assign [colon tags] ... for clearance
 * specifically ... they shouldn't be so hidden in the GUI. They should have a
 * different color though. Also suggested tags should be a different color as
 * well because it gets confusing with the other tags in the same area."*
 *
 * The old editor skipped every tag containing a colon, so `clearance:public` -
 * the tag that decides whether a person reaches a public website - could be
 * neither seen nor set from the pane that shows that person. It lives here now
 * because it was never Data's alone: the Data pane, both Allomone surfaces and
 * the calendar's day tags all draw it, and data.cpp was two lines from its
 * budget. What each kind of tag means is in domain/tag_kinds.hpp.
 */
#include "app/app_internal.hpp"
#include "domain/tag_kinds.hpp"
#include "domain/bestow.hpp" // tags given by another rune

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

// ── the tag recommender ─────────────────────────────────────────────────────
// Suggest tags to add to `target`, over the tag CO-OCCURRENCE GRAPH of its
// same-glyph peers (the user's ask: "what tags do other events/scripts have?"),
// ranked by one of three modes. This is a graph-structure read — the same
// substrate Allomone styles from, run backwards (structure → suggested tags).
//   • similarity (0):    tags held by things SIMILAR to the target (reinforce)
//   • dissimilarity (1): tags from DISSIMILAR things, pivoted at the mean
//                        similarity (diverge — make the target distinct)
//   • comprehensive (2): tags that forge a NEW link to poorly-connected
//                        (stranded) things (knit the whole graph together)
// Similarity is Jaccard over MEANINGFUL tags (type:/icon:/color: excluded — the
// first is the glyph itself, the others are render directives).
std::vector<std::string> HormigaApp::compute_tag_suggestions(
    const maiz::SceneNode& target, int mode, int k) const {
    auto meaningful = [](const std::vector<std::string>& tags) {
        std::set<std::string> m;
        for (const auto& t : tags)
            if (t.rfind("type:", 0) != 0 && t.rfind("icon:", 0) != 0 &&
                t.rfind("color:", 0) != 0)
                m.insert(t);
        return m;
    };
    std::set<std::string> T = meaningful(target.tags);
    struct Cand { std::set<std::string> tags; double sim = 0; int degree = 0; };
    std::vector<Cand> cands;
    for (const auto& n : scene.nodes) {
        if (n.glyph != target.glyph || n.name == target.name) continue;
        cands.push_back({meaningful(n.tags), 0, 0});
    }
    if (cands.empty()) return {};
    auto jaccard = [](const std::set<std::string>& a,
                      const std::set<std::string>& b) -> double {
        if (a.empty() || b.empty()) return 0.0;
        int inter = 0;
        for (const auto& x : a) if (b.count(x)) ++inter;
        int uni = (int)a.size() + (int)b.size() - inter;
        return uni > 0 ? (double)inter / uni : 0.0;
    };
    double sim_sum = 0;
    for (auto& c : cands) { c.sim = jaccard(T, c.tags); sim_sum += c.sim; }
    double sim_mean = sim_sum / (double)cands.size();
    if (mode == 2) // connectivity degree, for the comprehensive mode
        for (size_t i = 0; i < cands.size(); ++i)
            for (size_t j = i + 1; j < cands.size(); ++j)
                if (jaccard(cands[i].tags, cands[j].tags) > 0) {
                    ++cands[i].degree; ++cands[j].degree;
                }
    std::map<std::string, double> score;
    std::map<std::string, int> freq;
    for (const auto& c : cands)
        for (const auto& t : c.tags) {
            if (T.count(t)) continue; // already on the target
            ++freq[t];
            double s = 0;
            if (mode == 0)      s = T.empty() ? 1.0 : c.sim;   // similarity / frequency
            else if (mode == 1) s = sim_mean - c.sim;          // dissimilarity (pivot)
            else if (c.sim == 0.0) s = 1.0 / (1.0 + c.degree); // comprehensive: new link, prefer stranded
            score[t] += s;
        }
    std::vector<std::pair<std::string, double>> ranked;
    for (auto& [t, s] : score) {
        double v = s;
        if (mode == 1 && sim_mean == 0.0) v = -(double)freq[t]; // no signal → rarest first
        ranked.push_back({t, v});
    }
    std::sort(ranked.begin(), ranked.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<std::string> out;
    for (const auto& [t, s] : ranked) {
        if ((mode == 0 || mode == 2) && s <= 0) continue; // require real signal
        out.push_back(t);
        if ((int)out.size() >= k) break;
    }
    return out;
}

/* ── WHAT CLICKING A TAG MEANS, NOW THAT TAGS ARE DIFFERENT THINGS (2026-09-15) ─
 *
 * The author:
 *   *"'widget tags' should be a thing ... things like 'color' or 'icon' ... if
 *    clicked on should bring up some sort of color options ... the 'x' should
 *    remove the tag. clicking on the text itself should bring up the widget."*
 *   *"rather than deleting a bestowed tag, you instead would be redirected to
 *    whatever rune or thing is bestowing the tag."*
 *
 * So a chip is its text and, separately, an x:
 *   - a `color:` or `icon:` tag: the text opens a colour or icon picker;
 *   - a tag another rune GIVES this one (domain/bestow.hpp): indigo, with no x,
 *     and the text redirects to the giver, because removing it here would only be
 *     given back by the next apply;
 *   - any other tag: the x removes it.
 * A giver's tag the rune does not carry yet is offered below the suggestions,
 * outlined in indigo, and a tag that exists only because something can give it
 * is searchable in the add box (ui/widgets.cpp, tag_picker). */
void HormigaApp::draw_tag_editor(const maiz::SceneNode& n,
                                 std::vector<std::string>& out) {
    using hormiga::tagkind::Kind;
    using hormiga::bestow::Bestower;
    const ImVec4 teal(0.16f, 0.43f, 0.47f, 1.0f);
    const ImVec4 amber(0.72f, 0.47f, 0.06f, 1.0f);
    const ImVec4 green(0.18f, 0.50f, 0.26f, 1.0f);
    const ImVec4 indigo(0.35f, 0.33f, 0.72f, 1.0f);
    ImGui::Spacing();
    ImGui::TextDisabled("Tags");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("grey: a tag            teal: a value on an axis (kw:, lang:)\n"
                          "amber: clearance - decides what is published\n"
                          "indigo: given by another rune - click it to go there\n"
                          "click a color: or icon: tag to change it; x removes a tag\n"
                          "green outline: a suggestion, not added yet");

    /* 1. CLEARANCE AS SWITCHES, on the two glyphs the publishing seam reads.
     * Switches rather than a typed tag, because a typo here fails silently in
     * one of two bad directions: someone private gets published, or someone who
     * agreed to be listed never appears. */
    if (n.glyph == "contact" || n.glyph == "organization") {
        ImGui::PushStyleColor(ImGuiCol_CheckMark, amber);
        for (const auto& c : hormiga::tagkind::kClearances) {
            bool on = std::find(n.tags.begin(), n.tags.end(), c.tag) != n.tags.end();
            const std::string lbl =
                std::string(ICON_FA_SHIELD_HALVED) + "  " + c.label + "##" + c.tag;
            ImGui::PushStyleColor(ImGuiCol_Text, amber);
            if (ImGui::Checkbox(lbl.c_str(), &on))
                out.push_back("tag " + n.name + (on ? " +" : " -") + std::string(c.tag));
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c.help);
            ImGui::SameLine();
        }
        ImGui::PopStyleColor();
        ImGui::NewLine();
    }

    // who gives this rune which tags - only the data mantle holds givers
    const std::vector<Bestower> givers = scene.mantle == kDataMantle
                                             ? hormiga::bestow::covering(scene, n)
                                             : std::vector<Bestower>{};
    auto giver_of = [&](const std::string& t) -> const Bestower* {
        for (const auto& g : givers)
            if (g.tag == t) return &g;
        return nullptr;
    };
    auto color_of = [](const std::string& v) -> const MarkerColor* {
        for (const auto& c : kMarkerColors)
            if (v == c.tag) return &c;
        return nullptr;
    };
    auto icon_of = [](const std::string& v) -> const MarkerIcon* {
        for (const auto& ic : kMarkerIcons)
            if (v == ic.tag) return &ic;
        return nullptr;
    };

    // chips wrap onto the next line only when the next one does not fit
    const ImGuiStyle& st = ImGui::GetStyle();
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    bool first = true;
    auto place = [&](const std::string& label) {
        if (!first) {
            const float w = ImGui::CalcTextSize(label.c_str()).x + st.FramePadding.x * 2;
            if (ImGui::GetItemRectMax().x + st.ItemSpacing.x + w < right) ImGui::SameLine();
        }
        first = false;
    };
    auto push_fill = [&](const ImVec4& bg) {
        ImGui::PushStyleColor(ImGuiCol_Button, bg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(bg.x + 0.08f, bg.y + 0.08f, bg.z + 0.08f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(bg.x - 0.05f, bg.y - 0.05f, bg.z - 0.05f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    };

    /* 2. EVERY TAG, grouped and coloured by kind. Housekeeping (`type:`) is shown
     * but not removable from this pane: every block query asking for `type:event`
     * would silently stop matching. */
    static const Kind kOrder[] = {Kind::Plain, Kind::Namespaced, Kind::Clearance,
                                  Kind::Housekeeping};
    int idx = 0;
    for (Kind want : kOrder)
        for (const auto& t : n.tags) {
            const Kind k = hormiga::tagkind::classify(t);
            if (k != want) continue;
            const std::string ns =
                k == Kind::Plain ? std::string() : std::string(hormiga::tagkind::ns_of(t));
            const std::string val =
                k == Kind::Plain ? t : std::string(hormiga::tagkind::value_of(t));
            std::string label;
            if (k == Kind::Plain || k == Kind::Housekeeping) {
                label = t;
            } else if (k == Kind::Clearance) {
                label = std::string(ICON_FA_SHIELD_HALVED) + " " + val;
            } else if (ns == "icon") {
                const MarkerIcon* ic = icon_of(val);
                label = (ic ? std::string(ic->glyph) + "  " : std::string()) + "icon: " + val;
            } else {
                label = ns + ": " + val;
            }
            ImGui::PushID(idx++);
            const Bestower* giver = k == Kind::Housekeeping ? nullptr : giver_of(t);
            if (k == Kind::Housekeeping) {
                place(label);
                ImGui::BeginDisabled();
                ImGui::SmallButton(label.c_str());
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("this rune's kind, restated as a tag - block\n"
                                      "queries use it, so it is not removed here");
            } else if (giver) {
                const std::string glabel = std::string(ICON_FA_LINK) + "  " + label;
                place(glabel);
                push_fill(indigo);
                const bool go = ImGui::SmallButton(glabel.c_str());
                ImGui::PopStyleColor(4);
                if (go)
                    redirect_to(giver->rune, kDataMantle,
                                "@" + t + " on " + n.name + " is given by " + giver->rune);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("given by %s\n\nclick to go there - change or remove it at\n"
                                      "its source; removed here it would only come back",
                                      giver->how.c_str());
            } else {
                const bool widget = ns == "color" || ns == "icon";
                place(label + "  x");
                const MarkerColor* mc = ns == "color" ? color_of(val) : nullptr;
                if (mc) push_fill(ImGui::ColorConvertU32ToFloat4(mc->col));
                else if (k == Kind::Namespaced) push_fill(teal);
                else if (k == Kind::Clearance) push_fill(amber);
                const bool pressed = ImGui::SmallButton(label.c_str());
                if (k != Kind::Plain || mc) ImGui::PopStyleColor(4);
                if (widget && ImGui::IsItemHovered())
                    ImGui::SetTooltip("click to change the %s", ns.c_str());
                if (pressed && widget) ImGui::OpenPopup("##tagwidget");
                ImGui::SameLine(0, 2);
                if (ImGui::SmallButton("x")) out.push_back("tag " + n.name + " -" + t);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", k == Kind::Clearance
                                                ? "remove this clearance - it changes what is published"
                                                : "remove tag");
                if (widget && ImGui::BeginPopup("##tagwidget")) {
                    auto swap_to = [&](const std::string& to) {
                        if (to != t) out.push_back("tag " + n.name + " -" + t + " +" + to);
                        ImGui::CloseCurrentPopup();
                    };
                    if (ns == "color") {
                        ImGui::TextDisabled("color");
                        int i = 0;
                        for (const auto& c : kMarkerColors) {
                            if (i++ % 5) ImGui::SameLine();
                            if (ImGui::ColorButton(c.tag, ImGui::ColorConvertU32ToFloat4(c.col),
                                                   ImGuiColorEditFlags_NoTooltip,
                                                   ImVec2(26, 26)))
                                swap_to(std::string("color:") + c.tag);
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c.tag);
                        }
                    } else {
                        ImGui::TextDisabled("icon");
                        int i = 0;
                        for (const auto& ic : kMarkerIcons) {
                            if (i++ % 6) ImGui::SameLine();
                            ImGui::PushID(ic.tag);
                            if (ImGui::Button(ic.glyph, ImVec2(30, 30)))
                                swap_to(std::string("icon:") + ic.tag);
                            ImGui::PopID();
                            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ic.label);
                        }
                    }
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }

    // 3. ADD: the picker offers `ns:value` tags, tags givers can give, and a new one
    std::string picked = tag_picker("##addtag", detail_tag_buf, sizeof detail_tag_buf,
                                    "+ tag...  (kw:food works too)");
    if (!picked.empty()) out.push_back("tag " + n.name + " +" + picked);

    // 4. SUGGESTIONS, outlined in green so they never read as tags already held
    std::string key = n.name + "#" + std::to_string(tag_rec_mode);
    for (const auto& t : n.tags) key += "," + t;
    if (key != tag_rec_key) {
        tag_rec_key = key;
        tag_rec_cache = compute_tag_suggestions(n, tag_rec_mode, 6);
    }
    if (!tag_rec_cache.empty()) {
        static const char* mode_name[] = {"similar", "distinct", "connective"};
        ImGui::TextColored(green, "suggested (%s) - not added yet:",
                           mode_name[tag_rec_mode % 3]);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("tag-recommendation mode is set in Settings");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, green);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(green.x, green.y, green.z, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(green.x, green.y, green.z, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(green.x, green.y, green.z, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Text, green);
        first = true;
        for (size_t i = 0; i < tag_rec_cache.size(); ++i) {
            const std::string label = "+ " + tag_rec_cache[i];
            place(label);
            ImGui::PushID(1000 + (int)i);
            if (ImGui::SmallButton(label.c_str()))
                out.push_back("tag " + n.name + " +" + tag_rec_cache[i]);
            ImGui::PopID();
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
    }

    // 5. WHAT A GIVER COVERING THIS RUNE WOULD GIVE, not carried yet
    std::vector<const Bestower*> ungiven;
    for (const auto& g : givers)
        if (std::find(n.tags.begin(), n.tags.end(), g.tag) == n.tags.end())
            ungiven.push_back(&g);
    if (!ungiven.empty()) {
        ImGui::TextColored(indigo, "could be given - this is inside a map shape:");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Border, indigo);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(indigo.x, indigo.y, indigo.z, 0.10f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(indigo.x, indigo.y, indigo.z, 0.28f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(indigo.x, indigo.y, indigo.z, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_Text, indigo);
        first = true;
        for (size_t i = 0; i < ungiven.size(); ++i) {
            const std::string label = "+ " + ungiven[i]->tag + "  (" + ungiven[i]->rune + ")";
            place(label);
            ImGui::PushID(2000 + (int)i);
            if (ImGui::SmallButton(label.c_str()))
                out.push_back("tag " + n.name + " +" + ungiven[i]->tag);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("given by %s\nadd it now, or apply the shape to everything inside",
                                  ungiven[i]->how.c_str());
            ImGui::PopID();
        }
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar();
    }
}
