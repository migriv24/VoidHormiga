/* ui/farm.cpp — the Antfarm tab, v2 (the prototype, 2026-09-28).
 *
 * okf/concepts/platform/antfarm/v2/canvas.md. Connections first (A3: "everyone
 * else gets the dashboard over it"), the graph one click away, the Inspector
 * beside both. The Wiring view draws the effect boundary as an ant farm's ground
 * line: what sits above it leaves this device. v1 is one button away and keeps
 * running every publish until the migration (V7). */
#include "app/app_internal.hpp"
#include "voidmaiz/mobile.hpp" // segmented

namespace {

ImU32 row_color(const std::string& s) {
    if (s == "ready") return IM_COL32(80, 190, 110, 255);
    if (s == "needs") return IM_COL32(230, 170, 60, 255);
    if (s == "failing") return IM_COL32(225, 80, 70, 255);
    if (s == "planned") return IM_COL32(130, 130, 140, 255);
    return IM_COL32(150, 160, 185, 255);
}

/* The socket legend: shape AND colour, because colour is never the only signal. */
void legend() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const auto& t : farm::types()) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float r = 5.0f, cy = p.y + ImGui::GetTextLineHeight() * 0.5f;
        const ImU32 c = IM_COL32((t.rgb >> 16) & 0xff, (t.rgb >> 8) & 0xff, t.rgb & 0xff, 255);
        const ImVec2 m(p.x + r, cy);
        switch (t.shape) {
        case farm::Shape::Payload: dl->AddCircleFilled(m, r, c); break;
        case farm::Shape::Field:
            dl->AddQuadFilled(ImVec2(m.x, m.y - r), ImVec2(m.x + r, m.y), ImVec2(m.x, m.y + r), ImVec2(m.x - r, m.y), c);
            break;
        case farm::Shape::Reference: dl->AddRectFilled(ImVec2(m.x - r, m.y - r), ImVec2(m.x + r, m.y + r), c); break;
        case farm::Shape::Value: dl->AddCircle(m, r, c, 0, 2.0f); break;
        }
        ImGui::Dummy(ImVec2(2 * r + 3, 1));
        ImGui::SameLine(0, 0);
        ImGui::TextDisabled("%s", t.label);
        ImGui::SameLine(0, 12);
    }
    ImGui::NewLine();
}

} // namespace

