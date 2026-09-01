/* publish.cpp — PUBLISHING: the deploy holiday, the rollback, the history, and
 * the operator's panel over all three.
 *
 * ── WHY THIS FILE EXISTS, which is the whole point of it ─────────────────────
 *
 * `deploy_site` was defined in `headless_main.cpp` and nowhere else, and that
 * unit is compiled into `voidhormiga-cli` only. So the desktop application —
 * the half a volunteer running an outreach organization actually opens — could
 * not publish a website at all. Not "had no button": had no code. Every publish
 * of a real, live site went through an agent typing a CLI command on the
 * owner's behalf, which works exactly as long as the agent is there.
 *
 * That is a violation of founding commitment 1, and a quiet one:
 *
 *   > The CLI (the command bar *inside* the app), the GUI, and any agent are
 *   > three callers of the same verbs — ONE interaction surface.
 *
 * The commitment is not "an agent can do everything a person can". It is that
 * there is one set of verbs and three doors onto it. A capability that reaches
 * one door and not the others has broken the surface into two applications, and
 * this one broke it in the direction that matters most — the person who owns
 * the data losing an ability an agent has.
 *
 * So publishing lives in a translation unit BOTH main()s compile, and the rule
 * this file is here to hold is: **no capability ships into one front-end.** If
 * it cannot be reached from the GUI, the command bar and a headless script, it
 * is not finished.
 *
 * ── and why it is `publish.cpp` and not `website.cpp` ────────────────────────
 *
 * A website is one thing Hormiga publishes, not what Hormiga is. The panel here
 * is written against `hol_static_host` NODES rather than against "the website":
 * it lists whatever publish targets the Antfarm holds, and a second kind of
 * target — a newsletter send, a mirror, a hosted calendar feed — arrives as
 * another holon and another row, not as a rewrite. The Antfarm is where a
 * backend is WIRED; this is where one is OPERATED, and those are genuinely
 * different jobs done by different people at different times.
 */
#include "app/app_internal.hpp"
#include "publish/cloudflare.hpp"
#include "json.hpp"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <chrono>
#include <map>
#include <sstream>

/* ── the static-site DEPLOY holiday (2026-08-19) ─────────────────────────────
 *
 * The last leg of the pipeline the Antfarm has always described:
 * core -> publisher -> site -> deployer. `render_site` builds the folder; this
 * hands it to a managed host.
 *
 * ── why the vendor's API is NOT hardcoded in C++ ─────────────────────────────
 *
 * The field agent asked for "the pages:deploy endpoint" called from here. Two
 * reasons that shape was not taken, and one of them is a rule this project
 * already has.
 *
 * First, honestly: a REST API's exact shape is something this code cannot
 * verify — there are no credentials in the developer's tree, deliberately — and
 * an unverifiable API baked into a compiled binary goes stale silently and needs
 * a rebuild to correct. The Antfarm exists so that a backend is CONFIGURATION,
 * not code; putting one vendor's endpoint into C++ would make the node a label
 * on a hardcoded decision, which is the opposite of what it is for.
 *
 * Second, it generalizes for free. `deploy_cmd` on the node is a command
 * template, so Netlify, a plain rsync, or next year's corrected Cloudflare
 * invocation all work without touching Hormiga. `provider` supplies a built-in
 * default so the ordinary case needs no template at all.
 *
 * THE BUILT-IN DEFAULT IS UNVERIFIED against a live account and is marked as
 * such where the operator will see it. That is the honest state of it: the
 * shape is right, the exact flags are the operator's to confirm, and the
 * override exists precisely so confirming does not require me.
 *
 * ── the token never touches a command line ───────────────────────────────────
 *
 * `publish_image` puts its ImgBB key in the argv it shells out with, which is
 * readable in a process listing by anything running as the user. For a deploy
 * token — which can publish a website — that is worse, so the secret goes into
 * a curl `--config` file written next to the database, used, and deleted. The
 * command that reaches the log has the token nowhere in it because it never had
 * it. (`publish_image` should move to this pattern too; noted, not done here.)
 */
/* Set (or clear, with "") an environment variable for the child process. */
static void put_token(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    if (value.empty()) unsetenv(name);
    else setenv(name, value.c_str(), 1);
#endif
}

// trim_secret now lives in publish/cloudflare.hpp - see that file
using hormiga::cloudflare::trim_secret;

/* Did the host say it worked?
 *
 * ONE reader for both one-way doors, because the defect this replaces was two
 * checks that disagreed: `deploy_site` looked for `"success":true` and a JSON
 * `"url":"…"` while the command it recommends — `npx wrangler pages deploy` —
 * prints prose, so a successful deploy was reported as a failure with its own
 * success message quoted as the fault. Writing a second, narrower check for
 * rollback would have been that mistake again.
 *
 * Deliberately generous and deliberately not a parser: a deploy that half
 * worked is something a person must read, and every vendor spells this
 * differently. A tool that says neither is still a failure, which is the
 * honest default. */
