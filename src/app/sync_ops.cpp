/* sync_ops.cpp — the sync verbs, as the application sees them.
 *
 * okf/concepts/platform/collaboration.md. `src/sync/` finds an address, proves
 * an identity and moves bytes; it knows nothing about runes. THIS file is where
 * those bytes become the organization's database, and it is the only place the
 * two vocabularies meet.
 *
 * ── THE TWO RULES THIS FILE EXISTS TO HOLD ───────────────────────────────────
 *
 * 1. REPORT BY DEFAULT, WRITE ONLY ON `apply`. A merge is the one operation in
 *    this application that can lose somebody's work. The shape that protects a
 *    volunteer is the one where seeing what would happen costs nothing and is
 *    what you get by typing the obvious thing. `--apply` is the second command,
 *    typed by someone who has read the first one's output.
 *
 * 2. THE MERGED STATE GOES THROUGH THE DISPATCHER. Ground rule 3 has no
 *    exception for the network. `reload_from_state` is the same seam
 *    `open_database` uses, so a sync is as logged, replayable and undoable as
 *    an edit typed into the command bar. A sync that wrote into the store
 *    directly would be a second door, and every property this project rests on
 *    is a property of going through the first one.
 *
 * ── AND ONE THING THAT IS DELIBERATELY NOT HERE ──────────────────────────────
 *
 * Conflict RESOLUTION. Conflicts are reported, with their addresses and both
 * sides, and a human settles them with ordinary `set` commands afterwards.
 * Choosing for them would be exactly the "resolve by seniority" the whole
 * Allomone lattice was arranged to refuse.
 */
#include "app/app_internal.hpp"
#include "sync/merge.hpp"
#include "sync/peer.hpp"
#include "app/lan_share.hpp" // strip_private: the private-tag seam

