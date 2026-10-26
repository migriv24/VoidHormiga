/* app/database.cpp — the database's life on this device: new, open, save,
 * save as, and which one opens at start (Settings > Starting Hormiga).
 *
 * Moved out of app.cpp on 2026-09-25, when the start-up rule and leaving a
 * shared database on New/Open joined it: one subject, one file. */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"          // leave_database: New and Open leave the shared one
#include "platform/app_settings.hpp"  // the default database
#include "domain/seed.hpp"             // register_glyphs, seed_antfarm_transcript
#include "domain/workspaces.hpp"       // what a new database can start as

#include <chrono>
#include <ctime>

namespace fs = std::filesystem;

// ── the .miga v3 database bundle: Save / Save As / Open (miga-format.md) ─────

/* Clear the working copy's local files so two databases never bleed together
 * (author 2026-07-24: "different databases... have their own local storage").
 * Always drops the re-derivable caches (they rebuild from protocols); on a
 * database SWITCH also drops assets/ (each bundle carries its own). */
void HormigaApp::reset_working_copy(bool clear_assets) {
    std::error_code ec;
    tex_cache.clear(); // freed image paths must not return stale textures
    fs::remove_all(data_dir("tiles"), ec);
    fs::remove_all(data_dir("site"), ec);
    fs::remove_all(data_dir("exports"), ec);
    for (auto& e : fs::directory_iterator(base_dir, ec)) {
        std::string fn = e.path().filename().string();
        if (fn.rfind("preview-", 0) == 0 && e.path().extension() == ".html")
            fs::remove(e.path(), ec);
    }
    if (clear_assets) fs::remove_all(data_dir("assets"), ec);
}


/* Pack the working copy into the current bundle (backing up the old one), or
 * fall through to Save As when there's no bundle yet. Save ≈ commit. */
/* Back up the existing bundle before overwriting it (author #5: backups), and
 * keep the newest ten. Pure filesystem, so a background save runs it on its
 * worker: copying a 200 MB bundle is not the window's business. */
static void back_up_bundle(const fs::path& bundle, const fs::path& backups) {
    std::error_code ec;
    if (!fs::exists(bundle, ec)) return;
    fs::create_directories(backups, ec);
    char stamp[32];
    std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&t));
    const std::string stem = bundle.stem().string();
    fs::copy_file(bundle, backups / (stem + "-" + stamp + ".miga"), fs::copy_options::overwrite_existing, ec);
    std::vector<fs::path> mine;
    for (auto& e : fs::directory_iterator(backups, ec))
        if (e.path().filename().string().rfind(stem + "-", 0) == 0) mine.push_back(e.path());
    std::sort(mine.begin(), mine.end());
    for (size_t i = 0; i + 10 < mine.size(); ++i) fs::remove(mine[i], ec);
}

static std::string saved_line(const fs::path& out, const hormiga::miga::PackResult& r) {
    return "saved database " + out.filename().string() + " (" + std::to_string(r.assets) +
           " assets, " + std::to_string(r.bytes / 1024) + " KB)";
}

void HormigaApp::save_database(bool background) {
    if (cur_miga.empty()) { // no bundle yet → behave as Save As (author #1)
        if (on_save_file) {
            std::string p = on_save_file("my-database.miga");
            if (!p.empty()) save_database_as(p, background);
        } else {
            toast("no save dialog available", true);
        }
        return;
    }
    do_save(); // flush the live state into the working .db first
    /* The state is exported HERE, on the main thread, before anything moves:
     * the worker packs a snapshot and never touches the live core. */
    const std::string state = core.export_state();
    const auto extra = referenced_files(state);
    const fs::path out = cur_miga, base = base_dir, adir = assets_dir(), backups = data_dir("backups");
    if (!background) {
        back_up_bundle(out, backups);
        auto r = hormiga::miga::pack(state, base, out, out.stem().string(), adir, extra);
        if (r.ok) toast(saved_line(out, r));
        else toast("save database failed: " + r.error, true);
        return;
    }
    run_busy_async("Saving database…", [this, state, extra, out, base, adir, backups](const hormiga::BusyReport& report)
                                           -> hormiga::BusyFinish {
        report(0.0, "Keeping a backup");
        back_up_bundle(out, backups);
        auto r = std::make_shared<hormiga::miga::PackResult>(hormiga::miga::pack(
            state, base, out, out.stem().string(), adir, extra, {},
            [&](double f, const char* phase) { report(0.05 + 0.95 * f, phase); }));
        return [this, r, out] {
            if (r->ok) toast(saved_line(out, *r));
            else toast("save database failed: " + r->error, true);
        };
    });
}

/* Save the working copy as a NEW .miga at a user-CHOSEN path (author
 * 2026-07-24: "choose where we are saving"). The path comes from the OS save
 * dialog; .miga is ensured. This bundle becomes the current database. */
