/* update_smoke.cpp — the update client, with no network and no application.
 *
 * WHY THIS SUITE EXISTS IN THIS SHAPE. Every failure the update client can have
 * is silent. A version compare that reports 0.1.10 as older than 0.1.9 does not
 * throw; it just means nobody is ever told an update exists. A feed parser that
 * returns an empty `Feed` on a login page prints "up to date". A digest check
 * that leaves a mismatched installer on disk is how a bad download gets run a
 * week later by somebody who found it in Downloads.
 *
 * So the cases here are mostly the WRONG inputs — the shapes the network hands
 * you when something is off — and the two rules `void.json` commits us to:
 * never check without being asked, never install without being told.
 *
 * It links libsodium and nothing else. No Void Core, no view, no session, no
 * database, no socket. That is the same claim `src/update/` makes about itself
 * in `tools/check_layering.py`, made executable.
 */
#include "update/update.hpp"

#include <fstream>
#include <iostream>

namespace up = hormiga::update;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(x)                                                           \
    do {                                                                   \
        if (!(x)) {                                                        \
            std::cerr << "FAIL " << __LINE__ << ": " #x "\n";              \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

/* A feed in the shape `mago feed` writes, from the message of 2026-09-04. */
static const char* kFeed = R"({
  "feed": "void-updates/0.1",
  "generated": "2026-09-04T18:28:16+00:00",
  "publisher": "Void",
  "applications": {
    "voidmago": { "display_name": "Void Mago", "latest": "9.9.9", "releases": [] },
    "voidhormiga": {
      "display_name": "Void Hormiga",
      "latest": "0.1.10",
      "releases": [
        {
          "version": "0.1.10",
          "date": "2026-09-10",
          "change": "compatible",
          "summary": "the update client, and two blockers Mago found",
          "adds": ["voidhormiga-cli update"],
          "behavior_changes": [
            { "what": "the .miga bundle gained a field older readers ignore",
              "who_is_affected": "anyone opening a 0.1.10 bundle in 0.1.0" }
          ],
          "artifacts": {
            "windows-x64": {
              "file": "VoidHormiga-0.1.10-windows-x64-setup.exe",
              "url": "https://example.invalid/VoidHormiga-0.1.10-windows-x64-setup.exe",
              "bytes": 24117248,
              "sha256": "0000000000000000000000000000000000000000000000000000000000000000",
              "signature": null
            }
          }
        },
        {
          "version": "0.1.9",
          "date": "2026-09-05",
          "summary": "an older one, which must not be picked",
          "artifacts": {}
        }
      ]
    }
  }
})";

