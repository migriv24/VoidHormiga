/* app/database.cpp — the database's life on this device: new, open, save,
 * save as, and which one opens at start (Settings > Starting Hormiga).
 *
 * Moved out of app.cpp on 2026-09-25, when the start-up rule and leaving a
 * shared database on New/Open joined it: one subject, one file. */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"          // leave_database: New and Open leave the shared one
#include "platform/app_settings.hpp"  // the default database
#include "domain/seed.hpp"             // register_glyphs, seed_antfarm_transcript

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
void HormigaApp::save_database() {
    if (cur_miga.empty()) { // no bundle yet → behave as Save As (author #1)
        if (on_save_file) {
            std::string p = on_save_file("my-database.miga");
            if (!p.empty()) save_database_as(p);
        } else {
            toast("no save dialog available", true);
        }
        return;
    }
    do_save(); // flush the live state into the working .db first
    std::error_code ec;
    // back up the existing bundle before overwriting (author #5: backups)
    if (fs::exists(cur_miga)) {
        fs::create_directories(data_dir("backups"), ec);
        char stamp[32];
        std::time_t t = std::time(nullptr);
        std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&t));
        std::string stem = fs::path(cur_miga).stem().string();
        fs::copy_file(cur_miga, data_dir("backups") / (stem + "-" + stamp + ".miga"),
                      fs::copy_options::overwrite_existing, ec);
        // retention: keep the newest ~10 backups for this database
        std::vector<fs::path> mine;
        for (auto& e : fs::directory_iterator(data_dir("backups"), ec))
            if (e.path().filename().string().rfind(stem + "-", 0) == 0)
                mine.push_back(e.path());
        std::sort(mine.begin(), mine.end());
        for (size_t i = 0; i + 10 < mine.size(); ++i) fs::remove(mine[i], ec);
    }
    auto r = hormiga::miga::pack(core.export_state(), base_dir, cur_miga,
                                 fs::path(cur_miga).stem().string(), assets_dir(),
                                 referenced_files(core.export_state()));
    if (r.ok)
        toast("saved database " + fs::path(cur_miga).filename().string() + " (" +
              std::to_string(r.assets) + " assets, " +
              std::to_string(r.bytes / 1024) + " KB)");
    else
        toast("save database failed: " + r.error, true);
}

/* Save the working copy as a NEW .miga at a user-CHOSEN path (author
 * 2026-07-24: "choose where we are saving"). The path comes from the OS save
 * dialog; .miga is ensured. This bundle becomes the current database. */
void HormigaApp::save_database_as(const std::string& path) {
    if (path.empty()) return;
    fs::path out = path;
    if (out.extension() != ".miga") out += ".miga";
    do_save(); // flush live state into the working .db first
    auto r = hormiga::miga::pack(core.export_state(), base_dir, out,
                                 out.stem().string(), assets_dir(),
                                 referenced_files(core.export_state()));
    if (r.ok) {
        cur_miga = out.string();
        remember_bundle(cur_miga);
        toast("saved database to " + out.string() + " (" +
              std::to_string(r.assets) + " assets, " +
              std::to_string(r.bytes / 1024) + " KB)");
    } else {
        toast("save database failed: " + r.error, true);
    }
}

/* Start a COMPLETELY NEW, empty database (author 2026-07-24): a fresh working
 * copy — empty data, one blank newsletter document, the Antfarm topology (the
 * protocol layer) — and NO bundle yet (Save database as... names & places its
 * .miga). This is the multi-database "new project" entry the app was missing. */
void HormigaApp::new_database() {
    LanRuntime::leave_database(*this); // out of the shared one: a new database is nobody's yet
    reset_working_copy(true); // a fresh db owns nothing from the old working copy
    core = maiz::Core(); // a fresh, empty state (no demo data)
    install_host();
    channel_fields.clear();
    hormiga::register_glyphs(core);
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
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
    reproject();
    read_view_config();
    apply_theme();
    cur_miga.clear();      // unsaved: a new database has no file until Save As
    remember_bundle("");
    cur_doc = "issue-demo";
    cur_page.clear();
    ed.selection.clear();
    do_save();             // flush the fresh structure into the working .db
    if (!phone) toast("new database (empty) - use 'Save database as...' to name & place it"); // a phone names it first
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
void HormigaApp::reload_from_state(const std::string& state) {
    core = maiz::Core(state);
    install_host();
    channel_fields.clear();
    hormiga::register_glyphs(core);
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
    core.dispatch("config set actor human:hormiga");
    reproject();
    read_view_config();
    apply_theme();
    do_save(); // write the opened database into the working .db
    cur_page.clear();
    cur_doc = "issue-demo";
    ed.selection.clear();
}

void HormigaApp::open_database(const std::string& path) {
    LanRuntime::leave_database(*this); // the one open now stops being this device's shared one
    // isolate: drop the current working copy's assets + caches BEFORE the
    // bundle extracts its own into base_dir/assets/ (databases don't bleed)
    reset_working_copy(true);
    auto r = hormiga::miga::open(path, base_dir, assets_dir());
    if (!r.ok) {
        toast(r.version == 2 ? "that's a legacy secrets-only .miga (v2), not a "
                               "full database"
                             : "open failed: " + r.error,
              true);
        return;
    }
    reload_from_state(r.state);
    cur_miga = path;
    remember_bundle(path);
    load_secrets(); // an imgbb.key beside THIS bundle
    toast("opened database " + fs::path(path).filename().string() + " (" +
          std::to_string(r.assets) + " assets restored)");
}

