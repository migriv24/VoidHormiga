/* github.cpp — the GitHub Pages deploy. See github.hpp for the protocol and
 * for why this exists alongside the Cloudflare path rather than instead of it.
 */
#include "publish/github.hpp"

#include "publish/http.hpp"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace hormiga::github {
namespace {

constexpr const char* kApi = "https://api.github.com";

/* GitHub requires a User-Agent and versions its API by header. Both are sent on
 * every call; a missing UA is a 403 whose body says nothing about user agents. */
std::vector<std::string> hdrs() {
    return {"Accept: application/vnd.github+json",
            "X-GitHub-Api-Version: 2022-11-28",
            "User-Agent: VoidHormiga",
            "Content-Type: application/json"};
}

std::string read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/* GitHub's error body is a bare object with `message`, and often a `documentation_url`
 * and a list of `errors`. Passed through as close to verbatim as fits on a log
 * line, for the reason the Cloudflare path learned the hard way: the vendor's
 * own sentence is routinely the entire diagnosis, and summarising it costs an
 * hour. */
std::string why(const http::Response& r) {
    try {
        const auto j = nlohmann::json::parse(r.body);
        std::string m = j.value("message", std::string());
        if (j.contains("errors"))
            for (const auto& e : j["errors"]) {
                if (e.is_object()) {
                    const std::string em = e.value("message", std::string());
                    if (!em.empty()) m += "; " + em;
                    else m += "; " + e.value("code", std::string());
                }
            }
        if (!m.empty()) return "HTTP " + std::to_string(r.status) + ": " + m;
    } catch (...) {
    }
    if (r.status == 0)
        return r.body.empty() ? "no response (is curl on PATH?)"
                              : r.body.substr(0, 400);
    return "HTTP " + std::to_string(r.status) + ": " + r.body.substr(0, 400);
}

fs::path tmpdir(const Config& cfg) {
    return cfg.work_dir.empty() ? fs::temp_directory_path() : fs::path(cfg.work_dir);
}

http::Response api(const Config& cfg, const std::string& method,
                   const std::string& path, const std::string& body) {
    return http::call(cfg.shell, method, std::string(kApi) + path, cfg.token,
                      hdrs(), body, tmpdir(cfg), "gh");
}

} // namespace

bool split_repo(const std::string& spec, std::string& owner, std::string& repo) {
    std::string s = spec;
    // tolerate a pasted URL: https://github.com/owner/repo(.git)
    const size_t gh = s.find("github.com");
    if (gh != std::string::npos) {
        s = s.substr(gh + 10);
        if (!s.empty() && (s[0] == '/' || s[0] == ':')) s.erase(0, 1);
    }
    while (!s.empty() && s.back() == '/') s.pop_back();
    if (s.size() > 4 && s.compare(s.size() - 4, 4, ".git") == 0) s.resize(s.size() - 4);
    const size_t slash = s.find('/');
    if (slash == std::string::npos || slash == 0 || slash + 1 >= s.size())
        return false;
    owner = s.substr(0, slash);
    repo = s.substr(slash + 1);
    return repo.find('/') == std::string::npos;
}

std::string looks_like_token(const std::string& contents) {
    std::string t = contents;
    while (!t.empty() && (unsigned char)t.back() <= ' ') t.pop_back();
    const size_t b = t.find_first_not_of(" \t\r\n");
    t = b == std::string::npos ? std::string() : t.substr(b);

    if (t.empty()) return "the file is empty";
    if (t.find('\n') != std::string::npos)
        return "the file has more than one line - it should hold only the token";
    /* The prefixes GitHub actually issues. `ghp_` is a classic PAT, `github_pat_`
     * a fine-grained one, `gho_`/`ghu_`/`ghs_` are OAuth/app tokens that will
     * not have repository contents write. Anything else is accepted with a
     * shrug rather than refused: GitHub Enterprise and future prefixes are real,
     * and a shape test that refuses a working token is worse than one that lets
     * a bad one through to a call that will say so precisely. */
    if (t.rfind("ghp_", 0) == 0 || t.rfind("github_pat_", 0) == 0) return {};
    if (t.rfind("gho_", 0) == 0 || t.rfind("ghu_", 0) == 0 ||
        t.rfind("ghs_", 0) == 0)
        return "that is an OAuth/app token (" + t.substr(0, 4) +
               "...), not a personal access token - create one at GitHub > "
               "Settings > Developer settings > Personal access tokens";
    if (t.size() < 20)
        return "too short for a GitHub token (" + std::to_string(t.size()) +
               " characters)";
    for (char c : t)
        if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-'))
            return "contains characters a GitHub token does not - is this the "
                   "right file?";
    return {};
}

