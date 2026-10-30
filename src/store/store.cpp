/* store/store.cpp — see store.hpp and okf/concepts/platform/store.md. */
#include "store/store.hpp"

#include "voidmaiz/graph.hpp"

#include "json.hpp"
#include "sqlite3.h"

#include <sstream>
#include <utility>

namespace hormiga::store {

namespace {

/* A prepared statement, reused: bind, step, reset. */
class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) : db_(db) { sqlite3_prepare_v2(db, sql, -1, &st_, nullptr); }
    ~Stmt() { sqlite3_finalize(st_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    bool ok() const { return st_ != nullptr; }
    Stmt& text(int i, std::string_view v) {
        sqlite3_bind_text(st_, i, v.data() ? v.data() : "", (int)v.size(), SQLITE_TRANSIENT);
        return *this;
    }
    Stmt& null(int i) {
        sqlite3_bind_null(st_, i);
        return *this;
    }
    Stmt& integer(int i, long long v) {
        sqlite3_bind_int64(st_, i, v);
        return *this;
    }
    Stmt& real(int i, double v) {
        sqlite3_bind_double(st_, i, v);
        return *this;
    }
    // one statement that returns no rows; resets for the next use
    bool run() {
        const int rc = sqlite3_step(st_);
        sqlite3_reset(st_);
        sqlite3_clear_bindings(st_);
        return rc == SQLITE_DONE || rc == SQLITE_ROW;
    }
    bool row() { return sqlite3_step(st_) == SQLITE_ROW; }
    void reset() {
        sqlite3_reset(st_);
        sqlite3_clear_bindings(st_);
    }
    std::string_view col(int i) {
        const auto* p = (const char*)sqlite3_column_text(st_, i);
        return p ? std::string_view(p, (std::size_t)sqlite3_column_bytes(st_, i)) : std::string_view();
    }
    bool is_null(int i) { return sqlite3_column_type(st_, i) == SQLITE_NULL; }
    long long col_int(int i) { return sqlite3_column_int64(st_, i); }

private:
    sqlite3* db_;
    sqlite3_stmt* st_ = nullptr;
};

const char* kTables =
    "CREATE TABLE IF NOT EXISTS store_meta(key TEXT PRIMARY KEY, value TEXT);"
    "CREATE TABLE IF NOT EXISTS top(key TEXT PRIMARY KEY, pos INTEGER NOT NULL, json TEXT);"
    "CREATE TABLE IF NOT EXISTS mantles(id INTEGER PRIMARY KEY, pos INTEGER NOT NULL, name TEXT NOT NULL DEFAULT '',"
    " json TEXT NOT NULL DEFAULT '{}', layout TEXT, has_runes INTEGER NOT NULL DEFAULT 1,"
    " has_edges INTEGER NOT NULL DEFAULT 1, mid TEXT, deleted INTEGER NOT NULL DEFAULT 0, deleted_by TEXT);"
    "CREATE TABLE IF NOT EXISTS runes(id INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, pos REAL NOT NULL,"
    " name TEXT NOT NULL, rid TEXT, glyph TEXT, json TEXT NOT NULL, deleted INTEGER NOT NULL DEFAULT 0,"
    " deleted_by TEXT, bytes INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS edges(id INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, pos REAL NOT NULL,"
    " src TEXT NOT NULL, dst TEXT NOT NULL, relation TEXT, json TEXT NOT NULL, deleted INTEGER NOT NULL DEFAULT 0,"
    " deleted_by TEXT, bytes INTEGER NOT NULL DEFAULT 0);"
    "CREATE TABLE IF NOT EXISTS clusters(rune INTEGER PRIMARY KEY, mantle INTEGER NOT NULL, path TEXT NOT NULL,"
    " always INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX IF NOT EXISTS clusters_by_path ON clusters(mantle, path);"
    "CREATE TABLE IF NOT EXISTS est_mantles(mantle INTEGER PRIMARY KEY, runes INTEGER, edges INTEGER,"
    " rune_bytes INTEGER, edge_bytes INTEGER);"
    "CREATE TABLE IF NOT EXISTS est_leaves(mantle INTEGER, path TEXT, runes INTEGER, bytes INTEGER,"
    " PRIMARY KEY(mantle, path));"
    "CREATE TABLE IF NOT EXISTS tags(rune INTEGER NOT NULL, tag TEXT NOT NULL);";

// built after an import: inserting into indexed tables is several times slower
const char* kIndexes =
    "CREATE INDEX IF NOT EXISTS runes_by_name ON runes(mantle, name);"
    "CREATE INDEX IF NOT EXISTS runes_by_pos ON runes(mantle, pos);"
    "CREATE INDEX IF NOT EXISTS edges_by_src ON edges(mantle, src);"
    "CREATE INDEX IF NOT EXISTS edges_by_dst ON edges(mantle, dst);"
    "CREATE INDEX IF NOT EXISTS edges_by_pos ON edges(mantle, pos);"
    "CREATE INDEX IF NOT EXISTS tags_by_tag ON tags(tag, rune);"
    "CREATE INDEX IF NOT EXISTS tags_by_rune ON tags(rune);"
    "CREATE INDEX IF NOT EXISTS runes_by_rid ON runes(mantle, rid);"
    "CREATE INDEX IF NOT EXISTS runes_deleted_by ON runes(deleted_by) WHERE deleted_by IS NOT NULL;"
    "CREATE INDEX IF NOT EXISTS edges_deleted_by ON edges(deleted_by) WHERE deleted_by IS NOT NULL;";

const char* kDropAll =
    "DROP INDEX IF EXISTS runes_by_name; DROP INDEX IF EXISTS runes_by_pos;"
    "DROP INDEX IF EXISTS edges_by_src; DROP INDEX IF EXISTS edges_by_dst; DROP INDEX IF EXISTS edges_by_pos;"
    "DROP INDEX IF EXISTS tags_by_tag; DROP INDEX IF EXISTS tags_by_rune; DROP INDEX IF EXISTS runes_by_rid;"
    "DROP INDEX IF EXISTS runes_deleted_by; DROP INDEX IF EXISTS edges_deleted_by;"
    "DELETE FROM top; DELETE FROM mantles; DELETE FROM runes; DELETE FROM edges; DELETE FROM tags;"
    "DELETE FROM clusters; DELETE FROM est_mantles; DELETE FROM est_leaves;";

std::string str_of(const nlohmann::json& j, const char* k) {
    const auto it = j.find(k);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

int base64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

} // namespace

Store::Store(const std::filesystem::path& path) : path_(path) {
    if (sqlite3_open(path.string().c_str(), &db_) != SQLITE_OK) {
        error_ = db_ ? sqlite3_errmsg(db_) : "cannot open the store";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        return;
    }
    /* A bounded page cache (8 MB) whatever the database's size: the store is
     * how a database larger than memory is read at all. */
    exec("PRAGMA journal_mode=WAL; PRAGMA synchronous=NORMAL; PRAGMA cache_size=-8192;"
         "PRAGMA temp_store=FILE;");
    if (exec(kTables) && exec(kIndexes)) {
        Stmt m(db_, "INSERT OR IGNORE INTO store_meta VALUES('format', ?)");
        m.text(1, std::to_string(kFormat)).run();
    }
}

Store::~Store() {
    if (db_) sqlite3_close(db_);
}

bool Store::exec(const char* sql) {
    char* msg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &msg) != SQLITE_OK) {
        error_ = msg ? msg : "sqlite failed";
        sqlite3_free(msg);
        return false;
    }
    return true;
}

