/* app/lan_net.cpp — stage C: the sync seam is Void Maiz's `maiz::Network`.
 *
 * Void Maiz's 2026-09-19 message, §3. What this file is NOT is a sync
 * implementation: the replica loop, the splice, the export set, the conflicts
 * and the file transfer are `voidmaiz_net` over Void Palabra's session. What
 * this file is: the four things an application still owns.
 *
 *   1. WHERE THE REPLICA IS SAVED, and saving it before anything leaves
 *      (`persist` is not optional -- a delta sent before the save re-mints tags
 *      after a crash).
 *   2. WHAT MAY LEAVE: the Antfarm's private tags, and the Antfarm itself
 *      (lan-sharing.md §3a), as the one `ShareFilter` presence reads too.
 *   3. WHICH FIELDS NAME FILES, and where those files live on this device.
 *   4. MOVING BYTES. `Network` opens no socket; our room-key-sealed LAN session
 *      carries its frames. The author decided on 2026-09-19 that it may, **on
 *      the LAN only**, before Palabra's trust model lands.
 *
 * THE THREADS ONLY MOVE BYTES. `Network` is not thread-safe and the replica
 * under it is not, so every call into it happens on the frame thread; a link
 * thread pops from an outbox and pushes to an inbox, both under `rt.mu`.
 */
#include "app/lan_internal.hpp"

#ifdef HORMIGA_HAVE_NET

#include "voidpalabra/references.hpp"

namespace fs = std::filesystem;
using namespace lan_detail;

namespace {

double now_seconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

fs::path maiz_replica_path(const std::string& database) {
    return hormiga::profile::dir() / "replicas" / (database + ".maiz");
}

/* An asset's ADDRESS is the sha256 in its filename, and the file it names is
 * `assets/<anything>-<address>.<ext>`. `ingest_asset` writes that name from
 * 0.1.5 on (app.cpp); a file ingested by an older version carries an 8-hex FNV
 * name, is not an address, and so is never asked for -- which is exactly the
 * behaviour those databases have today. */
fs::path asset_for_address(const fs::path& dir, const std::string& address) {
    std::error_code ec;
    if (address.size() != 64 || !fs::is_directory(dir, ec)) return {};
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec)) continue;
        if (e.path().filename().string().find(address) != std::string::npos) return e.path();
    }
    return {};
}

}  // namespace

/* ── the network, opened once per database ──────────────────────────────────── */

bool LanRuntime::net_open(HormigaApp& app) {
    LanRuntime& rt = of(app);
    const std::string db = database_id(app, true);
    if (rt.net && rt.net_for == db) return true;

    voidpalabra::Replica replica;
    const std::string bytes = slurp(maiz_replica_path(db));
    std::string why;
    bool ok = bytes.empty()
                  ? voidpalabra::Replica::create(hormiga::lan::random_id("hz-"), replica, &why)
                  : voidpalabra::Replica::from_bytes(bytes, replica, &why);
    if (!ok) {
        app.log.push_back({"error", "sync", "this database's sync record will not open: " + why});
        return false;
    }

    maiz::NetOptions o;
    const fs::path replica_file = maiz_replica_path(db);
    o.persist = [replica_file](const std::string& b) {
        std::error_code ec;
        fs::create_directories(replica_file.parent_path(), ec);
        write_atomic(replica_file, b);
    };
    o.share = app.share_filter();
    // THE ANTFARM DOES NOT TRAVEL (lan-sharing.md §3a): wiring and keys come
    // from the host. The author's Q78 may reverse this; one line, here.
    o.share_mantle = [](const std::string& mantle) { return mantle != kAntfarmMantle; };

    /* The fields that name files. `path` is an image's and a resource's; a
     * contact's picture is `photo`. Anything else stays a string nobody fetches. */
    for (const char* field : {"content.path", "content.photo"})
        o.files.references.fields[field] = voidpalabra::sha256_hex_anywhere();
    HormigaApp* ap = &app;
    const fs::path assets = app.assets_dir();   // the database's folder, resolved once
    o.files.have = [assets](const std::string& address) {
        return !asset_for_address(assets, address).empty();
    };
    o.files.read = [assets](const std::string& address, std::string& out) {
        const fs::path p = asset_for_address(assets, address);
        if (p.empty()) return false;
        out = slurp(p);
        return !out.empty();
    };
    o.files.store = [ap, assets](const std::string& address, const std::string& data) {
        /* Palabra verified the bytes against the address before this is called.
         * The name keeps the address, because that is what makes it findable
         * again; the extension is whatever the document's path said. */
        std::error_code ec;
        fs::create_directories(assets, ec);
        write_atomic(assets / (address + ".bin"), data);
        ap->log.push_back({"info", "sync", "a file arrived from a member (" +
                                               address.substr(0, 8) + ")"});
    };
    // sign/verify stay empty: the room key is the trust boundary today, and the
    // author's decision above is scoped to the LAN because of it.

    rt.net = std::make_unique<maiz::Network>(app.core, std::move(replica), std::move(o));
    rt.net_for = db;
    rt.net->settings() = app.net_settings;
    return true;
}

