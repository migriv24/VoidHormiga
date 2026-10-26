/* app/save_writer.cpp — see save_writer.hpp. */
#include "app/save_writer.hpp"

#include "platform/storage.hpp"
#include "voidmaiz/project.hpp"

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

void SaveWriter::set_storage(Storage* storage) {
    flush(); // never swap the connection under a write
    std::lock_guard<std::mutex> g(mu_);
    storage_ = storage;
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
        Storage* storage = nullptr;
        {
            std::unique_lock<std::mutex> g(mu_);
            wake_.wait(g, [&] { return stop_ || pending_.has_value(); });
            if (stop_ && !pending_) return;
            job = std::move(*pending_);
            pending_.reset();
            writing_ = true;
            storage = storage_;
        }
        const auto t0 = std::chrono::steady_clock::now();
        Report rep;
        if (storage && storage->ok()) {
            /* Every mantle's rows, projected from the text the save was handed —
             * the same document the snapshot gets, so the two cannot disagree. */
            std::vector<maiz::Scene> scenes;
            scenes.reserve(job.mantles.size());
            for (const auto& name : job.mantles) {
                maiz::ProjectOptions po;
                po.mantle = name;
                scenes.push_back(maiz::project_scene(*job.state, *job.glyphs, po));
            }
            if (!storage->save(*job.state, scenes)) {
                rep.error = true;
                rep.message = "db: " + storage->error();
            }
        }
        {
            std::ofstream out(job.snapshot, std::ios::binary | std::ios::trunc);
            out.write(job.state->data(), (std::streamsize)job.state->size());
            if (!out) {
                rep.error = true;
                rep.message += (rep.message.empty() ? "" : "; ") + std::string("write failed: ") +
                               job.snapshot.string();
            }
        }
        rep.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        job = {}; // the document's memory goes now, not when the next save arrives
        {
            std::lock_guard<std::mutex> g(mu_);
            reports_.push_back(std::move(rep));
            writing_ = false;
        }
        idle_.notify_all();
    }
}

} // namespace hormiga
