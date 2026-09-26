/* platform/profile.cpp — see profile.hpp and okf/concepts/platform/lan-sharing.md §1. */
#include "platform/profile.hpp"

#include "domain/collab.hpp" // kPalette: one set of colours for profiles and presence

#include "json.hpp"
#include "stb_image.h"       // decls only -- the implementation is platform/stb_impl.cpp
#include "stb_image_write.h" // decls only -- the implementation is app/app.cpp
#include <sodium.h>

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

#include "platform/device_paths.hpp"

namespace fs = std::filesystem;

namespace hormiga::profile {

namespace {

std::string iso_today() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof buf, "%Y-%m-%d", &tm);
    return buf;
}

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::string u8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

/* Owner-only on POSIX. On Windows a file in the user's own AppData already has
 * an ACL that excludes other users, which is the same property. */
void owner_only(const fs::path& p) {
#ifndef _WIN32
    std::error_code ec;
    fs::permissions(p, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace,
                    ec);
#else
    (void)p;
#endif
}

std::string human_bytes(unsigned long long b) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.1f GB", (double)b / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

}  // namespace

fs::path dir() {
    // the device says where (platform/device_paths.hpp): on a desktop the same
    // per-user folder as always; on a phone, inside the app's own folder, where
    // HOME-based guessing had put it at "/.config/..." (2026-09-25)
    fs::path d = hormiga::device::get().profile;
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

Profile load_or_create(std::string* error) {
    Profile p;
    const fs::path d = dir();
    std::ifstream in(d / "profile.json", std::ios::binary);
    const nlohmann::json j = in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
    auto str = [&j](const char* k) {
        return j.is_object() && j.contains(k) && j[k].is_string() ? j[k].get<std::string>()
                                                                  : std::string();
    };
    p.username = str("username");
    p.color = str("color");
    p.avatar = str("avatar");
    p.created = str("created");
    const std::string key = slurp(d / "profile.key");
    bool dirty = false;
    if (key.size() == crypto_kx_PUBLICKEYBYTES + crypto_kx_SECRETKEYBYTES) {
        p.public_key = key.substr(0, crypto_kx_PUBLICKEYBYTES);
        p.secret_key = key.substr(crypto_kx_PUBLICKEYBYTES);
    } else if (sodium_init() >= 0) {
        unsigned char pk[crypto_kx_PUBLICKEYBYTES], sk[crypto_kx_SECRETKEYBYTES];
        crypto_kx_keypair(pk, sk);
        p.public_key.assign((char*)pk, sizeof pk);
        p.secret_key.assign((char*)sk, sizeof sk);
        sodium_memzero(sk, sizeof sk);
        dirty = true;
    }
    if (p.color.empty() && p.public_key.size() == crypto_kx_PUBLICKEYBYTES) {
        // a first colour from the key, so two fresh profiles are unlikely to match
        p.color = hormiga::collab::kPalette[(unsigned char)p.public_key[0] % 12];
        dirty = true;
    }
    if (p.created.empty()) {
        p.created = iso_today();
        dirty = true;
    }
    if (dirty) save(p, error);
    return p;
}

bool save(const Profile& p, std::string* error) {
    const fs::path d = dir();
    nlohmann::json j = nlohmann::json::object();
    j["about"] = "Void Hormiga profile: the person at this computer. Not part of any "
                 "database. profile.key beside it is the private key - never share it.";
    j["username"] = p.username;
    j["color"] = p.color;
    j["avatar"] = p.avatar;
    j["created"] = p.created;
    {
        std::ofstream out(d / "profile.json", std::ios::binary | std::ios::trunc);
        out << j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << "\n";
        if (!out) {
            if (error) *error = "cannot write " + u8(d / "profile.json");
            return false;
        }
    }
    if (p.public_key.size() == crypto_kx_PUBLICKEYBYTES &&
        p.secret_key.size() == crypto_kx_SECRETKEYBYTES) {
        const fs::path kf = d / "profile.key";
        std::ofstream out(kf, std::ios::binary | std::ios::trunc);
        out << p.public_key << p.secret_key;
        out.close();
        owner_only(kf);
    }
    return true;
}

fs::path avatar_path(const Profile& p) {
    if (p.avatar.empty()) return {};
    const fs::path a = dir() / p.avatar;
    std::error_code ec;
    return fs::exists(a, ec) ? a : fs::path();
}

bool set_avatar(Profile& p, const fs::path& picked, std::string* error) {
    std::error_code ec;
    /* A NEW NAME EACH TIME: pictures are cached by path, so a new picture under
     * the old name would keep showing the old one until a restart. */
    auto remove_others = [&ec](const std::string& keep) {
        for (const auto& e : fs::directory_iterator(dir(), ec)) {
            const std::string fn = e.path().filename().string();
            if (fn.rfind("avatar", 0) == 0 && fn != keep) fs::remove(e.path(), ec);
        }
    };
    if (picked.empty()) {
        remove_others("");
        p.avatar.clear();
        return save(p, error);
    }
    int w = 0, h = 0, n = 0;
    const std::string bytes = slurp(picked);
    if (!stbi_info_from_memory((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &n)) {
        if (error) *error = "that file is not a picture this build can read (PNG, JPEG, GIF, BMP)";
        return false;
    }
    std::string ext = picked.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    const std::string name = "avatar-" + std::to_string((long long)std::time(nullptr)) +
                             (ext.empty() ? std::string(".png") : ext);
    fs::copy_file(picked, dir() / name, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error) *error = "cannot copy the picture: " + ec.message();
        return false;
    }
    p.avatar = name;
    remove_others(name);  // after the copy, so picking the current picture again is safe
    return save(p, error);
}

std::string avatar_png(const Profile& p, int size) {
    const fs::path a = avatar_path(p);
    if (a.empty() || size <= 0) return {};
    const std::string bytes = slurp(a);
    int w = 0, h = 0, n = 0;
    unsigned char* px =
        stbi_load_from_memory((const unsigned char*)bytes.data(), (int)bytes.size(), &w, &h, &n, 4);
    if (!px) return {};
    // centre square, then a box filter down to size x size
    const int side = std::min(w, h), ox = (w - side) / 2, oy = (h - side) / 2;
    std::vector<unsigned char> out((std::size_t)size * size * 4);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const int x0 = ox + x * side / size, x1 = std::max(x0 + 1, ox + (x + 1) * side / size);
            const int y0 = oy + y * side / size, y1 = std::max(y0 + 1, oy + (y + 1) * side / size);
            unsigned long sum[4] = {0, 0, 0, 0}, cnt = 0;
            for (int yy = y0; yy < y1 && yy < h; ++yy)
                for (int xx = x0; xx < x1 && xx < w; ++xx) {
                    const unsigned char* s = px + ((std::size_t)yy * w + xx) * 4;
                    for (int c = 0; c < 4; ++c) sum[c] += s[c];
                    ++cnt;
                }
            for (int c = 0; c < 4; ++c)
                out[((std::size_t)y * size + x) * 4 + c] = (unsigned char)(cnt ? sum[c] / cnt : 0);
        }
    stbi_image_free(px);
    std::string png;
    stbi_write_png_to_func(
        [](void* ctx, void* data, int len) {
            static_cast<std::string*>(ctx)->append(static_cast<const char*>(data), (std::size_t)len);
        },
        &png, size, size, 4, out.data(), size * 4);
    return png;
}

