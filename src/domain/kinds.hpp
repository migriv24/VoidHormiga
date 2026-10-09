/* domain/kinds.hpp — the kinds of thing a database has, as DATA
 * (okf/concepts/foundation/kinds.md).
 *
 * The author, 2026-10-06: "what we want is to be able to organize and name our
 * data whatever we want. so contact, organization, event, etc. could be renamed
 * to: product, vendor, invoice, expiration date ... this will begin the NEXT
 * major version of hormiga."
 *
 * A KIND is how a sort of thing is called, drawn and what it can do. The
 * application ships some (contact, organization, event, ...); a database adds
 * its own, and renames the shipped ones, with `kind` runes in its `kinds`
 * mantle, which travels with it like any mantle. This file merges the two into
 * one REGISTRY, rebuilt at every projection, and is what every front-end,
 * renderer and the CLI asks:
 *
 *   - how does it LOOK?    title, plural, icon (a name), colour, palette group;
 *   - what can it DO?      TRAITS: `located` (on a map), `dated` (on the
 *                          calendar, by `date_field`), `listed` (in a directory);
 *   - what are its FIELDS? a new kind's are registered as a glyph from here.
 *
 * Code that used to ask `glyph == "contact"` asks a trait instead; that is the
 * restructure, and it is done site by site (kinds.md, "the order of work").
 *
 * Renaming a kind or a field changes what people READ, never what is stored: a
 * contact stays glyph `contact` and a field keeps its key.
 *
 * ImGui-free; the icon is a NAME, resolved to a glyph by the front-ends
 * (app/app_shared.cpp, kind_icon). */
#pragma once

#include "domain/scene_value.hpp" // temper::field_value

#include "json.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"
#include "voidmaiz/scene.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace hormiga::kinds {

inline const char* kMantle = "kinds"; // where a database keeps its kinds

struct Field {
    std::string key, label, editor; // editor: "" (text), "date", "multiline:60", "combo:a,b", "image"
};

struct Kind {
    std::string glyph;          // what the data stores; never renamed
    std::string title, plural;  // what people read
    std::string icon = "tag";   // a name (kind_icon resolves it)
    std::string color = "#7a7f8c";
    std::string category = "Things";
    std::string date_field, subtitle_field;
    std::vector<Field> fields;  // a new kind's own (a built-in's come from C++)
    bool located = false, dated = false, listed = false;
    bool builtin = false;       // shipped by the application
    bool legacy = false;        // a kind an earlier version shipped as a glyph (see legacy_kinds)
    bool defined = false;       // a `kind` rune says something about it
    bool palette = true;        // offered in Data's list and the add palette
};

/* ── KINDS AN EARLIER VERSION SHIPPED AS GLYPHS (2026-10-07) ─────────────────
 * 0.1.12 registered `product` in C++ for its grocery workspace (Corner Market),
 * and databases were made from it. 0.2.0 makes a store's products a kind the
 * database defines (Whisker Mart). Without this, a 0.1.12 grocery database
 * would open in 0.2.0 with every product's fields gone from view: the data
 * kept, nothing able to show it. So `product` stays KNOWN, with exactly the
 * fields 0.1.12 gave it, as a kind of the database's own (the directory's
 * consent rule applies to it like any). A database whose `kinds` mantle
 * defines `product` overrides it; it is offered in lists only where products
 * exist (HormigaApp::rebuild_kind_palette). */
inline std::vector<Kind> legacy_kinds() {
    Kind p;
    p.glyph = "product";
    p.title = "Product";
    p.plural = "Products";
    p.icon = "basket";
    p.color = "#3f7d3a";
    p.category = "Store";
    p.subtitle_field = "price";
    p.located = true;
    p.legacy = true;
    p.palette = false;
    p.fields = {{"price", "Price", ""},
                {"unit", "Sold by (each, kg, litre...)", ""},
                {"sku", "SKU or barcode", ""},
                {"stock", "In stock", ""},
                {"notes", "Notes", "multiline:60"},
                {"image_url", "Picture (URL)", ""}};
    return {p};
}

/* The application's own kinds of thing, as they were before kinds were data.
 * Their fields are declared in domain/seed.hpp; this is how they are called,
 * drawn and what they can do. */