ImportStats Store::import(JsonStream& in, const Progress& progress, const AssetSink& assets) {
    ImportStats st;
    if (!db_) {
        st.error = error_;
        return st;
    }
    error_.clear();
    if (!exec("BEGIN") || !exec(kDropAll)) {
        st.error = error_;
        return st;
    }
    Stmt top(db_, "INSERT OR REPLACE INTO top VALUES(?,?,?)");
    Stmt mantle_new(db_, "INSERT INTO mantles(pos) VALUES(?)");
    Stmt mantle_done(db_, "UPDATE mantles SET name=?, json=?, layout=?, has_runes=?, has_edges=?, mid=? WHERE id=?");
    Stmt rune(db_, "INSERT INTO runes(mantle,pos,name,rid,glyph,json,bytes) VALUES(?,?,?,?,?,?,?)");
    Stmt tag(db_, "INSERT INTO tags VALUES(?,?)");
    Stmt edge(db_, "INSERT INTO edges(mantle,pos,src,dst,relation,json,bytes) VALUES(?,?,?,?,?,?,?)");
    Stmt meta(db_, "INSERT OR REPLACE INTO store_meta VALUES(?,?)");

    int top_pos = 0;
    auto tick = [&](const char* phase) {
        if (progress && in.size()) progress((double)in.offset() / (double)in.size(), phase);
    };

    /* One mantle: its runes and edges become rows as they are read; every other
     * member is kept as written, for the mantle's own row. */
    auto read_mantle = [&](int pos) -> bool {
        mantle_new.integer(1, pos).run();
        const long long id = sqlite3_last_insert_rowid(db_);
        std::string skeleton = "{", layout, name, mid, raw;
        bool has_layout = false, has_runes = false, has_edges = false;
        long long rpos = 0, epos = 0;
        const bool ok = in.object([&](const std::string& key) -> bool {
            if (key == "runes") {
                has_runes = true;
                return in.array([&] {
                    if (!in.capture(raw)) return false;
                    nlohmann::json j = nlohmann::json::parse(raw, nullptr, false);
                    if (j.is_discarded() || !j.is_object()) return in.fail("a rune that is not an object");
                    const nlohmann::json sp = j.value("spirit", nlohmann::json::object());
                    rune.integer(1, id).real(2, (double)rpos++).text(3, str_of(sp, "name")).text(4, str_of(sp, "id"))
                        .text(5, str_of(j, "glyph")).text(6, raw).integer(7, (long long)raw.size()).run();
                    const long long rid = sqlite3_last_insert_rowid(db_);
                    if (const auto t = j.find("tags"); t != j.end() && t->is_array())
                        for (const auto& x : *t)
                            if (x.is_string()) {
                                tag.integer(1, rid).text(2, x.get<std::string>()).run();
                                ++st.tags;
                            }
                    if (++st.runes % 4096 == 0) tick("Reading the records");
                    return true;
                });
            }
            if (key == "layout" && in.peek() == '{') {
                has_layout = true;
                layout = "{";
                return in.object([&](const std::string& lk) -> bool {
                    if (lk == "edges") {
                        has_edges = true;
                        return in.array([&] {
                            if (!in.capture(raw)) return false;
                            nlohmann::json j = nlohmann::json::parse(raw, nullptr, false);
                            if (j.is_discarded() || !j.is_object()) return in.fail("an edge that is not an object");
                            edge.integer(1, id).real(2, (double)epos++).text(3, str_of(j, "from"))
                                .text(4, str_of(j, "to")).text(5, str_of(j, "relation")).text(6, raw)
                                .integer(7, (long long)raw.size()).run();
                            ++st.edges;
                            return true;
                        });
                    }
                    if (!in.capture(raw)) return false;
                    if (layout.size() > 1) layout += ",";
                    layout += nlohmann::json(lk).dump() + ":" + raw;
                    return true;
                }) && (layout += "}", true);
            }
            if (!in.capture(raw)) return false;
            if (key == "name" || key == "id") {
                const auto j = nlohmann::json::parse(raw, nullptr, false);
                if (j.is_string()) (key == "name" ? name : mid) = j.get<std::string>();
            }
            if (skeleton.size() > 1) skeleton += ",";
            skeleton += nlohmann::json(key).dump() + ":" + raw;
            return true;
        });
        if (!ok) return false;
        skeleton += "}";
        mantle_done.text(1, name).text(2, skeleton);
        if (has_layout) mantle_done.text(3, layout);
        else mantle_done.null(3);
        mantle_done.integer(4, has_runes ? 1 : 0).integer(5, has_edges ? 1 : 0).text(6, mid).integer(7, id).run();
        ++st.mantles;
        return true;
    };

    auto read_state = [&]() -> bool {
        std::string raw;
        return in.object([&](const std::string& key) -> bool {
            if (key == "mantles") {
                top.text(1, "mantles").integer(2, top_pos++).null(3).run();
                int pos = 0;
                return in.array([&] { return read_mantle(pos++); });
            }
            if (key == "_baseline") return in.skip(); // the store is the baseline
            if (!in.capture(raw)) return false;
            top.text(1, key).integer(2, top_pos++).text(3, raw).run();
            return true;
        });
    };

    /* The outer object is a bare state or an envelope; which is decided by its
     * keys, and both start with "version". Members that are not the state are
     * kept aside and filed once that is known. */
    std::vector<std::pair<std::string, std::string>> held;
    bool is_state = false;
    const bool ok = in.object([&](const std::string& key) -> bool {
        if (key == "state") {
            st.envelope = true;
            return read_state();
        }
        if (key == "magic") st.envelope = true;
        if (key == "assets" && in.peek() == '{') {
            st.envelope = true;
            return in.object([&](const std::string& path) -> bool {
                std::string bytes;
                unsigned acc = 0;
                int bits = 0;
                const bool good = in.string_chunks([&](std::string_view chunk) {
                    for (const char c : chunk) {
                        const int v = base64_value((unsigned char)c);
                        if (v < 0) continue; // padding, line breaks
                        acc = (acc << 6) | (unsigned)v;
                        bits += 6;
                        if (bits >= 8) {
                            bits -= 8;
                            bytes += (char)((acc >> bits) & 0xFF);
                        }
                    }
                });
                if (!good) return false;
                if (assets) assets(path, bytes);
                ++st.assets;
                tick("Restoring pictures");
                return true;
            });
        }
        if (key == "mantles") {
            is_state = true;
            top.text(1, "mantles").integer(2, top_pos++).null(3).run();
            int pos = 0;
            return in.array([&] { return read_mantle(pos++); });
        }
        if (key == "_baseline") return in.skip();
        std::string raw;
        if (!in.capture(raw)) return false;
        held.emplace_back(key, std::move(raw));
        return true;
    });

    if (ok) {
        if (st.envelope && !is_state) {
            for (const auto& [k, v] : held) {
                meta.text(1, "bundle." + k).text(2, v).run();
                if (k == "meta") {
                    const auto j = nlohmann::json::parse(v, nullptr, false);
                    if (j.is_object()) st.bundle_name = str_of(j, "name");
                }
            }
        } else {
            for (const auto& [k, v] : held) top.text(1, k).integer(2, top_pos++).text(3, v).run();
        }
    }
    if (!ok || !error_.empty()) {
        st.error = !in.ok() ? in.error() : (!error_.empty() ? error_ : "the document could not be read");
        exec("ROLLBACK");
        exec(kIndexes);
        return st;
    }
    if (progress) progress(1.0, "Indexing");
    if (!exec(kIndexes)) {
        st.error = error_;
        exec("ROLLBACK");
        return st;
    }
    refresh_estimates();
    if (!exec("COMMIT")) {
        st.error = error_;
        exec("ROLLBACK");
        return st;
    }
    st.ok = true;
    return st;
}

