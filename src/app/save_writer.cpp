/* app/save_writer.cpp — see save_writer.hpp. */
#include "app/save_writer.hpp"

#include "store/store.hpp"

#include <chrono>
#include <fstream>
#include <utility>

namespace hormiga {

SaveWriter::~SaveWriter() {
    {
        std::unique_lock<std::mutex> g(mu_);
        idle_.wait(g, [&] { return !pending_ && !writing_; });
        stop_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void SaveWriter::submit(Job job) {
    {
        std::lock_guard<std::mutex> g(mu_);
        pending_ = std::move(job); // a save still waiting is replaced: only the newest matters
        if (!worker_.joinable()) worker_ = std::thread([this] { run(); });
    }
    wake_.notify_one();
}

void SaveWriter::flush() {
    std::unique_lock<std::mutex> g(mu_);
    idle_.wait(g, [&] { return !pending_ && !writing_; });
}

bool SaveWriter::busy() const {
    std::lock_guard<std::mutex> g(mu_);
    return pending_.has_value() || writing_;
}

std::vector<SaveWriter::Report> SaveWriter::take_reports() {
    std::lock_guard<std::mutex> g(mu_);
    return std::exchange(reports_, {});
}

void SaveWriter::run() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> g(mu_);
            wake_.wait(g, [&] { return stop_ || pending_.has_value(); });
            if (stop_ && !pending_) return;
            job = std::move(*pending_);
            pending_.reset();
            writing_ = true;
        }
        const auto t0 = std::chrono::steady_clock::now();
        Report rep;
        {
            store::Store st(job.store); // its own connection: the main thread keeps writing to its own
            const std::filesystem::path tmp = job.snapshot.string() + ".writing";
            bool ok = st.ok();
            if (ok) {
                std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
                ok = st.write_state(out) && (bool)out;
            }
            std::error_code ec;
            if (ok) std::filesystem::rename(tmp, job.snapshot, ec);
            if (ok && !ec) st.set_meta("snapshot.stamp", snapshot_stamp(job.snapshot)); // ours, not news
            if (!ok || ec) {
                rep.error = true;
                rep.message = "write failed: " + job.snapshot.string() + (st.ok() ? "" : " (" + st.error() + ")");
                std::filesystem::remove(tmp, ec);
            }
        }
        rep.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        {
            std::lock_guard<std::mutex> g(mu_);
            reports_.push_back(std::move(rep));
            writing_ = false;
        }
        idle_.notify_all();
    }
}

} // namespace hormiga
