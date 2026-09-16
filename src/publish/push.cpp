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
 *   index   -> site/index/<name>   the published projection. Cleared the
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

/* ── HOST IT ONLINE: A HOLIDAY AS A FUNCTION CALL (2026-09-15) ───────────────
 *
 * domain/hosting.hpp has the author's words and the shape: a local file in, a
 * public link out, answered by whichever Antfarm node implements the protocol.
 * These are the implementations, one branch per row of that table.
 *
 * Here beside the object store because one of the three answers IS the object
 * store, and `store_config` above is how its credentials resolve: vault first,
 * then file, with the error naming both. A second resolver for the same secret
 * is how a green light stops predicting the upload it lights for.
 *
 * `host_online` takes the Antfarm scene and the site's address as arguments, and
 * does no dispatching, so the CLI (whose HormigaApp owns no database) and the GUI
 * call the same function. Writing the link into the image rune is the caller's:
 * `host_image` below for the GUI, the `host-online` effect in main/headless.cpp.
 */
#include "domain/hosting.hpp"

namespace {
std::string image_content_type(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = (char)std::tolower((unsigned char)c);
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".svg") return "image/svg+xml";
    return "application/octet-stream";
}

std::string read_trimmed(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    return hormiga::cloudflare::trim_secret(ss.str());
}

std::string config_url(maiz::Core& core) {
    std::string v = core.dispatch("config get site.base_url").data;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
    if (v == "null") v.clear();
    while (!v.empty() && v.back() == '/') v.pop_back();
    return v;
}
} // namespace

std::string HormigaApp::host_problem(const maiz::SceneNode& node, const std::string& base_url) {
    if (!hormiga::hosting::asset_host(node.glyph)) return "this node cannot host images";
    if (!on_shell_capture) return "no network transport on this front-end";
    if (node.glyph == "hol_imgbb") {
        if (!imgbb_key.empty()) return {};
        const std::string kf = field_value(node, "key_file");
        if (!kf.empty() &&
            !read_trimmed(fs::path(kf).is_absolute() ? fs::path(kf) : base_dir / kf).empty())
            return {};
        return "no ImgBB key - unlock the vault, add imgbb.key, or set key_file";
    }
    if (node.glyph == "hol_object_store") {
        if (field_value(node, "bucket").empty()) return "set its bucket";
        if (field_value(node, "access_key_id").empty()) return "set its access_key_id";
        if (field_value(node, "public_url").empty())
            return "set its public_url - the address the bucket is served at (an "
                   "r2.dev address, or your own domain)";
        return {};
    }
    if (base_url.empty())
        return "set the website's address (site.base_url, in Style > Site) - the link "
               "is built from it";
    return {};
}

const maiz::SceneNode* HormigaApp::pick_image_host(const maiz::Scene& farm,
                                                   const std::string& prefer,
                                                   const std::string& base_url,
                                                   std::string* why) {
    auto say = [&](const std::string& s) {
        if (why) *why = s;
    };
    if (!prefer.empty()) {
        const maiz::SceneNode* n = nullptr;
        for (const auto& f : farm.nodes)
            if (f.name == prefer) n = &f;
        if (!n || !hormiga::hosting::asset_host(n->glyph)) {
            say("no image host called '" + prefer + "' in the Antfarm");
            return nullptr;
        }
        say(host_problem(*n, base_url));
        return n;
    }
    const maiz::SceneNode* first = nullptr;
    for (const auto& h : hormiga::hosting::kAssetHosts)
        for (const auto& f : farm.nodes) {
            if (f.glyph != h.glyph) continue;
            if (!first) first = &f;
            if (host_problem(f, base_url).empty()) {
                say("");
                return &f;
            }
        }
    if (first) {
        say(first->name + ": " + host_problem(*first, base_url));
        return first;
    }
    say("no image host in the Antfarm - add ImgBB, an object store with a public "
        "address, or your website's host");
    return nullptr;
}

