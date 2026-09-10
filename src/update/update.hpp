/* update/update.hpp — the half of updating that is Hormiga's.
 *
 * ── THE SPLIT, AND WHY THERE IS NO VOID HUB ─────────────────────────────────
 *
 * Void Mago is a BUILD-TIME tool. It never runs on a user's machine, and an
 * updater that needed Mago installed would be one more bootstrap the family
 * cannot afford — the same argument that keeps Mago itself off Void Core. So
 * the seam is a document:
 *
 *     Mago                              Hormiga
 *     ----                              -------
 *     `mago feed` writes                one HTTP GET at startup, IF ASKED
 *       void-updates.json               compares `latest` against its own
 *     writes the receipt at install     reads it
 *     supplies `sha256`                 checks it
 *     —                                 ASKS THE PERSON, never skippably
 *     —                                 launches the installer
 *
 * There is no hub, and the reason is worth keeping: a hub is an application you
 * must install before the application you actually wanted. What a hub would
 * give a *user* is either unnecessary (a shared runtime cache — side-by-side
 * installs each carry their own 249 KB `libvoidcore.dll`, and shared runtimes
 * are what DLL hell is made of) or is not an application at all (a single place
 * that knows which versions exist — which is a list, and a list is a file).
 * **The hub is `void-updates.json`.**
 *
 * ── THE TWO RULES THIS FILE EXISTS TO ENFORCE ───────────────────────────────
 *
 * `void.json` commits us: *"no silent updates: the user is told an update
 * exists and chooses it."* Two halves, and the first is the one that is easy to
 * lose:
 *
 * 1. **Never check without being asked to.** A check is a network request a
 *    person did not make. Nothing here touches the network until `Prefs::ask`
 *    says `Startup`, and it starts at `Unasked` — so a fresh install asks
 *    permission for the *check* before it makes one, and "don't ask again" is
 *    a real, persisted answer rather than a snooze.
 *
 * 2. **Never install without being told to.** `download()` fetches and
 *    verifies; it does not run anything. Launching the installer is a separate
 *    call a front-end makes after a person clicked.
 *
 * ── WHY IT IS VIEW-FREE ─────────────────────────────────────────────────────
 *
 * The one machine you cannot attach a debugger to is the one an update broke.
 * Everything here is a pure decision over a document plus one `curl`, so the
 * whole path is exercisable from `voidhormiga-cli update --check` and from
 * `tests/update_smoke.cpp` with no window, no session and no database. The
 * ImGui half is `src/ui/updates.cpp` and contains no decisions.
 *
 * Feed shape: `../VoidMago/okf/concepts/shipping.md`, and the message
 * `MESSAGE_FOR_VOIDHORMIGA_mago-shipping-and-the-update-client-2026-09-04.md`
 * folded into `okf/log.md`.
 */
#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace hormiga::update {

namespace fs = std::filesystem;

/* WHAT A VERSION NUMBER CANNOT EXPRESS AND A RESOLVER CANNOT SEE: nothing
 * broke, no signature moved, and somebody's data is quietly different. This is
 * the category the `release` block exists for, and the difference between a
 * prompt people read and one they dismiss. */
struct BehaviorChange {
    std::string what;
    std::string who_is_affected;
};

struct Release {
    std::string version;
    std::string date;
    std::string change;   // "compatible" | "breaking" | ""
    std::string summary;  // one line — the headline of the prompt
    std::vector<std::string> adds;
    std::vector<BehaviorChange> behavior_changes;

    /* The artifact for THIS platform, resolved while parsing so no caller has
     * to know the feed's shape. `signature` is present-but-null in the feed
     * today: present so we can code against the field, null so nobody mistakes
     * its absence for a decision. Until it exists, `sha256` proves the bytes
     * arrived intact and proves nothing about who made them. */
    bool has_artifact = false;
    std::string file;
    std::string url;
    std::string sha256;
    std::string signature;
    long long bytes = 0;
};

struct Feed {
    bool ok = false;
    std::string error;
    std::string publisher;
    std::string generated;
    std::string display_name;
    std::string latest;               // the only field a comparison needs
    std::vector<Release> releases;    // this application's, as the feed gave them
};

/* Dotted-numeric compare, returns -1 / 0 / 1. Non-numeric trailing parts
 * ("0.2.0-rc1") compare lexically after the numbers, and a missing component
 * is zero, so "0.1" < "0.1.1". Deliberately small: the feed's `latest` is
 * written by Mago from a manifest Mago validated, so this does not need to be
 * a semver implementation — it needs to never claim a newer version is older,
 * which is the only way to fail dangerously. */
int compare_versions(const std::string& a, const std::string& b);

/* Read a `void-updates/0.1` document, keeping only `app`'s entry and only
 * `platform`'s artifacts. Anything else in the file is another application's
 * business; §"Read your own entry and ignore the rest". */
