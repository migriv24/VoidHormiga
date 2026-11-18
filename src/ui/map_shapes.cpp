/* ui/map_shapes.cpp — the map's shape tools that came with polygons (2026-11-14):
 * the polygon tool, a polygon's corner handles, *Generate perimeter*, and the menu
 * for several selected things. Split out of ui/map.cpp, whose draw_map_section
 * builds a `MapFrame` (ui/map_shapes.hpp) and calls these at the same points in
 * the frame. Design: okf/concepts/sections/gis/territory.md, "Polygons and
 * perimeters"; the ground they will stand on: okf/concepts/sections/gis/ground.md. */
#include "app/app_internal.hpp"
#include "ui/map_shapes.hpp"
#include "domain/generated.hpp" // the generators, their seeds and recipes

#include <algorithm>
#include <cmath>
#include <cstdio>

/* THE POLYGON TOOL (2026-11-14; Q102). Click to put a corner down; click the
 * first corner, double-click, or press Enter to close it (three corners at
 * least); Backspace takes the last corner back; Esc drops it. Corners land on
 * the grid when snap is on. Corners by direct click, never by a crosshair
 * (gis/research.md: twice as fast). */
void HormigaApp::map_polygon_tool(const MapFrame& f) {
    ImDrawList* dl = f.dl;
    const ImVec2 mouse = f.mouse;
    const bool hovered = f.hovered, left_press = f.left_press;
    const auto& to_screen = f.to_screen;
    const auto& to_geo = f.to_geo;
    const auto& snap_screen = f.snap;
    const ImVec2 sm = snap_screen(mouse);
    std::vector<ImVec2> P;
    for (const auto& c : map_poly_draft) P.push_back(to_screen(c.lat, c.lon));
    const bool closable = P.size() >= 3;
    const bool near_first =
        closable && hovered && std::fabs(mouse.x - P[0].x) <= 9 && std::fabs(mouse.y - P[0].y) <= 9;
    bool finish = false;
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        map_poly_draft.clear();
        map_draw_shape = 0;
    } else if (hovered && ImGui::IsKeyPressed(ImGuiKey_Backspace) && !map_poly_draft.empty()) {
        map_poly_draft.pop_back();
    } else if (hovered && closable && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))) {
        finish = true;
    } else if (hovered && closable && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        finish = true; // the double-click's first click already put the last corner down
    } else if (left_press && hovered) {
        if (near_first) finish = true;
        else {
            hormiga::gis::Coord c;
            to_geo(sm, c.lat, c.lon);
            map_poly_draft.push_back(c);
        }
    }
    // the preview: what is down, the edge to the pointer, and the first corner to close on
    const ImU32 pc = IM_COL32(46, 107, 79, 230);
    if (!P.empty()) {
        dl->AddPolyline(P.data(), (int)P.size(), pc, 0, 2.0f);
        dl->AddLine(P.back(), near_first ? P[0] : sm, IM_COL32(46, 107, 79, 150), 1.5f);
        for (size_t i = 0; i < P.size(); ++i) dl->AddCircleFilled(P[i], i == 0 && near_first ? 7.0f : 4.0f, pc);
        if (closable) dl->AddCircle(P[0], 9.0f, IM_COL32(46, 107, 79, near_first ? 255 : 120), 0, 1.5f);
    }
    dl->AddCircle(sm, 3.0f, pc, 0, 1.5f);
    const char* tip = P.size() < 3 ? "click to put a corner down  (Backspace: undo, Esc: cancel)"
                                   : "click the first corner, double-click or Enter to finish";
    dl->AddText(ImVec2(mouse.x + 14, mouse.y + 10), IM_COL32(40, 40, 40, 230), tip);
    if (finish && map_poly_draft.size() >= 3) {
        std::string name;
        for (int i = 1;; ++i) {
            name = "shape-" + std::to_string(i);
            if (!scene.find(name)) break;
        }
        std::vector<std::string> cmds = {"rune new mapshape " + name, "set " + name + " kind \"polygon\""};
        if (map_new_perimeter) cmds.push_back("set " + name + " style \"perimeter\"");
        for (auto& c : hormiga::shape::vertex_commands(name, map_poly_draft)) cmds.push_back(std::move(c));
        cmds.push_back("set " + name + " canvas " + text_arg(map_canvas));
        cmds.push_back("tag " + name + " +type:mapshape" +
                       (hormiga::canvas::join_layer(scene, map_layers(), map_sel, name).empty() ? std::string() : " +layer:" + map_sel));
        pending_cmds.push_back(maiz::compile_commit(cmds));
        ed.selection = {name};
        begin_naming(name, "mapshape");
        map_poly_draft.clear();
        map_draw_shape = 0; // one shape per arm, as the box tools
    }
}

