/* app/farm_host.cpp — see farm_host.hpp. */
#include "app/farm_host.hpp"

#include "app/app_internal.hpp" // the mantle names
#include "domain/chambers.hpp"   // the Assets, Network and Documents mantles
#include "domain/date_query.hpp" // query_matches: the ONE tag+date matcher every renderer uses
#include "platform/profile.hpp"
#include "platform/vault.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/wires.hpp"

#include <ctime>
#include <fstream>
#include <memory>

namespace hormiga::farmhost {

namespace {

long long rune_bytes(const maiz::SceneNode& n) {
    long long b = (long long)n.name.size() + (long long)n.glyph.size();
    for (const auto& t : n.tags) b += (long long)t.size() + 3;
    for (const auto& f : n.fields) b += (long long)(f.key.size() + f.value_json.size()) + 4;
    return b + 16;
}

std::vector<std::string> mantle_names(maiz::Core& core) {
    std::vector<std::string> out;
    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' ')) line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (!line.empty() && line != "(no mantles)") out.push_back(line);
    }
    return out;
}

bool is_document_mantle(const std::string& m) {
    return m != kDataMantle && m != kAntfarmMantle && m != farm::kMantle && m != kAlloMantle && m != kCivicMantle &&
           !hormiga::chambers::is_chamber(m);
}

maiz::Scene project(maiz::Core& core, const std::string& mantle) {
    maiz::ProjectOptions po;
    po.mantle = mantle;
    return maiz::project_scene(core, po);
}

std::string today_iso() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char b[16];
    std::strftime(b, sizeof b, "%Y-%m-%d", &tm);
    return b;
}

} // namespace

/* This device, as the Network chamber records it. From the profile on disk,
 * not the LAN runtime, so the first boot already knows its key fingerprint and
 * never mints a second profile rune for the same device. */
SelfFacts this_device(bool phone) {
    SelfFacts s;
    const hormiga::profile::Profile p = hormiga::profile::load_or_create();
    s.username = p.username;
    s.color = p.color;
    for (std::size_t i = 0; i < p.public_key.size() && i < 6; ++i) {
        char b[3];
        std::snprintf(b, sizeof b, "%02x", (unsigned char)p.public_key[i]);
        s.fingerprint += b;
    }
    s.device = phone ? "phone" : "desktop";
    s.serves_local = !phone;
#ifdef HORMIGA_PLATFORM
    s.platform = HORMIGA_PLATFORM;
#endif
    return s;
}

std::vector<std::string> chamber_commands(maiz::Core& core, const std::filesystem::path& base,
                                          const std::vector<std::filesystem::path>& also, const SelfFacts& self,
                                          const std::string& back) {
    namespace fs = std::filesystem;
    namespace ch = hormiga::chambers;
    const std::vector<std::string> names = mantle_names(core);
    auto has = [&](const char* m) { return std::find(names.begin(), names.end(), m) != names.end(); };
    if (!has(kDataMantle)) return {}; // an empty document: nothing to keep in step with yet
    std::vector<std::string> out;
    auto section = [&](const char* mantle, std::vector<std::string> cmds) {
        if (cmds.empty() && has(mantle)) return;
        out.push_back(has(mantle) ? std::string("use ") + mantle : std::string("mantle new ") + mantle);
        for (auto& c : cmds) out.push_back(std::move(c));
    };
    const maiz::Scene data = project(core, kDataMantle);

    // Documents: every Builder document, calendar view and map view
    std::vector<ch::DocFact> docs;
    for (const auto& m : names) {
        if (!is_document_mantle(m)) continue;
        ch::DocFact d{m, "newsletter", ""};
        for (const auto& n : project(core, m).nodes)
            if (n.glyph == "document") {
                if (!field_value(n, "kind").empty()) d.kind = field_value(n, "kind");
                d.title = field_value(n, "title");
            }
        docs.push_back(d);
    }
    for (const auto& n : data.nodes) {
        if (n.glyph == "calview") docs.push_back({n.name, "calendar", field_value(n, "title")});
        if (n.glyph == "map") docs.push_back({n.name, "map", ""});
    }
    section(ch::kDocuments, ch::reconcile_documents(project(core, ch::kDocuments), docs));

    // Network: this device, about itself
    ch::Self me{self.fingerprint, self.username, self.color, self.device, self.platform, self.serves_local, today_iso()};
    section(ch::kNetwork, ch::upsert_self(project(core, ch::kNetwork), me));

    // Assets: every file beside the database, and every file a rune names
    auto size_of = [&](const std::string& rel) -> long long {
        std::error_code ec;
        for (const fs::path& root : std::vector<fs::path>{base}) {
            const auto sz = fs::file_size(root / rel, ec);
            if (!ec) return (long long)sz;
        }
        for (const auto& root : also) {
            if (root.empty()) continue;
            const auto sz = fs::file_size(root / rel, ec);
            if (!ec) return (long long)sz;
        }
        return -1;
    };
    std::vector<ch::FileFact> files;
    std::error_code ec;
    for (fs::directory_iterator it(base / "assets", ec), end; !ec && it != end; it.increment(ec)) {
        const std::string fn = it->path().filename().string();
        if (!it->is_regular_file(ec) || fn.empty() || fn[0] == '.' || fn == "mirror.json") continue;
        files.push_back({"assets/" + fn, (long long)it->file_size(ec), self.username});
    }
    for (const auto& [rune, file] : ch::referenced_files(data)) files.push_back({file, size_of(file), ""});
    section(ch::kAssets, ch::register_assets(project(core, ch::kAssets), files));

    if (!out.empty() && !back.empty()) out.push_back("use " + back);
    return out;
}

