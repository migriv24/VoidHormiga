/* store_test.cpp — unit tests of the store (okf/concepts/platform/store.md).
 *
 * One function per behaviour, each named for what it promises. The reader is
 * tested on its own first, at a buffer of a few bytes so that every token
 * crosses a refill; then the store: the round trip against a live Void Core,
 * the .miga v3 envelope, selections, an import that fails halfway, and the
 * promise the whole design rests on, that importing four times the data does
 * not take four times the memory. */
#include "platform/process_memory.hpp"
#include "store/json_stream.hpp"
#include "store/store.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

namespace fs = std::filesystem;
using nlohmann::json;
using hormiga::store::JsonStream;
using hormiga::store::Selection;
using hormiga::store::Store;

static int failures = 0;
#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            ++failures;                                                              \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                            \
    } while (0)

namespace {

fs::path scratch(const std::string& name) {
    const fs::path d = fs::temp_directory_path() / "hormiga-store-test";
    fs::create_directories(d);
    const fs::path p = d / name;
    std::error_code ec;
    fs::remove(p, ec);
    fs::remove(p.string() + "-wal", ec);
    fs::remove(p.string() + "-shm", ec);
    return p;
}

void write_file(const fs::path& p, const std::string& text) {
    std::ofstream(p, std::ios::binary) << text;
}

// a state document as Void Core sees it: parsed, without the baseline the store does not keep
json canonical(const std::string& text) {
    json j = json::parse(text);
    j.erase("_baseline");
    return j;
}

/* A document with every shape the store has to carry: several mantles (one
 * empty), tags, quantities, a measure, value edges, an undirected edge, unicode,
 * config and a declared glyph. Built through a real core, so it is Core's own
 * format and not this file's idea of it. */
std::string sample_state() {
    maiz::Core c;
    c.register_glyph(R"({"glyph":"thing","label":"Thing","fields":["note","title"]})");
    c.register_glyph(R"({"glyph":"metric","label":"Metric","kind":"measure","fields":[]})");
    c.dispatch("mantle new org");
    for (int i = 0; i < 40; ++i) {
        const std::string n = "t" + std::to_string(i);
        c.dispatch("rune new thing " + n);
        c.dispatch("set " + n + " note " + maiz::arg("note " + std::to_string(i) + " — café ☕ \"quoted\"\n2nd line"));
        c.dispatch("tag " + n + " +c1:" + std::to_string(i % 4) + " +type:thing");
    }
    for (int i = 0; i + 1 < 40; ++i)
        c.dispatch("link t" + std::to_string(i) + " t" + std::to_string(i + 1) + " --relation next");
    c.dispatch("link t0 t5 --relation knows --undirected");
    c.dispatch("rune new metric revenue");
    c.dispatch("measure revenue --level ratio --unit USD");
    c.dispatch("link t3 revenue --weight 1234.5");
    c.dispatch("mantle new notes");
    c.dispatch("rune new thing n1");
    c.dispatch("mantle new empty");
    c.dispatch("config set org.name " + maiz::arg("Ñandú & Co"));
    c.dispatch("use org");
    return c.export_state();
}

// ── the reader ────────────────────────────────────────────────────────────

void strings_decode_escapes_and_surrogate_pairs() {
    JsonStream s = JsonStream::from_text(R"("a\"b\\c\/d\n\té😀")");
    std::string out;
    CHECK(s.string(out));
    CHECK(out == "a\"b\\c/d\n\t\xC3\xA9\xF0\x9F\x98\x80");
}

void capture_keeps_the_exact_bytes() {
    const std::string v = "{ \"a\" : [1, 2.5e3, true, null, \"x}]\\\"\"] , \"b\":{} }";
    JsonStream s = JsonStream::from_text("[" + v + ", 7]");
    std::string got;
    CHECK(s.expect('['));
    CHECK(s.capture(got));
    CHECK(got == v);
    CHECK(s.eat(','));
    CHECK(s.capture(got) && got == "7");
    CHECK(s.expect(']'));
}

void a_tiny_buffer_reads_the_same_as_a_large_one() {
    const std::string doc = sample_state();
    const fs::path p = scratch("tiny.json");
    write_file(p, doc);
    for (const std::size_t buffer : {std::size_t(1), std::size_t(3), std::size_t(7), std::size_t(4096)}) {
        JsonStream s(p.string(), buffer);
        std::string got;
        CHECK(s.capture(got));
        CHECK(got == doc);
        CHECK(s.offset() == doc.size());
    }
}

void objects_and_arrays_walk_in_order() {
    JsonStream s = JsonStream::from_text(R"({"x": [1, {"y": "z"}, []], "w": -0.5})");
    std::vector<std::string> keys, items;
    CHECK(s.object([&](const std::string& k) {
        keys.push_back(k);
        if (k == "x")
            return s.array([&] {
                std::string v;
                const bool ok = s.capture(v);
                items.push_back(v);
                return ok;
            });
        return s.skip();
    }));
    CHECK((keys == std::vector<std::string>{"x", "w"}));
    CHECK((items == std::vector<std::string>{"1", "{\"y\": \"z\"}", "[]"}));
}

void a_broken_document_says_where() {
    JsonStream s = JsonStream::from_text(R"({"a": "never closed)");
    const bool ok = s.object([&](const std::string&) { return s.skip(); });
    CHECK(!ok);
    CHECK(!s.ok());
    CHECK(s.error().find("never closed") != std::string::npos);
    CHECK(s.error().find("at byte") != std::string::npos);
}

void a_long_string_streams_in_pieces() {
    const std::string big(300000, 'Q');
    JsonStream s = JsonStream::from_text("\"" + big + "\"");
    std::size_t total = 0, pieces = 0;
    CHECK(s.string_chunks([&](std::string_view c) {
        total += c.size();
        ++pieces;
    }));
    CHECK(total == big.size());
    CHECK(pieces > 1); // never one 300 KB string in memory at once
}

// ── the store ─────────────────────────────────────────────────────────────

void a_document_comes_back_as_it_went_in() {
    const std::string doc = sample_state();
    const fs::path src = scratch("in.json"), db = scratch("round.store");
    write_file(src, doc);
    Store st(db);
    CHECK(st.ok());
    JsonStream in(src.string());
    const auto stats = st.import(in);
    CHECK(stats.ok);
    CHECK(stats.mantles == 3);
    CHECK(stats.runes == 42);
    CHECK(stats.edges == 41);
    CHECK(!stats.envelope);
    CHECK(canonical(st.state()) == canonical(doc));

    // and Void Core reads it back to the same document
    maiz::Core again(st.state());
    CHECK(canonical(again.export_state()) == canonical(doc));
    CHECK(st.rune_count("org") == 40 + 1);
    CHECK(st.rune_count() == 42);
}

void the_store_survives_being_reopened() {
    const std::string doc = sample_state();
    const fs::path src = scratch("reopen.json"), db = scratch("reopen.store");
    write_file(src, doc);
    {
        Store st(db);
        JsonStream in(src.string());
        CHECK(st.import(in).ok);
    }
    Store st(db);
    CHECK(st.ok());
    CHECK(canonical(st.state()) == canonical(doc));
}

void a_member_that_was_absent_stays_absent() {
    const std::string doc = R"({"version":1,"mantles":[{"id":"m1","name":"bare"},)"
                            R"({"id":"m2","name":"no-edges","runes":[],"layout":{"zoom":2}}],"config":{}})";
    const fs::path src = scratch("absent.json"), db = scratch("absent.store");
    write_file(src, doc);
    Store st(db);
    JsonStream in(src.string());
    CHECK(st.import(in).ok);
    CHECK(json::parse(st.state()) == json::parse(doc));
}

void a_v3_bundle_imports_its_state_and_pictures() {
    const std::string doc = sample_state();
    // "hello, picture" in base64
    const std::string env = R"({"magic":"MIGA","version":3,"meta":{"name":"Cats","app":"hormiga"},)"
                            R"("rebuild":{},"assets":{"assets/a.txt":"aGVsbG8sIHBpY3R1cmU="},"state":)" +
                            doc + "}";
    const fs::path src = scratch("bundle.miga"), db = scratch("bundle.store");
    write_file(src, env);
    Store st(db);
    JsonStream in(src.string(), 5); // tiny buffer: the base64 crosses refills
    std::string path, bytes;
    const auto stats = st.import(in, {}, [&](const std::string& p, const std::string& b) {
        path = p;
        bytes = b;
    });
    CHECK(stats.ok);
    CHECK(stats.envelope);
    CHECK(stats.bundle_name == "Cats");
    CHECK(stats.assets == 1);
    CHECK(path == "assets/a.txt");
    CHECK(bytes == "hello, picture");
    CHECK(canonical(st.state()) == canonical(doc));
}

void a_failed_import_leaves_the_store_as_it_was() {
    const std::string doc = sample_state();
    const fs::path good = scratch("good.json"), bad = scratch("bad.json"), db = scratch("atomic.store");
    write_file(good, doc);
    write_file(bad, doc.substr(0, doc.size() / 2)); // cut off in the middle of the runes
    Store st(db);
    {
        JsonStream in(good.string());
        CHECK(st.import(in).ok);
    }
    JsonStream in(bad.string());
    const auto stats = st.import(in);
    CHECK(!stats.ok);
    CHECK(!stats.error.empty());
    CHECK(canonical(st.state()) == canonical(doc)); // the first document, whole
}

void a_selection_holds_its_runes_and_only_their_edges() {
    const std::string doc = sample_state();
    const fs::path src = scratch("sel.json"), db = scratch("sel.store");
    write_file(src, doc);
    Store st(db);
    JsonStream in(src.string());
    CHECK(st.import(in).ok);

    Selection sel = Selection::none();
    sel.by_tag["org"] = {"c1:1"};        // t1, t5, t9, ...
    sel.by_name["org"] = {"t2"};         // and one more by name
    sel.whole.insert("notes");
    const json part = json::parse(st.state(sel));

    std::set<std::string> names;
    std::size_t edges = 0;
    for (const auto& m : part["mantles"]) {
        if (m["name"] == "org") {
            for (const auto& r : m["runes"]) names.insert(r["spirit"]["name"].get<std::string>());
            for (const auto& e : m["layout"]["edges"]) {
                ++edges;
                CHECK(names.count(e["from"].get<std::string>()) == 1);
                CHECK(names.count(e["to"].get<std::string>()) == 1);
            }
        }
        if (m["name"] == "notes") CHECK(m["runes"].size() == 1);
        if (m["name"] == "empty") CHECK(m["runes"].empty());
    }
    CHECK(names.size() == 11); // ten with c1:1, and t2
    CHECK(names.count("t1") && names.count("t2") && names.count("t5") && !names.count("t0"));
    CHECK(edges == 1); // of the `next` chain only t1->t2 has both ends loaded; t0-t5 and t3->revenue do not
    // the part is a state Core opens, and a mantle not chosen is still there, empty
    maiz::Core c(part.dump());
    maiz::ProjectOptions po;
    po.mantle = "org";
    CHECK(maiz::project_scene(c, po).nodes.size() == 11);
    CHECK(c.dispatch("mantles").text().find("empty") != std::string::npos);
}

/* Synthetic, large, and written as text: a core is too slow to build 10^5 runes. */
void write_big_state(const fs::path& p, int runes) {
    std::ofstream o(p, std::ios::binary);
    o << R"({"version":1,"domains":{},"mantles":[{"id":"m","name":"big","domain":null,"runes":[)";
    std::mt19937 rng(7);
    for (int i = 0; i < runes; ++i) {
        if (i) o << ",";
        o << R"({"content":{"display_name":"Organization number )" << i << R"(","geo":"45.)" << rng() % 100000
          << R"(,-122.)" << rng() % 100000 << R"("},"facets":{},"glyph":"organization","placement":null,)"
          << R"("relations":[],"spirit":{"id":"rune_)" << i << R"(","name":"org-)" << i
          << R"("},"tags":["type:organization","c1:)" << i % 97 << R"(","city:x)" << i % 13 << R"("]})";
    }
    o << R"(],"tags":{},"layout":{"edges":[)";
    for (int i = 0; i + 1 < runes; ++i) {
        if (i) o << ",";
        o << R"({"from":"org-)" << i << R"(","to":"org-)" << (i * 7 + 1) % runes
          << R"(","relation":"partners","weight":1,"directed":true})";
    }
    o << R"(]},"rules":[]}],"config":{},"active":{"mantle":"big","domain":null},"bindings":[]})";
}

