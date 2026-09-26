/* main/phone_harness.cpp — see phone_harness.hpp. */
#include "main/phone_harness.hpp"

#include "stb_image_write.h" // decls only: the one implementation lives in app.cpp

#include <GLFW/glfw3.h> // brings the system GL header: glReadPixels is GL 1.0

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

bool PhoneHarness::parse(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool more = i + 1 < argc;
        if (a == "--phone-screen" && more) {
            float w = 0, h = 0, d = 1;
            if (std::sscanf(argv[++i], "%fx%f@%f", &w, &h, &d) < 2 || w < 200 || h < 300 || d <= 0) {
                error = "--phone-screen takes WxH or WxH@DENSITY in dp, e.g. 412x915@2.625";
                return false;
            }
            w_dp = w;
            h_dp = h;
            density = d;
        } else if (a == "--safe" && more) {
            if (std::sscanf(argv[++i], "%f,%f", &safe_top, &safe_bottom) != 2) {
                error = "--safe takes TOP,BOTTOM in dp, e.g. 24,32";
                return false;
            }
        } else if (a == "--script" && more) {
            std::ifstream in(argv[++i]);
            if (!in) {
                error = std::string("cannot read the script ") + argv[i];
                return false;
            }
            for (std::string line; std::getline(in, line);) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
                if (!line.empty() && line[0] != '#') script.push_back(line);
            }
        }
    }
    return true;
}

void PhoneHarness::inject(ImGuiIO& io) {
    if (std::getenv("HORMIGA_SIM_TRACE"))
        std::fprintf(stderr, "sim: pos %.0f,%.0f down %d capture %d\n", io.MousePos.x, io.MousePos.y,
                     (int)io.MouseDown[0], (int)io.WantCaptureMouse);
    io.AddMouseSourceEvent(finger ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
    // THE FINGER STAYS WHERE IT TOUCHED. The GLFW backend feeds the desktop's
    // real cursor every frame; left alone it moved the pointer away between a
    // tap's press and release, and no tap ever clicked (found tracing, 2026-09-25).
    if (finger_.x >= 0) io.AddMousePosEvent(finger_.x, finger_.y);
    if (release_in_ >= 0 && release_in_-- == 0) io.AddMouseButtonEvent(0, false);
    if (!drag_.empty()) {
        finger_ = drag_.front();
        io.AddMousePosEvent(finger_.x, finger_.y);
        drag_.erase(drag_.begin());
        if (drag_.empty()) io.AddMouseButtonEvent(0, false);
        return;
    }
    if (release_in_ >= 0 || wait_-- > 0) return;
    while (pc_ < script.size()) {
        std::istringstream in(script[pc_++]);
        std::string op;
        in >> op;
        const float d = density;
        if (op == "tap") {
            float x = 0, y = 0;
            in >> x >> y;
            finger_ = ImVec2(x * d, y * d);
            io.AddMousePosEvent(finger_.x, finger_.y);
            io.AddMouseButtonEvent(0, true);
            release_in_ = 1; // held for one whole frame, as a real tap is
            return;
        }
        if (op == "move") { // a mouse hovering (menus open on hover); no press
            float x = 0, y = 0;
            in >> x >> y;
            finger_ = ImVec2(x * d, y * d);
            io.AddMousePosEvent(finger_.x, finger_.y);
            return;
        }
        if (op == "scroll") { // a mouse wheel at the finger's place: a canvas zooms
            float dy = 0;
            in >> dy;
            io.AddMouseWheelEvent(0.0f, dy);
            return;
        }
        if (op == "drag") {
            float x0, y0, x1, y1;
            in >> x0 >> y0 >> x1 >> y1;
            finger_ = ImVec2(x0 * d, y0 * d);
            io.AddMousePosEvent(finger_.x, finger_.y);
            io.AddMouseButtonEvent(0, true);
            for (int k = 1; k <= 12; ++k)
                drag_.push_back(ImVec2((x0 + (x1 - x0) * k / 12.0f) * d, (y0 + (y1 - y0) * k / 12.0f) * d));
            return;
        }
        if (op == "type") {
            std::string rest;
            std::getline(in, rest);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            io.AddInputCharactersUTF8(rest.c_str());
            return;
        }
        if (op == "key") {
            std::string k;
            in >> k;
            const ImGuiKey key = k == "back" ? ImGuiKey_AppBack : ImGuiKey_Enter;
            io.AddKeyEvent(key, true);
            io.AddKeyEvent(key, false);
            return;
        }
        if (op == "wait") {
            in >> wait_;
            return;
        }
        if (op == "shot") {
            in >> pending_shot_;
            return;
        }
        if (op == "quit") {
            quit_ = true;
            return;
        }
        std::fprintf(stderr, "phone script: unknown line '%s'\n", script[pc_ - 1].c_str());
    }
}

bool PhoneHarness::after_render(int fb_w, int fb_h) {
    if (!pending_shot_.empty()) {
        std::vector<unsigned char> px((std::size_t)fb_w * fb_h * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, fb_w, fb_h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        // GL's rows run bottom-up; a PNG's run top-down
        std::vector<unsigned char> flipped(px.size());
        const std::size_t row = (std::size_t)fb_w * 4;
        for (int y = 0; y < fb_h; ++y)
            std::memcpy(&flipped[(std::size_t)y * row], &px[(std::size_t)(fb_h - 1 - y) * row], row);
        if (!stbi_write_png(pending_shot_.c_str(), fb_w, fb_h, 4, flipped.data(), (int)row))
            std::fprintf(stderr, "phone script: could not write %s\n", pending_shot_.c_str());
        pending_shot_.clear();
    }
    return quit_;
}

void PhoneHarness::draw_system_bars() const {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float d = density;
    if (safe_top > 0) {
        dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + safe_top * d), IM_COL32(0, 0, 0, 90));
        dl->AddText(ImVec2(vp->Pos.x + 12 * d, vp->Pos.y + 4 * d), IM_COL32(255, 255, 255, 200), "12:00");
    }
    if (safe_bottom > 0) {
        const float y = vp->Pos.y + vp->Size.y - safe_bottom * d;
        dl->AddRectFilled(ImVec2(vp->Pos.x, y), ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                          IM_COL32(0, 0, 0, 60));
        const float cx = vp->Pos.x + vp->Size.x * 0.5f, cy = y + safe_bottom * d * 0.5f;
        dl->AddRectFilled(ImVec2(cx - 54 * d, cy - 2 * d), ImVec2(cx + 54 * d, cy + 2 * d),
                          IM_COL32(200, 200, 200, 220), 2 * d); // the gesture handle
    }
}
