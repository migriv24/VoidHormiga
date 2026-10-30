/* store/store.hpp — the database on disk (okf/concepts/platform/store.md).
 *
 * One SQLite file holds a whole Void Core state document as rows: a rune per
 * row, an edge per row, a tag per row, a mantle per row, and the state's other
 * top-level keys. Each rune and edge keeps the exact JSON Core wrote, so the
 * store can always give the document back; what it ADDS is that the document
 * no longer has to be in memory to be read, queried or partly loaded.
 *
 * Memory: a bounded page cache, plus whatever one call returns. Importing a
 * document reads it as a stream (store/json_stream.hpp), one rune at a time.
 *
 * DELETION IS SOFT until `purge`. A rune removed in the working set takes with
 * it edges to runes that were never loaded; if the removal is undone, Core
 * restores only what it held. So removed rows are marked with what removed them
 * (`rune:<id>`, `mantle:<id>`) and `restore` brings them back when that rune or
 * mantle reappears. `purge` forgets them, when no undo can reach them any more.
 *
 * Not thread-safe: one Store per thread, or serialize calls. */
#pragma once

#include "store/json_stream.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace hormiga::store {

using Progress = std::function<void(double fraction, const char* phase)>;
/* A picture in a v3 bundle: its path in the bundle ("assets/x.png") and bytes. */
using AssetSink = std::function<void(const std::string& path, const std::string& bytes)>;

struct ImportStats {
    bool ok = false;
    std::string error;
    long long mantles = 0, runes = 0, edges = 0, tags = 0, assets = 0;
    bool envelope = false;   // it was a .miga v3, not a bare state document
    std::string bundle_name; // the envelope's meta.name
};

/* Which runes a written state document holds. Every mantle is always present
 * (a mantle not selected is written with no runes and no edges), and an edge
 * is written only when BOTH its ends are. */
struct Selection {
    bool everything = true;
    std::set<std::string> whole;                              // mantles written whole
    std::map<std::string, std::vector<std::string>> by_tag;   // mantle -> a rune with ANY of these
    std::map<std::string, std::vector<std::string>> by_name;  // mantle -> these runes
    /* mantle -> cluster paths ("12", "12.4"): a path loads every leaf under it,
     * and the mantle's always-loaded runes (its measures) come with any. */
    std::map<std::string, std::vector<std::string>> by_cluster;
    static Selection all() { return {}; }
    static Selection none() {
        Selection s;
        s.everything = false;
        return s;
    }
};

struct MantleInfo {
    std::string name;
    long long runes = 0, edges = 0;
    long long bytes = 0;      // its runes' and edges' JSON
    long long rune_bytes = 0; // its runes' alone
};

/* One leaf of a mantle's cluster hierarchy (store/clusters), the unit a
 * working set loads. Paths sort so that leaves of one parent are adjacent. */
struct ClusterLeaf {
    std::string path;
    long long runes = 0, bytes = 0;
};

/* Runes of a mantle in order, from an offset, up to a byte budget. */
struct Page {
    std::vector<std::string> names;
    std::uint64_t bytes = 0;
};

struct ClusterStats {
    bool ok = false;
    long long runes = 0, leaves = 0, always = 0;
    int depth = 0;
    double modularity = 0, seconds = 0;
};

/* A mantle's row: everything but its runes and edges. */
struct MantleRow {
    std::string name, mid;   // mid: the mantle's own "id"
    std::string skeleton;    // the mantle object without runes and layout
    std::optional<std::string> layout; // the layout object without edges; absent if the mantle had none
    bool has_runes = true, has_edges = true;
};

class Store {
public:
    static constexpr int kFormat = 1;

    /* Opens, or creates, the store at `path`. */
    explicit Store(const std::filesystem::path& path);
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    bool ok() const { return db_ != nullptr && error_.empty(); }
    const std::string& error() const { return error_; }
    const std::filesystem::path& path() const { return path_; }

    /* Replace the whole store with the document `in` holds: a bare state
     * document, or a .miga v3 envelope (whose pictures go to `assets`). One
     * transaction: on failure the store is as it was. */
    ImportStats import(JsonStream& in, const Progress& progress = {}, const AssetSink& assets = {});

