/* miga.cpp — pack/unpack the .miga v3 database bundle. See miga.hpp.
 *
 * ── WHY THIS IS A SCANNER AND NOT A JSON PARSE (2026-10-24) ────────────────
 *
 * Issue #1 and #2 (a client opening a database two orders of magnitude past the
 * documented scale): opening froze the window. Measured on a 208 MB bundle
 * (10^5 runes, 400 pictures): 3.1 s in `open` alone, 6.2 s in `pack`, all on the
 * UI thread. The time went to building a JSON DOM of the WHOLE file -- every
 * picture's base64 copied into a json::string -- then dumping the state back out
 * as text for Void Core to parse again, and rewriting every picture whether or
 * not it had changed. Three copies of the file in memory, two parses of the
 * state, one write per picture per open.
 *
 * A bundle is a JSON object whose bulk is (a) base64 strings, which contain no
 * quote and no backslash, and (b) one state document Void Core wants AS TEXT.
 * So `open` reads the file once into one buffer and walks it with a tokenizer
 * that only records where things are: the state's byte range (handed to Void
 * Core as-is, no parse, no dump) and each picture's base64 range (decoded
 * straight out of the buffer, in parallel). The small members -- version, meta
 * -- are parsed properly. Anything the scanner does not recognise falls back to
 * the DOM path that was here before, so an odd but valid file still opens.
 *
 * `pack` is the same idea in reverse: stream each picture out as it is encoded,
 * and write the state as the text it already is.
 *
 * Progress is reported in BYTES of work, weighted by what each byte measurably
 * costs (see kW* below), so the bar moves at the rate time does. */
#include "platform/miga.hpp"

#include "json.hpp"
#include <sodium.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <sstream>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;