HormigaApp::HostedLink HormigaApp::host_online(const maiz::Scene& farm, const std::string& path,
                                              const std::string& name,
                                              const std::string& base_url,
                                              const std::string& prefer) {
    HostedLink out;
    std::string why;
    const maiz::SceneNode* host = pick_image_host(farm, prefer, base_url, &why);
    if (!host || !why.empty()) {
        out.error = why.empty() ? "no image host can answer" : why;
        return out;
    }
    out.node = host->name;
    const fs::path abs = resolve_file(path);
    std::error_code ec;
    if (path.empty() || !fs::exists(abs, ec)) {
        out.error = "no file on this computer at " + abs.string();
        return out;
    }
    if (host->glyph == "hol_imgbb") {
        if (imgbb_key.empty()) {
            const std::string kf = field_value(*host, "key_file");
            if (!kf.empty())
                imgbb_key =
                    read_trimmed(fs::path(kf).is_absolute() ? fs::path(kf) : base_dir / kf);
        }
        out.url = upload_to_imgbb(path, name);
        if (out.url.empty())
            out.error = log.empty() ? std::string("ImgBB did not answer with a link")
                                    : "ImgBB did not answer with a link: " + log.back().msg;
    } else if (host->glyph == "hol_object_store") {
        hormiga::aws::Config cc;
        if (!store_config(*host, base_dir, vault, on_shell_capture, log, cc)) {
            // the reason, not a pointer to it: the CLI has no log strip to point at
            out.error = log.empty() ? std::string("the object store is not ready")
                                    : log.back().msg;
            return out;
        }
        std::string prefix = field_value(*host, "prefix");
        if (!prefix.empty() && prefix.back() != '/') prefix += '/';
        const std::string key = prefix + "images/" + abs.filename().string();
        const hormiga::aws::Result r =
            hormiga::aws::put_file(cc, key, abs.string(), image_content_type(path));
        if (!r.ok) {
            out.error = "the upload failed: " + r.error;
            return out;
        }
        std::string pub = field_value(*host, "public_url");
        while (!pub.empty() && pub.back() == '/') pub.pop_back();
        out.url = pub + "/" + hormiga::aws::uri_encode(key, false);
    } else { // a website host: the file rides the next publish
        const std::string rel = stage_site_asset(abs.string());
        if (rel.empty()) {
            out.error = "could not copy the file into the website's folder";
            return out;
        }
        out.url = base_url + "/" + rel;
        out.after_publish = true;
    }
    out.ok = !out.url.empty();
    if (out.ok)
        log.push_back({"info", "host",
                       name + " -> " + out.url + " (through " + out.node +
                           (out.after_publish ? "; works after the next publish)" : ")")});
    return out;
}

bool HormigaApp::image_host_ready() {
    maiz::ProjectOptions ao;
    ao.mantle = kAntfarmMantle;
    std::string why;
    return pick_image_host(maiz::project_scene(core, ao), "", config_url(core), &why) &&
           why.empty();
}

/* The GUI's call: find the image rune, ask the Antfarm, write the link and the node
 * that made it into the rune. Dispatches, so it runs between frames (`run_busy`)
 * or from a control that returns straight afterwards. */
bool HormigaApp::host_image(const std::string& rune, const std::string& prefer) {
    maiz::ProjectOptions ao, dpo;
    ao.mantle = kAntfarmMantle;
    dpo.mantle = kDataMantle;
    const maiz::Scene farm = maiz::project_scene(core, ao);
    const maiz::Scene data = maiz::project_scene(core, dpo);
    std::string path;
    bool found = false;
    for (const auto& n : data.nodes)
        if (n.name == rune && n.glyph == "image") {
            path = field_value(n, "path");
            found = true;
        }
    if (!found) {
        toast("no image called " + rune, true);
        return false;
    }
    const HostedLink link = host_online(farm, path, rune, config_url(core), prefer);
    if (!link.ok) {
        toast("could not put " + rune + " online: " + link.error, true);
        return false;
    }
    const std::string was = scene.mantle;
    const bool away = !was.empty() && was != kDataMantle;
    if (away) dispatch_and_reproject(std::string("use ") + kDataMantle);
    /* `set` TAKES A PLAIN VALUE (2026-09-16). `json_arg(json_str(x))` is the pairing
     * for `setjson`, whose argument has to survive as JSON; on `set` it stored the
     * quotes as part of the value, so a path came back as `"assets/x.webp"` and
     * matched nothing -- which is how one picture became three image runes. */
    dispatch_and_reproject("set " + rune + " url " + json_str(link.url));
    dispatch_and_reproject("set " + rune + " hosted_by " + json_str(link.node));
    if (away) dispatch_and_reproject("use " + was);
    toast(link.after_publish ? rune + " has its link - it works after the next publish"
                             : rune + " is online");
    return true;
}

int HormigaApp::host_missing_images(const std::string& prefer) {
    maiz::ProjectOptions dpo;
    dpo.mantle = kDataMantle;
    const maiz::Scene data = maiz::project_scene(core, dpo);
    std::vector<std::string> todo;
    for (const auto& n : data.nodes) {
        if (n.glyph != "image" || !field_value(n, "url").empty()) continue;
        const std::string p = field_value(n, "path");
        std::error_code ec;
        if (!p.empty() &&
            fs::exists(fs::path(p).is_absolute() ? fs::path(p) : base_dir / p, ec))
            todo.push_back(n.name);
    }
    int done = 0;
    for (const auto& r : todo)
        if (host_image(r, prefer)) ++done;
    return done;
}