struct Run {
    double peak_mb = 0;
    double seconds = 0;
};

Run import_measured(const fs::path& src, const fs::path& db) {
    Store st(db);
    const auto base = hormiga::memory::process_bytes();
    std::uint64_t peak = base;
    const auto t0 = std::chrono::steady_clock::now();
    JsonStream in(src.string());
    const auto stats = st.import(in, [&](double, const char*) {
        peak = std::max(peak, hormiga::memory::process_bytes());
    });
    CHECK(stats.ok);
    peak = std::max(peak, hormiga::memory::process_bytes());
    Run r;
    r.peak_mb = (double)(peak - std::min(peak, base)) / (1024.0 * 1024.0);
    r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return r;
}

/* THE PROMISE: memory does not grow with the file. Four times the runes, and
 * the import's extra memory stays under a constant that a 2 GB device can
 * spare. Time is allowed to grow linearly (with a generous margin), never
 * quadratically. */
void importing_four_times_the_data_does_not_take_four_times_the_memory() {
    const fs::path small = scratch("n20k.json"), large = scratch("n80k.json");
    write_big_state(small, 20000);
    write_big_state(large, 80000);
    const Run a = import_measured(small, scratch("n20k.store"));
    const Run b = import_measured(large, scratch("n80k.store"));
    std::cout << "    20k runes: +" << a.peak_mb << " MB, " << a.seconds << " s;  80k runes: +" << b.peak_mb
              << " MB, " << b.seconds << " s  (file " << fs::file_size(large) / (1024 * 1024) << " MB)\n";
    CHECK(b.peak_mb < 64.0);                // a constant, far below the 2 GB device's budget
    CHECK(b.peak_mb < a.peak_mb * 2 + 16);  // and not proportional to the file
    CHECK(b.seconds < a.seconds * 10 + 1);  // linear (4x) with room for a noisy machine, not quadratic (16x)
    Store st(fs::temp_directory_path() / "hormiga-store-test" / "n80k.store");
    CHECK(st.rune_count("big") == 80000);
}