namespace {
std::string L(const std::string& a, const std::string& out, const std::string& b, const std::string& in) {
    return "link " + a + " " + b + " --relation " + out + ":" + in;
}
std::string S(const std::string& node, const std::string& field, const std::string& value) {
    return "set " + node + " " + field + " " + farm::json_quote(value);
}
std::string in_days(int days) {
    std::time_t t = std::time(nullptr) + (std::time_t)days * 86400;
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char b[16];
    std::strftime(b, sizeof b, "%Y-%m-%d", &tm);
    return b;
}
} // namespace

std::vector<std::string> showcase_commands(maiz::Core& core, const std::filesystem::path& base,
                                           const std::string& back, std::string& note) {
    namespace fs = std::filesystem;
    std::vector<std::string> c;
    const std::vector<std::string> names = mantle_names(core);
    auto has = [&](const std::string& m) { return std::find(names.begin(), names.end(), m) != names.end(); };
    if (!has(kDataMantle)) {
        note = "this database has no data mantle to showcase";
        return {};
    }
    const maiz::Scene data = project(core, kDataMantle);

    // ── content to present: a calendar view and a small website ─────────
    c.push_back(std::string("use ") + kDataMantle);
    if (!data.find("cat-birthdays")) {
        c.push_back("rune new calview cat-birthdays");
        c.push_back(S("cat-birthdays", "title", "Cat birthdays"));
        c.push_back(S("cat-birthdays", "filter", "birthday"));
        c.push_back(S("cat-birthdays", "kind", "events"));
        c.push_back(S("cat-birthdays", "mode", "month"));
    }
    if (!has("cat-colony-site")) {
        c.push_back("mantle new cat-colony-site");
        c.push_back("rune new document site-doc");
        c.push_back(S("site-doc", "kind", "website"));
        c.push_back(S("site-doc", "title", "The Cat Colony"));
        c.push_back("rune new hero site-hero");
        c.push_back(S("site-hero", "title_en", "The Cat Colony"));
        c.push_back(S("site-hero", "subtitle_en", "Fifty cats, their birthdays, and where they nap."));
        c.push_back(S("site-hero", "row", "0"));
        c.push_back("rune new directory site-cats");
        c.push_back(S("site-cats", "query", "cat AND NOT private"));
        c.push_back(S("site-cats", "kind", "contact"));
        c.push_back(S("site-cats", "caption_en", "Meet the cats"));
        c.push_back(S("site-cats", "row", "1"));
        c.push_back("rune new event_grid site-birthdays");
        c.push_back(S("site-birthdays", "query", "birthday AND date:future"));
        c.push_back(S("site-birthdays", "limit", "6"));
        c.push_back(S("site-birthdays", "caption_en", "Upcoming birthdays"));
        c.push_back(S("site-birthdays", "row", "2"));
        c.push_back("rune new map_embed site-map");
        c.push_back(S("site-map", "view", "usa"));
        c.push_back(S("site-map", "caption_en", "Where they live"));
        c.push_back(S("site-map", "row", "3"));
    }
    // a spreadsheet of newly arrived cats, to import (fictional, like the colony)
    std::error_code ec;
    fs::create_directories(base / "imports", ec);
    const fs::path csv = base / "imports" / "incoming-cats.csv";
    if (!fs::exists(csv, ec)) {
        std::ofstream o(csv, std::ios::binary);
        o << "name,display_name,tags,bio_en\n"
             "noodle,Noodle,cat orange playful new-arrival,Arrived in a shoebox and refuses to leave it.\n"
             "pepper,Pepper,cat black shy new-arrival,Watches the door. Will allow one chin scratch.\n"
             "biscuit,Biscuit,cat calico cuddly new-arrival,Kneads every blanket in the building.\n"
             "sprocket,Sprocket,cat tabby curious new-arrival,Has opened three cupboards so far.\n"
             "juniper,Juniper,cat white sunbeam new-arrival,Follows the sunbeam across the floor all day.\n"
             "mochi,Mochi,cat gray naps new-arrival,Mostly asleep. Occasionally, briefly, awake.\n";
    }

    // ── the colony ──────────────────────────────────────────────────────
    c.push_back(has(farm::kMantle) ? std::string("use ") + farm::kMantle : std::string("mantle new ") + farm::kMantle);
    const maiz::Scene farm_scene = has(farm::kMantle) ? project(core, farm::kMantle) : maiz::Scene{};
    auto node = [&](const char* glyph, const std::string& name) {
        if (!farm_scene.find(name)) c.push_back(std::string("rune new ") + glyph + " " + name);
    };
    // chambers: the database, its mantles, the tunnel
    node("farm_miga", "this-db");
    node("farm_separate", "chambers");
    node("farm_tunnel_assets", "pictures");
    // the grant, and the selections documents are built from
    node("farm_q_tag", "not-private");
    c.push_back(S("not-private", "expr", "NOT private"));
    node("farm_filter", "public");
    node("farm_q_glyph", "only-cats");
    c.push_back(S("only-cats", "glyphs", "contact"));
    node("farm_filter", "cats");
    node("farm_q_tag", "gentle-q");
    c.push_back(S("gentle-q", "expr", "friendly OR cuddly OR shy"));
    node("farm_filter", "gentle-cats");
    node("farm_q_tag", "orange-q");
    c.push_back(S("orange-q", "expr", "orange"));
    node("farm_filter", "orange-cats");
    node("farm_q_tag", "calico-q");
    c.push_back(S("calico-q", "expr", "calico OR tortoiseshell"));
    node("farm_filter", "calico-cats");
    node("farm_join", "warm-coats");
    node("farm_count", "how-many-warm");
    node("farm_q_tag", "birthday-q");
    c.push_back(S("birthday-q", "expr", "birthday"));
    node("farm_q_date", "future-q");
    c.push_back(S("future-q", "when", "future"));
    node("farm_q_and", "upcoming-q");
    node("farm_filter", "upcoming");
    node("farm_q_field", "has-email");
    c.push_back(S("has-email", "key", "email"));
    c.push_back(S("has-email", "op", "is set"));
    node("farm_filter", "reachable");
    node("farm_count", "cat-count");
    node("farm_measure", "photo-weight");
    // documents
    node("farm_website", "colony-site");
    c.push_back(S("colony-site", "document", "cat-colony-site"));
    node("farm_newsletter", "cat-news");
    c.push_back(S("cat-news", "document", "issue-demo"));
    c.push_back(S("cat-news", "mount", "/news/"));
    node("farm_calendar", "birthdays");
    c.push_back(S("birthdays", "document", "cat-birthdays"));
    c.push_back(S("birthdays", "mount", "/birthdays/"));
    node("farm_map", "where-they-nap");
    c.push_back(S("where-they-nap", "document", "usa"));
    c.push_back(S("where-they-nap", "mount", "/map/"));
    // rivers: home, and a photo river over three reservoirs
    node("farm_folder", "here");
    c.push_back(S("here", "path", "assets"));
    c.push_back(S("here", "placement", "each"));
    node("farm_river", "home");
    node("farm_folder", "usb-stick");
    c.push_back(S("usb-stick", "path", "mirror/usb-stick"));
    node("farm_bucket", "cat-cdn");
    c.push_back(S("cat-cdn", "bucket", "cat-colony-photos"));
    c.push_back(S("cat-cdn", "endpoint", "https://ACCOUNT.r2.cloudflarestorage.com"));
    c.push_back(S("cat-cdn", "public_url", "https://photos.cats.example.org"));
    node("farm_river", "photo-river");
    node("farm_store", "mirror-photos");
    node("farm_gauge", "photo-gauge");
    node("farm_dam", "hold-uploads");
    c.push_back(S("hold-uploads", "reason", "no uploads until the CDN key arrives"));
    // keys, domains, sources
    node("farm_key", "cloudflare-demo");
    c.push_back(S("cloudflare-demo", "provider", "cloudflare"));
    c.push_back(S("cloudflare-demo", "vault_entry", "farm-key:cloudflare-demo"));
    c.push_back(S("cloudflare-demo", "expires", in_days(12)));
    node("farm_key", "github-demo");
    c.push_back(S("github-demo", "provider", "github"));
    c.push_back(S("github-demo", "vault_entry", "farm-key:github-demo"));
    node("farm_expiry", "cdn-key-expiry");
    node("farm_local_domain", "preview-here");
    c.push_back(S("preview-here", "port", "8780"));
    c.push_back(S("preview-here", "placement", "each"));
    node("farm_web_domain", "cats-on-pages");
    c.push_back(S("cats-on-pages", "host", "github"));
    c.push_back(S("cats-on-pages", "target", "example-org/cat-colony"));
    c.push_back(S("cats-on-pages", "branch", "gh-pages"));
    c.push_back(S("cats-on-pages", "name", "cats.example.org"));
    node("farm_mail_domain", "cat-mail");
    node("farm_import_csv", "new-cats");
    c.push_back(S("new-cats", "file", "imports/incoming-cats.csv"));
    c.push_back(S("new-cats", "glyph", "contact"));
    // the network
    node("farm_profile", "me");
    node("farm_profile", "mittens");
    c.push_back(S("mittens", "username", "mittens"));
    node("farm_peer", "office-nas");

    const std::vector<std::string> wires = {
        L("here", "river", "home", "reservoirs"), L("home", "river", "this-db", "rests-in"),
        L("new-cats", "rows", "this-db", "import"),
        L("this-db", "all", "chambers", "mantle"),
        L("chambers", "data", "pictures", "data"), L("chambers", "assets", "pictures", "assets"),
        L("chambers", "data", "public", "mantle"), L("not-private", "query", "public", "where"),
        L("public", "kept", "cats", "mantle"), L("only-cats", "query", "cats", "where"),
        L("cats", "kept", "gentle-cats", "mantle"), L("gentle-q", "query", "gentle-cats", "where"),
        L("cats", "kept", "orange-cats", "mantle"), L("orange-q", "query", "orange-cats", "where"),
        L("cats", "kept", "calico-cats", "mantle"), L("calico-q", "query", "calico-cats", "where"),
        L("orange-cats", "kept", "warm-coats", "mantles"), L("calico-cats", "kept", "warm-coats", "mantles"),
        L("warm-coats", "mantle", "how-many-warm", "mantle"),
        L("birthday-q", "query", "upcoming-q", "queries"), L("future-q", "query", "upcoming-q", "queries"),
        L("public", "kept", "upcoming", "mantle"), L("upcoming-q", "query", "upcoming", "where"),
        L("cats", "kept", "reachable", "mantle"), L("has-email", "query", "reachable", "where"),
        L("cats", "kept", "cat-count", "mantle"), L("chambers", "assets", "photo-weight", "mantle"),
        L("public", "kept", "colony-site", "data"), L("public", "kept", "cat-news", "data"),
        L("reachable", "kept", "cat-news", "audience"), L("upcoming", "kept", "birthdays", "data"),
        L("cats", "kept", "where-they-nap", "data"),
        L("colony-site", "preview", "preview-here", "renditions"),
        L("cat-news", "preview", "preview-here", "renditions"),
        L("birthdays", "preview", "preview-here", "renditions"),
        L("where-they-nap", "preview", "preview-here", "renditions"),
        L("colony-site", "publish", "cats-on-pages", "renditions"),
        L("birthdays", "publish", "cats-on-pages", "renditions"),
        L("where-they-nap", "publish", "cats-on-pages", "renditions"),
        L("cat-news", "publish", "cat-mail", "renditions"),
        L("github-demo", "key", "cats-on-pages", "key"), L("cloudflare-demo", "key", "cat-cdn", "key"),
        L("cloudflare-demo", "key", "cat-mail", "key"), L("cloudflare-demo", "key", "cdn-key-expiry", "key"),
        L("here", "river", "photo-river", "reservoirs"), L("usb-stick", "river", "photo-river", "reservoirs"),
        L("cat-cdn", "river", "hold-uploads", "river"), L("hold-uploads", "river", "photo-river", "reservoirs"),
        L("chambers", "assets", "mirror-photos", "mantle"), L("photo-river", "river", "mirror-photos", "river"),
        L("photo-river", "river", "photo-gauge", "river"),
        L("mittens", "profile", "office-nas", "profile"),
    };
    const farm::Graph existing = farm::read(farm_scene);
    for (const auto& w : wires) {
        bool have = false;
        for (const auto& e : existing.wires) have |= w == "link " + e.from + " " + e.to + " --relation " + e.relation;
        if (!have) c.push_back(w);
    }
    if (!back.empty()) c.push_back("use " + back);
    note = "the Cat Colony showcase: website, birthdays calendar, map, newsletter, a photo river, keys and a web domain";
    return c;
}

