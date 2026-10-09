/* app/farm_effects.cpp — what Antfarm v2 nodes DO: runs, checks, previews, publishes.
 *
 * okf/concepts/platform/antfarm/v2/types.md §3: wires never write by themselves;
 * a write is a run, and every run that reaches outside the document is an
 * EFFECT, under the same gate as every other (`effect farm-run …`, refused in
 * the CLI until granted). Four ops:
 *
 *   farm-run <node> [apply]   a source (Import CSV), a store (files into a
 *                             river), the Data->Assets tunnel; rehearses first
 *   farm-check <node>         the smallest real test of a folder, a domain, a
 *                             bucket or a key
 *   farm-preview <document>   build it and serve it on its local domain
 *   farm-publish <document>   build everything mounted on its web domain and
 *                             send it
 *
 * NO SECOND DEPLOY PATH. Publishing and checking hand v1's proven code
 * (`deploy_site`, `check_host`, `check_store`, `host_online`) a v1-shaped host
 * built from the v2 domain or reservoir and its key. The field report's lessons
 * live in that code; a parallel implementation would have to relearn them. */
#include "app/app_internal.hpp"
#include "domain/import.hpp"
#include "domain/seed.hpp" // the glyph registrations a throwaway app replays with
#include "platform/profile.hpp"
#include "voidmaiz/gesture.hpp"

#include "json.hpp"
#include "imgui.h" // only to ask whether there is a window to draw a map picture with

namespace {

maiz::SceneField sfield(const std::string& key, const std::string& value) {
    maiz::SceneField f;
    f.key = key;
    f.value_json = farm::json_quote(value);
    return f;
}

/* A v1 host node, so the v1 publish code can do the work (see the header). */
maiz::SceneNode v1_node(const std::string& name, const std::string& glyph,
                        const std::vector<std::pair<std::string, std::string>>& fields) {
    maiz::SceneNode n;
    n.name = name;
    n.glyph = glyph;
    for (const auto& [k, v] : fields)
        if (!v.empty()) n.fields.push_back(sfield(k, v));
    return n;
}

std::string key_entry(const farm::Graph& g, const std::string& node) {
    const auto kw = g.into(node, "key");
    if (kw.empty()) return "";
    const farm::Node* k = g.find(kw[0]->from);
    return k ? k->field("vault_entry") : "";
}

/* The v1 scene a v2 web domain (or bucket) stands for. */
maiz::Scene v1_scene_for(const farm::Graph& g, const farm::Node& n) {
    maiz::Scene s;
    s.mantle = kAntfarmMantle;
    const std::string entry = key_entry(g, n.name);
    const std::string id = n.kind->id;
    if (id == "web-domain") {
        const std::string host = n.field("host");
        if (host == "github")
            s.nodes.push_back(v1_node(n.name, "hol_github",
                                      {{"repo", n.field("target")}, {"branch", n.field("branch")},
                                       {"token_key", entry}}));
        else
            s.nodes.push_back(v1_node(n.name, "hol_static_host",
                                      {{"provider", host.empty() ? "cloudflare" : host},
                                       {"project", n.field("target")},
                                       {"account_id", n.field("account")},
                                       {"token_key", entry}}));
        if (!n.field("name").empty()) s.nodes.push_back(v1_node(n.name + "-name", "hol_dns", {{"domain", n.field("name")}}));
    } else if (id == "bucket") {
        s.nodes.push_back(v1_node(n.name, "hol_object_store",
                                  {{"bucket", n.field("bucket")}, {"region", n.field("region")},
                                   {"endpoint", n.field("endpoint")},
                                   {"access_key_id", n.field("access_key_id")},
                                   {"secret_key", entry}, {"prefix", n.field("prefix")},
                                   {"public_url", n.field("public_url")}}));
    } else if (id == "image-host") {
        s.nodes.push_back(v1_node(n.name, "hol_imgbb", {}));
    }
    return s;
}

/* The chamber entry a document node names, and its content. */
struct DocTarget {
    std::string entry, of, kind, mount = "/";
};

DocTarget doc_target(maiz::Core& core, const farm::Node& n) {
    DocTarget t;
    t.kind = n.kind->id;
    const std::string want = n.field("document");
    maiz::ProjectOptions po;
    po.mantle = hormiga::chambers::kDocuments;
    for (const auto& d : maiz::project_scene(core, po).nodes)
        if (d.name == want || hormiga::field_value(d, "of") == want) {
            t.entry = d.name;
            t.of = hormiga::field_value(d, "of");
            if (!hormiga::field_value(d, "mount").empty()) t.mount = hormiga::field_value(d, "mount");
        }
    if (t.of.empty()) t.of = want;
    if (!n.field("mount").empty()) t.mount = n.field("mount");
    if (t.mount.empty() || t.mount.back() != '/') t.mount += "/";
    if (t.mount.front() != '/') t.mount = "/" + t.mount;
    return t;
}

bool copy_into(const fs::path& src, const fs::path& dst_dir, const std::string& name, std::string& err) {
    std::error_code ec;
    fs::create_directories(dst_dir, ec);
    fs::copy_file(src, dst_dir / name, fs::copy_options::overwrite_existing, ec);
    if (ec) err = ec.message();
    return !ec;
}

} // namespace

