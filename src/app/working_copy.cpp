/* app/working_copy.cpp — the working copy is a cache of the .miga
 * (app/working_copy.hpp; Void Rancho's okf/concepts/authority.md; okf
 * concepts/platform/miga-format.md, "The authority").
 *
 * THE RULE: a person meets the .miga and only the .miga. Its working copy (the
 * state document, the SQLite mirror, a store, unpacked pictures, tiles, a
 * rendered site) is a cache in Rancho's per-user cache folder, one per .miga
 * FILE, with a note saying how the two stand. Saving means the .miga; a cache
 * lost loses nothing saved. A database with no file yet works in an `unsaved-`
 * cache, which becomes the file's own at Save As. */
#include "app/app.hpp"
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"         // claim_home: a database opened from its cache is this device's
#include "platform/app_settings.hpp" // the database to open at start

#include <fstream>
#include <random>
#include <sstream>

namespace fs = std::filesystem;

namespace {

fs::path cache_root() { return rancho::cache_root("hormiga"); }

std::string fresh_suffix() {
    std::random_device rd;
    char b[24];
    std::snprintf(b, sizeof b, "%08x%04x", rd(), rd() & 0xffff);
    return b;
}

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

/* A working copy LEFT in a folder by an older Hormiga (the shared one beside
 * wherever the app was started) holds the last session's edits, and possibly
 * ones its .miga never got. It is kept once, as a .miga in the backups, before
 * the working copy moves into caches; the folder itself is left as it is. */
void keep_old_working_copy(const fs::path& folder, const std::string& state_name) {
    std::error_code ec;
    const fs::path doc = folder / state_name;
    const fs::path done = folder / (state_name + ".moved-to-cache");
    if (!fs::is_regular_file(doc, ec) || fs::exists(done, ec)) return;
    const std::string state = read_file(doc);
    if (state.empty()) return;
    const fs::path dir = rancho::keep_root("hormiga") / "backups";
    fs::create_directories(dir, ec);
    char stamp[32];
    const std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&t));
    const fs::path keep = dir / (folder.filename().string().rfind("unsaved-", 0) == 0
                                     ? std::string("unsaved-") + stamp + ".miga"
                                     : std::string("working-copy-before-caches-") + stamp + ".miga");
    const auto r = hormiga::miga::pack(state, folder, keep, "working copy", folder / "assets", {});
    if (r.ok) std::ofstream(done) << "kept as " << keep.string() << "\n";
}

} // namespace

/* WHICH .miga TO START ON (before init): the one given (a double-click, `--open`),
 * else Settings' default. On the first start that uses caches, an older working
 * copy beside the launch folder is kept in the backups, and the .miga it
 * belonged to becomes the default if none is set. "" : nothing to open (a new,
 * unsaved database). */
std::string HormigaApp::wc_boot_target(const std::string& open_arg) {
    std::error_code ec;
    key_dirs(); // the launch folder's note: which .miga an older working copy belonged to
    keep_old_working_copy(base_dir, state_name);
    hormiga::app_settings::Settings as = hormiga::app_settings::load();
    if (!as.migrated) {
        as.migrated = true;
        if (as.default_database.empty() && !bundle_file.empty() && fs::exists(bundle_file, ec))
            as.default_database = bundle_file.string();
        hormiga::app_settings::save(as);
    }
    if (!open_arg.empty()) return fs::absolute(open_arg, ec).string();
    if (!as.default_database.empty() && fs::exists(as.default_database, ec)) return as.default_database;
    if (!as.default_database.empty()) toast("the default database is missing: " + as.default_database, true);
    return {};
}

/* Before init: the working copy becomes `miga`'s cache. True when the cache can
 * be used as it is (it matches the .miga, or holds unsaved edits); false when it
 * is empty and init must start without the demo, for the caller to open the
 * .miga into it. A stale cache (the .miga changed since) is dropped first. */
bool HormigaApp::wc_boot(const std::string& miga) {
    // credentials kept beside the launch folder by an older Hormiga follow the person
    std::error_code ec;
    const fs::path old_vault = base_dir / "org.miga", vault = rancho::keep_root("hormiga") / "credentials.vault";
    if (!fs::exists(vault, ec) && fs::is_regular_file(old_vault, ec)) {
        fs::create_directories(vault.parent_path(), ec);
        fs::copy_file(old_vault, vault, ec);
    }
    const fs::path root = cache_root();
    fs::path c = rancho::working_copy_for(miga, root);
    rancho::CacheState s = rancho::cache_state(c);
    if (s == rancho::CacheState::Stale) { // what it holds is older than the file: rebuild
        rancho::forget(c, root);
        c = rancho::working_copy_for(miga, root);
        s = rancho::CacheState::New;
    }
    base_dir = c;
    data_dir_cache.clear();
    local_read = false;
    wc.cache = c;
    cur_miga = fs::absolute(miga, ec).string();
    wc.ahead = s == rancho::CacheState::Ahead || s == rancho::CacheState::Diverged;
    const std::string name = fs::path(miga).filename().string();
    if (s == rancho::CacheState::Ahead)
        toast("your unsaved changes to " + name + " from last time are here - Ctrl+S saves them into it");
    if (s == rancho::CacheState::Diverged)
        toast(name + " was changed elsewhere after your unsaved changes here were made. Saving now replaces "
                     "those other changes; File > Open it again to take them instead.",
              true);
    if (s == rancho::CacheState::New) {
        wc.filling = true;
        return false;
    }
    return true;
}