bool erase(std::string* error) {
    std::error_code ec;
    const fs::path d = dir();
    for (const auto& e : fs::directory_iterator(d, ec)) fs::remove_all(e.path(), ec);
    if (ec && error) *error = "could not remove the profile at " + u8(d) + ": " + ec.message();
    return !ec;
}

std::string credentials_key(const Profile& p) {
    if (p.secret_key.size() != crypto_kdf_KEYBYTES || sodium_init() < 0) return {};
    unsigned char sub[32];
    // subkey 1 of context "hrmgcred": only this use, never the key exchange's own key
    if (crypto_kdf_derive_from_key(sub, sizeof sub, 1, "hrmgcred",
                                   reinterpret_cast<const unsigned char*>(p.secret_key.data())) != 0)
        return {};
    char hex[sizeof sub * 2 + 1];
    sodium_bin2hex(hex, sizeof hex, sub, sizeof sub);
    sodium_memzero(sub, sizeof sub);
    std::string out(hex);
    sodium_memzero(hex, sizeof hex);
    return out;
}

std::string display_name(const Profile& p) {
    return p.username.empty() ? std::string("(no username yet)") : p.username;
}

std::vector<Fact> device_facts(const std::vector<Fact>& extra) {
    std::vector<Fact> f;
#ifdef _WIN32
    auto reg = [](const char* name) {
        char buf[256];
        DWORD len = sizeof buf;
        if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                         name, RRF_RT_REG_SZ, nullptr, buf, &len) == ERROR_SUCCESS)
            return std::string(buf);
        return std::string();
    };
    /* ProductName still says "Windows 10" on Windows 11 -- Microsoft never
     * changed the value -- so the build number is what decides. */
    const std::string build = reg("CurrentBuild");
    const std::string edition = reg("EditionID");
    const std::string shown = reg("DisplayVersion");
    const bool eleven = std::atoi(build.c_str()) >= 22000;
    f.push_back({"Operating system", std::string(eleven ? "Windows 11" : "Windows 10") +
                                         (edition.empty() ? "" : " " + edition) +
                                         (shown.empty() ? "" : " " + shown) +
                                         (build.empty() ? "" : " (build " + build + ")")});
    char host[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD hl = sizeof host;
    f.push_back({"Device name", GetComputerNameA(host, &hl) ? std::string(host) : ""});
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) f.push_back({"Memory", human_bytes(ms.ullTotalPhys)});
#elif defined(__APPLE__)
    char ver[64] = {};
    size_t vl = sizeof ver;
    sysctlbyname("kern.osproductversion", ver, &vl, nullptr, 0);
    f.push_back({"Operating system", std::string("macOS ") + ver});
    char host[256] = {};
    gethostname(host, sizeof host - 1);
    f.push_back({"Device name", host});
    unsigned long long mem = 0;
    size_t ml = sizeof mem;
    if (sysctlbyname("hw.memsize", &mem, &ml, nullptr, 0) == 0)
        f.push_back({"Memory", human_bytes(mem)});
