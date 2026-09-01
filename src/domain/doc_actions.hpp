/* doc_actions.hpp — the Builder's interaction vocabulary as NAMED actions
 * (okf/concepts/sections/builder.md; the Q18 elevation: "every builder gesture must
 * be a `doc` verb the way every map gesture is a `map` verb").
 *
 * Same discipline as map_actions.hpp: each ActionDescriptor's `compile` is
 * THE one definition — today the command bar's `doc <action> …` front-end
 * calls it; when the document canvas lands (B3), its drag/drop/resize
 * gestures call the SAME compile. One transcript entry either way.
 *
 * The grid convention (QA, decided): rows stack vertically; a row is 12
 * units wide; a component occupies (row, col, span). Stored fine-grained,
 * edited as slots. `doc migrate` converts a legacy adjacency chain to rows
 * non-destructively (wires kept until the old canvas retires).
 */
#pragma once

#include "voidmaiz/action.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace hormiga {

/* A component's grid placement (missing row = unplaced/legacy). */
inline int doc_field_int(const maiz::SceneNode& n, const char* key, int def) {
    for (const auto& f : n.fields)
        if (f.key == key) {
            std::string v = f.value_json;
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                v = v.substr(1, v.size() - 2);
            if (v.empty() || v == "null") return def;
            return std::atoi(v.c_str());
        }
    return def;
}

/* The legacy adjacency-chain order (shared shape with app.cpp's block_chain;
 * duplicated here so the registry stays headless — scene in, commands out). */
inline std::vector<const maiz::SceneNode*> doc_chain(const maiz::Scene& issue) {
    std::map<std::string, std::string> next;
    std::map<std::string, bool> has_prev;
    for (const auto& w : issue.wires)
        if (w.style == maiz::SceneWire::Style::Adjacency) {
            next[w.from] = w.to;
            has_prev[w.to] = true;
        }
    std::vector<const maiz::SceneNode*> chain;
    for (const auto& n : issue.nodes)
        if (next.count(n.name) && !has_prev[n.name])
            for (const maiz::SceneNode* c = &n; c;) {
                chain.push_back(c);
                auto it = next.find(c->name);
                c = (it != next.end()) ? issue.find(it->second) : nullptr;
            }
    for (const auto& n : issue.nodes)
        if (std::find(chain.begin(), chain.end(), &n) == chain.end())
            chain.push_back(&n);
    return chain;
}

/* Grid-order walk: (row, col) ascending when any component carries a row;
 * the adjacency chain otherwise. Renderers call THIS — migration flips the
 * order source without touching a renderer. `document` runes never render. */
inline std::vector<const maiz::SceneNode*> doc_order(const maiz::Scene& issue) {
    bool any_row = false;
    for (const auto& n : issue.nodes)
        if (doc_field_int(n, "row", -1) >= 0) { any_row = true; break; }
    std::vector<const maiz::SceneNode*> out;
    for (const auto* n : doc_chain(issue))
        if (n->glyph != "document") out.push_back(n);
    if (!any_row) return out;
    std::stable_sort(out.begin(), out.end(),
                     [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                         int ra = doc_field_int(*a, "row", 1 << 20);
                         int rb = doc_field_int(*b, "row", 1 << 20);
                         if (ra != rb) return ra < rb;
                         return doc_field_int(*a, "col", 0) <
                                doc_field_int(*b, "col", 0);
                     });
    return out;
}