void HormigaApp::draw_farm_section() {
    if (ImGui::SmallButton(ICON_FA_ARROW_LEFT "  Antfarm v1")) {
        fv2.v2 = false;
        dispatch_and_reproject(std::string("use ") + kAntfarmMantle);
        return;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("the Antfarm every publish runs on today; v2 does not change it");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1), "Antfarm v2 - prototype");
    ImGui::SameLine();
    ImGui::TextDisabled("(a design to try, not to rely on: okf/concepts/platform/antfarm/v2)");

    if (!fv2.here) {
        ImGui::Spacing();
        ImGui::TextWrapped("This database has no v2 Antfarm yet. Creating one adds a `farm` mantle with a "
                           "default colony: the database and where it rests, its chambers, a filter that "
                           "keeps private runes off the website, three documents, a local preview domain, "
                           "and this device's profile. v1 is not touched. It syncs to members like any mantle.");
        if (ImGui::Button(ICON_FA_PLUS "  Create the v2 Antfarm")) try_farm_verb("farm init");
        ImGui::SameLine();
        ImGui::TextDisabled("= `farm init`");
        if (ImGui::Button(ICON_FA_CAT "  Build the Cat Colony showcase")) try_farm_verb("farm showcase");
        ImGui::SameLine();
        ImGui::TextDisabled("= `farm showcase`: a website, a birthdays calendar, a map, a photo river, keys, a web domain");
        return;
    }
    if (scene.mantle != farm::kMantle) dispatch_and_reproject(std::string("use ") + farm::kMantle);

    maiz::segmented("##farm-view", {"Connections", "Wiring"}, fv2.view, 260.0f);
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_TABLE_COLUMNS "  Arrange")) {
        try_farm_verb("farm arrange");
        fv2.fit = true;
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("strata top to bottom (surface, ground, chambers), flow left to right;\n"
                          "positions are shared, so this arranges it on every device");
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_FA_EXPAND "  Fit")) fv2.fit = true;
    ImGui::SameLine();
    legend();

    nested_dockspace("farm-dock", nullptr, nullptr, "Antfarm v2##farm", "Inspector##farm");
    ImGui::Begin("Antfarm v2##farm", nullptr, ImGuiWindowFlags_NoScrollbar);
    if (fv2.view == 0) {
        /* ── CONNECTIONS: what this database is connected to, by question ── */
        ImGui::BeginChild("##farm-conn");
        std::string group;
        for (const auto& r : fv2.rows) {
            if (r.group != group) {
                group = r.group;
                ImGui::SeparatorText(group.c_str());
            }
            ImGui::PushID((r.group + r.name + r.why).c_str());
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const bool sel = !r.name.empty() && !ed.selection.empty() && ed.selection.front() == r.name;
            if (ImGui::Selectable("##row", sel, ImGuiSelectableFlags_AllowDoubleClick,
                                  ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 1.6f)) &&
                !r.name.empty()) {
                ed.selection = {r.name};
                if (ImGui::IsMouseDoubleClicked(0)) { // open it on the graph
                    if (const maiz::SceneNode* n = scene.find(r.name)) {
                        const ImVec2 avail = ImGui::GetContentRegionAvail();
                        ed.cam.x = n->x - avail.x * 0.5f / ed.cam.zoom;
                        ed.cam.y = n->y - avail.y * 0.4f / ed.cam.zoom;
                    }
                    fv2.view = 1;
                }
            }
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float lh = ImGui::GetTextLineHeight();
            dl->AddCircleFilled(ImVec2(p.x + 8, p.y + lh * 0.8f), 5.0f, row_color(r.state));
            const std::string left = r.name.empty() ? r.why : r.name + "   " + r.label;
            dl->AddText(ImVec2(p.x + 20, p.y + lh * 0.25f), ImGui::GetColorU32(ImGuiCol_Text), left.c_str());
            if (!r.name.empty()) {
                const std::string right = r.state + (r.why.empty() ? (r.line.empty() ? "" : " - " + r.line) : ": " + r.why);
                dl->AddText(ImVec2(p.x + 20 + std::max(260.0f, ImGui::CalcTextSize(left.c_str()).x + 24), p.y + lh * 0.25f),
                            row_color(r.state), right.c_str());
            }
            ImGui::PopID();
        }
        if (fv2.rows.empty()) ImGui::TextDisabled("nothing here yet");
        ImGui::Spacing();
        ImGui::TextDisabled("Double-click a row to find it on the graph. The same list: `farm` in the console.");
        ImGui::EndChild();
    } else {
        /* ── WIRING: the graph, with the surface line drawn over it ──────── */
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        if (fv2.fit && !scene.nodes.empty() && avail.x > 50 && avail.y > 50) { // the whole colony in view
            float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            for (const auto& n : scene.nodes) {
                x0 = std::min(x0, n.x), y0 = std::min(y0, n.y);
                x1 = std::max(x1, n.x + (n.w > 0 ? n.w : 240.0f)), y1 = std::max(y1, n.y + (n.h > 0 ? n.h : 80.0f) + 80.0f);
            }
            ed.cam.zoom = std::clamp(std::min(avail.x / (x1 - x0 + 120.0f), avail.y / (y1 - y0 + 120.0f)), 0.2f, 1.0f);
            ed.cam.x = x0 - 60.0f;
            ed.cam.y = y0 - 60.0f;
            fv2.fit = false;
        }
        maiz::CanvasNet anet;
        anet.surfaces = &surfaces;
        anet.roster = &roster;
        anet.display = net_settings.show;
        anet.shareable = share_now;
        anet.surface_id = "canvas:antfarm";
        const maiz::CanvasStyle st = farm_canvas_style(canvas_style);
        maiz::CanvasIO cio = maiz::edit_canvas("farm-canvas", scene, ed, st, &fv2.palette, &faces, {}, nullptr, &anet);
        for (const auto& cmd : cio.commands) dispatch_and_reproject(cmd);

        // the strata: a line where the surface is, and where the ground ends
        float top[4] = {1e9f, 1e9f, 1e9f, 1e9f}, bottom[4] = {-1e9f, -1e9f, -1e9f, -1e9f};
        for (const auto& n : scene.nodes) {
            const farm::Kind* k = farm::kind_by_glyph(n.glyph);
            if (!k) continue;
            const int b = k->stratum == farm::Stratum::Ground ? 1 : k->stratum == farm::Stratum::Chambers ? 2 : 0;
            int ins = 0, outs = 0;
            for (const auto& p : k->ports) (p.out ? outs : ins) += 1;
            const float h = (n.h > 0 ? n.h : (float)k->face_h) + 26.0f + 20.0f * (float)std::max(ins, outs);
            top[b] = std::min(top[b], n.y);
            bottom[b] = std::max(bottom[b], n.y + h);
        }
        /* The canvas draws in a child window, which would paint over anything
         * drawn on this one; the lines go on the foreground layer, clipped to
         * the canvas's own rectangle (docked windows never overlap it). */
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->PushClipRect(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), true);
        auto line = [&](float wy, const char* above, const char* below, ImU32 c) {
            const float sy = origin.y + (wy - ed.cam.y) * ed.cam.zoom;
            for (float x = origin.x; x < origin.x + avail.x; x += 14.0f)
                dl->AddLine(ImVec2(x, sy), ImVec2(x + 8.0f, sy), c, 2.0f);
            const float right = origin.x + avail.x - 10.0f; // labels at the right edge, clear of the nodes Arrange puts left
            dl->AddText(ImVec2(right - ImGui::CalcTextSize(above).x, sy - ImGui::GetTextLineHeight() - 2), c, above);
            dl->AddText(ImVec2(right - ImGui::CalcTextSize(below).x, sy + 3), c, below);
        };
        if (top[0] < 1e8f && top[1] < 1e8f && bottom[0] < top[1])
            line((bottom[0] + top[1]) * 0.5f, "SURFACE - leaves this device (every crossing is a gated effect)",
                 "GROUND - this device", IM_COL32(200, 150, 90, 200));
        if (top[2] < 1e8f && bottom[1] > -1e8f && bottom[1] < top[2])
            line((bottom[1] + top[2]) * 0.5f, "", "CHAMBERS - pure, live, inside the .miga", IM_COL32(120, 150, 200, 160));
        dl->PopClipRect();
    }
    ImGui::End();

    ImGui::Begin("Inspector##farm");
    draw_farm_inspector();
    ImGui::End();
}