void HormigaApp::save_database_as(const std::string& path, bool background) {
    if (path.empty()) return;
    fs::path out = path;
    if (out.extension() != ".miga") out += ".miga";
    /* SAVE AS MAKES A NEW DATABASE (the author, 2026-10-18: "'save as new'
     * effectively creates a new database. it's separate from whatever was being
     * used before"). It leaves the old one's room and takes an identity of its
     * own, before it is packed, so the new file never carries the old id. The
     * first save of a database that had no file yet is naming it, not copying. */
    std::error_code sec;
    const bool copying = !cur_miga.empty() && !(fs::exists(out, sec) && fs::equivalent(out, cur_miga, sec));
    const std::string was = copying ? LanRuntime::fork_identity(*this) : std::string();
    do_save(); // flush live state into the working .db first
    const std::string state = core.export_state();
    const auto extra = referenced_files(state);
    // what happens once the file exists (or does not), on the main thread either way
    const auto landed = [this, out, was](const hormiga::miga::PackResult& r) {
        // no new file, no new database: the open one is still the original, with its identity
        if (!r.ok && !was.empty()) core.dispatch("config set sync.database " + was);
        if (r.ok) {
            cur_miga = out.string();
            remember_bundle(cur_miga);
            LanRuntime::claim_home(*this);
            toast("saved database to " + out.string() + " (" + std::to_string(r.assets) +
                  " assets, " + std::to_string(r.bytes / 1024) + " KB)");
        } else {
            toast("save database failed: " + r.error, true);
        }
    };
    if (!background) {
        landed(hormiga::miga::pack(state, base_dir, out, out.stem().string(), assets_dir(), extra));
        return;
    }
    const fs::path base = base_dir, adir = assets_dir();
    run_busy_async("Saving database…", [state, extra, out, base, adir, landed](const hormiga::BusyReport& report)
                                           -> hormiga::BusyFinish {
        auto r = std::make_shared<hormiga::miga::PackResult>(
            hormiga::miga::pack(state, base, out, out.stem().string(), adir, extra, {}, report));
        return [r, landed] { landed(*r); };
    });
}

/* Start a COMPLETELY NEW, empty database (author 2026-07-24): a fresh working
 * copy — empty data, one blank newsletter document, the Antfarm topology (the
 * protocol layer) — and NO bundle yet (Save database as... names & places its
 * .miga). This is the multi-database "new project" entry the app was missing. */
void HormigaApp::new_database(const std::string& workspace) {
    LanRuntime::leave_database(*this); // out of the shared one: a new database is nobody's yet
    reset_working_copy(true); // a fresh db owns nothing from the old working copy
    core = maiz::Core(); // a fresh, empty state (no demo data)
    install_host();
    channel_fields.clear();
    hormiga::register_glyphs(core);
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
    farm::register_glyphs(core);
    hormiga::chambers::register_glyphs(core);
    core.dispatch("config set actor human:hormiga");
    // minimal structure so every section has a home to open into
    core.dispatch(std::string("mantle new ") + kDataMantle);   // empty org data
    core.dispatch("mantle new issue-demo");                    // one blank document
    core.dispatch(std::string("mantle new ") + kAlloMantle);   // Allomone rules
    core.dispatch("rune new hero masthead");
    core.dispatch("set masthead title_en \"New Newsletter\"");
    core.dispatch("set masthead row \"0\"");
    for (const auto& c : hormiga::seed_antfarm_transcript()) core.dispatch(c);
    core.dispatch(std::string("use ") + kDataMantle);
    // the WORKSPACE it starts as (okf/concepts/platform/workspaces.md): its
    // canvases, regions, categories and examples, as dispatcher commands
    std::string opens_on; // a workspace may open on a canvas of its own (a store's floor)
    const auto ws = hormiga::workspaces::transcript(workspace);
    if (!ws.kinds.empty()) { // its KINDS first, in their own mantle, then registered (domain/kinds.hpp)
        core.dispatch(std::string("mantle new ") + hormiga::kinds::kMantle);
        for (const auto& c : ws.kinds) core.dispatch(c);
        core.dispatch(std::string("use ") + kDataMantle);
        hormiga::kinds::apply(core, {});
    }
    for (const auto& c : ws.data) {
        const auto r = core.dispatch(c);
        if (!r.ok) log.push_back({"warn", "workspace", "refused while setting up: " + c});
        if (c.rfind("config set view.map.canvas ", 0) == 0) opens_on = c.substr(27);
    }
    while (!opens_on.empty() && (opens_on.back() == '"' || opens_on.back() == ' ')) opens_on.pop_back();
    while (!opens_on.empty() && opens_on.front() == '"') opens_on.erase(0, 1);
    map_canvas.clear();
    map_sel.clear();
    reproject();
    read_view_config();
    apply_theme();
    if (const auto cv = hormiga::canvas::find(scene, opens_on); cv.plan()) { // centred on its floor
        map_canvas = cv.name;
        map_cam.x = (float)(cv.w / 2);
        map_cam.y = (float)(cv.h / 2);
        map_cam.zoom = (float)cv.fit_zoom(); // the whole of it in view
        map_cam_loaded = true;
    }
    cur_miga.clear();      // unsaved: a new database has no file until Save As
    fv2.convert = true;    // a new database gets the Antfarm's default colony (convert_antfarm)
    fv2.fresh = true;
    remember_bundle("");
    cur_doc = "issue-demo";
    cur_page.clear();
    ed.selection.clear();
    do_save();             // flush the fresh structure into the working .db
    if (!phone) // a phone names it first
        toast(workspace.empty() || workspace == "organization"
                  ? "new database (empty) - use 'Save database as...' to name & place it"
                  : "new database, set up as " + std::string(hormiga::workspaces::find(workspace)
                                                                 ? hormiga::workspaces::find(workspace)->title
                                                                 : workspace) +
                        " - use 'Save database as...' to name & place it");
}

