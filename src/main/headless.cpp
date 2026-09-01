/* headless_main.cpp — Void Hormiga with no window attached.
 *
 * THE USE CASE, in the author's words (2026-08-18): give an agent a folder of
 * emails and fliers and say *"from this, update the database and create a new
 * newsletter."* The agent runs in a real terminal, somewhere else entirely; the
 * changes land in the document the GUI opens; a person reviews them after.
 *
 * WHY THIS IS SHORT. Void Maiz's `voidmaiz_headless` (2026-08-18) does the
 * session, the advisory lock, the journal, the effect gate and the briefing. Our
 * side is a declaration: which glyphs, which actions, which predicates, which
 * effects — the same values the GUI uses, so the two front-ends cannot drift
 * into two applications.
 *
 * HEADLESS IS THE REMOVAL OF A PROJECTION, not a second code path. There is no
 * sync and no import: a headless session and the GUI are two front-ends over ONE
 * state document, because the model has lived in Void Core since day one.
 *
 * ── the one thing that is NOT ~40 lines, and why ─────────────────────────────
 *
 * `render` and `render-site` — the newsletter and the website — are the half of
 * the ask that is not dispatcher commands. They live in `section_web.cpp` as
 * `HormigaApp::` methods, and `HormigaApp` owns its own `Core`, while a headless
 * `Session` owns the one that matters.
 *
 * They are reached HERE through the state document, which is the interface the
 * architecture already claims: `configure` hands us the session's `Core`, we
 * export it, replay it into a throwaway `HormigaApp`, and render from that. No
 * refactor, no second implementation of the render seam, and the newsletter an
 * agent generates is byte-identical to the one the button generates because it
 * is the same function over the same state.
 *
 * That `HormigaApp` never opens a window and never touches ImGui — measured
 * 2026-08-18: `section_web.cpp` contains **zero** ImGui references, the whole
 * Output domain having been view-free since it was written. The binary does
 * link the view module, because `HormigaApp`'s other methods are in the same
 * struct; it simply never calls them. The honest cost is binary size, and the
 * honest alternative was a second renderer, which is worse.
 */
#include "app/app_internal.hpp"
#include "platform/backup.hpp"
#include "app/paths.hpp"        // resolve_data_dir: where this database's assets are
#include "json.hpp" // the effect args arrive as {"args":[…]}
#include "domain/date_query.hpp"           // the date-aware block grammar
#include "domain/flier_read.hpp"           // proposing tags from a flier
#include "domain/seed.hpp"                 // register_glyphs: data + block + antfarm
#include "voidmaiz/headless.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>

namespace {

/* The session's Core, captured in `configure`.
 *
 * `Core::EffectHandler` is `(op, args)` and carries no handle, so an effect
 * cannot reach the model it is an effect OF. `HostApp::configure` is the only
 * place a `Core&` is offered, and it runs after the state document is loaded —
 * which is exactly the seam we need and is why this pointer exists rather than
 * a redesign. Single-threaded and process-lifetime; a session is a process. */
maiz::Core* g_core = nullptr;
std::filesystem::path g_base_dir;   // the DATABASE folder (the working directory)
std::filesystem::path g_ship_dir;   // where the BINARY lives (its shipped files)
std::string g_state_name = "demo-org.json";  // the document's filename (--state)

/* A MERGED DOCUMENT WAITING FOR THE SESSION TO GET OUT OF THE WAY.
 *
 * Set by the sync effect, written by `main` AFTER `run_cli` returns. Two
 * reasons, and the first one is a crash rather than a preference:
 *
 *  1. Replacing the session's Core from inside the effect handler frees the
 *     `std::function` that is executing (app.hpp, `pending_state`). Measured:
 *     the process died mid-effect, before the session saved, and left a stale
 *     lock behind a merge that had reported success.
 *  2. Even if it did not crash, the Session saves `core.export_state()` on
 *     close, so anything written to the file before then is overwritten.
 *
 * By the time this is drained the session has closed, saved, and released its
 * advisory lock -- so this write is the last one and it is uncontended. */
std::string g_pending_state;

/* WHAT SHIPS BESIDE THE BINARY, and why it is not the working directory.
 *
 * An agent runs this in the folder holding somebody's database, which is not a
 * checkout — it has no `okf/`, no guide, no source. So anything an agent needs
 * in order to learn the application has to be found next to the EXECUTABLE, not
 * next to the data. Reported as a briefing bug on first contact: `okf_root` was
 * "okf", resolved against the working directory, and was therefore a dead
 * pointer for every real caller.
 *
 * Falls back to the working directory, which is what a developer running from a
 * checkout has. */
std::string shipped(const char* name) {
    std::error_code ec;
    if (!g_ship_dir.empty() && std::filesystem::exists(g_ship_dir / name, ec))
        return (g_ship_dir / name).string();
    return name;
}

/* Render the newsletter or the site, through a throwaway app over the session's
 * current state. Returns the written path, or "" (the effect result convention).
 *
 * WHY THE STATE DOCUMENT AND NOT THE LIVE CORE. Replaying the export is the
 * cheapest thing that is definitely correct: `HormigaApp::core` is a value
 * member with three reassignment sites in the GUI's own boot, so it cannot be
 * aliased to somebody else's Core without changing what the GUI is. Replay
 * costs one serialization of a document that is already small enough to save on
 * every edit. */
/* THE SHELL TRANSPORT, WHICH HEADLESS DID NOT HAVE (2026-08-19).
 *
 * Hormiga's network holidays — ImgBB publish, map tiles, and now the static-site
 * deploy — reach the world by shelling out to `curl`, which ships with Windows
 * 10+ and needs nothing vendored or linked. That transport is a PLATFORM SEAM
 * (`on_shell_capture`), and only the GUI shell was setting it.
 *
 * So headless every one of them was silently a no-op: the field agent reported
 * that `build`, `deploy` and `preview` "print done and exit 0 having done
 * nothing", and this is why. An agent asked for the one operation that would
 * publish its images, was told it happened, and shipped a newsletter full of
 * `unpublished` boxes.
 *
 * Setting it here fixes the whole class at once rather than one holiday. */
std::string shell_capture(const std::string& cmd) {
    std::string out;
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, pipe)) > 0) out.append(buf, n);
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return out;
}