#include "json.hpp" // telling a .miga bundle from a bare state document

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace {

/* The helpers below take the LOG and the CORE by reference rather than the
 * whole app, because `HormigaApp`'s members are private and a free function
 * cannot reach them -- and taking only what is needed is the better shape
 * anyway. The members that call them are inside the struct and can hand
 * them over. `on_take_state` is a public host seam, so that one is passed as
 * the app itself. */
using Log = std::vector<maiz::LogEntry>;

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

/* A `.miga` bundle carries the state under `"state"`; a bare state document is
 * already what we want. Accepting both is not laxity — the two-folder case
 * (data-planes §7 phase F) passes a `.miga` between folders, and a peer on the
 * wire sends the document. One entry point, two shapes, and telling them apart
 * is one key lookup. */
std::string state_from(const std::string& text) {
    try {
        auto j = nlohmann::json::parse(text);
        if (j.is_object() && j.contains("state")) return j["state"].dump();
        if (j.is_object() && j.contains("mantles")) return text;
    } catch (...) {}
    return {};
}

/* Hand a merged document to whichever front-end we are running in.
 *
 * Refusing rather than silently doing nothing is the point: an application that
 * merged, said "applied", and kept its old data would be the worst outcome
 * available at this seam. */
/* Hand a merged document BACK to the caller rather than applying it here.
 *
 * See app.hpp on `pending_state` for why: applying it here would replace the
 * core from inside the effect handler that core owns, which is a use-after-free
 * that presents as "the merge said it worked and nothing happened". */
int take(std::string& out, Log& log, const std::string& merged,
         const std::string& version) {
    out = merged;
    log.push_back({"info", "sync", "applied - the database is now " + version});
    return 0;
}


/* ── the report ──────────────────────────────────────────────────────────────
 *
 * Written once and shared by every path, because the whole safety story is that
 * a person reads this before typing `apply`, and a report that differed between
 * the file path and the network path is a report nobody would learn to trust. */
void report(Log& log, const hormiga::sync::MergeResult& r, const char* source, bool apply) {
    auto say = [&](const char* level, const std::string& m) {
        log.push_back({level, "sync", m});
    };
    say("info", std::string("local  ") + r.version_local);
    say("info", std::string("peer   ") + r.version_remote);

    if (r.identical) {
        say("info", std::string("already in sync with ") + source + " - nothing to do");
        return;
    }
    say("info", std::string("merged ") + r.version_merged + "  (" +
                    std::to_string(r.mantles) + " mantles, " + std::to_string(r.runes) +
                    " runes)");

    /* ── THE LIMITATION, SAID OUT LOUD, ON BOTH SIDES ────────────────────────
     *
     * A fresh enrich has no memory of what this peer once observed, so a rune
     * that exists on exactly one side is AMBIGUOUS and the ambiguity points a
     * different way depending on which side it is:
     *
     *   only on the PEER  ->  they added it, OR **we deleted it and it is
     *                         coming back**. This is the resurrection.
     *   only HERE         ->  we added it, OR they deleted it and our copy is
     *                         about to re-teach them.
     *
     * The first version of this warned only about the second case, which is the
     * less dangerous one, and a real two-folder run made that obvious in about a
     * second: folder A deleted a contact, the merge brought it back, and the
     * report said "2 runes arrive from the file" in a cheerful info line. Both
     * lists are named now, and each says which mistake it could be. */
    int from_peer = 0, ours_only = 0;
    for (const auto& o : r.one_sided) (o.on_local ? ours_only : from_peer)++;

    if (from_peer) {
        say("warn", std::to_string(from_peer) + " rune(s) arrive from " + source +
                        ". If you DELETED any of these, this merge BRINGS THEM BACK - "
                        "deletions do not yet propagate (collaboration.md 2). "
                        "Re-delete after applying.");
        for (const auto& o : r.one_sided)
            if (!o.on_local)
                say("warn", "  arriving: " + o.mantle + " / " +
                                (o.name.empty() ? o.rune : o.name));
    }
    if (ours_only) {
        say("info", std::to_string(ours_only) +
                        " rune(s) exist only here; they will reach the peer. (If the "
                        "peer deleted them, their deletion will not survive this merge.)");
        for (const auto& o : r.one_sided)
            if (o.on_local)
                say("info", "  only here: " + o.mantle + " / " +
                                (o.name.empty() ? o.rune : o.name));
    }

    if (r.conflicts.empty()) {
        say("info", "no conflicts");
    } else {
        say("warn", std::to_string(r.conflicts.size()) +
                        " conflict(s) - both values survive; settle each with a `set`");
        for (const auto& c : r.conflicts) {
            std::string line = "  " + c.mantle + " / " + (c.rune.empty() ? "(mantle)" : c.rune) +
                               " . " + c.field + "  [" + c.hash.substr(0, 8) + "]";
            for (const auto& v : c.sides) line += "\n      = " + v;
            say("warn", line);
        }
    }
    if (!apply)
        say("info", "REPORT ONLY - nothing was written. Add `apply` to take this merge.");
}

/* What this device calls itself on the network. Stable across runs (it is the
 * database's own file name) and deliberately NOT the organization's name: a
 * datagram on the local network is readable by everyone on it, and advertising
 * which organization's database this is would be a disclosure nobody asked for. */
hormiga::sync::PeerInfo self_info(const std::string& state_name, const std::string& version,
                                  const std::string& fingerprint, std::uint16_t port) {
    hormiga::sync::PeerInfo self;
    self.peer_id = "hormiga:" + state_name;
    self.display = state_name;
    self.version = version;
    self.fingerprint = fingerprint;
    self.port = port;
    return self;
}

/* A keypair per database, kept beside it and never in the state document. The
 * secret half never leaves this file and never crosses the wire. */
hormiga::sync::KeyPair device_keys(const fs::path& base_dir, const std::string& state_name,
                                   Log& log) {
    const fs::path kf = base_dir / (state_name + ".peerkey");
    std::error_code ec;
    if (fs::exists(kf, ec)) {
        const std::string raw = slurp(kf);
        if (raw.size() == 64) return {raw.substr(0, 32), raw.substr(32)};
        log.push_back({"warn", "sync", "peer key file was unreadable; making a new one"});
    }
    hormiga::sync::KeyPair kp = hormiga::sync::generate_keypair();
    std::ofstream out(kf, std::ios::binary | std::ios::trunc);
    out << kp.public_key << kp.secret_key;
    return kp;
}

}  // namespace