namespace hormiga::miga {
namespace {

/* ── THE WORK MODEL ───────────────────────────────────────────────────────────
 * Relative cost of one byte in each phase, measured with tests/miga_bench.cpp on
 * the 208 MB bundle (2026-10-24, Windows, NVMe). The bar's fraction is
 *     F = sum over finished work of w_phase * bytes / sum over all work of the same,
 * which is the elapsed-time fraction exactly when these weights are each phase's
 * seconds-per-byte. They need only be right in RATIO, and only roughly: Harrison
 * et al. (UIST 2007) found people judge a bar by its steadiness more than by its
 * end, and a bar that is linear in time is the honest baseline. */
constexpr double kWRead = 1.0;     // per byte of the file read from disk
constexpr double kWExtract = 1.6;  // per byte of base64 decoded, compared, written
constexpr double kWState = 0.15;   // per byte of state handed over
constexpr double kWEncode = 2.2;   // pack: per byte of a picture encoded and written

std::string b64_encode(const unsigned char* bin, size_t len) {
    size_t n = sodium_base64_encoded_len(len, sodium_base64_VARIANT_ORIGINAL);
    std::string out(n, '\0');
    sodium_bin2base64(out.data(), n, bin, len, sodium_base64_VARIANT_ORIGINAL);
    if (!out.empty() && out.back() == '\0') out.pop_back(); // drop the NUL term
    return out;
}

bool b64_decode(const char* s, size_t n, std::vector<unsigned char>& out) {
    out.resize(n); // decoded is always shorter
    size_t real = 0;
    if (sodium_base642bin(out.data(), out.size(), s, n, nullptr, &real, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0)
        return false;
    out.resize(real);
    return true;
}

std::vector<unsigned char> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::string iso_now() {
    std::time_t t = std::time(nullptr);
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return b;
}

/* A monotone reporter: never goes backwards, never reports twice per ~10 ms, and
 * clamps to [0, 1] whatever the weights say. */
class Reporter {
public:
    Reporter(const ProgressFn& fn, double total) : fn_(fn), total_(total > 0 ? total : 1) {}
    void at(double done, const char* phase, bool force = false) {
        if (!fn_) return;
        const double f = std::clamp(done / total_, last_, 1.0);
        const auto now = std::chrono::steady_clock::now();
        if (!force && phase == phase_ && f - last_ < 0.002 &&
            now - when_ < std::chrono::milliseconds(10))
            return;
        last_ = f;
        when_ = now;
        phase_ = phase;
        fn_(f, phase);
    }

private:
    const ProgressFn& fn_;
    double total_, last_ = 0;
    const char* phase_ = nullptr;
    std::chrono::steady_clock::time_point when_{};
};

/* Where a bundle key lands. `assets/<rest>` goes to THIS database's assets
 * folder, wherever that now is; anything else stays base-relative. A key that
 * climbs out (`..`) or is absolute is refused: a bundle is a file somebody handed
 * you, and `base / "/etc/x"` is `/etc/x`. */
bool dest_for(const std::string& rel, const fs::path& base_dir, const fs::path& assets_dir,
              fs::path& out) {
    if (rel.empty() || rel.find("..") != std::string::npos) return false;
    const fs::path p(rel);
    if (p.is_absolute() || p.has_root_name() || p.has_root_directory()) return false;
    out = rel.rfind("assets/", 0) == 0 ? assets_dir / rel.substr(7) : base_dir / rel;
    return true;
}

// ── the scanner ─────────────────────────────────────────────────────────────

struct Span {
    size_t b = 0, e = 0; // [b, e) into the text
};

struct Scanned {
    bool ok = false;
    bool has_state = false, state_is_string = false;
    Span state;
    int version = 0;
    std::string name;
    struct Asset {
        std::string key;
        Span b64;
    };
    std::vector<Asset> assets;
};

class Cursor {
public:
    explicit Cursor(const std::string& t) : t_(t) {}
    size_t pos() const { return i_; }
    void ws() {
        while (i_ < t_.size() && (t_[i_] == ' ' || t_[i_] == '\n' || t_[i_] == '\r' || t_[i_] == '\t')) ++i_;
    }
    bool peek(char c) { ws(); return i_ < t_.size() && t_[i_] == c; }
    bool eat(char c) {
        if (!peek(c)) return false;
        ++i_;
        return true;
    }
    /* A string's CONTENT range, without the quotes. memchr to the next quote,
     * then count the backslashes before it: an odd count means it was escaped. */
    bool string(Span& s, bool& escaped) {
        if (!eat('"')) return false;
        s.b = i_;
        escaped = false;
        for (;;) {
            const void* q = std::memchr(t_.data() + i_, '"', t_.size() - i_);
            if (!q) return false;
            size_t j = (const char*)q - t_.data();
            size_t k = j;
            while (k > s.b && t_[k - 1] == '\\') --k;
            if ((j - k) % 2 == 0) {
                if (std::memchr(t_.data() + s.b, '\\', j - s.b)) escaped = true;
                s.e = j;
                i_ = j + 1;
                return true;
            }
            i_ = j + 1; // an escaped quote: keep looking
        }
    }
    std::string text_of(const Span& s, bool escaped) const {
        if (!escaped) return t_.substr(s.b, s.e - s.b);
        try {
            return json::parse(t_.substr(s.b - 1, s.e - s.b + 2)).get<std::string>();
        } catch (...) {
            return {};
        }
    }
    /* Step over one value of any kind, honouring strings inside containers. */
    bool skip_value() {
        ws();
        if (i_ >= t_.size()) return false;
        const char c = t_[i_];
        if (c == '"') {
            Span s;
            bool esc;
            return string(s, esc);
        }
        if (c == '{' || c == '[') {
            int depth = 0;
            while (i_ < t_.size()) {
                const char d = t_[i_];
                if (d == '"') {
                    Span s;
                    bool esc;
                    if (!string(s, esc)) return false;
                    continue;
                }
                if (d == '{' || d == '[') ++depth;
                else if (d == '}' || d == ']') {
                    if (--depth == 0) { ++i_; return true; }
                }
                ++i_;
            }
            return false;
        }
        while (i_ < t_.size() && t_[i_] != ',' && t_[i_] != '}' && t_[i_] != ']' && t_[i_] != ' ' &&
               t_[i_] != '\n' && t_[i_] != '\r' && t_[i_] != '\t')
            ++i_;
        return true;
    }

private:
    const std::string& t_;
    size_t i_ = 0;
};

Scanned scan(const std::string& text) {
    Scanned r;
    Cursor c(text);
    if (!c.eat('{')) return r;
    if (c.eat('}')) { r.ok = true; return r; }
    for (;;) {
        Span ks;
        bool kesc;
        if (!c.string(ks, kesc)) return r;
        const std::string key = c.text_of(ks, kesc);
        if (!c.eat(':')) return r;
        c.ws();
        if (key == "assets") {
            if (!c.eat('{')) {
                if (!c.skip_value()) return r; // not an object: ignored, as the DOM path did
            } else if (!c.eat('}')) {
                for (;;) {
                    Span as, vs;
                    bool aesc, vesc;
                    if (!c.string(as, aesc) || !c.eat(':')) return r;
                    c.ws();
                    if (!c.peek('"') || !c.string(vs, vesc) || vesc) return r; // base64 has no escapes
                    r.assets.push_back({c.text_of(as, aesc), vs});
                    if (c.eat(',')) continue;
                    if (!c.eat('}')) return r;
                    break;
                }
            }
        } else if (key == "state") {
            c.ws();
            r.has_state = true;
            r.state_is_string = c.peek('"');
            r.state.b = c.pos();
            if (!c.skip_value()) return r;
            r.state.e = c.pos();
        } else if (key == "version" || key == "meta") {
            const size_t b = c.pos();
            if (!c.skip_value()) return r;
            try {
                const json v = json::parse(text.substr(b, c.pos() - b));
                if (key == "version" && v.is_number_integer()) r.version = v.get<int>();
                if (key == "meta" && v.is_object()) r.name = v.value("name", std::string());
            } catch (...) {
                return r;
            }
        } else if (!c.skip_value()) {
            return r;
        }
        if (c.eat(',')) continue;
        if (!c.eat('}')) return r;
        break;
    }
    r.ok = true;
    return r;
}

/* The DOM path that was the whole implementation before 2026-10-24, kept for a
 * bundle the scanner does not recognise. Same results, no parallelism, coarse
 * progress. */
OpenResult open_dom(const std::string& text, const fs::path& base_dir, const fs::path& assets_dir,
                    Reporter& rep, double done) {
    OpenResult r;
    json env;
    try {
        env = json::parse(text);
    } catch (...) {
        r.error = "not a readable .miga (parse failed)";
        return r;
    }
    r.version = env.value("version", 0);
    r.name = env.value("meta", json::object()).value("name", std::string());
    if (r.version < 3 || !env.contains("state")) {
        r.version = env.value("version", 2);
        r.error = "legacy .miga (v2, secrets only) — no full database inside";
        return r;
    }
    std::error_code ec;
    if (env.contains("assets") && env["assets"].is_object()) {
        for (auto& [rel, b64] : env["assets"].items()) {
            fs::path dest;
            if (!dest_for(rel, base_dir, assets_dir, dest) || !b64.is_string()) continue;
            fs::create_directories(dest.parent_path(), ec);
            const std::string& s = b64.get_ref<const std::string&>();
            std::vector<unsigned char> bytes;
            b64_decode(s.data(), s.size(), bytes);
            std::ofstream of(dest, std::ios::binary | std::ios::trunc);
            of.write((const char*)bytes.data(), (std::streamsize)bytes.size());
            ++r.assets;
            done += kWExtract * (double)s.size();
            rep.at(done, "Restoring pictures");
        }
    }
    r.state = env["state"].is_string() ? env["state"].get<std::string>() : env["state"].dump();
    r.ok = true;
    return r;
}

} // namespace

PackResult pack(const std::string& state_json, const fs::path& base_dir,
                const fs::path& out_path, const std::string& name,
                const fs::path& assets_dir,
                const std::map<std::string, fs::path>& extra,
                const std::set<std::string>& skip,
                const ProgressFn& progress) {
    PackResult r;
    // what goes in, keyed and ordered, so the file is deterministic
    std::map<std::string, fs::path> files;
    std::error_code ec;
    const fs::path& adir = assets_dir;
    if (fs::exists(adir)) {
        for (auto it = fs::recursive_directory_iterator(adir, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file()) continue;
            /* THE KEY KEEPS THE FOLDER'S REAL NAME when the assets folder is
             * inside the database — `assets/cat-01.jpg`, or `demo-assets/…` for
             * a database whose photos live there. That matters because image
             * fields in the document are stored relative to the DATABASE
             * (`stage_site_asset` resolves `base_dir / rel`), so a bundle that
             * renamed the folder on the way in would restore fifty photos the
             * document could no longer find. For the default folder this is
             * byte-identical to the old `relative(path, base_dir)`, so bundles
             * written before any of this still open.
             *
             * Only an assets folder OUTSIDE the database falls back to a fixed
             * `assets/` prefix — there is no base-relative name to keep, and
             * `open` puts those back wherever the opener's assets folder is. */
            std::error_code rec;
            const std::string under =
                fs::relative(it->path(), base_dir, rec).generic_string();
            const bool inside =
                !rec && !under.empty() && under.rfind("..", 0) != 0;
            std::string rel =
                inside ? under
                       : ("assets" / fs::relative(it->path(), adir, rec))
                             .generic_string();
            if (rel.empty() || rel == "assets" || skip.count(rel)) continue;
            files[rel] = it->path();
        }
    }
    /* ── AND EVERY FILE THE MODEL POINTS AT FROM ANYWHERE ELSE (2026-09-03) ──
     *
     * The assets walk above is "back up the folder of originals", and it was the
     * whole of the bundle until a `download.file` pointed at a document in a
     * folder beside the database. The render staged it, the deploy published
     * it, and the bundle silently did not contain it — so opening that bundle
     * anywhere else gave "This file is not available", correctly and
     * unhelpfully: the site was correct, the bundle was correct, and the
     * combination was broken.
     *
     * `extra` is resolved by the CALLER (`HormigaApp::referenced_files`), which
     * has the glyph declarations this file deliberately does not: which fields
     * are files is a property of the model, and a copy of that knowledge here
     * would be a second place to keep it right. See app/paths.cpp, including
     * why a deploy token does not arrive in this map.
     *
     * Keyed by the model's own relative string, so `open` restores each file
     * exactly where the field expects it. An entry already bundled by the
     * assets walk is left alone — the folder version is the same bytes, and
     * counting it twice would make the report lie. */
    for (const auto& [rel, src] : extra) {
        if (files.count(rel) || skip.count(rel)) continue;
        std::error_code fe;
        if (!fs::is_regular_file(src, fe)) continue;
        files[rel] = src;
        r.beyond_assets.push_back(rel);
    }

    double total = kWState * (double)state_json.size();
    for (const auto& [_, p] : files) {
        std::error_code se;
        total += kWEncode * (double)fs::file_size(p, se);
    }
    Reporter rep(progress, total);
    double done = 0;
    rep.at(0, "Packing pictures", true);

    // write-temp-then-rename: a failed write never corrupts the current bundle
    fs::path tmp = out_path;
    tmp += ".part";
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!o) { r.error = "cannot write " + tmp.string(); return r; }
        const json meta = {{"name", name}, {"app", "hormiga"}, {"saved", iso_now()}};
        const json rebuild = {{"tiles", "re-fetch from the map source node"},
                              {"site", "re-render from the document mantles"}};
        o << "{\"magic\":\"MIGA\",\"version\":3,\"meta\":" << meta.dump()
          << ",\"rebuild\":" << rebuild.dump() << ",\"assets\":{";
        bool first = true;
        constexpr size_t kChunk = 3 * 1024 * 1024; // a multiple of 3: no padding mid-file
        for (const auto& [rel, p] : files) {
            const std::vector<unsigned char> bin = read_file(p);
            o << (first ? "" : ",") << json(rel).dump() << ":\"";
            first = false;
            for (size_t at = 0; at < bin.size(); at += kChunk) {
                const size_t n = std::min(kChunk, bin.size() - at);
                o << b64_encode(bin.data() + at, n);
                done += kWEncode * (double)n;
                rep.at(done, "Packing pictures");
            }
            o << '"';
            ++r.assets;
        }
        /* The state is JSON already (Void Core exported it), so it is written as
         * the text it is. The old path parsed it into a DOM only to dump it
         * again; it kept a non-JSON state as a string rather than lose it, and
         * so does this, by the cheap test of what it starts and ends with. */
        const size_t sb = state_json.find_first_not_of(" \t\r\n");
        const size_t se = state_json.find_last_not_of(" \t\r\n");
        const bool object = sb != std::string::npos && state_json[sb] == '{' && state_json[se] == '}';
        o << "},\"state\":";
        rep.at(done, "Writing the database", true);
        if (object) o << state_json;
        else o << json(state_json).dump();
        o << "}";
        if (!o) { r.error = "write failed"; return r; }
    }
    fs::rename(tmp, out_path, ec);
    if (ec) { r.error = ec.message(); fs::remove(tmp); return r; }
    r.bytes = (long long)fs::file_size(out_path, ec);
    rep.at(total, "Saved", true);
    r.ok = true;
    return r;
}

