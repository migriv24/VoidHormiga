/* phone/nav.hpp — a phone's chrome: the bottom navigation bar, the app bar, the
 * system back, and a stack of screens per destination.
 *
 * The author (2026-09-23): *"something like different screens like on most apps
 * we have a navigation bar instead, where we navigate to different 'screens'
 * ... Right now if we look at the interaction combinators demo, it doesn't do
 * this at all, it uses the 'file, edit, view' thing for a mobile app. that's
 * fine for a demo, but for an actual mobile application, we need a navigation
 * bar and real mobile features."* And: *"a navigation bar (or at least, the
 * core concept of one) belongs to maiz."*
 *
 * So this file is WRITTEN TO BE LIFTED. It knows nothing about Hormiga: ImGui
 * in, a tapped index out, no dispatcher, no model. When Void Maiz takes the
 * concept (asked 2026-09-23), this header is deleted and the four call sites in
 * phone.cpp change their namespace. Until then it lives here, and not in
 * `ui/`, because `phone/` may not include desktop panels (check_layering.py).
 *
 * THE ONE TRICK: both bars RESERVE their edge of the main viewport, the way
 * ImGui's own main menu bar reserves the top (`BeginViewportSideBar`). Every
 * window placed after them reads a smaller `WorkSize` — including Void Maiz's
 * FAB and snackbar, which anchor to the work area's bottom corner — so they sit
 * above the navigation bar without either of them knowing it exists. (The work
 * area updates on the next frame, which is exactly how the menu bar behaves.)
 */
#pragma once

#include "imgui.h"

#include <string>
#include <vector>

namespace hormiga::phone {

/* ── the bottom navigation bar ──────────────────────────────────────────────
 * Three to five destinations, each an icon over a short label, the current one
 * drawn in the accent colour. The platform convention (Material's navigation
 * bar, iOS's tab bar), and where a thumb reaches on a tall phone. */
struct NavItem {
    const char* icon;  // a Font Awesome codepoint string
    const char* label; // one word
    int badge = 0;     // a count drawn on the icon (0 = none)
};

float nav_bar_height();

/* Call FIRST in the frame, before any other window. Returns true the frame the
 * selection changes (a second tap on the current item also returns true, which
 * is the platform's "back to the top of this destination"). */
bool nav_bar(const std::vector<NavItem>& items, int& current);

/* The same bar with a LOCKED CENTRE BUTTON between its two halves (the author,
 * 2026-09-25: the bar is the person's to arrange, except one button in the
 * middle that always opens everything). `items` are the slots, left to right;
 * the centre sits after the first half. `current` is a slot index, or -1 when
 * the screen on show is not on the bar. Returns the slot tapped, kCentreTapped,
 * or -1. `centre_on` draws the centre as the current destination. */
inline constexpr int kCentreTapped = -2;
int nav_bar_centred(const std::vector<NavItem>& items, int current, bool centre_on);

/* The centre button's face: an ant, drawn (no icon font has one), in `col` on
 * a disc of `bg`. Also the gallery's header, so the two read as one thing. */
void draw_ant_button(ImDrawList* dl, ImVec2 centre, float radius, ImU32 bg, ImU32 col);

/* ── the app bar ────────────────────────────────────────────────────────────
 * A back chevron when `can_back`, then a title. Returns true when back is
 * tapped. Between begin and end the caller may draw trailing items on the same
 * line (`ImGui::SameLine` first). Always pair with end_app_bar. */
bool begin_app_bar(const char* title, bool can_back);
void end_app_bar();

/* ── the system back ───────────────────────────────────────────────────────
 * Escape, the mouse's back button, or Android's Back key (the android backend
 * reports AKEYCODE_BACK as ImGuiKey_AppBack) — and never while a text field has
 * the keyboard, where Escape means "stop typing". */
bool back_pressed();

/* ── a stack of screens per destination ─────────────────────────────────────
 * Switching destinations and coming back keeps your place, as every phone
 * does. A route is a string the host parses ("list", "detail:maria-3fa9-1"). */
struct Stack {
    std::vector<std::string> routes{"list"};
    const std::string& top() const { return routes.back(); }
    bool can_pop() const { return routes.size() > 1; }
    void push(std::string r) { routes.push_back(std::move(r)); }
    bool pop() {
        if (!can_pop()) return false;
        routes.pop_back();
        return true;
    }
    void reset() { routes.assign(1, "list"); }
};

} // namespace hormiga::phone