inline std::vector<Kind> builtins() {
    auto k = [](const char* g, const char* t, const char* p, const char* icon, const char* col, const char* cat,
                bool loc, bool dated, bool listed, const char* date_f, const char* sub_f) {
        Kind x;
        x.glyph = g, x.title = t, x.plural = p, x.icon = icon, x.color = col, x.category = cat;
        x.located = loc, x.dated = dated, x.listed = listed;
        x.date_field = date_f, x.subtitle_field = sub_f;
        x.builtin = true;
        return x;
    };
    return {
        k("contact", "Contact", "Contacts", "user", "#b3592e", "People", true, false, true, "", "role"),
        k("organization", "Organization", "Organizations", "building", "#8a6d3b", "People", true, false, true, "",
          "kind"),
        k("event", "Event", "Events", "calendar", "#3f6fae", "Events", true, true, false, "date", "date"),
        k("incident", "Incident", "Incidents", "warning", "#c83232", "Events", true, true, false, "date", "date"),
        k("job", "Job", "Jobs", "briefcase", "#5d7d3b", "Events", false, true, false, "deadline", "deadline"),
        k("image", "Image", "Images", "image", "#4e6d8d", "Assets", false, false, false, "", ""),
        k("resource", "Resource", "Resources", "book", "#4e8d85", "Assets", false, false, false, "", "topic"),
    };
}

struct Registry {
    std::vector<Kind> all;
    /* Kinds a database names like one of the APPLICATION's glyphs (a block, the
     * map, a note): never registered and never treated as data, reported so the
     * Kinds window can ask for another name. See `apply`. */
    std::vector<std::string> clashes;

    const Kind* find(const std::string& glyph) const {
        for (const auto& k : all)
            if (k.glyph == glyph) return &k;
        return nullptr;
    }
    std::string title(const std::string& glyph) const {
        const Kind* k = find(glyph);
        return k ? k->title : glyph;
    }
    std::string plural(const std::string& glyph) const {
        const Kind* k = find(glyph);
        return k ? k->plural : glyph;
    }
    bool located(const std::string& glyph) const {
        const Kind* k = find(glyph);
        return k && k->located;
    }
    bool dated(const std::string& glyph) const {
        const Kind* k = find(glyph);
        return k && k->dated && !k->date_field.empty();
    }
    bool listed(const std::string& glyph) const {
        const Kind* k = find(glyph);
        return k && k->listed;
    }
    /* The date that puts a rune of this kind on the calendar ("" = none). */
    std::string date_of(const maiz::SceneNode& n) const {
        const Kind* k = find(n.glyph);
        return k && k->dated && !k->date_field.empty() ? field_value(n, k->date_field.c_str())
                                                       : std::string();
    }
    /* The kinds a database made itself (registered as glyphs at load). */
    std::vector<const Kind*> own() const {
        std::vector<const Kind*> out;
        for (const auto& k : all)
            if (!k.builtin) out.push_back(&k);
        return out;
    }
};

inline bool has_tag(const maiz::SceneNode& n, const char* t) {
    return std::find(n.tags.begin(), n.tags.end(), t) != n.tags.end();
}

/* A `kind` rune, read over `base` (a built-in it renames, or a blank kind). */
inline void read_rune(const maiz::SceneNode& n, Kind& k) {
    auto f = [&](const char* key) { return field_value(n, key); };
    k.defined = true;
    k.palette = f("hidden") != "1"; // a kind the database defines is listed unless it says otherwise
    if (!f("title").empty()) k.title = f("title");
    if (!f("plural").empty()) k.plural = f("plural");
    else if (!f("title").empty()) k.plural = f("title") + "s";
    if (!f("icon").empty()) k.icon = f("icon");
    if (!f("color").empty()) k.color = f("color");
    if (!f("category").empty()) k.category = f("category");
    if (!f("date_field").empty()) k.date_field = f("date_field");
    if (!f("subtitle_field").empty()) k.subtitle_field = f("subtitle_field");
    // traits are tags on the kind rune; a built-in keeps its own unless the rune says otherwise
    if (has_tag(n, "trait:located")) k.located = true;
    if (has_tag(n, "trait:dated")) k.dated = true;
    if (has_tag(n, "trait:listed")) k.listed = true;
    if (has_tag(n, "-trait:located") || has_tag(n, "not:located")) k.located = false;
    const auto j = nlohmann::json::parse(f("fields"), nullptr, false);
    if (j.is_array()) {
        k.fields.clear();
        for (const auto& e : j) {
            if (!e.is_object() || !e.contains("key") || !e["key"].is_string()) continue;
            Field fl;
            fl.key = e["key"].get<std::string>();
            fl.label = e.value("label", fl.key);
            fl.editor = e.value("editor", "");
            if (!fl.key.empty()) k.fields.push_back(fl);
        }
    }
}

