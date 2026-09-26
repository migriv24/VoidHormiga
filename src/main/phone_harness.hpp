/* main/phone_harness.hpp — the phone front-end, driven and photographed on a desktop.
 *
 * The first APK was judged on a phone the day it shipped, and it failed in ways
 * no desktop run had shown (2026-09-25): the system bars covered the navigation
 * bar, the add button sat under the gesture strip, the database could not be
 * joined. None of it could be SEEN here. This is how it is seen now: the desktop
 * shell (`--phone`) takes a phone's screen, density and safe area, runs a script
 * of taps through the same ImGui input queue a finger feeds on Android, and
 * writes screenshots. So a phone change is looked at before it is shipped, and
 * a script that reproduces a bug keeps reproducing it.
 *
 *   voidhormiga --phone --phone-screen 412x915@1 --safe 24,32 --script s.txt <database>
 *
 * Script lines (coordinates in dp from the top-left of the screen):
 *   tap X Y        press and release there (two frames)
 *   move X Y       hover there without pressing (a desktop's menus open on hover)
 *   scroll DY      a mouse wheel where the finger last was (a canvas zooms)
 *   drag X0 Y0 X1 Y1   press, move over several frames, release
 *   type TEXT      characters into whatever field is active
 *   key enter|back
 *   wait N         N frames
 *   shot PATH      a PNG of the whole screen, safe area included
 *   quit
 * `#` starts a comment. */
#pragma once

#include "imgui.h"

#include <string>
#include <vector>

struct PhoneHarness {
    float w_dp = 412, h_dp = 880; // the screen, in dp
    float density = 1.0f;         // px per dp
    float safe_top = 0, safe_bottom = 0; // dp: the status bar, the gesture strip
    bool finger = true;           // a touch screen; false = a mouse (a desktop script)
    std::vector<std::string> script;
    std::string error;

    /* Reads --phone-screen / --safe / --script. False with `error` on a bad one. */
    bool parse(int argc, char** argv);
    int window_w() const { return (int)(w_dp * density + 0.5f); }
    int window_h() const { return (int)(h_dp * density + 0.5f); }

    /* Before ImGui::NewFrame (after the platform backend's): this frame's input. */
    void inject(ImGuiIO& io);
    /* After rendering, before the swap: screenshots, and whether to quit. */
    bool after_render(int fb_w, int fb_h);
    /* Paint what the system would draw in the safe area, so a screenshot shows it. */
    void draw_system_bars() const;

  private:
    std::size_t pc_ = 0;
    int wait_ = 0;
    int release_in_ = -1;        // frames until a held tap is released
    std::vector<ImVec2> drag_;   // positions still to visit, one a frame
    std::string pending_shot_;
    ImVec2 finger_{-1, -1};      // where the script's finger last touched
    bool quit_ = false;
};
