/* app/store_check.cpp — open a database the way the app does, with no window,
 * and prove the store path end to end (stage 5, app/working_store.hpp).
 *
 *     voidhormiga-cli store-check <bundle.miga> <empty folder>
 *     voidhormiga-cli store-check --synthetic N <empty folder>
 *
 * In `folder` as the working copy: start, open the bundle (the File > Open
 * path), edit a loaded rune through the dispatcher, save (the write-back),
 * check the store has the edit, mint a name, Save As (a v4 bundle packed from
 * the store), quit, start again (the boot from the store), and check the edit
 * survived. Prints what each step cost and the process's peak memory. Run with
 * HORMIGA_MACHINE_MB to plan as a smaller machine would, and under tools/lowmem
 * to hold the process to a real cap. Exit 0 only if every step held. */
#include "app/app_internal.hpp"

#include "rancho/memory.hpp"
#include "store/bundle.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

namespace {

using clk = std::chrono::steady_clock;
double since(clk::time_point t) { return std::chrono::duration<double>(clk::now() - t).count(); }
double mb(std::uint64_t b) { return (double)b / (1024.0 * 1024.0); }
std::uint64_t g_peak = 0;
void sample() { g_peak = std::max(g_peak, rancho::memory::process_bytes()); }

/* A linked database of `n` contacts in the data mantle, written as text: groups
 * of 500 that know each other, and one link in fifty to anyone. */
std::string synthetic_state(int n) {
    std::string o;
    std::mt19937 rng(3);
    o += R"({"version":1,"domains":{},"glyphs":{},"mantles":[{"id":"m1","name":")";
    o += kDataMantle;
    o += R"(","domain":null,"runes":[)";
    for (int i = 0; i < n; ++i) {
        o += i ? "," : "";
        o += R"({"content":{"display_name":"Record )" + std::to_string(i) + R"(","note":")" + std::string(120, 'n') +
             R"("},"facets":{},"glyph":"contact","placement":null,"relations":[],"spirit":{"id":"rune_)" +
             std::to_string(i) + R"(","name":"rec-)" + std::to_string(i) + R"("},"tags":["type:contact"]})";
    }
    o += R"(],"tags":{},"layout":{"edges":[)";
    bool first = true;
    for (int i = 0; i < n; ++i)
        for (int k = 0; k < 3; ++k) {
            const int j = rng() % 50 == 0 ? (int)(rng() % (unsigned)n) : (i / 500) * 500 + (int)(rng() % 500);
            if (j == i || j >= n) continue;
            o += first ? "" : ",";
            o += R"({"from":"rec-)" + std::to_string(i) + R"(","to":"rec-)" + std::to_string(j) +
                 R"(","relation":"knows","weight":1,"directed":true})";
            first = false;
        }
    o += R"(]},"rules":[]}],"config":{},"active":{"mantle":")";
    o += kDataMantle;
    o += R"(","domain":null},"bindings":[]})";
    return o;
}

bool fail(const char* what, const std::string& why = {}) {
    std::fprintf(stderr, "FAILED: %s%s%s\n", what, why.empty() ? "" : ": ", why.c_str());
    return false;
}

} // namespace