/* A throwaway app over the session's state, wired for world-facing work. */
void wire(HormigaApp& app) {
    app.base_dir = g_base_dir;
    app.ship_dir = g_ship_dir;   // the webfonts et al. travel with the EXE
    app.on_shell_capture = shell_capture;
    app.on_register_glyphs = [](maiz::Core& c) {
        hormiga::register_glyphs(c);
        hormiga::register_block_glyphs(c);
        hormiga::register_antfarm_glyphs(c);
    };
}

/* THE DOCUMENT THE LAST `render-site` ACTUALLY BUILT (2026-08-28).
 *
 * From the field report, §6: *"the `deployment` rune's `document` field is
 * still empty on every deploy. The effect knows which mantle it rendered."*
 *
 * It did, and it threw the fact away. `effect deploy-site <node>` is the
 * documented form and the one an agent uses; `document` is an OPTIONAL second
 * argument nobody passes, so `deployment_record` was handed "" every time and
 * wrote a history that could not answer the one question a history is for —
 * which document is this that went out.
 *
 * Guessing was never available: `render_from_state` says so, and a headless
 * publish of the wrong newsletter is not something `revert` helps with. But
 * REMEMBERING is not guessing. A session that rendered `org-website` and then
 * published knows, with certainty, what is in `site/`, and that certainty is
 * exactly what the record should carry. A session that publishes a folder it
 * did not build still records nothing, which is also the truth.
 *
 * Session-scoped and never persisted: it is a fact about this process, not
 * about the database. */
std::string g_last_rendered_doc;

std::string render_through_app(std::string_view op, std::string_view lang,
                               std::string_view document) {
    if (!g_core) return {};
    HormigaApp app;
    wire(app);
    const std::string out =
        app.render_from_state(g_core->export_state(), op, lang, document);
    if (op == "render-site" && !out.empty())
        g_last_rendered_doc = std::string(document);
    return out;
}

/* `effect render <lang> [<document>]` — the args arrive as `{"args":[…]}`.
 *
 * THE DOCUMENT IS NAMED, NOT GUESSED. In the GUI the Builder has a selected
 * document (`cur_doc`), which is view state and is not in the state document at
 * all — so a headless render has nothing to inherit. Defaulting to "whichever
 * looks like a newsletter" would mean an agent could publish the wrong one, and
 * a wrong newsletter is not a thing `revert` can help with. */
std::vector<std::string> effect_args(std::string_view args) {
    std::vector<std::string> out;
    try {
        auto j = nlohmann::json::parse(args);
        if (j.contains("args"))
            for (const auto& a : j["args"]) out.push_back(a.get<std::string>());
    } catch (...) {}
    return out;
}



/* ── refuse a .miga bundle as a state document ────────────────────────────────
 *
 * A P0 found by the first real agent run (2026-08-18), and the failure is the
 * shape this project keeps meeting: silent, destructive, `ok: true`.
 *
 * `.miga` is Hormiga's own container — `{magic, version, meta, assets, rebuild,
 * state}` — where the document the session understands is nested under `state`.
 * Handed one via `--state`, the session sees no `mantles` key, concludes the
 * document is empty, and **on the next write merges a fresh empty document's
 * keys into the bundle's top level**, beside the envelope. The real data
 * survives under `state`, but the file now carries a phantom empty document
 * that shadows it. A 60 MB archive of somebody's contacts is one `rune new`
 * away from that.
 *
 * WHY THE CHECK LIVES HERE and not upstream: `.miga` is ours. Void Maiz's
 * session takes a path and reads JSON; it cannot be expected to know a
 * container format it has never heard of. The host that owns the format owns
 * the sniff.
 *
 * Deliberately a REFUSAL rather than transparent unwrapping. Opening the bundle
 * and re-wrapping on save would be friendlier and would also mean a headless
 * session silently rewriting an archive's envelope, assets and rebuild
 * transcript — which is a lot of somebody's data to touch on an inference about
 * what they meant. Say what is wrong and let a person decide.
 *
 * `miga::looks_like_miga` is not used: it only checks for a leading `{`, which
 * every state document has, so it would reject valid input. */
bool is_miga_bundle(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return false;
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    char head[512] = {};
    in.read(head, sizeof head - 1);
    const std::string s(head, (size_t)in.gcount());
    // Both v2 and v3 open with a JSON object carrying "magic":"MIGA". Checking
    // the pair rather than the word alone keeps a rune whose text mentions
    // "MIGA" from being mistaken for an envelope.
    const size_t m = s.find("\"magic\"");
    return m != std::string::npos && s.find("\"MIGA\"", m) != std::string::npos;
}

