/* working_set_test.cpp — the working set keeps the store exactly in step.
 *
 * A MODEL-BASED TEST. The reference is a Void Core holding the WHOLE database
 * (what Hormiga did before the store). The system under test is a store plus a
 * Core holding one cluster of it. Both receive the same random commands, chosen
 * among runes the working set has loaded; after every command the store must
 * export the same document the reference does. That includes the runes and
 * edges the working set never loaded: a removal, a rename and their undos all
 * reach those.
 *
 * Seeded, so a failure is a sequence that can be replayed: the failing step and
 * the commands before it are printed. Plus focused tests for the cases the
 * random walk may visit only rarely. */
#include "store/store.hpp"
#include "store/working_set.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp" // compile_commit: several commands as one batch
#include "voidmaiz/project.hpp"

#include "json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;
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

const char* kThing = R"({"glyph":"thing","label":"Thing","fields":["note"]})";

fs::path scratch(const std::string& name) {
    const fs::path d = fs::temp_directory_path() / "hormiga-ws-test";
    fs::create_directories(d);
    const fs::path p = d / name;
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm"}) fs::remove(p.string() + s, ec);
    return p;
}

/* Ids are minted at random by each core, so they differ between the two; and
 * the order of runes and of edges carries no meaning Hormiga depends on. */
json canonical(const std::string& text) {
    json j = json::parse(text);
    j.erase("_baseline");
    for (auto& m : j["mantles"]) {
        m.erase("id");
        if (m.contains("runes")) {
            for (auto& r : m["runes"]) r["spirit"].erase("id");
            std::sort(m["runes"].begin(), m["runes"].end(), [](const json& a, const json& b) {
                return a["spirit"]["name"].get<std::string>() < b["spirit"]["name"].get<std::string>();
            });
        }
        if (m.contains("layout") && m["layout"].contains("edges"))
            std::sort(m["layout"]["edges"].begin(), m["layout"]["edges"].end(),
                      [](const json& a, const json& b) { return a.dump() < b.dump(); });
    }
    return j;
}

/* 240 runes in six clusters, edges inside and ACROSS clusters (so a loaded
 * rune has edges to runes that are not loaded), and a small second mantle. */
std::string database() {
    maiz::Core c;
    c.register_glyph(kThing);
    c.dispatch("mantle new org");
    std::mt19937 rng(11);
    std::vector<std::string> cmds;
    for (int i = 0; i < 240; ++i) {
        const std::string n = "r" + std::to_string(i);
        cmds.push_back("rune new thing " + n);
        cmds.push_back("tag " + n + " +c1:" + std::to_string(i % 6));
    }
    for (int k = 0; k < 600; ++k) {
        const int a = (int)(rng() % 240), b = (int)(rng() % 240);
        if (a == b) continue;
        cmds.push_back("link r" + std::to_string(a) + " r" + std::to_string(b) + " --relation knows");
    }
    c.dispatch(maiz::compile_commit(cmds));
    c.dispatch("mantle new notes");
    c.dispatch("rune new thing n1");
    c.dispatch("rune new thing n2");
    c.dispatch("use org");
    return c.export_state();
}

struct Pair {
    maiz::Core ref;  // the whole database
    Store store;
    maiz::Core ws;   // what the working set holds
    WorkingSet set;
    Pair(const std::string& doc, const fs::path& db) : ref(doc), store(db), set(store, ws) {
        ref.register_glyph(kThing);
        ws.register_glyph(kThing);
        JsonStream in = JsonStream::from_text(doc);
        CHECK(store.import(in).ok);
    }
    std::vector<std::string> loaded(const char* mantle = "org") {
        maiz::ProjectOptions po;
        po.mantle = mantle;
        std::vector<std::string> out;
        for (const auto& n : maiz::project_scene(ws, po).nodes) out.push_back(n.name);
        return out;
    }
    std::vector<maiz::SceneWire> wires() {
        maiz::ProjectOptions po;
        po.mantle = "org";
        return maiz::project_scene(ws, po).wires;
    }
    // the same command to both; true if the store still says what the reference says
    bool through = false; // through WorkingSet::dispatch (the fast path), or straight to Core
    bool both(const std::string& cmd) {
        const bool a = ref.dispatch(cmd).ok, b = through ? set.dispatch(cmd).ok : ws.dispatch(cmd).ok;
        CHECK(a == b);
        CHECK(set.sync());
        return canonical(store.state()) == canonical(ref.export_state());
    }
};