inline maiz::ActionRegistry make_doc_actions() {
    maiz::ActionRegistry reg;

    // doc place <glyph> <name> <row> [col] [span] — mint a component on the grid
    reg.add(maiz::ActionDescriptor{
        "place", "Place a component",
        "Create a new <glyph> component named <name> at grid <row> (top=0), "
        "optional <col> 0-11 and <span> 1-12 (default full width).",
        {{"glyph", "glyph", true, "component kind: hero|narrative|event_grid|…"},
         {"name", "text", true, "command-safe rune name (slug)"},
         {"row", "int", true, "vertical row, 0 = top"},
         {"col", "int", false, "start column 0-11 (default 0)"},
         {"span", "int", false, "width in units 1-12 (default 12)"}},
        "drop", // the document canvas binds palette-drop to THIS action
        [](const maiz::Scene& scene, const maiz::ActionArgs& args) {
            std::vector<std::string> out;
            auto g = args.find("glyph"), n = args.find("name"), r = args.find("row");
            if (g == args.end() || n == args.end() || r == args.end()) return out;
            if (scene.find(n->second)) return out; // name taken: decline
            auto cv = args.find("col");
            auto sv = args.find("span");
            int col = cv == args.end() ? 0 : std::atoi(cv->second.c_str());
            int span = sv == args.end() ? 12 : std::atoi(sv->second.c_str());
            col = std::clamp(col, 0, 11);
            span = std::clamp(span, 1, 12 - col);
            out.push_back("rune new " + g->second + " " + n->second);
            out.push_back("set " + n->second + " row \"" + r->second + "\"");
            out.push_back("set " + n->second + " col \"" + std::to_string(col) +
                          "\"");
            out.push_back("set " + n->second + " span \"" +
                          std::to_string(span) + "\"");
            return out;
        }});

    // doc move <name> <row> [col] — reposition an existing component
    reg.add(maiz::ActionDescriptor{
        "move", "Move a component",
        "Move an existing component to <row>, optional <col>.",
        {{"name", "node", true, "an existing component's name"},
         {"row", "int", true, "vertical row, 0 = top"},
         {"col", "int", false, "start column 0-11 (keep current if absent)"}},
        "drag",
        [](const maiz::Scene& scene, const maiz::ActionArgs& args) {
            std::vector<std::string> out;
            auto n = args.find("name"), r = args.find("row");
            if (n == args.end() || r == args.end()) return out;
            if (!scene.find(n->second)) return out;
            out.push_back("set " + n->second + " row \"" + r->second + "\"");
            if (auto c = args.find("col"); c != args.end())
                out.push_back("set " + n->second + " col \"" +
                              std::to_string(std::clamp(
                                  std::atoi(c->second.c_str()), 0, 11)) +
                              "\"");
            return out;
        }});

    // doc resize <name> <span> — change a component's width
    reg.add(maiz::ActionDescriptor{
        "resize", "Resize a component",
        "Set an existing component's width to <span> units (1-12).",
        {{"name", "node", true, "an existing component's name"},
         {"span", "int", true, "width in units 1-12"}},
        "handle-drag",
        [](const maiz::Scene& scene, const maiz::ActionArgs& args) {
            std::vector<std::string> out;
            auto n = args.find("name"), s = args.find("span");
            if (n == args.end() || s == args.end()) return out;
            const maiz::SceneNode* node = scene.find(n->second);
            if (!node) return out;
            int col = doc_field_int(*node, "col", 0);
            int span = std::clamp(std::atoi(s->second.c_str()), 1, 12 - col);
            out.push_back("set " + n->second + " span \"" +
                          std::to_string(span) + "\"");
            return out;
        }});

    // doc remove <name> — take a component off the document (undoable rm)
    reg.add(maiz::ActionDescriptor{
        "remove", "Remove a component",
        "Delete a component from the document (one undo restores it).",
        {{"name", "node", true, "an existing component's name"}},
        "delete",
        [](const maiz::Scene& scene, const maiz::ActionArgs& args) {
            std::vector<std::string> out;
            auto n = args.find("name");
            if (n == args.end() || !scene.find(n->second)) return out;
            out.push_back("rm " + n->second);
            return out;
        }});

    // doc migrate — legacy adjacency chain → grid rows (non-destructive:
    // wires stay; each block becomes one full-width row in chain order)
    reg.add(maiz::ActionDescriptor{
        "migrate", "Migrate chain to grid",
        "Assign grid rows to every legacy component in adjacency-chain order "
        "(one full-width row each). Wires are kept; re-running is a no-op.",
        {},
        "",
        [](const maiz::Scene& scene, const maiz::ActionArgs&) {
            std::vector<std::string> out;
            int row = 0;
            for (const auto* n : doc_chain(scene)) {
                if (n->glyph == "document") continue;
                if (doc_field_int(*n, "row", -1) < 0) { // only the unplaced
                    out.push_back("set " + n->name + " row \"" +
                                  std::to_string(row) + "\"");
                    out.push_back("set " + n->name + " col \"0\"");
                    out.push_back("set " + n->name + " span \"12\"");
                }
                ++row;
            }
            return out;
        }});

    return reg;
}

} // namespace hormiga