static bool host_said_ok(const std::string& resp) {
    auto has_ci = [&](const char* needle) {
        const std::string n(needle);
        for (size_t i = 0; i + n.size() <= resp.size(); ++i) {
            size_t k = 0;
            while (k < n.size() &&
                   std::tolower((unsigned char)resp[i + k]) ==
                       std::tolower((unsigned char)n[k]))
                ++k;
            if (k == n.size()) return true;
        }
        return false;
    };
    return has_ci("\"success\":true") || has_ci("\"success\": true") ||
           has_ci("deployment complete") || has_ci("deploy complete") ||
           has_ci("successfully deployed") || has_ci("deployed to") ||
           has_ci("site is live") || has_ci("rolled back") ||
           has_ci("rollback complete");
}

/* ── A RELATIVE PATH IN NODE CONFIG MEANS "BESIDE THE DATABASE" ──────────────
 *
 * The third instance of one bug in a week, and the field agent's report named the
 * rule that was missing rather than the symptom. `token_file` resolved against
 * the process working directory; the document did; and then `deploy_cmd`'s
 * script path did, which is what left the operator staring at:
 *
 *     [error] deploy: python: can't open file
 *             'C:\Users\migri\Documents\Projects\VoidHormiga\deploy_pages.py'
 *
 * The script is beside his database. He launched the app from somewhere else.
 * Nothing in that message is wrong and nothing in it is actionable.
 *
 * A `deploy_cmd` is an arbitrary command line, so there is no token in it we
 * are entitled to identify as "the path" and rewrite. What we CAN do is make
 * the whole command run where the operator thinks it runs — beside the
 * database, the same root `site/`, `assets/`, `exports/` and `fonts/` already
 * use. Then a relative path in a template means what a relative path means
 * everywhere else in this application, which was the entire ask.
 *
 * No new `script_dir` config key, deliberately: the report is right that a
 * fourth root would be one more thing to keep in sync, and the first time it
 * disagreed with `base_dir` somebody would lose an afternoon. */
static std::string run_in(const fs::path& dir, const std::string& cmd) {
#ifdef _WIN32
    // `/d` because cmd.exe will not change DRIVE without it, and a database on
    // D: with the app started from C: is an ordinary thing.
    return "cd /d \"" + dir.string() + "\" && " + cmd;
#else
    return "cd \"" + dir.string() + "\" && " + cmd;
#endif
}

/* Substitute {placeholders} a deploy template may use. The token is NOT among
 * them — it is only ever reachable through the curl config file. */
static std::string fill(std::string tpl, const std::map<std::string, std::string>& v) {
    for (const auto& [k, val] : v) {
        const std::string needle = "{" + k + "}";
        for (size_t i = tpl.find(needle); i != std::string::npos;
             i = tpl.find(needle, i + val.size()))
            tpl.replace(i, needle.size(), val);
    }
    return tpl;
}

