/* phone/phone_farm.cpp — Antfarm v2 on a phone (the prototype, 2026-09-28).
 *
 * The author: "we are building primarily for desktop and mobile will just have
 * to share". So the phone draws the SAME rows, faces and canvas style the desktop
 * computes (app/farm_app.cpp), in its own screens. Two things are the phone's
 * on purpose, from the workbook's A11 and mobile.md: no key values are typed on a
 * phone (a phone is a member doing field work, not the keeper of credentials),
 * and its readiness is its own, which is where "nowhere to go from this device"
 * earns its keep: a phone cannot serve a local preview. */
#include "phone/phone_ui.hpp"

#include "voidmaiz/mobile.hpp"
#include "voidmaiz/touch.hpp"

namespace {
ImU32 dot(const std::string& s) {
    if (s == "ready") return IM_COL32(80, 190, 110, 255);
    if (s == "needs") return IM_COL32(230, 170, 60, 255);
    if (s == "failing") return IM_COL32(225, 80, 70, 255);
    if (s == "planned") return IM_COL32(130, 130, 140, 255);
    return IM_COL32(150, 160, 185, 255);
}
} // namespace

void HormigaApp::PhoneUi::farm(HormigaApp& app, PhoneUi& ph, Frame& f) {
    const float dp = ph.dp;
    if (f.route.rfind("detail:", 0) == 0) {
        const std::string name = f.route.substr(7);
        const maiz::SceneNode* n = app.scene.find(name);
        const farm::Kind* k = n ? farm::kind_by_glyph(n->glyph) : nullptr;
        if (!n || !k) {
            maiz::dim_wrapped("This node was removed, here or on another device.");
            return;
        }
        app.ed.selection = {n->name};
        ImGui::TextUnformatted(n->name.c_str());
        ImGui::TextDisabled("%s", k->label.c_str());
        maiz::dim_wrapped(k->doc.c_str());
        if (auto it = app.fv2.faces.find(n->name); it != app.fv2.faces.end()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(dot(it->second.ready.state)));
            ImGui::TextWrapped("%s%s%s", it->second.ready.state.c_str(), it->second.ready.why.empty() ? "" : ": ",
                               it->second.ready.why.c_str());
            ImGui::PopStyleColor();
            for (const auto& l : it->second.lines) maiz::dim_wrapped(l.c_str());
        }
        ImGui::Separator();
        const farm::Graph g = farm::read(app.scene);
        for (const auto& p : k->ports)
            for (const farm::Wire* w : p.out ? g.out_of(n->name, p.name) : g.into(n->name, p.name))
                ImGui::TextDisabled(p.out ? ICON_FA_ARROW_RIGHT "  %s -> %s.%s" : ICON_FA_ARROW_LEFT "  %s <- %s.%s",
                                    p.name.c_str(), p.out ? w->to.c_str() : w->from.c_str(),
                                    p.out ? w->in.c_str() : w->out.c_str());
        if (k->id == "key") maiz::dim_wrapped(ICON_FA_LOCK "  A key's value is set on a desktop, never typed on a phone.");
        ImGui::Separator();
        maiz::WidgetContext ctx{app.scene, f.out, std::string(), 0.0f};
        for (const auto& fl : n->fields) {
            if (fl.editor == "hidden" || fl.key == "pos" || fl.key == "size") continue;
            ImGui::TextDisabled("%s", fl.label.empty() ? fl.key.c_str() : fl.label.c_str());
            ImGui::PushItemWidth(-FLT_MIN);
            ImGui::PushID(fl.key.c_str());
            maiz::widget_field(ctx, app.widgets, *n, fl);
            ImGui::PopID();
            ImGui::PopItemWidth();
        }
        return;
    }

    if (ImGui::SmallButton(ICON_FA_ARROW_LEFT "  Antfarm v1")) {
        app.fv2.v2 = false;
        return;
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1), "v2 prototype");
    if (!app.fv2.here) {
        maiz::dim_wrapped("This database has no v2 Antfarm yet. Creating one adds a `farm` mantle with a "
                          "default colony. v1 is not touched, and it syncs to members like any mantle.");
        if (ImGui::Button(ICON_FA_PLUS "  Create the v2 Antfarm", ImVec2(-FLT_MIN, 48.0f * dp)))
            app.try_farm_verb("farm init");
        if (ImGui::Button(ICON_FA_CAT "  The Cat Colony showcase", ImVec2(-FLT_MIN, 48.0f * dp)))
            app.try_farm_verb("farm showcase");
        return;
    }

    int mode = ph.farm_graph ? 1 : 0;
    if (maiz::segmented("##farm2-mode", {"Connections", "Graph"}, mode)) ph.farm_graph = mode == 1;
    if (ph.farm_graph) {
        if (!ph.farm_style_ready) {
            ph.farm_style = app.canvas_style;
            maiz::TouchProfile prof;
            prof.dp = dp;
            maiz::apply_touch_canvas(ph.farm_style, prof);
            app.ed.cam.zoom = std::clamp(dp * 0.6f, 0.4f, 2.0f);
            ph.farm_style_ready = true;
        }
        const maiz::TouchGate& gate = maiz::default_touch_gate();
        if (gate.pinch.active) { // the same pinch as v1's graph (2026-09-27)
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            maiz::Camera& cam = app.ed.cam;
            const float wx = cam.x + (gate.pinch.cx - origin.x) / cam.zoom;
            const float wy = cam.y + (gate.pinch.cy - origin.y) / cam.zoom;
            cam.zoom = std::clamp(cam.zoom * gate.pinch.scale, ph.farm_style.min_zoom, ph.farm_style.max_zoom);
            cam.x = wx - (gate.pinch.cx - origin.x) / cam.zoom - gate.pinch.dx / cam.zoom;
            cam.y = wy - (gate.pinch.cy - origin.y) / cam.zoom - gate.pinch.dy / cam.zoom;
        }
        maiz::CanvasNet anet;
        anet.surfaces = &app.surfaces;
        anet.roster = &app.roster;
        anet.display = app.net_settings.show;
        anet.shareable = app.share_now;
        anet.surface_id = "canvas:antfarm";
        const maiz::CanvasStyle st = app.farm_canvas_style(ph.farm_style);
        maiz::CanvasIO cio = maiz::edit_canvas("phone-farm", app.scene, app.ed, st, &app.fv2.palette, &app.faces,
                                               {}, nullptr, &anet);
        for (auto& c : cio.commands) f.out.push_back(std::move(c));
        return;
    }

    maiz::dim_wrapped("What this database is connected to, and what this phone can do with it.");
    const float card_h = 60.0f * dp, pad = 12.0f * dp;
    std::string group;
    for (const auto& r : app.fv2.rows) {
        if (r.group != group) {
            group = r.group;
            ImGui::Spacing();
            ImGui::TextDisabled("%s", group.c_str());
        }
        ImGui::PushID((r.group + r.name + r.why).c_str());
        const bool tapped = ImGui::InvisibleButton("##card", ImVec2(-FLT_MIN, card_h));
        ImGui::PopID();
        const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(r0, r1, ImGui::GetColorU32(ImGuiCol_FrameBg), 10.0f * dp);
        dl->AddCircleFilled(ImVec2(r0.x + pad + 5 * dp, r0.y + card_h * 0.5f), 6.0f * dp, dot(r.state));
        dl->PushClipRect(r0, ImVec2(r1.x - pad, r1.y), true);
        const std::string title = r.name.empty() ? std::string("a wire") : r.name + "  " + r.label;
        const std::string sub = r.why.empty() ? (r.line.empty() ? r.state : r.line) : r.state + ": " + r.why;
        dl->AddText(ImVec2(r0.x + pad * 2 + 10 * dp, r0.y + pad * 0.7f), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());
        dl->AddText(ImVec2(r0.x + pad * 2 + 10 * dp, r0.y + card_h * 0.52f), dot(r.state), sub.c_str());
        dl->PopClipRect();
        if (tapped && !r.name.empty() && ImGui::GetIO().MouseDragMaxDistanceSqr[0] < 36.0f * dp * dp)
            f.stack->push("detail:" + r.name);
    }
}
