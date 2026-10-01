/* antfarm/farm_kinds.cpp — the v2 node kinds, declared once.
 *
 * The palette, `farm kinds`, `farm ports`, the glyph registration and the check
 * all read THIS table, so a kind cannot be placeable and undeclared, or declared
 * and unplaceable (the hand-maintained v1 palette fell behind its glyphs twice).
 * okf/concepts/platform/antfarm/v2/ names every kind below; the page says why,
 * this says what. */
#include "antfarm/farm.hpp"

#include "json.hpp"

namespace farm {

const std::vector<TypeInfo>& types() {
    static const std::vector<TypeInfo> t = {
        {"mantle", "Mantle", Shape::Payload, 0x4a7fd0},
        {"query", "Query", Shape::Field, 0x2fb3a8},
        {"rendition", "Rendition", Shape::Payload, 0x4caf6a},
        {"river", "River", Shape::Reference, 0x9a6ad8},
        {"key", "Key", Shape::Reference, 0xe0b630},
        {"domain", "Domain", Shape::Reference, 0xe0803a},
        {"profile", "Profile", Shape::Reference, 0xe8e8ee},
        {"value", "Value", Shape::Value, 0x9098a4},
    };
    return t;
}

const TypeInfo* type_info(std::string_view id) {
    for (const auto& t : types())
        if (id == t.id) return &t;
    return nullptr;
}

const char* stratum_name(Stratum s) {
    switch (s) {
    case Stratum::Chambers: return "chambers";
    case Stratum::Ground: return "ground";
    case Stratum::Surface: return "surface";
    case Stratum::Network: return "network";
    }
    return "chambers";
}

const PortDecl* Kind::port(std::string_view name) const {
    if (const PortDecl* p = port(name, true)) return p;
    return port(name, false);
}

const PortDecl* Kind::port(std::string_view name, bool out) const {
    for (const auto& p : ports)
        if (p.name == name && p.out == out) return &p;
    return nullptr;
}

namespace {

PortDecl in(const char* name, const char* type, bool many = false, bool optional = false,
            bool writes = false) {
    return PortDecl{name, type, false, many, optional, writes};
}
PortDecl out(const char* name, const char* type) { return PortDecl{name, type, true}; }

const FieldDecl kPlacement{"placement", "Runs on (any, each, or a profile)", ""};

std::vector<Kind> build() {
    std::vector<Kind> k;
    auto add = [&](Kind x) { k.push_back(std::move(x)); };
    const unsigned CH = 0x3f6fae, DOC = 0x2e6b4f, GR = 0x8a6a3a, SU = 0x3f7f78,
                   KEY = 0xa8841c, NET = 0x6b7a8f, PL = 0x5d6068;

    // ── the chambers: the .miga and its mantles (mantles.md) ──────────────
    add({"miga", "farm_miga", "Miga (this database)", "Chambers", Stratum::Chambers, false,
         "this database: every chamber as one mantle, where it rests, what it imports",
         CH, 118, {},
         {in("rests-in", "river"), in("backups", "river", true, true),
          in("import", "mantle", true, true, true), out("all", "mantle")}});
    add({"separate", "farm_separate", "Separate chambers", "Chambers", Stratum::Chambers, false,
         "splits the database into Data, Assets, Network and Documents",
         CH, 88, {},
         {in("mantle", "mantle"), out("data", "mantle"), out("assets", "mantle"),
          out("network", "mantle"), out("documents", "mantle")}});
    add({"filter", "farm_filter", "Filter", "Chambers", Stratum::Chambers, false,
         "keeps the runes a query selects, and hands the others on",
         CH, 52, {},
         {in("mantle", "mantle"), in("where", "query", false, true), out("kept", "mantle"), out("rest", "mantle")}});
    add({"join", "farm_join", "Join", "Chambers", Stratum::Chambers, false,
         "several mantles as one", CH, 36, {},
         {in("mantles", "mantle", true), out("mantle", "mantle")}});
    add({"count", "farm_count", "Count", "Chambers", Stratum::Chambers, false,
         "how many runes", CH, 36, {}, {in("mantle", "mantle"), out("runes", "value")}});
    add({"measure", "farm_measure", "Measure", "Chambers", Stratum::Chambers, false,
         "how many runes and how many bytes", CH, 52, {},
         {in("mantle", "mantle"), out("bytes", "value"), out("runes", "value")}});

    // ── queries: rules, not sets (◆) ───────────────────────────────────────
    const unsigned Q = 0x2a7f78;
    add({"tag", "farm_q_tag", "Tag query", "Queries", Stratum::Chambers, false,
         "a Void Core tag expression: volunteer AND NOT private", Q, 36,
         {{"expr", "Tag expression", ""}}, {out("query", "query")}});
    add({"glyph", "farm_q_glyph", "Glyph query", "Queries", Stratum::Chambers, false,
         "runes of one or more glyphs: contact, event", Q, 36,
         {{"glyphs", "Glyphs (comma separated)", ""}}, {out("query", "query")}});
    add({"date", "farm_q_date", "Date window", "Queries", Stratum::Chambers, false,
         "dated runes: future, today, past, recurring or undated", Q, 36,
         {{"when", "When", "combo:future,today,past,recurring,undated"}}, {out("query", "query")}});
    add({"field", "farm_q_field", "Field query", "Queries", Stratum::Chambers, false,
         "a field that is set, empty, or equal to a value", Q, 36,
         {{"key", "Field", ""}, {"op", "Test", "combo:is set,is empty,equals"}, {"value", "Value", ""}},
         {out("query", "query")}});
    add({"and", "farm_q_and", "And", "Queries", Stratum::Chambers, false,
         "every query must hold", Q, 30, {}, {in("queries", "query", true), out("query", "query")}});
    add({"or", "farm_q_or", "Or", "Queries", Stratum::Chambers, false,
         "any query may hold", Q, 30, {}, {in("queries", "query", true), out("query", "query")}});
    add({"not", "farm_q_not", "Not", "Queries", Stratum::Chambers, false,
         "the opposite of a query", Q, 30, {}, {in("query", "query"), out("query", "query")}});

    // ── tunnels: pure mappings between chambers (mantles.md §4) ───────────
    add({"tunnel-assets", "farm_tunnel_assets", "Tunnel: Data to Assets", "Tunnels",
         Stratum::Chambers, false,
         "every picture a rune names, checked: referenced, missing, remote, orphaned",
         CH, 104, {}, {in("data", "mantle"), in("assets", "mantle")}});

    // ── documents: a presentation of the database (documents.md) ───────────
    const std::vector<FieldDecl> dfields = {{"document", "Document (its name)", ""},
                                            {"mount", "Mount path (/ or /events/)", ""}};
    auto doc = [&](const char* id, const char* glyph, const char* label, const char* what) {
        std::vector<PortDecl> p = {in("data", "mantle")};
        if (std::string(id) == "newsletter") p.push_back(in("audience", "mantle", false, true));
        p.push_back(out("preview", "rendition"));
        p.push_back(out("publish", "rendition"));
        add({id, glyph, label, "Documents", Stratum::Chambers, false, what, DOC, 104, dfields, p});
    };
    doc("website", "farm_website", "Website", "a website built from the database");
    doc("newsletter", "farm_newsletter", "Newsletter", "a newsletter issue: an email and a web page");
    doc("calendar", "farm_calendar", "Calendar", "a calendar: an .ics feed and a calendar page");
    doc("map", "farm_map", "Map", "a map: a map page and a GeoJSON layer");

    // ── rivers and reservoirs (rivers.md) ──────────────────────────────────
    add({"river", "farm_river", "River", "Rivers", Stratum::Ground, false,
         "a named place data rests, over its reservoirs (reads nearest, writes home first)",
         GR, 70, {}, {in("reservoirs", "river", true), out("river", "river")}});
    add({"folder", "farm_folder", "Folder reservoir", "Rivers", Stratum::Ground, false,
         "a folder on the device it runs on; can hold a live database",
         GR, 88, {{"path", "Folder (beside the database)", ""}, kPlacement}, {out("river", "river")}});
    add({"bucket", "farm_bucket", "Bucket reservoir", "Rivers", Stratum::Surface, false,
         "an S3-compatible bucket: AWS, R2, MinIO, Backblaze",
         SU, 88,
         {{"bucket", "Bucket", ""}, {"access_key_id", "Access key id (the secret is the key node)", ""},
          {"endpoint", "Endpoint (blank = AWS)", ""},
          {"region", "Region", ""}, {"prefix", "Prefix", ""}, {"public_url", "Public address", ""}},
         {in("key", "key"), out("river", "river")}});
    add({"image-host", "farm_image_host", "Image host", "Rivers", Stratum::Surface, false,
         "a host that takes a file and returns a link (ImgBB); cannot list what it holds",
         SU, 70, {{"provider", "Provider", "combo:imgbb"}}, {in("key", "key"), out("river", "river")}});
    add({"peer", "farm_peer", "Peer reservoir", "Rivers", Stratum::Surface, true,
         "a reservoir on another member's device, over LAN or Reticulum",
         PL, 52, {}, {in("profile", "profile"), out("river", "river")}});
    add({"store", "farm_store", "Store", "Rivers", Stratum::Ground, false,
         "when run, puts a mantle's files, or a sealed copy, into a river",
         GR, 52, {kPlacement}, {in("mantle", "mantle", false, false, true), in("river", "river")}});
    add({"gauge", "farm_gauge", "Gauge", "Rivers", Stratum::Ground, false,
         "how full a river is, measured where it can be without the network",
         GR, 70, {}, {in("river", "river"), out("used", "value"), out("free", "value")}});
    add({"distributary", "farm_distributary", "Distributary", "Rivers", Stratum::Ground, true,
         "routes objects matching a query to another river", PL, 36, {},
         {in("river", "river"), in("where", "query"), out("river", "river")}});
    add({"dam", "farm_dam", "Dam", "Rivers", Stratum::Ground, true,
         "holds a river's writes, with a reason", PL, 36, {{"reason", "Why", ""}},
         {in("river", "river"), out("river", "river")}});

    // ── keys (keys.md) ─────────────────────────────────────────────────────
    add({"key", "farm_key", "Key", "Keys", Stratum::Surface, false,
         "a credential shared between members; its value lives in each vault, never in a field",
         KEY, 88,
         {{"provider", "Provider", "combo:cloudflare,github,google-maps,imgbb,aws,weather,other"},
          {"vault_entry", "Vault entry", ""}, {"expires", "Expires (YYYY-MM-DD)", "date"},
          {"added_by", "Added by", ""}},
         {out("key", "key")}});
    add({"expiry", "farm_expiry", "Expiry", "Keys", Stratum::Chambers, false,
         "days until a key expires", KEY, 36, {}, {in("key", "key"), out("days", "value")}});
    add({"usage", "farm_usage", "Usage", "Keys", Stratum::Chambers, true,
         "who used a key, from which device, for what: read from our own log", PL, 36, {},
         {in("key", "key"), out("calls", "value")}});
    add({"budget", "farm_budget", "Budget", "Keys", Stratum::Chambers, true,
         "turns a key to needs when a period's use passes a limit", PL, 36,
         {{"limit", "Limit", ""}}, {in("value", "value")}});

    // ── domains (documents.md §4) ──────────────────────────────────────────
    add({"local-domain", "farm_local_domain", "Local domain", "Domains", Stratum::Ground, false,
         "localhost on a device that can serve it; needs no key",
         GR, 70, {{"port", "Port", ""}, kPlacement},
         {in("renditions", "rendition", true, false, true), out("domain", "domain")}});
    add({"web-domain", "farm_web_domain", "Web domain", "Domains", Stratum::Surface, false,
         "a public host: GitHub Pages, Cloudflare Pages or an S3 website; needs a key",
         SU, 104,
         {{"host", "Host", "combo:github,cloudflare,s3"}, {"target", "Repository or project", ""},
          {"branch", "Branch (GitHub)", ""}, {"account", "Account id (Cloudflare)", ""},
          {"name", "Custom name (example.org)", ""}},
         {in("renditions", "rendition", true, false, true), in("key", "key"), out("domain", "domain")}});
    add({"mail-domain", "farm_mail_domain", "Mail domain", "Domains", Stratum::Surface, true,
         "a sender for newsletters; what leaves is named before it leaves", PL, 52,
         {{"from", "From address", ""}},
         {in("renditions", "rendition", true, false, true), in("key", "key")}});

    // ── sources ────────────────────────────────────────────────────────────
    add({"import-csv", "farm_import_csv", "Import CSV", "Sources", Stratum::Ground, false,
         "a spreadsheet exported as CSV; importing is a run, one batch, one undo",
         GR, 52, {{"file", "CSV file", "path"}, {"glyph", "Import as", "combo:contact,organization,event,resource,job"}},
         {out("rows", "mantle")}});
    add({"import-sheets", "farm_import_sheets", "Import Google Sheets", "Sources", Stratum::Surface,
         true, "a Google Sheet, import only", PL, 36, {{"sheet_url", "Sheet address", ""}},
         {in("key", "key"), out("rows", "mantle")}});

    // ── the network (network.md) ───────────────────────────────────────────
    add({"profile", "farm_profile", "Profile", "Network", Stratum::Network, false,
         "a profile in the Network chamber: one device, one person's key",
         NET, 70, {{"username", "Username (blank = this device)", ""}}, {out("profile", "profile")}});

    // ── tidying ────────────────────────────────────────────────────────────
    add({"reroute", "farm_reroute", "Reroute", "Tidying", Stratum::Chambers, false,
         "fits any wire, to tidy the drawing", 0x8a8f99, 0, {},
         {in("in", ""), out("out", "")}});
    return k;
}

} // namespace

const std::vector<Kind>& kinds() {
    static const std::vector<Kind> k = build();
    return k;
}

const Kind* kind_by_id(std::string_view id) {
    for (const auto& k : kinds())
        if (k.id == id) return &k;
    return nullptr;
}

const Kind* kind_by_glyph(std::string_view glyph) {
    for (const auto& k : kinds())
        if (k.glyph == glyph) return &k;
    return nullptr;
}

std::string glyph_json(const Kind& k) {
    using nlohmann::json;
    json fields = json::array(), labels = json::object(), editors = json::object(),
         ports = json::array();
    for (const auto& f : k.fields) {
        fields.push_back(f.key);
        labels[f.key] = f.label;
        if (!f.editor.empty()) editors[f.key] = f.editor;
    }
    for (const auto& p : k.ports) {
        json jp = {{"name", p.name}, {"dir", p.out ? "out" : "in"}};
        if (!p.type.empty()) jp["type"] = p.type;
        if (!p.out) jp["max"] = p.many ? "many" : "1";
        if (p.optional) jp["optional"] = true;
        if (p.writes) jp["writes"] = true;
        ports.push_back(jp);
    }
    char color[16];
    std::snprintf(color, sizeof color, "#%06x", k.rgb & 0xffffff);
    json hints = {{"color", color}, {"category", k.group}, {"labels", labels},
                  {"ports", ports}, {"stratum", stratum_name(k.stratum)}};
    if (!editors.empty()) hints["editors"] = editors;
    if (k.id == "reroute")
        hints["shape"] = {{"kind", "polygon"}, {"sides", 6}};
    else
        hints["face"] = {{"w", 240}, {"h", k.face_h}};
    json g = {{"glyph", k.glyph},
              {"label", k.planned ? k.label + " (planned)" : k.label},
              {"fields", fields},
              {"hints", hints}};
    return g.dump();
}

void register_glyphs(maiz::Core& core) {
    for (const auto& k : kinds()) core.register_glyph(glyph_json(k));
}

std::string json_quote(std::string_view s) { return nlohmann::json(std::string(s)).dump(); }

std::string field_of(const maiz::SceneNode& n, std::string_view key) {
    for (const auto& f : n.fields) {
        if (f.key != key) continue;
        if (f.value_json.empty() || f.value_json == "null") return "";
        if (!f.is_string) return f.value_json;
        try {
            return nlohmann::json::parse(f.value_json).get<std::string>();
        } catch (...) {
            return f.value_json;
        }
    }
    return "";
}

} // namespace farm