int main() {
    // ── 1: version ordering, and the one way it fails dangerously ───────────
    //
    // "0.1.10" vs "0.1.9" is the whole reason this is not a string compare:
    // lexically "0.1.10" < "0.1.9", so a naive implementation reports the newer
    // release as older, says "up to date", and nobody is ever told. It does not
    // error and it never will.
    CHECK(up::compare_versions("0.1.10", "0.1.9") > 0);
    CHECK(up::compare_versions("0.1.9", "0.1.10") < 0);
    CHECK(up::compare_versions("0.1.0", "0.1.0") == 0);
    CHECK(up::compare_versions("0.2.0", "0.1.99") > 0);
    CHECK(up::compare_versions("1.0.0", "0.99.99") > 0);
    CHECK(up::compare_versions("0.1", "0.1.1") < 0);    // missing component is zero
    CHECK(up::compare_versions("0.1.0", "0.1") == 0);   // and it is genuinely zero

    // ── 2: the feed, read as our own entry only ─────────────────────────────
    const up::Feed f = up::parse_feed(kFeed, "voidhormiga", "windows-x64");
    CHECK(f.ok);
    CHECK(f.latest == "0.1.10");
    CHECK(f.display_name == "Void Hormiga");
    CHECK(f.publisher == "Void");
    CHECK(f.releases.size() == 2);
    // Void Mago's 9.9.9 sits in the same document and is none of our business.
    CHECK(f.latest != "9.9.9");
    if (!f.releases.empty()) {
        const up::Release& r = f.releases[0];
        CHECK(r.version == "0.1.10");
        CHECK(r.has_artifact);
        CHECK(r.bytes == 24117248);
        CHECK(r.signature.empty());   // "signature": null -- present, undecided
        CHECK(r.adds.size() == 1);
        CHECK(r.behavior_changes.size() == 1);
        CHECK(r.behavior_changes[0].who_is_affected.find("0.1.0") != std::string::npos);
    }
    // a release with no artifact for this platform is still a release
    if (f.releases.size() > 1) CHECK(!f.releases[1].has_artifact);
    // and a platform we do not build for finds nothing rather than something
    const up::Feed lin = up::parse_feed(kFeed, "voidhormiga", "linux-x64");
    CHECK(lin.ok && !lin.releases.empty() && !lin.releases[0].has_artifact);

    // ── 3: the shapes a network hands you when something is wrong ───────────
    //
    // Each of these used to be "up to date" in every updater anybody has ever
    // written by hand. They are errors with a sentence instead.
    CHECK(!up::parse_feed("<!DOCTYPE html><title>Sign in</title>", "voidhormiga",
                          "windows-x64").ok);                     // a login page
    CHECK(!up::parse_feed("{}", "voidhormiga", "windows-x64").ok); // valid JSON, not a feed
    CHECK(!up::parse_feed(R"({"feed":"something-else/1"})", "voidhormiga",
                          "windows-x64").ok);
    CHECK(!up::parse_feed(R"({"feed":"void-updates/0.1","applications":{}})",
                          "voidhormiga", "windows-x64").ok);      // not listed
    CHECK(!up::parse_feed(
               R"({"feed":"void-updates/0.1","applications":{"voidhormiga":{}}})",
               "voidhormiga", "windows-x64").ok);                 // no `latest`
    CHECK(!up::parse_feed("", "voidhormiga", "windows-x64").ok);

    // ── 4: the decision ─────────────────────────────────────────────────────
    CHECK(up::decide(f, "0.1.0", "").available);
    CHECK(!up::decide(f, "0.1.10", "").available);  // same version: nothing to say
    CHECK(!up::decide(f, "0.2.0", "").available);   // ahead of the feed (a dev build)
    {
        const up::Offer o = up::decide(f, "0.1.0", "");
        CHECK(o.release.version == "0.1.10");   // the LATEST, not the first listed
        CHECK(o.current == "0.1.0");
    }
    // "skip this version" is "not this one", not "never again": it expires the
    // moment something newer appears, so it cannot become an off-switch nobody
    // remembers flipping.
    {
        const up::Offer skipped = up::decide(f, "0.1.0", "0.1.10");
        CHECK(!skipped.available);
        CHECK(skipped.skipped);
        const up::Offer expired = up::decide(f, "0.1.0", "0.1.9");
        CHECK(expired.available);   // 0.1.10 is newer than the skip
    }
    // a feed that failed to parse offers nothing, rather than offering ""
    CHECK(!up::decide(up::parse_feed("nonsense", "voidhormiga", "windows-x64"),
                      "0.1.0", "").available);

    // ── 5: what the person actually reads ───────────────────────────────────
    //
    // The prompt has to carry the `release` block or it can only say "a new
    // version is available", which is the difference between a notification
    // people read and one they dismiss.
    {
        const std::string text = up::describe(up::decide(f, "0.1.0", ""));
        CHECK(text.find("0.1.10") != std::string::npos);
        CHECK(text.find("the update client") != std::string::npos);   // the summary
        CHECK(text.find("voidhormiga-cli update") != std::string::npos); // an add
        CHECK(text.find("older readers ignore") != std::string::npos);  // the change
        CHECK(text.find("anyone opening") != std::string::npos);        // who it hits
        CHECK(text.find("not signed") != std::string::npos);   // said out loud
        CHECK(up::describe(up::decide(f, "0.1.10", "")).empty());
    }

    // ── 6: the digest, against a value nothing computed twice ───────────────
    {
        const fs::path t = fs::temp_directory_path() / "hormiga-update-smoke.bin";
        { std::ofstream o(t, std::ios::binary); o << "abc"; }
        // SHA-256("abc"), the published vector. Not a value produced by this
        // code and then asserted against itself, which is the only kind of hash
        // assertion worth writing.
        CHECK(up::sha256_file(t) ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        fs::remove(t);
        CHECK(up::sha256_file(fs::temp_directory_path() / "no-such-file-here").empty());
    }

    // ── 7: the two refusals that are about a command line ───────────────────
    //
    // A URL out of a document fetched over the network becomes an argument to
    // `curl`. Refusing is right rather than escaping: no legitimate release has
    // a quote or a backslash in its URL, so the check costs nothing real, and
    // the alternative is trusting a quoting function with a command line that
    // installs software. Same reason Void Core's `link --weight` refuses a
    // non-number instead of coercing it to 0.0.
    {
        const up::Shell never = [](const std::string&) {
            CHECK(false);   // reaching the shell at all is the failure
            return std::string();
        };
        const fs::path tmp = fs::temp_directory_path();
        CHECK(!up::fetch_feed(never, "http://example.invalid/f.json", tmp).ok);
        CHECK(!up::fetch_feed(never, R"(https://x/"; rm -rf /; ")", tmp).ok);
        CHECK(!up::fetch_feed(never, "file:///etc/passwd", tmp).ok);
        CHECK(!up::fetch_feed(never, "", tmp).ok);

        up::Release bad;
        bad.has_artifact = true;
        bad.url = "https://example.invalid/x.exe";
        bad.file = "../../../../Windows/System32/evil.exe";
        bad.sha256 = std::string(64, '0');
        CHECK(!up::download(never, bad, tmp).ok);          // the filename
        bad.file = "ok.exe";
        bad.sha256 = "not-a-digest";
        CHECK(!up::download(never, bad, tmp).ok);          // unverifiable
        bad.sha256.clear();
        CHECK(!up::download(never, bad, tmp).ok);          // no digest at all
    }

    // ── 8: the preference is a string a person can read and edit ────────────
    CHECK(std::string(up::ask_name(up::Ask::Never)) == "never");
    CHECK(std::string(up::ask_name(up::Ask::Startup)) == "startup");
    CHECK(std::string(up::ask_name(up::Ask::Unasked)) == "unasked");
    CHECK(up::ask_from_name("never") == up::Ask::Never);
    CHECK(up::ask_from_name("startup") == up::Ask::Startup);
    // ANYTHING UNRECOGNISED IS `Unasked`, NOT `Startup`. A corrupted or
    // hand-edited preferences file must fail towards asking, never towards
    // making a request nobody agreed to.
    CHECK(up::ask_from_name("") == up::Ask::Unasked);
    CHECK(up::ask_from_name("sure why not") == up::Ask::Unasked);
    CHECK(!up::prefs_path().empty());
    // and it is outside any org folder: a fact about an installation
    CHECK(up::prefs_path().filename() == "updates.json");

    // ── 9: we know who we are ───────────────────────────────────────────────
    CHECK(std::string(up::app_name()) == "voidhormiga");
    CHECK(std::string(up::current_version()) != "0.0.0-unversioned");
    CHECK(std::string(up::default_feed_url()).rfind("https://", 0) == 0);

    // ── 10: THE FEED MAGO ACTUALLY WRITES (2026-09-08) ──────────────────────
    //
    // `kFeed` above was typed by hand from a message. That is the shape of
    // fixture that drifts: it asserts what we believed the other side emits,
    // and it keeps passing after the other side changes. This one is the
    // verbatim output of `mago feed voidhormiga --base-url ... --artifacts ...`
    // run against this repository's own `void.json` on 2026-09-08 (mago 0.1.6),
    // with a stand-in binary standing in for the installer -- so the bytes and
    // the digest are that file's, and everything structural is real.
    //
    // It exists because Mago added `stable_file` that day and the interesting
    // question was not whether we read it. It is whether an ADDED key can
    // disturb us at all, and whether the name we follow is still the right one
    // of the two: `file` is versioned and is what an update downloads;
    // `stable_file` is version-free and is what a WEBSITE links. Confusing them
    // fails in the direction nobody checks -- updates would fetch the URL of an
    // asset the feed does not describe.
    {
        static const char* kMagoFeed = R"({
  "feed": "void-updates/0.1",
  "generated": "2026-09-08T19:16:11+00:00",
  "publisher": "Void",
  "note": "What exists, for a client that already has one of these installed.",
  "applications": {
    "voidhormiga": {
      "display_name": "Void Hormiga",
      "description": "The outreach organization's application.",
      "kind": "application",
      "latest": "0.1.0",
      "releases": [
        {
          "version": "0.1.0",
          "date": "2026-09-04",
          "change": "compatible",
          "summary": "the first release",
          "adds": ["voidhormiga-cli update"],
          "behavior_changes": [],
          "artifacts": {
            "windows-x64": {
              "file": "VoidHormiga-0.1.0-windows-x64-setup.exe",
              "stable_file": "VoidHormiga-windows-x64-setup.exe",
              "url": "https://github.com/migriv24/VoidHormiga/releases/latest/download/VoidHormiga-0.1.0-windows-x64-setup.exe",
              "bytes": 200000,
              "sha256": "d70268bdbb2f7edb2ec32bdbff8329ae621cd6e18679b84ca698b533aad9e32e",
              "signature": null
            }
          }
        }
      ]
    }
  }
})";
        const up::Feed m = up::parse_feed(kMagoFeed, "voidhormiga", "windows-x64");
        CHECK(m.ok);
        CHECK(m.latest == "0.1.0");
        CHECK(m.display_name == "Void Hormiga");
        CHECK(m.releases.size() == 1);
        if (!m.releases.empty()) {
            const up::Release& r = m.releases[0];
            CHECK(r.has_artifact);
            // THE ONE THAT MATTERS: the versioned name, never the stable one.
            CHECK(r.file == "VoidHormiga-0.1.0-windows-x64-setup.exe");
            CHECK(r.url.find("VoidHormiga-0.1.0-windows-x64-setup.exe") !=
                  std::string::npos);
            CHECK(r.url.find("/releases/latest/download/") != std::string::npos);
            // and the digest survived, because a release without one is refused
            CHECK(r.sha256.size() == 64);
            CHECK(r.signature.empty());   // still honestly null
        }
        // A key we do not read must not become a key we mis-read: the feed is
        // additive and this is the assertion that keeps it that way.
        CHECK(!up::decide(m, "0.1.0", "").available);   // we ARE 0.1.0
        CHECK(up::decide(m, "0.0.9", "").available);    // an older install is offered

        /* The website's button points at the STABLE name, and nothing in this
         * binary composes that string -- it is typed into a `link` rune on a
         * download page. So the one way it can be wrong is by disagreeing with
         * Mago's `stable_installer_name()`, and the failure is silent in the
         * worst direction: updates keep working, because they follow `file`,
         * while every new visitor's download 404s. Pinned here against the name
         * Mago emitted, because this is the only file in this repository that
         * sees both halves. See okf/concepts/platform/download-page.md §4. */
        CHECK(std::string(kMagoFeed).find(
                  "\"stable_file\": \"VoidHormiga-windows-x64-setup.exe\"") !=
              std::string::npos);
    }

    // ── 11: off Windows the download is an archive, unpacked BESIDE us ───────
    //
    // 0.1.3's Linux client was handed a `.tar.gz` and `xdg-open`ed it, which
    // opens an archive viewer. The archive is now unpacked next to the running
    // install, never over it.
    CHECK(up::is_archive("VoidHormiga-0.1.4-linux-x64.tar.gz"));
    CHECK(!up::is_archive("VoidHormiga-0.1.4-windows-x64-setup.exe"));
    CHECK(!up::is_archive(".tar.gz"));