/* ── the five verbs, behind one entry point ──────────────────────────────── */

HormigaApp::SyncReport HormigaApp::sync_op(std::string_view op,
                                           const std::vector<std::string>& args,
                                           const std::string& state_json, bool apply) {
    SyncReport out;
    /* A LOCAL LOG, not the app's. The report is the product of this call, and a
     * caller that has to fish its lines out of a shared, growing buffer will
     * eventually print somebody else's. */
    Log log;

    /* Boot a core from the document we were handed. Every verb below reads the
     * state from HERE rather than from `this`, which is the whole reason the
     * signature takes it -- see app.hpp. */
    /* ITS OWN CORE, NEVER THE APP'S (2026-09-16). This read `core = maiz::Core(
     * state_json)`, which replaced the RUNNING core -- and `install_host()` is
     * what puts the effect handler and the log sink on a core, so afterwards
     * every `effect render-site`, `deploy-site` or `host-online` answered "no
     * host effect handler for 'effect'" until the app was restarted, with the
     * core's own warning going nowhere because the sink went with it. The
     * comment above always said this boots its own core; now it does. */
    maiz::Core probe(state_json);
    if (on_register_glyphs) on_register_glyphs(probe);

    auto arg = [&](std::size_t n) -> std::string {
        std::size_t seen = 0;
        for (const auto& a : args) {
            if (a == "apply" || a == "--apply") continue;
            if (seen++ == n) return a;
        }
        return {};
    };
    auto number = [&](std::size_t n, int fallback) {
        const std::string v = arg(n);
        if (v.empty()) return fallback;
        const int i = std::atoi(v.c_str());
        return i > 0 ? i : fallback;
    };

    std::string err;
    if (op == "sync-version") {
        out.value = hormiga::sync::version_name(state_json, &err);
        if (out.value.empty()) {
            log.push_back({"error", "sync", "no version name: " + err});
            out.rc = 1;
        } else {
            log.push_back({"info", "sync", out.value});
        }
        out.lines = std::move(log);
        return out;
    }

    if (op == "sync-merge") {
        const std::string path = arg(0);
        if (path.empty()) {
            log.push_back({"error", "sync",
                           "usage: effect sync-merge <file.miga|state.json> [apply]"});
            out.rc = 1;
            out.lines = std::move(log);
            return out;
        }
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            log.push_back({"error", "sync", "no such file: " + path});
            out.rc = 1;
            out.lines = std::move(log);
            return out;
        }
        const std::string peer_state = state_from(slurp(path));
        if (peer_state.empty()) {
            log.push_back({"error", "sync",
                           "not a state document or a .miga bundle: " + path});
            out.rc = 1;
            out.lines = std::move(log);
            return out;
        }
        /* MINT PREFIXES MUST DIFFER AND MUST BE STABLE. The local one is this
         * database's own file name so it survives restarts; the remote one is
         * the path it came from. Two peers minting the same tag is a corruption
         * `merge_states` refuses outright rather than working around. */
        const auto r = hormiga::sync::merge_states(state_json, peer_state,
                                                   "local:" + state_name, "file:" + path);
        if (!r.ok) {
            log.push_back({"error", "sync", "merge refused: " + r.error});
            out.rc = 1;
            out.lines = std::move(log);
            return out;
        }
        report(log, r, "the file", apply);
        out.value = r.version_merged;
        if (apply && !r.identical)
            out.rc = take(out.merged_state, log, r.merged_state, r.version_merged);
        out.lines = std::move(log);
        return out;
    }

    const auto keys = device_keys(base_dir, state_name, log);
    const std::string fp = hormiga::sync::fingerprint_of(keys.public_key);
    const std::string mine = hormiga::sync::version_name(state_json);

    if (op == "lan-peers") {
        const int seconds = number(0, 4);
        hormiga::sync::Beacon beacon;
        if (!beacon.start(self_info(state_name, mine, fp, hormiga::sync::kStreamPortDefault),
                          &err)) {
            log.push_back({"error", "sync", "beacon: " + err});
            out.rc = 1;
            out.lines = std::move(log);
            return out;
        }
        log.push_back({"info", "sync",
                       "listening " + std::to_string(seconds) + "s (this device is " +
                           fp.substr(0, 8) + ", " + mine + ")"});
        for (int i = 0; i < seconds * 10; i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto found = beacon.peers();
        beacon.stop();
        if (found.empty())
            log.push_back({"info", "sync",
                           "no peers - is the other device on this network with Hormiga "
                           "open and announcing?"});
        for (const auto& p : found)
            log.push_back({"info", "sync",
                           p.display + "  " + p.address + ":" + std::to_string(p.port) +
                               "  " + (p.version == mine ? "in sync"
                                                         : "DIFFERS (" + p.version + ")")});
        out.lines = std::move(log);
        return out;
    }

    /* `lan-serve` AND `lan-sync` ARE RETIRED (2026-09-17). They enriched both
     * current states on every exchange, which is precisely what resurrects
     * deletions -- and Void Palabra's point was that ONE peer still doing it
     * brings deletions back for everyone it syncs with. Members of a shared
     * database now keep in sync automatically through a replica that survives
     * between exchanges (app/lan_sync.cpp, lan-sharing.md §3b). A file from
     * elsewhere is still merged once with `sync-merge`. */
    if (op == "lan-serve" || op == "lan-sync") {
        log.push_back({"error", "sync",
                       "retired: members of a shared database sync automatically now "
                       "(Share database > Share over local network; `effect lan-stay` in the "
                       "CLI). To fold in a copy from a file, use `effect sync-merge`."});
        out.rc = 1;
    } else {
        log.push_back({"error", "sync", "unknown sync verb: " + std::string(op)});
        out.rc = 1;
    }
    out.lines = std::move(log);
    return out;
}

