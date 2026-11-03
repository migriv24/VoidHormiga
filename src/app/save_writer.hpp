/* app/save_writer.hpp — the working copy is written off the UI thread.
 *
 * A save writes the working copy twice over: the SQLite Data holiday (the state
 * document plus a row per fact) and the JSON snapshot beside it. Both are
 * proportional to the database, and both ran inside the `save` effect, on the
 * main thread, inside the dispatch. At 5×10^5 runes (the stress database,
 * okf/concepts/platform/scale.md) that was a projection of every mantle and
 * millions of rows — a window that stopped drawing for most of a minute on
 * every Ctrl+S, every typed field and every LAN merge.
 *
 * Now the effect hands the exported text to this writer and returns. The writer
 * projects and writes on its own thread. Saves COALESCE: one waiting while
 * another is written is replaced by a newer one, because only the newest state
 * is worth writing — the working copy is a mirror, never a history (the history
 * is the journal and the bundles' backups).
 *
 * What it promises the main thread:
 *  - flush() returns only when everything submitted is on disk. Shutdown,
 *    and anything that reads the working copy's files, flushes first.
 *  - The Storage is the writer's while it writes: a main-thread reader asks
 *    busy() and waits or says "saving" rather than share the connection.
 *  - Failures come back as reports (take_reports), drained on the main thread
 *    into the log and a toast — a failed save is never silent.
 *  - Order on disk is the effect's old order: the database, then the snapshot,
 *    so a snapshot newer than the database still means what boot reads it as. */
#pragma once

#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace hormiga {

class Storage;

class SaveWriter {
public:
    struct Job {
        std::shared_ptr<const std::string> state;  // the exported document
        std::shared_ptr<const std::string> glyphs; // the `glyphs` payload it projects with
        std::vector<std::string> mantles;          // every mantle's name
        std::filesystem::path snapshot;            // the JSON mirror beside the database
    };
    struct Report {
        bool error = false;
        std::string message;
        double ms = 0; // how long the write took
    };

    SaveWriter() = default;
    ~SaveWriter(); // flushes, then stops the thread
    SaveWriter(const SaveWriter&) = delete;
    SaveWriter& operator=(const SaveWriter&) = delete;

    /* `storage` may be null (the database is unavailable): only the snapshot is
     * written then. It must outlive the writer, or be swapped only after a flush. */
    void set_storage(Storage* storage);
    void submit(Job job);
    void flush();
    bool busy() const;
    std::vector<Report> take_reports();

private:
    void run();
    mutable std::mutex mu_;
    std::condition_variable wake_, idle_;
    std::optional<Job> pending_;
    bool writing_ = false, stop_ = false;
    Storage* storage_ = nullptr;
    std::vector<Report> reports_;
    std::thread worker_;
};

} // namespace hormiga
