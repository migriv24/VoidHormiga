/* push.cpp — putting a small artifact into an object store.
 *
 * ── THE POINT, IN ONE SENTENCE ───────────────────────────────────────────────
 *
 * Updating one contact should cost one small file being republished, not a whole
 * site being rendered and redeployed. That was the operator's complaint
 * (okf/concepts/platform/web-platform.md §7.2) and `publish-index` produced the artifact
 * that answers it; this is the half that gets it there.
 *
 * ── WHY `what` IS A CLOSED SET AND NOT A PATH ────────────────────────────────
 *
 * The obvious signature is `push <node> <path>`, and it is wrong. An effect that
 * uploads an arbitrary path is a way to exfiltrate anything on the operator's
 * disk with one command — including the unencrypted database, the vault, and the
 * token files sitting beside it. Nobody would design that on purpose; it is what
 * you get by making the argument general because general felt tidier.
 *
 * So there are two artifacts, named, and each maps to a fixed source:
 *
 *   index   -> site/index/*        the published projection. Cleared the
 *                                  clearance seam by construction, because
 *                                  `publish_index` is what wrote it.
 *   backup  -> <document>.bkp      the encrypted blob. Opaque to the store.
 *
 * Adding a third is an edit here, under this comment, by someone who has read
 * it — which is exactly the friction that decision deserves.
 *
 * ── AND WHY THE INDEX IS SAFE TO PUSH BUT THE DATABASE IS NOT ────────────────
 *
 * data-planes.md §1: the publication plane is a DERIVATIVE that has passed the
 * gate, and the admin plane never leaves except encrypted. `index` is the first;
 * `backup` is the second. There is deliberately no third option that would send
 * the admin plane in the clear, and this file is the reason there is not.
 */
#include "app/app_internal.hpp"
#include "publish/aws.hpp"
#include "publish/cloudflare.hpp"   // trim_secret: one reader for every credential

#include <fstream>
#include <iostream>
#include <sstream>

/* Resolve a `hol_object_store` node into a signing config.
 *
 * Shared by `push_to_store` and `check_store` so that the CHECK and the
 * OPERATION resolve their credential the same way. Two places that resolve one
 * secret differently is exactly how a "test this credential" button stops
 * predicting the thing it is testing — the field agent's finding about
 * Cloudflare's `/tokens/verify`, which answered "active" for a token that failed
 * every real call. */
static const maiz::SceneNode* find_store(const maiz::Scene& farm,
                                         std::string_view node, int& count) {
    const maiz::SceneNode* store = nullptr;
    count = 0;
    for (const auto& n : farm.nodes) {
        if (n.glyph != "hol_object_store") continue;
        ++count;
        if (node.empty() || n.name == node) store = &n;
    }
    return store;
}

/* Fill a signing config from a store node.
 *
 * Takes the four things it needs rather than the whole application, which is
 * what keeps the layering honest: `tools/check_layering.py` forbids `app` from
 * depending on `publish`, and a HormigaApp member whose parameter is an
 * `aws::Config` would have dragged the vendor surface into `app.hpp`. The
 * dependency runs the correct way now — publish knows about data, not the other
 * way round.
 *
 * Vault first, file second, and the error names both — the same resolution
 * order `deploy_site` uses. Two places that resolve one secret differently is
 * how a "test this credential" button stops predicting the operation it tests. */
static bool store_config(const maiz::SceneNode& store, const fs::path& base_dir,
                         hormiga::Vault& vault,
                         const std::function<std::string(const std::string&)>& shell,
                         std::vector<maiz::LogEntry>& log,
                         hormiga::aws::Config& cc) {
    cc.bucket = field_value(store, "bucket");
    cc.region = field_value(store, "region");
    if (cc.region.empty()) cc.region = "us-east-1";
    cc.endpoint = field_value(store, "endpoint");
    cc.creds.access_key_id = field_value(store, "access_key_id");
    cc.work_dir = base_dir.string();
    cc.shell = shell;
    cc.progress = [&log](const std::string& m) {
        log.push_back({"info", "push", m});
    };

    const std::string skey = field_value(store, "secret_key");
    const std::string sfile = field_value(store, "secret_file");
    if (!skey.empty() && vault.unlocked())
        cc.creds.secret_access_key =
            hormiga::cloudflare::trim_secret(vault.get(skey));
    if (cc.creds.secret_access_key.empty() && !sfile.empty()) {
        const fs::path p =
            fs::path(sfile).is_absolute() ? fs::path(sfile) : base_dir / sfile;
        std::ifstream in(p, std::ios::binary);
        if (in) {
            std::stringstream ss;
            ss << in.rdbuf();
            cc.creds.secret_access_key = hormiga::cloudflare::trim_secret(ss.str());
        }
    }
    if (cc.bucket.empty() || cc.creds.access_key_id.empty()) {
        log.push_back({"error", "push",
                       store.name + " needs a bucket and an access_key_id"});
        return false;
    }
    if (cc.creds.secret_access_key.empty()) {
        log.push_back({"error", "push",
                       skey.empty()
                           ? "no secret on " + store.name +
                                 " - set secret_key (kept in the vault, travels "
                                 "with the .miga) or secret_file"
                           : "secret_key '" + skey +
                                 "' is not in the vault - unlock it, or set "
                                 "secret_file instead"});
        return false;
    }
    if (!shell) {
        log.push_back({"error", "push", "no shell transport on this front-end"});
        return false;
    }
    return true;
}