farm::Context make_context(maiz::Core& core, const std::filesystem::path& base, const hormiga::Vault* vault,
                           const Device& dev, std::function<std::string(const std::string&)> presence) {
    namespace fs = std::filesystem;
    farm::Context c;
    maiz::Core* cp = &core;
    c.device = dev.kind;
    c.serves_local = dev.serves_local;
    c.me = dev.me;
    c.presence = presence ? presence : [me = dev.me](const std::string& u) {
        return u == me ? std::string("here (this device)") : std::string();
    };

    auto resolve = [base, also = dev.also](const std::string& p) -> fs::path {
        std::error_code ec;
        fs::path x(p);
        if (x.is_absolute()) return x;
        if (fs::exists(base / x, ec)) return base / x;
        if (fs::exists(base / "assets" / x, ec)) return base / "assets" / x;
        for (const auto& d : also)
            if (!d.empty() && fs::exists(d / x, ec)) return d / x;
        return base / x;
    };
    c.exists = [resolve](const std::string& p) {
        std::error_code ec;
        return fs::exists(resolve(p), ec);
    };
    c.folder = [resolve](const std::string& p) {
        farm::Folder f;
        std::error_code ec;
        const fs::path dir = resolve(p);
        f.exists = fs::is_directory(dir, ec);
        if (f.exists)
            for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
                if (it->is_regular_file(ec)) ++f.files, f.bytes += (long long)it->file_size(ec);
        const fs::space_info s = fs::space(f.exists ? dir : dir.parent_path(), ec);
        if (!ec) f.free = (long long)s.available;
        return f;
    };
    c.asset_files = [base] {
        std::vector<std::string> out;
        std::error_code ec;
        for (fs::directory_iterator it(base / "assets", ec), end; !ec && it != end; it.increment(ec))
            if (it->is_regular_file(ec)) out.push_back(it->path().filename().string());
        return out;
    };
    c.key_state = [vault, base](const std::string& entry) -> std::string {
        if (!vault) return "no-vault";
        if (!vault->unlocked()) return hormiga::Vault::exists((base / "org.miga").string()) ? "locked" : "no-vault";
        return vault->get(entry).empty() ? "missing" : "present";
    };

    // ── the chambers ────────────────────────────────────────────────────
    auto data = std::make_shared<maiz::Scene>([cp] {
        maiz::ProjectOptions po;
        po.mantle = kDataMantle;
        return maiz::project_scene(*cp, po);
    }());
    c.match = [](const std::string& expr, const farm::Rune& r) {
        if (!r.scene) return hormiga::query_matches(expr, maiz::Scene{}, r.node);
        return hormiga::query_matches(expr, *r.scene, r.node);
    };
    c.document_exists = [cp](const std::string& name) {
        for (const auto& n : project(*cp, hormiga::chambers::kDocuments).nodes)
            if (n.name == name || field_value(n, "of") == name) return field_value(n, "gone").empty();
        return false;
    };
    c.chamber = [cp, data](const std::string& which) {
        /* THE REAL CHAMBERS (domain/chambers.hpp). Data is the organization's
         * mantle; the other three are mantles of their own, kept in step by
         * `chamber_commands`. */
        std::vector<farm::Rune> out;
        std::shared_ptr<maiz::Scene> s = which == "data" ? data : std::make_shared<maiz::Scene>(project(*cp, which));
        for (const auto& n : s->nodes) {
            long long b = rune_bytes(n);
            if (which == "assets") {
                const std::string sz = field_value(n, "bytes");
                if (!sz.empty()) b = std::atoll(sz.c_str());
            }
            out.push_back({which, n, s, b});
        }
        return out;
    };
    return c;
}

