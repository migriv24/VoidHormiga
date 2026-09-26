/* platform/app_settings.cpp — see app_settings.hpp. */
#include "platform/app_settings.hpp"

#include "platform/device_paths.hpp"

#include "json.hpp"

#include <algorithm>
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
    return s;
}

bool save(const Settings& s) {
    std::error_code ec;
    fs::create_directories(file().parent_path(), ec);
    json j = {{"default_database", s.default_database}, {"recent", s.recent}, {"migrated", s.migrated}};
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

} // namespace hormiga::app_settings