maiz::HostApp build_app() {
    maiz::HostApp h;
    h.id = "hormiga";
    h.label = "Void Hormiga";
    h.version = "0.1";
    h.okf_root = shipped("okf");   // beside the BINARY; see shipped()

    /* THE AGENT'S TOOL LIST. Void Maiz built `ActionRegistry` in July so that
     * "a volunteer's click and an agent's `map place contact @here` become the
     * SAME transcript entry." That turned out to be the headless requirement,
     * written down a month early — the briefing is this manifest verbatim. */
    for (const auto& a : hormiga::make_map_actions().actions) h.actions.add(a);
    for (const auto& a : hormiga::make_doc_actions().actions) h.actions.add(a);

    /* The domain's condition vocabulary, so an agent can write Allomone rules
     * without reading our source. */
    h.predicates = hormiga::allomone::predicates({}).names();

    h.configure = [](maiz::Core& core) {
        g_core = &core;
        // The SAME registration the GUI runs. Glyphs are host config and are
        // not part of the exported state, so a divergence here is the one way
        // two front-ends can silently disagree about one document.
        hormiga::register_glyphs(core);
        hormiga::register_block_glyphs(core);
        hormiga::register_antfarm_glyphs(core);
    };

    h.effects = [](std::string_view op, std::string_view args) -> std::string {
        if (op == "save") {
            /* The SQLite mirror. Void Maiz's `Session::save()` writes the JSON
             * state document and deliberately does NOT dispatch this verb, so
             * an agent's work stays visible as unsaved changes against the
             * baseline — which is the whole cross-process review story. This
             * runs only when something asks for `save` on purpose, and it keeps
             * the database from going stale behind the document. */
            if (!g_core) return {};
            /* NAMED AFTER THE DOCUMENT IT MIRRORS. This was the literal
             * "demo-org.db", so a session on `org_01.state.json` mirrored it
             * into a file named after a different database — and if two
             * documents ever shared a folder, into the SAME file, with the
             * mirror belonging to whichever ran last. */
            hormiga::Storage db(g_base_dir /
                                (std::filesystem::path(g_state_name).stem().string() +
                                 ".db"));
            if (db.ok()) db.save(std::string(args), {maiz::project_scene(*g_core)});
            return {};
        }
        if (op == "deploy-site" || op == "deploy") {
            if (!g_core) return {};
            const std::vector<std::string> a = effect_args(args);
            const std::string node = a.empty() ? std::string() : a[0];
            /* Named wins; otherwise the document THIS SESSION rendered into
             * `site/`, which is what the upload is about to publish. Still ""
             * when the session rendered nothing — a publisher of somebody
             * else's folder has nothing honest to write there. */
            std::string doc = a.size() > 1 ? a[1] : std::string();
            if (doc.empty()) doc = g_last_rendered_doc;

            /* ── WHY THIS DOES NOT RE-RENDER ─────────────────────────────────
             *
             * The obvious fix for the stale-Spanish-page bug is to rebuild every
             * language right here, and in THIS front-end it is wrong. `doc` is
             * optional — `effect deploy-site org-pages` is the documented form
             * and the one the agent uses — and a headless render has no
             * `cur_doc` to inherit (see `render_from_state`, which says so).
             * Rendering with no document named would build a site from an EMPTY
             * mantle and overwrite a good `site/` folder with nothing, one line
             * before the one-way door that publishes it. That would be a worse
             * bug than the one being fixed, and of the same family: a step that
             * quietly does the wrong job because nobody told it which job.
             *
             * So the completeness-and-staleness gate lives in `deploy_site`,
             * where BOTH front-ends pass through it, and this path stays what it
             * says it is — a publisher of an already-built folder. The GUI,
             * which always knows its document, renders every language before
             * publishing; and `effect render-site` with no language argument now
             * builds every language, which is the step that was silently doing
             * half the job. */
            HormigaApp app;
            wire(app);
            /* THE ANTFARM, PROJECTED FROM THE SESSION'S OWN CORE. This used to
             * export the whole state document and replay it into a throwaway
             * app — the round trip that forced the function to live in this
             * unit, and therefore out of the desktop application entirely. */
            maiz::ProjectOptions ao;
            ao.mantle = kAntfarmMantle;
            const maiz::Scene farm = maiz::project_scene(*g_core, ao);
            const std::string url = app.deploy_site(farm, node);
            if (url.empty()) return {};
            /* RECORD IT THROUGH THE DOOR. The commands are dispatched into the
             * SESSION's core, so the publish lands in the journal with an actor
             * and shows up in `status`/`diff` like any other change — and the
             * person reviewing an unattended agent's work can see that it
             * published, which is the one thing they most need to see. */
            /* `lang` was the literal "en" here. It was never true — the upload
             * is the whole folder — and it made the history assert something
             * the deploy had not done. `site_langs_str()` is what actually
             * went out. */
            for (const auto& c :
                 app.deployment_record(farm, node, url, doc, hormiga::site_langs_str()))
                g_core->dispatch(c);
            return "\"" + url + "\"";
        }
        if (op == "rollback-site") {
            if (!g_core) return {};
            const std::vector<std::string> a = effect_args(args);
            HormigaApp app;
            wire(app);
            maiz::ProjectOptions ao;
            ao.mantle = kAntfarmMantle;
            const std::string done = app.rollback_site(
                maiz::project_scene(*g_core, ao),
                a.empty() ? std::string() : a[0],
                a.size() > 1 ? a[1] : std::string());
            if (done.empty()) return {};
            /* The history moves with what is live. Same rule as publishing: a
             * fact about the world is a command, not something computed later
             * from timestamps. */
            maiz::Scene farm = maiz::project_scene(*g_core, ao);
            g_core->dispatch(std::string("use ") + kAntfarmMantle);
            for (const auto& n : farm.nodes)
                if (n.glyph == "deployment" && field_value(n, "state") == "live")
                    g_core->dispatch("set " + n.name + " state superseded");
            for (const auto& n : farm.nodes)
                if (n.glyph == "deployment" && field_value(n, "vendor_id") == done)
                    g_core->dispatch("set " + n.name + " state live");
            return "\"" + done + "\"";
        }
        /* ── SYNC (okf/concepts/platform/collaboration.md) ────────────────────
         *
         * Five verbs, one shape: they REPORT, and they write only when the
         * caller adds `apply`. The report is the product — a merge is the one
         * operation here that can lose somebody's work, so seeing what would
         * happen has to be what you get by typing the obvious thing.
         *
         * The app object is a shell as everywhere else in this file; the STATE
         * comes from the session's core and the merged document goes back to it
         * through `on_take_state`, which `wire` installed. */
        /* ── `effect query '<expr>'` — what a block query will actually select
         *
         * `ls --tag` is Void Core's verb and evaluates Void Core's grammar,
         * which is a grammar with no idea what today is. The moment a block can
         * say `date:future`, `ls --tag` stops being able to answer the question
         * the AGENT-GUIDE tells people to ask it — *check the expression before
         * you put it in a block* — and an agent that trusts it gets a confident
         * wrong answer, which is the exact failure that guidance exists to
         * prevent.
         *
         * So this is `ls --tag` plus the clock, in the one place that knows
         * about `date:`: the same `query_matches` every renderer and the
         * Builder preview call, over the same data mantle, printing the same
         * set the page will contain. Not a second grammar — the same evaluation
         * reached through a second door.
         *
         * It prints the `date:` verdict beside each hit, because "why did this
         * flier not appear" is answered by *its event was nine days ago* and by
         * nothing else, and a matcher that would not say so is a matcher a
         * person has to guess at. Read-only: it dispatches nothing. */
        if (op == "query") {
            if (!g_core) return {};
            const std::vector<std::string> a = effect_args(args);
            /* The whole expression, however the shell split it. `AND`/`OR` are
             * space-separated words and a caller who forgets one pair of quotes
             * would otherwise silently evaluate only `type:event`. */
            std::string expr;
            for (const auto& t : a) {
                if (!expr.empty()) expr += " ";
                expr += t;
            }
            maiz::ProjectOptions qo;
            qo.mantle = kDataMantle;
            const maiz::Scene qdata = maiz::project_scene(*g_core, qo);
            const long long today = hormiga::today_days();
            auto verdict = [&](const maiz::SceneNode& n) -> const char* {
                switch (hormiga::when_of(qdata, n, today)) {
                case hormiga::When::Past: return "past";
                case hormiga::When::Today: return "today";
                case hormiga::When::Future: return "future";
                case hormiga::When::Recurring: return "recurring";
                case hormiga::When::Undated: return "undated";
                }
                return "undated";
            };
            int hits = 0;
            for (const auto& n : qdata.nodes) {
                if (!hormiga::query_matches(expr, qdata, n, today)) continue;
                ++hits;
                /* stderr for the listing, stdout for the count — the same split
                 * `render` keeps, so `--json` stays parseable. */
                std::cerr << "  " << n.name << "  [" << n.glyph << "] "
                          << verdict(n) << "\n";
            }
            std::cerr << "  " << hits << " match"
                      << (hits == 1 ? "" : "es")
                      << (hormiga::mentions_date(expr)
                              ? "  (dates as of today)"
                              : "")
                      << "\n";
            return std::to_string(hits);
        }
        /* ── `effect read-flier <image-rune>` — what is printed on the sheet
         *
         * The report asked for "an effect that PROPOSES a transcript of tags
         * from an image, for a person to dispatch", explicitly not for OCR in
         * the binary. This is that: it runs the recognizer the OPERATOR named
         * in `config set tools.image_text`, hands the text to
         * `domain/flier_read.hpp`, and prints commands. It dispatches nothing.
         *
         * The half that earns its place is the DATE. A flier says "September
         * 16th" on its face and nothing in the database has ever checked that
         * against the event it is linked to, which is how an August 19 sheet
         * stayed on a home page for nine days. So the mismatch is the loud line
         * in the output, and the `kw:` tags are the bulk.
         *
         * Absent by default: no config key, no capability, and a message that
         * says exactly what to set rather than a silent no-op. */
        if (op == "read-flier") {
            if (!g_core) return {};
            const std::vector<std::string> a = effect_args(args);
            if (a.empty()) {
                std::cerr << "  [error] read-flier: name an image rune\n";
                return {};
            }
            const std::string rune = a[0];

            std::string tool = g_core->dispatch("config get tools.image_text").data;
            if (tool.size() >= 2 && tool.front() == '"' && tool.back() == '"')
                tool = tool.substr(1, tool.size() - 2);
            if (tool == "null") tool.clear();
            if (tool.empty()) {
                std::cerr
                    << "  [error] read-flier: no recognizer configured. This "
                       "effect does not read images itself - it runs the tool "
                       "you name and turns what it prints into proposed "
                       "commands. For example:\n"
                       "      config set tools.image_text \"tesseract {path} - "
                       "-l eng+spa\"\n"
                       "    {path} is replaced with the image file.\n";
                return {};
            }

            maiz::ProjectOptions fo;
            fo.mantle = kDataMantle;
            const maiz::Scene fdata = maiz::project_scene(*g_core, fo);
            const maiz::SceneNode* img = fdata.find(rune);
            if (!img || img->glyph != "image") {
                std::cerr << "  [error] read-flier: " << rune
                          << " is not an image rune in the data mantle\n";
                return {};
            }
            const std::string path = hormiga::field_value(*img, "path");
            if (path.empty()) {
                std::cerr << "  [error] read-flier: " << rune
                          << " has no `path`, so there is no file to read\n";
                return {};
            }
            std::filesystem::path abs = std::filesystem::path(path);
            if (abs.is_relative()) abs = g_base_dir / abs;
            std::error_code fec;
            if (!std::filesystem::exists(abs, fec)) {
                std::cerr << "  [error] read-flier: " << abs.string()
                          << " is not on this machine\n";
                return {};
            }

            /* The path is QUOTED into the template rather than concatenated
             * bare: an asset filename with a space in it is ordinary, and the
             * operator writing the template should not have to think about it.
             * The template itself is the operator's own string, which is the
             * same trust boundary `deploy_cmd` sits on. */
            std::string cmd = tool;
            const std::string quoted = "\"" + abs.string() + "\"";
            for (size_t at = cmd.find("{path}"); at != std::string::npos;
                 at = cmd.find("{path}", at + quoted.size()))
                cmd.replace(at, 6, quoted);
            if (tool.find("{path}") == std::string::npos) cmd += " " + quoted;

            const std::string text = shell_capture(cmd);
            if (text.empty()) {
                std::cerr << "  [warn] read-flier: `" << cmd
                          << "` printed nothing. Run it yourself to see why - "
                             "this effect only reads its output.\n";
                return {};
            }

            /* The event this flier is wired to, if any — the thing its printed
             * date is checked against. Same relation names the renderer walks,
             * because a flier that the site can find and this cannot would be a
             * second idea of what "linked" means. */
            std::string ev_name, ev_iso;
            for (const auto& w : fdata.wires) {
                std::string other;
                if (w.from == rune) other = w.to;
                else if (w.to == rune) other = w.from;
                if (other.empty()) continue;
                const maiz::SceneNode* e = fdata.find(other);
                if (!e || e->glyph != "event") continue;
                ev_name = e->name;
                ev_iso = hormiga::field_value(*e, "date");
                if (!ev_iso.empty()) break; // a dated event wins over a recurring one
            }

            const auto prop = hormiga::flier::propose(
                rune, text, ev_name, ev_iso, hormiga::today_days());
            std::cerr << "  read " << abs.filename().string() << " through `"
                      << tool << "`\n";
            for (const auto& n : prop.notes) std::cerr << "  [note] " << n << "\n";
            std::cerr << "  proposed - NOTHING HAS BEEN CHANGED. Review, then "
                         "dispatch what you want:\n";
            for (const auto& c : prop.commands) std::cerr << "    " << c << "\n";
            if (prop.commands.empty())
                std::cerr << "    (no keywords worth proposing)\n";
            return std::to_string(prop.commands.size());
        }
        if (op == "sync-version" || op == "sync-merge" || op == "lan-peers" ||
            op == "lan-serve" || op == "lan-sync") {
            if (!g_core) return {};
            const std::vector<std::string> a = effect_args(args);
            HormigaApp app;
            wire(app);
            app.state_name = g_state_name;

            /* `apply` is a WORD, not a flag, and it may appear anywhere in the
             * arguments. A merge that ran because a dash was mistyped is not a
             * failure mode worth having. */
            bool apply = false;
            for (const auto& t : a)
                if (t == "apply" || t == "--apply") apply = true;

            const auto rep = app.sync_op(op, a, g_core->export_state(), apply);
            /* The report IS the output. stderr, so a `--json` caller's stdout
             * stays parseable -- the same split `render_from_state` uses. */
            for (const maiz::LogEntry& e : rep.lines)
                std::cerr << "  [" << e.level << "] sync: " << e.msg << "\n";
            if (!rep.merged_state.empty()) g_pending_state = rep.merged_state;
            if (rep.rc != 0) return {};
            return "\"" + (rep.value.empty() ? std::string("ok") : rep.value) + "\"";
        }
        if (op == "backup-database" || op == "restore-database") {
            /* ── THE ENCRYPTED BACKUP (okf/concepts/platform/data-planes.md §3) ───────
             *
             * A whole database as a blob the store cannot read, so that S3, a
             * USB stick or a shared drive are all acceptable places to keep the
             * organization's data. End-to-end encrypted by construction,
             * because the key never leaves this machine.
             *
             * THE PASSPHRASE IS NOT AN ARGUMENT. argv is readable in a process
             * listing by anything running as this user, which is the same
             * reason the deploy token goes through a curl config file rather
             * than a command line. It comes from the environment, and the
             * refusal below says so rather than failing obscurely. */
            const std::vector<std::string> a = effect_args(args);
            const char* env = std::getenv("HORMIGA_BACKUP_PASSPHRASE");
            const std::string pass = env ? env : std::string();
            if (pass.empty()) {
                std::cerr << "  [error] backup: set HORMIGA_BACKUP_PASSPHRASE "
                             "in the environment.\n"
                             "          It is deliberately not an argument: a "
                             "command line is readable\n"
                             "          by anything running as this user.\n";
                return {};
            }
            const bool restoring = op == "restore-database";
            const std::filesystem::path doc = g_base_dir / g_state_name;
            const std::filesystem::path bkp =
                a.size() > 1 ? std::filesystem::path(a[1])
                             : std::filesystem::path(doc.string() + ".bkp");
            const std::filesystem::path live =
                a.empty() ? doc : std::filesystem::path(a[0]);

            const auto r = restoring
                               ? hormiga::backup::open_file(bkp.string(),
                                                            live.string(), pass)
                               : hormiga::backup::seal_file(live.string(),
                                                            bkp.string(), pass);
            if (!r.ok) {
                std::cerr << "  [error] backup: " << r.error << "\n";
                return {};
            }
            if (restoring) {
                std::cerr << "  [info] backup: restored " << live.string()
                          << " (" << r.out << " bytes) from " << bkp.string()
                          << "\n";
                return "\"" + live.string() + "\"";
            }
            std::cerr << "  [info] backup: sealed " << r.in << " bytes to "
                      << bkp.string() << "\n"
                      << "  [info] backup: there is NO recovery for a forgotten "
                         "passphrase - that is what makes the store unable to "
                         "read this.\n";
            return "\"" + bkp.string() + "\"";
        }
        if (op == "pack-database") {
            /* THE PORTABLE BUNDLE, HEADLESS. A `.miga` v3 is one file holding
             * the state document plus every irreplaceable asset — the thing you
             * hand to somebody or keep on a drive. It could only be made from
             * the GUI's "save database as", so a database an AGENT operates
             * could not produce one at all. Closing that is what let the Cat
             * Colony demo folder exist (2026-09-01).
             *
             * NOT encrypted, and deliberately not `backup-database`: that seals
             * a file behind a passphrase for somewhere you do not trust. This is
             * the database itself, readable, for somewhere you do. */
            const std::vector<std::string> a = effect_args(args);
            /* `org_01.state.json` bundles as `org_01.miga`, not
             * `org_01.state.miga`: `.state` names the FORM of the working file,
             * and a bundle is not that form. Matches what the GUI's save-as has
             * always produced, so the two doors agree on the name. */
            std::string stem = std::filesystem::path(g_state_name).stem().string();
            if (stem.size() > 6 && stem.compare(stem.size() - 6, 6, ".state") == 0)
                stem.resize(stem.size() - 6);
            std::filesystem::path out = a.empty() ? std::filesystem::path(stem + ".miga")
                                                  : std::filesystem::path(a[0]);
            if (out.is_relative()) out = g_base_dir / out;
            if (out.extension() != ".miga") out += ".miga";
            /* Resolved through the SAME arithmetic `HormigaApp::data_dir` uses,
             * so a relocated `paths.assets` is the folder that gets bundled. */
            const auto r = hormiga::miga::pack(
                g_core->export_state(), g_base_dir, out, out.stem().string(),
                hormiga::resolve_data_dir(*g_core, g_base_dir, "assets"));
            if (!r.ok) {
                std::cerr << "  [error] pack: " << r.error << "\n";
                return {};
            }
            std::cerr << "  [info] pack: wrote " << out.string() << " ("
                      << r.bytes << " bytes, " << r.assets << " asset(s))\n";
            return "\"" + out.string() + "\"";
        }
        if (op == "check-store") {
            /* The smallest REAL operation against the bucket. The first thing an
             * operator with new credentials runs, and the last check before a
             * push is worth attempting. */
            if (!g_core) return {};
            const std::vector<std::string> a = effect_args(args);
            HormigaApp app;
            wire(app);
            maiz::ProjectOptions ao;
            ao.mantle = kAntfarmMantle;
            const int rc = app.check_store(maiz::project_scene(*g_core, ao),
                                           a.empty() ? std::string() : a[0]);
            return rc == 0 ? "\"ok\"" : std::string();
        }
        if (op == "push-store") {
            /* Send a small artifact to an object store. Two are publishable and
             * the set is closed: `index` (the cleared projection) and `backup`
             * (the encrypted blob). See src/publish/push.cpp for why this is not
             * a path the caller supplies. */
            if (!g_core) return {};
            const std::vector<std::string> a = effect_args(args);
            const std::string what = a.empty() ? std::string("index") : a[0];
            const std::string node = a.size() > 1 ? a[1] : std::string();
            HormigaApp app;
            wire(app);
            app.state_name = g_state_name;
            maiz::ProjectOptions ao;
            ao.mantle = kAntfarmMantle;
            const int n = app.push_to_store(maiz::project_scene(*g_core, ao),
                                            node, what);
            if (n < 0) return {};
            return std::to_string(n);
        }
        if (op == "publish-index") {
            /* The published subset as DATA. Reached through the SAME seam as
             * the renderers (`render_from_state`), which is what guarantees it
             * sees the same derived caches — `refresh_allo_rules()` in
             * particular, without which `web-hide` silently publishes runes a
             * rule says to withhold. */
            const std::vector<std::string> a = effect_args(args);
            const std::string doc = a.empty() ? std::string() : a[0];
            const std::string path = render_through_app("publish-index", "en", doc);
            if (path.empty()) return {};
            std::cerr << "wrote " << path << "\n";
            return "\"" + path + "\"";
        }
        if (op == "render" || op == "render-site") {
            const std::vector<std::string> a = effect_args(args);
            /* `<lang>` IS OPTIONAL NOW, SO THE DOCUMENT CAN BE FIRST.
             *
             * The signature was `render-site <lang> [<document>]` and the
             * document was read from slot 1 unconditionally. The moment the
             * language became optional, the natural thing to type —
             * `effect render-site org-website` — put the document in slot 0,
             * where nothing looked for it, and the effect rendered an EMPTY
             * mantle and reported success. Caught by running exactly that
             * command against a fixture and getting a 1.4 KB page with no
             * content in it.
             *
             * A language is one of a known, short list, so the argument can be
             * identified by what it IS rather than by where it sits. Both forms
             * work and neither can silently mean the other. */
            const bool named_lang =
                !a.empty() && std::find(hormiga::site_langs().begin(),
                                        hormiga::site_langs().end(),
                                        a[0]) != hormiga::site_langs().end();
            const std::string doc = named_lang ? (a.size() > 1 ? a[1] : std::string())
                                               : (a.empty() ? std::string() : a[0]);

            /* ── NO LANGUAGE MEANS EVERY LANGUAGE (2026-08-20) ────────────────
             *
             * This defaulted to `en`, and that default is what shipped a stale
             * Spanish page to a live community website. The operator's console:
             *
             *     set … summary_en '…'      set … summary_es '…'
             *     effect render-site        ← rebuilt English only
             *     effect deploy-site        ← uploaded both, one of them old
             *
             * Nothing warned, because from `render-site`'s point of view
             * nothing was wrong: it was asked for a language it was not given
             * and picked the one the developer speaks.
             *
             * A default that silently does HALF of a bilingual job is worse
             * than an error, so the bare verb now does the whole job. Naming a
             * language explicitly still renders just that one — that is the
             * preview loop, and it is fast on purpose. */
            if (op == "render-site" && !named_lang) {
                std::string first;
                for (const std::string& lg : hormiga::site_langs()) {
                    const std::string p = render_through_app(op, lg, doc);
                    if (p.empty()) return {};   // a half-built site is not a site
                    std::cerr << "wrote " << p << "\n";
                    if (first.empty()) first = p;
                }
                return "\"" + first + "\"";
            }
            const std::string lang = named_lang ? a[0] : std::string("en");
            std::string path = render_through_app(op, lang, doc);
            if (path.empty()) return {};
            std::cerr << "wrote " << path << "\n";
            return "\"" + path + "\"";
        }
        return {};   // everything else is a GUI affordance; say nothing
    };

    /* THE ONE-WAY DOORS. `consequence` is the field a refusal quotes back, so
     * it is written for the person deciding whether to hand an agent
     * `--allow-effects`, not for a changelog. */
    h.effect_ops = {
        {"render", "Build the newsletter as an HTML file under exports/.",
         true, "writes a file beside the database; nothing is sent to anyone"},
        {"render-site", "Build the public website into site/.",
         true, "writes the site folder — it is not published until you deploy it"},
        {"pack-database",
         "Pack the database and its assets into one portable .miga file "
         "(default: <database>.miga beside the document).",
         true,
         "writes one file beside the database; it is NOT encrypted, so it "
         "carries the organization's data in readable form - keep it where you "
         "would keep the database itself"},
        {"backup-database",
         "Seal the database into an encrypted backup only this passphrase can "
         "open (HORMIGA_BACKUP_PASSPHRASE). Safe to keep anywhere - the store "
         "cannot read it.",
         true,
         "writes an encrypted copy beside the database; nothing is sent "
         "anywhere, and a forgotten passphrase cannot be recovered"},
        {"restore-database",
         "Open an encrypted backup back into the database file, replacing it.",
         false,
         "OVERWRITES THE DATABASE with the backup's contents. What is in the "
         "file now is gone, and nothing in this application can bring it back"},
        {"check-store",
         "Can these credentials reach this bucket? Performs the smallest real "
         "operation against it (lists one key), so a pass predicts a push.",
         true,
         "reads one key listing from the bucket; writes nothing anywhere"},
        {"push-store",
         "Send a published artifact to an object store (S3 or compatible). "
         "Args: <index|backup> [<store-node>]. `index` refreshes the live "
         "directory without redeploying the site; `backup` sends the encrypted "
         "blob.",
         true,
         "uploads to a bucket you control - the index has already passed the "
         "clearance seam, and the backup is encrypted with a passphrase that "
         "never leaves this machine"},
        {"publish-index",
         "Build the published directory as data (site/index/directory.json) — "
         "the same clearance-gated subset the website shows, in a form a live "
         "site can read without being redeployed.",
         true,
         "writes a file beside the database; only runes tagged "
         "`clearance:public` are included and it is not published until you "
         "deploy it"},
        {"sync-version",
         "Say this database's version name (a Void Palabra cut name). Two "
         "devices that print the same string hold the same data and need no "
         "sync at all.",
         true, "reads the state document; changes nothing anywhere"},
        {"sync-merge",
         "Merge another state document or .miga into this database. "
         "Args: <path> [apply]. Without `apply` it REPORTS what would change "
         "and writes nothing.",
         true,
         "without `apply`, nothing is written. WITH `apply` it replaces the "
         "database with the merged result - conflicts keep one value and the "
         "other is reported, and runes you deleted may come back (deletions do "
         "not yet propagate)"},
        {"lan-peers",
         "Announce on the local network and list the other Hormiga devices "
         "that answer. Args: [seconds]. Read-only.",
         true,
         "sends a small announcement on the local network only (never routed "
         "off this subnet) and opens no connection"},
        {"lan-serve",
         "Wait for another device on this network to connect, then exchange "
         "and merge. Args: [seconds] [apply]. Compare the six-character code "
         "with the other screen before trusting it.",
         false,
         "accepts one encrypted connection from this network and sends this "
         "database's contents to whoever completes the handshake - compare the "
         "code on both screens first. Without `apply` the merge is only reported"},
        {"lan-sync",
         "Connect to another device on this network, exchange and merge. "
         "Args: <host> [port] [apply]. Compare the six-character code with the "
         "other screen before trusting it.",
         false,
         "sends this database's contents to that address over an encrypted "
         "channel - compare the code on both screens first. Without `apply` the "
         "merge is only reported"},
        {"read-flier",
         "Propose `kw:` tags and check the printed date for one image rune, "
         "using the recognizer named in `config tools.image_text`. Args: "
         "<image-rune>. Proposes only - it dispatches nothing.",
         true,
         "runs the command you configured against one image file and prints "
         "what it would suggest; nothing is changed and nothing is sent "
         "anywhere this build controls"},
        {"query",
         "List the runes a block query selects, RIGHT NOW - the same evaluation "
         "the renderer and the Builder preview use, so it understands `date:"
         "past`, `date:today`, `date:future`, `date:recurring` and "
         "`date:undated`, which `ls --tag` cannot. Args: <expression>.",
         true, "reads the data mantle and prints what matched; changes nothing"},
        {"save", "Mirror the state document into the SQLite database.",
         true, "overwrites the SQLite mirror beside the document"},
        {"deploy-site",
         "Upload the built site/ folder through an Antfarm hol_static_host node. "
         "Records the publish as a `deployment` rune in the antfarm mantle.",
         false,
         "PUBLISHES THE WEBSITE. Whatever is in site/ becomes the live page "
         "everyone sees, immediately, and nothing in this application can take "
         "it back"},
        {"rollback-site",
         "Ask the host to put a past deployment back in front of visitors. "
         "Args: <host-node> <deployment-id>. Needs `rollback_cmd` on the node.",
         false,
         "CHANGES THE LIVE WEBSITE. It puts back something that was published "
         "before, immediately, for everyone. It does not touch your database - "
         "only what the host is serving"},
    };
    return h;
}

} // namespace