/* The GUI's adapter over `sync_op`.
 *
 * Here rather than in `app.cpp` for the reason `find_long.py` exists to notice:
 * app.cpp is the SHELL, and a feature's argument parsing is not shell work. It
 * also keeps every line that knows what a sync verb is in one file.
 *
 * Returns false when the verb refused; `result` is the effect's JSON string. */
bool HormigaApp::gui_sync_effect(std::string_view op, std::string_view args,
                                 std::string& result) {
    std::vector<std::string> a;
    try {
        auto aj = nlohmann::json::parse(args);
        if (aj.contains("args"))
            for (const auto& v : aj["args"]) a.push_back(v.get<std::string>());
    } catch (...) {}

    /* `apply` is a WORD, not a flag, and it may appear anywhere. A merge that
     * ran because a dash was mistyped is not a failure mode worth having. */
    bool apply = false;
    for (const auto& t : a)
        if (t == "apply" || t == "--apply") apply = true;

    const SyncReport rep = sync_op(op, a, core.export_state(), apply);
    for (const auto& e : rep.lines) log.push_back(e);

    /* NOT `reload_from_state(...)` here. We are inside the effect handler,
     * which `core` owns, so replacing `core` now frees the lambda we are
     * executing. `frame()` drains this. (app.hpp, `pending_state`.) */
    if (!rep.merged_state.empty()) pending_state = rep.merged_state;

    if (rep.rc != 0) {
        toast("sync failed - see log", true);
        return false;
    }
    if (!apply) toast("sync: report only - read the log, then run it again with `apply`");
    result = json_str(rep.value.empty() ? "ok" : rep.value);
    return true;
}
