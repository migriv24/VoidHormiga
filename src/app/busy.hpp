/* app/busy.hpp — the "working…" curtain, and the work behind it off the UI thread.
 *
 * Issue #1: opening a large database froze the window, because the open ran on
 * the main thread and no frame was drawn until it finished. Issue #3: a spinner
 * says nothing about how long. Both are answered here, by what the research on
 * waiting says a person needs (okf/concepts/platform/scale.md):
 *
 *  - Feedback within 0.1 s and a live window throughout (Nielsen's limits): the
 *    work runs on a worker and the frame keeps drawing.
 *  - A determinate bar for anything past a couple of seconds (Myers, CHI 1985:
 *    people prefer a percent-done indicator and work better with one), moving at
 *    the rate time does (the bundle layer weights it by measured cost per byte).
 *  - What is happening, in words ("Restoring pictures"), and an estimate only
 *    once one can be honest: after two seconds and a tenth of the work, rounded
 *    coarsely, from a smoothed rate so it does not jitter.
 *
 * While a task runs NOTHING ELSE IS DRAWN, so nothing can be clicked: the old
 * database is being replaced, and an edit made to it now would be lost silently.
 * That is the same promise the blocking version kept by accident, kept on
 * purpose. */
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace hormiga {

using BusyReport = std::function<void(double fraction, const char* phase)>;
using BusyFinish = std::function<void()>;
// Runs on the worker; returns what the MAIN thread does when it is done.
using BusyWork = std::function<BusyFinish(const BusyReport&)>;

struct Busy {
    std::string label;               // non-empty: the curtain is up
    float progress = -1.0f;          // < 0: a spinner
    std::string phase;               // the line under the label
    std::function<void()> action;    // a blocking op, run at the start of the next frame

    ~Busy();
    bool running() const { return worker_.joinable(); }
    /* Start `work` on a worker. `now` is the clock the caller draws with. */
    void start(BusyWork work, double now);
    /* Main thread, once a frame: refresh progress, and when the worker is done,
     * join it and run its finish. True while it is still running. */
    bool poll(double now);
    /* "about 20 seconds left", or "" when an estimate would not be honest yet. */
    std::string estimate(double now) const;

private:
    std::thread worker_;
    std::atomic<bool> done_{false};
    std::atomic<float> frac_{0.0f};
    mutable std::mutex mu_;
    std::string worker_phase_;
    BusyFinish finish_;
    double since_ = 0, last_t_ = 0;
    float last_f_ = 0;
    double rate_ = 0; // fraction per second, exponentially smoothed
};

} // namespace hormiga