/* SETTINGS > STARTING HORMIGA (the author, 2026-09-25): "default database ...
 * as a default will be 'empty database' but a user could assign a specific
 * database as the one that pops up when they open the application". Called by a
 * shell that was NOT told which database to open, after init().
 *
 * Three rules keep it from ever losing work:
 *  - the first start with Settings keeps the database this person already uses
 *    as the default, so updating does not take their organization away;
 *  - before the working copy is replaced, it is packed into backups/ (an
 *    unsaved session is not dropped on the floor);
 *  - a phone (reopen_last) has no File menu to reopen things from, so it keeps
 *    the database it had, and only a fresh install starts empty.
 *
 * Also restores `cur_miga`, which nothing did at startup: after a restart the
 * app said "unsaved working copy" for a database that was saved, because only
 * the working copy's note remembered the bundle. */
void HormigaApp::open_default_database(bool reopen_last, bool first_run) {
    std::error_code ec;
    key_dirs(); // reads the working copy's note: the bundle it belongs to
    if (cur_miga.empty() && !bundle_file.empty() && fs::exists(bundle_file, ec)) cur_miga = bundle_file.string();

    if (reopen_last) {
        if (first_run && cur_miga.empty()) new_database(); // a new phone: empty, not the demo
        return;
    }
    hormiga::app_settings::Settings as = hormiga::app_settings::load();
    if (!as.migrated) {
        as.migrated = true;
        if (as.default_database.empty() && !cur_miga.empty() && !first_run) as.default_database = cur_miga;
        hormiga::app_settings::save(as);
    }
    const std::string want = as.default_database;
    if (!want.empty() && want == cur_miga) return; // the working copy already is it
    if (!want.empty() && !fs::exists(want, ec)) toast("the default database is missing: " + want, true);

    // about to replace the working copy: keep it first, unless this is a first run's demo
    if (!first_run) {
        fs::create_directories(data_dir("backups"), ec);
        char stamp[32];
        const std::time_t t = std::time(nullptr);
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&t));
        const std::string stem = cur_miga.empty() ? std::string("unsaved") : fs::path(cur_miga).stem().string();
        const fs::path keep = data_dir("backups") / (stem + "-before-start-" + stamp + ".miga");
        do_save();
        auto r = hormiga::miga::pack(core.export_state(), base_dir, keep, stem, assets_dir(),
                                     referenced_files(core.export_state()));
        if (r.ok) log.push_back({"info", "start", "kept the previous working copy in " + keep.string()});
    }
    if (!want.empty() && fs::exists(want, ec)) open_database(want);
    else new_database();
}

/* Replace the running core with a freshly-loaded state, re-establishing the
 * host seams and glyphs (the same setup init() does after a state load), and
 * persist it into the working .db so the app runs off local storage. */
/* The half of loading a database that needs NOTHING of the app: parse the state
 * into a core of its own and register the application's glyphs on it. Pure, so
 * an open runs it on its worker (issue #1); measured at 0.72 s for 10^5 runes,
 * which is 0.72 s the window now spends drawing instead of frozen. */
static maiz::Core prepare_core(const std::string& state) {
    maiz::Core c(state);
    hormiga::register_glyphs(c);
    hormiga::register_block_glyphs(c);
    hormiga::register_antfarm_glyphs(c);
    farm::register_glyphs(c);
    hormiga::chambers::register_glyphs(c);
    c.dispatch("config set actor human:hormiga");
    return c;
}

