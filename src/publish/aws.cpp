/* aws.cpp — see aws.hpp for why this exists and what it does and does not prove.
 *
 * The signing steps are AWS's, in AWS's order, with their names kept so this can
 * be read side by side with the specification. Where a rule looks arbitrary it
 * is quoted, because every one of them is a thing that produces an opaque 403
 * when got subtly wrong.
 */
#include "publish/aws.hpp"

#include "sodium.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace hormiga::aws {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

/* Trim ASCII spaces and tabs from both ends. SigV4 requires header VALUES to be
 * trimmed before signing, and a trailing space is exactly the kind of thing that
 * survives a copy-paste and then costs an afternoon. */
std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    const size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
}

std::string now_amz_date() {
    const std::time_t t = std::time(nullptr);
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &t);
#else
    gmtime_r(&t, &g);
#endif
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d%02d%02dT%02d%02d%02dZ", g.tm_year + 1900,
                  g.tm_mon + 1, g.tm_mday, g.tm_hour, g.tm_min, g.tm_sec);
    return buf;
}

std::string q(const std::string& s) { return "\"" + s + "\""; }

} // namespace

std::string hex(const std::string& raw) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(raw.size() * 2);
    for (unsigned char c : raw) {
        out += d[c >> 4];
        out += d[c & 0xf];
    }
    return out;
}

std::string sha256_hex(const std::string& data) {
    unsigned char h[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(h, (const unsigned char*)data.data(), data.size());
    return hex(std::string((const char*)h, sizeof h));
}

std::string hmac_sha256(const std::string& key, const std::string& data) {
    /* libsodium's one-shot `crypto_auth_hmacsha256` requires a 32-byte key.
     * HMAC accepts any length, and SigV4 uses keys that are not 32 bytes (the
     * first is "AWS4" + the secret), so the streaming API is the correct one —
     * `_init` performs the standard key derivation for us. Still libsodium's
     * HMAC; only the entry point differs. */
    crypto_auth_hmacsha256_state st;
    crypto_auth_hmacsha256_init(&st, (const unsigned char*)key.data(), key.size());
    crypto_auth_hmacsha256_update(&st, (const unsigned char*)data.data(),
                                  data.size());
    unsigned char out[crypto_auth_hmacsha256_BYTES];
    crypto_auth_hmacsha256_final(&st, out);
    return std::string((const char*)out, sizeof out);
}

/* RFC 3986 unreserved characters pass; everything else is %XX with UPPERCASE
 * hex. `encode_slash` is false for a path (where `/` separates segments) and
 * true for a query value. S3 additionally wants the path encoded ONCE — unlike
 * every other AWS service, which encodes it twice — which is why the caller
 * hands us an already-encoded path and we do not touch it again. */
std::string uri_encode(const std::string& s, bool encode_slash) {
    static const char* d = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else if (c == '/' && !encode_slash) {
            out += '/';
        } else {
            out += '%';
            out += d[c >> 4];
            out += d[c & 0xf];
        }
    }
    return out;
}

std::string signing_key(const std::string& secret, const std::string& date,
                        const std::string& region, const std::string& service) {
    /* The chain, and the reason it is a chain: each HMAC narrows the key's
     * authority, so a signing key that leaks is useless outside one day, one
     * region and one service. */
    const std::string kDate = hmac_sha256("AWS4" + secret, date);
    const std::string kRegion = hmac_sha256(kDate, region);
    const std::string kService = hmac_sha256(kRegion, service);
    return hmac_sha256(kService, "aws4_request");
}

