/* app/working_store.hpp — the working copy as a Void Rancho store, and the part
 * of it Void Core holds (okf/concepts/platform/store.md, stage 5;
 * okf/concepts/platform/frontier.md).
 *
 * The author, 2026-11-03: "we should always be able to open any database ...
 * if the bug still persists (we are unable to load some database), then that
 * means we have to keep developing to prevent that." So an open never asks
 * whether the machine can afford the database. It asks how much of it the
 * machine can hold, and gives Core that much:
 *
 *  - a database whose state fits the machine's budget (an eighth of its memory,
 *    rancho/plan.hpp) loads whole, through the same path as always;
 *  - one that does not opens into a Rancho store, the working copy, and Core
 *    holds a WORKING SET of it: its small mantles whole, and the large ones by
 *    cluster around what the person is looking at. Edits are still dispatcher
 *    commands on that Core, and the save effect writes them back to the store
 *    (rancho::WorkingSet::sync), costing what is loaded, never the database.
 *
 * Rancho's store is opened by each thread that needs it: a worker prepares a
 * working set with a connection of its own, and the main thread keeps one for
 * write-back. SQLite in WAL mode lets the two read side by side.
 *
 * Partial is a STATE, not an error. What cannot yet work on a part (a merge of
 * whole documents with another device) asks `partial()` and waits, saying why;
 * what reads the whole database (an export) reads it from the store. */
#pragma once

#include "rancho/plan.hpp"
#include "rancho/store.hpp"
#include "rancho/working_set.hpp"
#include "store/bundle.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace maiz {
class Core;
class RanchoLink;
struct Result;
} // namespace maiz

namespace hormiga {

/* This machine's working-set budget: an eighth of its memory (rancho/plan.hpp). */
rancho::Budget machine_budget();

/* Would this bundle's state fit the budget whole? Judged from the file's size,
 * which over-counts (a v3 file carries its pictures), so a "yes" is safe. A v4
 * bundle is a store already and is always opened as one. */
bool fits_whole(const std::filesystem::path& bundle, const rancho::Budget& budget);

/* What a worker hands the main thread: a plan, and Core's document for it. */
struct Prepared {
    bool ok = false;
    std::string error;
    rancho::Plan plan;
    std::string state;          // the working set's state document
    store::BundleResult bundle; // when it came from a bundle
    long long clustered = 0;    // mantles clustered on the way (the first open of a large one)
    std::string timings;        // what each phase cost, for the log
};

/* Open `bundle` (v3 or v4) into a store at `store_path`, cluster any mantle too
 * large to hold whole, and plan a working set. Constant memory in the size of
 * the bundle, apart from the working set itself. Worker-safe. */
Prepared open_into_store(const std::filesystem::path& bundle, const std::filesystem::path& store_path,
                         const std::filesystem::path& base_dir, const std::filesystem::path& assets_dir,
                         const rancho::Budget& budget, const rancho::Progress& progress = {});

/* Plan a working set of an existing store. With `focus_rune` (in
 * `focus_mantle`), the part around it; with `focus_leaf`, the part around that
 * cluster leaf. Worker-safe. */
Prepared prepare_store(const std::filesystem::path& store_path, const rancho::Budget& budget,
                       const std::string& focus_mantle = {}, const std::string& focus_rune = {},
                       const std::string& focus_leaf = {});

/* The plan around a cluster leaf: what rancho::plan does around a rune's leaf,
 * started from a leaf the person chose (the Data tab's parts). Asked of Rancho
 * as a `focus_leaf` argument; Hormiga's until then. */
rancho::Plan plan_around_leaf(rancho::Store& store, const rancho::Budget& budget, const std::string& mantle,
                              const std::string& leaf);

/* Delete a store and its -wal/-shm. */
void remove_store_files(const std::filesystem::path& store_path);

class WorkingStore {
public:
    WorkingStore();
    ~WorkingStore();
    WorkingStore(const WorkingStore&) = delete;
    WorkingStore& operator=(const WorkingStore&) = delete;

    /* `core` holds `plan`'s working set of the store at `path` (it was built
     * from Prepared::state): from now on the store is the working copy. */
    bool attach(maiz::Core& core, const std::filesystem::path& path, const rancho::Plan& plan);
    /* Back to a working copy that is Core's whole document. */
    void detach();

    bool active() const { return ws_ != nullptr; }
    /* Some of the database is not in Core. */
    bool partial() const { return active() && plan_.partial; }
    const rancho::Plan& plan() const { return plan_; }
    const std::filesystem::path& path() const { return path_; }
    rancho::Store* store() { return store_.get(); }
    const std::string& error() const { return error_; }

    /* A command through the working set, so the next sync writes only what it
     * named (rancho::WorkingSet::dispatch); the reply as Void Maiz gives it. */
    maiz::Result dispatch(const std::string& command);
    /* Write what Core now says back to the store. True when there was nothing
     * to write. Call after commands land (the save effect). */
    bool sync();
    /* A live rune of `mantle` with this name exists in the store, loaded or
     * not. False when not active: the scene is then the whole mantle. */
    bool name_taken(const std::string& mantle, const std::string& name);
    /* What the write-back has done (rancho::WorkingSet::Stats), null when not active. */
    const rancho::WorkingSet::Stats* stats() const { return ws_ ? &ws_->stats : nullptr; }
    /* Runes the database holds (exact), for "N of M". */
    long long total_runes();

private:
    std::unique_ptr<rancho::Store> store_;
    std::unique_ptr<maiz::RanchoLink> link_;
    std::unique_ptr<rancho::WorkingSet> ws_;
    rancho::Plan plan_;
    std::filesystem::path path_;
    std::string error_;
    long long total_ = -1;
};

} // namespace hormiga
