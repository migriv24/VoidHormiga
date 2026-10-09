/* phone/phone_map_sheet.cpp — the map's bottom sheet and its own screens.
 *
 * The canvas (phone_map.cpp) draws the map and decides what a finger meant;
 * this file answers it. The SHEET is the map's one panel: what is on the map
 * (at rest), a marker (a card, then its colour, icon and form at a drag up),
 * "add here", which of several, a region, a reference point. The SCREENS are
 * the routes the map pushes on its stack: search, put something already saved
 * here, and the views and base map. Every change is a dispatcher command in
 * `f.out`, the same the desktop sends. */
#include "phone/phone_map.hpp"

#include "domain/bestow.hpp" // one inside-the-shape test
#include "json.hpp"          // a note's first words

using namespace hormiga::phone;
using namespace hormiga::phone::mapx;

/* The map's own screens. True when the route was one of them (drawn). */
bool HormigaApp::PhoneUi::map_route(HormigaApp& app, PhoneUi& ph, Frame& f) {
    MapUi& m = *ph.map_ui;
    const float dp = ph.dp;
    ImGuiIO& io = ImGui::GetIO();
    const auto& views = m.views;
    const maiz::SceneNode* view = m.view;
    const std::string& channel = m.channel;
    const std::string& field = m.field;
    if (f.route == "search" || f.route == "existing") {
        const bool existing = f.route == "existing";
        char* buf = existing ? m.existing : m.search;
        const size_t cap = existing ? sizeof m.existing : sizeof m.search;
        if (m.focus_field) { // the screen window persists across routes: focus on arrival, once
            ImGui::SetKeyboardFocusHere();
            m.focus_field = false;
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##msearch",
                                 existing ? ICON_FA_MAGNIFYING_GLASS "  Who or what goes here?"
                                          : ICON_FA_MAGNIFYING_GLASS "  Search  (@tag works too)",
                                 buf, cap);
        maiz::text_input_kind(maiz::InputKind::Search);
        if (existing)
            maiz::dim_wrapped("Something already in the database, put where you pressed. Only things not yet on "
                              "this map are listed.");
        const std::string q = lower(buf);
        int shown = 0;
        for (const auto& n : app.scene.nodes) {
            if (!map_placeable(n)) continue;
            const std::string g = view_geo(n, channel);
            if (existing && !g.empty()) continue;
            const std::string t = n.glyph == "note" ? marker_caption(n) : title_of(n);
            if (!search_match(n, buf) && lower(subtitle_of(n)).find(q) == std::string::npos) continue; // @tags too
            if (++shown > 60) break;
            ImGui::PushID(n.name.c_str());
            const float h = 58.0f * dp;
            const bool tapped = ImGui::InvisibleButton("##row", ImVec2(-FLT_MIN, h));
            const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (ImGui::IsItemActive()) dl->AddRectFilled(a, b, ImGui::GetColorU32(ImGuiCol_FrameBgActive), 10 * dp);
            const float r = 17 * dp;
            const ImVec2 c(a.x + 8 * dp + r, (a.y + b.y) * 0.5f);
            dl->AddCircleFilled(c, r, kind_colour(n.glyph));
            const char* ic = kind_icon(n.glyph);
            const ImVec2 is = ImGui::CalcTextSize(ic);
            dl->AddText(ImVec2(c.x - is.x * 0.5f, c.y - is.y * 0.5f), IM_COL32_WHITE, ic);
            const float tx = c.x + r + 12 * dp;
            dl->AddText(ImVec2(tx, a.y + 8 * dp), ImGui::GetColorU32(ImGuiCol_Text), t.c_str());
            const std::string sub = kind_label(n.glyph) + (g.empty() ? "  ·  not on the map yet" : "  ·  on the map");
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, ImVec2(tx, a.y + 10 * dp + ImGui::GetFontSize()),
                        ImGui::GetColorU32(g.empty() ? ImGuiCol_TextDisabled : ImGuiCol_Text), sub.c_str());
            ImGui::PopID();
            if (tapped && io.MouseDragMaxDistanceSqr[0] < 64 * dp * dp) {
                double la, lo;
                if (existing) {
                    auto cmds = app.map_actions.run(
                        "move", app.scene, {{"name", n.name}, {"geo", geo_str(m.place_la, m.place_lo)}, {"field", field}});
                    if (!cmds.empty()) f.out.push_back(maiz::compile_commit(cmds));
                    m.sel = n.name;
                    m.set_sheet(MapUi::Marker);
                    m.existing[0] = 0;
                } else if (!g.empty() && hormiga::parse_geo(g, la, lo)) {
                    fly_to(m, la, lo, std::max(m.v.zoom, 16.0));
                    m.sel = n.name;
                    m.set_sheet(MapUi::Marker);
                } else { // not placed yet: the pin in the middle, for it
                    m.adjust = true;
                    m.adjust_rune = n.name;
                    m.set_sheet(MapUi::None);
                }
                f.stack->pop();
                return true;
            }
        }
        if (shown == 0) maiz::dim_wrapped(existing ? "Nothing unplaced matches." : "Nothing matches.");
        return true;
    }
    if (f.route == "layers") {
        /* THE DESKTOP'S LAYERS PANEL, AT A FINGER'S SIZE (2026-10-05). The
         * author: keep the phone "aligned with the design in the desktop (with
         * the new layer system and such)". The same stack, top first; the same
         * eye, the same "the one being edited", the same order, name, opacity,
         * own positions and labels; the base map as the bottom row. Each is the
         * command the desktop sends. Style rules stay on the desktop: the phone
         * says how many a layer has and uses them. No Actions here (the author:
         * not on the phone yet). */
        auto slider = [&](const char* label, float stored, float lo, float hi, const char* fmt, float* out) {
            ImGuiStorage* st = ImGui::GetStateStorage();
            const ImGuiID id = ImGui::GetID(label);
            float* v = st->GetFloatRef(id, stored);
            bool* held = st->GetBoolRef(id + 1, false);
            if (!*held) *v = stored; // follows the database until a finger holds it
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::SliderFloat(label, v, lo, hi, fmt);
            *held = ImGui::IsItemActive();
            if (!ImGui::IsItemDeactivatedAfterEdit()) return false;
            *out = *v;
            return true;
        };
        const float row_h = 52 * dp, eye_w = 48 * dp, arrow_w = 40 * dp;
        // WHICH CANVAS (2026-10-06, canvases.md): Earth, or a drawn floor plan
        ImGui::SeparatorText("Canvas");
        for (const auto& c : hormiga::canvas::all(app.scene)) {
            const std::string lbl = std::string(c.plan() ? ICON_FA_TABLE_CELLS "  " : ICON_FA_EARTH_AMERICAS "  ") +
                                    c.title + "##cv" + c.name;
            if (ImGui::RadioButton(lbl.c_str(), c.name == app.map_canvas) && c.name != app.map_canvas) {
                f.out.push_back("config set view.map.canvas " + json_str(c.name));
                app.map_canvas = c.name;
                app.map_sel.clear();
                m.deselect();
            }
        }
        if (ImGui::Button(ICON_FA_PLUS "  New floor plan", ImVec2(-FLT_MIN, 0))) app.map_new_canvas("Floor plan", 30, 20);
        ImGui::SeparatorText("Layers");
        if (ImGui::Button(ICON_FA_PLUS "  New layer on top", ImVec2(-FLT_MIN, 0)))
            app.map_new_earth(m.v.lat, m.v.lon, (int)m.v.zoom); // it says so itself (a toast)
        ImGui::Spacing();
        int move_from = -1, move_dir = 0;
        for (int i = (int)views.size() - 1; i >= 0; --i) { // top first, as every drawing app lists it
            const maiz::SceneNode& l = *views[(size_t)i];
            ImGui::PushID(l.name.c_str());
            const bool active = l.name == app.map_sel;
            const bool vis = hormiga::temper::field_value(l, "visible") != "0";
            const std::string ch = hormiga::temper::field_value(l, "channel");
            const bool own = !ch.empty() && ch != hormiga::canvas::shared_channel(app.map_canvas);
            const ImVec2 a = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(a, ImVec2(a.x + w, a.y + row_h),
                              ImGui::GetColorU32(active ? ImGuiCol_HeaderActive : ImGuiCol_FrameBg), 10 * dp);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            if (ImGui::Button(vis ? ICON_FA_EYE : ICON_FA_EYE_SLASH, ImVec2(eye_w, row_h)))
                f.out.push_back("set " + l.name + " visible \"" + (vis ? "0" : "1") + "\"");
            ImGui::SameLine(0, 0);
            const float name_w = w - eye_w - arrow_w * 2;
            if (ImGui::InvisibleButton("##edit", ImVec2(name_w, row_h))) app.map_sel = l.name;
            const ImVec2 na = ImGui::GetItemRectMin();
            const std::string nm = rune_title(l);
            const float fh = ImGui::GetFontSize();
            dl->AddText(ImVec2(na.x + 4 * dp, na.y + row_h * 0.5f - fh * (active ? 1.0f : 0.5f)),
                        ImGui::GetColorU32(vis ? ImGuiCol_Text : ImGuiCol_TextDisabled), nm.c_str());
            if (active || own) {
                const std::string sub = std::string(active ? "editing" : "") + (active && own ? "  ·  " : "") +
                                        (own ? ICON_FA_CODE_BRANCH " own positions" : "");
                dl->AddText(ImGui::GetFont(), fh * 0.85f, ImVec2(na.x + 4 * dp, na.y + row_h * 0.5f + 1 * dp),
                            ImGui::GetColorU32(ImGuiCol_TextDisabled), sub.c_str());
            }
            ImGui::SameLine(0, 0);
            ImGui::BeginDisabled(i == (int)views.size() - 1);
            if (ImGui::Button(ICON_FA_ARROW_UP "##up", ImVec2(arrow_w, row_h))) move_from = i, move_dir = +1;
            ImGui::EndDisabled();
            ImGui::SameLine(0, 0);
            ImGui::BeginDisabled(i == 0);
            if (ImGui::Button(ICON_FA_ARROW_DOWN "##down", ImVec2(arrow_w, row_h))) move_from = i, move_dir = -1;
            ImGui::EndDisabled();
            ImGui::PopStyleColor();
            ImGui::PopID();
            ImGui::Dummy(ImVec2(0, 2 * dp));
        }
        if (move_from >= 0) { // swap two neighbours, renumber 0..n-1: the desktop's rule
            std::vector<const maiz::SceneNode*> re = views;
            std::swap(re[(size_t)move_from], re[(size_t)(move_from + move_dir)]);
            std::vector<std::string> cmds;
            for (size_t k = 0; k < re.size(); ++k)
                if (hormiga::temper::field_value(*re[k], "order") != std::to_string(k))
                    cmds.push_back("set " + re[k]->name + " order \"" + std::to_string(k) + "\"");
            if (!cmds.empty()) f.out.push_back(maiz::compile_commit(cmds));
        }
        if (views.empty())
            maiz::dim_wrapped("No layers yet: everything shows in its kind's colour. A layer is a way of drawing "
                              "the map (its colours, which things, its labels), not a box things live in.");

        // the background: under every layer (Earth's base map, or the floor)
        if (!m.plan) {
            const ImVec2 a = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            ImGui::GetWindowDrawList()->AddRect(a, ImVec2(a.x + w, a.y + row_h), ImGui::GetColorU32(ImGuiCol_Border),
                                                10 * dp, 0, 1.0f * dp);
            const int bsi = std::clamp(app.basemap_src, 0, kBaseSourceCount - 1);
            const std::string lbl = std::string(ICON_FA_MAP "   Base map: ") + kBaseSources[bsi].label;
            ImGui::GetWindowDrawList()->AddText(ImVec2(a.x + 14 * dp, a.y + row_h * 0.5f - ImGui::GetFontSize() * 0.5f),
                                                ImGui::GetColorU32(ImGuiCol_Text), lbl.c_str());
            ImGui::Dummy(ImVec2(w, row_h));
        }

        if (const maiz::SceneNode* cr = m.plan ? app.scene.find(app.map_canvas) : nullptr) {
            ImGui::SeparatorText(ICON_FA_TABLE_CELLS "  The floor");
            maiz::WidgetContext wctx{app.scene, f.out, std::string(), 0.0f};
            for (const auto& fl : cr->fields)
                if (fl.key == "title" || fl.key == "width" || fl.key == "height" || fl.key == "grid" || fl.key == "unit")
                    maiz::widget_field(wctx, app.widgets, *cr, fl);
            maiz::dim_wrapped("Metres from the top-left corner. A region's tags go to everything placed inside it.");
        }
        if (view) {
            ImGui::SeparatorText(("Layer: " + rune_title(*view)).c_str());
            for (const auto& fl : view->fields)
                if (fl.key == "title") {
                    maiz::WidgetContext wctx{app.scene, f.out, std::string(), 0.0f};
                    maiz::widget_field(wctx, app.widgets, *view, fl);
                }
            float v;
            ImGui::TextDisabled("Opacity, seen under another layer");
            if (slider("##opacity", app.view_layer_opacity(view), 0.05f, 1.0f, "%.2f", &v)) {
                char b[32];
                std::snprintf(b, sizeof b, "%.2f", v);
                f.out.push_back("set " + view->name + " layer_opacity \"" + b + "\"");
            }
            ImGui::TextDisabled("Brightness, seen under another layer");
            if (slider("##brightness", app.view_layer_brightness(view), 0.3f, 1.5f, "%.2f", &v)) {
                char b[32];
                std::snprintf(b, sizeof b, "%.2f", v);
                f.out.push_back("set " + view->name + " layer_brightness \"" + b + "\"");
            }
            const std::string ch = hormiga::temper::field_value(*view, "channel");
            bool own = !ch.empty() && ch != hormiga::canvas::shared_channel(app.map_canvas);
            if (ImGui::Checkbox("Its own positions", &own))
                f.out.push_back("set " + view->name + " channel " + json_str(own ? hormiga::canvas::own_channel(app.map_canvas, view->name) : hormiga::canvas::shared_channel(app.map_canvas)));
            maiz::dim_wrapped(own ? "Moving something on this layer moves it only here."
                                  : "Things sit where they sit on every layer.");
            bool lbl = app.view_show_labels(view);
            if (ImGui::Checkbox("Names beside the markers", &lbl))
                f.out.push_back("set " + view->name + " show_labels \"" + (lbl ? "1" : "0") + "\"");
            const auto n_rules = m.rules.size();
            if (n_rules)
                ImGui::TextDisabled("%zu colour rule%s, set on the desktop", n_rules, n_rules == 1 ? "" : "s");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
            if (ImGui::Button(ICON_FA_TRASH "  Delete this layer", ImVec2(-FLT_MIN, 0))) {
                f.out.push_back("rm " + view->name);
                app.map_sel.clear();
                maiz::show_snackbar(ph.snack, "Deleted the layer. What was on it stays on the map", "UNDO");
            }
            ImGui::PopStyleColor();
        }

        if (m.plan) return true; // a floor has no base map
        ImGui::SeparatorText("Base map");
        for (int i = 0; i < kBaseSourceCount; ++i)
            if (ImGui::RadioButton(kBaseSources[i].label, app.basemap_src == i)) {
                app.basemap_src = i;
                f.out.push_back(std::string("config set ui.basemap \"") + kBaseSources[i].key + "\"");
            }
        float bv;
        ImGui::TextDisabled("Brightness");
        if (slider("##basebright", app.basemap_brightness, 0.3f, 1.0f, "%.2f", &bv)) {
            app.basemap_brightness = bv;
            char b[64];
            std::snprintf(b, sizeof b, "config set ui.basemap_brightness \"%.2f\"", bv);
            f.out.push_back(b);
        }
        ImGui::TextDisabled("Fade");
        if (slider("##basefade", app.basemap_fade, 0.0f, 1.0f, "%.2f", &bv)) {
            app.basemap_fade = bv;
            char b[64];
            std::snprintf(b, sizeof b, "config set ui.basemap_fade \"%.2f\"", bv);
            f.out.push_back(b);
        }
        ImGui::Spacing();
        maiz::dim_wrapped(kBaseSources[std::clamp(app.basemap_src, 0, kBaseSourceCount - 1)].attribution);
        return true;
    }

    return false;
}