std::string HormigaApp::deploy_site(const maiz::Scene& farm,
                                    std::string_view node) {
    /* Deploy is the one-way door, so its report reaches the terminal whatever
     * happens — including every early refusal. A refusal the caller cannot see
     * is how "it printed done and did nothing" started, and this function has
     * seven ways to decline. A scope guard means none of them can forget.
     *
     * stderr, so a --json caller's stdout stays parseable; the GUI reads the
     * same entries from its log strip. */
    const size_t log_from = log.size();
    struct Reporter {
        const std::vector<maiz::LogEntry>& log;
        size_t from;
        ~Reporter() {
            for (size_t i = from; i < log.size(); ++i)
                if (log[i].op == "deploy")
                    std::cerr << "  [" << log[i].level << "] deploy: "
                              << log[i].msg << "\n";
        }
    } _report{log, log_from};
    /* THIS DEPLOY'S ID, NOT THE LAST ONE'S. The field outlives the call, so a
     * publish that cannot learn its id would otherwise inherit the previous
     * one and stamp a `deployment` rune whose [restore] rolls back to the wrong
     * version — the one failure mode worse than having no id at all. */
    last_deploy_vendor_id.clear();
    /* THE ANTFARM ARRIVES PROJECTED, and that is the change that let this
     * function reach the desktop application at all (2026-08-20).
     *
     * It used to take the exported state document, rebuild a throwaway `Core`,
     * re-register three glyph sets and project — which meant it needed
     * `seed.hpp`, which is deliberately off the shared units' include path, so
     * it could only live in the headless translation unit. It therefore shipped
     * into the half of the application the person who owns the website does not
     * use, and every publish of a real site went through an agent typing a
     * command on their behalf.
     *
     * A `Scene` is what both front-ends already have. The replay is gone, the
     * dependency is gone, and the capability is in one place that both
     * main()s compile.
     */
    // Find the host node. Empty name picks the only one; several is a REFUSAL
    // rather than a guess — publishing the wrong website is not revertible.
    const maiz::SceneNode* host = nullptr;
    int hosts = 0;
    for (const auto& n : farm.nodes) {
        if (n.glyph != "hol_static_host") continue;
        ++hosts;
        if (node.empty() || n.name == node) host = &n;
    }
    if (!host) {
        log.push_back({"error", "deploy",
                       hosts == 0
                           ? "no hol_static_host node in the antfarm mantle - add "
                             "one and set provider/account_id/project/token_file"
                           : "no such host node: " + std::string(node)});
        return {};
    }
    if (node.empty() && hosts > 1) {
        log.push_back({"error", "deploy",
                       std::to_string(hosts) +
                           " static hosts are wired - name the one to publish to"});
        return {};
    }

    const std::string provider = field_value(*host, "provider");
    const std::string project = field_value(*host, "project");
    const std::string account = field_value(*host, "account_id");
    const std::string keyfile = field_value(*host, "token_file");
    const std::string tpl = field_value(*host, "deploy_cmd");

    const fs::path site = data_dir("site");
    std::error_code ec;
    /* ── EVERY LANGUAGE, OR NONE ─────────────────────────────────────────────
     *
     * This checked `index-en.html` and stopped, so a folder holding a current
     * English page and NO Spanish one passed the gate and published. The
     * upload is the whole folder; a folder missing half of a bilingual site is
     * not a site, and the failure is invisible from the English page an
     * operator naturally checks afterwards.
     *
     * The legacy single-page `index.html` (a site built before `page` runes)
     * still satisfies this, because it genuinely is the whole artifact. */
    /* ── EVERY LANGUAGE, BUILT IN ONE PASS, OR THIS DOOR DOES NOT OPEN ──────
     *
     * This used to check `index-en.html` and stop. The upload is the whole
     * `site/` FOLDER, so a folder holding a current English page and a Spanish
     * one that is missing — or present and a week old — published anyway, and
     * the operator could not see it, because the page a person checks
     * afterwards is the one in the language they read.
     *
     * Two conditions, and the second is the one that bites:
     *
     *   1. every language has an index — `effect render-site es` was never run;
     *   2. they were built TOGETHER — `effect render-site en` was run alone, so
     *      the Spanish page is real, valid, and stale.
     *
     * WHAT COUNTS AS A BUILT SITE, and the trap in asking. The first version of
     * this guard read "if `index.html` is absent" as "this is not a legacy
     * single-page site" and skipped everything below. It is not: a MODERN
     * render writes `index.html` too, as the language-chooser page that
     * redirects to `index-en.html`. The guard was therefore true of every real
     * site and the gate never ran once — found by deleting a Spanish page and
     * watching the deploy sail straight past it. The honest question is about
     * the per-language pages themselves.
     *
     * Refusals rather than warnings, because this is the one-way door: the cost
     * of being wrong is a community's Spanish site going stale in public, and
     * the remedy is one command. */
    {
        std::string missing, present;
        for (const std::string& lg : hormiga::site_langs()) {
            std::string& bucket =
                fs::exists(site / ("index-" + lg + ".html"), ec) ? present : missing;
            if (!bucket.empty()) bucket += ", ";
            bucket += lg;
        }

        const bool legacy_single_page =
            present.empty() && fs::exists(site / "index.html", ec);

        if (!legacy_single_page && !missing.empty()) {
            log.push_back({"error", "deploy",
                           "site/ has no index for " + missing +
                               " - run `effect render-site` first (with no "
                               "language argument it builds every language). "
                               "Publishing is not a per-language operation: the "
                               "whole folder goes out at once."});
            return {};
        }
        if (!legacy_single_page) {
            /* A render of every language writes them moments apart, so a wide
             * spread between newest and oldest means one language was skipped.
             * Ten minutes is deliberately loose: it is not trying to catch a
             * slow machine, only a language nobody rebuilt. */
            /* SEEDED FROM THE FIRST FILE, NOT FROM A SENTINEL. `newest` was
             * default-constructed — rep 0 — on the assumption that a real file
             * time is a positive number above the epoch. On this toolchain it
             * is not: a 2026 file reads as rep -4650394219000000000, so every
             * comparison against 0 failed, `newest` never moved off the epoch,
             * and the spread computed as 77 million minutes. Every deploy was
             * refused with a message about a stale page that was in fact
             * thirty milliseconds old.
             *
             * `file_time_type`'s epoch is unspecified by the standard, so no
             * sentinel is portable. The first successfully read timestamp is,
             * and it needs no assumption at all. */
            fs::file_time_type oldest{}, newest{};
            bool have = false;
            std::string old_lang;
            for (const std::string& lg : hormiga::site_langs()) {
                std::error_code fe;
                const auto t =
                    fs::last_write_time(site / ("index-" + lg + ".html"), fe);
                if (fe) continue;
                if (!have) { oldest = newest = t; old_lang = lg; have = true; continue; }
                if (t > newest) newest = t;
                if (t < oldest) { oldest = t; old_lang = lg; }
            }
            if (!have) old_lang.clear();
            if (!old_lang.empty() &&
                std::chrono::duration_cast<std::chrono::minutes>(newest - oldest)
                        .count() > 10) {
                log.push_back(
                    {"error", "deploy",
                     "site/ was not built in one pass - the " + old_lang +
                         " page is much older than the others, so publishing "
                         "now would put a stale " + old_lang +
                         " site in front of everyone. Run `effect render-site` "
                         "(no language argument builds all of them), then "
                         "publish again."});
                return {};
            }
        }
    }
    if (!on_shell_capture) {
        log.push_back({"error", "deploy", "no shell transport on this front-end"});
        return {};
    }
    /* ── THE CREDENTIAL COMES FROM THE VAULT FIRST (2026-08-20) ─────────────
     *
     * The author's correction, and it goes to what a `.miga` is FOR:
     *
     *   > the whole point of a miga file was to share information like API
     *   > keys, tokens, etc.
     *
     * A `token_file` is a path, and a path is exactly the thing that does not
     * travel: hand somebody your database and they get a node pointing at a
     * file on your disk. It is also what broke publishing today — a relative
     * path resolved against whichever folder the app happened to start in.
     *
     * `token_key` names a secret in the passphrase-locked vault
     * (`platform/vault.cpp`, XChaCha20-Poly1305 over argon2id). That rides the
     * bundle, encrypted, and needs no path at all. `token_file` remains for an
     * operator who would rather keep a secret out of the database entirely —
     * a legitimate preference, and still the right default for a shared repo.
     *
     * Vault first, file second, and the error names both. */
    std::string token;
    const std::string tkey = field_value(*host, "token_key");
    if (!tkey.empty() && vault.unlocked()) {
        token = trim_secret(vault.get(tkey));
        if (!token.empty())
            log.push_back({"info", "deploy", "using the token from the vault"});
    }
    if (token.empty() && keyfile.empty()) {
        log.push_back({"error", "deploy",
                       tkey.empty()
                           ? "no credential on " + host->name +
                                 " - set token_key (kept in the vault, travels "
                                 "with the .miga) or token_file"
                           : "token_key '" + tkey +
                                 "' is not in the vault - unlock it, or set "
                                 "token_file instead"});
        return {};
    }
    fs::path kp;
    if (token.empty()) {
        kp = fs::path(keyfile).is_absolute() ? fs::path(keyfile)
                                             : base_dir / keyfile;
        std::ifstream kin(kp, std::ios::binary);
        if (!kin) {
            log.push_back({"error", "deploy",
                           "cannot read token file: " + kp.string() +
                               " (a relative token_file resolves against the "
                               "folder the app was started in - an absolute "
                               "path, or token_key, avoids that)"});
            return {};
        }
        std::stringstream kss;
        kss << kin.rdbuf();
        token = trim_secret(kss.str());
        if (token.empty()) {
            log.push_back({"error", "deploy", "token file is empty: " + kp.string()});
            return {};
        }
    }

    // The secret lives in a curl config file, never in argv.
    const fs::path cfg = base_dir / ".deploy-curl.cfg";
    {
        std::ofstream co(cfg, std::ios::binary | std::ios::trunc);
        if (!co) {
            log.push_back({"error", "deploy", "cannot write " + cfg.string()});
            return {};
        }
        co << "header = \"Authorization: Bearer " << token << "\"\n";
    }

    std::string cmd;
    if (!tpl.empty()) {
        cmd = fill(tpl, {{"site", site.string()},
                         {"project", project},
                         {"account_id", account},
                         {"config", cfg.string()},
                         {"token_file", kp.string()},
                         // `{base}` is the database's folder, for a template
                         // that would rather be explicit than rely on the CWD
                         // `run_in` sets below. Both work; this one is legible
                         // in the log line.
                         {"base", base_dir.string()}});
    } else {
        /* ── NATIVE, SINCE 2026-08-20 — and this reverses a decision ─────────
         *
         * This branch used to REFUSE and hand the operator a wrangler command
         * to paste, on the reasoning that a backend is configuration and that
         * an API flow this code could not execute even once was one it had no
         * business asserting.
         *
         * The operator overruled it, and the day in between proved them right:
         * `deploy_cmd` had not removed the dependency, it had relocated it.
         * Publishing a real site needed Node, npm, a hand-pinned `wrangler@3`,
         * Python, and a wrapper script living in one person's folder — five
         * things a second organization would not have, to upload some HTML.
         *
         * `publish/cloudflare.cpp` now speaks the Pages Direct Upload protocol
         * itself, read out of wrangler's source rather than guessed, with the
         * asset hash verified against BLAKE3's published vectors. `deploy_cmd`
         * still wins when set — it is the escape hatch for a host we have never
         * heard of, and the way past this code the day Cloudflare moves. */
        if (provider == "cloudflare-pages" || provider == "cloudflare_pages" ||
            provider.empty()) {
            fs::remove(cfg, ec); // the native path never uses a curl config file
            hormiga::cloudflare::Config cc;
            cc.account_id = account;
            cc.project = project;
            cc.token = token;
            cc.site_dir = site.string();
            cc.work_dir = base_dir.string();
            cc.shell = on_shell_capture;
            /* THE LIVE CHANNEL IS THE ONLY CHANNEL. Every step used to be
             * logged twice — once here as it happened, once again by replaying
             * `res.steps` afterwards — which is why `hashed 57 file(s)` reached
             * the operator's console in duplicate. `progress` fires as the
             * upload proceeds, which is when the information is worth
             * something, so the replay is what goes. */
            cc.progress = [this](const std::string& m) {
                log.push_back({"info", "deploy", m});
            };
            const auto res = hormiga::cloudflare::deploy(cc);
            if (!res.ok) {
                /* Re-say the one step that failed, at error level. `progress`
                 * has no level, and a failure that scrolls past as [info] is a
                 * failure the operator reads as progress. There is at most one:
                 * every failing step returns immediately. */
                for (const auto& st : res.steps)
                    if (!st.ok)
                        log.push_back({"error", "deploy", st.what + ": " + st.detail});
                return {};
            }
            /* The deployment id is what a rollback needs, and it is the one
             * thing only the vendor knows — so it goes onto the `deployment`
             * rune the caller is about to write. */
            last_deploy_vendor_id = res.vendor_id;
            return res.url.empty() ? std::string("ok") : res.url;
        }
        log.push_back({"error", "deploy",
                       "no built-in upload for provider '" + provider +
                           "' - set deploy_cmd on " + host->name +
                           ". Placeholders: {site} {project} {account_id} "
                           "{config} {token_file}"});
        fs::remove(cfg, ec);
        return {};
    }

    /* THE TOKEN IN THE ENVIRONMENT, not in argv. A CLI deploy tool reads it
     * from `CLOUDFLARE_API_TOKEN`; putting it on the command line would make it
     * readable in a process listing by anything running as this user. Cleared
     * immediately after the call below. */
    put_token("CLOUDFLARE_API_TOKEN", token);
    put_token("HORMIGA_DEPLOY_TOKEN", token);
    log.push_back({"info", "deploy", "publishing site/ to " + host->name +
                                         " (" + provider + ")"});
    /* THE COMMAND, PLACEHOLDERS EXPANDED, AND WHERE IT RUNS. The report's ask,
     * and it makes this failure self-diagnosing: the operator's `python:
     * can't open file ...` was unreadable precisely because the line that would
     * have shown him which path was being tried was never printed. The token is
     * not in `cmd` — it never was, which is what makes this safe to log. */
    log.push_back({"info", "deploy", "in " + base_dir.string() + ": " + cmd});
    /* STDERR IS THE DIAGNOSTIC, and dropping it made every failure identical.
     * `_popen(cmd, "r")` reads stdout only; curl writes its progress meter and
     * ALL of its errors to stderr, so a DNS failure, a bad flag and a rejected
     * token all arrived here as an empty string and were reported as "the host
     * returned nothing". Found by running the built-in default against a real
     * endpoint — it was the message that hid the fact that the command itself
     * was wrong. (The ImgBB holiday has the same blindness; noted.) */
    const std::string resp = on_shell_capture(run_in(base_dir, cmd) + " 2>&1");
    fs::remove(cfg, ec); // the secret does not outlive the call
    put_token("CLOUDFLARE_API_TOKEN", "");
    put_token("HORMIGA_DEPLOY_TOKEN", "");

    /* Report what the host said. Deliberately not parsed into a schema: a deploy
     * that half-worked is something a person must read.
     *
     * ── THE MISMATCH INSIDE ONE FUNCTION (2026-08-19) ────────────────────────
     *
     * This looked for `"success":true` and a JSON `"url":"…"` — curl-and-JSON
     * shapes — while the command this same function SUGGESTS is
     * `npx wrangler pages deploy`, which prints prose:
     *
     *     ✨ Deployment complete! Take a peek over at https://….pages.dev
     *
     * No `"success":true`, no `"url":"`. So a SUCCESSFUL wrangler deploy fell
     * into the failure branch and was reported to the operator as an error with
     * its own success message quoted as the fault. That is the exact failure
     * mode this effect was built to prevent, between two lines of one function.
     *
     * The fix is to read both shapes, and to find the deployed address the way
     * a person does — the first https:// URL in the output — rather than the way
     * one vendor's JSON happens to spell it. A tool that prints neither is
     * still a failure, which is the honest default. */
    const bool ok = host_said_ok(resp);
    std::string url;
    const size_t up = resp.find("\"url\":\"");
    if (up != std::string::npos) {
        for (size_t i = up + 7; i < resp.size() && resp[i] != '"'; ++i) url += resp[i];
    } else {
        // the first https:// in the output, trimmed at whitespace or a quote —
        // which is how the person reading the terminal finds it too
        const size_t hp = resp.find("https://");
        if (hp != std::string::npos)
            for (size_t i = hp; i < resp.size(); ++i) {
                const char c = resp[i];
                if (std::isspace((unsigned char)c) || c == '"' || c == '\'' ||
                    c == '<' || c == ')')
                    break;
                url += c;
            }
        // trailing punctuation a sentence leaves on a URL
        while (!url.empty() && (url.back() == '.' || url.back() == ',')) url.pop_back();
    }

    if (!ok && url.empty()) {
        log.push_back({"error", "deploy",
                       resp.empty() ? "the host returned nothing (is curl on PATH?)"
                                    : resp.substr(0, 400)});
        return {};
    }
    /* ── THE VENDOR'S ID, RECOVERED FROM THE URL IT ALREADY GAVE US ──────────
     *
     * A `deploy_cmd` returns a URL and a success flag and has nowhere to put
     * the deployment id, so every fallback publish wrote a `deployment` rune
     * with `vendor_id ""` — a history row carrying a [restore] button with
     * nothing behind it. The report is right that this is worse than writing no
     * row at all: it offers an operator an undo that cannot run.
     *
     * Cloudflare Pages names each deployment's preview host after the
     * deployment id's first segment — `https://5015d22e.<project>.pages.dev`,
     * and the agent confirmed `5015d22e` against Cloudflare's own deployment
     * list. That is enough to restore against, and it is free.
     *
     * Deliberately narrow: only for this provider, only for the `*.pages.dev`
     * shape, and it takes the label only when it looks like the hex it should
     * be. A guessed id that is WRONG would roll back to something nobody asked
     * for, so an unrecognised URL leaves the field empty and the Publish panel
     * disables [restore] with the tooltip it already has. */
    if (last_deploy_vendor_id.empty() && !url.empty() &&
        (provider == "cloudflare-pages" || provider == "cloudflare_pages" ||
         provider.empty())) {
        const size_t s = url.find("://");
        const size_t b = s == std::string::npos ? std::string::npos : s + 3;
        const size_t d = b == std::string::npos ? std::string::npos : url.find('.', b);
        if (d != std::string::npos &&
            url.find(".pages.dev", d) != std::string::npos) {
            const std::string label = url.substr(b, d - b);
            bool hex = !label.empty() && label.size() <= 16;
            for (char c : label)
                hex = hex && std::isxdigit((unsigned char)c);
            if (hex) {
                last_deploy_vendor_id = label;
                log.push_back({"info", "deploy",
                               "deployment id " + label +
                                   " (read from the URL - the upload command "
                                   "has no channel for it)"});
            }
        }
    }
    log.push_back({"info", "deploy", url.empty() ? "host reported success" : url});
    return url.empty() ? std::string("ok") : url;
}