/* Before init, with nothing to open: a new database in a cache of its own.
 * Earlier unsaved caches are cleared first: one that holds edits is kept among
 * the backups as a .miga, so a session that was never saved is not lost. */
void HormigaApp::wc_boot_unsaved() {
    std::error_code ec;
    for (const fs::path& old : rancho::caches(cache_root())) {
        if (old.filename().string().rfind("unsaved-", 0) != 0) continue;
        const auto note = rancho::read_note(old);
        if (note && note->ahead) keep_old_working_copy(old, state_name);
        rancho::forget(old, cache_root());
    }
    const fs::path c = cache_root() / ("unsaved-" + fresh_suffix());
    fs::create_directories(c, ec);
    rancho::write_note(c, {}); // no file yet: Save As gives it one
    base_dir = c;
    data_dir_cache.clear();
    local_read = false;
    wc.cache = c;
    wc.filling = true;
    cur_miga.clear();
}

/* While running: let go of this working copy (saved into its own cache first)
 * and take up the one in `dir`. */
void HormigaApp::wc_switch(const fs::path& dir) {
    do_save();
    saver.flush();
    saver.set_storage(nullptr);
    storage.reset();
    wstore.detach();
    hormiga_release_floor();
    std::error_code ec;
    fs::create_directories(dir, ec);
    base_dir = dir;
    data_dir_cache.clear();
    local_read = false;
    tex_cache.clear(); // pictures by path: the paths now mean another folder
    if (!hormiga_claim_floor(org_file()))
        toast("another session holds this database - your changes may be overwritten by it", true);
    storage = std::make_unique<hormiga::Storage>(db_file());
    saver.set_storage(storage.get());
}

/* File > Open, into that file's own cache. True when it was loaded from the
 * cache, with nothing to unpack; false when the caller unpacks the .miga into
 * it (a new or stale cache, or one that is a store). */
bool HormigaApp::wc_open(const std::string& miga) {
    if (wc.cache.empty()) return false; // a working copy of its own: the older open, unchanged
    if (cur_miga.empty() && wc.ahead) wc_keep_aside("unsaved"); // a database with no file: its edits are kept
    const fs::path root = cache_root();
    fs::path c = rancho::working_copy_for(miga, root);
    rancho::CacheState s = rancho::cache_state(c);
    if (s == rancho::CacheState::Stale) {
        rancho::forget(c, root);
        c = rancho::working_copy_for(miga, root);
        s = rancho::CacheState::New;
    }
    wc_switch(c);
    wc.cache = c;
    wc.ahead = s == rancho::CacheState::Ahead || s == rancho::CacheState::Diverged;
    std::error_code ec;
    if (s == rancho::CacheState::New || fs::exists(store_file(), ec)) {
        if (wc.ahead) wc_keep_aside("unsaved"); // a store is reopened from the file; its edits go to the backups
        return false;
    }
    std::string state = storage && storage->ok() ? storage->load_state() : std::string();
    if (fs::exists(org_file(), ec) && fs::exists(db_file(), ec) &&
        fs::last_write_time(org_file(), ec) > fs::last_write_time(db_file(), ec))
        state = read_file(org_file()); // written by a headless run after the last save (as init does)
    if (state.empty() && fs::exists(org_file(), ec)) state = read_file(org_file());
    if (state.empty()) return false;
    reload_from_state(state);
    cur_miga = fs::absolute(miga, ec).string();
    remember_bundle(cur_miga);
    LanRuntime::claim_home(*this);
    load_secrets();
    wc_title();
    const std::string name = fs::path(miga).filename().string();
    toast(wc.ahead ? "opened " + name + " with your unsaved changes from last time - Ctrl+S saves them into it"
                   : "opened " + name);
    return true;
}

/* File > New: a cache of its own, with no file yet. */
void HormigaApp::wc_new() {
    if (wc.cache.empty()) return;
    if (cur_miga.empty() && wc.ahead) wc_keep_aside("unsaved");
    std::error_code ec;
    const fs::path c = cache_root() / ("unsaved-" + fresh_suffix());
    fs::create_directories(c, ec);
    rancho::write_note(c, {});
    wc_switch(c);
    wc.cache = c;
    wc.ahead = false;
}

/* An edit: the cache now holds what the .miga has not got (said once, in its note). */
void HormigaApp::wc_edited() {
    ++wc.edits;
    if (wc.cache.empty() || wc.ahead) return;
    wc.ahead = rancho::mark_ahead(wc.cache);
}

/* The .miga was written from the cache, or the cache filled from the .miga, as
 * it stood at edit `at`. Edits made while a save ran keep the cache ahead. */
void HormigaApp::wc_matched(unsigned long at) {
    if (wc.cache.empty()) return;
    rancho::mark_matched(wc.cache);
    wc.ahead = false;
    if (wc.edits != at) wc.ahead = rancho::mark_ahead(wc.cache);
}

/* The window's title names what a person handles: the .miga. */
void HormigaApp::wc_title() {
    data_dir_cache.clear(); // called wherever the .miga changes: exports and relative paths hang off it
    if (!on_title) return;
    std::error_code ec;
    if (!cur_miga.empty()) {
        const fs::path m = fs::absolute(cur_miga, ec);
        on_title("Hormiga - " + m.filename().string() + " - " + m.parent_path().string());
    } else if (!wc.cache.empty()) {
        on_title("Hormiga - a new database (not saved yet)");
    } else {
        const fs::path abs = fs::absolute(org_file(), ec);
        on_title(ec ? std::string("Hormiga")
                    : "Hormiga - " + abs.parent_path().filename().string() + " - " + abs.string());
    }
}