bool farm_exists(maiz::Core& core) {
    for (const auto& m : mantle_names(core))
        if (m == farm::kMantle) return true;
    return false;
}

maiz::Scene project_farm(maiz::Core& core) {
    maiz::ProjectOptions po;
    po.mantle = farm::kMantle;
    maiz::Scene s = maiz::project_scene(core, po);
    farm::resolve_named_wires(s);
    return s;
}

farm::SeedInfo seed_info(maiz::Core& core, const std::string& website_doc, const std::string& actor) {
    farm::SeedInfo s;
    s.actor = actor;
    s.website = website_doc;
    if (s.website.empty())
        for (const auto& m : mantle_names(core))
            if (is_document_mantle(m)) {
                s.website = m;
                break;
            }
    maiz::ProjectOptions po;
    po.mantle = kDataMantle;
    for (const auto& n : maiz::project_scene(core, po).nodes) {
        if (n.glyph == "calview" && s.calendar.empty()) s.calendar = n.name;
        if (n.glyph == "map" && s.map.empty()) s.map = n.name;
    }
    return s;
}

std::vector<Row> connection_rows(const farm::Graph& g, farm::Evaluator& ev) {
    static const std::vector<std::pair<const char*, std::vector<std::string>>> groups = {
        {"Where the database rests", {"miga"}},
        {"Where files are kept", {"river"}},
        {"Documents and where they go", {"website", "newsletter", "calendar", "map"}},
        {"Domains", {"local-domain", "web-domain"}},
        {"Keys", {"key"}},
        {"The network", {"profile"}},
        {"Filters: what documents may see", {"filter"}},
    };
    std::vector<Row> rows;
    for (const auto& [title, ids] : groups)
        for (const auto& n : g.nodes) {
            if (!n.kind || n.kind->planned) continue;
            if (std::find(ids.begin(), ids.end(), n.kind->id) == ids.end()) continue;
            const farm::Face f = ev.face(n.name);
            rows.push_back({title, n.name, n.kind->label, f.ready.state, f.ready.why,
                            f.lines.empty() ? std::string() : f.lines.front()});
        }
    for (const auto& n : g.nodes)
        if (n.kind && n.kind->planned)
            rows.push_back({"Planned", n.name, n.kind->label, "planned", "", n.kind->doc});
    for (const auto& b : farm::audit(g)) rows.push_back({"Wires that went around the door", "", "", "failing", b, ""});
    return rows;
}