Selection one_cluster() {
    Selection s = Selection::none();
    s.by_tag["org"] = {"c1:1"};
    s.whole.insert("notes");
    return s;
}

// ── focused ─────────────────────────────────────────────────────────────────

void loading_writes_nothing_and_holds_only_the_cluster() {
    Pair p(database(), scratch("load.store"));
    const std::string before = p.store.state();
    CHECK(p.set.load(one_cluster()));
    CHECK(p.loaded().size() == 40);
    CHECK(p.loaded("notes").size() == 2);
    CHECK(p.set.sync());
    CHECK(p.store.state() == before); // nothing changed, nothing written
    CHECK(p.set.stats.runes_written == 0);
}

void removing_a_rune_reaches_edges_that_were_never_loaded_and_undo_brings_them_back() {
    Pair p(database(), scratch("rm.store"));
    CHECK(p.set.load(one_cluster()));
    CHECK(p.both("rune rm r1")); // r1 has edges to runes in other clusters
    CHECK(p.both("undo"));       // ... and they come back, though Core never held them
    CHECK(p.set.stats.restores == 1);
    CHECK(p.both("redo"));
}

void a_rename_renames_every_edge_and_undo_renames_it_back() {
    Pair p(database(), scratch("rename.store"));
    CHECK(p.set.load(one_cluster()));
    CHECK(p.both("rune rename r7 seventh"));
    CHECK(p.set.stats.renames == 1);
    CHECK(p.both("undo"));
    CHECK(p.both("rune rename r13 thirteenth"));
    CHECK(p.both("set thirteenth note renamed-then-edited"));
}

void a_new_rune_may_not_take_an_unloaded_runes_name() {
    Pair p(database(), scratch("clash.store"));
    CHECK(p.set.load(one_cluster()));
    CHECK(p.set.name_taken("org", "r2"));  // in cluster 2: not loaded, still taken
    CHECK(!p.set.name_taken("org", "zz"));
    const std::string before = p.store.state();
    p.ws.dispatch("rune new thing r2"); // Core cannot see r2, so it allows this...
    CHECK(!p.set.sync());               // ...and the store refuses to overwrite it
    CHECK(p.set.stats.refused == 1);
    CHECK(canonical(p.store.state()) == canonical(before));
}

void removing_a_mantle_and_undoing_it_keeps_its_unloaded_runes() {
    Pair p(database(), scratch("mantle.store"));
    CHECK(p.set.load(one_cluster()));
    CHECK(p.both("mantle rm org"));
    CHECK(p.store.rune_count("org") == 0);
    CHECK(p.both("undo"));
    CHECK(p.store.rune_count("org") == 240); // all of them, not the 40 Core held
}

void a_second_working_set_sees_what_the_first_wrote() {
    Pair p(database(), scratch("second.store"));
    CHECK(p.set.load(one_cluster()));
    CHECK(p.both("set r1 note from-the-first"));
    CHECK(p.both("link r1 r7 --relation partners"));
    Selection s2 = Selection::none();
    s2.by_name["org"] = {"r1", "r7", "r2"};
    CHECK(p.set.load(s2));
    CHECK(p.loaded().size() == 3);
    CHECK(p.ws.dispatch("get r1 note").text().find("from-the-first") != std::string::npos);
    bool partners = false;
    for (const auto& w : p.wires()) partners = partners || (w.from == "r1" && w.to == "r7" && w.relation == "partners");
    CHECK(partners);
}

// ── the random walk ─────────────────────────────────────────────────────────