void HormigaApp::PhoneUi::map_sheet(HormigaApp& app, PhoneUi& ph, Frame& f) {
    MapUi& m = *ph.map_ui;
    const float dp = ph.dp;
    ImGuiIO& io = ImGui::GetIO();
    const auto& rules = m.rules;
    const std::string& channel = m.channel;
    const std::string& field = m.field;
    const maiz::SceneNode* view = m.view;
    const auto& placed = m.placed;
    const maiz::LocationFix& fix = m.fix;
    const bool have_fix = m.have_fix;
    using Placed = MapUi::Placed;
    auto select = [&](const std::string& name) { m.select(name, app.scene); };
    auto deselect = [&] { m.deselect(); };
    auto open_place = [&](double la, double lo, float acc) { m.open_place(la, lo, acc); };
    // ── THE SHEET ────────────────────────────────────────────────────────────
    if (m.adjust || m.draw) {
        app.ed.selection.clear();
        return;
    }
    const maiz::SceneNode* sn = m.sel.empty() ? nullptr : app.scene.find(m.sel);
    // a rune selected the frame it was made does not exist until the frame's
    // commands are dispatched: give it a few frames before calling it gone
    if ((m.sheet == MapUi::Marker || m.sheet == MapUi::Shape || m.sheet == MapUi::Ref) && !sn) {
        if (++m.missing > 3) deselect();
    } else {
        m.missing = 0;
    }
    app.ed.selection.clear();
    if (sn) app.ed.selection = {sn->name}; // presence: what my sheet is about

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10 * dp, 9 * dp));
    m.in_sheet = true;
    const bool sheet_ready = sn || (m.sheet != MapUi::Marker && m.sheet != MapUi::Shape && m.sheet != MapUi::Ref);
    if (maiz::begin_bottom_sheet("##map-sheet", m.bs) && sheet_ready) {
        auto dismiss = [&] {
            deselect();
            if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) ImGui::CloseCurrentPopup();
        };
        auto header = [&](const char* icon, ImU32 col, const std::string& title, const std::string& sub) {
            const ImVec2 a = ImGui::GetCursorScreenPos();
            const float r = 20 * dp;
            ImDrawList* sdl = ImGui::GetWindowDrawList();
            sdl->AddCircleFilled(ImVec2(a.x + r, a.y + r), r, col);
            const ImVec2 is = ImGui::CalcTextSize(icon);
            sdl->AddText(ImVec2(a.x + r - is.x * 0.5f, a.y + r - is.y * 0.5f), IM_COL32_WHITE, icon);
            ImGui::Dummy(ImVec2(2 * r, 2 * r));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.15f);
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopFont();
            if (!sub.empty()) ImGui::TextDisabled("%s", sub.c_str());
            ImGui::EndGroup();
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
            if (ImGui::Button(ICON_FA_XMARK "##close", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) dismiss();
        };
        auto swatches = [&](const std::string& cur, auto on_pick) {
            const float d = 34 * dp, gap = 8 * dp;
            const int per = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap) / (d + gap)));
            int i = 0;
            for (int c = -1; c < (int)(sizeof kMarkerColors / sizeof kMarkerColors[0]); ++c, ++i) {
                if (i % per) ImGui::SameLine(0, gap);
                ImGui::PushID(c + 100);
                const bool hit = ImGui::InvisibleButton("##sw", ImVec2(d, d));
                const ImVec2 a = ImGui::GetItemRectMin(), ctr(a.x + d / 2, a.y + d / 2);
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                const bool on = c < 0 ? cur.empty() : cur == kMarkerColors[c].tag;
                if (c < 0) { // "the kind's own colour"
                    sdl->AddCircle(ctr, d * 0.42f, ImGui::GetColorU32(ImGuiCol_TextDisabled), 0, 2 * dp);
                    sdl->AddLine(ImVec2(ctr.x - d * 0.28f, ctr.y + d * 0.28f), ImVec2(ctr.x + d * 0.28f, ctr.y - d * 0.28f),
                                 ImGui::GetColorU32(ImGuiCol_TextDisabled), 2 * dp);
                } else {
                    sdl->AddCircleFilled(ctr, d * 0.42f, kMarkerColors[c].col);
                }
                if (on) sdl->AddCircle(ctr, d * 0.5f, ImGui::GetColorU32(ImGuiCol_Text), 0, 2.5f * dp);
                ImGui::PopID();
                if (hit) on_pick(c < 0 ? std::string() : std::string(kMarkerColors[c].tag));
            }
        };
        auto retag = [&](const std::string& rune, const char* ns, const std::string& cur, const std::string& next) {
            if (cur == next) return;
            std::string cmd = "tag " + rune;
            if (!cur.empty()) cmd += std::string(" -") + ns + ":" + cur;
            if (!next.empty()) cmd += std::string(" +") + ns + ":" + next;
            f.out.push_back(cmd);
        };

        switch (m.sheet) {
        case MapUi::Marker: {
            const maiz::SceneNode& n = *sn;
            const MarkerLook L = marker_look(n, rules, app.allo_style_for("map", n.name));
            std::string sub = kind_label(n.glyph);
            if (const std::string s2 = subtitle_of(n); !s2.empty() && s2 != sub && n.glyph != "note") sub += "  ·  " + s2;
            double la = 0, lo = 0;
            const bool located = hormiga::parse_geo(view_geo(n, channel), la, lo);
            if (have_fix && located)
                sub += "  ·  " + human_m(map_distance(m, fix.lat, fix.lon, la, lo)) + " away";
            header(L.icon ? L.icon : kind_icon(n.glyph), L.col, n.glyph == "note" ? marker_caption(n) : title_of(n), sub);
            // the four things a person does with a place
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const float w = (ImGui::GetContentRegionAvail().x - gap * 3) / 4;
            auto action = [&](const char* icon, const char* label) {
                ImGui::BeginGroup();
                const bool hit = ImGui::Button((std::string(icon) + "##" + label).c_str(), ImVec2(w, 44 * dp));
                const ImVec2 ts = ImGui::CalcTextSize(label);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (w - ts.x) * 0.5f));
                ImGui::TextDisabled("%s", label);
                ImGui::EndGroup();
                return hit;
            };
            if (action(ICON_FA_UP_RIGHT_FROM_SQUARE, "Open")) {
                if (n.glyph == "note") f.stack->push("note:" + n.name);
                else f.stack->push("detail:" + n.name);
            }
            ImGui::SameLine();
            if (action(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Move")) {
                m.adjust = true;
                m.adjust_rune = n.name;
                if (located) fly_to(m, la, lo, std::max(m.v.zoom, 17.0));
                m.set_sheet(MapUi::None);
            }
            ImGui::SameLine();
            if (action(ICON_FA_NOTE_STICKY, "Note") && located) { // a note about this place, here
                const std::string name = app.mint_name("note");
                auto cmds = app.map_actions.run("place", app.scene,
                                                {{"glyph", "note"}, {"name", name}, {"geo", geo_str(la, lo)}, {"field", field}});
                if (!cmds.empty()) {
                    cmds.push_back("setjson " + name + " text " +
                                   json_arg(nlohmann::json("About " + title_of(n) + "\n").dump()));
                    f.out.push_back(maiz::compile_commit(cmds));
                    f.stack->push("note:" + name);
                }
            }
            ImGui::SameLine();
            if (action(ICON_FA_CROSSHAIRS, "Centre") && located) fly_to(m, la, lo, std::max(m.v.zoom, 16.0));

            if (m.bs.detent == 0) maiz::dim_wrapped("Drag up for its colour, icon and shape.");
            ImGui::SeparatorText("Colour");
            const std::string cur_col = tag_value(n, "color");
            swatches(cur_col, [&](const std::string& c) { retag(n.name, "color", cur_col, c); });
            ImGui::SeparatorText("Icon");
            {
                const std::string cur = tag_value(n, "icon");
                const float d = 44 * dp, gap2 = 6 * dp;
                const int per = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap2) / (d + gap2)));
                int i = 0;
                for (int k = -1; k < (int)(sizeof kMarkerIcons / sizeof kMarkerIcons[0]); ++k, ++i) {
                    if (i % per) ImGui::SameLine(0, gap2);
                    const bool on = k < 0 ? cur.empty() : cur == kMarkerIcons[k].tag;
                    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBgActive]);
                    ImGui::PushID(k + 300);
                    const bool hit = ImGui::Button(k < 0 ? "-" : kMarkerIcons[k].glyph, ImVec2(d, d));
                    ImGui::PopID();
                    if (on) ImGui::PopStyleColor();
                    if (hit) retag(n.name, "icon", cur, k < 0 ? std::string() : std::string(kMarkerIcons[k].tag));
                }
            }
            ImGui::SeparatorText("Shape");
            {
                const std::string cur = tag_value(n, "shape");
                const float d = 52 * dp, gap2 = 8 * dp;
                const int per = std::max(1, (int)((ImGui::GetContentRegionAvail().x + gap2) / (d + gap2)));
                int i = 0;
                for (const char* shn : kMarkerShapes) {
                    if (i++ % per) ImGui::SameLine(0, gap2);
                    const std::string val = std::string(shn) == "circle" ? std::string() : std::string(shn);
                    ImGui::PushID(shn);
                    const bool hit = ImGui::InvisibleButton("##shape", ImVec2(d, d));
                    ImGui::PopID();
                    const ImVec2 a = ImGui::GetItemRectMin();
                    ImDrawList* sdl = ImGui::GetWindowDrawList();
                    const bool on = cur == val;
                    sdl->AddRectFilled(a, ImVec2(a.x + d, a.y + d),
                                       ImGui::GetColorU32(on ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg), 10 * dp);
                    const MShape ms = shape_from(shn);
                    const bool tip = ms == MShape::Pin || ms == MShape::Balloon;
                    draw_marker_shape(sdl, ImVec2(a.x + d / 2, tip ? a.y + d - 8 * dp : a.y + d / 2), 9 * dp, L.col,
                                      IM_COL32_WHITE, ms);
                    if (hit) retag(n.name, "shape", cur, val);
                }
            }
            // notes about this place: anything noted within 40 m
            if (located) {
                std::vector<const maiz::SceneNode*> near;
                for (const auto& o : app.scene.nodes) {
                    double ola, olo;
                    if (o.glyph != "note" || o.name == n.name || !hormiga::parse_geo(view_geo(o, channel), ola, olo)) continue;
                    if (map_distance(m, la, lo, ola, olo) <= 40.0) near.push_back(&o);
                }
                if (!near.empty()) {
                    ImGui::SeparatorText("Notes here");
                    for (const auto* o : near)
                        if (ImGui::Selectable((std::string(ICON_FA_NOTE_STICKY "  ") + marker_caption(*o) + "##" + o->name).c_str()))
                            f.stack->push("note:" + o->name);
                }
            }
            // a reference point groups markers that share an address
            {
                std::vector<const maiz::SceneNode*> refs;
                for (const auto& o : app.scene.nodes)
                    if (o.glyph == "refpoint") refs.push_back(&o);
                const std::string cur = hormiga::temper::field_value(n, "ref");
                if (!refs.empty() || !cur.empty()) {
                    ImGui::SeparatorText("Grouped at");
                    if (ImGui::RadioButton("Its own place", cur.empty()) && !cur.empty())
                        f.out.push_back("set " + n.name + " ref \"\"");
                    for (const auto* rp : refs) {
                        std::string lbl = hormiga::temper::field_value(*rp, "label");
                        if (lbl.empty()) lbl = rp->name;
                        if (ImGui::RadioButton((lbl + "##" + rp->name).c_str(), cur == rp->name) && cur != rp->name)
                            f.out.push_back("set " + n.name + " ref \"" + rp->name + "\"");
                    }
                }
            }
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_MAP_PIN "  Take off the map", ImVec2(-FLT_MIN, 0))) {
                std::vector<std::string> cmds = {"set " + n.name + " " + field + " \"\""};
                if (field == "geo" || hormiga::temper::field_value(n, "geo").empty()) cmds.push_back("tag " + n.name + " -located");
                f.out.push_back(maiz::compile_commit(cmds));
                maiz::show_snackbar(ph.snack, "Took " + title_of(n) + " off the map", "UNDO");
                deselect();
            }
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
            if (ImGui::Button(ICON_FA_TRASH "  Delete", ImVec2(-FLT_MIN, 0))) {
                f.out.push_back("rm " + n.name);
                maiz::show_snackbar(ph.snack, "Deleted " + title_of(n), "UNDO");
                deselect();
            }
            ImGui::PopStyleColor();
            break;
        }
        case MapUi::Place: {
            std::string where = geo_str(m.place_la, m.place_lo);
            if (m.place_acc > 0) where = "Where you are, give or take " + human_m(m.place_acc);
            header(ICON_FA_LOCATION_DOT, IM_COL32(214, 64, 54, 255), "Add here", where);
            struct Kind {
                const char* glyph;
                const char* label;
                const char* icon;
            };
            /* WHAT CAN GO HERE: every kind that can be on a map (trait
             * `located`, domain/kinds.hpp), the database's own first (a store's
             * products, a home's devices), as the database calls them; then
             * notes and reference points, which are the map's own. */
            std::vector<Kind> kinds;
            const auto& reg = hormiga::kinds::current();
            for (int pass = 0; pass < 2; ++pass)
                for (const auto& kd : reg.all)
                    if (kd.located && kd.palette && kd.builtin == (pass == 1))
                        kinds.push_back({kd.glyph.c_str(), kd.title.c_str(), glyph_icon(kd.glyph)});
            kinds.push_back({"note", "Note", ICON_FA_NOTE_STICKY});
            kinds.push_back({"refpoint", "Reference point", ICON_FA_CROSSHAIRS});
            const float gap = ImGui::GetStyle().ItemSpacing.x;
            const float w = (ImGui::GetContentRegionAvail().x - gap * 2) / 3, h = 72 * dp;
            int i = 0;
            for (const Kind& k : kinds) {
                if (i++ % 3) ImGui::SameLine();
                ImGui::PushID(k.glyph);
                const bool hit = ImGui::InvisibleButton("##k", ImVec2(w, h));
                ImGui::PopID();
                const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                sdl->AddRectFilled(a, b, ImGui::GetColorU32(ImGui::IsItemActive() ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg),
                                   12 * dp);
                const ImU32 kc = std::string(k.glyph) == "refpoint" ? IM_COL32(120, 100, 70, 255)
                                                                     : (ImU32)glyph_marker_colour(k.glyph);
                const ImVec2 c((a.x + b.x) * 0.5f, a.y + 24 * dp);
                sdl->AddCircleFilled(c, 15 * dp, kc);
                const ImVec2 is = ImGui::CalcTextSize(k.icon);
                sdl->AddText(ImVec2(c.x - is.x * 0.5f, c.y - is.y * 0.5f), IM_COL32_WHITE, k.icon);
                const float fs = ImGui::GetFontSize() * 0.85f;
                const ImVec2 ls = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, k.label);
                sdl->AddText(ImGui::GetFont(), fs, ImVec2(c.x - ls.x * 0.5f, b.y - ls.y - 8 * dp),
                             ImGui::GetColorU32(ImGuiCol_Text), k.label);
                if (!hit) continue;
                const std::string g = geo_str(m.place_la, m.place_lo);
                if (std::string(k.glyph) == "refpoint") {
                    const std::string name = app.mint_name("refpoint");
                    f.out.push_back(maiz::compile_commit({"rune new refpoint " + name, "set " + name + " geo \"" + g + "\"",
                                                          "set " + name + " label \"Reference\"",
                                                          "set " + name + " canvas " + json_str(app.map_canvas)}));
                    m.sel = name;
                    m.set_sheet(MapUi::Ref);
                    break;
                }
                const std::string name = app.mint_name(k.glyph);
                auto cmds = app.map_actions.run("place", app.scene,
                                                {{"glyph", k.glyph}, {"name", name}, {"geo", g}, {"field", field}});
                if (cmds.empty()) break;
                f.out.push_back(maiz::compile_commit(cmds));
                deselect();
                // straight to what it needs: a name, or the note's words
                f.stack->push(std::string(k.glyph) == "note" ? "note:" + name : "detail:" + name);
                break;
            }
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_MAGNIFYING_GLASS "  Someone or something already saved", ImVec2(-FLT_MIN, 0))) {
                m.existing[0] = 0;
                m.focus_field = true;
                f.stack->push("existing");
            }
            if (have_fix && m.place_acc <= 0) {
                const std::string here = std::string(ICON_FA_LOCATION_CROSSHAIRS "  Where I am instead (") +
                                         human_m(fix.accuracy_m) + ")";
                if (ImGui::Button(here.c_str(), ImVec2(-FLT_MIN, 0))) {
                    open_place(fix.lat, fix.lon, std::max(1.0f, fix.accuracy_m));
                    fly_to(m, fix.lat, fix.lon, std::max(m.v.zoom, 16.0));
                }
            }
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_DRAW_POLYGON "  Draw a region instead", ImVec2(-FLT_MIN, 0))) {
                m.draw = 1;
                m.set_sheet(MapUi::None);
            }
            break;
        }
        case MapUi::Pick: {
            ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.1f);
            ImGui::Text("%d things here", (int)m.pick.size());
            ImGui::PopFont();
            maiz::dim_wrapped("They are too close together to tell apart. Which one?");
            for (const auto& nm : m.pick) {
                const maiz::SceneNode* pn = app.scene.find(nm);
                if (!pn) continue;
                const MarkerLook L = marker_look(*pn, rules, app.allo_style_for("map", pn->name));
                ImGui::PushID(nm.c_str());
                const bool hit = ImGui::InvisibleButton("##p", ImVec2(-FLT_MIN, 48 * dp));
                ImGui::PopID();
                const ImVec2 a = ImGui::GetItemRectMin();
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                const ImVec2 c(a.x + 18 * dp, a.y + 24 * dp);
                sdl->AddCircleFilled(c, 14 * dp, L.col);
                const char* ic = L.icon ? L.icon : kind_icon(pn->glyph);
                const ImVec2 is = ImGui::CalcTextSize(ic);
                sdl->AddText(ImVec2(c.x - is.x * 0.5f, c.y - is.y * 0.5f), IM_COL32_WHITE, ic);
                const std::string t = pn->glyph == "note" ? marker_caption(*pn) : title_of(*pn);
                sdl->AddText(ImVec2(c.x + 24 * dp, a.y + 6 * dp), ImGui::GetColorU32(ImGuiCol_Text), t.c_str());
                sdl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, ImVec2(c.x + 24 * dp, a.y + 8 * dp + ImGui::GetFontSize()),
                             ImGui::GetColorU32(ImGuiCol_TextDisabled), kind_label(pn->glyph).c_str());
                if (hit) {
                    select(nm);
                    break;
                }
            }
            break;
        }
        case MapUi::Shape: {
            const maiz::SceneNode& n = *sn;
            std::string lbl = hormiga::temper::field_value(n, "label");
            const bool ell = hormiga::temper::field_value(n, "kind") == "ellipse";
            header(ICON_FA_DRAW_POLYGON, IM_COL32(46, 107, 79, 255), lbl.empty() ? std::string("A region") : lbl,
                   ell ? "Ellipse" : "Rectangle");
            const std::string cur_col = tag_value(n, "color");
            swatches(cur_col, [&](const std::string& c) { retag(n.name, "color", cur_col, c); });
            // the tags it gives: the same chip editor as the desktop's inspector
            // (the registry's "bestow" editor), the same commands
            ImGui::Spacing();
            for (const auto& fl : n.fields)
                if (fl.key == "bestows") {
                    maiz::WidgetContext wctx{app.scene, f.out, std::string(), 0.0f};
                    maiz::widget_field(wctx, app.widgets, n, fl);
                }
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_PEN "  Name it", ImVec2(-FLT_MIN, 0))) f.stack->push("detail:" + n.name);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
            if (ImGui::Button(ICON_FA_TRASH "  Delete the region", ImVec2(-FLT_MIN, 0))) {
                f.out.push_back("rm " + n.name);
                maiz::show_snackbar(ph.snack, "Deleted the region", "UNDO");
                deselect();
            }
            ImGui::PopStyleColor();
            break;
        }
        case MapUi::Ref: {
            const maiz::SceneNode& n = *sn;
            std::string lbl = hormiga::temper::field_value(n, "label");
            int kids = 0;
            for (const auto& en : app.scene.nodes)
                if (hormiga::temper::field_value(en, "ref") == n.name) ++kids;
            header(ICON_FA_CROSSHAIRS, IM_COL32(120, 100, 70, 255), lbl.empty() ? n.name : lbl,
                   std::to_string(kids) + (kids == 1 ? " marker gathered here" : " markers gathered here"));
            maiz::dim_wrapped("A reference point keeps several things at one address apart on the map. It is only "
                              "seen in Hormiga, never in a picture or on the website. Hold it to move it and "
                              "everything with it.");
            if (ImGui::Button(ICON_FA_PEN "  Name it", ImVec2(-FLT_MIN, 0))) f.stack->push("detail:" + n.name);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
            if (ImGui::Button(ICON_FA_TRASH "  Delete (its markers stay)", ImVec2(-FLT_MIN, 0))) {
                std::vector<std::string> cmds;
                for (const auto& en : app.scene.nodes)
                    if (hormiga::temper::field_value(en, "ref") == n.name) cmds.push_back("set " + en.name + " ref \"\"");
                cmds.push_back("rm " + n.name);
                f.out.push_back(maiz::compile_commit(cmds));
                deselect();
            }
            ImGui::PopStyleColor();
            break;
        }
        default: { // nothing selected: what is on the map, nearest first
            std::vector<const Placed*> rows;
            for (const auto& P : placed) rows.push_back(&P);
            const double cla = have_fix && m.follow ? fix.lat : m.v.lat, clo = have_fix && m.follow ? fix.lon : m.v.lon;
            std::sort(rows.begin(), rows.end(), [&](const Placed* a, const Placed* b) {
                return map_distance(m, cla, clo, a->la, a->lo) < map_distance(m, cla, clo, b->la, b->lo);
            });
            ImGui::Text("%d on this map", (int)rows.size());
            ImGui::SameLine();
            ImGui::TextDisabled("%s", view ? ("·  " + title_of(*view)).c_str() : "");
            if (m.bs.detent == 0 && !rows.empty()) {
                ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("List").x - ImGui::GetStyle().FramePadding.x * 2);
                if (ImGui::SmallButton("List")) maiz::bottom_sheet_snap(m.bs, 1);
            }
            for (const Placed* P : rows) {
                ImGui::PushID(P->node->name.c_str());
                const bool hit = ImGui::InvisibleButton("##r", ImVec2(-FLT_MIN, 50 * dp));
                ImGui::PopID();
                const ImVec2 a = ImGui::GetItemRectMin();
                ImDrawList* sdl = ImGui::GetWindowDrawList();
                const ImVec2 c(a.x + 18 * dp, a.y + 25 * dp);
                sdl->AddCircleFilled(c, 14 * dp, P->look.col);
                const char* ic = P->look.icon ? P->look.icon : kind_icon(P->node->glyph);
                const ImVec2 is = ImGui::CalcTextSize(ic);
                sdl->AddText(ImVec2(c.x - is.x * 0.5f, c.y - is.y * 0.5f), IM_COL32_WHITE, ic);
                const std::string t = P->node->glyph == "note" ? marker_caption(*P->node) : title_of(*P->node);
                sdl->AddText(ImVec2(c.x + 24 * dp, a.y + 6 * dp), ImGui::GetColorU32(ImGuiCol_Text), t.c_str());
                const std::string sub = kind_label(P->node->glyph) + "  ·  " +
                                        human_m(map_distance(m, cla, clo, P->la, P->lo));
                sdl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f,
                             ImVec2(c.x + 24 * dp, a.y + 8 * dp + ImGui::GetFontSize()),
                             ImGui::GetColorU32(ImGuiCol_TextDisabled), sub.c_str());
                if (hit && io.MouseDragMaxDistanceSqr[0] < 64 * dp * dp) {
                    fly_to(m, P->la, P->lo, std::max(m.v.zoom, 16.0));
                    select(P->node->name);
                }
            }
            if (rows.empty())
                maiz::dim_wrapped("Nothing is on the map yet. Hold your finger on the map to add something "
                                  "there, or press + to place it exactly.");
            break;
        }
        }
    }
    maiz::end_bottom_sheet(m.bs);
    m.in_sheet = false;
    if (m.pending_sheet >= 0) {
        m.set_sheet((MapUi::Sheet)m.pending_sheet);
        m.pending_sheet = -1;
    }
    ImGui::PopStyleVar();
}