/* ── the headless render seam (app.hpp) ──────────────────────────────────────
 *
 * Defined in the HEADLESS unit rather than here, because the boot sequence
 * needs `seed.hpp`'s glyph registration and `seed.hpp` is deliberately off
 * `section_web.cpp`'s include path — it drags in the whole of nlohmann/json,
 * which is what put the link over PE's 16-bit section ceiling in the first
 * place (see app_internal.hpp). Declared in app.hpp; the GUI never calls it.
 *
 * The sequence is the GUI's own boot, minus everything that draws: replay,
 * register, project, derive.
 *
 * `refresh_allo_rules()` is not optional and is the easiest line to omit. The
 * render seam reads the derived caches, and `web-hide "1"` — the one Allomone
 * effect that reaches the Output domain — drops a rune from every query-backed
 * block. Skipping the derivation would publish runes a rule says to withhold,
 * silently, which is precisely the failure CLAUDE.md rule 6 exists to prevent. */
std::string HormigaApp::render_from_state(const std::string& state_json,
                                          std::string_view op,
                                          std::string_view lang,
                                          std::string_view document) {
    core = maiz::Core(state_json);
    hormiga::register_glyphs(core);
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
    scene = maiz::project_scene(core);
    refresh_allo_rules();
    /* `cur_doc` is Builder VIEW state, absent from the state document, so a
     * headless render has nothing to inherit. A NAMED document still wins.
     *
     * AN UNNAMED ONE NOW FALLS TO THE ACTIVE MANTLE, not to the `"issue-demo"`
     * this member is initialised with. That initialiser is a GUI convenience
     * and headless was inheriting it as a guess: `effect render-site` with no
     * document rendered whatever mantle happened to be called `issue-demo` —
     * for a real database, a NEWSLETTER — wrote it as the site's index.html,
     * shrank the sitemap to one page, and returned ok. The field agent lost a
     * render to it on 2026-09-01 and reported it as a build regression, which
     * is the tell: silent, wrong, and indistinguishable from success.
     *
     * Empty is not a fallback here, it is the answer: `ProjectOptions::mantle`
     * empty means "the active one", so `use org-website` followed by a bare
     * `effect render-site` now does what anyone would read it as doing. */
    cur_doc = std::string(document);
    /* Three output domains, one boot. `publish-index` joins the two renderers
     * here rather than getting its own entry point, because everything above
     * this line — replay, register, project, and especially
     * `refresh_allo_rules()` — is exactly what it needs and is exactly what is
     * easy to forget when writing a second one. */
    std::string path = op == "publish-index" ? publish_index()
                       : op == "render-site" ? render_site(lang)
                                             : render_preview(lang);

    /* THE RENDER'S OWN REPORT REACHES THE TERMINAL (2026-08-19).
     *
     * `render_preview` says useful things — the word count and read time, and
     * "N image(s) have no public URL so they will not load in email" — through
     * the app's log strip and a GUI toast. Headless, nobody was reading either,
     * so an agent shipped a real newsletter full of `unpublished` placeholder
     * boxes and only found out when a person opened it.
     *
     * stderr, not stdout, so a `--json` caller's output stays parseable. */
    for (const maiz::LogEntry& e : log)
        if (e.level == "warn" || e.level == "error" || e.op == "render")
            std::cerr << "  [" << e.level << "] " << e.op << ": " << e.msg << "\n";
    return path;
}


