/* store/bundle.cpp — see bundle.hpp. */
#include "store/bundle.hpp"

#include "sqlite3.h"

#include <cstring>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace hormiga::store {

namespace {

bool exec(sqlite3* db, const char* sql, std::string& error) {
    char* msg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &msg) == SQLITE_OK) return true;
    error = msg ? msg : "sqlite failed";
    sqlite3_free(msg);
    return false;
}

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

/* Write `bytes` to `p` unless it already holds exactly them. */
bool mirror_file(const fs::path& p, const std::string& bytes) {
    std::error_code ec;
    if (fs::exists(p, ec) && fs::file_size(p, ec) == bytes.size() && read_file(p) == bytes) return true;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), (std::streamsize)bytes.size());
    return (bool)out;
}

/* Where a file named in a bundle goes, or nothing if it would leave its folder. */
bool place(const std::string& path, const fs::path& base_dir, const fs::path& assets_dir, fs::path& out) {
    const bool asset = path.rfind("assets/", 0) == 0;
    const fs::path rel = fs::path(asset ? path.substr(7) : path).lexically_normal();
    if (rel.empty() || rel.is_absolute() || rel.has_root_name() || rel.has_root_directory()) return false;
    for (const auto& part : rel)
        if (part == "..") return false;
    out = (asset ? assets_dir : base_dir) / rel;
    return true;
}

void remove_store(const fs::path& p) {
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm"}) fs::remove(p.string() + s, ec);
}

} // namespace

BundleKind bundle_kind(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return BundleKind::Missing;
    char head[16] = {};
    in.read(head, sizeof head);
    if (in.gcount() == 16 && std::memcmp(head, "SQLite format 3", 16) == 0) return BundleKind::V4;
    for (std::streamsize i = 0; i < in.gcount(); ++i) {
        if (head[i] == ' ' || head[i] == '\n' || head[i] == '\r' || head[i] == '\t') continue;
        return head[i] == '{' ? BundleKind::V3 : BundleKind::Other;
    }
    return BundleKind::Other;
}

