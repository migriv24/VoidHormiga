/* http.hpp — the two things every cloud holiday in this folder needs: base64,
 * and one curl call whose secret never touches a command line.
 *
 * ── WHY THIS EXISTS ──────────────────────────────────────────────────────────
 *
 * `cloudflare.cpp` shipped first and grew its own `b64`, `q`, `post_json` and
 * `get` in an anonymous namespace, which was correct at the time: one vendor,
 * one file, no seam to guess at. The GitHub Pages deployer (2026-09-02) is the
 * second, and a second private copy of base64 is how two backends start
 * disagreeing about what a byte is.
 *
 * What is here is only what is genuinely vendor-neutral. The ENVELOPE is not:
 * Cloudflare answers `{"success":…,"errors":[…]}` and GitHub answers a bare
 * object with `message` on failure, and flattening those into one "did it
 * work?" would throw away the vendor's own sentence — which the 2026-08-20
 * field report established is routinely the entire diagnosis. Each holiday
 * reads its own vendor's words.
 *
 * ── THE SECRET NEVER TOUCHES A COMMAND LINE ──────────────────────────────────
 *
 * Every request goes through `curl --config <file>`, with the bearer header in
 * the file and the file removed when the call returns. Anything in argv is
 * readable in a process listing by anything running as this user, and a deploy
 * token can publish a website.
 */
#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace hormiga::http {

namespace fs = std::filesystem;

/* Shell-quote a path for the command line. Paths only — never a secret. */
inline std::string q(const std::string& s) { return "\"" + s + "\""; }

/* Standard base64. Cloudflare hashes it to key an asset; GitHub sends it as a
 * blob's `content`. One implementation, so the two cannot drift. */
inline std::string base64(const std::string& in) {
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

/* One HTTP call's outcome. `status` is 0 when curl itself failed (not on PATH,
 * DNS, a proxy) — which is a different problem from a 404 and must not be
 * reported as one. */
struct Response {
    int status = 0;
    std::string body;
};

/* A curl call with a bearer token and optional extra headers.
 *
 * The status code matters here in a way it did not for Cloudflare: GitHub says
 * "this branch does not exist yet" with a 404 on a call whose body is a
 * perfectly ordinary error object, and "this branch does not exist yet" is the
 * NORMAL first publish to a fresh `gh-pages`. A holiday that could not tell 404
 * from 401 would refuse the first deploy it was ever asked to make.
 *
 * `-w` appends the code on its own line after the body; anything else on that
 * last line means curl wrote a diagnostic there instead, and the status stays
 * 0 rather than being guessed at. */
inline Response call(const std::function<std::string(const std::string&)>& shell,
                     const std::string& method, const std::string& url,
                     const std::string& bearer,
                     const std::vector<std::string>& headers,
                     const std::string& body, const fs::path& tmp,
                     const char* tag) {
    Response r;
    if (!shell) return r;
    const fs::path cfgf = tmp / (std::string(".") + tag + "-curl.cfg");
    const fs::path bodyf = tmp / (std::string(".") + tag + "-body.json");
    {
        std::ofstream c(cfgf, std::ios::binary | std::ios::trunc);
        if (!bearer.empty())
            c << "header = \"Authorization: Bearer " << bearer << "\"\n";
        for (const std::string& h : headers) c << "header = \"" << h << "\"\n";
    }
    std::string cmd = "curl -sS --config " + q(cfgf.string()) + " -X " + method;
    if (!body.empty()) {
        std::ofstream b(bodyf, std::ios::binary | std::ios::trunc);
        b << body;
        b.close();
        cmd += " --data-binary @" + q(bodyf.string());
    }
    cmd += " -w \"\\n%{http_code}\" " + q(url) + " 2>&1";

    const std::string out = shell(cmd);
    std::error_code ec;
    fs::remove(cfgf, ec);
    fs::remove(bodyf, ec);

    const size_t nl = out.find_last_of('\n');
    if (nl == std::string::npos) { r.body = out; return r; }
    const std::string tail = out.substr(nl + 1);
    bool numeric = !tail.empty() && tail.size() <= 4;
    for (char c : tail) numeric = numeric && c >= '0' && c <= '9';
    if (!numeric) { r.body = out; return r; }
    r.status = std::stoi(tail);
    r.body = out.substr(0, nl);
    return r;
}

} // namespace hormiga::http