/* A SELECTION COSTS WHAT IT LOADS. The same 200 runes out of a store four
 * times larger take about as long: the query planner once walked the whole
 * mantle for them (0.9 s at 5×10^5) and once rescanned them per edge (40 s),
 * and neither showed up as anything but slowness. Runs after the import test,
 * whose stores it reuses. */
void a_selection_costs_what_it_loads_not_what_the_mantle_holds() {
    auto timed = [](const char* file) {
        Store st(fs::temp_directory_path() / "hormiga-store-test" / file);
        Selection sel = Selection::none();
        for (int i = 0; i < 200; ++i) sel.by_name["big"].push_back("org-" + std::to_string(i * 37));
        double best = 1e9;
        std::size_t bytes = 0;
        for (int k = 0; k < 3; ++k) { // best of three: the first warms the cache
            const auto t0 = std::chrono::steady_clock::now();
            bytes = st.state(sel).size();
            best = std::min(best, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count());
        }
        return std::make_pair(best, bytes);
    };
    const auto [a, abytes] = timed("n20k.store");
    const auto [b, bbytes] = timed("n80k.store");
    std::cout << "    200 runes from 20k: " << a * 1000 << " ms;  from 80k: " << b * 1000 << " ms\n";
    CHECK(abytes > 200 * 200 && bbytes > 200 * 200); // the runes are really there
    CHECK(b < a * 2.5 + 0.005);                     // not 4x: the mantle's size does not enter
}

} // namespace

