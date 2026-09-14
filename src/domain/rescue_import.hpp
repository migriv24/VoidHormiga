/* rescue_import.hpp — the Supabase-rescue Import holiday's compiler (phase C).
 *
 * Same philosophy as import.hpp: the dump is read ONCE, what lands in the
 * org is COMMANDS — one `compile_commit` batch, one undo frame, replayable
 * forever without the dump. Headless: JSON text in, commands out; the app
 * feeds files from the rescue directory, the test feeds synthetic strings.
 *
 * This is also the Q2 moment — the once-ever schema redesign at import time:
 *   contacts.title            → contact.role
 *   contacts.notes            → contact.notes (internal; render-seam guarded)
 *   contacts.office_phone/work_cell → contact.phone (first non-empty)
 *   contacts.receive_newsletter     → +newsletter:yes tag
 *   contacts.organization / events.organization → member-of / hosted-by EDGES
 *   organizations.description → organization.bio
 *   events: recurring weekly meetings → +recurring tag, days/times as fields
 *   presenters → contact runes (role "presenter") tagged with their month
 * json_store payloads (graph/tags/images/jobs/meta) are the NEXT importer
 * pass — deliberately not here yet.
 */
#pragma once

#include "domain/import.hpp" // detail::slug, detail::quote

#include "json.hpp"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace hormiga {

struct RescueImport {
    std::vector<std::string> commands;
    int contacts = 0, organizations = 0, events = 0, presenters = 0, links = 0;
    std::vector<std::string> notes;
    std::string error; // fatal ("" = ok)
};

namespace rdetail {

inline std::string s(const nlohmann::json& row, const char* key) {
    if (!row.contains(key) || row[key].is_null()) return {};
    if (row[key].is_string()) return detail::trim(row[key].get<std::string>());
    return row[key].dump();
}

/* The old sheet sometimes left header-echo rows behind ("Email" in the email
 * column, empty name). A rune needs a name; skip nameless rows. */
inline bool usable(const nlohmann::json& row, const char* name_key) {
    return !detail::slug(s(row, name_key)).empty();
}

} // namespace rdetail

