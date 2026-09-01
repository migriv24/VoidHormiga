/* storage.cpp — the SQLite Data holiday, v1. See storage.hpp for the shape. */
#include "platform/storage.hpp"

#include "sqlite3.h"

#include <chrono>
#include <cstdio>

namespace hormiga {

static const char* kSchema =
    "CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT);"
    "CREATE TABLE IF NOT EXISTS runes("
    "  mantle TEXT NOT NULL, name TEXT NOT NULL, glyph TEXT NOT NULL,"
    "  PRIMARY KEY(mantle,name));"
    "CREATE TABLE IF NOT EXISTS fields("
    "  mantle TEXT NOT NULL, rune TEXT NOT NULL, key TEXT NOT NULL, value TEXT,"
    "  PRIMARY KEY(mantle,rune,key));"
    "CREATE TABLE IF NOT EXISTS tags("
    "  mantle TEXT NOT NULL, rune TEXT NOT NULL, tag TEXT NOT NULL,"
    "  PRIMARY KEY(mantle,rune,tag));"
    "CREATE TABLE IF NOT EXISTS links("
    "  mantle TEXT NOT NULL, from_rune TEXT NOT NULL, to_rune TEXT NOT NULL,"
    "  relation TEXT);";

Storage::Storage(std::filesystem::path db_path) : path_(std::move(db_path)) {
    if (sqlite3_open(path_.string().c_str(), &db_) != SQLITE_OK) {
        err_ = db_ ? sqlite3_errmsg(db_) : "out of memory";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    char* msg = nullptr;
    if (sqlite3_exec(db_, kSchema, nullptr, nullptr, &msg) != SQLITE_OK) {
        err_ = msg ? msg : "schema failed";
        sqlite3_free(msg);
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

Storage::~Storage() {
    if (db_) sqlite3_close(db_);
}

/* Prepared-statement helper: bind text params in order, step, finalize. */
static bool run(sqlite3* db, const char* sql, const std::vector<std::string>& args,
                std::string& err) {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        return false;
    }
    for (int i = 0; i < (int)args.size(); ++i)
        sqlite3_bind_text(st, i + 1, args[i].c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(st) == SQLITE_DONE;
    if (!ok) err = sqlite3_errmsg(db);
    sqlite3_finalize(st);
    return ok;
}

bool Storage::save(const std::string& state_json,
                   const std::vector<maiz::Scene>& mantles) {
    if (!db_) return false;
    char* msg = nullptr;
    if (sqlite3_exec(db_, "BEGIN;DELETE FROM runes;DELETE FROM fields;"
                          "DELETE FROM tags;DELETE FROM links;",
                     nullptr, nullptr, &msg) != SQLITE_OK) {
        err_ = msg ? msg : "begin failed";
        sqlite3_free(msg);
        return false;
    }
    bool ok = true;
    for (const auto& scene : mantles) {
        for (const auto& n : scene.nodes) {
            ok = ok && run(db_, "INSERT INTO runes VALUES(?,?,?)",
                           {scene.mantle, n.name, n.glyph}, err_);
            for (const auto& f : n.fields) {
                if (f.value_json.empty() || f.value_json == "null") continue;
                std::string v = f.value_json;
                if (f.is_string && v.size() >= 2) v = v.substr(1, v.size() - 2);
                ok = ok && run(db_, "INSERT INTO fields VALUES(?,?,?,?)",
                               {scene.mantle, n.name, f.key, v}, err_);
            }
            for (const auto& t : n.tags)
                ok = ok && run(db_, "INSERT INTO tags VALUES(?,?,?)",
                               {scene.mantle, n.name, t}, err_);
        }
        for (const auto& w : scene.wires)
            ok = ok && run(db_, "INSERT INTO links VALUES(?,?,?,?)",
                           {scene.mantle, w.from, w.to, w.relation}, err_);
    }
    // ISO-8601 UTC now
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    char when[32];
    std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    ok = ok && run(db_, "INSERT OR REPLACE INTO meta VALUES('state',?)",
                   {state_json}, err_);
    ok = ok && run(db_, "INSERT OR REPLACE INTO meta VALUES('saved_at',?)",
                   {std::string(when)}, err_);
    if (sqlite3_exec(db_, ok ? "COMMIT" : "ROLLBACK", nullptr, nullptr, &msg) !=
        SQLITE_OK) {
        if (err_.empty()) err_ = msg ? msg : "commit failed";
        sqlite3_free(msg);
        return false;
    }
    sqlite3_free(msg);
    return ok;
}

static std::string one_text(sqlite3* db, const char* sql) {
    sqlite3_stmt* st = nullptr;
    std::string out;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW && sqlite3_column_text(st, 0))
        out = (const char*)sqlite3_column_text(st, 0);
    sqlite3_finalize(st);
    return out;
}

std::string Storage::load_state() {
    if (!db_) return {};
    return one_text(db_, "SELECT value FROM meta WHERE key='state'");
}

static long long one_count(sqlite3* db, const char* sql) {
    sqlite3_stmt* st = nullptr;
    long long out = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &st, nullptr) == SQLITE_OK &&
        sqlite3_step(st) == SQLITE_ROW)
        out = sqlite3_column_int64(st, 0);
    sqlite3_finalize(st);
    return out;
}

Storage::Stats Storage::stats() {
    Stats s;
    if (!db_) return s;
    s.runes = one_count(db_, "SELECT COUNT(*) FROM runes");
    s.tags = one_count(db_, "SELECT COUNT(*) FROM tags");
    s.links = one_count(db_, "SELECT COUNT(*) FROM links");
    s.saved_at = one_text(db_, "SELECT value FROM meta WHERE key='saved_at'");
    std::error_code ec;
    auto sz = std::filesystem::file_size(path_, ec);
    if (!ec) s.bytes = (long long)sz;
    return s;
}

} // namespace hormiga
