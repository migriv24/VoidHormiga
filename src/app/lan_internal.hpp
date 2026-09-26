/* app/lan_internal.hpp — what the LAN-sharing files share with each other and
 * with nobody else: small file and JSON helpers, the members registry, and the
 * two thread entry points. Split out of lan_share.cpp on 2026-09-16, when the
 * file passed its length budget, along the seams it already had: the plan and
 * the GUI-thread operations (lan_share.cpp), the threads (lan_threads.cpp),
 * presence (lan_presence.cpp) and the CLI verbs (lan_cli.cpp).
 */
#pragma once

#include "app/app_internal.hpp"
#include "app/lan_share.hpp"
#include "app/paths.hpp"
#include "platform/miga.hpp"
#include "sync/peer.hpp"

#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <set>
#include <sstream>

namespace lan_detail {

namespace fs = std::filesystem;

/* Where this device listens for member sync: the share port's neighbour, or
 * HORMIGA_SYNC_PORT, which lets two "devices" run on one machine (a phone
 * played by the harness beside a CLI host). Without it, the second dialled its
 * own listener: same address, same port. */
inline int member_sync_port(int share_port) {
    if (const char* e = std::getenv("HORMIGA_SYNC_PORT"))
        if (const int p = std::atoi(e); p > 0 && p < 65536) return p;
    return share_port + 1;
}

/* One clock for the link threads and the screens that show their transfers. */
inline double now_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
using hormiga::lan::Offer;
using hormiga::lan::Plan;
using hormiga::lan::PlanItem;
using hormiga::lan::Request;
using json = nlohmann::json;
using RegisterFn = std::function<void(maiz::Core&)>;

/* A joined database arrives with the host's replica document under this name; the
 * joiner's first replica starts from it and deletes it (lan_sync.cpp). */
inline constexpr const char* kSyncSeedFile = "sync-start.replica.json";

inline std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline bool write_atomic(const fs::path& p, const std::string& bytes) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    fs::path tmp = p;
    tmp += ".part";
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        o << bytes;
        if (!o) return false;
    }
    fs::remove(p, ec);
    fs::rename(tmp, p, ec);
    return !ec;
}

inline void owner_only(const fs::path& p) {
#ifndef _WIN32
    std::error_code ec;
    fs::permissions(p, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace,
                    ec);
#else
    (void)p;
#endif
}

inline std::string u8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

inline fs::path from_u8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

inline std::string today() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char b[16];
    std::strftime(b, sizeof b, "%Y-%m-%d", &tm);
    return b;
}

inline std::string stamp() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char b[24];
    std::strftime(b, sizeof b, "%Y%m%d-%H%M%S", &tm);
    return b;
}

inline std::string dump(const json& j) {
    return j.dump(-1, ' ', false, json::error_handler_t::replace);
}

inline std::string jstr(const json& j, const char* k) {
    return j.is_object() && j.contains(k) && j[k].is_string() ? j[k].get<std::string>()
                                                              : std::string();
}

inline std::string unquote(std::string v) {
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    return v == "null" ? std::string() : v;
}

inline std::string config(maiz::Core& core, const std::string& key) {
    return unquote(core.dispatch("config get " + key).data);
}

inline std::vector<std::string> mantles_of(maiz::Core& core) {
    std::vector<std::string> out;
    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' ')) line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (!line.empty() && line != "(no mantles)") out.push_back(line);
    }
    return out;
}

inline maiz::Scene project(maiz::Core& core, const char* mantle) {
    maiz::ProjectOptions po;
    po.mantle = mantle;
    return maiz::project_scene(core, po);
}

inline void say(LanRuntime& rt, const std::string& level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(rt.mu);
    rt.thread_log.push_back({level, msg});
}

/* A string that may go into a shell command as a quoted argument. A URL and a
 * path both arrived from ANOTHER device, so they are checked rather than
 * escaped: anything outside this set is refused. */
inline bool shell_safe(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s)
        if (c < 0x20 || c == '"' || c == '`' || c == '$' || c == '\\' || c == '\'' || c == '!' ||
            c == '%' || c == '^' || c == '&' || c == '|' || c == '<' || c == '>')
            return false;
    return true;
}

inline bool url_ok(const std::string& u) {
    if (u.rfind("https://", 0) != 0 && u.rfind("http://", 0) != 0) return false;
    for (unsigned char c : u)
        if (!(std::isalnum(c) || std::string(":/._~-?=+#@,;").find((char)c) != std::string::npos))
            return false;
    return u.size() < 2000;
}

/* The members registry, as a Void Core document of its own. */
inline bool add_member_doc(const RegisterFn& reg, const fs::path& file, const Request& who,
                    const std::string& role, const std::string& invited_by, std::string& error) {
    const std::string doc = slurp(file);
    maiz::Core c = doc.empty() ? maiz::Core() : maiz::Core(doc);
    if (reg) reg(c);
    bool has = false;
    for (const auto& m : mantles_of(c)) has = has || m == "members";
    c.dispatch(has ? "use members" : "mantle new members");
    const std::string name = "m-" + who.fingerprint.substr(0, 12);
    const maiz::Scene sc = project(c, "members");
    const maiz::SceneNode* existing = sc.find(name);
    std::vector<std::string> cmds;
    if (!existing) cmds.push_back("rune new member " + name);
    auto set = [&](const char* f, const std::string& v) {
        if (!v.empty()) cmds.push_back("set " + name + " " + f + " " + json_str(v));
    };
    set("username", who.user);
    set("color", who.color);
    set("fingerprint", who.fingerprint);
    set("public_key", hormiga::lan::b64(who.public_key));
    if (!existing || hormiga::field_value(*existing, "role").empty()) set("role", role);
    if (!existing || hormiga::field_value(*existing, "joined").empty()) set("joined", today());
    if (!existing) set("invited_by", invited_by);
    set("last_seen", today());
    set("avatar", hormiga::lan::b64(who.avatar_png));
    if (!existing) cmds.push_back("tag " + name + " +type:member");
    for (const auto& cmd : cmds) {
        const maiz::Result r = c.dispatch(cmd);
        if (!r.ok) {
            error = "members registry refused `" + cmd.substr(0, 40) + "`: " + r.text();
            return false;
        }
    }
    if (!write_atomic(file, c.export_state())) {
        error = "cannot write " + u8(file);
        return false;
    }
    return true;
}

inline Request request_from(const hormiga::profile::Profile& p) {
    Request r;
    r.user = p.username;
    r.color = p.color;
    r.public_key = p.public_key;
    r.fingerprint = hormiga::sync::fingerprint_of(p.public_key);
    r.avatar_png = hormiga::profile::avatar_png(p);
#ifdef HORMIGA_VERSION
    r.app = HORMIGA_VERSION;
#endif
    return r;
}

inline bool is_timeout(const std::string& err) {
    return err.find("before the timeout") != std::string::npos;
}

struct HostJob {
    std::shared_ptr<LanRuntime> rt;
    hormiga::sync::KeyPair keys;
    int port = 0;
    Plan plan;
    json welcome;            // db + host, filled on the GUI thread
    fs::path members_file;
    RegisterFn reg;
    std::string host_user;
};

void host_loop(HostJob job);

struct JoinJob {
    std::shared_ptr<LanRuntime> rt;
    hormiga::sync::KeyPair keys;
    Offer offer;
    fs::path dest;
    Request me;
};

void join_loop(JoinJob job);

}  // namespace lan_detail