std::vector<Step> check_host(const Config& cfg) {
    std::vector<Step> out;
    auto add = [&](bool ok, std::string what, std::string detail) {
        out.push_back({ok, std::move(what), std::move(detail)});
        return ok;
    };
    if (cfg.owner.empty() || cfg.repo.empty()) {
        add(false, "read the repository name",
            "set `repo` on the host node to owner/repo");
        return out;
    }
    const std::string base = "/repos/" + cfg.owner + "/" + cfg.repo;

    /* 1. The repository, which is the smallest real call the deploy makes and
     *    the one that distinguishes every credential problem from every naming
     *    problem: 401 is the token, 404 is the name OR a token that cannot see
     *    a private repo, 200 is neither. */
    const http::Response repo = api(cfg, "GET", base, "");
    if (repo.status != 200) {
        add(false, "read " + cfg.owner + "/" + cfg.repo,
            repo.status == 404
                ? why(repo) + " - either the repository does not exist, or this "
                              "token cannot see it (a fine-grained token must "
                              "list this repository explicitly)"
                : why(repo));
        return out;
    }
    bool can_push = false;
    std::string default_branch;
    try {
        const auto j = nlohmann::json::parse(repo.body);
        default_branch = j.value("default_branch", std::string());
        if (j.contains("permissions"))
            can_push = j["permissions"].value("push", false);
    } catch (...) {
    }
    add(true, "read " + cfg.owner + "/" + cfg.repo,
        default_branch.empty() ? "ok" : "default branch " + default_branch);
    /* `permissions.push` is what a deploy needs and is reported separately from
     * reading the repo, because a read-only token reads the repository
     * perfectly and then fails at step 3 of 7 — after the blobs have been
     * uploaded. A check that passes and a publish that fails halfway is the
     * outcome `check-store` was written to prevent. */
    add(can_push, "this token can write to it",
        can_push ? "ok"
                 : "the token can read this repository but not write to it - a "
                   "fine-grained token needs `Contents: read and write`, a "
                   "classic one needs `repo`");

    // 2. The branch Pages serves. Absent is FINE on a first publish.
    const http::Response ref =
        api(cfg, "GET", base + "/git/ref/heads/" + cfg.branch, "");
    add(true, "branch " + cfg.branch,
        ref.status == 200 ? "exists"
        : ref.status == 404
            ? "does not exist yet - the first publish creates it"
            : why(ref));

    // 3. Is Pages actually on, and pointed where we are about to publish?
    const http::Response pg = api(cfg, "GET", base + "/pages", "");
    if (pg.status == 200) {
        std::string url, src_branch;
        try {
            const auto j = nlohmann::json::parse(pg.body);
            url = j.value("html_url", std::string());
            if (j.contains("source")) src_branch = j["source"].value("branch", std::string());
        } catch (...) {
        }
        const bool aimed = src_branch.empty() || src_branch == cfg.branch;
        add(aimed, "GitHub Pages is on",
            aimed ? (url.empty() ? "ok" : "serving " + url)
                  : "Pages is serving branch '" + src_branch +
                        "' but this node publishes to '" + cfg.branch +
                        "' - point one of them at the other, or the deploy will "
                        "succeed and change nothing anyone can see");
    } else if (pg.status == 404) {
        add(true, "GitHub Pages is not enabled yet",
            cfg.enable_pages
                ? "the first publish turns it on (needs `Pages: read and write` "
                  "on the token)"
                : "enable it in the repository's Settings > Pages");
    } else {
        add(false, "read the Pages settings", why(pg));
    }
    return out;
}