/* ── the publish HISTORY, and why it is runes rather than a vendor query ─────
 *
 * The operator's question was *"buttons to revert to a previous version, like a
 * mini git sort of thing"*, and the tempting answer is to ask the host: every
 * managed CDN keeps a deployment list, and Cloudflare's is one GET away.
 *
 * That answer is wrong as the PRIMARY record, for the reason
 * `web-platform.md` opens with and makes the acceptance test for every vendor:
 * **every cloud host is disposable.** A history that lives in a vendor's
 * database is a history we lose when we walk away from them — and walking away
 * is the property the whole page is arranged to protect.
 *
 * So Hormiga keeps its own. One `deployment` rune per publish, written through
 * the dispatcher like every other change, which means it is logged, attributed,
 * replayable, diffable against `_baseline`, and in the `.miga` when the org
 * takes their data somewhere else. Founding commitment 1, applied to the one
 * operation that leaves the document.
 *
 * The vendor's own list is still useful and still read — it is what gives each
 * past deploy its permanent URL, so [view] can show you the old site before you
 * decide to restore it. It is an ENRICHMENT of a record we already hold, not
 * the record.
 *
 * Returns the commands rather than dispatching them: an effect must not reach
 * around into the model it is an effect OF. The caller dispatches, in whichever
 * front-end it is, as an ordinary batch. */
