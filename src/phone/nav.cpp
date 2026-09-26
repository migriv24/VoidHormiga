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
        const ImU32 on = ImGui::GetColorU32(ImGuiCol_CheckMark);
        const ImU32 off = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        ImFont* font = ImGui::GetFont();
        const float icon_px = ImGui::GetFontSize() * 1.35f;
        const float label_px = ImGui::GetFontSize() * 0.82f;
        for (int i = 0; i < (int)items.size(); ++i) {
            const NavItem& it = items[i];
            ImGui::SetCursorScreenPos(ImVec2(at.x + w * (float)i, at.y));
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##dest", ImVec2(w, size.y))) {
                changed = true;
                current = i;
            }
            const bool sel = (i == current);
            const bool hot = ImGui::IsItemHovered();
            const ImU32 col = sel ? on : (hot ? ImGui::GetColorU32(ImGuiCol_Text) : off);
            const float cx = at.x + w * ((float)i + 0.5f);
            // the selected destination gets a pill behind its icon (Material 3)
            const ImVec2 isz = font->CalcTextSizeA(icon_px, FLT_MAX, 0, it.icon);
            const float iy = at.y + size.y * 0.16f;
            if (sel) {
                const ImVec4 a = ImGui::ColorConvertU32ToFloat4(on);
                dl->AddRectFilled(ImVec2(cx - isz.x * 1.2f - 6, iy - 3),
                                  ImVec2(cx + isz.x * 1.2f + 6, iy + isz.y + 3),
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
                dl->AddText(font, label_px * 0.85f, ImVec2(bc.x - bs.x * 0.5f, bc.y - bs.y * 0.5f),
                            IM_COL32_WHITE, b);
            }
            ImGui::PopID();
        }
    }
    ImGui::End();
    return changed;
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
