/* cloudflare.hpp — publishing a site to Cloudflare Pages, natively.
 *
 * ── WHY THIS EXISTS, AND WHY IT IS A REVERSAL ────────────────────────────────
 *
 * On 2026-08-19 this project deliberately did NOT implement the vendor call. It
 * shipped `deploy_cmd` — a command template on the Antfarm node — with the
 * reasoning that a backend is configuration, and that an API flow this side
 * could not execute even once was one it had no business asserting.
 *
 * The operator overruled it on 2026-08-20, and was right:
 *
 *   > why are we relying on a python script from an external source to deploy
 *   > pages? that should be functionality that hormiga itself should have. if
 *   > we have to write more code for the antfarm specifically, then lets do
 *   > that!
 *
 * What the intervening day showed is that `deploy_cmd` did not remove the
 * dependency, it *relocated* it. Publishing a live site required Node,
 * npm, `wrangler@3` pinned by hand, Python, and a `deploy_pages.py` living in
 * one operator's folder — five things a second organization would not have, to
 * upload a folder of HTML. "Configuration, not code" is a good rule that had
 * turned into "somebody else's code, somewhere we cannot see it".
 *
 * `deploy_cmd` REMAINS and still wins when set. It is the right escape hatch
 * for a host Hormiga has never heard of, and it is how this native path can be
 * bypassed the day Cloudflare changes something. What changes is the default:
 * with a token and a project, publishing now works out of the box.
 *
 * ── THE PROTOCOL, read from wrangler's source rather than guessed ────────────
 *
 *   1. POST /accounts/{acct}/pages/projects/{proj}/upload-token   → a JWT
 *   2. POST /pages/assets/check-missing   {hashes}  → which we must send
 *   3. POST /pages/assets/upload          [{key,value,metadata,base64}]
 *   4. POST /pages/assets/upsert-hashes   {hashes}  (a caching hint)
 *   5. POST /accounts/{acct}/pages/projects/{proj}/deployments
 *          multipart: manifest={"/path":hash,…}, branch=…
 *
 * Every asset is keyed by
 *
 *     blake3( base64(contents) + extension_without_dot ).hex()[:32]
 *
 * which is why `vendor/blake3` exists. Steps 2–4 authenticate with the JWT from
 * step 1; steps 1 and 5 with the account API token.
 *
 * ── THE SECRET NEVER TOUCHES A COMMAND LINE ──────────────────────────────────
 *
 * Every request goes through `curl --config <file>`, with the bearer header in
 * the file and the file deleted when the call returns. Anything in argv is
 * readable in a process listing by anything running as this user.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hormiga::cloudflare {

/* One step's outcome, as the panel and the log want it. */
struct Step {
    bool ok = false;
    std::string what;   // "asked for an upload token"
    std::string detail; // the vendor's own words on failure, verbatim
};

struct DeployResult {
    bool ok = false;
    std::string url;      // the deployment's own permanent address
    std::string vendor_id; // the deployment id, for rollback
    std::vector<Step> steps;
};

/* Everything the flow needs, so the caller does the policy and this does the
 * protocol. `shell` runs a command and returns its combined output. */
struct Config {
    std::string account_id;
    std::string project;
    std::string token;
    std::string branch = "main";
    std::string site_dir;               // absolute path to the built site/
    std::string work_dir;               // where temp request files may live
    std::function<std::string(const std::string&)> shell;
    std::function<void(const std::string&)> progress; // optional, per step
};

/* Publish `site_dir`. Never throws; every failure comes back in `steps` with
 * the vendor's message unedited — the field agent's finding of 2026-08-20 was
 * that Cloudflare's own error ("Cannot use the access token from location:
 * 2601:…") contained the entire diagnosis, and that summarising it would have
 * cost an hour. */
DeployResult deploy(const Config& cfg);

/* Can this token actually do the job?
 *
 * NOT `/tokens/verify`, deliberately. It answers `{"status":"active"}` for a
 * token that returns 9109 on every real call, because it does not enforce the
 * token's own IP condition — which is exactly the trap the field agent fell into,
 * with a green light that predicted nothing. This reads the resource the deploy
 * actually touches: the account's Pages projects. */
Step check_token(const Config& cfg);

/* A secret as it comes out of a file: trailing newline and stray whitespace
 * removed. A token with a newline on the end is rejected by every API with an
 * error that names authentication rather than whitespace, which is a bad hour.
 *
 * Lived as a file-local `static` in publish.cpp until the panel was split out
 * and needed it too - the same "shared by proximity rather than by dependency"
 * the src/ restructure keeps turning up. */
inline std::string trim_secret(std::string s) {
    while (!s.empty() && (unsigned char)s.back() <= ' ') s.pop_back();
    const size_t b = s.find_first_not_of(" \t\r\n");
    return b == std::string::npos ? std::string() : s.substr(b);
}

/* Does this text look like a Cloudflare API token at all?
 *
 * A cheap shape test, for the file picker. Cloudflare API tokens are 40
 * characters of [A-Za-z0-9_-]; a `cfut_…` string is an upload token and will
 * fail every call (also learned the hard way). Returns "" when it looks fine,
 * or a sentence saying what is wrong with the file. */
std::string looks_like_token(const std::string& file_contents);

/* Roll a past deployment back to live. */
DeployResult rollback(const Config& cfg, const std::string& deployment_id);

} // namespace hormiga::cloudflare