#ifndef _WIN32
    {
        const fs::path root = fs::temp_directory_path() / "hormiga_update_unpack";
        std::error_code ec;
        fs::remove_all(root, ec);
        const fs::path src = root / "src" / "VoidHormiga-9.9.9-linux-x64";
        fs::create_directories(src);
        std::ofstream(src / "voidhormiga") << "#!/bin/sh\n";
        std::ofstream(src / "voidhormiga-cli") << "#!/bin/sh\n";
        const fs::path dl = root / "dl";
        fs::create_directories(dl);
        const fs::path archive = dl / "VoidHormiga-9.9.9-linux-x64.tar.gz";
        const std::string pack = "tar -czf '" + archive.string() + "' -C '" +
                                 (root / "src").string() + "' VoidHormiga-9.9.9-linux-x64";
        CHECK(std::system(pack.c_str()) == 0);

        const fs::path running = root / "apps" / "VoidHormiga-0.1.3-linux-x64";
        fs::create_directories(running);
        const up::Unpacked u = up::unpack_beside(archive, running);
        CHECK(u.ok);
        CHECK(u.folder == root / "apps" / "VoidHormiga-9.9.9-linux-x64");
        CHECK(fs::is_regular_file(u.binary));
        CHECK((fs::status(u.binary).permissions() & fs::perms::owner_exec) !=
              fs::perms::none);
        CHECK(fs::is_directory(running));   // the running copy is untouched

        // and never OVER the running copy, whatever the folders are called
        const fs::path same = root / "apps" / "VoidHormiga-9.9.9-linux-x64";
        const up::Unpacked over = up::unpack_beside(archive, same);
        CHECK(!over.ok);
        CHECK(over.error.find("running") != std::string::npos);
        fs::remove_all(root, ec);
    }
#endif

    if (failures == 0) {
        std::cout << "OK - version ordering + feed parsing + the decision + "
                     "the prompt + sha256 + the refusals + the archive\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
