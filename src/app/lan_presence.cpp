/* app/lan_presence.cpp — who else is here (lan-sharing.md §6): the beacon's
 * payload each second, sealed presence in and out, and the colour rule. */
#include "app/lan_internal.hpp"

namespace fs = std::filesystem;
using namespace lan_detail;

/* ── presence ───────────────────────────────────────────────────────────────── */

std::vector<const hormiga::lan::Activity*> LanRuntime::on_rune(const LanRuntime& rt, const std::string& rune) {
    std::vector<const hormiga::lan::Activity*> out;
    for (const auto& [fp, a] : rt.present)
        for (const auto& s : a.selection)
            if (s == rune) {
                out.push_back(&a);
                break;
            }
    return out;
}

std::string LanRuntime::color_of(const LanRuntime& rt, const std::string& fp) {
    auto it = rt.shown_color.find(fp);
    return it != rt.shown_color.end() ? it->second : std::string("#888888");
}

void LanRuntime::tick(HormigaApp& app, double now) {
    // nothing to do until something has used the runtime, except noticing a
    // database that has a room key (presence) -- checked once a second
    static double last_probe = -100.0;
    if (!app.lan) {
        if (now - last_probe < 2.0) return;
        last_probe = now;
        const auto share = hormiga::collab::share_settings(project(app.core, kAntfarmMantle));
        if (!share.presence || share.node.empty()) return;
        if (slurp(hormiga::find_key_file(share.key_file, app.key_dirs())).size() != 32) return;
    }
    LanRuntime& rt = of(app);

    // thread results into the log strip
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        for (const auto& [lvl, msg] : rt.thread_log) app.log.push_back({lvl, "share", msg});
        rt.thread_log.clear();
        if (!rt.joined_miga.empty() && !rt.joining && !rt.finishing) {
            rt.finishing = true;
            app.run_busy("Opening the shared database", [&app] { LanRuntime::finish_join(app); });
        }
    }
    if (now - rt.checked_at < 1.0) return;
    rt.checked_at = now;

    const maiz::Scene farm = project(app.core, kAntfarmMantle);
    const auto share = hormiga::collab::share_settings(farm);
    const std::vector<fs::path> dirs = app.key_dirs();
    const std::string which = u8(dirs.front()) + "|" + app.state_name + "|" + share.key_file;
    if (rt.room_for != which) {
        rt.room_for = which;
        const std::string k = slurp(hormiga::find_key_file(share.key_file, dirs));
        rt.room_key = k.size() == 32 ? k : std::string();
        const auto ms = hormiga::collab::membership_settings(farm);
        rt.members_file = hormiga::find_key_file(ms.file, dirs);
        rt.members_read_at = -100.0;
        rt.present.clear();
    }
    const bool presence = share.presence && !rt.room_key.empty() && !rt.me.username.empty();
    const bool want = rt.sharing || rt.discovering || presence;
    if (!want) {
        if (rt.beacon) rt.beacon->stop();
        rt.beacon.reset();
        rt.present.clear();
        return;
    }

    const std::string fp = fingerprint(rt);
    hormiga::sync::PeerInfo self;
    self.peer_id = (rt.sharing || presence) ? "hz" + fp : std::string();  // discovering alone stays quiet
    self.display = rt.me.username;
    self.fingerprint = fp;
    self.port = (std::uint16_t)share.port;
    std::string sealed, room;
    if (presence) {
        hormiga::lan::Activity me;
        me.fingerprint = fp;
        me.user = rt.me.username;
        me.color = rt.me.color;
        static const char* kSections[] = {"Data", "Builder", "Antfarm", "Map"};
        me.section = app.section >= 0 && app.section < 4 ? kSections[app.section] : "";
        me.mantle = app.scene.mantle;
        me.selection = app.ed.selection;
        sealed = hormiga::lan::seal_activity(me, rt.room_key);
        room = hormiga::lan::room_id(rt.room_key);
    }
    Offer offer;
    if (rt.sharing) {
        offer.db = config(app.core, "org.name");
        if (offer.db.empty() && !rt.plan.items.empty()) offer.db = fs::path(rt.plan.items.front().rel).stem().string();
        offer.description = config(app.core, "org.description");
        offer.user = rt.me.username;
        offer.color = rt.me.color;
        offer.port = share.port;
    }
    self.extra = hormiga::lan::beacon_extra(rt.sharing ? &offer : nullptr, room, sealed);
    if (!rt.beacon) {
        rt.beacon = std::make_unique<hormiga::sync::Beacon>();
        std::string err;
        if (!rt.beacon->start(self, &err)) {
            app.log.push_back({"warn", "share", "the network beacon would not start: " + err});
            rt.beacon.reset();
            return;
        }
    } else {
        rt.beacon->announce(self);
    }

    // presence in: members of this room only
    if (presence) {
        for (const auto& peer : rt.beacon->peers(12)) {
            hormiga::lan::ExtraParts parts;
            if (!hormiga::lan::read_extra(peer.extra, parts) || parts.room != room) continue;
            hormiga::lan::Activity a;
            if (!hormiga::lan::open_activity(parts.sealed, rt.room_key, a) || a.fingerprint == fp) continue;
            a.seen = peer.last_seen;
            rt.present[a.fingerprint] = a;
        }
    }
    const std::int64_t cut = (std::int64_t)std::time(nullptr) - 12;
    for (auto it = rt.present.begin(); it != rt.present.end();)
        it = it->second.seen < cut ? rt.present.erase(it) : std::next(it);

    // colours, against the registry
    if (now - rt.members_read_at > 5.0) {
        rt.members_read_at = now;
        rt.member_rows = rt.members_file.empty() ? decltype(rt.member_rows){} : members(app, rt.members_file);
        std::error_code ec;
        const fs::path cache = hormiga::profile::dir() / "members-cache";
        for (const auto& row : rt.member_rows) {
            auto a = row.find("avatar"), f = row.find("fingerprint");
            if (a == row.end() || f == row.end() || a->second.empty() || f->second.empty()) continue;
            const fs::path png = cache / (f->second + ".png");
            if (!fs::exists(png, ec)) {
                fs::create_directories(cache, ec);
                write_atomic(png, hormiga::lan::unb64(a->second));
            }
            rt.member_avatar[f->second] = u8(png);
        }
    }
    std::vector<hormiga::collab::Present> people;
    auto joined_of = [&](const std::string& key) {
        for (const auto& row : rt.member_rows) {
            auto f = row.find("fingerprint"), j = row.find("joined");
            if (f != row.end() && f->second == key && j != row.end()) return j->second;
        }
        return std::string();
    };
    people.push_back({fp, rt.me.color, joined_of(fp)});
    for (const auto& [k, a] : rt.present) people.push_back({k, a.color, joined_of(k)});
    rt.shown_color = hormiga::collab::assign_colors(people);
}