void LanRuntime::net_close(LanRuntime& rt) {
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.link_io.clear();
    }
    rt.net.reset();
    rt.net_for.clear();
}

/* ── the frame thread's half ────────────────────────────────────────────────── */

void LanRuntime::net_tick(HormigaApp& app, double now) {
    LanRuntime& rt = of(app);
    if (!rt.net) return;
    const maiz::NetMillis ms = (maiz::NetMillis)(now * 1000.0);

    // what the links received since the last frame
    std::vector<std::pair<std::string, std::string>> arrived;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        for (auto& [link, io] : rt.link_io) {
            for (auto& frame : io.in) arrived.push_back({link, std::move(frame)});
            io.in.clear();
        }
    }
    for (auto& [link, frame] : arrived) rt.net->receive(link, frame, ms);

    rt.net->settings().send = app.net_settings.send;
    rt.net->settings().show = app.net_settings.show;
    rt.net->settings().cautious_files = app.net_settings.cautious_files;
    rt.net->tick(ms, maiz::selection_ids(app.scene, app.ed.selection), app.surfaces);

    // frames out, to whichever link thread is carrying them
    auto outgoing = rt.net->take_outgoing();
    if (!outgoing.empty()) {
        std::lock_guard<std::mutex> lk(rt.mu);
        for (auto& o : outgoing) rt.link_io[o.link].out.push_back(std::move(o.frame));
    }
    for (const auto& n : rt.net->take_notes())
        app.log.push_back({n.level, "sync", n.link.empty() ? n.text : n.text + " (" + n.link + ")"});
    if (rt.net->take_spliced()) {
        /* A MERGE CHANGED THE DOCUMENT. Void Maiz splices `mantles` and
         * `glyphs` into the core we handed it, so there is no state to swap --
         * only the projection to redo. Undo history is gone by then, and must
         * be: a memento from before a peer's change would undo THEIR work. */
        app.reproject();
        if (!rt.headless) app.do_save();
        app.toast("a member's changes arrived");
    }
    /* What the beacon advertises, so a member can tell at a glance whether the
     * other device is behind: Palabra's version name over the flattened doc. */
    try {
        rt.shown_version = voidpalabra::version_name(rt.net->replica().flatten().root);
    } catch (...) {
        rt.shown_version.clear();
    }
}

/* ── one link, one thread: bytes in, bytes out ──────────────────────────────── */

void LanRuntime::net_pump(std::shared_ptr<LanRuntime> rt, hormiga::sync::Session* session,
                          const std::string& link, double until) {
    session->set_timeout_ms(700);
    std::string err;
    while (session->open() && now_seconds() < until) {
        std::vector<std::string> send_now;
        {
            std::lock_guard<std::mutex> lk(rt->mu);
            auto it = rt->link_io.find(link);
            if (it == rt->link_io.end()) break;         // the frame thread dropped it
            send_now.swap(it->second.out);
        }
        bool broken = false;
        for (const auto& frame : send_now)
            if (!session->send(frame, &err)) { broken = true; break; }
        if (broken) break;
        std::string in;
        if (session->receive(in, &err, hormiga::lan::kFileChunk * 4)) {
            std::lock_guard<std::mutex> lk(rt->mu);
            auto it = rt->link_io.find(link);
            if (it == rt->link_io.end()) break;
            it->second.in.push_back(std::move(in));
        } else if (err.find("timed out") == std::string::npos) {
            break;  // a real failure; a quiet socket is not one (peer.cpp)
        }
    }
    session->close();
    std::lock_guard<std::mutex> lk(rt->mu);
    rt->link_io.erase(link);
}


/* ── opening and closing links ──────────────────────────────────────────────── */

/* Called every tick from the frame thread. Starts a link to each member who is
 * present and behind us or ahead of us, tells `Network` about links the
 * listener accepted, and tells it about the ones that went away.
 *
 * WHO DIALS: the lower fingerprint, the same rule the 0.1.4 exchange used. Two
 * devices that both dial would hold two sessions for one pair of replicas. */
