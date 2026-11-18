/* ui/screens.cpp — a database's own tabs: `screen` documents drawn live inside
 * Hormiga (okf/concepts/platform/shell.md).
 *
 * A screen is built in the Builder like a website (rows, slots, spans) and drawn
 * here, in a dockable tab named by its title. Its components are data: a list
 * bound to a query, a form over the selected rune, an input and a button whose
 * actions are console lines (domain/actions.hpp). Content blocks are drawn as
 * text; a block a tab cannot draw yet says so in place rather than vanishing.
 *
 * EVERY CHANGE IS A COMMAND. A form's edit is a `set`; an action is its lines,
 * in the data mantle, plain commands as one batch (one undo) and verbs as
 * themselves. Nothing here writes the model any other way (CLAUDE.md rule 3). */
#include "app/app_internal.hpp"
#include "domain/actions.hpp"
#include "voidmaiz/mobile.hpp" // dim_wrapped

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <map>
#include <sstream>

namespace {

std::string today() {
    char b[16];
    const std::time_t t = std::time(nullptr);
    std::strftime(b, sizeof b, "%Y-%m-%d", std::localtime(&t));
    return b;
}

std::vector<std::string> csv(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s + ",") {
        if (c == ',') {
            while (!cur.empty() && cur.front() == ' ') cur.erase(0, 1);
            while (!cur.empty() && cur.back() == ' ') cur.pop_back();
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    return out;
}

int num(const maiz::SceneNode& n, const char* key, int d) {
    const std::string v = hormiga::temper::field_value(n, key);
    return v.empty() ? d : std::atoi(v.c_str());
}

} // namespace

/* Run an action: compile its lines against this screen's selection and input,
 * then queue them in the data mantle, commands as one batch, verbs as verbs. */
void HormigaApp::run_screen_action(const std::string& mantle, const std::string& action, const std::string* input) {
    hormiga::actions::Context cx;
    if (input) cx.input = *input, cx.has_input = true;
    cx.today = today();
    cx.mint = [this] { return mint_name("item"); };
    const maiz::Scene* data = scene.mantle == kDataMantle ? &scene : nullptr;
    if (!data)
        if (auto it = scenes_idle.find(kDataMantle); it != scenes_idle.end()) data = &it->second;
    /* SEVERAL SELECTED (2026-11-17): every list selects several the same way,
     * so an action that names {{selected}} runs once for each of them, all in
     * the one batch (one undo). One that does not name it runs once. */
    std::vector<const maiz::SceneNode*> targets;
    if (const auto s = shellui.selected.find(mantle); s != shellui.selected.end() && data)
        for (const auto& nm : s->second)
            if (const maiz::SceneNode* n = data->find(nm)) targets.push_back(n);
    const bool per_item = action.find("{{selected") != std::string::npos && targets.size() > 1;
    if (!per_item) cx.selected = targets.empty() ? nullptr : targets.front();
    hormiga::actions::Compiled c;
    for (std::size_t k = 0; k < (per_item ? targets.size() : 1); ++k) {
        if (per_item) cx.selected = targets[k];
        auto one = hormiga::actions::compile(action, cx);
        if (!one.ok()) {
            toast(one.error, true);
            return;
        }
        c.lines.insert(c.lines.end(), one.lines.begin(), one.lines.end());
    }
    const std::string prev = scene.mantle;
    pending_cmds.push_back(std::string("use ") + kDataMantle);
    std::vector<std::string> batch;
    auto flush = [&] {
        if (!batch.empty()) pending_cmds.push_back(maiz::compile_commit(batch));
        batch.clear();
    };
    for (const auto& line : c.lines) {
        if (hormiga::actions::is_verb_line(line)) {
            flush();
            pending_cmds.push_back(line);
        } else {
            batch.push_back(line);
        }
    }
    flush();
    if (!prev.empty() && prev != kDataMantle) pending_cmds.push_back("use " + prev);
}

void HormigaApp::draw_screens() {
    const std::vector<std::string> screens = screen_mantles();
    if (screens.empty()) return;
    // the data the screens show: the data mantle's scene, wherever it is kept right now
    const maiz::Scene* data = scene.mantle == kDataMantle ? &scene : nullptr;
    if (!data)
        if (auto it = scenes_idle.find(kDataMantle); it != scenes_idle.end()) data = &it->second;
    static maiz::Scene projected_data;
    static long long projected_gen = -1;
    if (!data) {
        if (projected_gen != (long long)model_gen) {
            maiz::ProjectOptions po;
            po.mantle = kDataMantle;
            projected_data = maiz::project_scene(core, po);
            projected_gen = (long long)model_gen;
        }
        data = &projected_data;
    }
    // each screen's own components, projected once per change
    static std::map<std::string, maiz::Scene> parts;
    static long long parts_gen = -1;
    if (parts_gen != (long long)model_gen) parts.clear(), parts_gen = (long long)model_gen;

    for (const std::string& m : screens) {
        if (!shellui.screen_open.count(m)) shellui.screen_open[m] = true; // a new screen opens
        if (!shellui.screen_open[m]) continue;
        auto pit = parts.find(m);
        if (pit == parts.end()) {
            maiz::ProjectOptions po;
            po.mantle = m;
            pit = parts.emplace(m, maiz::project_scene(core, po)).first;
        }
        const maiz::Scene& doc = pit->second;
        std::string title = m;
        std::vector<const maiz::SceneNode*> comps;
        for (const auto& n : doc.nodes) {
            if (n.glyph == "document") {
                const std::string t = hormiga::temper::field_value(n, "title");
                if (!t.empty()) title = t;
            } else if (n.glyph != "page") {
                comps.push_back(&n);
            }
        }
        std::stable_sort(comps.begin(), comps.end(), [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
            const int ra = num(*a, "row", 0), rb = num(*b, "row", 0);
            return ra != rb ? ra < rb : num(*a, "col", 0) < num(*b, "col", 0);
        });
        if (shellui.dock_id) ImGui::SetNextWindowDockID(shellui.dock_id, ImGuiCond_FirstUseEver);
        // the title can change; the id after ### cannot (a screen keeps its place in the layout)
        if (!ImGui::Begin((title + "###screen-" + m).c_str(), &shellui.screen_open[m])) {
            ImGui::End();
            continue;
        }
        std::vector<std::string>& sel = shellui.selected[m];
        const maiz::SceneNode* sel_node = sel.size() == 1 ? data->find(sel.front()) : nullptr;
        if (comps.empty())
            maiz::dim_wrapped("This screen is empty. Build it in the Builder: a list, a form, an input and a button "
                              "are under Screen in the palette.");
        size_t i = 0;
        while (i < comps.size()) { // one row at a time; slots share it by their spans
            const int row = num(*comps[i], "row", 0);
            size_t j = i;
            while (j < comps.size() && num(*comps[j], "row", 0) == row) ++j;
            const bool several = j - i > 1;
            if (several && !ImGui::BeginTable(("##row" + std::to_string(row)).c_str(), (int)(j - i))) {
                i = j;
                continue;
            }
            if (several)
                for (size_t k = i; k < j; ++k)
                    ImGui::TableSetupColumn(comps[k]->name.c_str(), ImGuiTableColumnFlags_WidthStretch,
                                            (float)std::max(1, num(*comps[k], "span", 12)));
            for (size_t k = i; k < j; ++k) {
                if (several) ImGui::TableNextColumn();
                const maiz::SceneNode& c = *comps[k];
                ImGui::PushID(c.name.c_str());
                auto f = [&](const char* key) { return hormiga::temper::field_value(c, key); };
                const std::string key = m + "/" + c.name;
                if (c.glyph == "section_header") {
                    ImGui::SeparatorText(f("title_en").c_str());
                } else if (c.glyph == "narrative") {
                    if (!f("heading_en").empty()) ImGui::TextUnformatted(f("heading_en").c_str());
                    ImGui::TextWrapped("%s", f("text_en").c_str());
                } else if (c.glyph == "quote") {
                    ImGui::TextWrapped("\"%s\"  %s", f("text_en").c_str(), f("author").c_str());
                } else if (c.glyph == "stat") {
                    ImGui::Text("%s  %s", f("number").c_str(), f("label_en").c_str());
                } else if (c.glyph == "divider") {
                    ImGui::Separator();
                } else if (c.glyph == "ui_list") {
                    if (!f("title_en").empty()) ImGui::SeparatorText(f("title_en").c_str());
                    const std::string q = f("query");
                    const auto cols = csv(f("columns"));
                    const int limit = num(c, "limit", 0);
                    std::vector<const maiz::SceneNode*> rows; // the query over what is loaded (the frontier's caveat)
                    for (const auto& n : data->nodes) {
                        if (q.empty() ? false : !search_match(n, q)) continue;
                        if (q.empty() && n.glyph == "mapshape") continue;
                        rows.push_back(&n);
                        if (limit > 0 && (int)rows.size() >= limit) break;
                    }
                    const int ncol = 1 + (int)cols.size();
                    // a screen's list selects as every list does (okf/concepts/platform/conventions.md)
                    maiz::ListSelect ls;
                    ls.begin(sel, (long)rows.size(), [&rows](long k) { return rows[(size_t)k]->name; },
                             {.scope_rect = true});
                    if (ImGui::BeginTable("##list", ncol,
                                          ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
                                              ImGuiTableFlags_Resizable,
                                          ImVec2(0, std::min(320.0f, ImGui::GetTextLineHeightWithSpacing() * (rows.size() + 2.0f))))) {
                        ImGui::TableSetupScrollFreeze(0, 1);
                        ImGui::TableSetupColumn("name");
                        for (const auto& col : cols) ImGui::TableSetupColumn(col.c_str());
                        ImGui::TableHeadersRow();
                        ImGuiListClipper clip; // drawn O(visible)
                        clip.Begin((int)rows.size());
                        while (clip.Step())
                            for (int r = clip.DisplayStart; r < clip.DisplayEnd; ++r) {
                                const maiz::SceneNode& n = *rows[(size_t)r];
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Selectable((rune_title(n) + "##" + n.name).c_str(), ls.item(r, n.name),
                                                  ImGuiSelectableFlags_SpanAllColumns); // forms and actions read it
                                for (const auto& col : cols) {
                                    ImGui::TableNextColumn();
                                    ImGui::TextUnformatted(hormiga::temper::field_value(n, col.c_str()).c_str());
                                }
                            }
                        ImGui::EndTable();
                    }
                    ls.end();
                    ImGui::TextDisabled(sel.size() > 1 ? "%d  (%d selected)" : "%d", (int)rows.size(), (int)sel.size());
                } else if (c.glyph == "ui_form") {
                    if (!f("title_en").empty()) ImGui::SeparatorText(f("title_en").c_str());
                    if (sel.size() > 1) {
                        ImGui::TextDisabled("%d selected: the form shows one at a time; a button acts on all of them",
                                            (int)sel.size());
                    } else if (!sel_node) {
                        ImGui::TextDisabled("select something in a list on this screen");
                    } else {
                        ImGui::TextDisabled("%s", rune_title(*sel_node).c_str());
                        std::vector<std::string> keys = csv(f("fields"));
                        if (keys.empty())
                            for (const auto& fl : sel_node->fields) keys.push_back(fl.key);
                        for (const auto& fk : keys) {
                            const std::string stored = hormiga::temper::field_value(*sel_node, fk.c_str());
                            std::string& b = shellui.buffers[key + "/" + sel_node->name + "/" + fk];
                            // follow the model, except while this very field is being typed in
                            if (ImGui::GetActiveID() != ImGui::GetID(fk.c_str())) b = stored;
                            char buf[512];
                            std::snprintf(buf, sizeof buf, "%s", b.c_str());
                            ImGui::InputText(fk.c_str(), buf, sizeof buf);
                            b = buf;
                            if (ImGui::IsItemDeactivatedAfterEdit() && b != stored) {
                                pending_cmds.push_back(std::string("use ") + kDataMantle);
                                pending_cmds.push_back("set " + sel_node->name + " " + fk + " " + text_arg(buf));
                                if (scene.mantle != kDataMantle && !scene.mantle.empty())
                                    pending_cmds.push_back("use " + scene.mantle);
                            }
                        }
                    }
                } else if (c.glyph == "ui_input") {
                    std::string& b = shellui.buffers[key];
                    char buf[512];
                    std::snprintf(buf, sizeof buf, "%s", b.c_str());
                    const std::string label = f("label_en").empty() ? std::string("input") : f("label_en");
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    const bool enter = ImGui::InputTextWithHint(("##" + c.name).c_str(),
                                                                (label + (f("placeholder").empty() ? "" : ": " + f("placeholder"))).c_str(),
                                                                buf, sizeof buf, ImGuiInputTextFlags_EnterReturnsTrue);
                    b = buf;
                    if (ImGui::IsItemHovered() && !f("action").empty()) ImGui::SetTooltip("On Enter:\n%s", f("action").c_str());
                    if (enter) {
                        const std::string typed = b;
                        b.clear();
                        run_screen_action(m, f("action"), &typed);
                        ImGui::SetKeyboardFocusHere(-1); // ready for the next scan
                    }
                } else if (c.glyph == "ui_button") {
                    const std::string label = f("label_en").empty() ? std::string("Run") : f("label_en");
                    if (ImGui::Button(label.c_str())) run_screen_action(m, f("action"), nullptr);
                    if (ImGui::IsItemHovered()) // what it will do, before it does it
                        ImGui::SetTooltip("%s", f("action").empty() ? "(no action yet)" : f("action").c_str());
                } else {
                    ImGui::TextDisabled("(%s: not shown in a tab yet)", c.glyph.c_str());
                }
                ImGui::PopID();
            }
            if (several) ImGui::EndTable();
            i = j;
        }
        ImGui::End();
    }
}