BundleResult pack_v4(Store& store, const fs::path& base_dir, const fs::path& assets_dir,
                     const std::vector<std::string>& extra, const fs::path& out, const std::string& name,
                     const Progress& progress) {
    BundleResult r;
    r.name = name;
    const fs::path tmp = out.string() + ".packing";
    remove_store(tmp);
    if (progress) progress(0.05, "Copying the database");
    if (!store.vacuum_into(tmp)) {
        r.error = "could not copy the database: " + store.error();
        return r;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(tmp.string().c_str(), &db) != SQLITE_OK) {
        r.error = "could not open the copy";
        sqlite3_close(db);
        return r;
    }
    bool ok = exec(db, "PRAGMA journal_mode=DELETE; BEGIN;"
                       // a bundle is what the database IS: no row an undo could still have reached
                       "DELETE FROM tags WHERE rune IN (SELECT id FROM runes WHERE deleted=1);"
                       "DELETE FROM clusters WHERE rune IN (SELECT id FROM runes WHERE deleted=1);"
                       "DELETE FROM runes WHERE deleted=1; DELETE FROM edges WHERE deleted=1;"
                       "DELETE FROM mantles WHERE deleted=1;"
                       "CREATE TABLE IF NOT EXISTS assets(path TEXT PRIMARY KEY, bytes BLOB NOT NULL);"
                       "DELETE FROM assets;",
                   r.error);
    // the pictures, one at a time: memory is the largest one, not all of them
    std::vector<std::pair<std::string, fs::path>> files; // name in the bundle, file on disk
    std::error_code ec;
    if (ok && fs::is_directory(assets_dir, ec))
        for (fs::recursive_directory_iterator it(assets_dir, ec), end; !ec && it != end; it.increment(ec))
            if (it->is_regular_file(ec))
                files.emplace_back("assets/" + fs::relative(it->path(), assets_dir, ec).generic_string(), it->path());
    for (const auto& rel : extra) // files a rune names outside the assets folder
        if (rel.rfind("assets/", 0) != 0 && fs::is_regular_file(base_dir / rel, ec))
            files.emplace_back(fs::path(rel).generic_string(), base_dir / rel);
    sqlite3_stmt* put = nullptr;
    if (ok) ok = sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO assets VALUES(?,?)", -1, &put, nullptr) == SQLITE_OK;
    for (std::size_t i = 0; ok && i < files.size(); ++i) {
        const std::string& rel = files[i].first;
        const std::string bytes = read_file(files[i].second);
        sqlite3_bind_text(put, 1, rel.c_str(), (int)rel.size(), SQLITE_TRANSIENT);
        sqlite3_bind_blob64(put, 2, bytes.data(), (sqlite3_uint64)bytes.size(), SQLITE_STATIC);
        ok = sqlite3_step(put) == SQLITE_DONE;
        sqlite3_reset(put);
        ++r.assets;
        r.asset_bytes += (long long)bytes.size();
        if (progress) progress(0.3 + 0.6 * (double)(i + 1) / (double)files.size(), "Packing pictures");
    }
    sqlite3_finalize(put);
    if (ok) {
        sqlite3_stmt* meta = nullptr;
        sqlite3_prepare_v2(db, "INSERT OR REPLACE INTO store_meta VALUES(?,?)", -1, &meta, nullptr);
        for (const auto& [k, v] : {std::pair<std::string, std::string>{"bundle.version", "4"},
                                   {"bundle.name", name}, {"bundle.app", "hormiga"}}) {
            sqlite3_bind_text(meta, 1, k.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(meta, 2, v.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(meta);
            sqlite3_reset(meta);
        }
        sqlite3_finalize(meta);
        sqlite3_stmt* count = nullptr;
        sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM runes", -1, &count, nullptr);
        if (sqlite3_step(count) == SQLITE_ROW) r.runes = sqlite3_column_int64(count, 0);
        sqlite3_finalize(count);
        ok = exec(db, "COMMIT", r.error);
    } else {
        if (r.error.empty()) r.error = sqlite3_errmsg(db);
        exec(db, "ROLLBACK", r.error);
    }
    sqlite3_close(db);
    if (!ok) {
        remove_store(tmp);
        return r;
    }
    fs::rename(tmp, out, ec);
    if (ec) {
        r.error = "could not write " + out.string() + ": " + ec.message();
        remove_store(tmp);
        return r;
    }
    if (progress) progress(1.0, "Packed");
    r.ok = true;
    return r;
}

BundleResult open_bundle(const fs::path& in, const fs::path& store_path, const fs::path& base_dir,
                         const fs::path& assets_dir, const Progress& progress) {
    BundleResult r;
    const BundleKind kind = bundle_kind(in);
    if (kind == BundleKind::Missing || kind == BundleKind::Other) {
        r.error = kind == BundleKind::Missing ? "no such file: " + in.string() : "not a .miga bundle";
        return r;
    }
    remove_store(store_path);
    std::error_code ec;
    if (kind == BundleKind::V3) { // one JSON document: streamed into a new store
        Store st(store_path);
        JsonStream js(in.string());
        const auto stats = st.import(js, progress, [&](const std::string& path, const std::string& bytes) {
            fs::path to;
            if (place(path, base_dir, assets_dir, to)) mirror_file(to, bytes);
            else ++r.refused;
        });
        r.ok = stats.ok;
        r.error = stats.error;
        r.assets = stats.assets;
        r.runes = stats.runes;
        r.name = stats.bundle_name;
        return r;
    }
    // v4: the store itself, copied; then its pictures out into the assets folder
    if (progress) progress(0.05, "Copying the database");
    if (!fs::copy_file(in, store_path, fs::copy_options::overwrite_existing, ec)) {
        r.error = "could not copy " + in.string() + ": " + ec.message();
        return r;
    }
    sqlite3* db = nullptr;
    if (sqlite3_open(store_path.string().c_str(), &db) != SQLITE_OK) {
        r.error = "could not open the copied database";
        sqlite3_close(db);
        return r;
    }
    sqlite3_stmt* q = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT path, bytes FROM assets", -1, &q, nullptr) == SQLITE_OK) {
        while (sqlite3_step(q) == SQLITE_ROW) {
            const std::string path((const char*)sqlite3_column_text(q, 0));
            const auto* blob = (const char*)sqlite3_column_blob(q, 1);
            const std::string bytes(blob ? blob : "", (std::size_t)sqlite3_column_bytes(q, 1));
            fs::path to;
            if (!place(path, base_dir, assets_dir, to)) {
                ++r.refused;
                continue;
            }
            mirror_file(to, bytes);
            ++r.assets;
            r.asset_bytes += (long long)bytes.size();
        }
    }
    sqlite3_finalize(q);
    exec(db, "DROP TABLE IF EXISTS assets", r.error); // the working copy keeps them as files
    if (sqlite3_prepare_v2(db, "SELECT value FROM store_meta WHERE key='bundle.name'", -1, &q, nullptr) == SQLITE_OK &&
        sqlite3_step(q) == SQLITE_ROW)
        r.name = (const char*)sqlite3_column_text(q, 0);
    sqlite3_finalize(q);
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM runes WHERE deleted=0", -1, &q, nullptr) == SQLITE_OK &&
        sqlite3_step(q) == SQLITE_ROW)
        r.runes = sqlite3_column_int64(q, 0);
    sqlite3_finalize(q);
    sqlite3_close(db);
    if (progress) progress(1.0, "Opened");
    r.ok = true;
    return r;
}

} // namespace hormiga::store