bool Store::write_state(std::ostream& out, const Selection& sel) {
    if (!db_) return false;
    exec("CREATE TEMP TABLE IF NOT EXISTS ws(name TEXT PRIMARY KEY)"); // before the statements that read it
    Stmt tops(db_, "SELECT key, json FROM top ORDER BY pos");
    Stmt ms(db_, "SELECT id, name, json, layout, has_runes, has_edges FROM mantles WHERE deleted=0 ORDER BY pos");
    Stmt all_runes(db_, "SELECT json FROM runes WHERE mantle=? AND deleted=0 ORDER BY pos");
    Stmt all_edges(db_, "SELECT json FROM edges WHERE mantle=? AND deleted=0 ORDER BY pos");
    /* A selection is read FROM the loaded names, through the name indexes, so it
     * costs what it loads and not what the mantle holds. The order is forced
     * (CROSS JOIN, INDEXED BY): the planner, knowing nothing of the temp table's
     * size, walked the whole mantle in order to skip a sort (0.9 s for 1 MB of a
     * 300 MB mantle), and for edges chose the dst index and rescanned the loaded
     * set per row (40 s). Both now take ~0.03 s. */
    Stmt ws_runes(db_, "SELECT r.json FROM temp.ws w CROSS JOIN runes r INDEXED BY runes_by_name"
                       " ON r.mantle=?1 AND r.name=w.name WHERE r.deleted=0 ORDER BY r.pos");
    Stmt ws_edges(db_, "SELECT e.json FROM temp.ws w CROSS JOIN edges e INDEXED BY edges_by_src"
                       " ON e.mantle=?1 AND e.src=w.name CROSS JOIN temp.ws w2 ON w2.name=e.dst"
                       " WHERE e.deleted=0 ORDER BY e.pos");
    Stmt ws_clear(db_, "DELETE FROM temp.ws");
    Stmt ws_tag(db_, "INSERT OR IGNORE INTO temp.ws SELECT r.name FROM tags t JOIN runes r ON r.id=t.rune"
                     " WHERE t.tag=? AND r.mantle=? AND r.deleted=0");
    Stmt ws_name(db_, "INSERT OR IGNORE INTO temp.ws VALUES(?)");
    /* A path and every leaf under it, as ONE index range ("12" .. "12.~": a
     * digit sorts after '.', so "120" is outside it); the second condition only
     * keeps "12" from matching "12x". Written with OR, the planner read the
     * mantle's whole index per path: 58 ms a leaf, 28 s for the ~480 small
     * leaves a 2 GB machine's plan holds at 5×10^5 runes. Now 0.5 ms. */
    Stmt ws_cluster(db_, "INSERT OR IGNORE INTO temp.ws SELECT r.name FROM clusters c INDEXED BY clusters_by_path"
                         " JOIN runes r ON r.id=c.rune WHERE c.mantle=?1 AND c.path>=?2 AND c.path<=?2||'.~'"
                         " AND (c.path=?2 OR c.path>?2||'.') AND r.deleted=0");
    Stmt ws_always(db_, "INSERT OR IGNORE INTO temp.ws SELECT r.name FROM clusters c JOIN runes r ON r.id=c.rune"
                        " WHERE c.mantle=?1 AND r.deleted=0 AND c.always=1");

    /* Rows straight to `out`: a mantle of 5×10^5 runes is written without its
     * runes ever being one string. */
    auto list = [&](Stmt* q, long long id) {
        out << "[";
        if (q) {
            q->integer(1, id);
            bool first = true;
            while (q->row()) {
                if (!first) out << ",";
                out << q->col(0);
                first = false;
            }
            q->reset();
        }
        out << "]";
    };
    // "{a,b" of an object written as "{a,b}": the members, without the brace; and whether there are any
    auto open_object = [&](std::string_view object) {
        const auto end = object.rfind('}');
        const std::string_view body = end == std::string_view::npos ? std::string_view("{") : object.substr(0, end);
        out << body;
        for (std::size_t i = body.find('{') + 1; i < body.size(); ++i)
            if (body[i] != ' ' && body[i] != '\n' && body[i] != '\r' && body[i] != '\t') return true;
        return false;
    };

    out << "{";
    bool first_top = true;
    while (tops.row()) {
        const std::string key(tops.col(0));
        if (!first_top) out << ",";
        first_top = false;
        out << nlohmann::json(key).dump() << ":";
        if (key != "mantles" || !tops.is_null(1)) {
            out << tops.col(1);
            continue;
        }
        out << "[";
        bool first_m = true;
        while (ms.row()) {
            const long long id = ms.col_int(0);
            const std::string name(ms.col(1));
            const std::string skeleton(ms.col(2));
            const bool has_layout = !ms.is_null(3);
            const std::string layout = has_layout ? std::string(ms.col(3)) : std::string();
            const bool has_runes = ms.col_int(4) != 0, has_edges = ms.col_int(5) != 0;

            Stmt* runes = nullptr;
            Stmt* edges = nullptr;
            if (sel.everything || sel.whole.count(name)) {
                runes = &all_runes;
                edges = &all_edges;
            } else if (sel.by_tag.count(name) || sel.by_name.count(name) || sel.by_cluster.count(name)) {
                runes = &ws_runes;
                edges = &ws_edges;
                ws_clear.run();
                if (const auto t = sel.by_tag.find(name); t != sel.by_tag.end())
                    for (const auto& tg : t->second) ws_tag.text(1, tg).integer(2, id).run();
                if (const auto n = sel.by_name.find(name); n != sel.by_name.end())
                    for (const auto& nm : n->second) ws_name.text(1, nm).run();
                if (const auto c = sel.by_cluster.find(name); c != sel.by_cluster.end()) {
                    for (const auto& path : c->second) ws_cluster.integer(1, id).text(2, path).run();
                    ws_always.integer(1, id).run();
                }
            }

            if (!first_m) out << ",";
            first_m = false;
            bool comma = open_object(skeleton);
            if (has_runes) {
                out << (comma ? "," : "") << "\"runes\":";
                list(runes, id);
                comma = true;
            }
            if (has_layout) {
                out << (comma ? "," : "") << "\"layout\":";
                const bool lcomma = open_object(layout);
                if (has_edges) {
                    out << (lcomma ? "," : "") << "\"edges\":";
                    list(edges, id);
                }
                out << "}";
            }
            out << "}";
        }
        ms.reset();
        out << "]";
    }
    tops.reset();
    out << "}";
    return (bool)out;
}