inline RescueImport compile_rescue_import(
    const std::string& contacts_json, const std::string& organizations_json,
    const std::string& events_json, const std::string& presenters_json,
    const std::function<bool(const std::string&)>& name_taken) {
    using nlohmann::json;
    using namespace rdetail;
    RescueImport out;

    json contacts, orgs, events, presenters;
    try {
        contacts = json::parse(contacts_json.empty() ? "[]" : contacts_json);
        orgs = json::parse(organizations_json.empty() ? "[]" : organizations_json);
        events = json::parse(events_json.empty() ? "[]" : events_json);
        presenters = json::parse(presenters_json.empty() ? "[]" : presenters_json);
    } catch (const std::exception& e) {
        out.error = std::string("bad dump json: ") + e.what();
        return out;
    }

    std::set<std::string> minted;
    auto mint = [&](const std::string& raw, const std::string& fallback) {
        std::string base = detail::slug(raw);
        if (base.empty()) base = fallback;
        std::string name = base;
        for (int i = 2; minted.count(name) || name_taken(name); ++i)
            name = base + "-" + std::to_string(i);
        minted.insert(name);
        return name;
    };
    auto set_if = [&](const std::string& rune, const char* field, const std::string& v) {
        if (!v.empty())
            out.commands.push_back("set " + rune + " " + field + " " + detail::quote(v));
    };

    // organizations first — contacts and events link to them by name.
    // org_map: raw-name slug → the rune actually minted (collisions get a
    // suffix, and the edges must follow the rename)
    std::map<std::string, std::string> org_map;
    for (const auto& row : orgs) {
        if (!usable(row, "name")) continue;
        std::string name = mint(s(row, "name"), "org");
        org_map[detail::slug(s(row, "name"))] = name;
        out.commands.push_back("rune new organization " + name);
        set_if(name, "abbreviation", s(row, "abbreviation"));
        set_if(name, "email", s(row, "contact_email"));
        set_if(name, "url", s(row, "website"));
        set_if(name, "location", s(row, "location"));
        set_if(name, "bio", s(row, "description"));
        set_if(name, "image_url", s(row, "image_url"));
        out.commands.push_back("tag " + name + " +type:org +status:active");
        ++out.organizations;
    }
    auto link_org = [&](const std::string& rune, const std::string& org_raw,
                        const char* relation) {
        std::string org = detail::slug(org_raw);
        if (org.empty()) return;
        auto it = org_map.find(org); // imported org (rename-aware) first,
        if (it != org_map.end()) org = it->second;
        else if (!name_taken(org)) { // then a pre-existing rune of that name
            out.notes.push_back(rune + ": organization \"" + org_raw +
                                "\" not found - field kept, no edge");
            return;
        }
        out.commands.push_back("link " + rune + " " + org + " --relation " + relation);
        ++out.links;
    };

    for (const auto& row : contacts) {
        if (!usable(row, "name")) continue;
        std::string name = mint(s(row, "name"), "contact");
        out.commands.push_back("rune new contact " + name);
        set_if(name, "role", s(row, "title"));
        set_if(name, "email", s(row, "email"));
        std::string phone = s(row, "office_phone");
        if (phone.empty()) phone = s(row, "work_cell");
        set_if(name, "phone", phone);
        set_if(name, "website", s(row, "website"));
        set_if(name, "notes", s(row, "notes")); // internal; seam-guarded
        set_if(name, "image_url", s(row, "image_url"));
        std::string tags = "tag " + name + " +type:contact +status:active";
        if (row.contains("receive_newsletter") && row["receive_newsletter"].is_boolean() &&
            row["receive_newsletter"].get<bool>())
            tags += " +newsletter:yes";
        out.commands.push_back(tags);
        link_org(name, s(row, "organization"), "member-of");
        ++out.contacts;
    }

    for (const auto& row : events) {
        if (!usable(row, "title")) continue;
        std::string name = mint(s(row, "title"), "event");
        out.commands.push_back("rune new event " + name);
        set_if(name, "days", s(row, "days"));
        set_if(name, "start_time", s(row, "start_time"));
        set_if(name, "end_time", s(row, "end_time"));
        set_if(name, "venue", s(row, "location"));
        set_if(name, "virtual", s(row, "virtual_location"));
        set_if(name, "summary", s(row, "description"));
        set_if(name, "email", s(row, "contact_email"));
        set_if(name, "color", s(row, "color"));
        set_if(name, "icon_url", s(row, "icon_url"));
        out.commands.push_back("tag " + name + " +type:event +recurring");
        link_org(name, s(row, "organization"), "hosted-by");
        ++out.events;
    }

    for (const auto& row : presenters) {
        if (!usable(row, "name")) continue;
        std::string name = mint(s(row, "name"), "presenter");
        out.commands.push_back("rune new contact " + name);
        out.commands.push_back("set " + name + " role \"presenter\"");
        set_if(name, "bio", s(row, "description"));
        set_if(name, "website", s(row, "slides_link"));
        std::string tags = "tag " + name + " +type:contact";
        std::string month = detail::lower(s(row, "presentation_month"));
        if (!month.empty()) tags += " +month:" + detail::slug(month);
        out.commands.push_back(tags);
        link_org(name, s(row, "organization"), "member-of");
        ++out.presenters;
    }

    return out;
}

/* compile_json_store_import — the SECOND rescue pass, over json_store's
 * semi-structured payloads (graph/images/jobs/<name>_meta). Phase one
 * (compile_rescue_import) landed the clean tables; this landed whatever the
 * old app kept beside them: freeform connections, the real image library,
 * the jobs board, and per-row tags. Same discipline: one JSON read, commands
 * out, the caller batches with compile_commit for one undo frame.
 *
 * Resolution strategy, stated honestly: json_store's ids don't appear on
 * the runes phase one minted (a gap — recorded, not silently patched), so
 * every lookup here goes through the SAME slug the source name/title would
 * have produced (`detail::slug`) and reports a note on miss rather than
 * guessing. This matches real data except in the rare case a phase-one
 * collision suffix (-2) was needed; that case surfaces as a note, not a
 * silent drop.
 */
struct JsonStoreImport {
    std::vector<std::string> commands;
    int images = 0, jobs = 0, graph_edges = 0, meta_tags = 0;
    std::vector<std::string> notes;
    std::string error;
};

namespace rdetail {

/* Known drift in the rescued tag vocabulary (found via the org's own `tags`
 * json_store entry) — the temper concept made concrete: one idempotent rule
 * per known variant, applied at the one seam tags enter the system. */
inline std::string normalize_tag(std::string t) {
    t = detail::lower(detail::trim(t));
    if (t == "reoccuring") return "recurring";
    if (t == "reoccurring") return "recurring";
    return t;
}

} // namespace rdetail