std::vector<std::string> HormigaApp::deployment_record(const maiz::Scene& farm,
                                                       const std::string& host,
                                                       const std::string& url,
                                                       const std::string& document,
                                                       const std::string& lang) {
    std::vector<std::string> out;
    char stamp[32], name[96];
    const std::time_t t = std::time(nullptr);
    std::tm lt{};
#ifdef _WIN32
    localtime_s(&lt, &t);
#else
    localtime_r(&t, &lt);
#endif
    std::snprintf(stamp, sizeof stamp, "%04d-%02d-%02dT%02d:%02d:%02d",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                  lt.tm_min, lt.tm_sec);
    std::snprintf(name, sizeof name, "deploy-%04d%02d%02d-%02d%02d%02d",
                  lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                  lt.tm_min, lt.tm_sec);
    auto q = [](const std::string& v) { return "'" + v + "'"; };
    out.push_back(std::string("use ") + kAntfarmMantle);
    /* THE PREVIOUS LIVE DEPLOY STOPS BEING LIVE. Exactly one `state live` at a
     * time is what makes "which one is up right now" answerable without asking
     * the vendor — and it is a command, so it is in the log with everything
     * else rather than being inferred from timestamps later. */
    for (const auto& n : farm.nodes)
        if (n.glyph == "deployment" && field_value(n, "state") == "live")
            out.push_back("set " + n.name + " state superseded");
    out.push_back(std::string("rune new deployment ") + name);
    out.push_back(std::string("set ") + name + " host " + q(host));
    if (!url.empty()) out.push_back(std::string("set ") + name + " url " + q(url));
    out.push_back(std::string("set ") + name + " at " + q(stamp));
    out.push_back(std::string("set ") + name + " document " + q(document));
    out.push_back(std::string("set ") + name + " lang " + q(lang));
    if (!last_deploy_vendor_id.empty())
        out.push_back(std::string("set ") + name + " vendor_id " +
                      q(last_deploy_vendor_id));
    out.push_back(std::string("set ") + name + " state live");
    return out;
}

