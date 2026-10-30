/* plan_test.cpp — the budget, the clustering index, and the plan.
 *
 * The clustering is tested on a database with PLANTED communities (dense
 * inside a group, sparse across), so there is a right answer: the leaves must
 * recover the groups. The plan is tested against the promise it makes: a
 * database that fits is loaded whole; one that does not is loaded within the
 * budget, around the rune the person is looking at, with its measures. */
#include "store/plan.hpp"
#include "store/store.hpp"
#include "store/working_set.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/graph.hpp"
#include "voidmaiz/project.hpp"

#include <filesystem>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace hormiga::store;

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ++failures;                                                              \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                            \
    } while (0)

namespace {

constexpr std::uint64_t MB = 1024ull * 1024ull;

fs::path scratch(const std::string& name) {
    const fs::path d = fs::temp_directory_path() / "hormiga-plan-test";
    fs::create_directories(d);
    const fs::path p = d / name;
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm"}) fs::remove(p.string() + s, ec);
    return p;
}

/* `groups` groups of `size` runes: each rune links to 6 others in its group
 * and, rarely, to one outside. Plus a measure every rune has a value on. */
std::string planted(int groups, int size, std::map<std::string, int>* truth = nullptr) {
    maiz::Core c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    c.dispatch("mantle new org");
    CHECK(c.dispatch("glyph declare " + maiz::arg(R"({"glyph":"metric","label":"Metric","kind":"measure","fields":[]})")).ok);
    std::mt19937 rng(5);
    std::vector<std::string> cmds;
    const int n = groups * size;
    for (int i = 0; i < n; ++i) {
        cmds.push_back("rune new thing r" + std::to_string(i));
        cmds.push_back("set r" + std::to_string(i) + " note " + maiz::arg(std::string(200, 'x')));
        if (truth) (*truth)["r" + std::to_string(i)] = i / size;
    }
    cmds.push_back("rune new metric score");
    for (int i = 0; i < n; ++i) {
        const int g = i / size;
        for (int k = 0; k < 6; ++k) {
            const int j = g * size + (int)(rng() % (unsigned)size);
            if (j != i) cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string(j));
        }
        if (rng() % 20 == 0) cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string(rng() % (unsigned)n));
        cmds.push_back("link r" + std::to_string(i) + " score --weight " + std::to_string(i % 9));
    }
    CHECK(c.dispatch(maiz::compile_commit(cmds)).ok); // one failed command would roll the whole batch back
    c.dispatch("mantle new notes");
    c.dispatch("rune new thing note1");
    c.dispatch("use org");
    return c.export_state();
}

void import(Store& st, const std::string& doc) {
    JsonStream in = JsonStream::from_text(doc);
    CHECK(st.import(in).ok);
}

// ── the budget ──────────────────────────────────────────────────────────────

void the_budget_is_an_eighth_of_the_machine_within_bounds() {
    const auto two = Budget::for_machine(2048 * MB);
    CHECK(two.bytes == 256 * MB);
    CHECK(two.undo_depth == 4);
    CHECK(Budget::for_machine(512 * MB).bytes == 128 * MB);  // never below 128 MB
    CHECK(Budget::for_machine(64ull * 1024 * MB).bytes == 4096 * MB); // never above 4 GB
    CHECK(Budget::for_machine(32ull * 1024 * MB).undo_depth == 16);
    CHECK(Budget::for_machine(0).bytes == 256 * MB); // unknown: the smallest promised machine
    // 256 MB at depth 4 holds about 5.4 MB of text: (25 + 5.5 × 4) per byte
    CHECK(two.text_bytes() > 5 * MB && two.text_bytes() < 6 * MB);
}

// ── the clustering ──────────────────────────────────────────────────────────

void clustering_recovers_planted_groups() {
    std::map<std::string, int> truth;
    Store st(scratch("cluster.store"));
    import(st, planted(8, 60, &truth));
    CHECK(!st.clustered("org"));
    const auto stats = st.cluster("org", 100);
    CHECK(stats.ok);
    CHECK(stats.runes == 481);
    CHECK(stats.always == 1); // the measure
    CHECK(st.clustered("org"));
    std::vector<std::uint32_t> a, b;
    std::map<std::string, std::uint32_t> ids;
    for (const auto& [name, group] : truth) {
        const std::string path = st.cluster_of("org", name);
        CHECK(!path.empty());
        a.push_back((std::uint32_t)group);
        b.push_back(ids.emplace(path, (std::uint32_t)ids.size()).first->second);
    }
    const double nmi = maiz::graph::nmi(a, b);
    std::cout << "    " << stats.leaves << " leaves, NMI with the planted groups " << nmi << "\n";
    CHECK(nmi > 0.9);
    CHECK(st.cluster_of("org", "score") == "!");
    // every leaf at or under the target
    for (const auto& l : st.leaves("org")) CHECK(l.runes <= 100);
}