Feed parse_feed(const std::string& json, const std::string& app,
                const std::string& platform);

// ── PREFERENCES: PER MACHINE, NOT PER DATABASE ──────────────────────────────
//
// Deliberately NOT `config set update.*`. Core's config tier rides the saved
// org, so "don't ask me about updates" would be an answer attached to whichever
// database happened to be open — and it would travel to another device on the
// next merge, answering a question that device was never asked. This is a fact
// about an installation.
//
// It lives in `%LOCALAPPDATA%/VoidHormiga/`, which is the SUITE folder the NSIS
// installer writes to and the parent of each side-by-side version folder. So
// the answer survives the update it was given for, which is the whole point.

enum class Ask {
    Unasked,  // we have never asked; ask before the first network request
    Never,    // "don't ask again", and it is real
    Startup,  // check when the application starts
};

struct Prefs {
    Ask ask = Ask::Unasked;
    std::string feed_url;       // empty = the compiled-in default
    std::string skip_version;   // "not this one" — cleared when a newer appears
    std::string last_checked;   // ISO-8601, informational only
};

fs::path prefs_path();
Prefs load_prefs();
bool save_prefs(const Prefs&);
const char* ask_name(Ask);        // for the CLI and the file
Ask ask_from_name(const std::string&);

// ── THE DECISION ────────────────────────────────────────────────────────────

struct Offer {
    bool available = false;   // strictly newer than `current`, and not skipped
    bool skipped = false;     // newer, but the person said not this one
    Release release;
    std::string current;
};
Offer decide(const Feed& feed, const std::string& current,
             const std::string& skip_version);

// ── THE NETWORK ─────────────────────────────────────────────────────────────
//
// `curl` ships with Windows 10 1803+ and every platform we target, which is why
// there is no HTTP client vendored here. It is NOT `publish/http.hpp`'s call:
// that one deliberately does not follow redirects (it carries bearer tokens to
// APIs) and deliberately has no time limit (an asset upload takes minutes), and
// this one needs both the opposite ways. The reasons are written at the call.
//
// The shell seam is passed in rather than taken, so every decision in this file
// is exercisable with no network at all.

using Shell = std::function<std::string(const std::string&)>;

struct Fetch {
    bool ok = false;
    int status = 0;
    std::string body;
    std::string error;
};
Fetch fetch_feed(const Shell&, const std::string& url, const fs::path& tmp);

/* fetch + parse + decide, in one call, because every caller wants exactly
 * those three and getting the order wrong is how a check happens that nobody
 * asked for. It stamps `prefs.last_checked` and does NOT save — writing the
 * file is the caller's, so a check made from a `--check` on the command line
 * does not quietly rewrite a preference the person set in the window. */
struct CheckResult {
    bool ok = false;
    std::string error;
    Feed feed;
    Offer offer;
};
CheckResult check(const Shell&, Prefs& prefs, const fs::path& tmp);

/* ISO-8601 UTC to the second. Exposed because the front-ends stamp the same
 * clock into the same file. */
std::string now_iso8601();

struct Download {
    bool ok = false;
    std::string error;
    fs::path file;
};
/* Fetch the artifact into `dir` and CHECK ITS DIGEST BEFORE RETURNING OK. A
 * failed check deletes the file: a mismatched installer left on disk beside a
 * message nobody read is how a bad download gets run anyway. */
Download download(const Shell&, const Release&, const fs::path& dir);

/* Hand the verified installer to the OS and return. It RUNS; it does not
 * replace anything, and nothing here ever deletes the version calling it —
 * side-by-side installs are what make "try the update" a reversible decision,
 * and the running copy is what a person falls back to when the new one is
 * wrong. Repointing the Start Menu shortcut and removing an old version are
 * the installer's business, not ours.
 *
 * False means the OS refused to launch it; the file is still on disk, verified,
 * and every caller says where. */
bool launch_installer(const fs::path&);

/* Streaming SHA-256 (libsodium — ground rule 6, never hand-rolled). Lowercase
 * hex, "" if the file cannot be read. */
std::string sha256_file(const fs::path&);

// ── WHO WE ARE ──────────────────────────────────────────────────────────────

const char* app_name();        // "voidhormiga" — our key in the feed
const char* current_version(); // HORMIGA_VERSION, from CMake's PROJECT_VERSION
const char* platform_tag();    // "windows-x64" — our key in `artifacts`
const char* default_feed_url();

/* A human-readable rendering of what an offer contains: the summary, what it
 * adds, and every behavior change with who it affects. One function so the GUI
 * modal and the CLI say the same words, and so "the prompt is worth reading"
 * is a property of one place. Empty when there is nothing to offer. */
std::string describe(const Offer&);

} // namespace hormiga::update
