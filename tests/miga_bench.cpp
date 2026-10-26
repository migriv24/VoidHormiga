/* miga_bench.cpp — what opening and saving a .miga costs, phase by phase.
 *
 * A measurement, not a test (okf/concepts/platform/scale.md). Built on demand:
 *     cmake --build build --target hormiga_miga_bench
 *     hormiga_miga_bench big.miga [runs]
 *
 * Opens the bundle into a scratch working copy twice: the first time into an
 * empty folder (every asset written), the second into the folder the first left
 * (every asset already there), which is the case an unchanged reopen hits. Then
 * packs it back. Each phase the progress callback names is timed. */
#include "platform/miga.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>

namespace fs = std::filesystem;
using clk = std::chrono::steady_clock;

namespace {

struct PhaseClock {
    clk::time_point start = clk::now(), last = start;
    std::string phase;
    std::map<std::string, double> ms;
    std::vector<std::string> order;
    void on(double, const char* p) {
        const auto now = clk::now();
        if (p != phase) {
            if (!phase.empty()) ms[phase] += std::chrono::duration<double, std::milli>(now - last).count();
            else ms["(before the first report)"] += std::chrono::duration<double, std::milli>(now - start).count();
            last = now;
            phase = p;
            if (std::find(order.begin(), order.end(), phase) == order.end()) order.push_back(phase);
        }
    }
    void finish(const char* what) {
        const auto now = clk::now();
        if (!phase.empty()) ms[phase] += std::chrono::duration<double, std::milli>(now - last).count();
        const double total = std::chrono::duration<double, std::milli>(now - start).count();
        std::printf("%-28s %9.1f ms\n", what, total);
        for (const auto& p : order) std::printf("    %-24s %9.1f ms\n", p.c_str(), ms[p]);
    }
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: hormiga_miga_bench <bundle.miga>\n"; return 2; }
    const fs::path bundle = argv[1];
    const fs::path work = fs::temp_directory_path() / "hormiga-miga-bench";
    std::error_code ec;
    fs::remove_all(work, ec);
    fs::create_directories(work, ec);
    std::printf("%s, %.1f MB\n", bundle.filename().string().c_str(),
                (double)fs::file_size(bundle) / (1024.0 * 1024.0));

    std::string state;
    for (const char* label : {"open (empty working copy)", "open (assets already there)"}) {
        PhaseClock pc;
        auto r = hormiga::miga::open(bundle, work, work / "assets",
                                     [&](double f, const char* p) { pc.on(f, p); });
        pc.finish(label);
        if (!r.ok) { std::cerr << "open failed: " << r.error << "\n"; return 1; }
        std::printf("    assets written %d, kept %d, state %.1f MB\n", r.assets, r.assets_kept,
                    (double)r.state.size() / (1024.0 * 1024.0));
        state = std::move(r.state);
    }
    {
        PhaseClock pc;
        auto r = hormiga::miga::pack(state, work, work / "repacked.miga", "bench", work / "assets", {}, {},
                                     [&](double f, const char* p) { pc.on(f, p); });
        pc.finish("pack");
        if (!r.ok) { std::cerr << "pack failed: " << r.error << "\n"; return 1; }
    }
    fs::remove_all(work, ec);
    return 0;
}