/* ── rollback: the undo, as configuration ────────────────────────────────────
 *
 * `rollback_cmd` is `deploy_cmd`'s sibling and exists for the same reason: the
 * vendor call is the operator's to state, not ours to compile in. Cloudflare,
 * Netlify and GitHub Pages all keep deployment history and all expose a
 * rollback; none of them agree on how, and none of them are stable enough to
 * bake into a binary that needs a release to correct.
 *
 * `{deployment}` is the extra placeholder — the vendor's own id for the version
 * to go back to, which we hold on the `deployment` rune precisely so that
 * restoring does not require asking the vendor what it used to be called.
 *
 * This is a one-way door in the same sense `deploy-site` is: it changes what
 * the public sees, immediately. It is a *smaller* door — it puts back something
 * that was already live — but it is still not `revert`. */
std::string HormigaApp::rollback_site(const maiz::Scene& farm,
                                      std::string_view node,
                                      std::string_view deployment) {
    const size_t log_from = log.size();
    struct Reporter {
        const std::vector<maiz::LogEntry>& log;
        size_t from;
        ~Reporter() {
            for (size_t i = from; i < log.size(); ++i)
                if (log[i].op == "rollback")
                    std::cerr << "  [" << log[i].level << "] rollback: "
                              << log[i].msg << "\n";
        }
    } _report{log, log_from};

    const maiz::SceneNode* host = nullptr;
    int hosts = 0;
    for (const auto& n : farm.nodes) {
        if (n.glyph != "hol_static_host") continue;
        ++hosts;
        if (node.empty() || n.name == node) host = &n;
    }
    if (!host) {
        log.push_back({"error", "rollback",
                       hosts == 0 ? "no hol_static_host node in the antfarm mantle"
                                  : "no such host node: " + std::string(node)});
        return {};
    }
    if (node.empty() && hosts > 1) {
        log.push_back({"error", "rollback",
                       std::to_string(hosts) +
                           " static hosts are wired - name the one to roll back"});
        return {};
    }
    const std::string tpl = field_value(*host, "rollback_cmd");
    if (tpl.empty()) {
        log.push_back(
            {"error", "rollback",
             "no rollback_cmd on " + host->name +
                 " - set it to your host's rollback command. Placeholders: "
                 "{deployment} {project} {account_id} {config} {token_file}. "
                 "For Cloudflare Pages: curl -X POST --config \"{config}\" "
                 "https://api.cloudflare.com/client/v4/accounts/{account_id}"
                 "/pages/projects/{project}/deployments/{deployment}/rollback"});
        return {};
    }
    if (deployment.empty()) {
        log.push_back({"error", "rollback",
                       "name the deployment to restore - rolling back to an "
                       "unspecified version is not a thing to guess at"});
        return {};
    }
    if (!on_shell_capture) {
        log.push_back({"error", "rollback", "no shell transport on this front-end"});
        return {};
    }
    const std::string keyfile = field_value(*host, "token_file");
    fs::path kp = fs::path(keyfile).is_absolute() ? fs::path(keyfile)
                                                  : base_dir / keyfile;
    std::ifstream kin(kp, std::ios::binary);
    if (!kin) {
        log.push_back({"error", "rollback", "cannot read token file: " + kp.string()});
        return {};
    }
    std::stringstream kss;
    kss << kin.rdbuf();
    const std::string token = trim_secret(kss.str());
    if (token.empty()) {
        log.push_back({"error", "rollback", "token file is empty: " + kp.string()});
        return {};
    }
    std::error_code ec;
    const fs::path cfg = base_dir / ".rollback-curl.cfg";
    {
        std::ofstream co(cfg, std::ios::binary | std::ios::trunc);
        if (!co) {
            log.push_back({"error", "rollback", "cannot write " + cfg.string()});
            return {};
        }
        co << "header = \"Authorization: Bearer " << token << "\"\n";
    }
    const std::string cmd =
        fill(tpl, {{"deployment", std::string(deployment)},
                   {"project", field_value(*host, "project")},
                   {"account_id", field_value(*host, "account_id")},
                   {"config", cfg.string()},
                   {"token_file", kp.string()},
                   {"base", base_dir.string()}});
    put_token("CLOUDFLARE_API_TOKEN", token);
    log.push_back({"info", "rollback",
                   "restoring " + std::string(deployment) + " on " + host->name});
    // same root as deploy: a relative path in a rollback_cmd means beside the
    // database, not beside whatever folder the app was started from
    const std::string resp = on_shell_capture(run_in(base_dir, cmd) + " 2>&1");
    fs::remove(cfg, ec);
    put_token("CLOUDFLARE_API_TOKEN", "");
    if (!host_said_ok(resp)) {
        log.push_back({"error", "rollback",
                       resp.empty() ? "the host returned nothing (is curl on PATH?)"
                                    : resp.substr(0, 400)});
        return {};
    }
    log.push_back({"info", "rollback", "restored " + std::string(deployment)});
    return std::string(deployment);
}

