/* app/busy.cpp — see busy.hpp. */
#include "app/busy.hpp"
#include "app/app_internal.hpp" // the curtain itself is the app's: run_busy and its overlay

#include <cmath>

namespace hormiga {

Busy::~Busy() {
    if (worker_.joinable()) worker_.join(); // never leave a thread writing into a dead app
}

void Busy::start(BusyWork work, double now) {
    if (worker_.joinable()) worker_.join();
    done_ = false;
    frac_ = 0.0f;
    {
        std::lock_guard<std::mutex> g(mu_);
        worker_phase_.clear();
    }
    finish_ = nullptr;
    since_ = last_t_ = now;
    last_f_ = 0;
    rate_ = 0;
    progress = 0.0f;
    worker_ = std::thread([this, work = std::move(work)] {
        const BusyReport report = [this](double f, const char* phase) {
            frac_ = (float)std::fmin(1.0, std::fmax((double)frac_.load(), f)); // never backwards
            std::lock_guard<std::mutex> g(mu_);
            worker_phase_ = phase ? phase : "";
        };
        BusyFinish fin;
        try {
            fin = work(report);
        } catch (...) {
            fin = nullptr; // the finish reports nothing; the curtain still comes down
        }
        std::lock_guard<std::mutex> g(mu_);
        finish_ = std::move(fin);
        done_ = true;
    });
}

bool Busy::poll(double now) {
    if (!worker_.joinable()) return false;
    const float f = frac_.load();
    progress = f;
    {
        std::lock_guard<std::mutex> g(mu_);
        phase = worker_phase_;
    }
    /* The RATE, smoothed: r <- a*r + (1-a)*df/dt, with the weight of the past
     * set by how much time passed (a = exp(-dt/tau)), so the estimate does not
     * depend on the frame rate. tau = 1.5 s: long enough to iron out the jumps
     * between files, short enough to follow a real change of phase. */
    const double dt = now - last_t_;
    if (dt >= 0.2) {
        const double inst = (f - last_f_) / dt;
        const double a = std::exp(-dt / 1.5);
        rate_ = rate_ > 0 ? a * rate_ + (1 - a) * inst : inst;
        last_t_ = now;
        last_f_ = f;
    }
    if (!done_.load()) return true;
    worker_.join();
    BusyFinish fin;
    {
        std::lock_guard<std::mutex> g(mu_);
        fin = std::move(finish_);
    }
    label.clear();
    phase.clear();
    progress = -1.0f;
    if (fin) fin();
    return false;
}

std::string Busy::estimate(double now) const {
    const double elapsed = now - since_;
    const float f = frac_.load();
    if (elapsed < 2.0 || f < 0.1f || f >= 0.99f || rate_ <= 0) return {};
    const double left = (1.0 - f) / rate_;
    if (left < 3) return "a moment left";
    if (left < 60) {
        const int s = (int)(std::ceil(left / 5.0) * 5); // coarse: 5 s steps
        return "about " + std::to_string(s) + " seconds left";
    }
    return "about " + std::to_string((int)std::ceil(left / 60.0)) + " minutes left";
}

} // namespace hormiga

// ── the curtain, as the application draws it (moved from app.cpp, 2026-10-25) ──

/* Defer a heavy, blocking operation by ONE frame so the "working…" overlay
 * paints before the main thread stalls on it. The label shows immediately;
 * the action runs at the start of the next frame, then the overlay clears. */
void HormigaApp::run_busy(const std::string& label, std::function<void()> action) {
    busy.label = label;
    busy.progress = -1.0f;
    if (busy.action) // two asked for in one frame (a script): both, in order
        action = [first = std::move(busy.action), then = std::move(action)] { first(); then(); };
    busy.action = std::move(action);
}

/* The blocking-op overlay: a label + a spinner (busy_progress < 0) or a
 * progress bar, over a dimmed screen. Shown for deferred heavy ops (save /
 * open / new database). Async work (tile downloads, import jobs) shows its own
 * indicators in place; this is the modal "the app is working" curtain. */
void HormigaApp::draw_busy_overlay() {
    if (busy.label.empty()) return;
    const float busy_progress = busy.progress;
    const std::string& busy_label = busy.label;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowBgAlpha(0.55f);
    if (ImGui::Begin("##busydim", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        ImVec2 c(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                 vp->WorkPos.y + vp->WorkSize.y * 0.5f);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddRectFilled(ImVec2(c.x - 170, c.y - 40), ImVec2(c.x + 170, c.y + 40),
                          IM_COL32(30, 30, 34, 240), 10.0f);
        // a spinning arc for the indeterminate case
        if (busy_progress < 0) {
            float t = (float)ImGui::GetTime() * 6.0f;
            for (int i = 0; i < 12; ++i) {
                float a = t + i * 0.5236f;
                int alpha = 40 + (i * 215 / 12);
                dl->AddCircleFilled(ImVec2(c.x - 130 + std::cos(a) * 14,
                                           c.y + std::sin(a) * 14),
                                    2.6f, IM_COL32(255, 255, 255, alpha));
            }
        } else {
            dl->AddRect(ImVec2(c.x - 148, c.y + 14), ImVec2(c.x + 148, c.y + 26),
                        IM_COL32(255, 255, 255, 160), 3.0f);
            dl->AddRectFilled(
                ImVec2(c.x - 146, c.y + 16),
                ImVec2(c.x - 146 + 292 * std::clamp(busy_progress, 0.0f, 1.0f),
                       c.y + 24),
                IM_COL32(120, 190, 140, 230), 2.0f);
        }
        dl->AddText(ImVec2(c.x - 100, c.y - 22), IM_COL32(240, 240, 245, 255),
                    busy_label.c_str());
        // what is happening, in words, and how long, once that can be honest (app/busy.hpp)
        const std::string est = busy.estimate(ImGui::GetTime());
        const std::string sub = busy.phase + (busy.phase.empty() || est.empty() ? "" : "  -  ") + est;
        if (!sub.empty())
            dl->AddText(ImVec2(c.x - 148, c.y - 4), IM_COL32(190, 190, 200, 255), sub.c_str());
    }
    ImGui::End();
}
