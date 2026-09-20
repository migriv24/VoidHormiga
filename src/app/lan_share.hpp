/* app/lan_share.hpp — sharing a database over the local network, joining one,
 * and seeing who else is in it.
 *
 * okf/concepts/platform/lan-sharing.md is the design; this is the application
 * side of it. `sync/peer.*` finds addresses, proves keys and moves sealed bytes;
 * `app/lan_wire.*` says what the bytes are; THIS file knows what a database is:
 * which files the Antfarm says to send, which runes are private, where the room
 * key and the members registry live, and how an arrival becomes an opened
 * database through the dispatcher.
 *
 * ONE RUNTIME PER APPLICATION, created on first use. It owns one beacon (offers,
 * presence and discovery share it), at most one host thread and one join
 * thread. Threads never touch the core: anything that needs the database is
 * prepared on the GUI thread before a thread starts, or handed back to be done
 * on the next frame (`run_busy`) -- the same rule `pending_state` keeps for sync.
 *
 * `LanRuntime` is a friend of `HormigaApp` so this can live outside app.cpp and
 * app.hpp; its operations are static and take the app, so the CLI calls the same
 * code the windows do.
 */
#pragma once

#include "app/lan_wire.hpp"
#include "domain/collab.hpp"
#include "platform/profile.hpp"
#include "sync/peer.hpp"   // the sealed LAN session that carries frames
#include "sync/replica.hpp"
#ifdef HORMIGA_HAVE_NET
#include "voidmaiz/net.hpp"  // stage C: the sync seam is Void Maiz's
#endif

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

struct HormigaApp;
namespace hormiga::sync {
class Beacon;
}

namespace hormiga::lan {

/* One line of the share plan (lan-sharing.md §3). */
struct PlanItem {
    std::string kind;  // database | asset | store | key | vault | members | room-key
    std::string rel;   // where it lands, relative to the joining device's folder
    std::string why;   // said to the host before anything is sent
    std::filesystem::path src;
    long long bytes = 0;
    bool send = true;
};

struct Plan {
    std::vector<PlanItem> items;
    int private_withheld = 0;  // runes carrying a private tag
    int hosted_skipped = 0;    // pictures the joiner downloads instead
    long long bytes = 0;
    std::map<std::string, std::string> vault;  // name -> secret, only while unlocked
    std::vector<std::string> skip;             // bundle keys left out (hosted online)
    std::vector<std::string> notes;
};

/* Someone asking to join, as the host sees them. */
struct Request {
    std::string user, color, fingerprint, public_key, avatar_png, app, sas, address;
};

}  // namespace hormiga::lan

struct LanRuntime {
    // ── who is at this computer ─────────────────────────────────────────────
    hormiga::profile::Profile me;
    std::string profile_error;

    // ── the one beacon ──────────────────────────────────────────────────────
    std::unique_ptr<hormiga::sync::Beacon> beacon;
    bool discovering = false;
    double last_announce = -100.0;

    // ── hosting ─────────────────────────────────────────────────────────────
    std::atomic<bool> sharing{false};
    std::thread host_thread;
    hormiga::lan::Plan plan;           // what the host will send; built before sharing
    std::filesystem::path plan_miga;   // the prepared bundle (a temp file)
    std::filesystem::path members_file;
    std::string room_key;              // 32 raw bytes, "" when this database has none
    enum class Answer { Waiting, Allow, Deny };
    struct Pending {
        hormiga::lan::Request req;
        std::atomic<int> answer{0};  // Answer
    };

    // ── joining ─────────────────────────────────────────────────────────────
    std::atomic<bool> joining{false};
    std::thread join_thread;
    char dest[512] = {};

    // ── shared with the threads, under `mu` ─────────────────────────────────
    std::mutex mu;
    std::shared_ptr<Pending> pending;  // a request waiting for Allow / Deny
    std::string host_status, join_status, join_sas, join_error;
    std::filesystem::path joined_miga;  // set by the join thread when files are written
    std::map<std::string, std::string> joined_vault;
    std::vector<std::pair<std::string, std::string>> thread_log;  // level, message

    // ── presence ────────────────────────────────────────────────────────────
    std::map<std::string, hormiga::lan::Activity> present;  // fingerprint -> latest
    std::map<std::string, std::string> shown_color;         // fingerprint -> colour on screen
    std::map<std::string, std::string> member_avatar;       // fingerprint -> cached PNG path
    std::vector<std::map<std::string, std::string>> member_rows;  // the registry, re-read every few seconds
    double members_read_at = -100.0, checked_at = -100.0;
    std::string room_for;  // which database `room_key` was read for
    bool finishing = false;  // finish_join is queued

    // ── keeping members in sync (lan-sharing.md §3b; Palabra's replica) ──────
    struct Progress {
        std::string user;
        long long done = 0, total = 0;
        bool sending = false;
    };
#ifdef HORMIGA_HAVE_NET
    /* STAGE C: the sync seam is Void Maiz's. `net` owns the replica, the
     * sessions, the splice and the files; `link_io` is the only thing the link
     * threads touch, under `mu`. */
    struct LinkIO {
        std::vector<std::string> out, in;
        bool connected = false;   // the frame thread has called Network::connect
        bool carrying = false;    // a link thread is on it
    };
    std::unique_ptr<maiz::Network> net;
    std::string net_for;                       // the database id it belongs to
    std::map<std::string, LinkIO> link_io;     // link id (a peer fingerprint) -> queues
#endif
    std::unique_ptr<hormiga::sync::SharedReplica> replica;
    std::string replica_for;        // the database id it belongs to
    std::string shown_version;      // what it shows
    std::size_t observed_hash = 0;  // of the state last observed, so an idle tick is free
    double prepared_at = -100.0;
    std::atomic<bool> listening{false};
    std::vector<hormiga::sync::ReplicaConflict> conflicts;
    std::map<std::string, double> exchange_started;  // fingerprint -> when (GUI thread)
    std::string synced_note;                         // "synced with X at 14:02"
    bool headless = false;                           // the CLI: no working-copy save
    std::string cli_state;                           // the CLI: what to write back
    // under `mu`:
    std::string outgoing_doc;                        // the latest document to hand a member
    std::set<std::string> member_fps;                // who may connect to sync
    std::vector<std::pair<std::string, std::string>> incoming;  // (user, document)
    std::map<std::string, Progress> progress;        // fingerprint -> transfer