/* ── THE PUBLISH PANEL ────────────────────────────────────────────────────────
 *
 * The operator's surface. Four questions, visible together, because the fourth
 * one is irreversible and a person should see the other three before they
 * answer it:
 *
 *     what am I publishing · where does it go · what does it look like · go
 *
 * ── preview and publish are DIFFERENT WORDS FOR A REASON ─────────────────────
 *
 * The author's rule for this panel: *"there should always be a difference
 * between a preview and a publish."* The difference here is not styling, it is
 * three separate things that all point the same way:
 *
 *   - Build and Preview are ordinary buttons. Publish is a wide green one, set
 *     apart, and it is the only control in this application that opens a
 *     confirmation.
 *   - The confirmation quotes the effect's own `consequence` string verbatim.
 *     That sentence was written for the person deciding whether to hand an
 *     agent `--allow-effects`, and it is the right sentence here for the same
 *     reason. A GUI path must not skip a warning the CLI path gives.
 *   - Preview says where it goes (a file, or localhost). Publish says who sees
 *     it (everyone, now).
 *
 * ── it is not a website tab ──────────────────────────────────────────────────
 *
 * Hormiga publishes a website; it is not a website builder. So this panel is
 * written against the `hol_static_host` NODES in the Antfarm, not against "the
 * site": another kind of publish target arrives as another holon and another
 * row. The Antfarm is where you WIRE a backend, this is where you OPERATE one,
 * and those are different jobs — usually done by different people, and always
 * at different times.
 */
