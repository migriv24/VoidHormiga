/* scale_bench.cpp — what one edit costs at N runes, phase by phase.
 *
 * Not a test (it asserts nothing and is not in ctest): a measurement, built on
 * demand with `cmake --build build --target hormiga_scale_bench`, for the
 * question okf/concepts/platform/scale.md asks — which costs grow with the
 * size of the database, and how fast. Point it at state documents of
 * increasing size and fit the exponents:
 *
 *     hormiga_scale_bench a.state.json b.state.json ...
 *
 * Each phase is timed in isolation, median of several runs, so that the cost
 * of an edit can be split into what Void Core does (dispatch, the undo frame),
 * what the projection does (export, parse, project) and what Hormiga adds. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

double median_ms(int runs, const std::function<void()>& f) {
    std::vector<double> t;
    for (int i = 0; i < runs; ++i) {
        const auto a = std::chrono::steady_clock::now();
        f();
        const auto b = std::chrono::steady_clock::now();
        t.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

} // namespace

int main(int argc, char** argv) {
    std::cout << "runes\tstate_KB\tload_ms\texport_ms\tproject_ms\tset_ms\tbatch1_ms\thistory_ms\tconfig_ms\n";
    for (int i = 1; i < argc; ++i) {
        std::ifstream in(argv[i], std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        const std::string state = ss.str();

        maiz::Core core(state);
        core.dispatch("use demo-org");
        const maiz::Scene sc = maiz::project_scene(core);
        const size_t n = sc.nodes.size();
        if (n == 0) { std::cerr << argv[i] << ": no runes in demo-org\n"; continue; }
        const std::string target = sc.nodes[n / 2].name;

        const double load = median_ms(3, [&] { maiz::Core c(state); });
        const double exp = median_ms(5, [&] { (void)core.export_state(); });
        const double proj = median_ms(5, [&] { (void)maiz::project_scene(core); });
        int k = 0;
        const double set = median_ms(9, [&] {
            core.dispatch("set " + target + " display_name v" + std::to_string(k++));
        });
        const double batch1 = median_ms(9, [&] {
            core.dispatch("batch " + maiz::arg("[\"set " + target + " display_name w" +
                                               std::to_string(k++) + "\"]"));
        });
        const double hist = median_ms(5, [&] { (void)core.dispatch("history"); });
        // a verb outside the undoable slice: no memento, so this isolates the snapshot's share
        const double cfg = median_ms(9, [&] { core.dispatch("config set bench.k " + std::to_string(k++)); });
        std::cout << n << "\t" << state.size() / 1024 << "\t" << load << "\t" << exp << "\t" << proj
                  << "\t" << set << "\t" << batch1 << "\t" << hist << "\t" << cfg << "\n";
    }
    return 0;
}