    /* The state document, whole or as `sel` chooses. */
    bool write_state(std::ostream& out, const Selection& sel = Selection::all());
    std::string state(const Selection& sel = Selection::all());

    std::vector<MantleInfo> mantles(); // exact: counts every row

    /* THE SAME, ESTIMATED, for planning: kept in two small tables, computed at
     * import and after clustering, so a plan does not read every rune of the
     * database on every open (4.7 s of a 7.4 s open at 5×10^5). Edits since
     * drift them a little; a plan is a budget, not a ledger. A mantle the
     * estimates do not know yet is counted exactly. */
    std::vector<MantleInfo> estimates();
    std::vector<ClusterLeaf> leaf_estimates(const std::string& mantle);
    void refresh_estimates();

    /* A consistent, compacted copy of the whole store at `out` (SQLite's
     * VACUUM INTO), taken without stopping it: the first step of a bundle. */
    bool vacuum_into(const std::filesystem::path& out);

    /* The store's own notes (its format, what a bundle was called, what
     * snapshot was written from it last), as text. "" when unset. */
    std::string meta(const std::string& key);
    void set_meta(const std::string& key, const std::string& value);
    /* True if the store holds a document (an import happened, or rows were written). */
    bool has_document();
    long long rune_count(const std::string& mantle = {}); // {} = all mantles

    // ── write-back (store/working_set.hpp is its caller) ──────────────────
    bool begin();
    bool commit();
    void rollback();

    void put_top(const std::string& key, const std::string& json);
    void keep_only_top(const std::set<std::string>& keys); // drops the others ("mantles" stays)

    /* The live mantle with this mid, else a soft-deleted one (restored, with
     * everything it took), else a new one. Returns its row id. */
    long long put_mantle(const MantleRow& m, int pos);
    void delete_mantle(long long mantle); // soft, with everything in it

    /* A rune as Core now has it. `json` is the rune's whole object. */
    void put_rune(long long mantle, const std::string& name, const std::string& json);
    void rename_rune(long long mantle, const std::string& from, const std::string& to);
    void delete_rune(long long mantle, const std::string& name); // soft, with every edge touching it
    /* Bring back a rune soft-deleted with this spirit id, and what it took. */
    bool restore_rune(long long mantle, const std::string& rid);

    /* Make the live edges between `src` and `dst` with `relation` exactly
     * these (each a whole edge object), keeping rows that already match. */
    void set_edges(long long mantle, const std::string& src, const std::string& dst, const std::string& relation,
                   const std::vector<std::string>& jsons);

    /* Forget soft-deleted rows. */
    void purge();

    /* The live mantle's row id by name, -1 if none. */
    long long mantle_row(const std::string& name);
    /* The spirit id of the live rune `name`, if there is one. */
    std::optional<std::string> live_rune_id(long long mantle, const std::string& name);

    // ── clusters: Hormiga's index of where each rune belongs ──────────────
    /* Cluster a mantle by its links (Void Maiz's Leiden hierarchy, leaves of at
     * most `target` runes) and store each rune's path. Derived data, like an
     * index: rebuilt, never authored, and not part of the document. Runes of a
     * measure glyph are marked always-loaded, because every value edge points
     * at one. */
    ClusterStats cluster(const std::string& mantle, std::uint32_t target = 2000, const Progress& progress = {});
    std::vector<ClusterLeaf> leaves(const std::string& mantle); // sorted by path
    /* Runes in their order from `offset`, while their bytes fit `byte_cap`
     * (at least one), and at most `max_runes` of them (0: no limit). */
    Page page(const std::string& mantle, long long offset, std::uint64_t byte_cap, long long max_runes = 0);
    bool clustered(const std::string& mantle);
    /* The leaf path a rune is in, "" if none. */
    std::string cluster_of(const std::string& mantle, const std::string& rune);
    /* File a rune under a leaf (a rune made after the clustering). */
    void assign_cluster(const std::string& mantle, const std::string& rune, const std::string& path);

private:
    bool exec(const char* sql);

    sqlite3* db_ = nullptr;
    std::string error_;
    std::filesystem::path path_;
};

} // namespace hormiga::store