void LanRuntime::net_links(HormigaApp& app, double now) {
    LanRuntime& rt = of(app);
    if (!rt.net) return;
    const maiz::NetMillis ms = (maiz::NetMillis)(now * 1000.0);
    const std::string mine = fingerprint(rt);
    const hormiga::sync::KeyPair keys{rt.me.public_key, rt.me.secret_key};

    std::set<std::string> members;
    for (const auto& row : rt.member_rows)
        if (auto f = row.find("fingerprint"), l = row.find("left");
            f != row.end() && (l == row.end() || l->second.empty()))
            members.insert(f->second);

    std::vector<std::string> to_open, to_close;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        for (auto& [link, io] : rt.link_io)
            if (!io.connected) { io.connected = true; to_open.push_back(link); }
        for (const auto& s : rt.net->links())
            if (!rt.link_io.count(s.link)) to_close.push_back(s.link);
    }
    for (const auto& link : to_open) rt.net->connect(link, ms);
    for (const auto& link : to_close) rt.net->disconnect(link, ms);

    // dial the members who are here and are not already carried
    for (const auto& [fp, a] : rt.present) {
        if (a.address.empty() || !members.count(fp) || !(mine < fp)) continue;
        {
            std::lock_guard<std::mutex> lk(rt.mu);
            if (rt.link_io.count(fp)) continue;
            rt.link_io[fp].carrying = true;
        }
        auto started = rt.exchange_started.find(fp);
        if (started != rt.exchange_started.end() && now - started->second < 4.0) {
            std::lock_guard<std::mutex> lk(rt.mu);
            rt.link_io.erase(fp);
            continue;
        }
        rt.exchange_started[fp] = now;
        std::thread(net_dial, app.lan, keys, a,
                    a.sync_port > 0 ? a.sync_port : hormiga::collab::kMemberSyncPort)
            .detach();
    }
}

/* The dialling half: reach the peer, prove it is who the beacon said, then move
 * bytes until the round goes quiet. Nothing here touches `Network`. */
void LanRuntime::net_dial(std::shared_ptr<LanRuntime> rt, hormiga::sync::KeyPair keys,
                          hormiga::lan::Activity peer, int port) {
    hormiga::sync::Session s;
    std::string pk, sas, err;
    /* PATIENTLY: the listener rebinds between accepts, so a dial that lands in
     * that window is refused and means nothing. Six tries over three seconds. */
    bool ok = false;
    for (int i = 0; i < 6 && !ok; ++i) {
        ok = s.connect(peer.address, (std::uint16_t)port, keys, pk, sas, &err);
        if (!ok) std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    const std::string link = peer.fingerprint;
    if (!ok || hormiga::sync::fingerprint_of(pk) != link) {
        std::lock_guard<std::mutex> lk(rt->mu);
        if (!ok) rt->thread_log.push_back({"warn", "sync: could not reach " + peer.user + ": " + err});
        else rt->thread_log.push_back({"warn", "sync: the device at " + peer.address +
                                                   " is not " + peer.user + " - nothing sent"});
        rt->link_io.erase(link);
        return;
    }
    {
        std::lock_guard<std::mutex> lk(rt->mu);
        rt->thread_log.push_back({"info", "sync: reached " + peer.user + ", carrying frames"});
    }
    net_pump(rt, &s, link, now_seconds() + 25.0);
}

/* The listening half. A link is accepted only from a member of this database;
 * the frame thread learns about it from `link_io` and opens the session side. */
void LanRuntime::net_listen(std::shared_ptr<LanRuntime> rt, hormiga::sync::KeyPair keys, int port) {
    while (rt->listening) {
        auto carried = std::make_shared<hormiga::sync::Session>();
        std::string pk, sas, err;
        if (!carried->accept_one((std::uint16_t)port, keys, pk, sas, 1000, &err)) continue;
        const std::string link = hormiga::sync::fingerprint_of(pk);
        {
            std::lock_guard<std::mutex> lk(rt->mu);
            if (!rt->member_fps.count(link) || rt->link_io.count(link)) continue;
            rt->link_io[link].carrying = true;
            rt->thread_log.push_back({"info", "sync: a member connected, carrying frames"});
        }
        /* THE LISTENER KEEPS LISTENING. Pumping here would mean nobody is bound
         * for as long as one link lives, so a second member -- or the same one
         * reconnecting -- would dial a closed port and the failure would look
         * like a network problem. The session moves into its own thread. */
        std::thread([rt, carried, link] {
            net_pump(rt, carried.get(), link, now_seconds() + 25.0);
        }).detach();
    }
}

#endif  // HORMIGA_HAVE_NET