std::string Store::state(const Selection& sel) {
    std::ostringstream out;
    write_state(out, sel);
    return out.str();
}

std::vector<MantleInfo> Store::mantles() {
    std::vector<MantleInfo> out;
    if (!db_) return out;
    Stmt q(db_, "SELECT name, (SELECT COUNT(*) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
                " (SELECT COUNT(*) FROM edges e WHERE e.mantle=m.id AND e.deleted=0),"
                " (SELECT COALESCE(SUM(bytes),0) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
                " (SELECT COALESCE(SUM(bytes),0) FROM edges e WHERE e.mantle=m.id AND e.deleted=0)"
                " FROM mantles m WHERE m.deleted=0 ORDER BY pos");
    while (q.row())
        out.push_back({std::string(q.col(0)), q.col_int(1), q.col_int(2), q.col_int(3) + q.col_int(4), q.col_int(3)});
    return out;
}

long long Store::rune_count(const std::string& mantle) {
    if (!db_) return 0;
    if (mantle.empty()) {
        Stmt q(db_, "SELECT COUNT(*) FROM runes WHERE deleted=0");
        return q.row() ? q.col_int(0) : 0;
    }
    Stmt q(db_, "SELECT COUNT(*) FROM runes WHERE deleted=0 AND mantle=(SELECT id FROM mantles WHERE name=?"
                " AND deleted=0)");
    q.text(1, mantle);
    return q.row() ? q.col_int(0) : 0;
}

