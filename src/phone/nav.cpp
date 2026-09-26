/* phone/nav.cpp — see nav.hpp. ImGui only; nothing here knows Hormiga. */
#include "phone/nav.hpp"

#include "imgui_internal.h" // BeginViewportSideBar: reserve an edge of the viewport

#include <algorithm>
#include <cstdio>

namespace hormiga::phone {

namespace {

const ImGuiWindowFlags kBarFlags = ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_NoScrollWithMouse |
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoCollapse;

// the title is set larger than body text (a phone's top bar reads at arm's length)
constexpr float kTitleScale = 1.3f;
float app_bar_height() {
    const ImGuiStyle& s = ImGui::GetStyle();
    return std::max(ImGui::GetFrameHeight(), ImGui::GetFontSize() * kTitleScale + s.FramePadding.y * 2.0f) +
           s.WindowPadding.y * 2.0f;
}

/* One destination: an icon over its label, a pill behind the current one, a
 * badge if it has a count. Returns true when tapped. Drawn in the bar window. */
bool draw_item(const NavItem& it, int id, float x, float w, bool sel) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 at(x, ImGui::GetWindowPos().y);
    const float h = ImGui::GetWindowSize().y;
    const ImU32 on = ImGui::GetColorU32(ImGuiCol_CheckMark);
    const ImU32 off = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    ImFont* font = ImGui::GetFont();
    const float icon_px = ImGui::GetFontSize() * 1.35f;
    const float label_px = ImGui::GetFontSize() * 0.82f;
    ImGui::SetCursorScreenPos(at);
    ImGui::PushID(id);
    const bool tapped = ImGui::InvisibleButton("##dest", ImVec2(w, h));
    const bool hot = ImGui::IsItemHovered();
    ImGui::PopID();
    const ImU32 col = sel ? on : (hot ? ImGui::GetColorU32(ImGuiCol_Text) : off);
    const float cx = x + w * 0.5f;
    // the selected destination gets a pill behind its icon (Material 3)
    const ImVec2 isz = font->CalcTextSizeA(icon_px, FLT_MAX, 0, it.icon);
    const float iy = at.y + h * 0.16f;
    if (sel) {
        const ImVec4 a = ImGui::ColorConvertU32ToFloat4(on);
        dl->AddRectFilled(ImVec2(cx - isz.x * 1.2f - 6, iy - 3), ImVec2(cx + isz.x * 1.2f + 6, iy + isz.y + 3),
                          ImGui::GetColorU32(ImVec4(a.x, a.y, a.z, 0.18f)), isz.y);
    }
    dl->AddText(font, icon_px, ImVec2(cx - isz.x * 0.5f, iy), col, it.icon);
    const ImVec2 lsz = font->CalcTextSizeA(label_px, FLT_MAX, 0, it.label);
    dl->AddText(font, label_px, ImVec2(cx - lsz.x * 0.5f, iy + isz.y + 6), col, it.label);
    if (it.badge > 0) {
        char b[8];
        std::snprintf(b, sizeof b, "%d", std::min(it.badge, 99));
        const float r = label_px * 0.75f;
        const ImVec2 bc(cx + isz.x * 0.5f + r * 0.6f, iy + r * 0.3f);
        dl->AddCircleFilled(bc, r, IM_COL32(220, 70, 70, 255));
        const ImVec2 bs = font->CalcTextSizeA(label_px * 0.85f, FLT_MAX, 0, b);
        dl->AddText(font, label_px * 0.85f, ImVec2(bc.x - bs.x * 0.5f, bc.y - bs.y * 0.5f), IM_COL32_WHITE, b);
    }
    return tapped;
}

} // namespace

float nav_bar_height() { return ImGui::GetFontSize() * 3.6f; }

bool nav_bar(const std::vector<NavItem>& items, int& current) {
    bool changed = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::BeginViewportSideBar("##phone-nav", ImGui::GetMainViewport(),
                                                  ImGuiDir_Down, nav_bar_height(), kBarFlags);
    ImGui::PopStyleVar();
    if (open && !items.empty()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 at = ImGui::GetWindowPos();
        const ImVec2 size = ImGui::GetWindowSize();
        // a hairline above the bar, so it reads as an edge and not a gap
        dl->AddLine(at, ImVec2(at.x + size.x, at.y), ImGui::GetColorU32(ImGuiCol_Border));
        const float w = size.x / (float)items.size();
        for (int i = 0; i < (int)items.size(); ++i)
            if (draw_item(items[i], i, at.x + w * (float)i, w, i == current)) {
                changed = true;
                current = i;
            }
    }
    ImGui::End();
    return changed;
}