inline JsonStoreImport compile_json_store_import(
    const std::string& graph_json, const std::string& images_json,
    const std::string& jobs_json, const std::string& contacts_meta_json,
    const std::string& events_meta_json, const std::string& presenters_meta_json,
    const std::string& contacts_json, const std::string& events_json,
    const std::string& presenters_json,
    /* Rune NAMES are unique across the whole mantle, not per glyph — this
     * must be glyph-agnostic or minting collides with an existing rune of a
     * DIFFERENT glyph and the whole batch rolls back (found against the
     * real dump: "Oakshire Inspires Benefit" existed as an event AND as an
     * image name). */
    const std::function<bool(const std::string& /*name*/)>& name_taken,
    /* For RESOLUTION (does a referenced rune exist, and is it the right
     * kind) — "" if no such rune, else its glyph. */
    const std::function<std::string(const std::string& /*name*/)>& glyph_of) {
    using nlohmann::json;
    using namespace rdetail;
    JsonStoreImport out;

    json graph, images, jobs, cmeta, emeta, pmeta, contacts, events, presenters;
    try {
        graph = json::parse(graph_json.empty() ? "{}" : graph_json);
        auto unwrap = [](const std::string& j, const char* key) {
            json v = json::parse(j.empty() ? "{}" : j);
            return v.contains(key) ? v[key] : json::array();
        };
        images = unwrap(images_json, "images");
        jobs = unwrap(jobs_json, "jobs");
        cmeta = json::parse(contacts_meta_json.empty() ? "{}" : contacts_meta_json);
        emeta = json::parse(events_meta_json.empty() ? "{}" : events_meta_json);
        pmeta = json::parse(presenters_meta_json.empty() ? "{}" : presenters_meta_json);
        contacts = json::parse(contacts_json.empty() ? "[]" : contacts_json);
        events = json::parse(events_json.empty() ? "[]" : events_json);
        presenters = json::parse(presenters_json.empty() ? "[]" : presenters_json);
    } catch (const std::exception& e) {
        out.error = std::string("bad json_store payload: ") + e.what();
        return out;
    }

    // id → slug, for every row kind that meta stores or graph edges key by
    std::map<std::string, std::string> contact_slug, event_slug, presenter_slug;
    for (const auto& r : contacts)
        if (r.contains("id"))
            contact_slug[nlohmann::to_string(r["id"])] = detail::slug(s(r, "name"));
    for (const auto& r : events)
        if (r.contains("id"))
            event_slug[nlohmann::to_string(r["id"])] = detail::slug(s(r, "title"));
    for (const auto& r : presenters)
        if (r.contains("id"))
            presenter_slug[nlohmann::to_string(r["id"])] = detail::slug(s(r, "name"));

    // ── graph: only "connected_to" (member_of duplicates phase one) ────────
    std::unordered_map<std::string, std::string> label_glyph; // graph node id → glyph
    std::unordered_map<std::string, std::string> label_slug;  // graph node id → slug
    if (graph.contains("nodes"))
        for (auto it = graph["nodes"].begin(); it != graph["nodes"].end(); ++it) {
            std::string type = s(it.value(), "type");
            std::string glyph = (type == "org") ? "organization" : "contact";
            label_glyph[it.key()] = glyph;
            label_slug[it.key()] = detail::slug(s(it.value(), "label"));
        }
    if (graph.contains("edges"))
        for (const auto& e : graph["edges"]) {
            if (s(e, "relation") != "connected_to") continue;
            std::string from_id = s(e, "from_id"), to_id = s(e, "to_id");
            if (!label_slug.count(from_id) || !label_slug.count(to_id)) continue;
            std::string a = label_slug[from_id], b = label_slug[to_id];
            std::string ga = label_glyph[from_id], gb = label_glyph[to_id];
            if (glyph_of(a) != ga) {
                out.notes.push_back("connection: \"" + a + "\" not found - skipped");
                continue;
            }
            if (glyph_of(b) != gb) {
                out.notes.push_back("connection: \"" + b + "\" not found - skipped");
                continue;
            }
            out.commands.push_back("link " + a + " " + b + " --relation connected-to");
            ++out.graph_edges;
        }

    // ── the image library ────────────────────────────────────────────────
    // mint() collides on NAME alone (glyph-agnostic) — matching how the core
    // actually enforces uniqueness within a mantle
    std::set<std::string> minted;
    auto mint = [&](const std::string& raw, const std::string& fallback) {
        std::string base = detail::slug(raw);
        if (base.empty()) base = fallback;
        std::string name = base;
        for (int i = 2; minted.count(name) || name_taken(name); ++i)
            name = base + "-" + std::to_string(i);
        minted.insert(name);
        return name;
    };
    for (const auto& row : images) {
        std::string idfrag = s(row, "id");
        if (idfrag.size() > 8) idfrag = idfrag.substr(0, 8);
        std::string name = mint(s(row, "name"), "image-" + idfrag);
        out.commands.push_back("rune new image " + name);
        std::string url = s(row, "url");
        if (!url.empty())
            out.commands.push_back("set " + name + " url " + detail::quote(url));
        std::string alt = s(row, "alt");
        if (!alt.empty())
            out.commands.push_back("set " + name + " alt " + detail::quote(alt));
        std::string desc = s(row, "description");
        if (!desc.empty())
            out.commands.push_back("set " + name + " description " +
                                   detail::quote(desc));
        std::string tags = "tag " + name + " +type:image";
        if (row.contains("tags"))
            for (const auto& t : row["tags"]) {
                std::string nt = normalize_tag(t.get<std::string>());
                if (!nt.empty()) tags += " +" + nt;
            }
        out.commands.push_back(tags);
        if (row.contains("event_ids"))
            for (const auto& eid : row["event_ids"]) {
                std::string key = eid.is_string() ? eid.get<std::string>()
                                                  : nlohmann::to_string(eid);
                auto it = event_slug.find(key);
                if (it != event_slug.end() && glyph_of(it->second) == "event") {
                    out.commands.push_back("link " + name + " " + it->second +
                                           " --relation flyer-of");
                }
            }
        ++out.images;
    }

    // ── the jobs board ───────────────────────────────────────────────────
    for (const auto& row : jobs) {
        std::string idfrag = s(row, "id");
        if (idfrag.size() > 8) idfrag = idfrag.substr(0, 8);
        std::string name = mint(s(row, "title"), "job-" + idfrag);
        out.commands.push_back("rune new job " + name);
        auto set_if = [&](const char* field, const std::string& v) {
            if (!v.empty())
                out.commands.push_back("set " + name + " " + field + " " +
                                       detail::quote(v));
        };
        set_if("org", s(row, "org"));
        set_if("pay", s(row, "pay"));
        set_if("job_type", s(row, "job_type"));
        set_if("location", s(row, "location"));
        set_if("description", s(row, "description"));
        set_if("availability", s(row, "availability"));
        set_if("deadline", s(row, "close_date"));
        set_if("contact_name", s(row, "contact_name"));
        set_if("contact_email", s(row, "contact_email"));
        set_if("contact_phone", s(row, "contact_phone"));
        std::string tags = "tag " + name + " +type:job";
        bool active = row.contains("active") && row["active"].is_boolean() &&
                     row["active"].get<bool>();
        tags += active ? " +status:active" : " +status:inactive";
        if (row.contains("tags"))
            for (const auto& t : row["tags"]) {
                std::string nt = normalize_tag(t.get<std::string>());
                if (!nt.empty()) tags += " +" + nt;
            }
        out.commands.push_back(tags);
        std::string org_slug = detail::slug(s(row, "org"));
        if (!org_slug.empty() && glyph_of(org_slug) == "organization")
            out.commands.push_back("link " + name + " " + org_slug +
                                   " --relation posted-by");
        ++out.jobs;
    }

    // ── per-row meta: tags onto already-imported runes ──────────────────
    auto apply_meta = [&](const json& meta, const std::map<std::string, std::string>& slugmap,
                          const char* glyph) {
        for (auto it = meta.begin(); it != meta.end(); ++it) {
            auto sit = slugmap.find(it.key());
            if (sit == slugmap.end()) continue;
            if (glyph_of(sit->second) != std::string(glyph)) {
                out.notes.push_back(std::string(glyph) + " id " + it.key() +
                                    " (\"" + sit->second + "\") not found for meta tags");
                continue;
            }
            if (!it.value().contains("tags") || it.value()["tags"].empty()) continue;
            std::string tags = "tag " + sit->second;
            for (const auto& t : it.value()["tags"]) {
                std::string nt = normalize_tag(t.get<std::string>());
                if (!nt.empty()) tags += " +" + nt;
            }
            out.commands.push_back(tags);
            ++out.meta_tags;
        }
    };
    apply_meta(cmeta, contact_slug, "contact");
    apply_meta(emeta, event_slug, "event");
    apply_meta(pmeta, presenter_slug, "contact"); // presenters landed as contacts

    return out;
}

} // namespace hormiga