void walk(bool through) {
    Pair p(database(), scratch(through ? "walk-fast.store" : "walk.store"));
    p.through = through;
    CHECK(p.set.load(one_cluster()));
    std::mt19937 rng(20261027);
    std::vector<std::string> history;
    int fresh = 0;
    auto pick = [&](const std::vector<std::string>& v) { return v[rng() % v.size()]; };
    for (int step = 0; step < 400 && failures == 0; ++step) {
        const auto names = p.loaded();
        if (names.size() < 2) break;
        const std::string a = pick(names), b = pick(names);
        std::string cmd;
        switch (rng() % 15) {
        case 0: case 1: cmd = "set " + a + " note v" + std::to_string(step); break;
        case 2: cmd = "tag " + a + " +t" + std::to_string(step % 5); break;
        case 3: cmd = "tag " + a + " -t" + std::to_string(step % 5); break;
        case 4: {
            const std::string n = "new" + std::to_string(fresh++);
            cmd = maiz::compile_commit({"rune new thing " + n, "tag " + n + " +c1:1"});
            break;
        }
        case 5: cmd = "rune rm " + a; break;
        case 6: cmd = "rune rename " + a + " ren" + std::to_string(fresh++); break;
        case 7: if (a != b) cmd = "link " + a + " " + b + " --relation r" + std::to_string(step % 3); break;
        case 8: {
            const auto ws = p.wires();
            if (!ws.empty()) {
                const auto& w = ws[rng() % ws.size()];
                cmd = "unlink " + w.from + " " + w.to;
            }
            break;
        }
        case 9: if (a != b) cmd = "link " + a + " " + b + " --relation knows --weight " + std::to_string(step % 7); break;
        case 10: case 11: cmd = "undo"; break;
        case 12: cmd = "redo"; break;
        case 13: cmd = maiz::compile_commit({"use notes", "set n1 note w" + std::to_string(step), "use org"}); break;
        case 14: cmd = "config set walk.k v" + std::to_string(step); break;
        }
        if (cmd.empty()) continue;
        history.push_back(cmd);
        if (!p.both(cmd)) {
            ++failures;
            std::cerr << "FAIL the store diverged at step " << step << " after:\n";
            for (std::size_t i = history.size() > 12 ? history.size() - 12 : 0; i < history.size(); ++i)
                std::cerr << "    " << history[i] << "\n";
        }
    }
    std::cout << "    " << history.size() << " commands; " << p.set.stats.targeted << " targeted and "
              << p.set.stats.full << " full syncs, " << p.set.stats.renames << " renames, " << p.set.stats.restores
              << " restores, " << p.set.stats.runes_removed << " removals\n";
    CHECK(history.size() > 300);
    if (through) CHECK(p.set.stats.targeted > 150); // the fast path really ran, and was still right
}

void a_random_walk_keeps_the_store_equal_to_the_whole_database() { walk(false); }
void the_fast_path_keeps_it_equal_too() { walk(true); }

} // namespace

int main() {
    const struct {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"loading writes nothing and holds only the cluster", loading_writes_nothing_and_holds_only_the_cluster},
        {"removing a rune reaches unloaded edges, and undo brings them back",
         removing_a_rune_reaches_edges_that_were_never_loaded_and_undo_brings_them_back},
        {"a rename renames every edge, and undo renames it back",
         a_rename_renames_every_edge_and_undo_renames_it_back},
        {"a new rune may not take an unloaded rune's name", a_new_rune_may_not_take_an_unloaded_runes_name},
        {"removing a mantle and undoing it keeps its unloaded runes",
         removing_a_mantle_and_undoing_it_keeps_its_unloaded_runes},
        {"a second working set sees what the first wrote", a_second_working_set_sees_what_the_first_wrote},
        {"a random walk keeps the store equal to the whole database",
         a_random_walk_keeps_the_store_equal_to_the_whole_database},
        {"the fast path keeps it equal too", the_fast_path_keeps_it_equal_too},
    };
    for (const auto& t : tests) {
        const int before = failures;
        t.fn();
        std::cout << (failures == before ? "ok   " : "FAIL ") << t.name << "\n";
    }
    if (failures) std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
