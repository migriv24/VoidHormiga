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

/* ── v1 -> v2: the migration (okf/concepts/platform/antfarm/v2/migration.md) ──
 *
 * Reads the v1 `antfarm` mantle and writes a v2 `farm` mantle that does the same
 * work, as ONE list of ordinary commands (so it is logged, undoable, replayable).
 * The v1 mantle is not edited: it keeps running the Publish tab and LAN sharing,
 * which still read it, and it is what a person compares the result against.
 * Key FILES are the one thing commands cannot carry: they are listed, and the
 * caller seals them into this device's vault when it applies. */
namespace {
std::string mq(const std::string& node, const std::string& field, const std::string& value) {
    return "set " + node + " " + field + " " + farm::json_quote(value);
}
std::string ml(const std::string& a, const std::string& out, const std::string& b, const std::string& in) {
    return "link " + a + " " + b + " --relation " + out + ":" + in;
}
std::string safe_name(std::string s) {
    for (char& c : s)
        if (!(std::isalnum((unsigned char)c) || c == '-' || c == '_')) c = '-';
    return s.empty() ? std::string("node") : s;
}
} // namespace

MigratePlan migrate_plan(maiz::Core& core) {
    MigratePlan p;
    if (farm_exists(core)) {
        p.refused = "this database already has a v2 Antfarm (the `farm` mantle). The migration builds one from "
                    "v1 and will not write over an existing graph.";
        return p;
    }
    const maiz::Scene v1 = project(core, kAntfarmMantle);
    if (v1.nodes.empty()) {
        p.refused = "there is no v1 Antfarm to migrate; `farm init` creates a v2 one";
        return p;
    }
    auto& c = p.commands;
    auto add = [&](const char* glyph, const std::string& name) { c.push_back(std::string("rune new ") + glyph + " " + name); };
    auto f = [](const maiz::SceneNode& n, const char* k) { return field_value(n, k); };
    c.push_back(std::string("mantle new ") + farm::kMantle);
    add("farm_miga", "this-db");
    c.push_back(mq("this-db", "version", "2"));
    c.push_back(mq("this-db", "migrated_from", kAntfarmMantle));
    add("farm_separate", "chambers");
    c.push_back(ml("this-db", "all", "chambers", "mantle"));
    add("farm_tunnel_assets", "pictures");
    c.push_back(ml("chambers", "data", "pictures", "data"));
    c.push_back(ml("chambers", "assets", "pictures", "assets"));

    // where the database rests: v1's SQLite store and asset folder become one home river
    std::string assets_dir = "assets";
    for (const auto& n : v1.nodes)
        if (n.glyph == "hol_fs_assets" && !f(n, "dir").empty()) assets_dir = f(n, "dir");
    add("farm_folder", "here");
    c.push_back(mq("here", "path", assets_dir));
    c.push_back(mq("here", "placement", "each"));
    add("farm_river", "home");
    c.push_back(ml("here", "river", "home", "reservoirs"));
    c.push_back(ml("home", "river", "this-db", "rests-in"));
    p.mapped.push_back("hol_sqlite + hol_fs_assets -> the home river (folder `" + assets_dir + "`)");

    // keys: one per distinct credential a v1 node names
    std::map<std::string, std::string> key_for; // vault entry or file -> key node
    auto key = [&](const std::string& provider, const std::string& entry, const std::string& file,
                   const std::string& owner) -> std::string {
        const std::string id = !entry.empty() ? entry : file;
        if (id.empty()) return "";
        if (auto it = key_for.find(id); it != key_for.end()) return it->second;
        const std::string name = safe_name(provider + "-key");
        std::string unique = name;
        for (int i = 2; std::any_of(key_for.begin(), key_for.end(), [&](auto& kv) { return kv.second == unique; }); ++i)
            unique = name + "-" + std::to_string(i);
        add("farm_key", unique);
        c.push_back(mq(unique, "provider", provider));
        const std::string vault_entry = !entry.empty() ? entry : "farm-key:" + unique;
        c.push_back(mq(unique, "vault_entry", vault_entry));
        if (entry.empty()) p.key_files.push_back({file, vault_entry, owner});
        key_for[id] = unique;
        return unique;
    };

    std::string local_domain, website_domain;
    std::vector<std::string> web_domains;
    std::string dns_name;
    for (const auto& n : v1.nodes)
        if (n.glyph == "hol_dns" && !f(n, "domain").empty()) dns_name = f(n, "domain");
    for (const auto& n : v1.nodes) {
        const std::string g = n.glyph;
        if (g == "org_core" || g == "hol_sqlite" || g == "hol_fs_assets" || g == "hol_dns" || g == "deployment" ||
            g == "member" || g == "peer")
            continue;
        if (g == "hol_csv") {
            const std::string name = safe_name(n.name);
            add("farm_import_csv", name);
            c.push_back(mq(name, "glyph", "contact"));
            c.push_back(ml(name, "rows", "this-db", "import"));
            p.mapped.push_back(n.name + " (CSV) -> Import CSV `" + name + "` (name its file)");
        } else if (g == "hol_imgbb") {
            const std::string k = key("imgbb", "imgbb_key", f(n, "key_file"), n.name);
            add("farm_image_host", safe_name(n.name));
            c.push_back(mq(safe_name(n.name), "provider", "imgbb"));
            if (!k.empty()) c.push_back(ml(k, "key", safe_name(n.name), "key"));
            add("farm_river", "photos-online");
            c.push_back(ml(safe_name(n.name), "river", "photos-online", "reservoirs"));
            p.mapped.push_back(n.name + " (ImgBB) -> an image host in the river `photos-online`");
        } else if (g == "hol_object_store") {
            const std::string b = safe_name(n.name);
            add("farm_bucket", b);
            for (const char* fld : {"bucket", "region", "endpoint", "access_key_id", "prefix", "public_url"})
                if (!f(n, fld).empty()) c.push_back(mq(b, fld, f(n, fld)));
            const std::string k = key("aws", f(n, "secret_key"), f(n, "secret_file"), n.name);
            if (!k.empty()) c.push_back(ml(k, "key", b, "key"));
            p.mapped.push_back(n.name + " (object store) -> Bucket `" + b + "`");
        } else if (g == "hol_localhost") {
            local_domain = safe_name(n.name);
            add("farm_local_domain", local_domain);
            c.push_back(mq(local_domain, "port", f(n, "port").empty() ? "8780" : f(n, "port")));
            c.push_back(mq(local_domain, "placement", "each"));
            p.mapped.push_back(n.name + " (localhost) -> Local domain `" + local_domain + "`");
        } else if (g == "hol_github" || g == "hol_static_host") {
            const std::string d = safe_name(n.name);
            add("farm_web_domain", d);
            const bool gh = g == "hol_github";
            c.push_back(mq(d, "host", gh ? "github" : (f(n, "provider").empty() ? "cloudflare" : f(n, "provider"))));
            c.push_back(mq(d, "target", f(n, gh ? "repo" : "project")));
            if (gh && !f(n, "branch").empty()) c.push_back(mq(d, "branch", f(n, "branch")));
            if (!gh && !f(n, "account_id").empty()) c.push_back(mq(d, "account", f(n, "account_id")));
            if (!dns_name.empty()) c.push_back(mq(d, "name", dns_name));
            const std::string k = key(gh ? "github" : "cloudflare", f(n, "token_key"), f(n, "token_file"), n.name);
            if (!k.empty()) c.push_back(ml(k, "key", d, "key"));
            web_domains.push_back(d);
            if (website_domain.empty()) website_domain = d;
            p.mapped.push_back(n.name + (gh ? " (GitHub Pages)" : " (static host)") + " -> Web domain `" + d + "`" +
                               (dns_name.empty() ? "" : ", named " + dns_name));
            if (!f(n, "deploy_cmd").empty())
                p.notes.push_back(n.name + " has a custom deploy_cmd: v2 publishes through the same v1 code, which "
                                           "still reads it from the v1 node");
        } else if (g == "hol_sheets") {
            add("farm_import_sheets", safe_name(n.name));
            p.mapped.push_back(n.name + " (Sheets) -> Import Google Sheets (planned)");
        } else if (g == "hol_html") {
            p.mapped.push_back(n.name + " (HTML publisher) -> one document node per Builder document (below)");
        } else if (g == "hol_lan_share" || g == "hol_membership") {
            p.kept.push_back(n.name + " (" + g + "): LAN sharing still reads it from v1, so it stays there");
        } else {
            p.skipped.push_back(n.name + " (" + g + ")");
        }
    }

    // one document node per Builder document, granted the Data chamber, previewed and published
    for (const auto& m : mantle_names(core)) {
        if (!is_document_mantle(m)) continue;
        std::string kind = "newsletter";
        for (const auto& n : project(core, m).nodes)
            if (n.glyph == "document" && !field_value(n, "kind").empty()) kind = field_value(n, "kind");
        const std::string d = safe_name("doc-" + m);
        add(kind == "website" ? "farm_website" : "farm_newsletter", d);
        c.push_back(mq(d, "document", m));
        c.push_back(ml("chambers", "data", d, "data"));
        if (!local_domain.empty()) c.push_back(ml(d, "preview", local_domain, "renditions"));
        if (kind == "website" && !website_domain.empty()) c.push_back(ml(d, "publish", website_domain, "renditions"));
        p.mapped.push_back("document `" + m + "` -> a " + kind + " node");
    }
    if (web_domains.size() > 1)
        p.notes.push_back("v1 had " + std::to_string(web_domains.size()) + " publish hosts; the website publishes to `" +
                          website_domain + "`. Wire the others where you want them.");
    add("farm_profile", "me");
    return p;
}