Signed sign(const Request& req, const Credentials& creds) {
    Signed out;
    out.amz_date = req.amz_date.empty() ? now_amz_date() : req.amz_date;
    const std::string date = out.amz_date.substr(0, 8);   // YYYYMMDD

    const std::string payload_hash = sha256_hex(req.payload);

    /* ── the headers that are always signed ──────────────────────────────────
     * `host` and `x-amz-date` are mandatory. `x-amz-content-sha256` is required
     * by S3 specifically and is what lets it verify the body it received is the
     * body that was signed. */
    std::map<std::string, std::string> h;
    for (const auto& [k, v] : req.headers) h[lower(k)] = trim(v);
    h["host"] = req.host;
    h["x-amz-date"] = out.amz_date;
    h["x-amz-content-sha256"] = payload_hash;
    if (!creds.session_token.empty())
        h["x-amz-security-token"] = creds.session_token;

    std::string canonical_headers, signed_headers;
    for (const auto& [k, v] : h) {   // std::map is already sorted by key
        canonical_headers += k + ":" + v + "\n";
        if (!signed_headers.empty()) signed_headers += ";";
        signed_headers += k;
    }

    std::string canonical_query;
    for (const auto& [k, v] : req.query) {   // sorted by key, values encoded
        if (!canonical_query.empty()) canonical_query += "&";
        canonical_query += uri_encode(k, true) + "=" + uri_encode(v, true);
    }

    out.canonical_request = req.method + "\n" + req.path + "\n" + canonical_query +
                            "\n" + canonical_headers + "\n" + signed_headers +
                            "\n" + payload_hash;

    const std::string scope =
        date + "/" + req.region + "/" + req.service + "/aws4_request";
    out.string_to_sign = "AWS4-HMAC-SHA256\n" + out.amz_date + "\n" + scope +
                         "\n" + sha256_hex(out.canonical_request);

    out.signature = hex(hmac_sha256(
        signing_key(creds.secret_access_key, date, req.region, req.service),
        out.string_to_sign));

    out.authorization = "AWS4-HMAC-SHA256 Credential=" + creds.access_key_id +
                        "/" + scope + ", SignedHeaders=" + signed_headers +
                        ", Signature=" + out.signature;
    out.headers = h;
    return out;
}

namespace {

std::string host_for(const Config& cfg) {
    if (!cfg.endpoint.empty()) return cfg.endpoint;
    return cfg.bucket + ".s3." + cfg.region + ".amazonaws.com";
}

/* Did the vendor refuse? S3 answers an error with an XML body carrying a
 * `<Code>`; a success has no body at all for a PUT. Deliberately NOT a parser:
 * the body is handed back whole, because AWS's SigV4 refusals include the
 * canonical request THEY computed, and diffing that against ours is the entire
 * diagnosis. Summarising it away would throw out the one useful thing. */
bool looks_like_error(const std::string& body) {
    return body.find("<Error") != std::string::npos ||
           body.find("<Code>") != std::string::npos;
}

Result run(const Config& cfg, const std::string& method, const std::string& key,
           const std::string& body, const std::string& file,
           const std::string& content_type) {
    Result r;
    if (!cfg.shell) {
        r.error = "no shell transport on this front-end";
        return r;
    }
    if (cfg.bucket.empty() || cfg.creds.access_key_id.empty() ||
        cfg.creds.secret_access_key.empty()) {
        r.error = "the object store is not configured (bucket, access key id "
                  "and secret are all required)";
        return r;
    }

    /* THE PAYLOAD HASH FOR A FILE. S3 requires the body's hash in the
     * signature, so a file has to be read to hash it even though curl will
     * stream it. That is one read of a backup-sized file and is worth it: the
     * alternative is UNSIGNED-PAYLOAD, which works but gives up the integrity
     * check that makes a corrupted upload detectable at the far end. */
    std::string payload = body;
    if (!file.empty()) {
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            r.error = "cannot read " + file;
            return r;
        }
        std::stringstream ss;
        ss << in.rdbuf();
        payload = ss.str();
    }

    Request req;
    req.method = method;
    req.host = host_for(cfg);
    req.path = "/" + uri_encode(key, false);
    req.region = cfg.region;
    req.service = "s3";
    req.payload = method == "GET" ? std::string() : payload;
    if (!content_type.empty()) req.headers["content-type"] = content_type;

    const Signed s = sign(req, cfg.creds);

