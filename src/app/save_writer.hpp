/* app/save_writer.hpp — the working copy's JSON snapshot, written off the UI thread.
 *
 * The database lives in the working copy's store (okf/concepts/platform/
 * store.md), written as each command lands. What `save` still writes is the
 * JSON SNAPSHOT beside it: the state document `voidhormiga-cli`, agents and the
 * boot-time fallback read. At 5×10^5 runes that document is hundreds of
 * megabytes, so it is streamed out of the store on a connection of its own and
 * a thread of its own, in constant memory, written to a temporary file and
 * renamed (a reader never sees half of one).
 *
 * Saves COALESCE: one waiting while another is written is replaced by a newer
 * one, because only the newest state is worth writing.
 *
 * What it promises the main thread:
 *  - flush() returns only when everything submitted is on disk. Shutdown,
 *    and anything that reads the snapshot, flushes first.
 *  - Failures come back as reports (take_reports), drained on the main thread
 *    into the log and a toast — a failed save is never silent. */
#pragma once

#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace hormiga {

/* A snapshot file's identity: its time and size. The store records the stamp
 * of the snapshot it last wrote, so a snapshot with any other stamp was written
 * by somebody else (the CLI, an agent) and is newer news (app/store_host.cpp). */
inline std::string snapshot_stamp(const std::filesystem::path& file) {
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(file, ec);
    if (ec) return {};
    return std::to_string((long long)t.time_since_epoch().count()) + "/" +
           std::to_string((unsigned long long)std::filesystem::file_size(file, ec));
}

class SaveWriter {
public:
    struct Job {
        std::filesystem::path store;    // the working copy's store, read on a connection of the writer's own
        std::filesystem::path snapshot; // the JSON state document to write
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
    std::vector<Report> reports_;
    std::thread worker_;
};

} // namespace hormiga