std::map<std::string, maiz::PortStyle> port_styles() {
    std::map<std::string, maiz::PortStyle> m;
    for (const auto& t : farm::types()) {
        maiz::PortStyle s;
        s.color = IM_COL32((t.rgb >> 16) & 0xff, (t.rgb >> 8) & 0xff, t.rgb & 0xff, 255);
        s.shape = t.shape == farm::Shape::Field       ? maiz::PortShape::Diamond
                  : t.shape == farm::Shape::Reference ? maiz::PortShape::Square
                  : t.shape == farm::Shape::Value     ? maiz::PortShape::Ring
                                                      : maiz::PortShape::Circle;
        m[t.id] = s;
    }
    return m;
}

maiz::WireWriter wire_writer(std::function<const maiz::Scene&()> scene,
                             std::function<void(const std::string&)> refused) {
    maiz::WireWriter w;
    w.link = [scene, refused](const maiz::PortRef& from, const maiz::PortRef& to) -> std::vector<std::string> {
        const maiz::Scene& s = scene();
        /* A port by index, on a node that may not exist yet: the add-and-link
         * box mints the node in the SAME batch as the wire, so its kind is read
         * from the name Void Maiz gives it (`<glyph>-<tag><n>`). */
        struct End { const farm::Kind* kind = nullptr; std::string port; bool out = false; bool exists = false; };
        auto resolve = [&](const maiz::PortRef& p) {
            End e;
            if (const maiz::SceneNode* sn = s.find(p.node)) {
                e.exists = true;
                e.kind = farm::kind_by_glyph(sn->glyph);
            } else {
                std::size_t best = 0;
                for (const auto& k : farm::kinds())
                    if (p.node.rfind(k.glyph + "-", 0) == 0 && k.glyph.size() > best) {
                        best = k.glyph.size();
                        e.kind = &k;
                    }
            }
            if (e.kind && p.port >= 1 && p.port <= (int)e.kind->ports.size()) {
                e.port = e.kind->ports[(std::size_t)p.port - 1].name;
                e.out = e.kind->ports[(std::size_t)p.port - 1].out;
            }
            return e;
        };
        maiz::PortRef a = from, b = to;
        End ea = resolve(a), eb = resolve(b);
        if (!ea.out && eb.out) { // dragged from an input to an output: turn it round
            std::swap(a, b);
            std::swap(ea, eb);
        }
        if (!ea.kind || !eb.kind || ea.port.empty() || eb.port.empty() || !ea.out || eb.out) {
            if (refused) refused("a wire runs from an output to an input");
            return {};
        }
        if (ea.exists && eb.exists) {
            const farm::Graph g = farm::read(s);
            const farm::Wire* old = nullptr; // a one-wire input's occupant: the canvas unlinks it itself
            const std::string why = farm::check_plug(g, a.node, ea.port, b.node, eb.port, &old);
            if (!why.empty()) {
                if (refused) refused(why);
                return {};
            }
        } else {
            const std::string ta = ea.kind->port(ea.port, true)->type, tb = eb.kind->port(eb.port, false)->type;
            if (!ta.empty() && !tb.empty() && ta != tb) {
                if (refused) refused("refused: " + eb.port + " takes a " + tb + ", and " + ea.port + " is a " + ta);
                return {};
            }
        }
        return {"link " + a.node + " " + b.node + " --relation " + ea.port + ":" + eb.port};
    };
    w.unlink = [](const maiz::SceneWire& wire) -> std::vector<std::string> {
        return {"unlink " + wire.from + " " + wire.to + " --relation " + wire.relation};
    };
    return w;
}

std::string key_set(hormiga::Vault& vault, const std::filesystem::path& vault_file, const std::string& entry,
                    const std::string& value) {
    if (!vault.unlocked()) return "the vault is locked on this device, so the value was not stored";
    if (value.empty()) return "no value given; nothing stored";
    vault.set(entry, value);
    if (!vault.save(vault_file.string())) return "could not write the vault: " + vault.error();
    return "stored in this device's vault (sealed). It is never written to a field, the log or an export.";
}

} // namespace hormiga::farmhost
