/* open_probe.cpp — open a database the way Hormiga does, without a window,
 * and say what it cost.
 *
 *     hormiga_open_probe <bundle.miga | state.json> [--machine-mb 2048]
 *     hormiga_open_probe --synthetic 100000 [--machine-mb 2048]
 *
 * Streams the file into a store, clusters any mantle too large for the
 * budget, plans a working set for a machine of `--machine-mb` (default: the
 * smallest promised, 2 GB), loads it into Void Core, projects it, makes edits
 * through the dispatcher and writes them back, then checks the store has them.
 * Run under tools/lowmem to hold the whole process to a real cap
 * (okf/concepts/platform/store.md). Exit 0 only if every step held. */
#include "platform/process_memory.hpp"
#include "store/plan.hpp"
#include "store/store.hpp"
#include "store/working_set.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace fs = std::filesystem;
using namespace hormiga::store;
using clk = std::chrono::steady_clock;

namespace {

double since(clk::time_point t) { return std::chrono::duration<double>(clk::now() - t).count(); }
double mb(std::uint64_t b) { return (double)b / (1024.0 * 1024.0); }
std::uint64_t peak = 0;
void sample() { peak = std::max(peak, hormiga::memory::process_bytes()); }

/* A clustered, linked database written as text (a core is too slow to build one this size). */
void synthetic(const fs::path& p, int n) {
    std::ofstream o(p, std::ios::binary);
    std::mt19937 rng(3);
    o << R"({"version":1,"domains":{},"glyphs":{},"mantles":[{"id":"m1","name":"data","domain":null,"runes":[)";
    for (int i = 0; i < n; ++i) {
        o << (i ? "," : "") << R"({"content":{"display_name":"Record )" << i << R"(","note":")"
          << std::string(120, 'n') << R"("},"facets":{},"glyph":"contact","placement":null,"relations":[],)"
          << R"("spirit":{"id":"rune_)" << i << R"(","name":"rec-)" << i << R"("},"tags":["type:contact"]})";
    }
    o << R"(],"tags":{},"layout":{"edges":[)";
    const int group = 500;
    bool first = true;
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < 3; ++k) {
            const int j = rng() % 50 == 0 ? (int)(rng() % (unsigned)n) : (i / group) * group + (int)(rng() % group);
            if (j == i || j >= n) continue;
            o << (first ? "" : ",") << R"({"from":"rec-)" << i << R"(","to":"rec-)" << j
              << R"(","relation":"knows","weight":1,"directed":true})";
            first = false;
        }
    o << R"(]},"rules":[]},{"id":"m2","name":"settings","domain":null,"runes":[],"tags":{},"layout":{"edges":[]},)"
      << R"("rules":[]}],"config":{},"active":{"mantle":"data","domain":null},"bindings":[]})";
}

} // namespace

int main(int argc, char** argv) {
    fs::path src;
    std::uint64_t machine = 2048ull << 20;
    int synth = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--machine-mb" && i + 1 < argc) machine = std::stoull(argv[++i]) << 20;
        else if (a == "--synthetic" && i + 1 < argc) synth = std::stoi(argv[++i]);
        else src = a;
    }
    const fs::path dir = fs::temp_directory_path() / "hormiga-open-probe";
    fs::create_directories(dir);
    if (synth > 0) {
        src = dir / ("synthetic-" + std::to_string(synth) + ".json");
        if (!fs::exists(src)) synthetic(src, synth);
    }
    if (src.empty()) {
        std::fprintf(stderr, "usage: hormiga_open_probe <file> | --synthetic N  [--machine-mb M]\n");
        return 2;
    }
    const fs::path db = dir / "probe.store";
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm"}) fs::remove(db.string() + s, ec);

    const auto t0 = clk::now();
    const Budget budget = Budget::for_machine(machine);
    std::printf("machine %.0f MB: working-set budget %.0f MB, undo depth %d, %.1f MB of text\n", mb(machine),
                mb(budget.bytes), budget.undo_depth, mb(budget.text_bytes()));

    Store st(db);
    JsonStream in(src.string());
    const auto imp = st.import(in, [](double, const char*) { sample(); });
    sample();
    if (!imp.ok) {
        std::fprintf(stderr, "import failed: %s\n", imp.error.c_str());
        return 1;
    }
    std::printf("import   %6.1f s   %lld runes, %lld edges          process peak %6.0f MB\n", since(t0), imp.runes,
                imp.edges, mb(peak));

    const auto t1 = clk::now();
    for (const auto& m : st.mantles())
        if ((std::uint64_t)m.bytes > budget.text_bytes() / 8 && !st.clustered(m.name)) {
            const auto c = st.cluster(m.name, 2000, [](double, const char*) { sample(); });
            std::printf("cluster  %6.1f s   %s: %lld leaves, depth %d, Q %.3f   process peak %6.0f MB\n", since(t1),
                        m.name.c_str(), c.leaves, c.depth, c.modularity, mb(peak));
        }

    const auto t2 = clk::now();
    const Plan plan = hormiga::store::plan(st, budget);
    const double plan_s = since(t2);
    const auto t2b = clk::now();
    maiz::Core core;
    core.set_undo_depth(budget.undo_depth); // the undo frames are part of the budget
    WorkingSet ws(st, core);
    if (!ws.load(plan.sel)) {
        std::fprintf(stderr, "load failed: %s\n", ws.error().c_str());
        return 1;
    }
    const double load_s = since(t2b);
    sample();
    std::string active;
    long long most = -1;
    for (const auto& m : st.mantles())
        if (m.runes > most) {
            most = m.runes;
            active = m.name;
        }
    const auto t2c = clk::now();
    ws.dispatch("use " + active);
    maiz::Scene scene = maiz::project_scene(core);
    const double project_s = since(t2c);
    sample();
    std::printf("load     %6.1f s   (plan %.1f, load %.1f, project %.1f) %lld of %lld runes (%s), %.1f MB of text"
                "   process peak %6.0f MB\n",
                since(t2), plan_s, load_s, project_s, plan.runes, plan.total_runes, plan.partial ? "partial" : "whole",
                mb(plan.text_bytes), mb(peak));

    // edits through the dispatcher, written back one by one, and one undo
    const auto t3 = clk::now();
    if (scene.nodes.empty()) {
        std::fprintf(stderr, "nothing loaded to edit\n");
        return 1;
    }
    const std::string target = scene.nodes[scene.nodes.size() / 2].name;
    double dispatch_s = 0, sync_s = 0;
    for (int k = 0; k < 30; ++k) {
        const auto d0 = clk::now();
        ws.dispatch("set " + target + " note " + maiz::arg("probe edit " + std::to_string(k)));
        dispatch_s += since(d0);
        const auto s0 = clk::now();
        const bool synced = ws.sync();
        sync_s += since(s0);
        if (!synced) {
            std::fprintf(stderr, "sync failed: %s\n", ws.error().c_str());
            return 1;
        }
        sample();
    }
    core.dispatch("undo");
    ws.sync();
    scene = maiz::project_scene(core);
    sample();
    const double edit_ms = since(t3) * 1000.0 / 31.0;
    Selection one = Selection::none();
    one.by_name[active] = {target};
    const bool written = st.state(one).find("probe edit 28") != std::string::npos;
    std::printf("edits    %6.1f ms each: Core %.1f ms, write-back %.1f ms; stored: %s   process peak %6.0f MB\n",
                edit_ms, dispatch_s * 1000.0 / 30.0, sync_s * 1000.0 / 30.0, written ? "yes" : "NO", mb(peak));
    std::printf("total    %6.1f s\n", since(t0));
    return written ? 0 : 1;
}
