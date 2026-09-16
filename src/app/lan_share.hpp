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

#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
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
    static std::string strip_private(HormigaApp& app, const std::string& state_json,
                                     const hormiga::collab::ShareSettings& s, int* withheld);

    /* The members registry (lan-sharing.md §4), its own small database. */
    static bool add_member(HormigaApp& app, const std::filesystem::path& file,
                           const hormiga::lan::Request& who, const std::string& role,
                           const std::string& invited_by, std::string& error);
    static std::vector<std::map<std::string, std::string>> members(
        HormigaApp& app, const std::filesystem::path& file);

    /* Presence, for highlighting: the present members whose selection holds `rune`. */
    static std::vector<const hormiga::lan::Activity*> on_rune(const LanRuntime& rt,
                                                              const std::string& rune);
    static std::string color_of(const LanRuntime& rt, const std::string& fingerprint);

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
    static void mark_item(HormigaApp& app, const std::string& rune);
    static void outline_nodes(HormigaApp& app, float canvas_x, float canvas_y);

    /* `effect lan-offers`, `effect lan-share`, `effect lan-join` (headless). */
    static int cli(HormigaApp& app, std::string_view op, const std::vector<std::string>& args,
                   const std::string& state_json, std::string& value);
};
