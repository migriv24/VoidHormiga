/* app/lan_sync.cpp — members keeping one database in sync, automatically.
 *
 * okf/concepts/platform/lan-sharing.md §3b. The author: *"i would want syncing to
 * be automatic … as long as it WORKS correctly."* It could not work correctly
 * until Void Palabra shipped the replica (2026-09-16): our old merge enriched both
 * current states on every exchange, so a deletion came back, and on a timer it
 * would come back forever.
 *
 * THE LOOP, on the GUI thread except where it says otherwise:
 *
 *   1. every few seconds, if the document changed: strip the private runes,
 *      `observe`, SAVE the replica, publish the document for the listener;
 *   2. presence carries the version each member's replica shows, sealed to the
 *      room -- so nobody connects unless two members actually differ;
 *   3. when they differ, the member with the LOWER key fingerprint connects
 *      (a thread): both send their whole document, with progress;
 *   4. at the start of the next frame each received document is merged: observe
 *      first (so an edit made in between is recorded, not overwritten), merge,
 *      save, splice -- mantles and glyphs only, private runes put back -- and swap
 *      the core, keeping what the person was looking at;
 *   5. conflicts are listed in the Share window; nothing is resolved silently.
 *
 * Palabra's list of what an automatic loop gets wrong, and where each is handled:
 * conflicts are never written back as decisions (their `observe`); idle ticks mint
 * nothing (theirs, and ours skips an unchanged document entirely); a delete that
 * raced an edit is shown (`conflicts`); the replica never travels with a join (it
 * lives in the profile folder, not the database); a copied replica forks on
 * `identity_collision`; the replica is saved before anything is sent; undo cannot
 * revert another member's change because the swapped-in core starts with an empty
 * undo stack; whole documents are always exchanged, so a lost delta cannot happen.
 */
#include "app/lan_internal.hpp"

#include <functional>

namespace fs = std::filesystem;
using namespace lan_detail;

