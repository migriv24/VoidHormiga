/* storage.hpp — the SQLite Data holiday, v1 (phase C; okf/concepts/platform/antfarm/index.md).
 *
 * The shape, honest about what owns what: the CORE owns the model in memory
 * and the dispatcher owns change; this holiday owns WHERE THE ORG LIVES.
 * One .db file holds two representations:
 *   - meta.state       — the exported state document, verbatim: the exact
 *                        round-trip (camera, config, everything).
 *   - runes/fields/tags/links — normalized rows derived from the same save,
 *                        one row per fact: queryable at scale, greppable by
 *                        sqlite3-anything, and the substrate the tag-grammar→
 *                        SQL compilation will target when scale demands it
 *                        (today the grammar matches in-memory via projection).
 * A JSON snapshot mirror rides along beside the .db (the snapshot-fallback
 * principle: a corrupted database must never mean a dead app).
 */
#pragma once

#include "voidmaiz/scene.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;

namespace hormiga {

class Storage {
public:
    explicit Storage(std::filesystem::path db_path);
    ~Storage();
    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    bool ok() const { return db_ != nullptr; }
    const std::string& error() const { return err_; }
    const std::filesystem::path& path() const { return path_; }

    /* One transaction: replace the normalized rows with these mantles'
     * projections and store the state document. False on failure (err()). */
    bool save(const std::string& state_json, const std::vector<maiz::Scene>& mantles);

    /* The stored state document, "" when absent/unreadable. */
    std::string load_state();

    struct Stats {
        long long runes = 0, tags = 0, links = 0;
        long long bytes = 0;      // file size on disk
        std::string saved_at;     // ISO-8601 UTC of the last save, "" if never
    };
    Stats stats();

private:
    sqlite3* db_ = nullptr;
    std::string err_;
    std::filesystem::path path_;
};

} // namespace hormiga