std::string migrate_report(const MigratePlan& p, bool applied) {
    if (!p.refused.empty()) return "refused: " + p.refused + "\n";
    std::string o = applied ? "migrated to Antfarm v2 (one batch; `undo` reverses it in this session)\n"
                            : "rehearsal: what `farm migrate apply` would do\n";
    auto list = [&](const char* title, const std::vector<std::string>& v) {
        if (v.empty()) return;
        o += std::string(title) + "\n";
        for (const auto& s : v) o += "  " + s + "\n";
    };
    list("MAPPED", p.mapped);
    list("STAYS IN V1 (still read there)", p.kept);
    list("NOT MIGRATED", p.skipped);
    if (!p.key_files.empty()) {
        o += "KEY FILES, sealed into this device's vault on apply (the files are not deleted)\n";
        for (const auto& k : p.key_files) o += "  " + k.file + " -> " + k.entry + " (from " + k.node + ")\n";
    }
    list("NOTES", p.notes);
    o += std::to_string(p.commands.size()) + " commands\n";
    return o;
}

bool migrated(maiz::Core& core) {
    if (!farm_exists(core)) return false;
    for (const auto& n : project(core, farm::kMantle).nodes)
        if (n.glyph == "farm_miga" && !field_value(n, "migrated_from").empty()) return true; // init alone is not a move
    return false;
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

std::string grant_line(maiz::Core& core, const farm::Context& ctx, const std::string& document) {
    if (!farm_exists(core)) return "";
    const maiz::Scene s = project_farm(core);
    const farm::Graph g = farm::read(s);
    farm::Evaluator ev(g, ctx);
    std::string out;
    for (const auto& n : g.nodes) {
        if (!n.kind || (n.kind->id != "website" && n.kind->id != "newsletter")) continue;
        const std::string want = n.field("document");
        if (want != document && want != "doc-" + document) continue;
        const auto dw = g.into(n.name, "data");
        const std::size_t all = ev.chamber("data").size();
        if (dw.empty()) {
            out += (out.empty() ? "" : "  ·  ") + n.name + " is not connected in the Antfarm, so it may publish nothing";
            continue;
        }
        std::size_t seen = 0;
        for (const auto& r : ev.eval(dw[0]->from, dw[0]->out).runes) seen += r.chamber == "data";
        out += (out.empty() ? "" : "  ·  ") + n.name + " may publish " + std::to_string(seen) + " of " +
               std::to_string(all) + " data runes" + (seen < all ? ", narrowed by " + dw[0]->from : "");
    }
    return out;
}

void apply_grant(maiz::Scene& data, const std::set<std::string>* grant) {
    if (!grant) return;
    data.nodes.erase(std::remove_if(data.nodes.begin(), data.nodes.end(),
                                    [&](const maiz::SceneNode& n) { return !grant->count(n.name); }),
                     data.nodes.end());
    data.wires.erase(std::remove_if(data.wires.begin(), data.wires.end(),
                                    [&](const maiz::SceneWire& w) { return !grant->count(w.from) || !grant->count(w.to); }),
                     data.wires.end());
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
