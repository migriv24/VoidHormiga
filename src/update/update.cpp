/* update/update.cpp — the update client's decisions and its one network call.
 * The contract, and why the split is where it is, is in update.hpp. */
#include "update/update.hpp"

#include <sodium.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX   // the ucrt64 headers already define it; redefining warns
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>

#include "json.hpp"

namespace hormiga::update {
namespace {

using nlohmann::json;

/* Shell-quote a PATH. Paths only, and only ones this process built out of
 * `temp_directory_path()` plus a filename `safe_filename` already vetted — the
 * URL is quoted the same way but is refused outright if it contains anything a
 * shell could read, because escaping is the wrong tool when the alternative is
 * available. `publish/http.hpp` has the same three-line function; it is not
 * shared, so that `src/update/` depends on nothing but the standard library,
 * libsodium and the vendored JSON. A folder that is meant to keep working when
 * the rest of the application does not should not link the deploy holidays. */
std::string q(const std::string& s) { return "\"" + s + "\""; }

std::string str_of(const json& o, const char* key) {
    const auto it = o.find(key);
    return (it != o.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

} // namespace

/* ISO-8601 UTC, to the second. Informational: it is in the preferences file so
 * a person (or an agent reading it) can see when we last went to the network,
 * which is the sort of thing that should be inspectable when the rule is
 * "never check without being asked to". */
std::string now_iso8601() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

namespace {

/* An installer URL is about to be handed to a shell. `curl` is given the URL as
 * a quoted argument, but a URL carrying a quote, a backslash or a newline is
 * not a URL — it is an attempt, and the feed is a file fetched over the
 * network. Refuse rather than escape: there is no legitimate release whose
 * artifact URL needs any of these, so the check costs nothing real and the
 * alternative is trusting a quoting function with a command line that installs
 * software. Same reason `link --weight` refuses instead of coercing. */
bool safe_url(const std::string& u) {
    if (u.size() < 8 || u.size() > 2000) return false;
    if (u.rfind("https://", 0) != 0) return false; // no plaintext, ever
    for (const unsigned char c : u) {
        if (c < 0x21 || c > 0x7e) return false;  // control chars, space, non-ASCII
        if (c == '"' || c == '\'' || c == '\\' || c == '`' ||
            c == '$' || c == '&' || c == '|' || c == ';' || c == '<' || c == '>')
            return false;
    }
    return true;
}

/* A filename from the feed becomes a path on this machine. Anything with a
 * separator or a `..` in it is refused for the obvious reason. */
bool safe_filename(const std::string& f) {
    if (f.empty() || f.size() > 200) return false;
    if (f.find("..") != std::string::npos) return false;
    for (const unsigned char c : f) {
        if (c == '/' || c == '\\' || c == ':' || c < 0x20 || c > 0x7e) return false;
        if (c == '"' || c == '\'' || c == '`' || c == '$' || c == '&' ||
            c == '|' || c == ';' || c == '<' || c == '>' || c == '*' || c == '?')
            return false;
    }
    return true;
}

bool valid_sha256(const std::string& h) {
    if (h.size() != 64) return false;
    for (const char c : h)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    return true;
}

} // namespace

// ── VERSIONS ────────────────────────────────────────────────────────────────

int compare_versions(const std::string& a, const std::string& b) {
    auto split = [](const std::string& v) {
        std::vector<std::string> parts;
        std::string cur;
        for (const char c : v) {
            if (c == '.') { parts.push_back(cur); cur.clear(); }
            else cur += c;
        }
        parts.push_back(cur);
        return parts;
    };
    const auto pa = split(a), pb = split(b);
    for (size_t i = 0; i < pa.size() || i < pb.size(); ++i) {
        const std::string sa = i < pa.size() ? pa[i] : "0";
        const std::string sb = i < pb.size() ? pb[i] : "0";
        /* Numeric while both sides are numeric — "10" must beat "9", which is
         * the one thing a lexical compare gets wrong and the one way this can
         * fail dangerously (reporting a newer release as older, so nobody is
         * ever told about it). A non-numeric component ("1-rc2") falls back to
         * a string compare of the whole component. */
        const bool na = !sa.empty() && sa.find_first_not_of("0123456789") == std::string::npos;
        const bool nb = !sb.empty() && sb.find_first_not_of("0123456789") == std::string::npos;
        if (na && nb) {
            const long long va = std::atoll(sa.c_str()), vb = std::atoll(sb.c_str());
            if (va != vb) return va < vb ? -1 : 1;
        } else if (sa != sb) {
            return sa < sb ? -1 : 1;
        }
    }
    return 0;
}

// ── THE FEED ────────────────────────────────────────────────────────────────

Feed parse_feed(const std::string& text, const std::string& app,
                const std::string& platform) {
    Feed f;
    json d = json::parse(text, nullptr, false);
    if (d.is_discarded() || !d.is_object()) {
        f.error = "the update feed is not JSON";
        return f;
    }
    const std::string kind = str_of(d, "feed");
    if (kind.rfind("void-updates/", 0) != 0) {
        /* Named rather than guessed at. A feed URL that has quietly become a
         * login page, a 404 body or somebody else's document is the common case
         * here, and "not JSON we recognise" is a better sentence than a silent
         * "you are up to date". */
        f.error = "not a void-updates document (found \"" +
                  (kind.empty() ? std::string("no `feed` key") : kind) + "\")";
        return f;
    }
    f.publisher = str_of(d, "publisher");
    f.generated = str_of(d, "generated");

    const auto apps = d.find("applications");
    if (apps == d.end() || !apps->is_object() || !apps->contains(app)) {
        f.error = "the feed lists no application called \"" + app + "\"";
        return f;
    }
    const json& me = (*apps)[app];
    f.display_name = str_of(me, "display_name");
    f.latest = str_of(me, "latest");
    if (f.latest.empty()) {
        f.error = "the feed's entry for \"" + app + "\" has no `latest`";
        return f;
    }

    const auto rels = me.find("releases");
    if (rels != me.end() && rels->is_array()) {
        for (const json& r : *rels) {
            if (!r.is_object()) continue;
            Release rel;
            rel.version = str_of(r, "version");
            if (rel.version.empty()) continue;
            rel.date = str_of(r, "date");
            rel.change = str_of(r, "change");
            rel.summary = str_of(r, "summary");
            if (const auto a = r.find("adds"); a != r.end() && a->is_array())
                for (const json& s : *a)
                    if (s.is_string()) rel.adds.push_back(s.get<std::string>());
            if (const auto b = r.find("behavior_changes");
                b != r.end() && b->is_array())
                for (const json& c : *b)
                    if (c.is_object())
                        rel.behavior_changes.push_back(
                            {str_of(c, "what"), str_of(c, "who_is_affected")});
            /* Only OUR platform's artifact is carried out of the parse. A
             * release with none is still a release — it is reported and
             * offered as unavailable rather than hidden, because "there is a
             * 0.1.1 and it has no Windows build" is a true and useful thing to
             * be able to say. */
            if (const auto arts = r.find("artifacts");
                arts != r.end() && arts->is_object() && arts->contains(platform)) {
                const json& a = (*arts)[platform];
                if (a.is_object()) {
                    rel.file = str_of(a, "file");
                    rel.url = str_of(a, "url");
                    rel.sha256 = str_of(a, "sha256");
                    if (const auto s = a.find("signature");
                        s != a.end() && s->is_string())
                        rel.signature = s->get<std::string>();
                    if (const auto by = a.find("bytes");
                        by != a.end() && by->is_number())
                        rel.bytes = by->get<long long>();
                    rel.has_artifact = !rel.url.empty() && !rel.file.empty();
                }
            }
            f.releases.push_back(std::move(rel));
        }
    }
    f.ok = true;
    return f;
}

Offer decide(const Feed& feed, const std::string& current,
             const std::string& skip_version) {
    Offer o;
    o.current = current;
    if (!feed.ok || feed.latest.empty()) return o;
    if (compare_versions(feed.latest, current) <= 0) return o;

    for (const Release& r : feed.releases)
        if (r.version == feed.latest) o.release = r;
    if (o.release.version.empty()) o.release.version = feed.latest;

    /* `skip_version` is "not this one", not "never again": once something newer
     * than the skipped version appears, the skip has expired and the person is
     * told. A skip that outlived its release would be an off-switch nobody
     * remembers flipping. */
    if (!skip_version.empty() &&
        compare_versions(feed.latest, skip_version) <= 0) {
        o.skipped = true;
        return o;
    }
    o.available = true;
    return o;
}

std::string describe(const Offer& o) {
    if (o.release.version.empty()) return {};
    const Release& r = o.release;
    std::ostringstream s;
    s << "Hormiga " << r.version;
    if (!r.date.empty()) s << "  (" << r.date << ")";
    s << "\nyou are running " << o.current << "\n";
    if (!r.summary.empty()) s << "\n" << r.summary << "\n";
    if (!r.adds.empty()) {
        s << "\nadds\n";
        for (const auto& a : r.adds) s << "  - " << a << "\n";
    }
    if (!r.behavior_changes.empty()) {
        s << "\nthings that behave differently\n";
        for (const auto& b : r.behavior_changes) {
            s << "  - " << b.what << "\n";
            if (!b.who_is_affected.empty())
                s << "      affects: " << b.who_is_affected << "\n";
        }
    }
    if (!r.has_artifact)
        s << "\nno " << platform_tag() << " build is listed for this release.\n";
    else if (r.signature.empty())
        /* Said out loud rather than left to the reader. Mago's manifest field
         * says `ed25519` and nothing implements it yet; the feed carries
         * `"signature": null` — present so we can code against it, null so its
         * absence is not mistaken for a decision. `sha256` proves the bytes
         * arrived intact and proves nothing at all about who made them. */
        s << "\nthis release is not signed: the checksum proves the download "
             "arrived intact,\nnot who built it.\n";
    return s.str();
}

// ── PREFERENCES ─────────────────────────────────────────────────────────────

const char* ask_name(Ask a) {
    switch (a) {
        case Ask::Never: return "never";
        case Ask::Startup: return "startup";
        default: return "unasked";
    }
}

Ask ask_from_name(const std::string& s) {
    if (s == "never") return Ask::Never;
    if (s == "startup" || s == "always" || s == "yes") return Ask::Startup;
    return Ask::Unasked;
}

fs::path prefs_path() {
    /* `%LOCALAPPDATA%\VoidHormiga` is exactly the NSIS `$INSTDIR` Mago's
     * emitter writes (`suite_dir` is the display name without spaces), and it
     * is the PARENT of each side-by-side version folder. So the answer to
     * "should I check for updates" survives the update it was given for, which
     * a file inside the version folder would not. */
#ifdef _WIN32
    if (const char* la = std::getenv("LOCALAPPDATA"); la && *la)
        return fs::path(la) / "VoidHormiga" / "updates.json";
    if (const char* up = std::getenv("USERPROFILE"); up && *up)
        return fs::path(up) / "AppData" / "Local" / "VoidHormiga" / "updates.json";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
        return fs::path(xdg) / "voidhormiga" / "updates.json";
    if (const char* home = std::getenv("HOME"); home && *home)
        return fs::path(home) / ".config" / "voidhormiga" / "updates.json";
#endif
    return fs::temp_directory_path() / "voidhormiga-updates.json";
}

Prefs load_prefs() {
    Prefs p;
    std::ifstream in(prefs_path(), std::ios::binary);
    if (!in) return p;   // Unasked: a machine with no file has not been asked
    std::ostringstream ss;
    ss << in.rdbuf();
    json d = json::parse(ss.str(), nullptr, false);
    if (d.is_discarded() || !d.is_object()) return p;
    p.ask = ask_from_name(str_of(d, "check"));
    p.feed_url = str_of(d, "feed_url");
    p.skip_version = str_of(d, "skip_version");
    p.last_checked = str_of(d, "last_checked");
    return p;
}

bool save_prefs(const Prefs& p) {
    const fs::path dst = prefs_path();
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    json d;
    d["check"] = ask_name(p.ask);
    d["feed_url"] = p.feed_url;
    d["skip_version"] = p.skip_version;
    d["last_checked"] = p.last_checked;
    /* Write-temp-then-rename, the discipline the vault, the bundle and the
     * merge already use. A half-written preferences file reads as `Unasked`,
     * which would re-ask a question somebody has already answered "never" to —
     * the one outcome this file exists to prevent. */
    const fs::path tmp = dst.string() + ".writing";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << d.dump(2) << "\n";
        if (!out) return false;
    }
    fs::rename(tmp, dst, ec);
    if (ec) { fs::remove(tmp, ec); return false; }
    return true;
}

// ── THE NETWORK ─────────────────────────────────────────────────────────────

Fetch fetch_feed(const Shell& shell, const std::string& url, const fs::path& tmp) {
    Fetch f;
    if (!shell) { f.error = "no shell seam"; return f; }
    if (!safe_url(url)) {
        f.error = "refusing to fetch \"" + url + "\": not a plain https URL";
        return f;
    }
    /* ITS OWN CURL RATHER THAN `publish/http.hpp`, for two reasons that are
     * both about this call specifically:
     *
     *  - `-L`. `releases/latest/download/<name>` is a 302 to a storage host.
     *    The publish holidays talk to APIs that answer directly and must NOT
     *    follow redirects with a bearer token attached, so adding `-L` there
     *    to serve this call would be a security change made for a convenience.
     *  - `--max-time`. A hung network must not be indistinguishable from a
     *    hung application, and the deploy path deliberately has no ceiling
     *    because an asset upload legitimately takes minutes.
     *
     * There is no token and no request body here, so nothing that made
     * `http::call` worth sharing applies: the feed is a public static file.
     * `-w` appends the status on its own line; anything else on that line means
     * curl wrote a diagnostic there instead, and the status stays 0 rather than
     * being guessed at. */
    std::error_code ec;
    fs::create_directories(tmp, ec);
    const fs::path body = tmp / ".voidhormiga-feed.json";
    fs::remove(body, ec);
    const std::string cmd = "curl -sSL --max-time 30 -o " + q(body.string()) +
                            " -w \"%{http_code}\" " + q(url) + " 2>&1";
    const std::string tail = shell(cmd);

    {
        std::ifstream in(body, std::ios::binary);
        std::ostringstream ss;
        ss << in.rdbuf();
        f.body = ss.str();
    }
    fs::remove(body, ec);

    /* The status is the LAST line curl wrote; anything before it is a
     * diagnostic on stderr, which is the whole of the message when the network
     * is the problem. */
    std::string code = tail, note;
    if (const size_t nl = tail.find_last_of('\n'); nl != std::string::npos) {
        note = tail.substr(0, nl);
        code = tail.substr(nl + 1);
    }
    while (!code.empty() && (code.back() == '\r' || code.back() == ' ')) code.pop_back();
    bool numeric = !code.empty() && code.size() <= 4;
    for (const char c : code) numeric = numeric && c >= '0' && c <= '9';
    f.status = numeric ? std::stoi(code) : 0;

    if (f.status == 0) {
        f.error = "could not reach " + url +
                  (note.empty() && tail.empty() ? std::string(" (no response)")
                                                : (" (" + (note.empty() ? tail : note) + ")"));
        return f;
    }
    if (f.status < 200 || f.status >= 300) {
        f.error = "the update feed answered HTTP " + std::to_string(f.status);
        return f;
    }
    if (f.body.empty()) {
        f.error = "the update feed answered HTTP 200 with an empty body";
        return f;
    }
    f.ok = true;
    return f;
}

CheckResult check(const Shell& shell, Prefs& prefs, const fs::path& tmp) {
    CheckResult c;
    const std::string url = prefs.feed_url.empty() ? default_feed_url() : prefs.feed_url;
    const Fetch f = fetch_feed(shell, url, tmp);
    prefs.last_checked = now_iso8601();  // we went to the network; say so
    if (!f.ok) { c.error = f.error; return c; }
    c.feed = parse_feed(f.body, app_name(), platform_tag());
    if (!c.feed.ok) { c.error = c.feed.error; return c; }
    c.offer = decide(c.feed, current_version(), prefs.skip_version);
    c.ok = true;
    return c;
}

std::string sha256_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    crypto_hash_sha256_state st;
    crypto_hash_sha256_init(&st);
    std::vector<char> buf(64 * 1024);
    while (in) {
        in.read(buf.data(), (std::streamsize)buf.size());
        const std::streamsize n = in.gcount();
        if (n > 0)
            crypto_hash_sha256_update(&st, (const unsigned char*)buf.data(), (size_t)n);
    }
    unsigned char out[crypto_hash_sha256_BYTES];
    crypto_hash_sha256_final(&st, out);
    static const char* H = "0123456789abcdef";
    std::string hex;
    hex.reserve(sizeof out * 2);
    for (const unsigned char c : out) { hex += H[c >> 4]; hex += H[c & 15]; }
    return hex;
}

Download download(const Shell& shell, const Release& r, const fs::path& dir) {
    Download d;
    if (!shell) { d.error = "no shell seam"; return d; }
    if (!r.has_artifact) {
        d.error = "no " + std::string(platform_tag()) + " build in this release";
        return d;
    }
    if (!safe_url(r.url)) {
        d.error = "refusing to download \"" + r.url + "\": not a plain https URL";
        return d;
    }
    if (!safe_filename(r.file)) {
        d.error = "refusing a release whose filename is \"" + r.file + "\"";
        return d;
    }
    if (!valid_sha256(r.sha256)) {
        /* THE ONLY REFUSAL HERE THAT IS ABOUT POLICY RATHER THAN SAFETY, and
         * it is the right one: an installer we cannot check is an installer we
         * have no reason to run. Mago always writes `sha256`, so this fires
         * only when the feed is wrong or is not the feed. */
        d.error = "this release carries no usable sha256; refusing to download "
                  "an installer that cannot be checked";
        return d;
    }

    std::error_code ec;
    fs::create_directories(dir, ec);
    const fs::path dst = dir / r.file;
    fs::remove(dst, ec);

    // -L: GitHub's release assets are a redirect to a storage host.
    const std::string cmd = "curl -sSL --fail --max-time 900 -o " +
                            q(dst.string()) + " " + q(r.url) + " 2>&1";
    const std::string out = shell(cmd);
    if (!fs::exists(dst, ec)) {
        d.error = "the download did not produce a file" +
                  (out.empty() ? std::string() : (": " + out));
        return d;
    }

    const std::string got = sha256_file(dst);
    auto lower = [](std::string s) {
        for (char& c : s) if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
        return s;
    };
    if (got.empty() || lower(got) != lower(r.sha256)) {
        /* THE FILE IS DELETED, and that is the point of doing the check here
         * rather than in a caller. A mismatched installer left on disk beside
         * a message nobody read is how a bad download gets run anyway — a week
         * later, by somebody who found it in Downloads and assumed it was
         * fine. */
        fs::remove(dst, ec);
        d.error = "the download does not match the checksum in the feed "
                  "(expected " + lower(r.sha256) + ", got " +
                  (got.empty() ? std::string("nothing readable") : lower(got)) +
                  "). The file has been deleted.";
        return d;
    }
    if (r.bytes > 0) {
        const auto sz = (long long)fs::file_size(dst, ec);
        if (!ec && sz != r.bytes)
            /* Not fatal: the digest already settled it, and a `bytes` that
             * disagrees with a matching sha256 means Mago's bookkeeping is off
             * rather than the file being wrong. Worth saying, not worth
             * refusing. */
            d.error = "note: the feed said " + std::to_string(r.bytes) +
                      " bytes and the file is " + std::to_string(sz) +
                      "; the checksum matched, so the download is good.";
    }
    d.ok = true;
    d.file = dst;
    return d;
}

bool launch_installer(const fs::path& file) {
    std::error_code ec;
    if (!fs::exists(file, ec)) return false;
#ifdef _WIN32
    /* ShellExecute rather than `system()`: the installer is a GUI program and
     * this process is about to keep running (or be closed by the person, on
     * their own terms). The same call `desktop.cpp` uses to open a rendered
     * page -- the OS decides what "run this" means, we do not. */
    const HINSTANCE h = ShellExecuteA(nullptr, "open", file.string().c_str(),
                                      nullptr, nullptr, SW_SHOWNORMAL);
    return (INT_PTR)h > 32;   // ShellExecute's own success threshold
#else
    const std::string cmd = "xdg-open " + q(file.string()) + " >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
#endif
}

// ── WHO WE ARE ──────────────────────────────────────────────────────────────

#ifndef HORMIGA_VERSION
/* Never reached in a CMake build — `project(voidhormiga VERSION ...)` defines
 * it — and deliberately obvious rather than plausible if some other build
 * system ever compiles this file without it. */
#define HORMIGA_VERSION "0.0.0-unversioned"
#endif
#ifndef HORMIGA_PLATFORM
/* Never reached in a CMake build either -- `HORMIGA_PLATFORM_TAG` is computed
 * from the target platform and defined on every target that compiles this file.
 *
 * This used to answer "windows-x64" under `_WIN32` and "unknown" everywhere
 * else, which was right on Windows by accident and quietly wrong anywhere else:
 * the platform string is the KEY this binary looks itself up under in the
 * feed's `artifacts` object, so a binary calling itself "unknown" matches no
 * release ever published and reports "up to date" forever. That is the failure
 * this project keeps finding -- a true-looking answer to a question that was
 * never actually asked. So the fallback is now obviously broken rather than
 * plausibly fine, exactly like HORMIGA_VERSION's above. */
#define HORMIGA_PLATFORM "unconfigured-platform"
#endif

const char* app_name() { return "voidhormiga"; }
const char* current_version() { return HORMIGA_VERSION; }
const char* platform_tag() { return HORMIGA_PLATFORM; }

const char* default_feed_url() {
    /* GitHub Releases, and `releases/latest/download/<name>` rather than a
     * pinned tag: it resolves to the newest release's asset of that name, so
     * the URL compiled into 0.1.0 still finds the feed in 2028. `mago feed
     * --base-url .../releases/latest/download` writes artifact URLs in the same
     * shape, and they resolve the same way for as long as the release they name
     * is the latest — which is exactly when we offer them.
     *
     * A machine that needs to point somewhere else sets `feed_url` in
     * `updates.json`; nothing here is compiled into a decision. */
    return "https://github.com/migriv24/VoidHormiga/releases/latest/download/"
           "void-updates.json";
}

} // namespace hormiga::update