DeployResult deploy(const Config& cfg) {
    DeployResult r;
    auto step = [&](bool ok, std::string what, std::string detail) {
        r.steps.push_back({ok, std::move(what), std::move(detail)});
        // successes only through `progress`: it has no severity, and a failure
        // filed under the same [info] as "uploaded 30 file(s)" is a failure the
        // operator reads as progress. Same rule as the Cloudflare path.
        if (ok && cfg.progress) cfg.progress(r.steps.back().what);
        return ok;
    };
    if (cfg.owner.empty() || cfg.repo.empty()) {
        step(false, "read the repository name",
             "set `repo` on the host node to owner/repo");
        return r;
    }
    const std::string base = "/repos/" + cfg.owner + "/" + cfg.repo;
    std::error_code ec;

    // ── gather the built site ───────────────────────────────────────────────
    struct FileEnt { std::string rel; fs::path path; };
    std::vector<FileEnt> files;
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
        files.push_back({fs::relative(p, root, ec).generic_string(), p});
    }
    if (files.empty()) {
        step(false, "read the built site", "no files in " + cfg.site_dir);
        return r;
    }
    step(true, "read " + std::to_string(files.size()) + " file(s)", "ok");

    // ── the parent commit, if the branch is already there ───────────────────
    std::string parent;
    {
        const http::Response ref =
            api(cfg, "GET", base + "/git/ref/heads/" + cfg.branch, "");
        if (ref.status == 200) {
            try {
                parent = nlohmann::json::parse(ref.body)["object"].value(
                    "sha", std::string());
            } catch (...) {
            }
            if (parent.empty() &&
                !step(false, "read branch " + cfg.branch,
                      "GitHub answered 200 with no object sha: " +
                          ref.body.substr(0, 200)))
                return r;
            step(true, "branch " + cfg.branch + " is at " + parent.substr(0, 8), "ok");
        } else if (ref.status == 404) {
            /* THE ORDINARY FIRST PUBLISH. A fresh `gh-pages` does not exist
             * until something creates it, and refusing here would mean this
             * holiday could never make the deploy it was added for. The commit
             * below is parentless and the ref is created rather than moved. */
            step(true, "branch " + cfg.branch + " does not exist yet",
                 "this publish creates it");
        } else {
            step(false, "read branch " + cfg.branch, why(ref));
            return r;
        }
    }

    // ── one blob per file ───────────────────────────────────────────────────
    struct TreeEnt { std::string path, sha; };
    std::vector<TreeEnt> tree;
    for (const FileEnt& f : files) {
        nlohmann::json body;
        body["content"] = http::base64(read_file(f.path));
        body["encoding"] = "base64";
        const http::Response b = api(cfg, "POST", base + "/git/blobs", body.dump());
        if (b.status != 201) {
            step(false, "upload " + f.rel, why(b));
            return r;
        }
        std::string sha;
        try {
            sha = nlohmann::json::parse(b.body).value("sha", std::string());
        } catch (...) {
        }
        if (sha.empty()) {
            step(false, "upload " + f.rel,
                 "GitHub accepted the blob and returned no sha: " +
                     b.body.substr(0, 200));
            return r;
        }
        tree.push_back({f.rel, sha});
    }
    /* `.nojekyll`, added to the TREE and not to `site/`. Without it GitHub runs
     * Jekyll over the branch and silently drops every path beginning with an
     * underscore — a failure that presents as "three images are missing from
     * the gallery" and points at nothing. It is a property of this host, so a
     * Cloudflare deploy has no business carrying it. */
    {
        nlohmann::json body;
        body["content"] = http::base64("");
        body["encoding"] = "base64";
        const http::Response b = api(cfg, "POST", base + "/git/blobs", body.dump());
        if (b.status != 201) {
            step(false, "add .nojekyll", why(b));
            return r;
        }
        try {
            tree.push_back({".nojekyll",
                            nlohmann::json::parse(b.body).value("sha", std::string())});
        } catch (...) {
        }
    }
    /* A CNAME file when a custom domain is wired. GitHub reads this file — not
     * an API field — as the authority on which domain the branch is served at,
     * and a deploy that omits it UNSETS a custom domain that was configured in
     * the web UI. So it is written whenever we know the domain, which is
     * whenever a `hol_dns` node is plugged into this host. */
    if (!cfg.cname.empty()) {
        nlohmann::json body;
        body["content"] = http::base64(cfg.cname + "\n");
        body["encoding"] = "base64";
        const http::Response b = api(cfg, "POST", base + "/git/blobs", body.dump());
        if (b.status != 201) {
            step(false, "add CNAME (" + cfg.cname + ")", why(b));
            return r;
        }
        try {
            tree.push_back({"CNAME",
                            nlohmann::json::parse(b.body).value("sha", std::string())});
        } catch (...) {
        }
    }
    step(true, "uploaded " + std::to_string(tree.size()) + " file(s) as blobs", "ok");

    // ── the tree: the whole site, with no base_tree ─────────────────────────
    std::string tree_sha;
    {
        nlohmann::json body;
        body["tree"] = nlohmann::json::array();
        for (const TreeEnt& t : tree)
            body["tree"].push_back({{"path", t.path},
                                    {"mode", "100644"},
                                    {"type", "blob"},
                                    {"sha", t.sha}});
        /* NO `base_tree` ON PURPOSE. With one, the commit is a patch and a page
         * deleted from the model stays live forever. Without one, the tree IS
         * the built folder and the deploy is a mirror — which is what `site/`
         * already is of the database. */
        const http::Response t = api(cfg, "POST", base + "/git/trees", body.dump());
        if (t.status != 201) {
            step(false, "build the tree", why(t));
            return r;
        }
        try {
            tree_sha = nlohmann::json::parse(t.body).value("sha", std::string());
        } catch (...) {
        }
        if (tree_sha.empty()) {
            step(false, "build the tree",
                 "GitHub accepted the tree and returned no sha: " +
                     t.body.substr(0, 200));
            return r;
        }
    }

    // ── the commit ──────────────────────────────────────────────────────────
    std::string commit;
    {
        nlohmann::json body;
        body["message"] = cfg.message.empty()
                              ? std::string("Publish site (Void Hormiga)")
                              : cfg.message;
        body["tree"] = tree_sha;
        body["parents"] = nlohmann::json::array();
        if (!parent.empty()) body["parents"].push_back(parent);
        const http::Response c = api(cfg, "POST", base + "/git/commits", body.dump());
        if (c.status != 201) {
            step(false, "commit", why(c));
            return r;
        }
        try {
            commit = nlohmann::json::parse(c.body).value("sha", std::string());
        } catch (...) {
        }
        if (commit.empty()) {
            step(false, "commit",
                 "GitHub accepted the commit and returned no sha: " +
                     c.body.substr(0, 200));
            return r;
        }
        step(true, "committed " + commit.substr(0, 8), "ok");
    }

    // ── move (or create) the branch ─────────────────────────────────────────
    {
        http::Response mv;
        if (parent.empty()) {
            nlohmann::json body;
            body["ref"] = "refs/heads/" + cfg.branch;
            body["sha"] = commit;
            mv = api(cfg, "POST", base + "/git/refs", body.dump());
            if (mv.status != 201) {
                step(false, "create branch " + cfg.branch, why(mv));
                return r;
            }
        } else {
            nlohmann::json body;
            body["sha"] = commit;
            /* FORCE, because a publish is a mirror of the built folder and not
             * a merge with whatever is on the branch. Anything on `gh-pages`
             * that Hormiga did not put there is not content — it is a previous
             * render — and a non-fast-forward here would be a deploy that
             * refuses because the site changed, which is every deploy. */
            body["force"] = true;
            mv = api(cfg, "PATCH", base + "/git/refs/heads/" + cfg.branch,
                     body.dump());
            if (mv.status != 200) {
                step(false, "move branch " + cfg.branch, why(mv));
                return r;
            }
        }
        step(true, "branch " + cfg.branch + " now at " + commit.substr(0, 8), "ok");
    }

    // ── make sure Pages is on, and find out where the site is ───────────────
    std::string url;
    {
        http::Response pg = api(cfg, "GET", base + "/pages", "");
        if (pg.status == 404 && cfg.enable_pages) {
            /* ── THE PART `deploy-site` COULD NOT DO (field report A5) ───────
             *
             * "The first deploy to a new host needed two steps Hormiga cannot
             * take … Create the Pages project. `deploy-site` publishes into a
             * project and fails if it does not exist."
             *
             * On Cloudflare that is still true. Here it is one call, so it is
             * made — and it is made AFTER the branch exists, because GitHub
             * refuses to enable Pages on a branch that is not there yet, and a
             * refusal in that order would be indistinguishable from a bad
             * token. A failure is reported and does not sink the deploy: the
             * commit is already on the branch, and enabling Pages by hand in
             * the repository settings is a thirty-second job that does not need
             * a re-upload. */
            nlohmann::json body;
            body["source"] = {{"branch", cfg.branch}, {"path", "/"}};
            const http::Response on = api(cfg, "POST", base + "/pages", body.dump());
            if (on.status == 201 || on.status == 204) {
                step(true, "turned GitHub Pages on for " + cfg.branch, "ok");
                pg = api(cfg, "GET", base + "/pages", "");
            } else {
                r.steps.push_back({true, "GitHub Pages is not enabled",
                                   why(on) + " - the commit is published; turn "
                                             "Pages on in the repository's "
                                             "Settings > Pages, source branch " +
                                       cfg.branch});
                if (cfg.progress) cfg.progress(r.steps.back().what + ": " +
                                               r.steps.back().detail);
            }
        }
        if (pg.status == 200) {
            try {
                url = nlohmann::json::parse(pg.body).value("html_url", std::string());
            } catch (...) {
            }
        }
    }
    /* The address GitHub itself reports, else the one the naming rules make
     * certain: `owner.github.io` is served at the apex and every other
     * repository under `/repo/`. Constructed rather than left empty because the
     * `deployment` rune's `url` is what a person clicks to check the publish,
     * and "we published something somewhere" is not a record. */
    if (url.empty()) {
        std::string ol = cfg.owner;
        for (char& c : ol) c = (char)std::tolower((unsigned char)c);
        std::string rl = cfg.repo;
        for (char& c : rl) c = (char)std::tolower((unsigned char)c);
        url = "https://" + ol + ".github.io" + (rl == ol + ".github.io" ? "/" : "/" + cfg.repo + "/");
    }
    if (!cfg.cname.empty()) url = "https://" + cfg.cname + "/";

    r.ok = true;
    r.url = url;
    r.vendor_id = commit;
    return r;
}

