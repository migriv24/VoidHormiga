/* bundle_test.cpp — the .miga v4 file (store/bundle.hpp).
 *
 * Packing and opening give back the same database and the same pictures; a
 * v3 file still opens; what was removed is not packed; a failed pack leaves the
 * old file alone; and a packed database takes edits once opened. */
#include "sqlite3.h"
#include "store/bundle.hpp"
#include "store/store.hpp"
#include "store/working_set.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"

#include "json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

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

const fs::path kDir = fs::temp_directory_path() / "hormiga-bundle-test";

fs::path fresh(const std::string& name) {
    const fs::path p = kDir / name;
    std::error_code ec;
    fs::remove_all(p, ec);
    for (const char* s : {"-wal", "-shm", ".packing"}) fs::remove(p.string() + s, ec);
    return p;
}

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

void put(const fs::path& p, const std::string& bytes) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << bytes;
}

json canonical(const std::string& text) {
    json j = json::parse(text);
    j.erase("_baseline");
    return j;
}

std::string sample() {
    maiz::Core c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    c.dispatch("mantle new org");
    std::vector<std::string> cmds;
    for (int i = 0; i < 50; ++i) {
        cmds.push_back("rune new thing r" + std::to_string(i));
        cmds.push_back("set r" + std::to_string(i) + " note " + maiz::arg("ñ note " + std::to_string(i)));
    }
    for (int i = 0; i + 1 < 50; ++i) cmds.push_back("link r" + std::to_string(i) + " r" + std::to_string(i + 1));
    c.dispatch(maiz::compile_commit(cmds));
    return c.export_state();
}

/* A working store with three pictures, one in a subfolder and one binary. */
void working_copy(const fs::path& store, const fs::path& assets, const std::string& doc) {
    Store st(store);
    JsonStream in = JsonStream::from_text(doc);
    CHECK(st.import(in).ok);
    put(assets / "a.png", "PNG-ish bytes");
    put(assets / "flyers" / "b.jpg", std::string("\x00\xff\x10 binary\x00", 11));
    put(assets / "c.txt", "plain");
}

void a_file_is_known_by_its_first_bytes() {
    const fs::path v3 = fresh("kind.miga"), other = fresh("kind.txt"), v4 = fresh("kind4.miga");
    put(v3, "  \n{\"magic\":\"MIGA\"}");
    put(other, "hello");
    CHECK(bundle_kind(v3) == BundleKind::V3);
    CHECK(bundle_kind(other) == BundleKind::Other);
    CHECK(bundle_kind(fresh("nope.miga")) == BundleKind::Missing);
    {
        Store st(fresh("kind4.store"));
        CHECK(pack_v4(st, kDir, fresh("kind4-assets"), {}, v4, "k").ok);
    }
    CHECK(bundle_kind(v4) == BundleKind::V4);
}

void packing_and_opening_give_back_the_database_and_its_pictures() {
    const std::string doc = sample();
    const fs::path store = fresh("w.store"), assets = fresh("w-assets"), out = fresh("w.miga");
    working_copy(store, assets, doc);
    {
        Store st(store);
        put(kDir / "w-extra" / "resume.pdf", "a file outside the assets folder");
        const auto r = pack_v4(st, kDir, assets, {"w-extra/resume.pdf", "w-extra/missing.pdf"}, out, "Cats");
        CHECK(r.ok);
        CHECK(r.assets == 4); // three pictures and the file a rune names elsewhere; the missing one skipped
        CHECK(r.runes == 50);
    }
    const fs::path store2 = fresh("w2.store"), assets2 = fresh("w2-assets"), base2 = fresh("w2-base");
    const auto o = open_bundle(out, store2, base2, assets2);
    CHECK(o.ok);
    CHECK(o.name == "Cats");
    CHECK(o.assets == 4);
    CHECK(slurp(base2 / "w-extra" / "resume.pdf") == "a file outside the assets folder");
    CHECK(o.runes == 50);
    Store st2(store2);
    CHECK(canonical(st2.state()) == canonical(doc));
    CHECK(slurp(assets2 / "a.png") == "PNG-ish bytes");
    CHECK(slurp(assets2 / "flyers" / "b.jpg") == std::string("\x00\xff\x10 binary\x00", 11));
    CHECK(slurp(assets2 / "c.txt") == "plain");
}

void a_v3_file_still_opens() {
    const std::string doc = sample();
    const fs::path v3 = fresh("old.miga");
    put(v3, R"({"magic":"MIGA","version":3,"meta":{"name":"Old"},"assets":{"assets/x.txt":"eA=="},"state":)" + doc +
                "}");
    const fs::path store = fresh("old.store"), assets = fresh("old-assets");
    const auto r = open_bundle(v3, store, kDir, assets);
    CHECK(r.ok);
    CHECK(r.name == "Old");
    CHECK(r.runes == 50);
    CHECK(slurp(assets / "x.txt") == "x");
    Store st(store);
    CHECK(canonical(st.state()) == canonical(doc));
}

