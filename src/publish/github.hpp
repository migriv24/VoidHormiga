/* github.hpp — publishing a site to GitHub Pages, natively.
 *
 * ── WHY GITHUB PAGES, WHEN CLOUDFLARE PAGES ALREADY WORKS ────────────────────
 *
 * The roadmap has said "deploy holidays (folder, GitHub Pages)" since phase E
 * was written, and `hol_github` has been in the Antfarm palette — with a `repo`
 * field, a `site` input port, a colour and a label — since the holidays were
 * first registered. Nothing read any of it. A node an operator can place, wire
 * and configure, that does nothing when they press publish, is the same defect
 * the 2026-09-02 field report named about `image_grid.columns`: *a field that
 * does nothing is worse than no field*, and this was a whole holiday of them.
 *
 * The substantive reason is `web-platform.md`'s acceptance test for every
 * vendor on that page: **every cloud host is disposable.** A single deploy
 * target makes that a claim rather than a property. Two targets that publish
 * the same `site/` folder from the same command make it something an
 * organization can exercise on a Tuesday — and GitHub Pages is the one an
 * organization already has, for free, without a card on file.
 *
 * ── THE PROTOCOL: the Git Data API, not the Pages API ────────────────────────
 *
 * GitHub Pages has no "upload these files" endpoint. What it has is a branch it
 * serves, so publishing is a git commit made over REST:
 *
 *   1. GET   /repos/{o}/{r}                          — does it exist, can we see it
 *   2. GET   /repos/{o}/{r}/git/ref/heads/{branch}   — the parent, or 404 = fresh
 *   3. POST  /repos/{o}/{r}/git/blobs                — one per file, base64
 *   4. POST  /repos/{o}/{r}/git/trees                — the WHOLE tree, no base_tree
 *   5. POST  /repos/{o}/{r}/git/commits              — parented on step 2
 *   6. PATCH /repos/{o}/{r}/git/refs/heads/{branch}  — move the branch (force)
 *   7. GET/POST /repos/{o}/{r}/pages                 — enable it if it is off
 *
 * NO `base_tree` IN STEP 4, and that is a decision rather than a shortcut. With
 * a base tree the new commit is a patch over the old one, so a page deleted
 * from the model stays live on the site forever. Without it the tree IS the
 * built folder, and the deploy is a mirror — which is what `site/` already is
 * of the database. Publishing must not be able to leave something behind that
 * the model no longer contains.
 *
 * ── `.nojekyll`, WHICH IS NOT OPTIONAL ───────────────────────────────────────
 *
 * GitHub Pages runs Jekyll over the branch unless a `.nojekyll` file sits at its
 * root, and Jekyll silently drops every file and folder whose name begins with
 * an underscore. Hormiga does not currently emit one — but a site is a folder of
 * whatever an operator's assets are called, and "your gallery is missing the
 * three images whose filenames start with `_`" is a bug report nobody will
 * connect to a static-site generator they did not know was running. The file is
 * added to the tree here rather than written into `site/`, because it is a
 * property of this host and not of the built site: a Cloudflare deploy has no
 * business carrying it.
 *
 * ── ROLLBACK IS FREE, AND IT IS THE REASON THE COMMIT SHA IS THE vendor_id ────
 *
 * A `deployment` rune keeps `vendor_id` so a past version can be restored. On
 * Cloudflare that is a deployment id recovered from a preview URL. Here it is
 * the commit sha, which is the real thing rather than a recovered label — and
 * rolling back is one force-update of the ref back onto it. The old tree is
 * still in the repository; nothing has to be re-uploaded.
 *
 * ── THE TOKEN ────────────────────────────────────────────────────────────────
 *
 * A fine-grained personal access token with `Contents: read and write` on the
 * one repository (add `Pages: read and write` to let this create the Pages site
 * on first publish), or a classic token with `repo`. It rides the same two
 * doors every other credential in this application does: `token_key` in the
 * passphrase-locked vault first, `token_file` beside the database second, and
 * it never appears in argv.
 */
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace hormiga::github {

/* One step's outcome, shaped like `cloudflare::Step` on purpose: the publish
 * panel and `deploy_site`'s logging read both, and a second shape would mean a
 * second reader. */
struct Step {
    bool ok = false;
    std::string what;   // "uploaded 30 file(s) as blobs"
    std::string detail; // GitHub's own words on failure, verbatim
};

struct DeployResult {
    bool ok = false;
    std::string url;       // where the site is served
    std::string vendor_id; // the commit sha — what a rollback needs
    std::vector<Step> steps;
};

struct Config {
    std::string owner;               // "clicklafont"
    std::string repo;                // "clicklafont.github.io"
    std::string branch = "gh-pages"; // the branch Pages serves
    std::string token;
    std::string cname;    // custom domain → a CNAME file in the tree ("" = none)
    std::string message;  // commit message ("" = a generated one)
    bool enable_pages = true; // turn Pages on if it is off (needs Pages: write)
    std::string site_dir; // absolute path to the built site/
    std::string work_dir; // where temp request files may live
    std::function<std::string(const std::string&)> shell;
    std::function<void(const std::string&)> progress;
};

/* Publish `site_dir` as one commit on `branch`. Never throws; every failure
 * comes back in `steps` with GitHub's message unedited. */
DeployResult deploy(const Config& cfg);

/* Can these credentials actually publish to this repository?
 *
 * The sibling of `cloudflare::check_token`, and it makes the same argument: it
 * performs a REAL read against the resource a deploy touches rather than asking
 * the vendor whether a token is valid in the abstract. The field report's note
 * on Cloudflare — an account-scoped `cfat_…` token that answers
 * `Invalid API Token` to `/user/tokens/verify` while working perfectly against
 * every account endpoint — is the exact trap: **a green light that does not
 * predict the operation is worse than no light.**
 *
 * Reports the repository, whether the branch exists yet, and whether Pages is
 * turned on and pointed at that branch, because those are three different
 * problems with three different fixes and only one of them is the token. */
std::vector<Step> check_host(const Config& cfg);

/* Point `branch` back at a past commit. The old tree is already in the
 * repository, so this uploads nothing. */
DeployResult rollback(const Config& cfg, const std::string& commit_sha);

/* Split "owner/repo" (or a full GitHub URL) into its two halves. Returns false
 * when there is no sensible reading — a host node configured with a bare name
 * is a refusal, not a guess about whose repository was meant. */
bool split_repo(const std::string& spec, std::string& owner, std::string& repo);

/* Does this text look like a GitHub token at all? A cheap shape test for the
 * file picker and for `check-host`'s first line. Returns "" when it looks fine,
 * or a sentence saying what is wrong with the file. */
std::string looks_like_token(const std::string& file_contents);

} // namespace hormiga::github