// ── write-back ───────────────────────────────────────────────────────────────

bool Store::begin() { return db_ && exec("BEGIN"); }
bool Store::commit() { return db_ && exec("COMMIT"); }
void Store::rollback() {
    if (db_) exec("ROLLBACK");
}

void Store::put_top(const std::string& key, const std::string& json) {
    Stmt q(db_, "INSERT INTO top(key, pos, json) VALUES(?1, (SELECT COALESCE(MAX(pos), -1) + 1 FROM top), ?2)"
                " ON CONFLICT(key) DO UPDATE SET json=excluded.json");
    q.text(1, key).text(2, json).run();
}

void Store::keep_only_top(const std::set<std::string>& keys) {
    std::vector<std::string> gone;
    {
        Stmt q(db_, "SELECT key FROM top");
        while (q.row()) {
            const std::string k(q.col(0));
            if (k != "mantles" && !keys.count(k)) gone.push_back(k);
        }
    }
    Stmt del(db_, "DELETE FROM top WHERE key=?");
    for (const auto& k : gone) del.text(1, k).run();
    Stmt m(db_, "INSERT OR IGNORE INTO top VALUES('mantles', (SELECT COALESCE(MAX(pos), -1) + 1 FROM top), NULL)");
    m.run();
}

namespace {
std::string mantle_tag(const std::string& mid, long long row) {
    return mid.empty() ? "mantle#" + std::to_string(row) : "mantle:" + mid;
}
} // namespace

long long Store::put_mantle(const MantleRow& m, int pos) {
    long long id = -1;
    if (!m.mid.empty()) {
        Stmt live(db_, "SELECT id FROM mantles WHERE mid=? AND deleted=0");
        live.text(1, m.mid);
        if (live.row()) id = live.col_int(0);
        live.reset();
        if (id < 0) { // a mantle removed in this session and now back (an undo): with what it took
            Stmt gone(db_, "SELECT id FROM mantles WHERE mid=? AND deleted=1 ORDER BY id DESC LIMIT 1");
            gone.text(1, m.mid);
            if (gone.row()) id = gone.col_int(0);
            gone.reset();
            if (id >= 0) {
                const std::string tag = mantle_tag(m.mid, id);
                Stmt a(db_, "UPDATE mantles SET deleted=0, deleted_by=NULL WHERE id=?");
                a.integer(1, id).run();
                Stmt b(db_, "UPDATE runes SET deleted=0, deleted_by=NULL WHERE deleted_by=?");
                b.text(1, tag).run();
                Stmt c(db_, "UPDATE edges SET deleted=0, deleted_by=NULL WHERE deleted_by=?");
                c.text(1, tag).run();
            }
        }
    } else {
        Stmt byname(db_, "SELECT id FROM mantles WHERE name=? AND deleted=0");
        byname.text(1, m.name);
        if (byname.row()) id = byname.col_int(0);
        byname.reset();
    }
    if (id < 0) {
        Stmt add(db_, "INSERT INTO mantles(pos) VALUES(?)");
        add.integer(1, pos).run();
        id = sqlite3_last_insert_rowid(db_);
    }
    Stmt up(db_, "UPDATE mantles SET pos=?, name=?, json=?, layout=?, has_runes=?, has_edges=?, mid=? WHERE id=?");
    up.integer(1, pos).text(2, m.name).text(3, m.skeleton);
    if (m.layout) up.text(4, *m.layout);
    else up.null(4);
    up.integer(5, m.has_runes ? 1 : 0).integer(6, m.has_edges ? 1 : 0).text(7, m.mid).integer(8, id).run();
    return id;
}

void Store::delete_mantle(long long mantle) {
    std::string mid;
    {
        Stmt q(db_, "SELECT mid FROM mantles WHERE id=?");
        q.integer(1, mantle);
        if (q.row()) mid = std::string(q.col(0));
    }
    const std::string tag = mantle_tag(mid, mantle);
    Stmt a(db_, "UPDATE mantles SET deleted=1, deleted_by=?1 WHERE id=?2");
    a.text(1, tag).integer(2, mantle).run();
    Stmt b(db_, "UPDATE runes SET deleted=1, deleted_by=?1 WHERE mantle=?2 AND deleted=0");
    b.text(1, tag).integer(2, mantle).run();
    Stmt c(db_, "UPDATE edges SET deleted=1, deleted_by=?1 WHERE mantle=?2 AND deleted=0");
    c.text(1, tag).integer(2, mantle).run();
}

void Store::put_rune(long long mantle, const std::string& name, const std::string& json) {
    const nlohmann::json j = nlohmann::json::parse(json, nullptr, false);
    const nlohmann::json sp = j.is_object() ? j.value("spirit", nlohmann::json::object()) : nlohmann::json::object();
    const std::string rid = str_of(sp, "id"), glyph = j.is_object() ? str_of(j, "glyph") : std::string();
    long long row = -1;
    {
        Stmt q(db_, "SELECT id FROM runes WHERE mantle=? AND name=? AND deleted=0");
        q.integer(1, mantle).text(2, name);
        if (q.row()) row = q.col_int(0);
    }
    if (row >= 0) {
        Stmt up(db_, "UPDATE runes SET rid=?, glyph=?, json=?, bytes=? WHERE id=?");
        up.text(1, rid).text(2, glyph).text(3, json).integer(4, (long long)json.size()).integer(5, row).run();
        Stmt drop(db_, "DELETE FROM tags WHERE rune=?");
        drop.integer(1, row).run();
    } else {
        Stmt add(db_, "INSERT INTO runes(mantle, pos, name, rid, glyph, json, bytes)"
                      " VALUES(?1, (SELECT COALESCE(MAX(pos), -1) + 1 FROM runes WHERE mantle=?1), ?2, ?3, ?4, ?5, ?6)");
        add.integer(1, mantle).text(2, name).text(3, rid).text(4, glyph).text(5, json)
            .integer(6, (long long)json.size()).run();
        row = sqlite3_last_insert_rowid(db_);
    }
    Stmt tag(db_, "INSERT INTO tags VALUES(?,?)");
    if (j.is_object())
        if (const auto t = j.find("tags"); t != j.end() && t->is_array())
            for (const auto& x : *t)
                if (x.is_string()) tag.integer(1, row).text(2, x.get<std::string>()).run();
}