void HormigaApp::reload_from_state(const std::string& state) { adopt_core(prepare_core(state)); }

/* The main thread's half: the prepared core becomes the database, with this
 * app's host seams on it, and everything that reads it is refreshed. */
void HormigaApp::adopt_core(maiz::Core&& fresh) {
    core = std::move(fresh);
    install_host();
    channel_fields.clear();
    reproject();
    read_view_config();
    apply_theme();
    do_save(); // write the opened database into the working .db
    cur_page.clear();
    cur_doc = "issue-demo";
    ed.selection.clear();
    fv2.convert = true; // its Antfarm, at the next frame's start (convert_antfarm)
}

/* ── OPENING A DATABASE, IN TWO HALVES (2026-10-24, issue #1) ─────────────────
 *
 * Isolation (databases never bleed, author 2026-07-24) used to be "delete the
 * assets folder, then write every picture in the bundle": correct, and a full
 * rewrite on every open. It is now a MIRROR (miga::OpenOptions::mirror_assets):
 * the folder ends up exactly the bundle's, but a picture already there with the
 * same bytes is left alone. The caches still go, as before. */
static double ms_since(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
}

void HormigaApp::finish_open(const std::string& path, const hormiga::miga::OpenResult& r,
                             maiz::Core&& fresh, double bundle_ms, double core_ms) {
    if (!r.ok) {
        toast(r.version == 2 ? "that's a legacy secrets-only .miga (v2), not a "
                               "full database"
                             : "open failed: " + r.error,
              true);
        return;
    }
    const auto t = std::chrono::steady_clock::now();
    adopt_core(std::move(fresh));
    cur_miga = path;
    remember_bundle(path);
    LanRuntime::claim_home(*this); // a copy of a database this device knows is a database of its own
    load_secrets(); // an imgbb.key beside THIS bundle
    /* What the open cost, phase by phase, in the log: issue #2 promised
     * measurements, and this is where anybody's come from. */
    char line[200];
    std::snprintf(line, sizeof line, "opened in %.0f ms: bundle %.0f, database %.0f, view and working copy %.0f",
                  bundle_ms + core_ms + ms_since(t), bundle_ms, core_ms, ms_since(t));
    log.push_back({"info", "open", line});
    toast("opened database " + fs::path(path).filename().string() + " (" + std::to_string(r.assets) +
          " pictures restored" + (r.assets_kept ? ", " + std::to_string(r.assets_kept) + " already here" : "") + ")");
}

void HormigaApp::open_database(const std::string& path) {
    LanRuntime::leave_database(*this); // the one open now stops being this device's shared one
    reset_working_copy(false);         // the caches; the assets folder is mirrored by the open
    const auto t0 = std::chrono::steady_clock::now();
    hormiga::miga::OpenOptions o;
    o.mirror_assets = true;
    auto r = hormiga::miga::open(path, base_dir, assets_dir(), o);
    const double bundle_ms = ms_since(t0);
    const auto t1 = std::chrono::steady_clock::now();
    maiz::Core fresh = r.ok ? prepare_core(r.state) : maiz::Core();
    finish_open(path, r, std::move(fresh), bundle_ms, ms_since(t1));
}

void HormigaApp::run_busy_async(const std::string& label, hormiga::BusyWork work) {
    busy.label = label;
    busy.start(std::move(work), ImGui::GetTime());
}

void HormigaApp::begin_open_database(const std::string& path, std::function<void()> then) {
    LanRuntime::leave_database(*this);
    reset_working_copy(false);
    const fs::path base = base_dir, adir = assets_dir();
    run_busy_async("Opening database…", [this, path, base, adir, then](const hormiga::BusyReport& report)
                                            -> hormiga::BusyFinish {
        const auto t0 = std::chrono::steady_clock::now();
        hormiga::miga::OpenOptions o;
        o.mirror_assets = true;
        // the bundle is most of the bytes; the database's own load is weighed after it
        o.progress = [&](double f, const char* phase) { report(0.8 * f, phase); };
        auto r = std::make_shared<hormiga::miga::OpenResult>(hormiga::miga::open(path, base, adir, o));
        const double bundle_ms = ms_since(t0);
        const auto t1 = std::chrono::steady_clock::now();
        std::shared_ptr<maiz::Core> fresh;
        if (r->ok) {
            report(0.8, "Loading the database");
            fresh = std::make_shared<maiz::Core>(prepare_core(r->state));
            r->state = std::string(); // forty megabytes nobody needs any more
        }
        const double core_ms = ms_since(t1);
        report(0.97, "Drawing it");
        return [this, path, r, fresh, bundle_ms, core_ms, then] {
            finish_open(path, *r, fresh ? std::move(*fresh) : maiz::Core(), bundle_ms, core_ms);
            if (r->ok && then) then();
        };
    });
}

