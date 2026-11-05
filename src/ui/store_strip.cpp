/* ui/store_strip.cpp — the Data tab when the database is larger than this
 * computer holds at once (stage 5, app/working_store.hpp).
 *
 * The author, 2026-11-03: "we should always be able to open any database". It
 * opens, and Core holds a part: the small mantles whole and the large ones by
 * cluster. This strip says that plainly, and makes every rune reachable:
 *  - the PARTS: the data mantle's cluster leaves, each loadable, so a person can
 *    walk the whole database a part at a time;
 *  - a SEARCH of the whole database, answered by the store without loading it
 *    (a rune's name, or a tag expression: `type:event AND city:austin`), each
 *    hit loadable with the part around it.
 * A load is a reload, never an edit: unsaved work is written back first. */
#include "app/app_internal.hpp"

#include <cctype>

namespace {

/* A rune's name, typed: one word with no tag syntax in it. */
bool looks_like_name(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s)
        if (std::isspace((unsigned char)c) || c == ':' || c == '(' || c == ')') return false;
    return true;
}

std::string with_commas(long long n) {
    std::string d = std::to_string(n), out;
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (i && (d.size() - i) % 3 == 0) out += ',';
        out += d[i];
    }
    return out;
}

} // namespace

void HormigaApp::draw_store_strip() {
    if (!wstore.partial() || !wstore.store()) return;
    rancho::Store& st = *wstore.store();
    const auto& plan = wstore.plan();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.20f, 0.24f, 0.30f, 0.55f));
    ImGui::BeginChild("##storestrip", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 2.2f), true,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::TextWrapped("This database is larger than this computer holds at once. Showing %s of its %s runes%s%s.",
                       with_commas(plan.runes).c_str(), with_commas(wstore.total_runes()).c_str(),
                       plan.focus.empty() ? "" : ": the part around ", plan.focus.c_str());

    if (ImGui::SmallButton("Other parts...")) {
        store_leaves = st.leaves(kDataMantle);
        ImGui::OpenPopup("##storeparts");
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("the database's parts, found from its own links: load any of them");
    ImGui::SameLine(0, 16);
    ImGui::SetNextItemWidth(260);
    const bool go = ImGui::InputTextWithHint("##storefind", "find in the whole database: a name, or type:event",
                                             store_find, sizeof store_find, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::SmallButton("Find") || go) {
        const std::string q = store_find;
        store_found.clear();
        if (looks_like_name(q)) { // a name first: the store knows every rune's, loaded or not
            const long long m = st.mantle_row(kDataMantle);
            if (m >= 0 && st.live_rune_id(m, q)) store_found.push_back(q);
        }
        if (store_found.empty() && !q.empty()) store_found = st.select(kDataMantle, q, 200);
        const long long all = store_found.size() == 200 ? st.count(kDataMantle, q) : (long long)store_found.size();
        store_find_said = all == 0 ? "nothing in the whole database matches"
                                   : with_commas(all) + " in the whole database" +
                                         (all > (long long)store_found.size() ? ", the first 200 listed" : "");
        ImGui::OpenPopup("##storefound");
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (ImGui::BeginPopup("##storeparts")) {
        ImGui::TextUnformatted("Parts of the data, found from its links. Loading one holds it and its neighbours.");
        ImGui::Separator();
        ImGui::BeginChild("##partslist", ImVec2(420, 320));
        ImGuiListClipper clip;
        clip.Begin((int)store_leaves.size());
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                const auto& l = store_leaves[(size_t)i];
                ImGui::PushID(i);
                const bool here = l.path == plan.focus;
                if (ImGui::Selectable((l.path + "  (" + with_commas(l.runes) + " runes)" + (here ? "  - shown" : ""))
                                          .c_str(),
                                      here, ImGuiSelectableFlags_AllowDoubleClick) &&
                    !here) {
                    load_part(kDataMantle, l.path);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("##storefound")) {
        ImGui::TextUnformatted(store_find_said.c_str());
        if (!store_found.empty()) ImGui::Separator();
        ImGui::BeginChild("##foundlist", ImVec2(420, store_found.empty() ? 1.0f : 320.0f));
        ImGuiListClipper clip;
        clip.Begin((int)store_found.size());
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                const std::string& name = store_found[(size_t)i];
                ImGui::PushID(i);
                const bool held = scene.find(name) != nullptr;
                if (ImGui::Selectable((name + (held ? "  - loaded" : "")).c_str())) {
                    if (held) ed.selection = {name};
                    else load_part(kDataMantle, "", name);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}