void Store::rename_rune(long long mantle, const std::string& from, const std::string& to) {
    Stmt r(db_, "UPDATE runes SET name=?1 WHERE mantle=?2 AND name=?3 AND deleted=0");
    r.text(1, to).integer(2, mantle).text(3, from).run();
    // every live edge that names it, loaded or not: Core would have renamed them all had it held them
    std::vector<std::pair<long long, std::string>> rows;
    {
        Stmt q(db_, "SELECT id, json FROM edges WHERE mantle=?1 AND deleted=0 AND (src=?2 OR dst=?2)");
        q.integer(1, mantle).text(2, from);
        while (q.row()) rows.emplace_back(q.col_int(0), std::string(q.col(1)));
    }
    Stmt up(db_, "UPDATE edges SET src=?1, dst=?2, json=?3, bytes=length(?3) WHERE id=?4");
    for (auto& [id, js] : rows) {
        nlohmann::json e = nlohmann::json::parse(js, nullptr, false);
        if (!e.is_object()) continue;
        if (str_of(e, "from") == from) e["from"] = to;
        if (str_of(e, "to") == from) e["to"] = to;
        const std::string dumped = e.dump();
        up.text(1, str_of(e, "from")).text(2, str_of(e, "to")).text(3, dumped).integer(4, id).run();
    }
}

void Store::delete_rune(long long mantle, const std::string& name) {
    long long row = -1;
    std::string rid;
    {
        Stmt q(db_, "SELECT id, rid FROM runes WHERE mantle=? AND name=? AND deleted=0");
        q.integer(1, mantle).text(2, name);
        if (q.row()) {
            row = q.col_int(0);
            rid = std::string(q.col(1));
        }
    }
    if (row < 0) return;
    const std::string tag = rid.empty() ? "rune#" + std::to_string(row) : "rune:" + rid;
    Stmt a(db_, "UPDATE runes SET deleted=1, deleted_by=?1 WHERE id=?2");
    a.text(1, tag).integer(2, row).run();
    Stmt b(db_, "UPDATE edges SET deleted=1, deleted_by=?1 WHERE mantle=?2 AND deleted=0 AND (src=?3 OR dst=?3)");
    b.text(1, tag).integer(2, mantle).text(3, name).run();
}

bool Store::restore_rune(long long mantle, const std::string& rid) {
    if (rid.empty()) return false;
    long long row = -1;
    std::string name;
    {
        Stmt q(db_, "SELECT id, name FROM runes WHERE mantle=? AND rid=? AND deleted=1 ORDER BY id DESC LIMIT 1");
        q.integer(1, mantle).text(2, rid);
        if (q.row()) {
            row = q.col_int(0);
            name = std::string(q.col(1));
        }
    }
    if (row < 0) return false;
    {
        Stmt clash(db_, "SELECT 1 FROM runes WHERE mantle=? AND name=? AND deleted=0");
        clash.integer(1, mantle).text(2, name);
        if (clash.row()) return false; // its name was taken since: it comes back as a new rune
    }
    Stmt a(db_, "UPDATE runes SET deleted=0, deleted_by=NULL WHERE id=?");
    a.integer(1, row).run();
    // its edges, where the other end is still there
    Stmt b(db_, "UPDATE edges SET deleted=0, deleted_by=NULL WHERE deleted_by=?1 AND mantle=?2 AND EXISTS("
                " SELECT 1 FROM runes r WHERE r.mantle=?2 AND r.deleted=0"
                " AND r.name=(CASE WHEN edges.src=?3 THEN edges.dst ELSE edges.src END))");
    b.text(1, "rune:" + rid).integer(2, mantle).text(3, name).run();
    return true;
}

void Store::set_edges(long long mantle, const std::string& src, const std::string& dst, const std::string& relation,
                      const std::vector<std::string>& jsons) {
    struct Row {
        long long id;
        nlohmann::json j;
        bool used = false;
    };
    std::vector<Row> have;
    {
        Stmt q(db_, "SELECT id, json FROM edges WHERE mantle=? AND src=? AND dst=? AND relation=? AND deleted=0"
                    " ORDER BY pos");
        q.integer(1, mantle).text(2, src).text(3, dst).text(4, relation);
        while (q.row()) have.push_back({q.col_int(0), nlohmann::json::parse(q.col(1), nullptr, false)});
    }
    std::vector<const std::string*> missing;
    for (const auto& js : jsons) {
        const nlohmann::json want = nlohmann::json::parse(js, nullptr, false);
        auto it = std::find_if(have.begin(), have.end(), [&](const Row& r) { return !r.used && r.j == want; });
        if (it != have.end()) it->used = true;
        else missing.push_back(&js);
    }
    Stmt up(db_, "UPDATE edges SET json=?, bytes=length(?1) WHERE id=?2");
    Stmt add(db_, "INSERT INTO edges(mantle, pos, src, dst, relation, json, bytes)"
                  " VALUES(?1, (SELECT COALESCE(MAX(pos), -1) + 1 FROM edges WHERE mantle=?1), ?2, ?3, ?4, ?5,"
                  " length(?5))");
    for (const std::string* js : missing) {
        auto spare = std::find_if(have.begin(), have.end(), [](const Row& r) { return !r.used; });
        if (spare != have.end()) {
            spare->used = true;
            up.text(1, *js).integer(2, spare->id).run();
        } else {
            add.integer(1, mantle).text(2, src).text(3, dst).text(4, relation).text(5, *js).run();
        }
    }
    Stmt del(db_, "DELETE FROM edges WHERE id=?");
    for (const auto& r : have)
        if (!r.used) del.integer(1, r.id).run();
}