DeployResult rollback(const Config& cfg, const std::string& commit_sha) {
    DeployResult r;
    if (cfg.owner.empty() || cfg.repo.empty()) {
        r.steps.push_back({false, "read the repository name",
                           "set `repo` on the host node to owner/repo"});
        return r;
    }
    if (commit_sha.empty()) {
        r.steps.push_back({false, "read the commit to restore",
                           "name the deployment to restore"});
        return r;
    }
    const std::string base = "/repos/" + cfg.owner + "/" + cfg.repo;
    /* THE COMMIT IS CHECKED BEFORE THE BRANCH IS MOVED. A force-update onto a
     * sha that is not in this repository is refused by GitHub, but a sha that IS
     * in the repository and is not a site build would publish something nobody
     * asked for — and restoring is exactly the moment an operator is least able
     * to look. Reading it first costs one call. */
    const http::Response c = api(cfg, "GET", base + "/git/commits/" + commit_sha, "");
    if (c.status != 200) {
        r.steps.push_back({false, "find commit " + commit_sha.substr(0, 8), why(c)});
        return r;
    }
    nlohmann::json body;
    body["sha"] = commit_sha;
    body["force"] = true;
    const http::Response mv =
        api(cfg, "PATCH", base + "/git/refs/heads/" + cfg.branch, body.dump());
    if (mv.status != 200) {
        r.steps.push_back({false, "move branch " + cfg.branch, why(mv)});
        return r;
    }
    r.ok = true;
    r.vendor_id = commit_sha;
    r.steps.push_back({true,
                       "branch " + cfg.branch + " restored to " +
                           commit_sha.substr(0, 8),
                       "ok"});
    return r;
}

} // namespace hormiga::github