OpenResult open(const fs::path& miga_path, const fs::path& base_dir,
                const fs::path& assets_dir, const OpenOptions& options) {
    OpenResult r;
    std::error_code ec;
    const auto size = fs::file_size(miga_path, ec);
    std::ifstream in(miga_path, std::ios::binary);
    if (!in || ec) { r.error = "cannot open " + miga_path.string(); return r; }

    /* The total is an estimate until the scan has measured the assets: the file
     * is mostly base64, so every byte is assumed to be extracted, and the state's
     * share is corrected once its size is known. Overestimating is the safe
     * direction: the bar can only go up. */
    double total = (kWRead + kWExtract) * (double)size;
    Reporter rep(options.progress, total);
    rep.at(0, "Reading the file", true);

    std::string text;
    text.resize((size_t)size);
    constexpr size_t kChunk = 16 * 1024 * 1024;
    for (size_t at = 0; at < text.size();) {
        const size_t n = std::min(kChunk, text.size() - at);
        in.read(text.data() + at, (std::streamsize)n);
        if (!in) { r.error = "could not read " + miga_path.string(); return r; }
        at += n;
        rep.at(kWRead * (double)at, "Reading the file");
    }
    double done = kWRead * (double)size;

    const Scanned s = scan(text);
    if (!s.ok) return open_dom(text, base_dir, assets_dir, rep, done);

    r.version = s.version;
    r.name = s.name;
    if (r.version < 3 || !s.has_state) {
        r.version = r.version ? r.version : 2;
        r.error = "legacy .miga (v2, secrets only) — no full database inside";
        return r;
    }

    /* ── EXTRACT, IN PARALLEL, AND ONLY WHAT DIFFERS ──────────────────────────
     * Each picture is independent: decode it out of the buffer, and if a file
     * with the same bytes is already at the destination, leave it (no write, no
     * new mtime, nothing for a virus scanner to re-read). Comparing costs a
     * read; writing costs a write, and on Windows a write is the expensive one.
     * Workers count bytes; this thread reports, so the callback is never called
     * from a worker. */
    size_t b64_total = 0;
    for (const auto& a : s.assets) b64_total += a.b64.e - a.b64.b;
    total = done + kWExtract * (double)b64_total + kWState * (double)(s.state.e - s.state.b);
    Reporter rep2(options.progress, total);
    rep2.at(done, "Restoring pictures", true);

    std::vector<fs::path> dests(s.assets.size());
    std::vector<char> usable(s.assets.size(), 0);
    for (size_t i = 0; i < s.assets.size(); ++i)
        usable[i] = dest_for(s.assets[i].key, base_dir, assets_dir, dests[i]) ? 1 : 0;

    std::atomic<size_t> next{0}, bytes_done{0};
    std::atomic<int> written{0}, kept{0};
    std::mutex err_mu;
    std::string first_error;
    const auto work = [&] {
        std::vector<unsigned char> bin, have;
        for (;;) {
            const size_t i = next.fetch_add(1);
            if (i >= s.assets.size()) return;
            const auto& a = s.assets[i];
            const size_t n = a.b64.e - a.b64.b;
            if (usable[i]) {
                std::error_code e;
                fs::create_directories(dests[i].parent_path(), e);
                if (!b64_decode(text.data() + a.b64.b, n, bin)) {
                    std::lock_guard<std::mutex> g(err_mu);
                    if (first_error.empty()) first_error = "a damaged picture in the bundle: " + a.key;
                } else {
                    bool same = false;
                    if (fs::file_size(dests[i], e) == bin.size() && !e) {
                        std::ifstream f(dests[i], std::ios::binary);
                        have.resize(bin.size());
                        same = f.read((char*)have.data(), (std::streamsize)have.size()) &&
                               std::memcmp(have.data(), bin.data(), bin.size()) == 0;
                    }
                    if (same) {
                        ++kept;
                    } else {
                        std::ofstream of(dests[i], std::ios::binary | std::ios::trunc);
                        of.write((const char*)bin.data(), (std::streamsize)bin.size());
                        if (!of) {
                            std::lock_guard<std::mutex> g(err_mu);
                            if (first_error.empty()) first_error = "could not write " + dests[i].string();
                        } else {
                            ++written;
                        }
                    }
                }
            }
            bytes_done += n;
        }
    };
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const size_t nthreads = std::min<size_t>({8, hw, std::max<size_t>(1, s.assets.size())});
    std::vector<std::thread> pool; // with more than one, this thread only reports
    if (nthreads > 1)
        for (size_t t = 0; t < nthreads; ++t) pool.emplace_back(work);
    if (pool.empty()) {
        work();
    } else {
        while (next.load() < s.assets.size()) {
            rep2.at(done + kWExtract * (double)bytes_done.load(), "Restoring pictures");
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    }
    for (auto& t : pool) t.join();
    done += kWExtract * (double)b64_total;
    r.assets = written.load();
    r.assets_kept = kept.load();
    if (!first_error.empty()) { r.error = first_error; return r; }

    /* MIRROR: the assets folder becomes exactly the bundle's. Only regular files
     * inside the assets folder are considered; nothing outside it is touched. */
    if (options.mirror_assets && fs::exists(assets_dir, ec)) {
        std::unordered_set<std::string> keep;
        for (size_t i = 0; i < dests.size(); ++i)
            if (usable[i]) keep.insert(dests[i].lexically_normal().generic_string());
        std::vector<fs::path> gone;
        for (auto it = fs::recursive_directory_iterator(assets_dir, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
            if (it->is_regular_file() && !keep.count(it->path().lexically_normal().generic_string()))
                gone.push_back(it->path());
        for (const auto& p : gone) {
            std::error_code re;
            if (fs::remove(p, re)) ++r.assets_removed;
        }
    }

    rep2.at(done, "Reading the database", true);
    if (s.state_is_string) {
        try {
            r.state = json::parse(text.substr(s.state.b, s.state.e - s.state.b)).get<std::string>();
        } catch (...) {
            r.error = "the bundle's state is damaged";
            return r;
        }
    } else {
        r.state.assign(text, s.state.b, s.state.e - s.state.b);
    }
    rep2.at(total, "Reading the database", true);
    r.ok = true;
    return r;
}

bool looks_like_miga(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    char head[8] = {};
    in.read(head, 7);
    // v3/v2 both start with a JSON object containing "magic":"MIGA"
    std::string s(head);
    return s.find('{') != std::string::npos; // cheap; full check is in open()
}

} // namespace hormiga::miga
