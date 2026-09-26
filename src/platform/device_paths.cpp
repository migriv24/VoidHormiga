/* platform/device_paths.cpp — see device_paths.hpp. */
#include "platform/device_paths.hpp"

#include <cstdlib>
#include <mutex>

namespace fs = std::filesystem;

namespace hormiga::device {

namespace {

std::string env(const char* k) {
    const char* v = std::getenv(k);
    return v ? std::string(v) : std::string();
}

std::mutex g_mu;
bool g_set = false;
Paths g_paths;

#ifndef HORMIGA_PLATFORM
#define HORMIGA_PLATFORM "unknown"
#endif

} // namespace

Paths desktop_defaults() {
    Paths p;
    p.kind = Kind::Desktop;
    p.platform = HORMIGA_PLATFORM;
#ifdef _WIN32
    const fs::path home = env("USERPROFILE");
    p.data = fs::path(env("APPDATA")) / "VoidHormiga";
#elif defined(__APPLE__)
    const fs::path home = env("HOME");
    p.data = home / "Library" / "Application Support" / "VoidHormiga";
#else
    const fs::path home = env("HOME");
    const std::string xdg = env("XDG_CONFIG_HOME");
    p.data = (xdg.empty() ? home / ".config" : fs::path(xdg)) / "voidhormiga";
#endif
    p.profile = p.data / "profile";
    if (const std::string o = env("HORMIGA_PROFILE_DIR"); !o.empty()) p.profile = o;
    p.databases = home.empty() ? fs::path() : home / "Documents" / "HormigaFiles";
    return p;
}

Paths phone(const fs::path& app_dir) {
    Paths p;
    p.kind = Kind::Phone;
    p.platform = HORMIGA_PLATFORM;
    p.data = app_dir;
    p.profile = app_dir / "profile";
    p.databases = app_dir / "databases";
    return p;
}

void set(const Paths& p) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_paths = p;
    g_set = true;
}

const Paths& get() {
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_set) {
        g_paths = desktop_defaults();
        g_set = true;
    }
    return g_paths;
}

const char* kind_name(Kind k) { return k == Kind::Phone ? "phone" : "desktop"; }

} // namespace hormiga::device