void the_same_database_clusters_the_same_way_twice() {
    const std::string doc = planted(6, 50);
    Store a(scratch("twice-a.store")), b(scratch("twice-b.store"));
    import(a, doc);
    import(b, doc);
    a.cluster("org", 80);
    b.cluster("org", 80);
    bool same = true;
    for (int i = 0; i < 300; ++i)
        same = same && a.cluster_of("org", "r" + std::to_string(i)) == b.cluster_of("org", "r" + std::to_string(i));
    CHECK(same); // a device that clusters again gets the same leaves
}

// ── the plan ────────────────────────────────────────────────────────────────

void a_database_that_fits_is_loaded_whole() {
    Store st(scratch("fits.store"));
    import(st, planted(4, 20));
    const Plan p = plan(st, Budget::for_machine(2048 * MB));
    CHECK(!p.partial);
    CHECK(p.sel.everything);
    CHECK(p.runes == p.total_runes);
}

void a_database_that_does_not_fit_is_loaded_within_the_budget() {
    Store st(scratch("big.store"));
    import(st, planted(8, 60));
    st.cluster("org", 60);
    Budget tiny;
    tiny.bytes = 8 * MB; // about 170 KB of text: a few leaves, not the database
    tiny.undo_depth = 4;
    const Plan p = plan(st, tiny);
    CHECK(p.partial);
    CHECK(p.sel.whole.count("notes")); // small mantles whole
    CHECK(p.sel.by_cluster.count("org"));
    CHECK(p.runes < p.total_runes);
    const std::string text = st.state(p.sel);
    std::cout << "    budget " << tiny.text_bytes() / 1024 << " KB of text; the plan's state is "
              << text.size() / 1024 << " KB, " << p.runes << " of " << p.total_runes << " runes\n";
    CHECK(text.size() <= tiny.text_bytes() * 5 / 4); // the estimate holds, within a quarter

    maiz::Core core;
    WorkingSet ws(st, core);
    CHECK(ws.load(p.sel));
    maiz::ProjectOptions po;
    po.mantle = "org";
    const auto scene = maiz::project_scene(core, po);
    bool has_score = false;
    for (const auto& n : scene.nodes) has_score = has_score || n.name == "score";
    CHECK(has_score); // the measure comes with any cluster
    std::size_t values = 0;
    for (const auto& w : scene.wires) values += w.to == "score";
    CHECK(values + 1 == scene.nodes.size()); // every loaded rune's value edge is there
}

void the_rune_in_focus_is_always_in_the_plan() {
    Store st(scratch("focus.store"));
    import(st, planted(8, 60));
    st.cluster("org", 60);
    Budget tiny;
    tiny.bytes = 2 * MB;
    for (const char* who : {"r5", "r250", "r479"}) {
        const Plan p = plan(st, tiny, "org", who);
        maiz::Core core;
        WorkingSet ws(st, core);
        CHECK(ws.load(p.sel));
        CHECK(core.dispatch(std::string("cat ") + who).ok);
        CHECK(p.focus == st.cluster_of("org", who));
    }
}

void a_large_mantle_with_no_clustering_loads_its_first_runes() {
    Store st(scratch("pages.store"));
    import(st, planted(8, 60));
    Budget tiny;
    tiny.bytes = 2 * MB;
    const Plan p = plan(st, tiny);
    CHECK(p.partial);
    CHECK(p.sel.by_name.count("org"));
    const auto& names = p.sel.by_name.at("org");
    CHECK(!names.empty() && names.front() == "r0");
    CHECK(st.state(p.sel).size() <= tiny.text_bytes() * 5 / 4);
}

} // namespace

int main() {
    const struct {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"the budget is an eighth of the machine, within bounds", the_budget_is_an_eighth_of_the_machine_within_bounds},
        {"clustering recovers planted groups", clustering_recovers_planted_groups},
        {"the same database clusters the same way twice", the_same_database_clusters_the_same_way_twice},
        {"a database that fits is loaded whole", a_database_that_fits_is_loaded_whole},
        {"a database that does not fit is loaded within the budget",
         a_database_that_does_not_fit_is_loaded_within_the_budget},
        {"the rune in focus is always in the plan", the_rune_in_focus_is_always_in_the_plan},
        {"a large mantle with no clustering loads its first runes",
         a_large_mantle_with_no_clustering_loads_its_first_runes},
    };
    for (const auto& t : tests) {
        const int before = failures;
        t.fn();
        std::cout << (failures == before ? "ok   " : "FAIL ") << t.name << "\n";
    }
    if (failures) std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