    const fs::path tmp =
        cfg.work_dir.empty() ? fs::temp_directory_path() : fs::path(cfg.work_dir);
    const fs::path cfgf = tmp / ".aws-curl.cfg";
    {
        /* THE SECRET NEVER TOUCHES argv. Same rule as the Cloudflare deploy: a
         * command line is readable in a process listing by anything running as
         * this user, and an Authorization header is a bearer credential for the
         * length of its validity. */
        std::ofstream c(cfgf, std::ios::binary | std::ios::trunc);
        if (!c) {
            r.error = "cannot write " + cfgf.string();
            return r;
        }
        for (const auto& [k, v] : s.headers)
            if (k != "host") c << "header = " << q(k + ": " + v) << "\n";
        c << "header = " << q("Authorization: " + s.authorization) << "\n";
        if (!content_type.empty())
            c << "header = " << q("Content-Type: " + content_type) << "\n";
    }

    std::string cmd = "curl -sS -X " + method + " --config " + q(cfgf.string());
    fs::path bodyf;
    if (!file.empty()) {
        cmd += " --data-binary @" + q(file);
    } else if (!body.empty()) {
        bodyf = tmp / ".aws-body";
        std::ofstream b(bodyf, std::ios::binary | std::ios::trunc);
        b << body;
        b.close();
        cmd += " --data-binary @" + q(bodyf.string());
    }
    cmd += " " + q("https://" + req.host + req.path) + " 2>&1";

    if (cfg.progress)
        cfg.progress(method + " s3://" + cfg.bucket + "/" + key);
    const std::string resp = cfg.shell(cmd);

    std::error_code ec;
    fs::remove(cfgf, ec);
    if (!bodyf.empty()) fs::remove(bodyf, ec);

    if (looks_like_error(resp)) {
        /* VERBATIM. AWS's SigV4 refusals contain the canonical request they
         * computed; that text is the diagnosis and a summary would discard it.
         * This rule has now paid for itself three times on Cloudflare. */
        r.error = resp.substr(0, 1200);
        return r;
    }
    if (resp.rfind("curl:", 0) == 0) {
        r.error = resp.substr(0, 400);
        return r;
    }
    r.ok = true;
    r.body = resp;
    r.bytes = (long long)payload.size();
    return r;
}

} // namespace

Result put_object(const Config& cfg, const std::string& key,
                  const std::string& body, const std::string& content_type) {
    return run(cfg, "PUT", key, body, "", content_type);
}

Result put_file(const Config& cfg, const std::string& key,
                const std::string& path, const std::string& content_type) {
    return run(cfg, "PUT", key, "", path, content_type);
}

Result get_object(const Config& cfg, const std::string& key) {
    return run(cfg, "GET", key, "", "", "");
}

Result check_access(const Config& cfg) {
    /* The smallest REAL operation against the actual bucket, rather than a
     * credential validator in the abstract. The field agent's finding, which is
     * now a rule here: a green light that does not predict the operation is
     * worse than no light. */
    Config c = cfg;
    Request req;
    req.method = "GET";
    req.host = host_for(cfg);
    req.path = "/";
    req.query["list-type"] = "2";
    req.query["max-keys"] = "1";
    req.region = cfg.region;
    req.service = "s3";

    const Signed s = sign(req, cfg.creds);
    Result r;
    if (!cfg.shell) {
        r.error = "no shell transport on this front-end";
        return r;
    }
    const fs::path tmp =
        cfg.work_dir.empty() ? fs::temp_directory_path() : fs::path(cfg.work_dir);
    const fs::path cfgf = tmp / ".aws-curl.cfg";
    {
        std::ofstream c(cfgf, std::ios::binary | std::ios::trunc);
        if (!c) {
            r.error = "cannot write " + cfgf.string();
            return r;
        }
        for (const auto& [k, v] : s.headers)
            if (k != "host") c << "header = " << q(k + ": " + v) << "\n";
        c << "header = " << q("Authorization: " + s.authorization) << "\n";
    }
    const std::string url = "https://" + req.host + "/?list-type=2&max-keys=1";
    const std::string resp =
        cfg.shell("curl -sS --config " + q(cfgf.string()) + " " + q(url) + " 2>&1");
    std::error_code ec;
    fs::remove(cfgf, ec);
    if (looks_like_error(resp) || resp.rfind("curl:", 0) == 0) {
        r.error = resp.substr(0, 1200);
        return r;
    }
    r.ok = true;
    r.body = resp;
    return r;
}

} // namespace hormiga::aws
