/* app/lan_presence.cpp — who else is here (lan-sharing.md §6): the beacon's
 * payload each second, sealed presence in and out, and the colour rule. */
#include "app/lan_internal.hpp"

namespace fs = std::filesystem;
using namespace lan_detail;

/* ── presence ───────────────────────────────────────────────────────────────── */

/* BY ID WHEN THE PEER SENT IDS (2026-09-19). The author made a PRIVATE note on
 * one device and a shared note on another, both named `note-1`, and the private
 * one lit up with the other person's colour: a name is a handle two members can
 * both mint, and matching on it cannot tell their runes apart. */
std::vector<const hormiga::lan::Activity*> LanRuntime::on_rune(const LanRuntime& rt, const std::string& rune,
                                                                const std::string& id) {
    std::vector<const hormiga::lan::Activity*> out;
    for (const auto& [fp, a] : rt.present) {
        const auto& keys = a.ids.empty() || id.empty() ? a.selection : a.ids;
        const std::string& want = a.ids.empty() || id.empty() ? rune : id;
        if (std::find(keys.begin(), keys.end(), want) != keys.end()) out.push_back(&a);
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
        static const bool trace = std::getenv("HORMIGA_SYNC_TRACE") != nullptr; // a harness reads it
        for (const auto& [lvl, msg] : rt.thread_log) {
            app.log.push_back({lvl, "share", msg});
            if (trace) std::fprintf(stderr, "[%s] share: %s\n", lvl.c_str(), msg.c_str());
        }
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
        rt.listening = false;  // the member-sync listener stops with presence
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
        me.sync_port = lan_detail::member_sync_port(share.port);   // where this device listens for member sync
        me.selection = app.ed.selection;
        for (const auto& s : me.selection) {
            const auto* n = app.scene.find(s);
            me.ids.push_back(n ? n->id : std::string());
        }
        /* WHAT WE ACTUALLY SEND, from 0.1.5: Void Maiz composes it from this
         * frame's surfaces, applies the sender's switches, and drops every
         * selected rune the share filter keeps on this device. The legacy
         * fields above go out beside it for a 0.1.4 peer. */
        maiz::Profile self_profile;
        self_profile.id = fp;
        self_profile.name = rt.me.username;
        self_profile.rgb = hormiga::collab::rgb_of(rt.me.color);
        me.presence = maiz::presence_to_json(
            maiz::compose_presence(self_profile, maiz::selection_ids(app.scene, app.ed.selection),
                                   app.surfaces, app.net_settings.send, app.scene, app.share_now));
        me.version = rt.shown_version;  // members compare this before connecting (§3b)
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
            a.address = peer.address;
            a.heard = peer.heard;
            rt.present[a.fingerprint] = a;
            /* CONNECTION STRENGTH (2026-09-25): a member announces every 3 s, so
             * over the last ~30 s ten beacons should have arrived. How many did
             * is how much of this link works; a phone at the edge of the Wi-Fi
             * loses datagrams long before it loses the member. */
            auto mark = rt.heard_mark.find(a.fingerprint);
            if (mark == rt.heard_mark.end()) {
                rt.heard_mark[a.fingerprint] = {now, a.heard};
            } else if (now - mark->second.first >= 30.0) {
                const float expected = (float)((now - mark->second.first) / 3.0);
                rt.strength[a.fingerprint] =
                    std::clamp((float)(a.heard - mark->second.second) / std::max(1.0f, expected), 0.0f, 1.0f);
                mark->second = {now, a.heard};
            }
        }
    }
    const std::int64_t cut = (std::int64_t)std::time(nullptr) - 12;
    for (auto it = rt.present.begin(); it != rt.present.end();)
        it = it->second.seen < cut ? rt.present.erase(it) : std::next(it);

    /* ── THE ROSTER IS WHAT THE VIEWS READ (Void Maiz, stage A) ──────────────
     * A peer that sends Maiz presence is taken at its word about surfaces and
     * selection; a 0.1.4 peer is rebuilt from the fields it does send. WHO it
     * is comes from the transport either way -- the fingerprint we opened the
     * sealed payload with -- never from the payload, or a peer could speak as
     * another. The colour is the members registry's, so no two people are
     * shown alike whatever they each prefer. */
    app.roster.set_self(fp);
    for (const auto& [peer_fp, a] : rt.present) {
        maiz::PresenceState ps;
        if (a.presence.empty() || !maiz::presence_from_json(a.presence, ps)) {
            ps = {};
            ps.selection = a.ids.empty() ? a.selection : a.ids;
            if (!a.section.empty()) { ps.surfaces = {a.section}; ps.focus = a.section; }
        }
        ps.who.id = peer_fp;
        ps.who.name = a.user;
        ps.who.rgb = hormiga::collab::rgb_of(color_of(rt, peer_fp));
        app.roster.update(ps, now);
    }
    app.roster.prune(now, 12.0);

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
    sync_tick(app, now, presence);
}