/* THE STATE FILE MUST BE THE ONE THE GUI OPENS, and this is the whole of the
 * "when I open the app it should be updated" story.
 *
 * Void Maiz defaults to `<app.id>.state.json`, so a bare run would write
 * `hormiga.state.json` while the GUI reads `demo-org.json` — and it would fail
 * by showing STALE DATA rather than by erroring, which is the worst shape a bug
 * can have. `run_cli` takes no options struct, so the default is injected here,
 * before it parses. An explicit `--state` still wins. */
int main(int argc, char** argv) {
    g_base_dir = std::filesystem::current_path();
    std::error_code ec;
    if (argc > 0)
        g_ship_dir = std::filesystem::absolute(argv[0], ec).parent_path();

    std::vector<char*> args(argv, argv + argc);
    std::string given_state;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string_view(argv[i]) == "--state") given_state = argv[i + 1];

    /* ── `--restore-backup`, AND WHY IT IS A FLAG RATHER THAN AN EFFECT ──────
     *
     * There is a `restore-database` effect, and it is unusable in the one
     * situation restoring exists for. Found by testing the actual disaster
     * rather than the happy path: corrupt the database, try to restore it, and
     * the session refuses to start —
     *
     *     error: the state document at … is not a JSON object. Refusing to
     *     start rather than replacing it with an empty one.
     *
     * That refusal is CORRECT and should stay. But every effect runs inside a
     * session, and a session needs a loadable document, so the recovery tool
     * was gated behind the thing being broken. **A recovery tool that requires
     * a working system is not a recovery tool.**
     *
     * So this runs in `main`, before any session exists, and touches nothing
     * but two files. The effect stays for the ordinary case — deliberately
     * rolling back a database that is fine — and this is for the day the file
     * is a pile of bytes. */
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string_view(argv[i]) != "--restore-backup") continue;
        const char* env = std::getenv("HORMIGA_BACKUP_PASSPHRASE");
        if (!env || !*env) {
            std::cerr << "set HORMIGA_BACKUP_PASSPHRASE in the environment.\n"
                         "It is deliberately not an argument: a command line is "
                         "readable by anything running as this user.\n";
            return 2;
        }
        const std::filesystem::path bkp = argv[i + 1];
        const std::filesystem::path doc =
            given_state.empty() ? (g_base_dir / "demo-org.json")
                                : std::filesystem::path(given_state);
        const auto r =
            hormiga::backup::open_file(bkp.string(), doc.string(), env);
        if (!r.ok) {
            std::cerr << "restore failed: " << r.error << "\n";
            return 1;
        }
        std::cerr << "restored " << doc.string() << " (" << r.out
                  << " bytes) from " << bkp.string() << "\n";
        return 0;
    }

    static std::string flag = "--state";
    static std::string path = (g_base_dir / "demo-org.json").string();
    if (given_state.empty()) {
        args.insert(args.begin() + 1, flag.data());
        args.insert(args.begin() + 2, path.data());
    }

    const std::filesystem::path state = given_state.empty() ? path : given_state;
    /* ── THE DATA FOLDER IS THE DOCUMENT'S FOLDER, NOT THE PROCESS'S ─────────
     *
     * `g_base_dir` was the working directory outright, so `--state
     * ../org-data/org.state.json` read that document and then wrote `site/`,
     * `assets/`, `exports/` and the SQLite mirror beside the CALLER instead of
     * beside the database. Same rule the GUI now follows and the same rule a
     * relative `token_file` follows: everything an organization owns hangs off
     * the folder its document lives in.
     *
     * Only when `--state` was given: a bare run already has them equal, and
     * this must not change what a plain `voidhormiga-cli` in an org folder
     * does. */
    if (!given_state.empty()) {
        std::error_code bec;
        const std::filesystem::path abs = std::filesystem::absolute(state, bec);
        if (!bec && !abs.parent_path().empty()) g_base_dir = abs.parent_path();
    }
    g_state_name = state.filename().string();
    if (is_miga_bundle(state)) {
        std::cerr << state.string()
                  << " is a .miga bundle, not a state document.\n"
                     "Opening it here would merge an empty document into the "
                     "envelope and shadow the real data.\n"
                     "Unpack it first and point --state at the unpacked "
                     "<name>.state.json.\n";
        return 2;   // a usage error, which is what it is
    }
    /* A typo'd --state is otherwise indistinguishable from a first run: the
     * session creates an empty document and reports success. We cannot refuse
     * (a real first run has to start somewhere) so we say so, loudly, on
     * stderr — where it does not corrupt a --json caller's stdout. */
    std::error_code sec;
    if (!std::filesystem::exists(state, sec)) {
        if (!given_state.empty())
            std::cerr << "note: " << state.string()
                      << " does not exist; starting an EMPTY document.\n"
                         "      If you meant an existing database, check the path.\n";
        else
            /* THE BARE RUN WAS THE SILENT ONE, and it was the dangerous half:
             * `--state` at least names a path somebody typed. With no argument
             * we invent `demo-org.json` in whatever folder the process happens
             * to be in — which is how an organization's real database ended up
             * with a second, empty twin beside a source tree, edited for two
             * days before anyone noticed (see `state_name` in app.hpp).
             *
             * Not an error: a genuine first run has to start somewhere, and
             * refusing would break every "cd somewhere new and begin" flow.
             * But it says what it is doing and names the way out, because a
             * warning with no alternative is no better than silence. */
            std::cerr << "note: no database in this folder - creating an EMPTY "
                      << g_state_name << " in\n        "
                      << g_base_dir.string()
                      << "\n      To open an existing one instead, name it:  "
                         "--state <path-to>.state.json\n";
    }

    const int rc = maiz::run_cli(build_app(), (int)args.size(), args.data());

    /* ── THE MERGED DOCUMENT LANDS HERE ──────────────────────────────────────
     *
     * The session has closed, written its own state and released the advisory
     * lock, so this is the last write and it is uncontended. See
     * `g_pending_state` for why it cannot happen where it was computed.
     *
     * Write-temp-then-rename, the same discipline the vault and the bundle use:
     * a crash between `open` and `write` must never leave the organization with
     * a half-written database. */
    if (!g_pending_state.empty()) {
        const std::filesystem::path dst = state;
        const std::filesystem::path tmp = dst.string() + ".merging";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out << g_pending_state;
            if (!out) {
                std::cerr << "  [error] sync: could not write " << tmp.string()
                          << " - the merge was NOT applied\n";
                return rc == 0 ? 1 : rc;
            }
        }
        std::error_code rec;
        std::filesystem::rename(tmp, dst, rec);
        if (rec) {
            std::cerr << "  [error] sync: could not replace " << dst.string() << " ("
                      << rec.message() << ") - the merge was NOT applied; the result "
                      << "is in " << tmp.string() << "\n";
            return rc == 0 ? 1 : rc;
        }
        std::cerr << "  [info] sync: wrote the merged database to " << dst.string() << "\n";
    }
    return rc;
}