int nav_bar_centred(const std::vector<NavItem>& items, int current, bool centre_on) {
    int tapped = -1;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool open = ImGui::BeginViewportSideBar("##phone-nav", ImGui::GetMainViewport(), ImGuiDir_Down,
                                                  nav_bar_height(), kBarFlags);
    ImGui::PopStyleVar();
    if (open) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 at = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
        dl->AddLine(at, ImVec2(at.x + size.x, at.y), ImGui::GetColorU32(ImGuiCol_Border));
        // the centre takes a slot's width and a little more; the rest share what is left
        const int n = (int)items.size(), left = (n + 1) / 2;
        const float cw = size.x / (float)(n + 1) * 1.1f;
        const float w = n > 0 ? (size.x - cw) / (float)n : 0.0f;
        for (int i = 0; i < n; ++i) {
            const float x = at.x + w * (float)i + (i >= left ? cw : 0.0f);
            if (draw_item(items[i], i, x, w, i == current)) tapped = i;
        }
        // THE HORMIGA BUTTON: fixed in the middle, raised on a disc, never moved
        const float cx = at.x + w * (float)left + cw * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(cx - cw * 0.5f, at.y));
        if (ImGui::InvisibleButton("##hormiga", ImVec2(cw, size.y))) tapped = kCentreTapped;
        const bool held = ImGui::IsItemActive();
        const float r = std::min(cw * 0.36f, size.y * 0.36f) * (held ? 0.94f : 1.0f);
        const ImVec4 acc = ImGui::GetStyle().Colors[ImGuiCol_CheckMark];
        const ImU32 bg = centre_on ? ImGui::GetColorU32(acc)
                                   : ImGui::GetColorU32(ImVec4(acc.x * 0.85f, acc.y * 0.85f, acc.z * 0.85f, 1.0f));
        dl->AddCircleFilled(ImVec2(cx, at.y + size.y * 0.47f + r * 0.08f), r * 1.04f, IM_COL32(0, 0, 0, 40)); // shadow
        draw_ant_button(dl, ImVec2(cx, at.y + size.y * 0.47f), r, bg, IM_COL32(255, 255, 255, 240));
    }
    ImGui::End();
    return tapped;
}

void draw_ant_button(ImDrawList* dl, ImVec2 c, float r, ImU32 bg, ImU32 col) {
    dl->AddCircleFilled(c, r, bg, 48);
    // an ant seen from above, head up: three body segments, six legs, two antennae
    const float u = r / 10.0f, th = std::max(1.0f, u * 0.75f);
    auto P = [&](float x, float y) { return ImVec2(c.x + x * u, c.y + y * u); };
    const ImVec2 head = P(0, -4.6f), thorax = P(0, -1.2f), gaster = P(0, 3.4f);
    for (int s = -1; s <= 1; s += 2) {
        const float k = (float)s;
        // legs: out from the thorax, bent at the knee, the back pair swept back
        dl->AddPolyline(std::vector<ImVec2>{P(k * 0.8f, -2.0f), P(k * 3.6f, -3.4f), P(k * 5.0f, -5.4f)}.data(), 3, col, 0, th);
        dl->AddPolyline(std::vector<ImVec2>{P(k * 0.9f, -1.2f), P(k * 4.2f, -0.8f), P(k * 5.8f, 0.2f)}.data(), 3, col, 0, th);
        dl->AddPolyline(std::vector<ImVec2>{P(k * 0.8f, -0.4f), P(k * 3.4f, 1.8f), P(k * 4.6f, 5.2f)}.data(), 3, col, 0, th);
        // antennae: up from the head, elbowed outwards
        dl->AddPolyline(std::vector<ImVec2>{P(k * 0.6f, -5.6f), P(k * 1.4f, -7.4f), P(k * 3.2f, -8.0f)}.data(), 3, col, 0, th);
    }
    dl->AddEllipseFilled(head, ImVec2(1.5f * u, 1.3f * u), col);
    dl->AddEllipseFilled(thorax, ImVec2(1.1f * u, 1.7f * u), col);
    dl->AddCircleFilled(P(0, 0.9f), 0.55f * u, col); // the waist
    dl->AddEllipseFilled(gaster, ImVec2(2.1f * u, 2.8f * u), col);
}

bool begin_app_bar(const char* title, bool can_back) {
    bool back = false;
    ImGui::BeginViewportSideBar("##phone-appbar", ImGui::GetMainViewport(), ImGuiDir_Up,
                                app_bar_height(), kBarFlags);
    ImGui::AlignTextToFramePadding();
    if (can_back) {
        // a wide target: the chevron is small, the finger is not
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        back = ImGui::Button("\xef\x81\x93##back", ImVec2(ImGui::GetFrameHeight() * 1.4f, 0)); // U+F053 chevron-left
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * kTitleScale);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    return back;
}

void end_app_bar() {
    // the bar's lower edge, like the navigation bar's upper one
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    dl->AddLine(ImVec2(p.x, p.y + s.y - 1), ImVec2(p.x + s.x, p.y + s.y - 1),
                ImGui::GetColorU32(ImGuiCol_Border));
    ImGui::End();
}

bool back_pressed() {
    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_AppBack, false)) return true;
    if (ImGui::IsMouseClicked(3)) return true; // the mouse's "back" side button
    return !io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
}

} // namespace hormiga::phone