void what_was_removed_is_not_packed() {
    const std::string doc = sample();
    const fs::path store = fresh("rm.store"), assets = fresh("rm-assets"), out = fresh("rm.miga");
    working_copy(store, assets, doc);
    {
        Store st(store);
        maiz::Core core;
        core.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
        WorkingSet ws(st, core);
        CHECK(ws.load(Selection::all()));
        core.dispatch("use org");
        ws.dispatch("rune rm r10"); // soft-deleted in the store, with its two edges
        CHECK(ws.sync());
        CHECK(pack_v4(st, kDir, assets, {}, out, "x").ok);
    }
    const fs::path store2 = fresh("rm2.store");
    CHECK(open_bundle(out, store2, kDir, fresh("rm2-assets")).ok);
    Store st2(store2);
    const std::string text = st2.state();
    CHECK(text.find("\"r10\"") == std::string::npos);
    CHECK(st2.rune_count("org") == 49);
    // and no row is waiting for an undo that cannot happen any more
    maiz::Core core;
    WorkingSet ws(st2, core);
    CHECK(ws.load(Selection::all()));
    core.dispatch("use org");
    CHECK(!core.dispatch("cat r10").ok);
}

void a_failed_pack_leaves_the_old_file_alone() {
    const fs::path store = fresh("f.store"), assets = fresh("f-assets");
    working_copy(store, assets, sample());
    Store st(store);
    const fs::path nowhere = kDir / "no-such-folder" / "x.miga";
    const auto r = pack_v4(st, kDir, assets, {}, nowhere, "x");
    CHECK(!r.ok);
    CHECK(!r.error.empty());
    CHECK(!fs::exists(nowhere));
    CHECK(!fs::exists(nowhere.string() + ".packing"));
}

void an_opened_bundle_takes_edits() {
    const fs::path store = fresh("e.store"), assets = fresh("e-assets"), out = fresh("e.miga");
    working_copy(store, assets, sample());
    {
        Store st(store);
        CHECK(pack_v4(st, kDir, assets, {}, out, "e").ok);
    }
    const fs::path store2 = fresh("e2.store");
    CHECK(open_bundle(out, store2, kDir, fresh("e2-assets")).ok);
    Store st(store2);
    maiz::Core core;
    core.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note"]})");
    WorkingSet ws(st, core);
    CHECK(ws.load(Selection::all()));
    core.dispatch("use org");
    CHECK(ws.dispatch("set r3 note edited-after-open").ok);
    CHECK(ws.sync());
    CHECK(st.state().find("edited-after-open") != std::string::npos);
}

/* A bundle is a file from somewhere else: a picture named "../../x" or with an
 * absolute path is refused, and nothing is written outside the working copy. */
void a_bundle_cannot_write_outside_the_working_copy() {
    const fs::path base = fresh("jail-base"), assets = base / "assets", outside = fresh("jail-outside.txt");
    fs::create_directories(assets);
    // v3: the paths are in the JSON
    const fs::path v3 = fresh("evil.miga");
    put(v3, R"({"magic":"MIGA","version":3,"meta":{},"assets":{"assets/../../jail-outside.txt":"eA==",)"
            R"("../jail-outside.txt":"eA==","C:/Windows/evil.txt":"eA==","/etc/evil":"eA==",)"
            R"("assets/fine.txt":"eA=="},"state":)" + sample() + "}");
    const auto r = open_bundle(v3, fresh("evil.store"), base, assets);
    CHECK(r.ok);
    CHECK(r.refused == 4);
    CHECK(!fs::exists(outside));
    CHECK(slurp(assets / "fine.txt") == "x");
    // v4: the paths are rows
    const fs::path v4 = fresh("evil4.miga");
    {
        Store st(fresh("evil4-src.store"));
        CHECK(pack_v4(st, kDir, fresh("evil4-assets"), {}, v4, "x").ok);
    }
    { // hostile rows, written straight into the bundle file
        sqlite3* db = nullptr;
        CHECK(sqlite3_open(v4.string().c_str(), &db) == SQLITE_OK);
        CHECK(sqlite3_exec(db,
                           "INSERT INTO assets VALUES('assets/../../jail-outside.txt', x'78');"
                           "INSERT INTO assets VALUES('/etc/evil', x'78');"
                           "INSERT INTO assets VALUES('assets/also-fine.txt', x'79');",
                           nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(db);
    }
    const auto r4 = open_bundle(v4, fresh("evil4.store"), base, assets);
    CHECK(r4.ok);
    CHECK(r4.refused == 2);
    CHECK(!fs::exists(outside));
    CHECK(slurp(assets / "also-fine.txt") == "y");
}

} // namespace

int main() {
    fs::create_directories(kDir);
    const struct {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"a file is known by its first bytes", a_file_is_known_by_its_first_bytes},
        {"packing and opening give back the database and its pictures",
         packing_and_opening_give_back_the_database_and_its_pictures},
        {"a v3 file still opens", a_v3_file_still_opens},
        {"what was removed is not packed", what_was_removed_is_not_packed},
        {"a failed pack leaves the old file alone", a_failed_pack_leaves_the_old_file_alone},
        {"an opened bundle takes edits", an_opened_bundle_takes_edits},
        {"a bundle cannot write outside the working copy", a_bundle_cannot_write_outside_the_working_copy},
    };
    for (const auto& t : tests) {
        const int before = failures;
        t.fn();
        std::cout << (failures == before ? "ok   " : "FAIL ") << t.name << "\n";
    }
    if (failures) std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