/* A selected polygon's handles; true when this frame's right-click took a corner
 * away (so the shape's menu does not open for the same click). */
bool HormigaApp::map_polygon_handles(const MapFrame& f, const maiz::SceneNode& sel, const std::vector<ImVec2>& sel_ring,
                                     ImVec2 sel_mn, ImVec2 sel_mx) {
    ImDrawList* dl = f.dl;
    const ImVec2 mouse = f.mouse;
    const bool hovered = f.hovered, left_press = f.left_press, poly_editing = f.poly_editing;
    const maiz::SceneNode* hit = f.marker_hit;
    const auto& to_geo = f.to_geo;
    const maiz::SceneNode* sel_shape = &sel;
    bool vertex_removed = false;
    const std::vector<ImVec2>& R = sel_ring;
    const size_t n = R.size();
    const bool open = hormiga::shape::is_line(sel); // a path: no edge from its last corner back to its first
    const size_t edges = open ? n - 1 : n, keep = open ? 2 : 3;
    auto mid = [&](size_t i) { return ImVec2((R[i].x + R[(i + 1) % n].x) * 0.5f, (R[i].y + R[(i + 1) % n].y) * 0.5f); };
    /* HANDLES AT THE SCREEN'S RESOLUTION (2026-11-17). An imported boundary has a thousand-odd
     * corners; zoomed out, every handle was drawn, a solid band along its
     * border. A corner shows (and can be taken) when it stands 10 px from the last
     * one shown; an edge's "add a corner" handle when the edge is 24 px long. Zoom
     * in and every corner is there. */
    std::vector<char> shown(n, 0);
    for (size_t i = 0, last = 0; i < n; ++i)
        if (i == 0 || std::fabs(R[i].x - R[last].x) + std::fabs(R[i].y - R[last].y) >= 10.0f) shown[i] = 1, last = i;
    auto long_edge = [&](size_t i) {
        const ImVec2 a = R[i], b = R[(i + 1) % n];
        return std::fabs(a.x - b.x) + std::fabs(a.y - b.y) >= 24.0f;
    };
    int vhit = -1, mhit = -1;
    for (size_t i = 0; i < n; ++i)
        if (shown[i] && hovered && std::fabs(mouse.x - R[i].x) <= 7 && std::fabs(mouse.y - R[i].y) <= 7) vhit = (int)i;
    if (vhit < 0)
        for (size_t i = 0; i < edges; ++i)
            if (long_edge(i) && hovered && std::fabs(mouse.x - mid(i).x) <= 6 && std::fabs(mouse.y - mid(i).y) <= 6)
                mhit = (int)i;
    for (size_t i = 0; i < edges; ++i) { // the ghost handles first, so a corner is drawn over its edge's
        if (!long_edge(i)) continue;
        const bool hot = (int)i == mhit;
        const float r = hot ? 5.0f : 3.5f;
        dl->AddCircleFilled(mid(i), r, IM_COL32(255, 255, 255, hot ? 240 : 160));
        dl->AddCircle(mid(i), r, IM_COL32(40, 70, 60, hot ? 255 : 150), 0, 1.2f);
    }
    for (size_t i = 0; i < n; ++i) {
        const bool hot = (int)i == vhit || (poly_editing && (int)i == map_poly_idx);
        if (!shown[i] && !hot) continue;
        const float h = hot ? 6.0f : 4.5f;
        dl->AddRectFilled(ImVec2(R[i].x - h, R[i].y - h), ImVec2(R[i].x + h, R[i].y + h), IM_COL32(255, 255, 255, 245), 2.0f);
        dl->AddRect(ImVec2(R[i].x - h, R[i].y - h), ImVec2(R[i].x + h, R[i].y + h), IM_COL32(40, 70, 60, 255), 2.0f, 0, 1.5f);
    }
    const bool on_it = hovered && map_shape_hit(*sel_shape, R, sel_mn, sel_mx, mouse, 6.0f) && !hit;
    if (vhit >= 0 || mhit >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    else if (on_it || (poly_editing && map_poly_idx < 0)) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    if (left_press && (vhit >= 0 || mhit >= 0 || on_it) && !ImGui::GetIO().KeyShift && !ImGui::GetIO().KeyCtrl) {
        map_poly_edit = sel_shape->name;
        map_poly_start = R;
        map_poly_idx = vhit;
        if (mhit >= 0) { // a new corner, born where the edge's handle was
            map_poly_start.insert(map_poly_start.begin() + mhit + 1, mid((size_t)mhit));
            map_poly_idx = mhit + 1;
        }
        map_poly_live = map_poly_start;
        map_poly_grab = mouse;
    }
    if (vhit >= 0 && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        vertex_removed = true; // and no shape menu for this click
        if (n > keep) {
            std::vector<hormiga::gis::Coord> v;
            for (size_t i = 0; i < n; ++i)
                if ((int)i != vhit) {
                    hormiga::gis::Coord c;
                    to_geo(R[i], c.lat, c.lon);
                    v.push_back(c);
                }
            pending_cmds.push_back(maiz::compile_commit(
                hormiga::shape::vertex_commands(sel_shape->name, v, open, hormiga::shape::extra_rings(*sel_shape))));
        } else {
            toast(open ? "a path keeps at least two points" : "a polygon keeps at least three corners");
        }
    }
    return vertex_removed;
}

/* The corners in hand, let go: one batch of the vertices and their box. True
 * when a polygon edit ended this frame. */
bool HormigaApp::map_polygon_release(const MapFrame& f) {
    if (map_poly_edit.empty() || ImGui::IsMouseDown(ImGuiMouseButton_Left)) return false;
    const ImVec2 mouse = f.mouse;
    const auto& to_geo = f.to_geo;
    bool moved = map_poly_live.size() != map_poly_start.size() ||
                 std::fabs(mouse.x - map_poly_grab.x) + std::fabs(mouse.y - map_poly_grab.y) > 2.0f;
    const maiz::SceneNode* edited = scene.find(map_poly_edit);
    const bool open = edited && hormiga::shape::is_line(*edited);
    if (moved && map_poly_live.size() >= (open ? 2u : 3u)) {
        std::vector<hormiga::gis::Coord> v;
        for (const auto& p : map_poly_live) {
            hormiga::gis::Coord c;
            to_geo(p, c.lat, c.lon);
            v.push_back(c);
        }
        /* Its further rings (holes, parts) go where the outline went when the
         * whole shape was dragged, by the same move on screen; a corner moved
         * leaves them where they are. */
        auto rings = edited ? hormiga::shape::extra_rings(*edited) : std::vector<std::vector<hormiga::gis::Coord>>{};
        if (map_poly_idx < 0 && !rings.empty() && map_poly_live.size() == map_poly_start.size()) {
            const ImVec2 d(map_poly_live[0].x - map_poly_start[0].x, map_poly_live[0].y - map_poly_start[0].y);
            for (auto& r : rings)
                for (auto& c : r) {
                    const ImVec2 s = f.to_screen(c.lat, c.lon);
                    to_geo(ImVec2(s.x + d.x, s.y + d.y), c.lat, c.lon);
                }
        }
        pending_cmds.push_back(maiz::compile_commit(hormiga::shape::vertex_commands(map_poly_edit, v, open, rings)));
    }
    map_poly_edit.clear();
    map_poly_idx = -1;
    map_poly_start.clear();
    map_poly_live.clear();
    return true;
}

/* GENERATING (2026-11-14; okf gis/generation.md). One door for every generator:
 * the selection, in the order it was made, becomes seeds; the ground is the
 * canvas's when snap is on (a plan's grid; Earth's streets once loaded) and free
 * space when it is off; the result is a shape with its recipe (its generator, its
 * ground, a link to each seed). A perimeter is kept 14 px clear of what it goes
 * round at the zoom it was made at. Work the ground expects to be long goes
 * behind the busy curtain's bar (the author: "always remember to use loading
 * bars"); free space and a small grid take microseconds and show none. */
namespace {
constexpr double kBarAfter = 200000; // units of a ground's work (grid cells) before the bar is worth showing
}

static hormiga::generated::PlaceOf map_place_of(const MapFrame& f) {
    return [&f](const maiz::SceneNode& n, hormiga::gis::XY& xy) {
        ImVec2 at;
        if (!f.where_drawn(&n, at)) return false; // where it is DRAWN: a fanned child of a reference point
        double la, lo;
        f.to_geo(at, la, lo);
        xy = {lo, la};
        return true;
    };
}

static void map_margin(const MapFrame& f, double& mx, double& my) {
    double la0, lo0, la1, lo1;
    f.to_geo(f.ctr, la0, lo0);
    f.to_geo(ImVec2(f.ctr.x + 14, f.ctr.y + 14), la1, lo1);
    mx = std::fabs(lo1 - lo0), my = std::fabs(la1 - la0);
}

void HormigaApp::map_generate(const MapFrame& f, const std::string& which, const std::vector<std::string>& names) {
    hormiga::generated::Request rq;
    rq.which = which;
    rq.seeds = names;
    rq.canvas = map_canvas;
    rq.use_ground = map_snap;
    map_margin(f, rq.margin_x, rq.margin_y);
    for (int i = 1;; ++i) {
        rq.name = (which == "perimeter" ? "perimeter-" : "path-") + std::to_string(i);
        if (!scene.find(rq.name)) break;
    }
    if (!hormiga::canvas::join_layer(scene, map_layers(), map_sel, rq.name).empty()) rq.layer = map_sel;
    auto prep = std::make_shared<hormiga::generated::Prepared>(hormiga::generated::prepare(scene, rq, map_place_of(f)));
    if (!prep->error.empty()) {
        toast(prep->error, true);
        return;
    }
    auto land = [this](const hormiga::generated::Result& r, const std::string& name) {
        if (!r.error.empty()) {
            toast(r.error, true);
            return;
        }
        pending_cmds.push_back(maiz::compile_commit(r.commands));
        ed.selection = {name};
        begin_naming(name, "mapshape", r.label); // named as it is made, like any drawn shape
    };
    if (prep->cost < kBarAfter) {
        land(hormiga::generated::finish(*prep, hormiga::generated::make(*prep)), rq.name);
        return;
    }
    const std::string name = rq.name;
    run_busy_async(which == "perimeter" ? "Generating a perimeter" : "Generating a path",
                   [prep, land, name](const hormiga::BusyReport& report) -> hormiga::BusyFinish {
                       auto made = std::make_shared<hormiga::gis::Made>(hormiga::generated::make(*prep, [&](double fr) {
                           report(fr, "following the ground");
                           return true;
                       }));
                       return [prep, made, land, name] { land(hormiga::generated::finish(*prep, *made), name); };
                   });
}

void HormigaApp::map_regenerate(const MapFrame& f, const maiz::SceneNode& shape) {
    double mx, my;
    map_margin(f, mx, my);
    const auto r = hormiga::generated::regenerate(scene, shape, map_place_of(f), mx, my);
    if (!r.error.empty()) toast(r.error, true);
    else pending_cmds.push_back(maiz::compile_commit(r.commands));
}

void HormigaApp::map_set_style(const std::vector<std::string>& names, bool perimeter) {
    std::vector<std::string> cmds;
    for (const auto& nm : names)
        if (const maiz::SceneNode* n = scene.find(nm); n && n->glyph == "mapshape" && hormiga::shape::is_perimeter(*n) != perimeter)
            cmds.push_back("set " + nm + " style \"" + std::string(perimeter ? "perimeter" : "area") + "\"");
    if (!cmds.empty()) pending_cmds.push_back(maiz::compile_commit(cmds));
}

/* Several things selected and right-clicked: act on all of them (2026-11-14). */
void HormigaApp::map_multi_menu(const MapFrame& f) {
    if (!ImGui::BeginPopup("map-multi-menu")) return;
    std::vector<std::string> shapes, others;
    for (const auto& nm : ed.selection)
        if (const maiz::SceneNode* n = scene.find(nm)) (n->glyph == "mapshape" ? shapes : others).push_back(nm);
    ImGui::TextDisabled("%d selected: %d shape(s), %d other", (int)ed.selection.size(), (int)shapes.size(),
                        (int)others.size());
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_FA_DRAW_POLYGON "  Generate perimeter around them")) map_generate(f, "perimeter", ed.selection);
    if (ImGui::MenuItem(ICON_FA_ROUTE "  Generate path through them")) map_generate(f, "path", ed.selection);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("through each, in the order they were selected%s",
                          map_snap ? "\n(snap is on: along the ground)" : "\n(straight between them; turn snap on to follow the ground)");
    if (!shapes.empty()) {
        if (ImGui::MenuItem("Draw the shapes as perimeters")) map_set_style(shapes, true);
        if (ImGui::MenuItem("Draw the shapes as areas")) map_set_style(shapes, false);
    }
    if (ImGui::BeginMenu("Color all")) {
        for (const auto& c : kMarkerColors) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(c.col));
            const bool pick = ImGui::MenuItem(c.tag);
            ImGui::PopStyleColor();
            if (pick) {
                std::vector<std::string> cmds;
                for (const auto& nm : ed.selection)
                    if (const maiz::SceneNode* n = scene.find(nm)) {
                        const std::string cur = tag_value(*n, "color");
                        cmds.push_back("tag " + nm + (cur.empty() ? std::string() : " -color:" + cur) + " +color:" + c.tag);
                    }
                pending_cmds.push_back(maiz::compile_commit(cmds));
            }
        }
        ImGui::EndMenu();
    }
    if (!shapes.empty()) {
        ImGui::Separator();
        // only the drawn shapes: a contact or an event is never deleted from a map menu
        if (ImGui::MenuItem(("Delete the " + std::to_string(shapes.size()) + " shape(s)").c_str())) {
            std::vector<std::string> cmds;
            for (const auto& nm : shapes) cmds.push_back("rm " + nm);
            pending_cmds.push_back(maiz::compile_commit(cmds));
            ed.selection = others;
        }
    }
    ImGui::EndPopup();
}