/* NO HOST SEAMS: the CLI's throwaway app, loaded from the session's state for
 * the length of one farm effect. Its core answers no effect of its own; the
 * commands an effect makes are dispatched into the SESSION's core by the
 * caller (main/farm_cli.cpp), which is where the journal and the gate are. */
void HormigaApp::load_throwaway(const std::string& state_json) {
    core = maiz::Core(state_json);
    hormiga::register_glyphs(core);
    hormiga::register_block_glyphs(core);
    hormiga::register_antfarm_glyphs(core);
    farm::register_glyphs(core);
    hormiga::chambers::register_glyphs(core);
    hormiga::kinds::apply(core, {}); // the database's own kinds, after every application glyph
    scene = maiz::project_scene(core);
    refresh_allo_rules();
    if (hormiga::Vault::exists(vault_path().string()))
        vault.unlock(vault_path().string(), hormiga::profile::credentials_key(hormiga::profile::load_or_create()));
    load_secrets();
}

HormigaApp::FarmFx HormigaApp::farm_effect(const std::string& op, const std::vector<std::string>& a) {
    FarmFx fx;
    auto fail = [&](std::string why) {
        fx.ok = false;
        fx.text = std::move(why);
        return fx;
    };
    if (a.empty()) return fail("which node? `farm " + op.substr(5) + " <node>`");
    const maiz::Scene fs_ = hormiga::farmhost::project_farm(core);
    const farm::Graph g = farm::read(fs_);
    const farm::Node* n = g.find(a[0]);
    if (!n || !n->kind) return fail("no v2 node called " + a[0]);
    if (n->kind->planned) return fail(n->name + " is " + n->kind->label + ": planned, and no code answers it yet");
    const bool apply = a.size() > 1 && a[1] == "apply";
    const farm::Context ctx = farm_context();
    farm::Evaluator ev(g, ctx);
    const std::string id = n->kind->id;
    const std::string me = hormiga::profile::load_or_create().username;
    const std::string place = n->placement();
    if (place != "any" && place != "each" && !me.empty() && place != me)
        return fail(n->name + " runs on " + place + ", not on this device (" + me + ")");

    // ── farm-run ─────────────────────────────────────────────────────────────
    if (op == "farm-run") {
        if (id == "import-csv") {
            const std::string file = n->field("file");
            std::string glyph = n->field("glyph");
            if (glyph.empty()) glyph = "contact";
            std::ifstream in(resolve_file(file), std::ios::binary);
            if (file.empty() || !in) return fail("cannot read " + (file.empty() ? std::string("(no file named)") : file));
            std::stringstream ss;
            ss << in.rdbuf();
            maiz::ProjectOptions po;
            po.mantle = kDataMantle;
            const maiz::Scene data = maiz::project_scene(core, po);
            const auto res = hormiga::compile_csv_import(ss.str(), glyph, hormiga::glyph_fields(core, glyph),
                                                         [&](const std::string& nm) { return data.find(nm) != nullptr; });
            if (!res.error.empty()) return fail("import failed: " + res.error);
            fx.text = (apply ? "imported " : "would import ") + std::to_string(res.rows) + " " + glyph + "(s) from " +
                      file + (res.notes.empty() ? "" : ", " + std::to_string(res.notes.size()) + " note(s)");
            for (const auto& note : res.notes) fx.text += "\n  " + note;
            if (!apply) fx.text += "\n(rehearsal - add `apply` to import, as one batch and one undo)";
            else {
                fx.commands.push_back(std::string("use ") + kDataMantle);
                for (const auto& c : res.commands) fx.commands.push_back(c);
            }
            return fx;
        }
        if (id == "tunnel-assets") {
            maiz::Core& c = core;
            auto cmds = hormiga::farmhost::chamber_commands(c, base_dir, {ship_dir},
                                                            hormiga::farmhost::this_device((bool)phone), "");
            fx.text = cmds.empty() ? std::string("the chambers are in step: nothing to register")
                                   : (apply ? "registered " : "would register ") + std::to_string(cmds.size()) +
                                         " command(s) of chamber changes";
            if (apply) fx.commands = cmds;
            else if (!cmds.empty()) fx.text += "\n(rehearsal - add `apply`)";
            return fx;
        }
        if (id == "store") {
            const auto mw = g.into(n->name, "mantle");
            const farm::Value m = mw.empty() ? farm::Value{} : ev.eval(mw[0]->from, mw[0]->out);
            const auto rw = g.into(n->name, "river");
            if (rw.empty()) return fail(n->name + " has no river wired");
            const farm::Value river = ev.eval(rw[0]->from, rw[0]->out);
            // the files this mantle names
            std::set<std::string> files;
            for (const auto& r : m.runes) {
                if (r.chamber == "assets") files.insert(farm::field_of(r.node, "file"));
                else
                    for (const auto& fl : r.node.fields)
                        if (fl.editor == "image" || (r.node.glyph == "image" && fl.key == "path")) {
                            const std::string v = farm::field_of(r.node, fl.key);
                            if (!v.empty() && v.rfind("http", 0) != 0) files.insert(v);
                        }
            }
            files.erase("");
            int copied = 0, already = 0, hosted = 0, missing = 0;
            std::vector<std::string> notes;
            for (const auto& ref : river.refs) {
                const farm::Node* res = g.find(ref);
                if (!res || !res->kind) continue;
                const std::string rk = res->kind->id;
                if (rk == "folder") {
                    const fs::path dir = resolve_file(res->field("path"));
                    std::error_code same_ec;
                    if (fs::equivalent(dir, assets_dir(), same_ec)) {
                        notes.push_back(ref + " is the home folder: the files are already there");
                        continue;
                    }
                    for (const auto& f : files) {
                        const fs::path src = resolve_file(f);
                        std::error_code ec;
                        if (!fs::exists(src, ec)) { ++missing; continue; }
                        const std::string nm = src.filename().string();
                        if (fs::exists(dir / nm, ec) && fs::file_size(dir / nm, ec) == fs::file_size(src, ec)) { ++already; continue; }
                        std::string err;
                        if (!apply) { ++copied; continue; }
                        if (copy_into(src, dir, nm, err)) ++copied;
                        else notes.push_back(nm + ": " + err);
                    }
                } else if (rk == "bucket" || rk == "image-host") {
                    const maiz::Scene v1 = v1_scene_for(g, *res);
                    if (rk == "image-host") imgbb_key = vault.unlocked() ? vault.get(key_entry(g, ref)) : std::string();
                    maiz::ProjectOptions ao;
                    ao.mantle = hormiga::chambers::kAssets;
                    const maiz::Scene assets = maiz::project_scene(core, ao);
                    for (const auto& f : files) {
                        const std::string aname = hormiga::chambers::asset_name(f);
                        const maiz::SceneNode* an = assets.find(aname);
                        if (an && !hormiga::field_value(*an, "url").empty()) { ++already; continue; }
                        if (!apply) { ++hosted; continue; }
                        const HostedLink link = host_online(v1, f, aname, "", ref);
                        if (!link.error.empty() || link.url.empty()) {
                            notes.push_back(f + ": " + (link.error.empty() ? "no link came back" : link.error));
                            continue;
                        }
                        ++hosted;
                        if (an) {
                            fx.commands.push_back(std::string("use ") + hormiga::chambers::kAssets);
                            fx.commands.push_back("set " + aname + " url " + farm::json_quote(link.url));
                            fx.commands.push_back("set " + aname + " hosted_by " + farm::json_quote(ref));
                        }
                    }
                } else {
                    notes.push_back(ref + " (" + res->kind->label + ") cannot hold files yet");
                }
            }
            fx.text = std::string(apply ? "" : "rehearsal: ") + std::to_string(files.size()) + " file(s) named; " +
                      std::to_string(copied) + (apply ? " copied" : " to copy") + ", " + std::to_string(hosted) +
                      (apply ? " put online" : " to put online") + ", " + std::to_string(already) +
                      " already there, " + std::to_string(missing) + " not on this device";
            for (const auto& s : notes) fx.text += "\n  " + s;
            if (!apply) fx.text += "\n(add `apply` to run it)";
            return fx;
        }
        return fail(n->name + " (" + n->kind->label + ") has nothing to run");
    }

    // ── farm-check ───────────────────────────────────────────────────────────
    if (op == "farm-check") {
        if (id == "folder") {
            const fs::path dir = resolve_file(n->field("path"));
            std::error_code ec;
            fs::create_directories(dir, ec);
            const fs::path probe = dir / ".hormiga-check";
            { std::ofstream o(probe); o << "ok"; }
            const bool ok = fs::exists(probe, ec);
            fs::remove(probe, ec);
            const auto sp = fs::space(dir, ec);
            fx.ok = ok;
            fx.text = std::string(ok ? "ok: " : "failing: ") + dir.string() + (ok ? " is writable" : " cannot be written") +
                      (ec ? "" : ", " + farm::human_bytes((long long)sp.available) + " free");
            return fx;
        }
        if (id == "local-domain") {
            fx.ok = ctx.serves_local;
            fx.text = ctx.serves_local ? "ok: this device can serve http://localhost:" +
                                             (n->field("port").empty() ? std::string("8780") : n->field("port"))
                                       : "needs: this device cannot serve a local address";
            return fx;
        }
        auto check_v1 = [&](const farm::Node& node) {
            const maiz::Scene v1 = v1_scene_for(g, node);
            const size_t from = log.size();
            const int rc = node.kind->id == "bucket" ? check_store(v1, node.name) : check_host(v1, node.name);
            std::string out = node.name + ": " + (rc == 0 ? "ok" : rc < 0 ? "could not run" : "failing");
            for (size_t i = from; i < log.size(); ++i) out += "\n  " + log[i].msg;
            return std::make_pair(rc == 0, out);
        };
        if (id == "web-domain" || id == "bucket") {
            const auto [ok, text] = check_v1(*n);
            fx.ok = ok;
            fx.text = text;
            return fx;
        }
        if (id == "key") {
            bool any = false;
            fx.ok = true;
            for (const farm::Wire* w : g.out_of(n->name, "key")) {
                const farm::Node* user = g.find(w->to);
                if (!user || !user->kind || (user->kind->id != "web-domain" && user->kind->id != "bucket")) continue;
                any = true;
                const auto [ok, text] = check_v1(*user);
                fx.ok = fx.ok && ok;
                fx.text += (fx.text.empty() ? "" : "\n") + text;
            }
            if (!any) return fail("a key is checked through what uses it, and nothing checkable uses " + n->name);
            return fx;
        }
        return fail(n->name + " (" + n->kind->label + ") has no check yet");
    }

    // ── farm-preview / farm-publish: build what is mounted, then send it ────
    const bool publishing = op == "farm-publish";
    if (op != "farm-preview" && !publishing) return fail("unknown farm effect " + op);
    if (id != "website" && id != "newsletter" && id != "calendar" && id != "map")
        return fail(n->name + " is not a document");
    const std::string port = publishing ? "publish" : "preview";
    const farm::Node* domain = nullptr;
    for (const farm::Wire* w : g.out_of(n->name, port)) {
        const farm::Node* d = g.find(w->to);
        if (d && d->kind && (d->kind->id == (publishing ? "web-domain" : "local-domain"))) domain = d;
    }
    if (!domain) return fail(n->name + "." + port + " goes nowhere " + (publishing ? "public" : "local") +
                             " - wire it to a " + (publishing ? "web domain" : "local domain"));
    const farm::Face df = ev.face(domain->name);
    if (df.ready.state != "ready" && df.ready.state != "idle")
        return fail(domain->name + " is not ready: " + df.ready.why);

    // every document mounted on that domain, built into one site/ folder
    const fs::path site = data_dir("site");
    std::vector<const farm::Node*> docs;
    for (const farm::Wire* w : g.into(domain->name, "renditions"))
        if (w->out == port)
            if (const farm::Node* d = g.find(w->from); d && d->kind) docs.push_back(d);
    std::stable_sort(docs.begin(), docs.end(), [](const farm::Node* x, const farm::Node* y) {
        return (x->kind->id == "website") > (y->kind->id == "website"); // the website first: it rewrites site/
    });
    std::vector<std::string> built;
    const std::string keep_doc = cur_doc;
    const maiz::Scene keep_scene = scene;
    /* THE GRANT, ENFORCED (documents.md §5). Each document renders against only
     * the data runes its `data` input carries, so a Builder block that names a
     * rune the Antfarm filtered out finds nothing. An unwired input grants
     * nothing, and that document is refused rather than published empty. */
    for (const farm::Node* d : docs)
        if (g.into(d->name, "data").empty())
            return fail(d->name + " is not connected: an unwired document can see nothing, so it was not built");
    for (const farm::Node* d : docs) {
        const DocTarget t = doc_target(core, *d);
        const std::string dk = d->kind->id;
        {
            auto granted = std::make_shared<std::set<std::string>>();
            const auto dw = g.into(d->name, "data");
            for (const auto& r : ev.eval(dw[0]->from, dw[0]->out).runes)
                if (r.chamber == "data") granted->insert(r.node.name);
            fv2.grant = granted;
        }
        if (dk == "website") {
            cur_doc = t.of;
            render_site("en");
            render_site("es");
            built.push_back(d->name + " -> / (" + t.of + ")");
        } else if (dk == "calendar") {
            maiz::ProjectOptions dio;
            dio.mantle = kDataMantle;
            scene = maiz::project_scene(core, dio);
            const maiz::Scene views = scene; // the calendar view itself is not a granted rune
            hormiga::farmhost::apply_grant(scene, fv2.grant.get());
            if (const maiz::SceneNode* v = views.find(t.of)) {
                if (!scene.find(t.of)) scene.nodes.push_back(*v); // the view, not granted data
                cal_apply_view(*v);
            }
            const std::string ics = export_calendar_ics();
            std::string err;
            const fs::path dir = site / t.mount.substr(1);
            if (!ics.empty() && copy_into(ics, dir, "calendar.ics", err)) {
                std::ofstream html(dir / "index.html", std::ios::trunc);
                html << "<!doctype html><meta charset=utf-8><title>" << t.of << "</title>"
                     << "<h1>" << t.of << "</h1><p><a href=\"calendar.ics\">Subscribe (calendar.ics)</a></p>";
                built.push_back(d->name + " -> " + t.mount + "calendar.ics");
            }
        } else if (dk == "map") {
            /* A MAP RENDITION IS A GEOJSON LAYER (documents.md §1): every rune the
             * document can see that has a place, as a Feature. Pure, so it works
             * without a window; the picture of it (export_map_png draws with
             * ImGui's font atlas) is added only when there is a window to draw with. */
            const fs::path dir = site / t.mount.substr(1);
            std::error_code ec;
            fs::create_directories(dir, ec);
            nlohmann::json features = nlohmann::json::array();
            const auto dw = g.into(d->name, "data");
            const farm::Value seen = dw.empty() ? farm::Value{} : ev.eval(dw[0]->from, dw[0]->out);
            std::string items;
            for (const auto& r : seen.runes) {
                const std::string geo = farm::field_of(r.node, "geo");
                double lat = 0, lon = 0;
                if (geo.empty() || std::sscanf(geo.c_str(), "%lf,%lf", &lat, &lon) != 2) continue;
                std::string title = farm::field_of(r.node, "display_name");
                if (title.empty()) title = farm::field_of(r.node, "title_en");
                if (title.empty()) title = r.node.name;
                features.push_back({{"type", "Feature"},
                                    {"geometry", {{"type", "Point"}, {"coordinates", {lon, lat}}}},
                                    {"properties", {{"name", r.node.name}, {"title", title}, {"tags", r.node.tags}}}});
                items += "<li>" + title + " <small>(" + geo + ")</small></li>";
            }
            std::ofstream(dir / "places.geojson", std::ios::trunc)
                << nlohmann::json{{"type", "FeatureCollection"}, {"features", features}}.dump(1);
            bool picture = false;
            if (ImGui::GetCurrentContext()) {
                maiz::ProjectOptions dio;
                dio.mantle = kDataMantle;
                scene = maiz::project_scene(core, dio);
                const maiz::Scene views = scene;
                hormiga::farmhost::apply_grant(scene, fv2.grant.get());
                if (const maiz::SceneNode* v = views.find(t.of)) scene.nodes.push_back(*v); // the view, not granted data
                const std::string png = export_map_png(t.of, false);
                std::string err;
                picture = !png.empty() && copy_into(png, dir, "map.png", err);
            }
            std::ofstream html(dir / "index.html", std::ios::trunc);
            html << "<!doctype html><meta charset=utf-8><title>" << t.of << "</title><h1>" << t.of << "</h1>"
                 << (picture ? "<img src=\"map.png\" style=\"max-width:100%\" alt=\"map\">" : "")
                 << "<p><a href=\"places.geojson\">places.geojson</a> (" << features.size()
                 << " places)</p><ul>" << items << "</ul>";
            built.push_back(d->name + " -> " + t.mount + "places.geojson (" + std::to_string(features.size()) +
                            " places" + (picture ? ", and map.png" : "") + ")");
        } else if (dk == "newsletter") {
            cur_doc = t.of;
            const std::string html = render_preview("en");
            std::string err;
            if (!html.empty() && copy_into(html, site / t.mount.substr(1), "index.html", err))
                built.push_back(d->name + " -> " + t.mount);
        }
    }
    fv2.grant.reset(); // the tabs' own previews see everything again
    cur_doc = keep_doc;
    scene = keep_scene;
    fx.text = "built into " + site.string() + ":";
    for (const auto& b : built) fx.text += "\n  " + b;

    if (!publishing) {
        const int p = std::atoi(domain->field("port").c_str());
        if (on_open) { // a window: serve it (the CLI builds and says where it is)
            host_srv.stop();
            if (host_srv.start(site, /*host_mode=*/true, p > 0 ? p : 8780)) {
                const std::string url = "http://127.0.0.1:" + std::to_string(host_srv.port()) + "/";
                fx.text += "\nserving at " + url;
                on_open(url);
            } else fx.text += "\ncould not bind a localhost port";
        }
        return fx;
    }
    const maiz::Scene v1 = v1_scene_for(g, *domain);
    const std::string url = deploy_site(v1, domain->name);
    if (url.empty()) {
        fx.ok = false;
        for (const auto& e : log)
            if (e.op == "deploy" && e.level == "error") fx.text += "\n  " + e.msg;
        fx.text += "\npublish failed";
        return fx;
    }
    fx.text += "\npublished: " + url;
    for (const auto& c : deployment_record(v1, domain->name, url, doc_target(core, *n).of, hormiga::site_langs_str()))
        fx.commands.push_back(c);
    fx.commands.push_back(std::string("use ") + hormiga::chambers::kDocuments);
    for (const farm::Node* d : docs) {
        const DocTarget t = doc_target(core, *d);
        if (t.entry.empty()) continue;
        fx.commands.push_back("set " + t.entry + " last_published " + farm::json_quote(url));
        fx.commands.push_back("set " + t.entry + " published_to " + farm::json_quote(domain->name));
    }
    return fx;
}

/* The GUI's side of the gate: `effect farm-…` from a face button, the
 * Inspector or the command bar. The commands land in THIS core, as one batch. */
std::string HormigaApp::farm_effect_gui(std::string_view op, std::string_view args) {
    std::vector<std::string> a;
    try {
        auto j = nlohmann::json::parse(args);
        if (j.contains("args"))
            for (const auto& x : j["args"]) a.push_back(x.get<std::string>());
    } catch (...) {}
    const std::string back = scene.mantle;
    const FarmFx fx = farm_effect(std::string(op), a);
    log.push_back({fx.ok ? "info" : "error", std::string(op), fx.text});
    toast(fx.text.substr(0, fx.text.find('\n')), !fx.ok);
    if (!fx.commands.empty()) {
        std::vector<std::string> c = fx.commands;
        c.push_back("use " + back);
        core.dispatch(maiz::compile_commit(c));
    }
    return fx.ok ? farm::json_quote(fx.text) : std::string();
}