    ~LanRuntime();

    /* ── operations: GUI thread (or the CLI's only thread) ─────────────────── */
    static LanRuntime& of(HormigaApp& app);
    static std::string fingerprint(const LanRuntime& rt);

    /* What sharing would send, read from the Antfarm. `state_out` receives the
     * outgoing document: private runes removed, absolute key paths made plain
     * file names. */
    static hormiga::lan::Plan build_plan(HormigaApp& app, std::string* state_out);
    static bool start_sharing(HormigaApp& app, std::string& error);
    static void stop_sharing(HormigaApp& app);
    static void answer(HormigaApp& app, bool allow);

    static std::vector<hormiga::lan::Offer> offers(HormigaApp& app);
    static bool start_join(HormigaApp& app, const hormiga::lan::Offer& o,
                           const std::filesystem::path& dest, std::string& error);
    /* After the join thread wrote the files: back up what was open, open the new
     * database, keep the vault secrets, fetch the hosted pictures. */
    static void finish_join(HormigaApp& app);

    /* Once a frame: the beacon's payload, presence in and out, thread results
     * into the log. Cheap when nothing is shared. */
    static void tick(HormigaApp& app, double now);

    /* Runes removed from a document before it leaves this device. */
    /* `antfarm_too`: member sync also leaves the Antfarm out -- wiring and keys come
     * from the host (lan-sharing.md §3a), they are not merged. */
    static std::string strip_private(HormigaApp& app, const std::string& state_json,
                                     const hormiga::collab::ShareSettings& s, int* withheld,
                                     bool antfarm_too = false);

    /* Member sync (app/lan_sync.cpp). */
    static std::string database_id(HormigaApp& app, bool create);
    static bool sync_prepare(HormigaApp& app, double now, bool force);
    static void sync_tick(HormigaApp& app, double now, bool presence);
    static void apply_incoming(HormigaApp& app);  // start of a frame
    static bool resolve_conflict(HormigaApp& app, const std::string& hash, std::size_t side);
    static std::set<std::pair<std::string, std::string>> private_keys(HormigaApp& app);
    static void swap_state(HormigaApp& app, const std::string& state);
    static void draw_sync_section(HormigaApp& app);

    /* The members registry (lan-sharing.md §4), its own small database. */
    static bool add_member(HormigaApp& app, const std::filesystem::path& file,
                           const hormiga::lan::Request& who, const std::string& role,
                           const std::string& invited_by, std::string& error);
    static std::vector<std::map<std::string, std::string>> members(
        HormigaApp& app, const std::filesystem::path& file);

    /* Presence, for highlighting: the present members whose selection holds
     * `rune` (by id when the peer sent ids). The VIEWS no longer call this --
     * they read `app.roster`, which lan_presence.cpp fills. Kept for the
     * presence strip and the share window. */
    static std::vector<const hormiga::lan::Activity*> on_rune(const LanRuntime& rt,
                                                              const std::string& rune,
                                                              const std::string& id = {});
    static std::string color_of(const LanRuntime& rt, const std::string& fingerprint);

#ifdef HORMIGA_HAVE_NET
    /* Stage C (app/lan_net.cpp). Everything here runs on the FRAME thread
     * except `net_pump`, which only moves bytes. */
    static bool net_open(HormigaApp& app);
    static void net_close(LanRuntime& rt);
    static void net_tick(HormigaApp& app, double now);
    static void net_links(HormigaApp& app, double now);
    static void net_pump(std::shared_ptr<LanRuntime> rt, hormiga::sync::Session* session,
                         const std::string& link, double until);
    static void net_dial(std::shared_ptr<LanRuntime> rt, hormiga::sync::KeyPair keys,
                         hormiga::lan::Activity peer, int port);
    static void net_listen(std::shared_ptr<LanRuntime> rt, hormiga::sync::KeyPair keys, int port);
#endif

    /* This device's networking preferences, beside the profile (stage B). */
    static std::filesystem::path net_settings_file();
    static void load_net_settings(HormigaApp& app);
    static bool save_net_settings(HormigaApp& app);
    static void refresh_self(HormigaApp& app);   // profile -> net_settings.self

    /* The windows (ui/share.cpp, ui/profile_window.cpp). */
    static void draw_windows(HormigaApp& app);
    static void draw_presence_strip(HormigaApp& app);
    static void draw_profile(HormigaApp& app);
    static void draw_share(HormigaApp& app);
    static void draw_discover(HormigaApp& app);
    static void draw_request(HormigaApp& app);
    /* A person's picture or, without one, their initial on their colour. */
    static void draw_avatar(HormigaApp& app, const std::string& png_path, const std::string& name,
                            const std::string& color, float x, float y, float size);
    /* Highlighting (lan-sharing.md §6): marks on the last-drawn list row, and
     * outlines on a node canvas whose top-left screen corner is (x, y). */

    /* `effect lan-offers`, `effect lan-share`, `effect lan-join` (headless). */
    static int cli(HormigaApp& app, std::string_view op, const std::vector<std::string>& args,
                   const std::string& state_json, std::string& value);
};