/* The registry for a database: the built-ins, then its `kinds` mantle over
 * them (a rune named like a built-in renames it; any other is a new kind). */
inline Registry load(const maiz::Scene& kinds_mantle) {
    Registry r;
    r.all = builtins();
    for (auto& l : legacy_kinds()) r.all.push_back(std::move(l));
    for (const auto& n : kinds_mantle.nodes) {
        if (n.glyph != "kind") continue;
        Kind* k = nullptr;
        for (auto& b : r.all)
            if (b.glyph == n.name) k = &b;
        if (k && k->legacy) { // the database defines it: its definition, not the old one
            *k = Kind{};
            k->glyph = n.name;
            k->title = n.name;
            k->plural = n.name + "s";
        }
        if (!k) {
            Kind fresh;
            fresh.glyph = n.name;
            fresh.title = n.name;
            fresh.plural = n.name + "s";
            r.all.push_back(fresh);
            k = &r.all.back();
        }
        read_rune(n, *k);
    }
    return r;
}

/* The process's registry: rebuilt by whoever projects (the app's reproject,
 * the CLI's load). Read from the one thread that owns the core. */
inline Registry& current() {
    static Registry r = [] {
        Registry b;
        b.all = builtins();
        return b;
    }();
    return r;
}

/* A database's own kind, as Void Core's glyph declaration: its fields, their
 * labels and editors, and (if it is `located`) the location facet with every
 * position channel, as the built-in located kinds carry it. */
inline std::string glyph_json(const Kind& k, const std::vector<std::string>& channel_fields) {
    nlohmann::json j;
    j["glyph"] = k.glyph;
    j["label"] = k.title;
    nlohmann::json fields = nlohmann::json::array(), labels = nlohmann::json::object(),
                   editors = nlohmann::json::object();
    auto add = [&](const std::string& key, const std::string& label, const std::string& editor) {
        for (const auto& f : fields)
            if (f == key) return;
        fields.push_back(key);
        labels[key] = label;
        if (!editor.empty()) editors[key] = editor;
    };
    add("title", "Name", "");
    for (const auto& f : k.fields) add(f.key, f.label, f.editor);
    if (k.located) {
        add("geo", "Location (lat,lon)", "");
        add("ref", "Reference point (fan-out parent)", "");
        add("ref_off", "", "hidden");
        for (const auto& cf : channel_fields)
            add(cf, cf.rfind("geo_cv_", 0) == 0 ? "Location on the canvas '" + cf.substr(7) + "'"
                                                : "Location in layer '" + cf.substr(4) + "'",
                "");
    }
    j["fields"] = fields;
    j["hints"] = {{"color", k.color}, {"face", {{"w", 190}, {"h", 58}}}, {"category", k.category},
                  {"labels", labels}, {"editors", editors}};
    return j.dump();
}

/* ── WHO A DIRECTORY MAY LIST, AND THE LINE UNDER EACH NAME ────────────────
 * Every kind with the trait `listed`; `kind` is one of them, or "" / "both"
 * for all. render/published.hpp asks here for the website, the newsletter and
 * the Builder's preview alike; the consent gates stay there. */
inline bool in_directory(const maiz::SceneNode& dn, const std::string& kind) {
    if (!current().listed(dn.glyph)) return false;
    return kind.empty() || kind == "both" || dn.glyph == kind;
}
/* A contact's role, an organization's abbreviation, or the field a database's
 * own kind shows under its name. NEVER a field the gates guard (`notes`:
 * CLAUDE.md rule 6; `email`, `phone`: the second consent), whatever the kind
 * says, because a kind's subtitle is chosen by whoever made the kind. */