#else
    std::string pretty;
    std::ifstream osr("/etc/os-release");
    for (std::string line; std::getline(osr, line);)
        if (line.rfind("PRETTY_NAME=", 0) == 0) {
            pretty = line.substr(12);
            if (pretty.size() >= 2 && pretty.front() == '"') pretty = pretty.substr(1, pretty.size() - 2);
        }
    utsname u{};
    uname(&u);
    f.push_back({"Operating system", (pretty.empty() ? std::string(u.sysname) : pretty) +
                                         " (kernel " + u.release + ")"});
    char host[256] = {};
    gethostname(host, sizeof host - 1);
    f.push_back({"Device name", host});
    const long pages = sysconf(_SC_PHYS_PAGES), page = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && page > 0) f.push_back({"Memory", human_bytes((unsigned long long)pages * page)});
#endif
    const char* arch =
#if defined(__x86_64__) || defined(_M_X64)
        "x86-64";
#elif defined(__aarch64__) || defined(_M_ARM64)
        "ARM64";
#elif defined(__i386__) || defined(_M_IX86)
        "x86";
#else
        "unknown";
#endif
    f.push_back({"Processor", std::string(arch) + ", " +
                                  std::to_string(std::thread::hardware_concurrency()) + " threads"});
#ifdef HORMIGA_VERSION
    f.push_back({"Void Hormiga", HORMIGA_VERSION});
#endif
    for (const auto& e : extra) f.push_back(e);
    return f;
}

}  // namespace hormiga::profile