int HormigaApp::store_check(const std::string& bundle_in, int synthetic, const std::string& folder) {
    std::setvbuf(stdout, nullptr, _IONBF, 0); // a crash must not take the last lines with it
    // init() styles the window it would draw: a context with no window is enough
    if (!ImGui::GetCurrentContext()) ImGui::CreateContext();
    const fs::path dir = fs::absolute(folder);
    std::error_code ec;
    fs::create_directories(dir, ec);
    const auto budget = hormiga::machine_budget();
    std::printf("planning for a machine with a %.0f MB working-set budget (%.1f MB of text, undo depth %d)\n",
                mb(budget.bytes), mb(budget.text_bytes()), budget.undo_depth);

    fs::path bundle = bundle_in;
    if (synthetic > 0) {
        bundle = dir / ("synthetic-" + std::to_string(synthetic) + ".miga");
        if (!fs::exists(bundle)) {
            const std::string state = synthetic_state(synthetic);
            const auto r = hormiga::miga::pack(state, dir, bundle, "synthetic", dir / "no-assets");
            if (!r.ok) return fail("writing the synthetic bundle", r.error) ? 0 : 1;
        }
    }
    base_dir = dir / "copy";
    fs::remove_all(base_dir, ec); // a clean working copy every run: the boot under test is the second one
    fs::create_directories(base_dir, ec);
    state_name = "check.json";
    init();
    sample();

    const auto t0 = clk::now();
    const std::size_t logged = log.size();
    open_database(fs::absolute(bundle).string());
    sample();
    for (std::size_t i = logged; i < log.size(); ++i) // what the open said: its phases, and anything that went wrong
        if (log[i].level != "info" || log[i].op == "open" || log[i].op == "store")
            std::printf("         [%s %s] %s\n", log[i].level.c_str(), log[i].op.c_str(), log[i].msg.c_str());
    std::printf("open     %6.1f s   %s   process peak %6.0f MB\n", since(t0),
                wstore.active() ? (wstore.partial() ? "a part, in a store" : "all of it, in a store")
                                : "all of it, as a document",
                mb(g_peak));
    if (!wstore.active()) {
        std::printf("the database fits this machine whole: the store path is not used\n");
        return 0;
    }
    const auto& plan = wstore.plan();
    auto said = [&](const char* when) {
        if (const auto* s = wstore.stats())
            std::printf("         write-back after %s: %lld syncs (%lld targeted, %lld full), %lld runes written, "
                        "%lld edge keys, %lld refused\n",
                        when, s->syncs, s->targeted, s->full, s->runes_written, s->edge_keys, s->refused);
    };
    said("the open");
    std::printf("held    %lld of %lld runes, %.1f MB of text, focus %s\n", plan.runes, wstore.total_runes(),
                mb(plan.text_bytes), plan.focus.empty() ? "(none)" : plan.focus.c_str());

    // an edit through the dispatcher, and the save that writes it back
    dispatch_and_reproject(std::string("use ") + kDataMantle);
    if (scene.nodes.empty()) return fail("nothing of the data mantle is loaded") ? 0 : 1;
    const std::string target = scene.nodes[scene.nodes.size() / 2].name;
    const auto t1 = clk::now();
    const auto r = dispatch_and_reproject("set " + target + " note " + maiz::arg("store-check edit"));
    if (!r.ok) return fail("the edit", r.text()) ? 0 : 1;
    do_save();
    sample();
    rancho::Selection one = rancho::Selection::none();
    one.by_name[kDataMantle] = {target};
    const bool stored = wstore.store()->state(one).find("store-check edit") != std::string::npos;
    std::printf("edit     %6.1f ms with its write-back; in the store: %s   process peak %6.0f MB\n",
                since(t1) * 1000.0, stored ? "yes" : "NO", mb(g_peak));
    said("the edit");
    if (!stored) return fail("the edit did not reach the store") ? 0 : 1;

    // a name the store holds is taken, loaded or not
    if (plan.partial) {
        std::string unloaded;
        for (const auto& n : wstore.store()->select(kDataMantle, "type:contact", 2000))
            if (!scene.find(n)) {
                unloaded = n;
                break;
            }
        if (!unloaded.empty() && !wstore.name_taken(kDataMantle, unloaded))
            return fail("an unloaded rune's name read as free", unloaded) ? 0 : 1;
        std::printf("names    an unloaded rune's name (%s) is taken\n", unloaded.c_str());
    }

    // Save As: a v4 bundle, packed from the store
    const fs::path out = dir / "saved.miga";
    fs::remove(out, ec);
    const auto t2 = clk::now();
    save_database_as(out.string(), false);
    sample();
    const bool v4 = hormiga::store::bundle_kind(out) == hormiga::store::BundleKind::V4;
    std::printf("save as  %6.1f s   %s, %.1f MB   process peak %6.0f MB\n", since(t2), v4 ? "a v4 bundle" : "NOT v4",
                mb((std::uint64_t)fs::file_size(out, ec)), mb(g_peak));
    if (!v4) return fail("Save As did not write a v4 bundle") ? 0 : 1;
    {
        rancho::Store saved(out);
        if (saved.rune_count() != wstore.total_runes())
            return fail("the saved bundle does not hold the whole database",
                        std::to_string(saved.rune_count()) + " of " + std::to_string(wstore.total_runes()))
                       ? 0
                       : 1;
        if (saved.state(one).find("store-check edit") == std::string::npos)
            return fail("the saved bundle lost the edit") ? 0 : 1;
    }
    shutdown();

    // a second start, from the working copy the first left: the store
    {
        HormigaApp again;
        again.base_dir = base_dir;
        again.state_name = state_name;
        const auto t3 = clk::now();
        again.init();
        sample();
        if (!again.wstore.active()) return fail("the second start did not boot from the store") ? 0 : 1;
        const bool kept = again.wstore.store()->state(one).find("store-check edit") != std::string::npos;
        std::printf("restart  %6.1f s   from the store, %lld of %lld runes held; the edit %s   process peak %6.0f MB\n",
                    since(t3), again.wstore.plan().runes, again.wstore.total_runes(), kept ? "kept" : "LOST",
                    mb(g_peak));
        if (!kept) return fail("the edit did not survive a restart") ? 0 : 1;
        again.shutdown();
    }
    std::printf("store-check: every step held\n");
    return 0;
}