int HormigaApp::push_to_store(const maiz::Scene& farm, std::string_view node,
                              std::string_view what) {
    /* Every refusal reaches the terminal, the same scope-guard pattern
     * `deploy_site` uses — a refusal the caller cannot see is how "it printed
     * done and did nothing" started, and this function has eight ways to
     * decline. */
    const size_t log_from = log.size();
    struct Reporter {
        const std::vector<maiz::LogEntry>& log;
        size_t from;
        ~Reporter() {
            for (size_t i = from; i < log.size(); ++i)
                if (log[i].op == "push")
                    std::cerr << "  [" << log[i].level << "] push: " << log[i].msg
                              << "\n";
        }
    } _report{log, log_from};

    int stores = 0;
    const maiz::SceneNode* store = find_store(farm, node, stores);
    if (!store) {
        log.push_back({"error", "push",
                       stores == 0
                           ? "no hol_object_store node in the antfarm mantle - "
                             "add one and set bucket/region/access_key_id"
                           : "no such store node: " + std::string(node)});
        return -1;
    }
    if (node.empty() && stores > 1) {
        log.push_back({"error", "push",
                       std::to_string(stores) +
                           " object stores are wired - name the one to push to"});
        return -1;
    }

    hormiga::aws::Config cc;
    if (!store_config(*store, base_dir, vault, on_shell_capture, log, cc))
        return -1;

    std::string prefix = field_value(*store, "prefix");
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';

    std::error_code ec;
    int sent = 0;

    if (what == "index") {
        const fs::path dir = data_dir("site") / "index";
        if (!fs::is_directory(dir, ec)) {
            log.push_back({"error", "push",
                           "nothing to push - run `effect publish-index` (or "
                           "`render-site`, which writes a live directory's "
                           "fragment) first"});
            return -1;
        }
        for (auto it = fs::directory_iterator(dir, ec);
             it != fs::directory_iterator(); it.increment(ec)) {
            if (ec || !it->is_regular_file(ec)) continue;
            const fs::path f = it->path();
            std::ifstream in(f, std::ios::binary);
            std::stringstream ss;
            ss << in.rdbuf();
            const std::string name = f.filename().string();
            const std::string ctype =
                f.extension() == ".json" ? "application/json" : "text/html";
            const auto r = hormiga::aws::put_object(cc, prefix + "index/" + name,
                                                    ss.str(), ctype);
            if (!r.ok) {
                /* THE VENDOR'S OWN WORDS, UNPARSED. AWS returns the canonical
                 * request IT computed on a signature mismatch, which is the
                 * entire diagnosis and is exactly what a summary would discard.
                 * This rule has paid for itself three times on Cloudflare. */
                log.push_back({"error", "push", name + ": " + r.error});
                return -1;
            }
            ++sent;
        }
        if (sent == 0) {
            log.push_back({"error", "push", "site/index/ is empty"});
            return -1;
        }
    } else if (what == "backup") {
        const fs::path bkp = base_dir / (state_name + ".bkp");
        if (!fs::exists(bkp, ec)) {
            log.push_back({"error", "push",
                           "no backup to push - run `effect backup-database` "
                           "first (its passphrase never leaves this machine)"});
            return -1;
        }
        const auto r = hormiga::aws::put_file(
            cc, prefix + "backup/" + bkp.filename().string(), bkp.string(),
            "application/octet-stream");
        if (!r.ok) {
            log.push_back({"error", "push", r.error});
            return -1;
        }
        ++sent;
    } else {
        log.push_back({"error", "push",
                       "nothing called '" + std::string(what) +
                           "' can be pushed. Two artifacts are publishable: "
                           "`index` (the cleared projection) and `backup` (the "
                           "encrypted blob). The database itself is not one of "
                           "them, deliberately."});
        return -1;
    }

    log.push_back({"info", "push",
                   "pushed " + std::to_string(sent) + " file(s) to " +
                       cc.bucket + " via " + store->name});
    return sent;
}


/* ── CAN THESE CREDENTIALS REACH THIS BUCKET? ─────────────────────────────────
 *
 * The first thing an operator with new credentials will run, and it exists
 * because `hormiga::aws::check_access` was implemented and unreachable — a
 * capability with no caller, which is the same shape as a producer with no
 * consumer and just as useless.
 *
 * It performs the smallest REAL operation against the actual bucket (list one
 * key) rather than validating a credential in the abstract. That rule came from
 * the field agent losing an afternoon to Cloudflare's `/tokens/verify`, which
 * cheerfully answered "active" for a token that returned 9109 on every useful
 * call: **a green light that does not predict the operation is worse than no
 * light.**
 */
int HormigaApp::check_store(const maiz::Scene& farm, std::string_view node) {
    const size_t log_from = log.size();
    struct Reporter {
        const std::vector<maiz::LogEntry>& log;
        size_t from;
        ~Reporter() {
            for (size_t i = from; i < log.size(); ++i)
                if (log[i].op == "store")
                    std::cerr << "  [" << log[i].level << "] store: "
                              << log[i].msg << "\n";
        }
    } _report{log, log_from};

    int stores = 0;
    const maiz::SceneNode* store = find_store(farm, node, stores);
    if (!store) {
        log.push_back({"error", "store",
                       stores == 0 ? "no hol_object_store node in the antfarm mantle"
                                   : "no such store node: " + std::string(node)});
        return -1;
    }
    hormiga::aws::Config cc;
    if (!store_config(*store, base_dir, vault, on_shell_capture, log, cc))
        return -1;

    const auto r = hormiga::aws::check_access(cc);
    if (!r.ok) {
        /* THE VENDOR'S OWN WORDS. AWS distinguishes InvalidAccessKeyId from
         * SignatureDoesNotMatch from AccessDenied from NoSuchBucket, and those
         * are four completely different problems with four different fixes.
         * Summarising them into "check failed" would throw away the diagnosis. */
        log.push_back({"error", "store", r.error});
        return -1;
    }
    log.push_back({"info", "store",
                   "these credentials can reach " + cc.bucket +
                       " - the same operation a push performs"});
    return 0;
}
