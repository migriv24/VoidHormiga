/* cloudflare.cpp — the Pages Direct Upload flow. See cloudflare.hpp for why
 * this is native rather than a `deploy_cmd` shelling out to wrangler.
 */
#include "publish/cloudflare.hpp"

#include "blake3.hpp"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace hormiga::cloudflare {
namespace {

constexpr const char* kApi = "https://api.cloudflare.com/client/v4";

/* Cloudflare wants the file's bytes as base64, and then hashes THAT string.
 * (Not the raw bytes — see hashFile in wrangler's deploy-helpers.) */
std::string b64(const std::string& in) {
    static const char* T =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const unsigned v = ((unsigned char)in[i] << 16) |
                           ((unsigned char)in[i + 1] << 8) |
                           (unsigned char)in[i + 2];
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += T[(v >> 6) & 63];
        out += T[v & 63];
    }
    if (i + 1 == in.size()) {
        const unsigned v = (unsigned char)in[i] << 16;
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += "==";
    } else if (i + 2 == in.size()) {
        const unsigned v =
            ((unsigned char)in[i] << 16) | ((unsigned char)in[i + 1] << 8);
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += T[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/* A browser decides what to do with a file by what we say it is, so this list
 * is not cosmetic: serve a .css as text/plain and the page arrives unstyled. */
std::string content_type(const std::string& ext) {
    static const std::pair<const char*, const char*> kTypes[] = {
        {"html", "text/html"},         {"htm", "text/html"},
        {"css", "text/css"},           {"js", "text/javascript"},
        {"json", "application/json"},  {"svg", "image/svg+xml"},
        {"png", "image/png"},          {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},        {"gif", "image/gif"},
        {"webp", "image/webp"},        {"avif", "image/avif"},
        {"ico", "image/x-icon"},       {"woff2", "font/woff2"},
        {"woff", "font/woff"},         {"ttf", "font/ttf"},
        {"txt", "text/plain"},         {"xml", "application/xml"},
        {"ics", "text/calendar"},      {"pdf", "application/pdf"},
        {"webmanifest", "application/manifest+json"},
        {"map", "application/json"},
    };
    for (const auto& [e, t] : kTypes)
        if (ext == e) return t;
    return "application/octet-stream";
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

/* Shell-quote a path for the command line. Paths only — never a secret. */
std::string q(const std::string& s) { return "\"" + s + "\""; }

struct Asset {
    std::string rel;   // "/index-en.html", the manifest key
    std::string hash;  // the 32-hex Pages asset key
    std::string ctype;
    fs::path path;
};

/* One curl call with a bearer token, a JSON body, and no secret in argv.
 *
 * Both the header and the body go into files: the header because argv is world
 * readable, the body because an upload payload is megabytes of base64 and
 * command lines have limits measured in kilobytes. */
std::string post_json(const Config& cfg, const std::string& url,
                      const std::string& bearer, const std::string& body,
                      const fs::path& tmp) {
    const fs::path cfgf = tmp / ".cf-curl.cfg";
    const fs::path bodyf = tmp / ".cf-body.json";
    {
        std::ofstream c(cfgf, std::ios::binary | std::ios::trunc);
        c << "header = \"Authorization: Bearer " << bearer << "\"\n"
          << "header = \"Content-Type: application/json\"\n";
        std::ofstream b(bodyf, std::ios::binary | std::ios::trunc);
        b << body;
    }
    const std::string cmd = "curl -sS --config " + q(cfgf.string()) +
                            " -X POST --data-binary @" + q(bodyf.string()) +
                            " " + q(url) + " 2>&1";
    const std::string out = cfg.shell ? cfg.shell(cmd) : std::string();
    std::error_code ec;
    fs::remove(cfgf, ec);
    fs::remove(bodyf, ec);
    return out;
}

std::string get(const Config& cfg, const std::string& url,
                const std::string& bearer, const fs::path& tmp) {
    const fs::path cfgf = tmp / ".cf-curl.cfg";
    {
        std::ofstream c(cfgf, std::ios::binary | std::ios::trunc);
        c << "header = \"Authorization: Bearer " << bearer << "\"\n";
    }
    const std::string cmd =
        "curl -sS --config " + q(cfgf.string()) + " " + q(url) + " 2>&1";
    const std::string out = cfg.shell ? cfg.shell(cmd) : std::string();
    std::error_code ec;
    fs::remove(cfgf, ec);
    return out;
}

/* Cloudflare answers every call with the same envelope. Pull `success` and,
 * when it is false, the errors — VERBATIM, because the vendor's own sentence is
 * routinely the entire diagnosis. */
bool envelope_ok(const std::string& raw, nlohmann::json& result,
                 std::string& err) {
    try {
        const auto j = nlohmann::json::parse(raw);
        if (j.value("success", false)) {
            if (j.contains("result")) result = j["result"];
            return true;
        }
        std::string msgs;
        if (j.contains("errors"))
            for (const auto& e : j["errors"]) {
                if (!msgs.empty()) msgs += "; ";
                if (e.contains("code")) msgs += std::to_string(e.value("code", 0)) + " ";
                msgs += e.value("message", std::string());
            }
        err = msgs.empty() ? raw.substr(0, 400) : msgs;
        return false;
    } catch (...) {
        // not JSON at all: curl's own error, a proxy page, an empty body
        err = raw.empty() ? "no response (is curl on PATH?)" : raw.substr(0, 400);
        return false;
    }
}

} // namespace

std::string looks_like_token(const std::string& contents) {
    std::string t = contents;
    while (!t.empty() && (unsigned char)t.back() <= ' ') t.pop_back();
    const size_t b = t.find_first_not_of(" \t\r\n");
    t = b == std::string::npos ? std::string() : t.substr(b);

    if (t.empty()) return "the file is empty";
    if (t.find('\n') != std::string::npos)
        return "the file has more than one line - it should hold only the token";
    /* `cfut_…` is an UPLOAD token, not an account API token. The distinction
     * cost a real afternoon: it is accepted everywhere a string is accepted and
     * rejected by every call that matters. */
    if (t.rfind("cfut_", 0) == 0)
        return "that is a Pages upload token (cfut_...), not an account API "
               "token - create one at Cloudflare > My Profile > API Tokens";
    if (t.size() < 30)
        return "too short for a Cloudflare API token (" +
               std::to_string(t.size()) + " characters)";
    for (char c : t)
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-'))
            return "contains characters an API token does not - is this the "
                   "right file?";
    return {};
}

Step check_token(const Config& cfg) {
    Step s;
    s.what = "the token can list this account's Pages projects";
    if (cfg.account_id.empty()) {
        s.detail = "no account id set on the host node";
        return s;
    }
    const fs::path tmp = cfg.work_dir.empty() ? fs::temp_directory_path()
                                              : fs::path(cfg.work_dir);
    const std::string raw =
        get(cfg, std::string(kApi) + "/accounts/" + cfg.account_id +
                    "/pages/projects?per_page=1",
            cfg.token, tmp);
    nlohmann::json result;
    std::string err;
    if (!envelope_ok(raw, result, err)) {
        s.detail = err;
        return s;
    }
    s.ok = true;
    s.detail = "ok";
    return s;
}

DeployResult deploy(const Config& cfg) {
    DeployResult r;
    /* ── EACH STEP IS ANNOUNCED ONCE ─────────────────────────────────────────
     *
     * `hashed 57 file(s)` printed TWICE in the operator's console, reported
     * twice by the field agent. Not a double walk — a double REPORT: every step
     * was pushed to `r.steps` and simultaneously handed to `cfg.progress`, and
     * the caller in `publish.cpp` then replayed the whole of `r.steps` into the
     * same log the progress callback had already written to.
     *
     * `progress` is the live channel and it is the one that stays, because
     * during a real upload the operator wants each step as it happens rather
     * than five lines at the end. The caller no longer replays. A doubled log
     * line is cosmetic right up until somebody counts the entries to work out
     * whether a retry happened. */
    auto step = [&](bool ok, std::string what, std::string detail) {
        r.steps.push_back({ok, std::move(what), std::move(detail)});
        /* Successes only. `progress` has no severity, so announcing a FAILURE
         * through it would file the one line that matters under the same
         * [info] as "hashed 57 file(s)". Every failing step returns
         * immediately, so the caller reports it once, at error level, with the
         * vendor's own words attached. */
        if (ok && cfg.progress) cfg.progress(r.steps.back().what);
        return ok;
    };
    const fs::path tmp = cfg.work_dir.empty() ? fs::temp_directory_path()
                                              : fs::path(cfg.work_dir);
    std::error_code ec;

    // ── gather + hash every file under site/ ────────────────────────────────
    std::vector<Asset> assets;
    const fs::path root(cfg.site_dir);
    if (!fs::is_directory(root, ec)) {
        step(false, "read the built site", "no such directory: " + cfg.site_dir);
        return r;
    }
    for (auto it = fs::recursive_directory_iterator(root, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec || !it->is_regular_file(ec)) continue;
        const fs::path p = it->path();
        const std::string fn = p.filename().string();
        if (!fn.empty() && fn[0] == '.') continue; // .DS_Store and friends
        Asset a;
        a.path = p;
        std::string rel = fs::relative(p, root, ec).generic_string();
        a.rel = "/" + rel;
        const std::string ext = lower(p.extension().string().size() > 1
                                          ? p.extension().string().substr(1)
                                          : std::string());
        a.ctype = content_type(ext);
        /* THE KEY CLOUDFLARE STORES THE ASSET UNDER. Base64 first, then the
         * extension without its dot, then blake3, then the first 32 hex
         * characters. Any deviation uploads a file the manifest cannot find. */
        a.hash = blake3::hex(b64(read_file(p)) + ext).substr(0, 32);
        assets.push_back(std::move(a));
    }
    if (assets.empty()) {
        step(false, "read the built site", "no files in " + cfg.site_dir);
        return r;
    }
    step(true, "hashed " + std::to_string(assets.size()) + " file(s)", "ok");

    // ── 1. an upload token ──────────────────────────────────────────────────
    /* ── IT IS A GET (2026-08-20) ────────────────────────────────────────────
     *
     * This was a POST, and a POST is refused at Cloudflare's edge before the
     * API handler is reached:
     *
     *     GET  …/pages/projects/{p}/upload-token → {"result":{"jwt":"eyJ…"}}
     *     POST …/pages/projects/{p}/upload-token → {"code":1001,
     *                                               "error":"method_not_allowed"}
     *
     * Both verified by hand against a live account by the field agent, same
     * token, same URL. It is step 1 of 5, so nothing after it had ever run
     * against a real project — the whole native publish path was dead behind
     * one HTTP verb, and the entire Python/wrangler/Node chain existed only to
     * route around it.
     *
     * Note the error SHAPE, because it is why passing the vendor's body
     * through verbatim keeps earning its place: `{"code":…,"error":…}` is not
     * Cloudflare's usual `{"success":false,"errors":[…]}` envelope, so a parser
     * looking for `errors[0].message` finds nothing and reports nothing.
     * `envelope_ok` falls back to the raw body, which is what made this a
     * one-minute diagnosis instead of an afternoon. */
    std::string jwt;
    {
        const std::string raw =
            get(cfg,
                std::string(kApi) + "/accounts/" + cfg.account_id +
                    "/pages/projects/" + cfg.project + "/upload-token",
                cfg.token, tmp);
        nlohmann::json res;
        std::string err;
        if (!envelope_ok(raw, res, err)) {
            step(false, "asked Cloudflare for an upload token", err);
            return r;
        }
        jwt = res.value("jwt", std::string());
        if (jwt.empty()) {
            step(false, "asked Cloudflare for an upload token",
                 "no jwt in the response");
            return r;
        }
        step(true, "got an upload token", "ok");
    }

    // ── 2. which assets does Cloudflare not already have? ───────────────────
    std::vector<std::string> missing;
    {
        nlohmann::json body;
        body["hashes"] = nlohmann::json::array();
        for (const Asset& a : assets) body["hashes"].push_back(a.hash);
        const std::string raw = post_json(
            cfg, std::string(kApi) + "/pages/assets/check-missing", jwt,
            body.dump(), tmp);
        nlohmann::json res;
        std::string err;
        if (!envelope_ok(raw, res, err)) {
            step(false, "asked which files are missing", err);
            return r;
        }
        if (res.is_array())
            for (const auto& h : res) missing.push_back(h.get<std::string>());
        step(true,
             std::to_string(missing.size()) + " of " +
                 std::to_string(assets.size()) + " file(s) need uploading",
             "ok");
    }

    // ── 3. upload them, in buckets ─────────────────────────────────────────
    if (!missing.empty()) {
        auto is_missing = [&](const std::string& h) {
            return std::find(missing.begin(), missing.end(), h) != missing.end();
        };
        /* Cloudflare's own client batches uploads and caps a bucket well below
         * its 50 MiB limit. Bytes rather than a file count, because one 4 MB
         * flier and four hundred 2 KB pages are the same request otherwise. */
        constexpr size_t kBucketBytes = 8u * 1024u * 1024u;
        size_t sent = 0;
        std::vector<nlohmann::json> bucket;
        size_t bucket_bytes = 0;
        auto flush = [&]() -> bool {
            if (bucket.empty()) return true;
            nlohmann::json arr = nlohmann::json::array();
            for (auto& b : bucket) arr.push_back(std::move(b));
            const std::string raw =
                post_json(cfg, std::string(kApi) + "/pages/assets/upload", jwt,
                          arr.dump(), tmp);
            nlohmann::json res;
            std::string err;
            if (!envelope_ok(raw, res, err)) {
                step(false, "uploaded files", err);
                return false;
            }
            sent += bucket.size();
            bucket.clear();
            bucket_bytes = 0;
            if (cfg.progress)
                cfg.progress("uploaded " + std::to_string(sent) + "/" +
                             std::to_string(missing.size()));
            return true;
        };
        for (const Asset& a : assets) {
            if (!is_missing(a.hash)) continue;
            const std::string payload = b64(read_file(a.path));
            if (bucket_bytes + payload.size() > kBucketBytes && !flush()) return r;
            nlohmann::json f;
            f["key"] = a.hash;
            f["value"] = payload;
            f["metadata"]["contentType"] = a.ctype;
            f["base64"] = true;
            bucket_bytes += payload.size();
            bucket.push_back(std::move(f));
        }
        if (!flush()) return r;
        step(true, "uploaded " + std::to_string(sent) + " file(s)", "ok");
    }

    // ── 4. tell Cloudflare to keep them cached (best effort) ───────────────
    {
        nlohmann::json body;
        body["hashes"] = nlohmann::json::array();
        for (const Asset& a : assets) body["hashes"].push_back(a.hash);
        const std::string raw = post_json(
            cfg, std::string(kApi) + "/pages/assets/upsert-hashes", jwt,
            body.dump(), tmp);
        nlohmann::json res;
        std::string err;
        /* NOT fatal, and wrangler says the same: a failure here only slows the
         * NEXT deploy, because the assets are already uploaded. Reporting it as
         * a failed publish would be a lie about what the visitor can see. */
        if (!envelope_ok(raw, res, err))
            step(true, "cache hint declined (harmless)", err);
    }

    // ── 5. create the deployment ───────────────────────────────────────────
    {
        nlohmann::json manifest = nlohmann::json::object();
        for (const Asset& a : assets) manifest[a.rel] = a.hash;
        const fs::path mf = tmp / ".cf-manifest.json";
        {
            std::ofstream m(mf, std::ios::binary | std::ios::trunc);
            m << manifest.dump();
        }
        const fs::path cfgf = tmp / ".cf-curl.cfg";
        {
            std::ofstream c(cfgf, std::ios::binary | std::ios::trunc);
            c << "header = \"Authorization: Bearer " << cfg.token << "\"\n";
        }
        // multipart: the manifest as a form FIELD read from a file, so a large
        // site does not meet the command-line length limit
        const std::string cmd =
            "curl -sS --config " + q(cfgf.string()) +
            " -X POST -F \"manifest=<" + mf.string() + "\"" + " -F \"branch=" +
            cfg.branch + "\" " +
            q(std::string(kApi) + "/accounts/" + cfg.account_id +
              "/pages/projects/" + cfg.project + "/deployments") +
            " 2>&1";
        const std::string raw = cfg.shell ? cfg.shell(cmd) : std::string();
        fs::remove(cfgf, ec);
        fs::remove(mf, ec);
        nlohmann::json res;
        std::string err;
        if (!envelope_ok(raw, res, err)) {
            step(false, "created the deployment", err);
            return r;
        }
        r.vendor_id = res.value("id", std::string());
        r.url = res.value("url", std::string());
        if (r.url.empty() && res.contains("aliases") && res["aliases"].is_array() &&
            !res["aliases"].empty())
            r.url = res["aliases"][0].get<std::string>();
        step(true, "created the deployment", r.url.empty() ? "ok" : r.url);
    }

    r.ok = true;
    return r;
}

DeployResult rollback(const Config& cfg, const std::string& deployment_id) {
    DeployResult r;
    const fs::path tmp = cfg.work_dir.empty() ? fs::temp_directory_path()
                                              : fs::path(cfg.work_dir);
    const std::string raw =
        post_json(cfg,
                  std::string(kApi) + "/accounts/" + cfg.account_id +
                      "/pages/projects/" + cfg.project + "/deployments/" +
                      deployment_id + "/rollback",
                  cfg.token, "{}", tmp);
    nlohmann::json res;
    std::string err;
    if (!envelope_ok(raw, res, err)) {
        r.steps.push_back({false, "rolled back to " + deployment_id, err});
        return r;
    }
    r.ok = true;
    r.vendor_id = deployment_id;
    r.url = res.value("url", std::string());
    r.steps.push_back({true, "rolled back to " + deployment_id,
                       r.url.empty() ? "ok" : r.url});
    return r;
}

} // namespace hormiga::cloudflare