inline std::string directory_line(const maiz::SceneNode& dn) {
    if (dn.glyph == "contact") return field_value(dn, "role");
    if (dn.glyph == "organization") return field_value(dn, "abbreviation");
    const Kind* k = current().find(dn.glyph);
    if (!k || k->subtitle_field.empty()) return {};
    const std::string& f = k->subtitle_field;
    if (f == "notes" || f == "email" || f == "phone") return {};
    return field_value(dn, f.c_str());
}

/* ── WHAT AN EVENT GRID SHOWS (2026-10-06) ────────────────────────────────
 * `kind` "" or "event": events, as always (published unless an Allomone rule
 * hides one). Or one of the database's own DATED kinds (a campaign's sessions,
 * a home's chores) — and then only runes that carry `clearance:public`,
 * because a kind somebody made may hold anything (a store's sales name its
 * customers), and its default is the directory's, not the event's: nothing
 * leaves unless it says so. Its card shows its name, its date and its
 * `directory_line`: never notes, email or phone. */
inline bool in_event_grid(const maiz::SceneNode& dn, const std::string& kind) {
    if (kind.empty() || kind == "event") return dn.glyph == "event";
    const Kind* k = current().find(kind);
    if (!k || k->builtin || dn.glyph != kind || !current().dated(kind)) return false;
    return has_tag(dn, "clearance:public");
}
/* The date an event grid sorts and prints: an event's `date`, or the kind's own. */
inline std::string grid_date(const maiz::SceneNode& dn) {
    const Kind* k = current().find(dn.glyph);
    if (k && !k->builtin) return current().date_of(dn);
    return field_value(dn, "date");
}

/* ── WHAT A PUBLIC MAP MAY SHOW (2026-10-07) ──────────────────────────────
 * The website's map widget publishes placed runes by SUBTRACTION: everything
 * with a position except contacts (the people seam), notes, images, layers and
 * reference points. That was safe while the only placed kinds were
 * organizations, events and incidents. A database's own placed kind may be
 * anything (a home's residents, a store's customers), so it takes the
 * directory's rule instead: on a public map only when it carries
 * `clearance:public`. The built-ins keep the rule they always had. */
/* A database's own kind (not a built-in): what the public outputs treat with
 * the directory's rule (consent, and name, date and line only). */
inline bool is_own(const maiz::SceneNode& dn) {
    const Kind* k = current().find(dn.glyph);
    return k && !k->builtin;
}
/* ...and withheld from every public output until it carries `clearance:public`. */
inline bool withheld(const maiz::SceneNode& dn) { return is_own(dn) && !has_tag(dn, "clearance:public"); }

inline bool on_public_map(const maiz::SceneNode& dn) {
    if (dn.glyph == "contact" || dn.glyph == "map" || dn.glyph == "image" || dn.glyph == "note" ||
        dn.glyph == "refpoint")
        return false;
    const Kind* k = current().find(dn.glyph);
    if (k && !k->builtin) return has_tag(dn, "clearance:public");
    return true;
}

/* ── A BUILT-IN KIND'S FIELDS, AS A DATABASE NAMES THEM (Q109, 2026-10-06) ──
 * The author's lean taken: a database may relabel a built-in kind's fields and
 * give it fields of its own; it never renames a field's KEY, which is what the
 * data stores. So the application remembers each built-in's descriptor as it
 * registered it (seed.hpp calls `remember_builtin`), and `apply` registers it
 * again with the database's labels and extra fields over it. */
inline std::map<std::string, std::string>& builtin_descriptors() {
    static std::map<std::string, std::string> d;
    return d;
}
inline void remember_builtin(const std::string& json) {
    const auto j = nlohmann::json::parse(json, nullptr, false);
    if (j.is_object() && j.contains("glyph") && j["glyph"].is_string())
        builtin_descriptors()[j["glyph"].get<std::string>()] = json;
}
/* A built-in's own fields: key, label, editor, in its order (hidden ones and
 * position channels left out: they are machinery, not what a person names). */
