/* phone/phone_map.cpp — the map, on a phone.
 *
 * The author, 2026-10-04: "the clients need a map on mobile ... the map we have
 * on desktop works pretty well, but ... a lot of the functionality would have
 * to be different on mobile." The design, the research behind it, and the
 * desktop-to-phone inventory are in okf/concepts/sections/gis/phone.md.
 * The short version, which is what this file is organized around:
 *
 *   ONE FINGER MOVES THE MAP. ALWAYS. On the desktop a press on a marker drags
 *   the marker; under a finger 1 cm wide that rule moves a marker every time
 *   somebody pans across a dense street. So a drag pans, and a marker moves
 *   only after a LONG PRESS picks it up (it lifts above the finger so the
 *   finger does not hide where it lands).
 *
 *   A TAP SELECTS, AND A SHEET ANSWERS. Selection opens a bottom sheet (a
 *   card, then the marker's colour, icon and form at a drag up) instead of a
 *   side panel or a right-click menu. A tap that lands on several markers
 *   asks which, instead of guessing.
 *
 *   ADDING HAS TWO SPEEDS. Long-press the map: "add here", the fast way. Or
 *   the + button: a pin fixed in the middle of the screen and the map moved
 *   under it, the precise way (the finger never covers the spot). Both end
 *   at the same "what is it?" sheet, and both are the desktop's `place`
 *   action: one batch, undoable, replayable.
 *
 *   THE PHONE KNOWS WHERE IT IS, IF ASKED. The locate button is the only
 *   thing that may ever cause the location prompt; a fix is drawn with its
 *   accuracy, and "here" is offered wherever a place is chosen.
 *
 * Zoom is continuous here (gis/view.hpp): pinch, double tap (+1), two-finger
 * tap (-1), tap-then-drag (one-handed), and a fling coasts. All of it is view
 * state; the camera is flushed once a gesture settles, to its own config key
 * (`view.phone.map.camera`), so a phone does not move a desktop's map.
 *
 * Everything that changes the database is a dispatcher command in `f.out`:
 * the same `place`/`move` actions, `tag`, `set`, `rm` and `rune new` the
 * desktop's map sends. Nothing here writes a rune any other way. */
#include "phone/phone_map.hpp" // its state and helpers, shared with the sheet

using namespace hormiga::phone;
using namespace hormiga::phone::mapx;



bool HormigaApp::PhoneUi::map_placeable(const maiz::SceneNode& n) {
    // a kind that can be on a map (trait `located`, domain/kinds.hpp), and notes
    return n.glyph == "note" || hormiga::kinds::current().located(n.glyph);
}

void HormigaApp::PhoneUi::map_focus(HormigaApp&, PhoneUi& ph, const std::string& rune) {
    if (!ph.map_ui) ph.map_ui = std::make_shared<MapUi>();
    ph.map_ui->focus = rune;
    ph.screen = kMap;
    ph.stacks[kMap].reset();
}

bool HormigaApp::PhoneUi::map_back(PhoneUi& ph) {
    if (!ph.map_ui || ph.screen != kMap || ph.stacks[kMap].can_pop()) return false;
    MapUi& m = *ph.map_ui;
    if (m.adjust || m.draw) {
        m.adjust = false;
        m.adjust_rune.clear();
        m.draw = 0;
        return true;
    }
    if (m.sheet != MapUi::None) {
        m.sel.clear();
        m.pick.clear();
        m.set_sheet(MapUi::None);
        return true;
    }
    return false;
}