namespace {

constexpr long long kMaxDoc = 512LL << 20;  // a member's document larger than this is refused

fs::path replica_path(const std::string& database) {
    return hormiga::profile::dir() / "replicas" / (database + ".replica");
}

void set_progress(LanRuntime& rt, const std::string& fp, const std::string& user, long long done,
                  long long total, bool sending) {
    std::lock_guard<std::mutex> lk(rt.mu);
    rt.progress[fp] = {user, done, total, sending};
}

bool send_doc(LanRuntime& rt, hormiga::sync::Session& s, const std::string& fp, const std::string& user,
              const std::string& doc, std::string& err) {
    for (std::size_t off = 0; off < doc.size(); off += hormiga::lan::kFileChunk) {
        if (!s.send(doc.substr(off, hormiga::lan::kFileChunk), &err)) return false;
        set_progress(rt, fp, user, (long long)std::min(doc.size(), off + hormiga::lan::kFileChunk),
                     (long long)doc.size(), true);
    }
    return true;
}

bool receive_doc(LanRuntime& rt, hormiga::sync::Session& s, const std::string& fp, const std::string& user,
                 long long total, std::string& doc, std::string& err) {
    doc.clear();
    if (total < 0 || total > kMaxDoc) {
        err = "a member announced a document larger than this version accepts";
        return false;
    }
    while ((long long)doc.size() < total) {
        std::string chunk;
        if (!s.receive(chunk, &err, hormiga::lan::kFileChunk)) return false;
        if ((long long)(doc.size() + chunk.size()) > total) {
            err = "a member sent more than it announced";
            return false;
        }
        doc += chunk;
        set_progress(rt, fp, user, (long long)doc.size(), total, false);
    }
    return true;
}

long long bytes_of(const json& j) {
    return j.contains("bytes") && j["bytes"].is_number_integer() ? j["bytes"].get<long long>() : -1;
}

/* The member who connects: send theirs-is-different, both documents, done. */
void exchange(std::shared_ptr<LanRuntime> rt, hormiga::sync::KeyPair keys, hormiga::lan::Activity peer,
              std::string my_user, int port) {
    const std::string fp = peer.fingerprint;
    std::string doc;
    {
        std::lock_guard<std::mutex> lk(rt->mu);
        doc = rt->outgoing_doc;
    }
    hormiga::sync::Session s;
    std::string pk, sas, err;
    bool ok = false;
    for (int i = 0; i < 4 && !ok; ++i) {
        ok = s.connect(peer.address, (std::uint16_t)port, keys, pk, sas, &err);
        if (!ok) std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }
    auto finish = [&](const std::string& level, const std::string& msg) {
        std::lock_guard<std::mutex> lk(rt->mu);
        rt->progress.erase(fp);
        if (!msg.empty()) rt->thread_log.push_back({level, msg});
    };
    if (!ok) return finish("warn", "sync: could not reach " + peer.user + ": " + err);
    if (hormiga::sync::fingerprint_of(pk) != fp)
        return finish("warn", "sync: the device at " + peer.address + " is not " + peer.user + " - nothing sent");
    s.set_timeout_ms(30000);
    set_progress(*rt, fp, peer.user, 0, (long long)doc.size(), true);
    if (!s.send(dump({{"t", "sync"}, {"user", my_user}, {"bytes", (long long)doc.size()}}), &err) ||
        !send_doc(*rt, s, fp, peer.user, doc, err))
        return finish("warn", "sync with " + peer.user + " stopped: " + err);
    std::string hello, theirs;
    if (!s.receive(hello, &err, 65536)) return finish("warn", "sync with " + peer.user + " stopped: " + err);
    const json h = json::parse(hello, nullptr, false);
    if (jstr(h, "t") != "sync" || !receive_doc(*rt, s, fp, peer.user, bytes_of(h), theirs, err))
        return finish("warn", "sync with " + peer.user + " stopped: " + (err.empty() ? "unexpected reply" : err));
    {
        std::lock_guard<std::mutex> lk(rt->mu);
        rt->incoming.push_back({peer.user, std::move(theirs)});
    }
    finish("info", "");
}

/* The member who is connected to: only members of this database are answered. */
void listen_loop(std::shared_ptr<LanRuntime> rt, hormiga::sync::KeyPair keys, int port) {
    while (rt->listening) {
        hormiga::sync::Session s;
        std::string pk, sas, err;
        if (!s.accept_one((std::uint16_t)port, keys, pk, sas, 800, &err)) {
            if (!is_timeout(err)) std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        const std::string fp = hormiga::sync::fingerprint_of(pk);
        {
            std::lock_guard<std::mutex> lk(rt->mu);
            if (!rt->member_fps.count(fp)) {
                rt->thread_log.push_back({"warn", "sync: refused a device that is not a member (" +
                                                      fp.substr(0, 8) + ")"});
                continue;
            }
        }
        s.set_timeout_ms(30000);
        std::string hello, theirs, mine;
        if (!s.receive(hello, &err, 65536)) continue;
        const json h = json::parse(hello, nullptr, false);
        const std::string user = jstr(h, "user").empty() ? fp.substr(0, 8) : jstr(h, "user");
        if (jstr(h, "t") != "sync" || !receive_doc(*rt, s, fp, user, bytes_of(h), theirs, err)) {
            std::lock_guard<std::mutex> lk(rt->mu);
            rt->progress.erase(fp);
            rt->thread_log.push_back({"warn", "sync from " + user + " stopped: " + err});
            continue;
        }
        {
            std::lock_guard<std::mutex> lk(rt->mu);
            mine = rt->outgoing_doc;
        }
        if (s.send(dump({{"t", "sync"}, {"bytes", (long long)mine.size()}}), &err)) send_doc(*rt, s, fp, user, mine, err);
        std::lock_guard<std::mutex> lk(rt->mu);
        rt->progress.erase(fp);
        rt->incoming.push_back({user, std::move(theirs)});
    }
}

}  // namespace

/* ── the database's identity, and its replica ───────────────────────────────── */

std::string LanRuntime::database_id(HormigaApp& app, bool create) {
    std::string id = config(app.core, "sync.database");
    if (id.empty() && create) {
        id = hormiga::lan::random_id("db-");
        app.core.dispatch("config set sync.database " + id);
    }
    return id;
}

/* What a splice puts back from this device: its private runes, and its whole
 * Antfarm, which member sync never merges (lan-sharing.md §3a). */
std::set<std::pair<std::string, std::string>> LanRuntime::private_keys(HormigaApp& app) {
    std::set<std::pair<std::string, std::string>> keep;
    const auto share = hormiga::collab::share_settings(project(app.core, kAntfarmMantle));
    for (const auto& mt : mantles_of(app.core))
        for (const auto& n : project(app.core, mt.c_str()).nodes)
            if (hormiga::collab::is_private(n, share) || mt == kAntfarmMantle) keep.insert({mt, n.id});
    return keep;
}

bool LanRuntime::sync_prepare(HormigaApp& app, double now, bool force) {
    LanRuntime& rt = of(app);
    const std::string db = database_id(app, true);
    std::string why;
    if (!rt.replica || rt.replica_for != db) {
        auto r = std::make_unique<hormiga::sync::SharedReplica>();
        const std::string bytes = slurp(replica_path(db));
        if (!r->open(bytes, hormiga::lan::random_id("hz-"), &why)) {
            app.log.push_back({"error", "sync", "this database's sync record will not open: " + why});
            return false;
        }
        /* A JOINED DATABASE'S FIRST REPLICA starts from the host's document, under
         * this device's own new id, so the two share one history from the start. */
        const fs::path seed = hormiga::find_key_file(kSyncSeedFile, app.key_dirs());
        if (bytes.empty() && fs::is_regular_file(seed)) {
            if (r->merge(slurp(seed), &why) == hormiga::sync::SharedReplica::Merge::ok) {
                app.log.push_back({"info", "sync", "the shared history starts from the host's"});
                std::error_code ec;
                fs::remove(seed, ec);
            } else {
                app.log.push_back({"warn", "sync", "the host's starting history was not usable: " + why});
            }
        }
        rt.replica = std::move(r);
        rt.replica_for = db;
        rt.observed_hash = 0;
    }
    const std::string state = app.core.export_state();
    const std::size_t h = std::hash<std::string>{}(state);
    if (!force && h == rt.observed_hash) return true;  // an idle tick costs one hash
    const auto share = hormiga::collab::share_settings(project(app.core, kAntfarmMantle));
    const std::string shared = strip_private(app, state, share, nullptr, true);
    std::size_t changes = 0;
    if (!rt.replica->observe(shared, &changes, &why)) {
        app.log.push_back({"error", "sync", "could not record this device's changes: " + why});
        return false;
    }
    rt.observed_hash = h;
    rt.prepared_at = now;
    // SAVED BEFORE ANYTHING LEAVES THE DEVICE (Palabra: a crash after sending and
    // before saving would reuse tags)
    if (changes || !fs::exists(replica_path(db)))
        write_atomic(replica_path(db), rt.replica->to_bytes());
    rt.shown_version = rt.replica->shown_version();
    std::lock_guard<std::mutex> lk(rt.mu);
    rt.outgoing_doc = rt.replica->doc_json();
    return true;
}

void LanRuntime::sync_tick(HormigaApp& app, double now, bool presence) {
    LanRuntime& rt = of(app);
    if (!presence) {
        rt.listening = false;
        return;
    }
    if (now - rt.prepared_at > 3.0 && !sync_prepare(app, now, false)) return;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        rt.member_fps.clear();
        for (const auto& row : rt.member_rows)
            if (auto f = row.find("fingerprint"), l = row.find("left");
                f != row.end() && (l == row.end() || l->second.empty()))
                rt.member_fps.insert(f->second);
    }
    const hormiga::sync::KeyPair keys{rt.me.public_key, rt.me.secret_key};
    if (!rt.listening) {
        rt.listening = true;
        std::thread(listen_loop, app.lan, keys, hormiga::collab::kMemberSyncPort).detach();
    }
    const std::string mine = fingerprint(rt);
    for (const auto& [fp, a] : rt.present) {
        if (a.version.empty() || a.version == rt.shown_version || a.address.empty()) continue;
        if (!(mine < fp)) continue;  // the lower fingerprint connects; the other one waits
        {
            std::lock_guard<std::mutex> lk(rt.mu);
            if (rt.progress.count(fp) || !rt.member_fps.count(fp)) continue;
            rt.progress[fp] = {a.user, 0, 0, true};
        }
        auto started = rt.exchange_started.find(fp);
        if (started != rt.exchange_started.end() && now - started->second < 6.0) {
            std::lock_guard<std::mutex> lk(rt.mu);
            rt.progress.erase(fp);
            continue;
        }
        rt.exchange_started[fp] = now;
        std::thread(exchange, app.lan, keys, a, rt.me.username, hormiga::collab::kMemberSyncPort).detach();
    }
}