inline std::vector<Field> builtin_fields(const std::string& glyph) {
    std::vector<Field> out;
    auto it = builtin_descriptors().find(glyph);
    if (it == builtin_descriptors().end()) return out;
    const auto j = nlohmann::json::parse(it->second, nullptr, false);
    if (!j.is_object() || !j.contains("fields")) return out;
    const auto hints = j.value("hints", nlohmann::json::object());
    const auto labels = hints.value("labels", nlohmann::json::object());
    const auto editors = hints.value("editors", nlohmann::json::object());
    for (const auto& f : j["fields"]) {
        if (!f.is_string()) continue;
        const std::string key = f.get<std::string>();
        const std::string ed = editors.value(key, "");
        if (ed == "hidden" || key.rfind("geo_", 0) == 0 || key == "ref_off") continue;
        out.push_back({key, labels.value(key, key), ed});
    }
    return out;
}
/* The built-in's descriptor with the database's labels over it and its extra
 * fields after it; "" when the database says nothing about its fields. */
inline std::string merged_builtin(const Kind& k) {
    auto it = builtin_descriptors().find(k.glyph);
    if (it == builtin_descriptors().end() || k.fields.empty()) return {};
    auto j = nlohmann::json::parse(it->second, nullptr, false);
    if (!j.is_object()) return {};
    if (!j.contains("hints") || !j["hints"].is_object()) j["hints"] = nlohmann::json::object();
    auto& hints = j["hints"];
    if (!hints.contains("labels") || !hints["labels"].is_object()) hints["labels"] = nlohmann::json::object();
    if (!hints.contains("editors") || !hints["editors"].is_object()) hints["editors"] = nlohmann::json::object();
    for (const auto& f : k.fields) {
        bool known = false;
        for (const auto& e : j["fields"]) known = known || (e.is_string() && e.get<std::string>() == f.key);
        if (!known) {
            j["fields"].push_back(f.key);
            if (!f.editor.empty()) hints["editors"][f.key] = f.editor;
        }
        if (!f.label.empty()) hints["labels"][f.key] = f.label;
    }
    j["label"] = k.title;
    return j.dump();
}

/* LOAD A DATABASE'S KINDS INTO A CORE: read its `kinds` mantle, register each
 * kind it made as a glyph (so projection keeps their fields), and make the
 * result the current registry. Returns a signature of what was registered, so
 * a caller re-projects only when it changed. The app calls it from reproject,
 * the CLI after loading a document. */
inline std::string apply(maiz::Core& core, const std::vector<std::string>& channel_fields) {
    maiz::Scene ks;
    try {
        maiz::ProjectOptions po;
        po.mantle = kMantle;
        ks = maiz::project_scene(core, po);
    } catch (...) {
    }
    Registry r = load(ks);
    std::string sig;
    /* A DATABASE'S KIND NEVER REPLACES ONE OF THE APPLICATION'S GLYPHS. A kind
     * called "Directory" or "Note" would otherwise register over the document
     * block or the notes, and break them for everyone the database reaches. The
     * Kinds window refuses such a name; this is the guard for a kind rune that
     * arrives some other way (a member's device, an old file, a script). A
     * glyph counts as the application's when the core knows it and this
     * process did not register it as a database's kind. */
    static std::set<std::string> ours;
    for (auto it = r.all.begin(); it != r.all.end();) {
        if (!it->builtin && !ours.count(it->glyph) && core.dispatch("glyphs " + it->glyph).ok) {
            r.clashes.push_back(it->glyph);
            it = r.all.erase(it);
        } else {
            ++it;
        }
    }
    for (const Kind* k : r.own()) {
        const std::string j = glyph_json(*k, channel_fields);
        core.register_glyph(j);
        ours.insert(k->glyph);
        sig += j;
    }
    for (const auto& c : r.clashes) sig += "!" + c;
    // built-ins the database relabelled or extended; and back to the
    // application's own for one it no longer says anything about
    static std::set<std::string> widened;
    std::set<std::string> now;
    for (const auto& k : r.all) {
        if (!k.builtin || !k.defined) continue;
        const std::string j = merged_builtin(k);
        if (j.empty()) continue;
        core.register_glyph(j);
        now.insert(k.glyph);
        sig += j;
    }
    for (const auto& g : widened)
        if (!now.count(g))
            if (auto it = builtin_descriptors().find(g); it != builtin_descriptors().end())
                core.register_glyph(it->second);
    widened = now;
    for (const auto& k : r.all) // renames change labels everywhere, so they are part of it too
        if (k.defined) sig += k.glyph + k.title + k.plural + k.icon + k.color;
    current() = std::move(r);
    return sig;
}

} // namespace hormiga::kinds