void Store::purge() {
    exec("DELETE FROM tags WHERE rune IN (SELECT id FROM runes WHERE deleted=1);"
         "DELETE FROM clusters WHERE rune IN (SELECT id FROM runes WHERE deleted=1);"
         "DELETE FROM runes WHERE deleted=1; DELETE FROM edges WHERE deleted=1; DELETE FROM mantles WHERE deleted=1;");
}

long long Store::mantle_row(const std::string& name) {
    Stmt q(db_, "SELECT id FROM mantles WHERE name=? AND deleted=0");
    q.text(1, name);
    return q.row() ? q.col_int(0) : -1;
}

std::optional<std::string> Store::live_rune_id(long long mantle, const std::string& name) {
    Stmt q(db_, "SELECT rid FROM runes WHERE mantle=? AND name=? AND deleted=0");
    q.integer(1, mantle).text(2, name);
    if (!q.row()) return std::nullopt;
    return std::string(q.col(0));
}

// ── clusters ─────────────────────────────────────────────────────────────────

ClusterStats Store::cluster(const std::string& mantle, std::uint32_t target, const Progress& progress) {
    ClusterStats out;
    const auto t0 = std::chrono::steady_clock::now();
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;

    // the glyphs declared as measures: their runes are values, not places in the graph
    std::set<std::string> measures;
    {
        Stmt q(db_, "SELECT json FROM top WHERE key='glyphs'");
        if (q.row()) {
            const auto g = nlohmann::json::parse(q.col(0), nullptr, false);
            if (g.is_object())
                for (const auto& [name, d] : g.items())
                    if (d.is_object() && str_of(d, "kind") == "measure") measures.insert(name);
        }
    }
    if (progress) progress(0.05, "Reading the links");
    std::vector<long long> rows;
    std::vector<char> always;
    std::unordered_map<std::string, std::uint32_t> index;
    {
        Stmt q(db_, "SELECT id, name, glyph FROM runes WHERE mantle=? AND deleted=0 ORDER BY pos");
        q.integer(1, mid);
        while (q.row()) {
            index.emplace(std::string(q.col(1)), (std::uint32_t)rows.size());
            rows.push_back(q.col_int(0));
            always.push_back(measures.count(std::string(q.col(2))) ? 1 : 0);
        }
    }
    std::vector<maiz::graph::Edge> edges;
    {
        Stmt q(db_, "SELECT src, dst FROM edges WHERE mantle=? AND deleted=0");
        q.integer(1, mid);
        while (q.row()) {
            const auto a = index.find(std::string(q.col(0))), b = index.find(std::string(q.col(1)));
            if (a == index.end() || b == index.end() || a->second == b->second) continue;
            if (always[a->second] || always[b->second]) continue; // a value edge is not a tie
            edges.push_back({a->second, b->second, 1.0});
        }
    }
    index.clear();
    if (progress) progress(0.3, "Finding the clusters");
    const auto g = maiz::graph::Graph::from_edges((std::uint32_t)rows.size(), edges);
    edges.clear();
    edges.shrink_to_fit();
    maiz::graph::LeidenOptions opts;
    opts.seed = 1; // the same database clusters the same way on every device
    const auto h = maiz::graph::hierarchy(g, target, opts);
    std::vector<std::uint32_t> level1(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) level1[i] = h.path[i].empty() ? 0 : h.path[i][0];
    out.modularity = maiz::graph::modularity(g, level1);
    out.depth = h.depth;

    if (progress) progress(0.8, "Filing the records");
    if (!begin()) return out;
    {
        Stmt del(db_, "DELETE FROM clusters WHERE mantle=?");
        del.integer(1, mid).run();
    }
    std::set<std::string> leaves;
    Stmt add(db_, "INSERT OR REPLACE INTO clusters VALUES(?,?,?,?)");
    for (std::size_t i = 0; i < rows.size(); ++i) {
        std::string path;
        if (always[i]) {
            path = "!"; // sorts before every number: loaded with any cluster
            ++out.always;
        } else {
            for (std::size_t k = 0; k < h.path[i].size(); ++k)
                path += (k ? "." : "") + std::to_string(h.path[i][k]);
            leaves.insert(path);
        }
        add.integer(1, rows[i]).integer(2, mid).text(3, path).integer(4, always[i] ? 1 : 0).run();
    }
    refresh_estimates();
    if (!commit()) {
        rollback();
        return out;
    }
    out.ok = true;
    out.runes = (long long)rows.size();
    out.leaves = (long long)leaves.size();
    out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (progress) progress(1.0, "Filing the records");
    return out;
}

std::vector<ClusterLeaf> Store::leaves(const std::string& mantle) {
    std::vector<ClusterLeaf> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT c.path, COUNT(*), SUM(r.bytes) FROM clusters c JOIN runes r ON r.id=c.rune"
                " WHERE c.mantle=? AND r.deleted=0 AND c.always=0 GROUP BY c.path ORDER BY c.path");
    q.integer(1, mid);
    while (q.row()) out.push_back({std::string(q.col(0)), q.col_int(1), q.col_int(2)});
    return out;
}