/* ── landing what arrived ───────────────────────────────────────────────────── */

void LanRuntime::swap_state(HormigaApp& app, const std::string& state) {
    const std::vector<std::string> selection = app.ed.selection;
    app.core = maiz::Core(state);
    app.install_host();  // the new core needs the effect handler and the log sink again
    app.channel_fields.clear();
    if (app.on_register_glyphs) app.on_register_glyphs(app.core);
    app.core.dispatch("config set actor human:hormiga");
    app.reproject();
    // keep what the person had selected, where it still exists
    app.ed.selection.clear();
    for (const auto& s : selection)
        if (app.scene.find(s)) app.ed.selection.push_back(s);
    if (!app.lan || !app.lan->headless) app.do_save();
}

void LanRuntime::apply_incoming(HormigaApp& app) {
    if (!app.lan) return;
    LanRuntime& rt = *app.lan;
    std::vector<std::pair<std::string, std::string>> arrived;
    {
        std::lock_guard<std::mutex> lk(rt.mu);
        arrived.swap(rt.incoming);
    }
    if (arrived.empty() || !sync_prepare(app, ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0, true)) return;
    const std::string before = rt.replica->shown_version();
    const std::size_t conflicts_before = rt.conflicts.size();
    std::vector<std::string> from;
    std::string why;
    for (const auto& [user, doc] : arrived) {
        auto r = rt.replica->merge(doc, &why);
        if (r == hormiga::sync::SharedReplica::Merge::identity_collision) {
            /* This replica's id is in use somewhere else -- a copied profile folder,
             * a restored backup. Fork to a fresh id and merge again (Palabra's fix). */
            app.log.push_back({"warn", "sync", "this device's sync identity was copied or restored; "
                                               "it has a new one now"});
            if (rt.replica->fork(hormiga::lan::random_id("hz-"), &why))
                r = rt.replica->merge(doc, &why);
        }
        if (r == hormiga::sync::SharedReplica::Merge::ok) from.push_back(user);
        else app.log.push_back({"warn", "sync", "did not take " + user + "'s changes: " + why});
    }
    write_atomic(replica_path(rt.replica_for), rt.replica->to_bytes());
    rt.conflicts = rt.replica->conflicts();
    if (rt.replica->shown_version() != before) {
        const std::string merged = rt.replica->splice_into(app.core.export_state(), private_keys(app));
        if (merged.empty()) {
            app.log.push_back({"error", "sync", "could not apply the merged document"});
            return;
        }
        swap_state(app, merged);
        rt.observed_hash = 0;
        sync_prepare(app, 0.0, true);  // records nothing new: it observes its own output
    }
    rt.shown_version = rt.replica->shown_version();
    if (!from.empty()) {
        std::string names;
        for (const auto& u : from) names += (names.empty() ? "" : ", ") + u;
        rt.synced_note = "in sync with " + names + " (" + stamp().substr(9, 2) + ":" + stamp().substr(11, 2) + ")";
        if (rt.replica->shown_version() != before) app.log.push_back({"info", "sync", "took changes from " + names});
    }
    if (rt.conflicts.size() > conflicts_before)
        app.toast(std::to_string(rt.conflicts.size() - conflicts_before) +
                      " new sync conflict(s) - see Share database", true);
}

bool LanRuntime::resolve_conflict(HormigaApp& app, const std::string& hash, std::size_t side) {
    LanRuntime& rt = of(app);
    if (!rt.replica || !sync_prepare(app, 0.0, true)) return false;
    if (!rt.replica->resolve(hash, side)) {
        app.toast("that conflict changed in the meantime - look again", true);
        rt.conflicts = rt.replica->conflicts();
        return false;
    }
    write_atomic(replica_path(rt.replica_for), rt.replica->to_bytes());
    const std::string merged = rt.replica->splice_into(app.core.export_state(), private_keys(app));
    if (!merged.empty()) swap_state(app, merged);
    rt.observed_hash = 0;
    sync_prepare(app, 0.0, true);
    rt.conflicts = rt.replica->conflicts();
    return true;
}