/* ── THE SCREEN ─────────────────────────────────────────────────────────────── */
void HormigaApp::PhoneUi::map(HormigaApp& app, PhoneUi& ph, Frame& f) {
    if (!ph.map_ui) ph.map_ui = std::make_shared<MapUi>();
    MapUi& m = *ph.map_ui;
    const float dp = ph.dp;
    ImGuiIO& io = ImGui::GetIO();
    /* WHERE THE PHONE IS only means something in a world with Earth
     * coordinates (okf/concepts/sections/gis/worlds.md): a scanned floor plan or
     * a fantasy map has no latitude for a GPS fix to land on. The world says so
     * through its metric; until a source can be georeferenced, haversine is
     * the test, and anything else gets no locate button and no blue dot. */
    const BaseSource& world = kBaseSources[std::clamp(app.basemap_src, 0, kBaseSourceCount - 1)];
    // THE CANVAS (2026-10-06, domain/canvas.hpp): Earth, or a drawn plan, which
    // has no tiles and no latitude for a fix to land on
    const hormiga::canvas::Canvas cvs = hormiga::canvas::find(app.scene, app.map_canvas);
    const bool plan = cvs.plan();
    const bool earth = !plan && world.metric == hormiga::gis::Metric::Haversine;
    maiz::LocationPlatform* loc = earth ? maiz::location() : nullptr;

    // the layers, bottom to top (the desktop's own list): the one being edited
    // gives the rules and the position channel; with none chosen, the top one
    const std::vector<const maiz::SceneNode*> views = app.map_layers();
    const maiz::SceneNode* view = nullptr;
    for (auto* v : views)
        if (v->name == app.map_sel) view = v;
    if (!view && !views.empty()) {
        view = views.back();
        app.map_sel = view->name;
    }
    const std::string channel = [&] {
        std::string c = view ? hormiga::temper::field_value(*view, "channel") : "";
        return c.empty() ? std::string("main") : c;
    }();
    const auto rules = view ? parse_view_rules_of(*view) : std::vector<MapRule>{};
    const std::string field = geo_field_for(channel);

    // ── the other routes on this screen's stack ──────────────────────────────
    if (f.route.rfind("note:", 0) == 0) { // a note opened from the map: the Notes editor
        PhoneUi::notes(app, ph, f);
        return;
    }
    m.views = views; // this frame's context, for the sub-screens and the sheet (phone_map_sheet.cpp)
    m.view = view;
    m.channel = channel;
    m.rules = rules;
    m.field = field;
    m.plan = plan;
    if (PhoneUi::map_route(app, ph, f)) return;

    // ── THE MAP ──────────────────────────────────────────────────────────────
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 sz = ImGui::GetContentRegionAvail();
    if (sz.x < 32 || sz.y < 32) return;
    m.v.w = sz.x;
    m.v.h = sz.y;
    m.v.tile_px = 256.0f * std::max(1.0f, dp * 0.5f); // tiles readable at a phone's density
    if (m.canvas_shown != app.map_canvas) { // another canvas: its own world, and its own camera
        m.canvas_shown = app.map_canvas;
        m.loaded = false;
        m.v.flat = plan;
        m.v.span = cvs.span();
        m.v.min_zoom = plan ? cvs.min_zoom() : 3;
        m.v.max_zoom = plan ? cvs.max_zoom() : 19;
    }
    const std::string cam_key = hormiga::canvas::camera_key(kCamKey, app.map_canvas);
    if (!m.loaded) {
        m.loaded = true;
        maiz::Camera c;
        if (maiz::parse_camera(app.core.dispatch("config get " + cam_key).data, c) ||
            maiz::parse_camera(
                app.core.dispatch("config get " + hormiga::canvas::camera_key("view.map.camera", app.map_canvas))
                    .data,
                c)) {
            m.v.lon = c.x;
            m.v.lat = c.y;
            m.v.zoom = std::clamp((double)c.zoom, m.v.min_zoom, m.v.max_zoom);
        } else if (plan) { // nothing saved on a floor: the whole floor, centred
            m.v.lat = cvs.h / 2;
            m.v.lon = cvs.w / 2;
            const double fit = std::log2(std::min(sz.x / cvs.w, sz.y / cvs.h) * 0.9 *
                                         cvs.span() / m.v.tile_px);
            m.v.zoom = std::clamp(fit, m.v.min_zoom, m.v.max_zoom);
        } else { // nothing saved: fit everything placed, or a whole continent
            double fx0 = 1, fy0 = 1, fx1 = 0, fy1 = 0;
            int nplaced = 0;
            for (const auto& n : app.scene.nodes) {
                double la, lo;
                if (!map_placeable(n) || !hormiga::parse_geo(view_geo(n, channel), la, lo)) continue;
                const double fx = (m.v.wx(lo) / m.v.world()), fy = (m.v.wy(la) / m.v.world());
                fx0 = std::min(fx0, fx), fy0 = std::min(fy0, fy), fx1 = std::max(fx1, fx), fy1 = std::max(fy1, fy);
                ++nplaced;
            }
            if (nplaced) {
                m.v.lon = m.v.ux((fx0 + fx1) / 2);
                m.v.lat = m.v.uy((fy0 + fy1) / 2);
                m.v.zoom = nplaced == 1 ? 15.0 : fit_zoom(m, fx0, fy0, fx1, fy1);
            } else {
                m.v.lat = 39.5;
                m.v.lon = -98.35;
                m.v.zoom = 3.5;
            }
        }
    }
    // a rune to show, from Data or a note ("Show on the map")
    if (!m.focus.empty()) {
        const maiz::SceneNode* n = app.scene.find(m.focus);
        double la, lo;
        if (n && hormiga::parse_geo(view_geo(*n, channel), la, lo)) {
            fly_to(m, la, lo, std::max(m.v.zoom, 16.0));
            m.sel = n->name;
            m.set_sheet(MapUi::Marker);
        } else if (n) {
            m.adjust = true;
            m.adjust_rune = n->name;
            m.set_sheet(MapUi::None);
        }
        m.focus.clear();
    }
    step_motion(m, io.DeltaTime);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
    dl->PushClipRect(p0, p1, true);
    ImGui::InvisibleButton("##map", sz);
    maiz::presence_focus_if_active(app.surfaces, "map");
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const ImVec2 mouse = io.MousePos;
    auto scr = [&](double la, double lo) {
        float x, y;
        m.v.to_screen(la, lo, x, y);
        return ImVec2(p0.x + x, p0.y + y);
    };
    auto geo_at = [&](ImVec2 s, double& la, double& lo) { m.v.to_geo(s.x - p0.x, s.y - p0.y, la, lo); };

    PhoneUi::map_ground(app, ph, dl, p0, p1); // tiles, or a plan's floor (phone_map_ground.cpp)

    /* ── THE OTHER VISIBLE LAYERS, ghosted underneath (the desktop's rule) ──
     * Each draws its own positions in its own colour, at its opacity and
     * brightness; only the layer being edited answers a finger. */
    for (auto* v : views) {
        if (v == view || hormiga::temper::field_value(*v, "visible") == "0") continue;
        std::string vch = hormiga::temper::field_value(*v, "channel");
        if (vch.empty()) vch = "main";
        const auto vrules = parse_view_rules_of(*v);
        const std::string vfilter = hormiga::temper::field_value(*v, "filter");
        const float lop = app.view_layer_opacity(v), lbr = app.view_layer_brightness(v);
        auto dim = [&](ImU32 c) {
            const int r = std::min(255, (int)((c & 0xFF) * lbr)), g = std::min(255, (int)(((c >> 8) & 0xFF) * lbr)),
                      b = std::min(255, (int)(((c >> 16) & 0xFF) * lbr));
            return IM_COL32(r, g, b, (int)(lop * 255));
        };
        for (const auto& n : app.scene.nodes) {
            if (!map_placeable(n) || (!vfilter.empty() && !maiz::node_matches(vfilter, n))) continue;
            double la, lo;
            if (!hormiga::parse_geo(view_geo(n, vch), la, lo)) continue;
            const ImVec2 s = scr(la, lo);
            if (s.x < p0.x - 20 * dp || s.x > p1.x + 20 * dp || s.y < p0.y - 20 * dp || s.y > p1.y + 20 * dp) continue;
            const MarkerLook L = marker_look(n, vrules, app.allo_style_for("map", n.name));
            dl->AddCircleFilled(s, 5.5f * dp, dim(L.col));
            dl->AddCircle(s, 5.5f * dp, IM_COL32(255, 255, 255, (int)(lop * 0.6f * 255)), 0, 1.5f * dp);
        }
    }

    // ── what is on the map ───────────────────────────────────────────────────
    /* What THIS layer draws: everything placed, unless its eye is shut or its
     * own `filter` (a tag query, no UI yet) keeps only what it holds. */
    using Placed = MapUi::Placed;
    const auto fans = app.ref_fans(app.scene, channel);
    const bool layer_hidden = view && hormiga::temper::field_value(*view, "visible") == "0";
    const std::string layer_filter = view ? hormiga::temper::field_value(*view, "filter") : std::string();
    std::vector<Placed> placed;
    for (const auto& n : app.scene.nodes) {
        if (!map_placeable(n)) continue;
        if (layer_hidden || (!layer_filter.empty() && !maiz::node_matches(layer_filter, n))) continue;
        double la, lo;
        ImVec2 s;
        bool fanned = false;
        if (auto it = fans.find(n.name); it != fans.end()) {
            la = it->second.lat;
            lo = it->second.lon;
            s = scr(la, lo);
            s.x += it->second.dx * dp; // the desktop's pixels, at this screen's density
            s.y += it->second.dy * dp;
            fanned = true;
        } else {
            if (!hormiga::parse_geo(view_geo(n, channel), la, lo)) continue;
            s = scr(la, lo);
        }
        if (n.name == m.lift && active) continue; // drawn under the finger instead
        Placed P{&n, la, lo, s, s, marker_look(n, rules, app.allo_style_for("map", n.name)), fanned};
        const auto o = hormiga::gis::marker_outline(hormiga::gis::marker_form(P.look.shape), 11 * dp);
        P.face = ImVec2(s.x + o.face.x, s.y + o.face.y);
        placed.push_back(P);
    }

    // ── regions (drawn shapes), under everything ─────────────────────────────
    const maiz::SceneNode* shape_hit = nullptr;
    for (const auto& n : app.scene.nodes) {
        if (n.glyph != "mapshape" || !hormiga::canvas::on(n, app.map_canvas)) continue;
        double la1, lo1, la2, lo2;
        if (!hormiga::parse_geo(hormiga::temper::field_value(n, "geo1"), la1, lo1) ||
            !hormiga::parse_geo(hormiga::temper::field_value(n, "geo2"), la2, lo2))
            continue;
        const ImVec2 a = scr(la1, lo1), b = scr(la2, lo2);
        const ImVec2 mn(std::min(a.x, b.x), std::min(a.y, b.y)), mx(std::max(a.x, b.x), std::max(a.y, b.y));
        unsigned col = IM_COL32(46, 107, 79, 255);
        const MarkerLook sl = marker_look(n, rules, app.allo_style_for("map", n.name));
        if (sl.col != glyph_marker_colour(n.glyph)) col = sl.col;
        const bool sel = m.sheet == MapUi::Shape && m.sel == n.name;
        const ImU32 fill = with_alpha(col, 0x33), line = with_alpha(col, sel ? 0xFF : 0xBB);
        if (hormiga::temper::field_value(n, "kind") == "ellipse") {
            const ImVec2 c((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f), r((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f);
            dl->AddEllipseFilled(c, r, fill);
            dl->AddEllipse(c, r, line, 0, 0, (sel ? 3.0f : 1.8f) * dp);
        } else {
            dl->AddRectFilled(mn, mx, fill, 4 * dp);
            dl->AddRect(mn, mx, line, 4 * dp, 0, (sel ? 3.0f : 1.8f) * dp);
        }
        const std::string lbl = hormiga::temper::field_value(n, "label");
        if (!lbl.empty()) { // inside its own region, as on the desktop
            dl->PushClipRect(ImVec2(mn.x + 2 * dp, mn.y), ImVec2(mx.x - 2 * dp, mx.y), true);
            dl->AddText(ImVec2(mn.x + 6 * dp, mn.y + 4 * dp), line, lbl.c_str());
            dl->PopClipRect();
        }
        if (mouse.x >= mn.x && mouse.x <= mx.x && mouse.y >= mn.y && mouse.y <= mx.y &&
            (hormiga::temper::field_value(n, "kind") != "ellipse" || [&] {
                const float rx = (mx.x - mn.x) * 0.5f, ry = (mx.y - mn.y) * 0.5f;
                const float nx = (mouse.x - (mn.x + rx)) / std::max(1.0f, rx), ny = (mouse.y - (mn.y + ry)) / std::max(1.0f, ry);
                return nx * nx + ny * ny <= 1.0f;
            }()))
            shape_hit = &n;
    }

    // ── reference points: the gizmo that fans co-located markers (editor-only) ─
    const maiz::SceneNode* ref_hit = nullptr;
    float ref_hit_d = 1e9f;
    for (const auto& n : app.scene.nodes) {
        if (n.glyph != "refpoint" || !hormiga::canvas::on(n, app.map_canvas)) continue;
        double la, lo;
        if (!hormiga::parse_geo(hormiga::temper::field_value(n, "geo"), la, lo)) continue;
        const bool lifted = active && m.lift == n.name;
        const ImVec2 c = lifted ? ImVec2(mouse.x, mouse.y - 30 * dp) : scr(la, lo);
        const ImU32 gc = IM_COL32(120, 100, 70, 230);
        for (const auto& [nm, fan] : fans)
            if (std::abs(fan.lat - la) < 1e-9 && std::abs(fan.lon - lo) < 1e-9)
                dl->AddLine(c, ImVec2(c.x + fan.dx * dp, c.y + fan.dy * dp), IM_COL32(120, 100, 70, 110), 1.2f * dp);
        dl->AddCircle(c, 9 * dp, gc, 0, 2 * dp);
        dl->AddLine(ImVec2(c.x - 13 * dp, c.y), ImVec2(c.x + 13 * dp, c.y), gc, 1.2f * dp);
        dl->AddLine(ImVec2(c.x, c.y - 13 * dp), ImVec2(c.x, c.y + 13 * dp), gc, 1.2f * dp);
        const float d = std::hypot(mouse.x - c.x, mouse.y - c.y);
        if (d < 24 * dp && d < ref_hit_d) {
            ref_hit = &n;
            ref_hit_d = d;
        }
    }

    // ── clusters: markers whose anchors would overlap become one counted disc.
    // Fanned children never cluster (somebody arranged them on purpose), nor
    // does the selected one (it is what the sheet is about). Past zoom 17 the
    // map stops grouping, and a tap that lands on several asks which. ──────
    struct Cluster {
        ImVec2 c;
        std::vector<int> members;
    };
    std::vector<Cluster> clusters;
    std::vector<int> cluster_of(placed.size(), -1);
    if (m.v.zoom < 17.0) {
        const float R = 30 * dp;
        for (int i = 0; i < (int)placed.size(); ++i) {
            if (placed[i].fanned || placed[i].node->name == m.sel) continue;
            int join = -1;
            for (int k = 0; k < (int)clusters.size() && join < 0; ++k)
                if (std::hypot(clusters[k].c.x - placed[i].s.x, clusters[k].c.y - placed[i].s.y) < R) join = k;
            if (join < 0) {
                clusters.push_back({placed[i].s, {i}});
                join = (int)clusters.size() - 1;
            } else {
                auto& cl = clusters[join];
                const float w = (float)cl.members.size();
                cl.c = ImVec2((cl.c.x * w + placed[i].s.x) / (w + 1), (cl.c.y * w + placed[i].s.y) / (w + 1));
                cl.members.push_back(i);
            }
            cluster_of[i] = join;
        }
    }

    // ── markers and clusters ─────────────────────────────────────────────────
    const bool labels = app.view_show_labels(view) && m.v.zoom >= 14.5;
    std::vector<ImVec4> label_boxes;
    auto draw_label = [&](const std::string& text, ImVec2 face, float r, bool strong) {
        const float fs = ImGui::GetFontSize() * (strong ? 0.95f : 0.82f) * app.view_label_scale(view);
        const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, text.c_str());
        const ImVec2 cand[3] = {{face.x + r + 5 * dp, face.y - ts.y * 0.5f},
                                {face.x - r - 5 * dp - ts.x, face.y - ts.y * 0.5f},
                                {face.x - ts.x * 0.5f, face.y - r - 4 * dp - ts.y}};
        for (const ImVec2& c : cand) {
            bool clear = true;
            for (const auto& b : label_boxes)
                if (c.x < b.z && c.x + ts.x > b.x && c.y < b.w && c.y + ts.y > b.y) clear = false;
            if (!clear && !strong) continue;
            label_boxes.push_back({c.x, c.y, c.x + ts.x, c.y + ts.y});
            for (int k = 0; k < 4; ++k) // a halo keeps it legible over any tile
                dl->AddText(ImGui::GetFont(), fs, ImVec2(c.x + (k & 1 ? 1.2f : -1.2f) * dp, c.y + (k & 2 ? 1.2f : -1.2f) * dp),
                            IM_COL32(255, 255, 255, 200), text.c_str());
            dl->AddText(ImGui::GetFont(), fs, c, IM_COL32(35, 35, 40, 255), text.c_str());
            return;
        }
    };
    auto draw_one = [&](const Placed& P, bool selected, bool ghost) {
        const MShape sh = shape_from(P.look.shape);
        float r = (P.look.icon ? 11.5f : 9.5f) * dp + (selected ? 3.0f * dp : 0.0f) + std::min(P.look.weight, 6.0f) * dp;
        const ImU32 col = ghost ? with_alpha(P.look.col, 0x70) : P.look.col;
        maiz::presence_rect(app.surfaces, app.roster, app.net_settings.show, "map", P.node->id,
                            ImVec2(P.face.x - r - 2, P.face.y - r - 2), ImVec2(P.face.x + r + 2, P.face.y + r + 2),
                            maiz::Mark::Outline, app.share_now && !app.share_now(*P.node));
        const ImVec2 face = draw_marker_shape(dl, P.s, r, col, IM_COL32(255, 255, 255, ghost ? 120 : 240), sh);
        if (P.look.icon) {
            const float isz = r * 1.05f;
            const ImVec2 is = ImGui::GetFont()->CalcTextSizeA(isz, FLT_MAX, 0, P.look.icon);
            dl->AddText(ImGui::GetFont(), isz, ImVec2(face.x - is.x * 0.5f, face.y - is.y * 0.5f), IM_COL32_WHITE,
                        P.look.icon);
        } else if (sh == MShape::Pin) {
            dl->AddCircleFilled(face, r * 0.36f, IM_COL32(255, 255, 255, 235));
        }
        if (selected || labels) {
            const std::string cap = !P.look.label.empty() ? P.look.label
                                    : P.node->glyph == "note" ? marker_caption(*P.node)
                                                              : title_of(*P.node);
            draw_label(cap, face, r, selected);
        }
    };
    for (int i = 0; i < (int)placed.size(); ++i) {
        const int k = cluster_of[i];
        if (k >= 0 && clusters[k].members.size() > 1) continue;
        if (placed[i].node->name == m.sel) continue; // drawn last, on top
        draw_one(placed[i], false, m.adjust || m.draw);
    }
    for (const auto& cl : clusters) {
        if (cl.members.size() < 2) continue;
        // the colour most of its members share, so a cluster of incidents still reads red
        std::map<unsigned, int> votes;
        for (int i : cl.members) ++votes[placed[i].look.col];
        unsigned col = votes.begin()->first;
        for (const auto& [c, v] : votes)
            if (v > votes[col]) col = c;
        const float r = (13.0f + std::min(6.0f, (float)cl.members.size())) * dp;
        dl->AddCircleFilled(ImVec2(cl.c.x + 1.2f * dp, cl.c.y + 2 * dp), r, IM_COL32(0, 0, 0, 45));
        dl->AddCircleFilled(cl.c, r + 3 * dp, with_alpha(col, 0x55));
        dl->AddCircleFilled(cl.c, r, col);
        dl->AddCircle(cl.c, r, IM_COL32(255, 255, 255, 235), 0, 2 * dp);
        const std::string num = std::to_string(cl.members.size());
        const float fs = ImGui::GetFontSize() * 0.95f;
        const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, num.c_str());
        dl->AddText(ImGui::GetFont(), fs, ImVec2(cl.c.x - ts.x * 0.5f, cl.c.y - ts.y * 0.5f), IM_COL32_WHITE, num.c_str());
    }
    for (const auto& P : placed)
        if (P.node->name == m.sel) draw_one(P, true, false);

    // ── where the phone is: a dot, and the circle it might be anywhere in ────
    if (loc) { // the answer to the prompt arrives later, from the system
        const maiz::LocationAccess acc = loc->access();
        const bool refused = acc == maiz::LocationAccess::Denied || acc == maiz::LocationAccess::Unavailable;
        if (refused && (m.follow || m.want_fix)) {
            if (m.want_fix && acc != m.last_access)
                maiz::show_snackbar(ph.snack, acc == maiz::LocationAccess::Denied
                                                  ? "Location not allowed (Settings > Apps)"
                                                  : "Location is switched off");
            m.follow = m.want_fix = false;
        }
        m.last_access = acc;
    }
    maiz::LocationFix fix;
    const bool have_fix = loc && loc->latest(fix);
    if (have_fix) {
        const ImVec2 c = scr(fix.lat, fix.lon);
        const float acc_px = (float)(fix.accuracy_m / std::max(1e-6, m.v.metres_per_px()));
        const bool stale = fix.age_s > 30;
        const ImU32 blue = stale ? IM_COL32(120, 130, 150, 255) : IM_COL32(26, 115, 232, 255);
        if (acc_px > 9 * dp) {
            dl->AddCircleFilled(c, acc_px, with_alpha(blue, 0x26), 64);
            dl->AddCircle(c, acc_px, with_alpha(blue, 0x70), 64, 1.2f * dp);
        }
        const float pulse = stale ? 0.0f : 0.5f + 0.5f * std::sin((float)ImGui::GetTime() * 3.0f);
        dl->AddCircleFilled(c, (11 + 3 * pulse) * dp, with_alpha(blue, (int)(40 + 30 * pulse)));
        dl->AddCircleFilled(c, 9 * dp, IM_COL32_WHITE);
        dl->AddCircleFilled(c, 6.5f * dp, blue);
        if (m.want_fix) { // the first fix after asking: go there
            fly_to(m, fix.lat, fix.lon, std::max(m.v.zoom, 16.0));
            m.want_fix = false;
        } else if (m.follow && !active && !m.anim.on) {
            float sx, sy;
            m.v.to_screen(fix.lat, fix.lon, sx, sy);
            if (std::hypot(sx - sz.x * 0.5f, sy - sz.y * 0.5f) > 2 * dp) fly_to(m, fix.lat, fix.lon, m.v.zoom);
        }
    }

    // ── THE PRESS ────────────────────────────────────────────────────────────
    const maiz::TouchGate& gate = maiz::default_touch_gate();
    const bool touch = io.MouseSource == ImGuiMouseSource_TouchScreen;
    const float slop = 9 * dp;
    // what is under a point: the clusters, then the markers (nearest first), then the rest
    auto markers_at = [&](ImVec2 q, float within) {
        std::vector<std::pair<float, int>> hits;
        for (int i = 0; i < (int)placed.size(); ++i) {
            const int k = cluster_of[i];
            if (k >= 0 && clusters[k].members.size() > 1) continue;
            const float d = std::min(std::hypot(q.x - placed[i].face.x, q.y - placed[i].face.y),
                                     std::hypot(q.x - placed[i].s.x, q.y - placed[i].s.y) + 4 * dp);
            if (d <= within) hits.push_back({d, i});
        }
        std::sort(hits.begin(), hits.end());
        return hits;
    };
    auto cluster_at = [&](ImVec2 q) {
        for (int k = 0; k < (int)clusters.size(); ++k)
            if (clusters[k].members.size() > 1 && std::hypot(q.x - clusters[k].c.x, q.y - clusters[k].c.y) < 26 * dp)
                return k;
        return -1;
    };
    auto select = [&](const std::string& name) { m.select(name, app.scene); };
    auto deselect = [&] { m.deselect(); };
    auto open_place = [&](double la, double lo, float acc) { m.open_place(la, lo, acc); };

    if (ImGui::IsItemActivated()) {
        m.pressing = true;
        m.press_at = mouse;
        m.press_t = ImGui::GetTime();
        m.travel = 0;
        m.long_done = false;
        m.coasting = false;
        m.anim.on = false;
        if (m.draw && !m.adjust) {
            geo_at(mouse, m.d_la, m.d_lo);
            m.drawing = true;
        }
    }
    if (m.pressing && active) {
        m.travel = std::max(m.travel, std::hypot(mouse.x - m.press_at.x, mouse.y - m.press_at.y));
        // a LONG PRESS: still, for as long as a long press takes
        if (!m.long_done && !m.drawing && m.travel < slop && ImGui::GetTime() - m.press_t >= 0.42) {
            m.long_done = true;
            if (!m.adjust) {
                const auto hits = markers_at(m.press_at, 24 * dp);
                if (!hits.empty()) { // pick it up
                    m.lift = placed[hits.front().second].node->name;
                    m.lift_ref = false;
                    select(m.lift);
                } else if (ref_hit) {
                    m.lift = ref_hit->name;
                    m.lift_ref = true;
                    m.sel = ref_hit->name;
                    m.set_sheet(MapUi::Ref);
                } else { // add here
                    double la, lo;
                    geo_at(m.press_at, la, lo);
                    m.drop_at = m.press_at;
                    open_place(la, lo, 0);
                }
            }
        }
        const ImVec2 d = io.MouseDelta;
        if (!m.lift.empty() || m.drawing) {
            // staged: the lifted marker or the region follows the finger
        } else if ((d.x != 0 || d.y != 0) && m.travel >= slop * 0.5f && !gate.pinch.active) {
            m.v.pan(d.x, d.y);
            m.follow = false;
            m.cam_dirty = true;
            m.moved_at = ImGui::GetTime();
        }
    }
    // the lifted thing, under the finger: it rides ABOVE the finger, and its
    // tip is where it will land (a finger on glass covers what is under it)
    const ImVec2 lift_tip(mouse.x, mouse.y - 34 * dp);
    if (active && !m.lift.empty()) {
        if (const maiz::SceneNode* ln = app.scene.find(m.lift); ln && !m.lift_ref) {
            const MarkerLook L = marker_look(*ln, rules, app.allo_style_for("map", ln->name));
            dl->AddLine(ImVec2(lift_tip.x - 6 * dp, lift_tip.y), ImVec2(lift_tip.x + 6 * dp, lift_tip.y),
                        IM_COL32(40, 40, 40, 200), 1.5f * dp);
            dl->AddLine(ImVec2(lift_tip.x, lift_tip.y - 6 * dp), ImVec2(lift_tip.x, lift_tip.y + 6 * dp),
                        IM_COL32(40, 40, 40, 200), 1.5f * dp);
            const ImVec2 face = draw_marker_shape(dl, lift_tip, 13 * dp, L.col, IM_COL32_WHITE,
                                                  L.shape.empty() ? MShape::Pin : shape_from(L.shape));
            if (L.icon) {
                const ImVec2 is = ImGui::GetFont()->CalcTextSizeA(13 * dp * 1.05f, FLT_MAX, 0, L.icon);
                dl->AddText(ImGui::GetFont(), 13 * dp * 1.05f, ImVec2(face.x - is.x * 0.5f, face.y - is.y * 0.5f),
                            IM_COL32_WHITE, L.icon);
            }
        }
    }
    if (m.drawing && active) { // the region being drawn
        const ImVec2 a = scr(m.d_la, m.d_lo);
        const ImVec2 mn(std::min(a.x, mouse.x), std::min(a.y, mouse.y)), mx(std::max(a.x, mouse.x), std::max(a.y, mouse.y));
        if (m.draw == 2)
            dl->AddEllipse(ImVec2((mn.x + mx.x) / 2, (mn.y + mx.y) / 2), ImVec2((mx.x - mn.x) / 2, (mx.y - mn.y) / 2),
                           IM_COL32(46, 107, 79, 230), 0, 0, 2.5f * dp);
        else
            dl->AddRect(mn, mx, IM_COL32(46, 107, 79, 230), 4 * dp, 0, 2.5f * dp);
    }
    if (ImGui::IsItemDeactivated() && m.pressing) {
        m.pressing = false;
        const bool tap = m.travel < slop && !m.long_done && ImGui::GetTime() - m.press_t < 0.6;
        if (!m.lift.empty()) { // set it down where its tip is
            const maiz::SceneNode* ln = app.scene.find(m.lift);
            if (ln && m.travel >= slop) {
                double la, lo;
                geo_at(m.lift_ref ? ImVec2(mouse.x, mouse.y - 30 * dp) : lift_tip, la, lo);
                const std::string pref = hormiga::temper::field_value(*ln, "ref");
                const maiz::SceneNode* rp = pref.empty() ? nullptr : app.scene.find(pref);
                if (m.lift_ref) {
                    f.out.push_back("set " + ln->name + " geo \"" + geo_str(la, lo) + "\"");
                } else if (rp && rp->glyph == "refpoint") {
                    // a fanned child: a re-arrangement around its reference point,
                    // in the desktop's pixels, never a new absolute position
                    double rla, rlo;
                    if (hormiga::parse_geo(hormiga::temper::field_value(*rp, "geo"), rla, rlo)) {
                        const ImVec2 rc = scr(rla, rlo);
                        char off[48];
                        std::snprintf(off, sizeof off, "%.1f,%.1f", (lift_tip.x - rc.x) / dp, (lift_tip.y - rc.y) / dp);
                        f.out.push_back("set " + ln->name + " ref_off \"" + off + "\"");
                    }
                } else {
                    auto cmds = app.map_actions.run("move", app.scene,
                                                    {{"name", ln->name}, {"geo", geo_str(la, lo)}, {"field", field}});
                    if (!cmds.empty()) f.out.push_back(maiz::compile_commit(cmds));
                    maiz::show_snackbar(ph.snack, "Moved " + title_of(*ln), "UNDO");
                }
            }
            m.lift.clear();
        } else if (m.drawing) {
            m.drawing = false;
            double la, lo;
            geo_at(mouse, la, lo);
            if (m.travel >= slop) {
                const std::string name = app.mint_name("shape");
                f.out.push_back(maiz::compile_commit(
                    {"rune new mapshape " + name, std::string("set ") + name + " kind \"" + (m.draw == 2 ? "ellipse" : "rect") + "\"",
                     "set " + name + " geo1 \"" + geo_str(m.d_la, m.d_lo) + "\"",
                     "set " + name + " geo2 \"" + geo_str(la, lo) + "\"",
                     "set " + name + " canvas " + json_str(app.map_canvas), "tag " + name + " +type:mapshape"}));
                m.draw = 0;
                m.sel = name;
                m.set_sheet(MapUi::Shape);
            }
        } else if (tap && !m.adjust) {
            const int k = cluster_at(m.press_at);
            const auto hits = markers_at(m.press_at, 24 * dp);
            if (k >= 0) { // a cluster: open it up
                double fx0 = 1, fy0 = 1, fx1 = 0, fy1 = 0;
                for (int i : clusters[k].members) {
                    const double fx = m.v.wx(placed[i].lo) / m.v.world(), fy = m.v.wy(placed[i].la) / m.v.world();
                    fx0 = std::min(fx0, fx), fy0 = std::min(fy0, fy), fx1 = std::max(fx1, fx), fy1 = std::max(fy1, fy);
                }
                const double zfit = fit_zoom(m, fx0, fy0, fx1, fy1);
                if (zfit <= m.v.zoom + 0.5 || fx1 - fx0 < 1e-12) { // already as close as it gets: say which
                    m.pick.clear();
                    for (int i : clusters[k].members) m.pick.push_back(placed[i].node->name);
                    m.set_sheet(MapUi::Pick);
                } else {
                    fly_to(m, m.v.uy((fy0 + fy1) / 2), m.v.ux((fx0 + fx1) / 2),
                           std::max(zfit, m.v.zoom + 1.0));
                }
            } else if (hits.size() >= 2 && hits[1].first - hits[0].first < 10 * dp) {
                m.pick.clear(); // too close to call: ask
                for (const auto& h : hits) m.pick.push_back(placed[h.second].node->name);
                m.set_sheet(MapUi::Pick);
            } else if (!hits.empty()) {
                select(placed[hits.front().second].node->name);
            } else if (ref_hit) {
                m.sel = ref_hit->name;
                m.set_sheet(MapUi::Ref);
            } else if (shape_hit) {
                m.sel = shape_hit->name;
                m.set_sheet(MapUi::Shape);
            } else {
                deselect();
            }
        } else if (m.travel >= slop) {
            m.since_pan = 0; // a fling may follow, a frame or two late
        }
        m.drop_at = ImVec2(-1, -1);
        if (m.sheet != MapUi::Place) m.drop_at = ImVec2(-1, -1);
    }
    if (m.since_pan < 4) {
        ++m.since_pan;
        if (gate.gestures.fling) {
            m.coast_x = gate.gestures.vx;
            m.coast_y = gate.gestures.vy;
            m.coasting = true;
            m.since_pan = 99;
        }
    }
    // ── the gestures a finger has and a mouse does not ──────────────────────
    const bool over_map = mouse.x >= p0.x && mouse.x < p1.x && mouse.y >= p0.y && mouse.y < p1.y;
    if (hovered || active || gate.pinch.active || gate.gestures.quick_zoom || gate.gestures.two_finger_tap) {
        const ImVec2 g(gate.gestures.x, gate.gestures.y);
        const bool g_in = g.x >= p0.x && g.x < p1.x && g.y >= p0.y && g.y < p1.y;
        if (gate.pinch.active && gate.pinch.cx >= p0.x && gate.pinch.cy >= p0.y && gate.pinch.cy < p1.y) {
            m.v.zoom_about(gate.pinch.scale, gate.pinch.cx - p0.x, gate.pinch.cy - p0.y);
            m.v.pan(gate.pinch.dx, gate.pinch.dy);
            m.anim.on = m.coasting = false;
            m.follow = false;
            m.cam_dirty = true;
            m.moved_at = ImGui::GetTime();
        }
        if (gate.gestures.quick_zoom && g_in) {
            m.v.zoom_about(gate.gestures.zoom_scale, g.x - p0.x, g.y - p0.y);
            m.cam_dirty = true;
            m.moved_at = ImGui::GetTime();
        }
        if (gate.gestures.taps == 2 && g_in && m.lift.empty()) start_zoom(m, 1.0, g.x - p0.x, g.y - p0.y);
        if (gate.gestures.two_finger_tap && g_in) start_zoom(m, -1.0, g.x - p0.x, g.y - p0.y);
    }
    if (!touch && hovered) { // a mouse on the desktop's --phone: the wheel and a double click
        if (io.MouseWheel != 0) {
            m.v.zoom_about(std::pow(2.0, io.MouseWheel * 0.5), mouse.x - p0.x, mouse.y - p0.y);
            m.cam_dirty = true;
            m.moved_at = ImGui::GetTime();
        }
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) start_zoom(m, 1.0, mouse.x - p0.x, mouse.y - p0.y);
    }
    (void)over_map;
    /* WHAT THE SHEET IS ABOUT STAYS IN SIGHT. A sheet covers the bottom of the
     * map; a place chosen down there would be hidden by the very sheet that
     * asks about it. So the map slides until the point sits in the visible
     * part, a little above its middle. */
    if (m.reveal && !active) {
        m.reveal = false;
        const float sheet_frac = m.bs.detents.empty() ? 0.3f : m.bs.detents[std::clamp(m.bs.detent, 0, (int)m.bs.detents.size() - 1)];
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float visible_bottom = std::min(p1.y, vp->WorkPos.y + vp->WorkSize.y * (1.0f - sheet_frac));
        const float top = p0.y + 70 * dp; // under the search bar
        float sx, sy;
        m.v.to_screen(m.reveal_la, m.reveal_lo, sx, sy);
        sx += p0.x;
        sy += p0.y;
        const float want_y = top + (visible_bottom - top) * 0.55f;
        if (sy > visible_bottom - 40 * dp || sy < top + 30 * dp || sx < p0.x + 30 * dp || sx > p1.x - 30 * dp) {
            // the centre that puts the point at want_y (and back in from a side edge)
            const bool off_side = sx < p0.x + 30 * dp || sx > p1.x - 30 * dp;
            double la, lo;
            geo_at(ImVec2(off_side ? sx : p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f + (sy - want_y)), la, lo);
            fly_to(m, la, lo, m.v.zoom);
        }
    }
    // the camera, flushed once things settle (config tier: logged, never undone)
    if (m.cam_dirty && !active && !m.anim.on && !m.coasting && !gate.pinch.active &&
        ImGui::GetTime() - m.moved_at > 0.6) {
        m.cam_dirty = false;
        maiz::Camera c;
        c.x = (float)m.v.lon;
        c.y = (float)m.v.lat;
        c.zoom = (float)m.v.zoom;
        f.out.push_back(maiz::compile_camera(c, cam_key));
    }

    // ── the pin dropped by a long press, while "add here" is asked ───────────
    if (m.sheet == MapUi::Place) {
        const ImVec2 at = scr(m.place_la, m.place_lo);
        draw_marker_shape(dl, at, 12 * dp, IM_COL32(214, 64, 54, 255), IM_COL32_WHITE, MShape::Pin);
        const auto o = hormiga::gis::marker_outline(hormiga::gis::MarkerForm::Pin, 12 * dp);
        dl->AddCircleFilled(ImVec2(at.x, at.y + o.face.y), 12 * dp * 0.36f, IM_COL32_WHITE);
        if (m.place_acc > 0) {
            const float acc_px = (float)(m.place_acc / std::max(1e-6, m.v.metres_per_px()));
            dl->AddCircle(at, acc_px, IM_COL32(214, 64, 54, 140), 64, 1.5f * dp);
        }
    }
    // ── placement: a pin fixed in the middle; the map moves under it ─────────
    const ImVec2 mid(p0.x + sz.x * 0.5f, p0.y + sz.y * 0.5f);
    if (m.adjust) {
        const float lift = active ? 8 * dp : 0.0f; // it rises a little while the map moves
        dl->AddEllipseFilled(mid, ImVec2(6 * dp, 2.2f * dp), IM_COL32(0, 0, 0, 90));
        dl->AddCircleFilled(mid, 2.2f * dp, IM_COL32(30, 30, 30, 220));
        const ImVec2 tip(mid.x, mid.y - lift);
        const ImVec2 face = draw_marker_shape(dl, tip, 14 * dp, IM_COL32(214, 64, 54, 255), IM_COL32_WHITE, MShape::Pin);
        dl->AddCircleFilled(face, 14 * dp * 0.36f, IM_COL32_WHITE);
    }

    // ── a scale bar, and the base map's credit ───────────────────────────────
    {
        const float sheet_h = m.bs.initialized ? m.bs.height * ImGui::GetMainViewport()->WorkSize.y : 0.0f;
        const float y = p1.y - std::max(sheet_h - (ImGui::GetMainViewport()->WorkPos.y + ImGui::GetMainViewport()->WorkSize.y - p1.y), 0.0f) - 14 * dp;
        const double mpp = m.v.metres_per_px();
        if (y >= p0.y + sz.y * 0.4f) { // under a tall sheet there is no map to read
            const double metres = nice_metres(mpp, 90 * dp);
            const float len = (float)(metres / mpp);
            const ImVec2 a(p0.x + 12 * dp, y);
            // a canvas in feet or squares says so; metres read as they always did
            const std::string t = plan && cvs.unit != "m" ? [&] {
                char b[32];
                std::snprintf(b, sizeof b, "%g %s", metres, cvs.unit.c_str());
                return std::string(b);
            }()
                                                          : human_m(metres);
            dl->AddRectFilled(ImVec2(a.x - 4 * dp, a.y - ImGui::GetFontSize() - 6 * dp), ImVec2(a.x + len + 4 * dp, a.y + 5 * dp),
                              IM_COL32(255, 255, 255, 170), 4 * dp);
            dl->AddLine(a, ImVec2(a.x + len, a.y), IM_COL32(50, 50, 50, 230), 2 * dp);
            dl->AddLine(a, ImVec2(a.x, a.y - 5 * dp), IM_COL32(50, 50, 50, 230), 2 * dp);
            dl->AddLine(ImVec2(a.x + len, a.y), ImVec2(a.x + len, a.y - 5 * dp), IM_COL32(50, 50, 50, 230), 2 * dp);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.8f, ImVec2(a.x, a.y - ImGui::GetFontSize() - 3 * dp),
                        IM_COL32(40, 40, 40, 255), t.c_str());
            // a floor plan draws nobody's tiles, so it credits nobody
            const char* credit = plan ? "" : kBaseSources[std::clamp(app.basemap_src, 0, kBaseSourceCount - 1)].attribution;
            const float cs = ImGui::GetFontSize() * 0.68f;
            const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(cs, FLT_MAX, 0, credit);
            if (*credit) dl->AddRectFilled(ImVec2(p1.x - ts.x - 8 * dp, y - ts.y - 2 * dp), ImVec2(p1.x, y + 3 * dp), IM_COL32(255, 255, 255, 160));
            dl->AddText(ImGui::GetFont(), cs, ImVec2(p1.x - ts.x - 4 * dp, y - ts.y), IM_COL32(70, 70, 70, 255), credit);
        }
        if (int pend = app.tiles.pending(); pend > 0) {
            const float r = 7 * dp;
            const ImVec2 c(p1.x - 18 * dp, p0.y + 70 * dp);
            const float a0 = (float)ImGui::GetTime() * 6.0f;
            dl->PathArcTo(c, r, a0, a0 + 4.2f, 16);
            dl->PathStroke(IM_COL32(60, 60, 60, 200), 0, 2.2f * dp);
        }
    }
    dl->PopClipRect();

    // ── the floating controls: windows of their own, so a tap on one never
    // reaches the map under it ──────────────────────────────────────────────
    const ImGuiWindowFlags float_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing;
    const float btn = 50 * dp;
    auto round_button = [&](const char* id, const char* icon, bool on) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, btn * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, on ? ImGui::GetStyle().Colors[ImGuiCol_CheckMark] : ImVec4(1, 1, 1, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, on ? ImGui::GetStyle().Colors[ImGuiCol_CheckMark] : ImVec4(1, 1, 1, 1));
        ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(1, 1, 1, 1) : ImVec4(0.15f, 0.17f, 0.2f, 1));
        ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.25f);
        const bool hit = ImGui::Button((std::string(icon) + "##" + id).c_str(), ImVec2(btn, btn));
        ImGui::PopFont();
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImGui::GetForegroundDrawList(ImGui::GetMainViewport()); // (shadow drawn by the window below)
        (void)a, (void)b;
        return hit;
    };
    // top: search, and the layers
    {
        ImGui::SetNextWindowPos(ImVec2(p0.x + 10 * dp, p0.y + 10 * dp));
        ImGui::SetNextWindowSize(ImVec2(sz.x - 20 * dp, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##map-top", nullptr, float_flags & ~ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleVar();
        if (m.adjust || m.draw) {
            const std::string what = m.draw ? (m.draw == 2 ? "Drag across the map to draw an ellipse"
                                                           : "Drag across the map to draw a rectangle")
                                     : m.adjust_rune.empty()
                                         ? "Move the map to put the pin where the new place goes"
                                         : [&] {
                                               const maiz::SceneNode* an = app.scene.find(m.adjust_rune);
                                               return "Move the map to put the pin where " +
                                                      (an ? title_of(*an) : m.adjust_rune) + " goes";
                                           }();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.13f, 0.15f, 0.92f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12 * dp);
            ImGui::BeginChild("##hint", ImVec2(-FLT_MIN, 0), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            ImGui::TextWrapped("%s", what.c_str());
            ImGui::PopStyleColor();
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        } else {
            const float w = sz.x - 20 * dp - btn - 8 * dp;
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, btn * 0.5f);
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.96f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 1));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.37f, 0.4f, 1));
            if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS "   Search the map##msearch", ImVec2(w, btn))) {
                m.search[0] = 0;
                m.focus_field = true;
                f.stack->push("search");
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);
            ImGui::SameLine(0, 8 * dp);
            if (round_button("layers", ICON_FA_LAYER_GROUP, false)) f.stack->push("layers");
        }
        ImGui::End();
    }
    // bottom right: where am I, and add; or the placement / drawing bar
    const float sheet_top = [&] {
        if (m.sheet == MapUi::None && !m.adjust && !m.draw) {
            const float h = m.bs.initialized ? m.bs.height : 0.10f;
            return ImGui::GetMainViewport()->WorkPos.y + ImGui::GetMainViewport()->WorkSize.y * (1.0f - h);
        }
        if (m.sheet != MapUi::None && m.bs.initialized)
            return ImGui::GetMainViewport()->WorkPos.y + ImGui::GetMainViewport()->WorkSize.y * (1.0f - m.bs.height);
        return p1.y;
    }();
    if (m.adjust || m.draw) {
        ImGui::SetNextWindowPos(ImVec2(p0.x + 10 * dp, p1.y - 10 * dp), 0, ImVec2(0, 1));
        ImGui::SetNextWindowSize(ImVec2(sz.x - 20 * dp, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14 * dp);
        ImGui::Begin("##map-bar", nullptr, (float_flags & ~ImGuiWindowFlags_NoBackground) & ~ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleVar();
        const float gap = ImGui::GetStyle().ItemSpacing.x;
        if (m.draw) {
            int kind = m.draw - 1;
            if (maiz::segmented("##drawkind", {"Rectangle", "Ellipse"}, kind)) m.draw = kind + 1;
            if (ImGui::Button("Done", ImVec2(-FLT_MIN, 0))) m.draw = 0;
        } else {
            const bool can_here = have_fix;
            const int n = can_here ? 3 : 2;
            const float w = (ImGui::GetContentRegionAvail().x - gap * (n - 1)) / n;
            if (ImGui::Button("Cancel", ImVec2(w, btn))) {
                m.adjust = false;
                m.adjust_rune.clear();
            }
            if (can_here) {
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_LOCATION_CROSSHAIRS "  Here", ImVec2(w, btn)))
                    fly_to(m, fix.lat, fix.lon, std::max(m.v.zoom, 17.0));
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::Button(ICON_FA_CHECK "  Place", ImVec2(w, btn))) {
                double la, lo;
                geo_at(mid, la, lo);
                if (!m.adjust_rune.empty()) {
                    auto cmds = app.map_actions.run("move", app.scene,
                                                    {{"name", m.adjust_rune}, {"geo", geo_str(la, lo)}, {"field", field}});
                    if (!cmds.empty()) f.out.push_back(maiz::compile_commit(cmds));
                    select(m.adjust_rune);
                } else {
                    open_place(la, lo, 0);
                }
                m.adjust = false;
                m.adjust_rune.clear();
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::End();
    } else if (sheet_top > p0.y + sz.y * 0.45f) { // a tall sheet has the screen: the buttons step aside
        ImGui::SetNextWindowPos(ImVec2(p1.x - 12 * dp, std::min(p1.y, sheet_top) - 12 * dp), 0, ImVec2(1, 1));
        ImGui::Begin("##map-fab", nullptr, float_flags);
        if (loc) {
            const maiz::LocationAccess acc = loc->access();
            if (round_button("locate", m.follow ? ICON_FA_LOCATION_ARROW : ICON_FA_LOCATION_CROSSHAIRS, m.follow)) {
                switch (acc) {
                case maiz::LocationAccess::Denied:
                    maiz::show_snackbar(ph.snack, "Location not allowed (Settings > Apps)");
                    break;
                case maiz::LocationAccess::Unavailable:
                    maiz::show_snackbar(ph.snack, "Location is switched off");
                    break;
                case maiz::LocationAccess::Asking: break;
                default:
                    if (!loc->following()) loc->request(true); // the ONE place the prompt may come from
                    if (have_fix) fly_to(m, fix.lat, fix.lon, std::max(m.v.zoom, 16.0));
                    else m.want_fix = true;
                    m.follow = true;
                }
            }
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, btn * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.3f);
        const bool add = ImGui::Button(ICON_FA_PLUS "##map-add", ImVec2(btn, btn));
        ImGui::PopFont();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar();
        if (add) {
            m.adjust = true; // the precise way: the pin in the middle
            m.adjust_rune.clear();
            m.set_sheet(MapUi::None);
            m.sel.clear();
            if (have_fix && fix.age_s < 60) fly_to(m, fix.lat, fix.lon, std::max(m.v.zoom, 17.0));
        }
        ImGui::End();
    }

    // ── THE SHEET (phone_map_sheet.cpp) ──────────────────────────────────────
    m.placed = placed;
    m.fix = fix;
    m.have_fix = have_fix;
    PhoneUi::map_sheet(app, ph, f);
}