bool Store::clustered(const std::string& mantle) {
    const long long mid = mantle_row(mantle);
    if (mid < 0) return false;
    Stmt q(db_, "SELECT 1 FROM clusters WHERE mantle=? LIMIT 1");
    q.integer(1, mid);
    return q.row();
}

std::string Store::cluster_of(const std::string& mantle, const std::string& rune) {
    const long long mid = mantle_row(mantle);
    Stmt q(db_, "SELECT c.path FROM runes r JOIN clusters c ON c.rune=r.id WHERE r.mantle=? AND r.name=?"
                " AND r.deleted=0");
    q.integer(1, mid).text(2, rune);
    return q.row() ? std::string(q.col(0)) : std::string();
}

void Store::assign_cluster(const std::string& mantle, const std::string& rune, const std::string& path) {
    const long long mid = mantle_row(mantle);
    Stmt q(db_, "INSERT OR REPLACE INTO clusters SELECT id, mantle, ?3, 0 FROM runes"
                " WHERE mantle=?1 AND name=?2 AND deleted=0");
    q.integer(1, mid).text(2, rune).text(3, path).run();
}

Page Store::page(const std::string& mantle, long long offset, std::uint64_t byte_cap, long long max_runes) {
    Page out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT name, bytes FROM runes WHERE mantle=? AND deleted=0 ORDER BY pos LIMIT -1 OFFSET ?");
    q.integer(1, mid).integer(2, offset);
    while (q.row()) {
        const auto b = (std::uint64_t)q.col_int(1);
        if (!out.names.empty() && out.bytes + b > byte_cap) break;
        out.names.emplace_back(q.col(0));
        out.bytes += b;
        if (max_runes > 0 && (long long)out.names.size() >= max_runes) break;
    }
    return out;
}

void Store::refresh_estimates() {
    exec("DELETE FROM est_mantles; DELETE FROM est_leaves;"
         "INSERT INTO est_mantles SELECT m.id,"
         " (SELECT COUNT(*) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
         " (SELECT COUNT(*) FROM edges e WHERE e.mantle=m.id AND e.deleted=0),"
         " (SELECT COALESCE(SUM(bytes),0) FROM runes r WHERE r.mantle=m.id AND r.deleted=0),"
         " (SELECT COALESCE(SUM(bytes),0) FROM edges e WHERE e.mantle=m.id AND e.deleted=0)"
         " FROM mantles m WHERE m.deleted=0;"
         "INSERT INTO est_leaves SELECT c.mantle, c.path, COUNT(*), SUM(r.bytes) FROM clusters c"
         " JOIN runes r ON r.id=c.rune WHERE r.deleted=0 AND c.always=0 GROUP BY c.mantle, c.path;");
}

std::vector<MantleInfo> Store::estimates() {
    std::vector<MantleInfo> out;
    if (!db_) return out;
    std::vector<std::pair<long long, std::string>> unknown;
    {
        Stmt q(db_, "SELECT m.id, m.name, e.runes, e.edges, e.rune_bytes, e.edge_bytes FROM mantles m"
                    " LEFT JOIN est_mantles e ON e.mantle=m.id WHERE m.deleted=0 ORDER BY m.pos");
        while (q.row()) {
            if (q.is_null(2)) {
                unknown.emplace_back(q.col_int(0), std::string(q.col(1)));
                out.push_back({std::string(q.col(1))});
                continue;
            }
            out.push_back({std::string(q.col(1)), q.col_int(2), q.col_int(3), q.col_int(4) + q.col_int(5), q.col_int(4)});
        }
    }
    for (const auto& [id, name] : unknown) { // a mantle made since: small, as a rule; counted exactly
        Stmt q(db_, "SELECT (SELECT COUNT(*) FROM runes WHERE mantle=?1 AND deleted=0),"
                    " (SELECT COUNT(*) FROM edges WHERE mantle=?1 AND deleted=0),"
                    " (SELECT COALESCE(SUM(bytes),0) FROM runes WHERE mantle=?1 AND deleted=0),"
                    " (SELECT COALESCE(SUM(bytes),0) FROM edges WHERE mantle=?1 AND deleted=0)");
        q.integer(1, id);
        if (!q.row()) continue;
        for (auto& m : out)
            if (m.name == name) m = {name, q.col_int(0), q.col_int(1), q.col_int(2) + q.col_int(3), q.col_int(2)};
    }
    return out;
}

std::vector<ClusterLeaf> Store::leaf_estimates(const std::string& mantle) {
    std::vector<ClusterLeaf> out;
    const long long mid = mantle_row(mantle);
    if (mid < 0) return out;
    Stmt q(db_, "SELECT path, runes, bytes FROM est_leaves WHERE mantle=? ORDER BY path");
    q.integer(1, mid);
    while (q.row()) out.push_back({std::string(q.col(0)), q.col_int(1), q.col_int(2)});
    if (out.empty() && clustered(mantle)) return leaves(mantle); // clustered before the estimates existed
    return out;
}

bool Store::vacuum_into(const std::filesystem::path& out) {
    if (!db_) return false;
    Stmt q(db_, "VACUUM INTO ?");
    q.text(1, out.string());
    if (q.run()) return true;
    error_ = sqlite3_errmsg(db_);
    return false;
}

std::string Store::meta(const std::string& key) {
    if (!db_) return {};
    Stmt q(db_, "SELECT value FROM store_meta WHERE key=?");
    q.text(1, key);
    return q.row() ? std::string(q.col(0)) : std::string();
}

void Store::set_meta(const std::string& key, const std::string& value) {
    if (!db_) return;
    Stmt q(db_, "INSERT OR REPLACE INTO store_meta VALUES(?,?)");
    q.text(1, key).text(2, value).run();
}

bool Store::has_document() {
    if (!db_) return false;
    Stmt q(db_, "SELECT 1 FROM top LIMIT 1");
    return q.row();
}

} // namespace hormiga::store
