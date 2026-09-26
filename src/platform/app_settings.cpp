/* platform/app_settings.cpp — see app_settings.hpp. */
#include "platform/app_settings.hpp"

#include "platform/device_paths.hpp"

#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace hormiga::app_settings {

namespace {

fs::path file() { return hormiga::device::get().data / "settings.json"; }

} // namespace

Settings load() {
    Settings s;
    std::ifstream in(file(), std::ios::binary);
    if (!in) return s;
    const json j = json::parse(in, nullptr, false);
    if (!j.is_object()) return s;
    if (j.contains("default_database") && j["default_database"].is_string())
        s.default_database = j["default_database"].get<std::string>();
    if (j.contains("recent") && j["recent"].is_array())
        for (const auto& r : j["recent"])
            if (r.is_string() && s.recent.size() < kRecentMax) s.recent.push_back(r.get<std::string>());
    s.migrated = j.value("migrated", false);
    if (j.contains("phone_nav") && j["phone_nav"].is_array())
        for (const auto& k : j["phone_nav"])
            if (k.is_string()) s.phone_nav.push_back(k.get<std::string>());
    return s;
}

bool save(const Settings& s) {
    std::error_code ec;
    fs::create_directories(file().parent_path(), ec);
    json j = {{"default_database", s.default_database}, {"recent", s.recent}, {"migrated", s.migrated}};
    if (!s.phone_nav.empty()) j["phone_nav"] = s.phone_nav;
    const fs::path tmp = fs::path(file()).concat(".part");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << j.dump(2);
        if (!out) return false;
    }
    fs::rename(tmp, file(), ec);
    return !ec;
}

void note_recent(const std::string& path) {
    if (path.empty()) return;
    std::error_code ec;
    const std::string abs = fs::absolute(path, ec).string();
    Settings s = load();
    s.recent.erase(std::remove(s.recent.begin(), s.recent.end(), abs), s.recent.end());
    s.recent.erase(std::remove_if(s.recent.begin(), s.recent.end(),
                                  [](const std::string& p) {
                                      std::error_code e;
                                      return !fs::exists(p, e);
                                  }),
                   s.recent.end());
    s.recent.insert(s.recent.begin(), abs);
    if (s.recent.size() > kRecentMax) s.recent.resize(kRecentMax);
    save(s);
}

std::vector<KnownDatabase> known_databases(const std::vector<std::string>& folders) {
    const Settings s = load();
    std::vector<KnownDatabase> out;
    auto add = [&](const fs::path& p) -> KnownDatabase* {
        std::error_code ec;
        if (p.extension() != ".miga" || !fs::is_regular_file(p, ec)) return nullptr;
        const std::string abs = fs::absolute(p, ec).lexically_normal().string();
        for (auto& k : out)
            if (k.path == abs) return &k;
        KnownDatabase k;
        k.path = abs;
        k.name = p.stem().string();
        k.bytes = (long long)fs::file_size(p, ec);
        const auto t = fs::last_write_time(p, ec);
        if (!ec) // file_clock -> seconds since 1970, by the offset between the two clocks now
            k.modified = (long long)std::chrono::duration_cast<std::chrono::seconds>(
                             (t - fs::file_time_type::clock::now()) + std::chrono::system_clock::now().time_since_epoch())
                             .count();
        out.push_back(k);
        return &out.back();
    };
    for (const auto& f : folders) {
        std::error_code ec;
        for (fs::directory_iterator it(f, ec), end; !ec && it != end; it.increment(ec)) {
            if (it->is_directory(ec)) { // a joined database sits in a folder of its own
                std::error_code e2;
                for (fs::directory_iterator in(it->path(), e2), e; !e2 && in != e; in.increment(e2))
                    if (KnownDatabase* k = add(in->path())) k->in_folder = true;
            } else if (KnownDatabase* k = add(it->path())) {
                k->in_folder = true;
            }
        }
    }
    for (const auto& r : s.recent)
        if (KnownDatabase* k = add(r)) k->recent = true;
    if (!s.default_database.empty())
        if (KnownDatabase* k = add(s.default_database)) k->is_default = true;
    std::stable_sort(out.begin(), out.end(),
                     [](const KnownDatabase& a, const KnownDatabase& b) { return a.modified > b.modified; });
    return out;
}

} // namespace hormiga::app_settings