int main() {
    const struct {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"strings decode escapes and surrogate pairs", strings_decode_escapes_and_surrogate_pairs},
        {"capture keeps the exact bytes", capture_keeps_the_exact_bytes},
        {"a tiny buffer reads the same as a large one", a_tiny_buffer_reads_the_same_as_a_large_one},
        {"objects and arrays walk in order", objects_and_arrays_walk_in_order},
        {"a broken document says where", a_broken_document_says_where},
        {"a long string streams in pieces", a_long_string_streams_in_pieces},
        {"a document comes back as it went in", a_document_comes_back_as_it_went_in},
        {"the store survives being reopened", the_store_survives_being_reopened},
        {"a member that was absent stays absent", a_member_that_was_absent_stays_absent},
        {"a v3 bundle imports its state and pictures", a_v3_bundle_imports_its_state_and_pictures},
        {"a failed import leaves the store as it was", a_failed_import_leaves_the_store_as_it_was},
        {"a selection holds its runes and only their edges", a_selection_holds_its_runes_and_only_their_edges},
        {"importing 4x the data does not take 4x the memory",
         importing_four_times_the_data_does_not_take_four_times_the_memory},
        {"a selection costs what it loads, not what the mantle holds",
         a_selection_costs_what_it_loads_not_what_the_mantle_holds},
    };
    for (const auto& t : tests) {
        const int before = failures;
        t.fn();
        std::cout << (failures == before ? "ok   " : "FAIL ") << t.name << "\n";
    }
    if (failures) std::cerr << failures << " failure(s)\n";
    return failures ? 1 : 0;
}
