/* seed.hpp — Hormiga's glyph set (v0) and the demo org, shared by the app and
 * the replay smoke test.
 *
 * The glyphs are host config, registered per session (never exported state).
 * The demo org is a TRANSCRIPT — a vector of dispatcher commands — because
 * that is the founding commitment made testable: the same lines that build it
 * in the app rebuild it headless, and the projected scenes must match.
 *
 * Everything here is fictional. Real org data never lives in this repo
 * (CLAUDE.md ground rule 2); it arrives in phase C through import holidays.
 */
#pragma once

/* The block and antfarm glyph families live beside this file since
 * 2026-08-20. Included here so every existing `#include "domain/seed.hpp"`
 * keeps working: a split should not make its callers do anything. */
#include "domain/glyphs_blocks.hpp"
#include "domain/glyphs_antfarm.hpp"

#include "domain/civic.hpp"            // the civic record owns its own glyphs too
#include "domain/hormiga_allomone.hpp" // the domain Allomone owns its own glyphs

#include "voidmaiz/embed.hpp"

#include "json.hpp"

#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace hormiga {

/* The data glyphs (v0 shapes — the light redesign lands with phase C's
 * import; see okf/concepts/foundation/data-model.md). `notes` on contact is the
 * internal-notes field the render seam will guard (security concept §3).
 *
 * `channel_geo_fields` carries the per-view position fields ("geo_<channel>")
 * of every UNLOCKED view. Projection only fills fields the glyph DECLARES
 * (../VoidMaiz/src/project/project.cpp fill_fields), so channel positions
 * must be registered onto the placeable glyphs or the canvas never sees
 * them — reproject() keeps this in sync as views unlock and lock. */
/* THE RUNE KIND (`"kind"`, Void Core 0.2.14 section 3.3): `entity` | `act` |
 * `measure`, defaulting to `entity`. Hormiga is almost entirely entities and
 * that default is left alone rather than typed out everywhere -- five glyphs
 * carry `"kind":"act"` and nothing else does.
 *
 * The five are the ones whose rune records a HAPPENING whose subject is some
 * other rune: `statement` (a council member said this, at this offset, in that
 * meeting), `revision` (a policy changed), `submission` (a stranger sent
 * something in), `deployment` (a site was published), `incident` (something
 * occurred). Each is already reified for the arity reason Q64 turned on -- the
 * sentence they express is ternary and an edge label is binary -- so the
 * annotation names what the model already does rather than proposing anything.
 *
 * `event` is deliberately NOT one of them. An event in Hormiga is the thing a
 * person attends -- a venue, times, a flier, a page on the website -- and its
 * fields are read by renderers, not filled as the roles of a verb. Marking it
 * `act` would be reading the English word rather than the model.
 *
 * `measure` is unused here on purpose: Q65's answer is that Hormiga's numbers
 * are POINTS (dates, coordinates, grid columns), and a point has no magnitude,
 * so there is nothing for a weight to be. */
inline void register_glyphs(maiz::Core& core,
                            const std::vector<std::string>& channel_geo_fields = {}) {
    // splice the channel fields into a geo-bearing glyph's declaration
    auto with_channels = [&](const char* json) {
        if (channel_geo_fields.empty()) return std::string(json);
        auto j = nlohmann::json::parse(json);
        for (const auto& f : channel_geo_fields) {
            j["fields"].push_back(f);
            j["hints"]["labels"][f] =
                "Location in view '" + f.substr(4) + "'"; // strip "geo_"
        }
        return j.dump();
    };
    // `hints.editors` binds fields to widget-protocol editors (one binding
    // lights up the inspector, forms, faces, and the coming table cells);
    // `hints.category` groups the add palette. Both are upstream conventions
    // (MESSAGE_FOR_VOIDHORMIGA 2026-07-16 §1/§3).
    // field shapes = the Q2 light redesign, sized to what the rescue import
    // actually carries (src/rescue_import.hpp is the once-ever mapping)
    // `geo` is the Territory facet ("lat,lon" — map coordinates, not assumed
    // Earth); set by map actions / drag-to-place, queried by `effect query near`
    core.register_glyph(with_channels(
        R"({"glyph":"contact","label":"Contact",)"
        /* `bio_en`/`bio_es` joined `bio` on 2026-09-03. A bilingual site's
         * directory published one language of prose whatever page it was on,
         * which made the organization's OWN CONTENT the one text that could not
         * be translated -- the author's `site.languages` commitment applied one
         * layer down (portfolio report A5). `bio` is still read, last, so no
         * existing database loses a word. */
        R"("fields":["display_name","avatar","role","email","phone","website",)"
        R"("bio_en","bio_es","bio","notes","image_url","geo","ref","ref_off"],)"
        R"("hints":{"color":"#b3592e","face":{"w":190,"h":58},"category":"People",)"
        R"("editors":{"avatar":"image","role":"enum","bio_en":"multiline:70",)"
        R"("bio_es":"multiline:70","bio":"multiline:70","notes":"multiline:70",)"
        R"("ref_off":"hidden","ext_uid":"hidden"},)"
        R"("labels":{"avatar":"Photo","role":"Role","email":"Email","phone":"Phone","website":"Website",)"
        R"__("display_name":"Name as printed (blank = the rune name)",)__"
        R"__("bio_en":"Public bio (English)","bio_es":"Biografia publica (espanol)",)__"
        R"__("bio":"Public bio (legacy, used when neither language is set)",)__"
        R"__("image_url":"Photo URL (legacy)",)__"
        R"__("geo":"Location (lat,lon)","ref":"Reference point (fan-out parent)",)__"
        R"__("notes":"Internal notes (never exported)"}}})__"));
    core.register_glyph(with_channels(
        R"({"glyph":"organization","label":"Organization",)"
        R"("fields":["display_name","avatar","abbreviation","kind","email","url",)"
        R"("location","bio_en","bio_es","bio","image_url","geo","ref","ref_off"],)"
        R"("hints":{"color":"#8a6d3b","face":{"w":200,"h":52},"category":"People",)"
        R"("editors":{"avatar":"image","bio_en":"multiline:70",)"
        R"("bio_es":"multiline:70","bio":"multiline:70","ref_off":"hidden"},)"
        R"("labels":{"avatar":"Logo / photo","abbreviation":"Abbreviation","email":"Contact email",)"
        R"__("display_name":"Name as printed (blank = the rune name)",)__"
        R"__("kind":"Kind (e.g. community center, coalition)",)__"
        R"__("url":"Website","location":"Location (text)",)__"
        R"__("bio_en":"Description (public, English)",)__"
        R"__("bio_es":"Descripcion (publica, espanol)",)__"
        R"__("bio":"Description (legacy, used when neither language is set)",)__"
        R"__("geo":"Location (lat,lon)","ref":"Reference point (fan-out parent)",)__"
        R"__("image_url":"Logo URL (legacy)"}}})__"));
    /* ── WHO A VISITOR IS, AND WHAT THEY ARE ASKING FOR (2026-08-19) ─────────
     *
     * The website grows logins, profiles and visitor-submitted events. The
     * question that decides the whole design is where that data lives, and the
     * answer is the one ground rule 3 already gives: **the model lives in Void
     * Core and the dispatcher is the only door.** A backend that could write
     * into the org's database directly would be a second door, and every
     * property this project rests on — replay, undo, attribution, one reviewable
     * batch — is a property of going through the first one.
     *
     * So a visitor does not write. A visitor PROPOSES, and a person accepts.
     *
     * THIS IS THE THIRD TIME THAT SHAPE HAS ARRIVED, which is the argument that
     * it is the right one rather than a convenience:
     *   - Allomone's Weaver proposes rules; it never writes them.
     *   - Void Reyna proposes a dataset as a command transcript; Hormiga
     *     confirms, because the confirmation must be a logged command and so
     *     must happen in the consumer.
     *   - a website visitor proposes an edit; an admin confirms.
     * Same seam, three sources, and the `submission` glyph is that seam reified.
     *
     * `transcript` IS A COMMAND TRANSCRIPT — the same text `import_reyna_transcript`
     * already accepts, and it is filtered by the same rule: only model-building
     * verbs pass, so `use`, `config`, `deploy`, `effect` and `script` are refused.
     * A stranger's profile edit structurally cannot become an effect. That
     * filter was written for a harvested PDF and it turns out to be exactly the
     * check a hostile submission needs, which is what a real seam looks like.
     *
     * THERE IS NO `kind` FIELD, deliberately, and for the reason the civic
     * record refused one: we do not classify types of change. Whether a
     * submission is "claiming a contact" or "proposing an event" is READ OFF
     * the commands it carries — a derived view, computed for the reviewer, never
     * stored and never trusted as a summary of what the transcript actually does.
     *
     * `actor` is the identity the sign-in provider vouched for (`google:sub`),
     * NOT a person. Which contact that identity is, is a link in this database
     * and nowhere else — so the provider can be swapped and the relationships
     * survive it, which is `antfarm.md`'s disposability rule applied to login. */
    core.register_glyph(
        R"({"glyph":"submission","label":"Submission","kind":"act",)"
        R"("fields":["actor","actor_email","transcript","received","state",)"
        R"("decided","note","evidence"],)"
        R"("hints":{"color":"#8a6d3b","face":{"w":230,"h":72},"category":"Inbox",)"
        R"("editors":{"transcript":"multiline:70","note":"multiline:60",)"
        R"("received":"date","decided":"date",)"
        R"("state":"combo:pending,approved,rejected"},)"
        R"__("labels":{"actor":"Signed-in identity (provider:subject)",)__"
        R"__("actor_email":"Email the provider vouched for",)__"
        R"__("transcript":"Proposed commands (model-building verbs only)",)__"
        R"__("received":"Received","state":"State","decided":"Decided on",)__"
        R"__("note":"Reviewer note","evidence":"Uploaded file / URL"}}})__");

    core.register_glyph(with_channels(
        R"({"glyph":"event","label":"Event",)"
        R"("fields":["title_en","title_es","date","days","start_time","end_time",)"
        R"("venue","virtual","summary_en","summary_es",)"
        R"("summary","email","color","icon_url","geo","ref","ref_off",)"
        R"("ext_uid","rrule"],)"
        R"("hints":{"color":"#3f6fae","face":{"w":210,"h":64},"category":"Events",)"
        R"("editors":{"date":"date","summary":"multiline:60",)"
        R"("summary_en":"multiline:60","summary_es":"multiline:60",)"
        R"("ref_off":"hidden"},)"
        R"__("labels":{"title_en":"Title (English; blank = the rune name)",)__"
        R"__("title_es":"Titulo (espanol)","summary_en":"Summary (English)",)__"
        R"__("summary_es":"Resumen (espanol)",)__"
        R"__("date":"Date (one-off)","days":"Days (recurring)",)__"
        R"("start_time":"Starts","end_time":"Ends","venue":"Venue",)"
        R"__("virtual":"Virtual link","summary":"Summary","email":"Contact email",)__"
        R"__("geo":"Location (lat,lon)","ref":"Reference point (fan-out parent)",)__"
        R"__("color":"Card color","icon_url":"Icon URL (legacy)",)__"
        R"__("ext_uid":"Imported UID (matches this to its source calendar)",)__"
        R"__("rrule":"Recurrence rule, as the source calendar wrote it"}}})__"));
    /* job fields sized to the rescue's real jobs-board shape (json_store.jobs).
     *
     * ── `title_en`/`title_es`, ADDED 2026-08-20 ──────────────────────────────
     *
     * `display_name` reached `contact` and `organization`; `title_en` reached
     * `event`. `job` got neither, so its card heading was `humanize(name)` and
     * the site printed:
     *
     *     sass-survivor-access-coordinator → "Sass Survivor Access Coordinator"
     *
     * SASS is Sexual Assault Support Services. A rune NAME is a command-safe
     * slug: it is allowed to be an acronym flattened to lowercase, allowed to
     * carry a typo, allowed to carry an internal suffix. Published prose is
     * allowed none of those, which is the entire reason `title_en` exists — and
     * a job posting is prose in two languages exactly as much as an event is.
     *
     * This is the failure class `tools/lint_glyph_fields.py` was built for, one
     * level up: not "a declared field nothing renders", but "a thing every
     * other published glyph declares and this one does not". */
    core.register_glyph(
        R"({"glyph":"job","label":"Job",)"
        R"("fields":["title_en","title_es","org","pay","job_type","location",)"
        R"("description",)"
        R"("availability","deadline","contact_name","contact_email","contact_phone"],)"
        R"("hints":{"color":"#5d7d3b","face":{"w":190,"h":52},"category":"Events",)"
        R"("editors":{"deadline":"date","description":"multiline:70",)"
        R"("job_type":"combo:full-time,part-time,contract,internship,volunteer",)"
        R"("availability":"combo:open,closed"},)"
        R"__("labels":{"title_en":"Title (English)","title_es":"Title (Spanish)",)__"
        R"("org":"Organization","pay":"Pay","job_type":"Type",)"
        R"("location":"Location","description":"Description",)"
        R"("availability":"Availability","deadline":"Closes",)"
        R"__("contact_name":"Contact name","contact_email":"Contact email",)__"
        R"__("contact_phone":"Contact phone"}}})__");
    core.register_glyph(
        R"({"glyph":"image","label":"Image",)"
        /* `alt` and `description` are prose an organization wrote, and a
         * caption under a flier on the Spanish page is exactly the text a
         * Spanish reader needs (portfolio report A5). Legacy fields last. */
        R"("fields":["path","alt_en","alt_es","alt","url","hosted_by",)"
        R"("description_en","description_es","description"],)"
        R"("hints":{"color":"#7d5bb0","face":{"w":180,"h":118},"category":"Assets",)"
        R"("editors":{"path":"image","description_en":"multiline:60",)"
        R"("description_es":"multiline:60","description":"multiline:60"},)"
        R"__("labels":{"path":"Image file",)__"
        R"__("alt_en":"Alt text, English (for accessibility)",)__"
        R"__("alt_es":"Texto alternativo, espanol",)__"
        R"__("alt":"Alt text (legacy, used when neither language is set)",)__"
        R"__("url":"Link on the internet (set by Host it online, or by hand)",)__"
        R"__("hosted_by":"The Antfarm node that put it online",)__"
        R"__("description_en":"Description (English)",)__"
        R"__("description_es":"Descripcion (espanol)",)__"
        R"__("description":"Description (legacy)"}}})__");
    core.register_glyph(
        R"({"glyph":"resource","label":"Resource","fields":["path","topic"],)"
        R"("hints":{"color":"#4e8d85","face":{"w":190,"h":52},"category":"Assets",)"
        R"("editors":{"path":"path"},)"
        R"("labels":{"path":"File","topic":"Topic"}}})");
    core.register_glyph(
        R"({"glyph":"note","label":"Note","fields":["text"],)"
        R"("hints":{"color":"#6f6f78","face":{"w":240,"h":120},"category":"Notes",)"
        R"("editors":{"text":"multiline:110"},"labels":{"text":"Text"}}})");
    // incidents are NOT events (author, 2026-07-22): dated occurrences —
    // road closures, ICE activity, emergencies — that belong on the map and,
    // when dated, on the calendar as a SEPARATE 'incident' entry, never mixed
    // with planned events. type:incident keeps the query surfaces apart.
    core.register_glyph(with_channels(
        R"({"glyph":"incident","label":"Incident","kind":"act",)"
        R"("fields":["date","time","severity","description","geo","ref",)"
        R"("ref_off","ext_uid"],)"
        R"("hints":{"color":"#a83232","face":{"w":200,"h":60},"category":"Events",)"
        R"("editors":{"date":"date","description":"multiline:70",)"
        R"("severity":"combo:low,medium,high,critical","ref_off":"hidden",)"
        R"("ext_uid":"hidden"},)"
        R"("labels":{"date":"Date","time":"Time","severity":"Severity",)"
        R"__("description":"Description","geo":"Location (lat,lon)",)__"
        R"__("ref":"Reference point (fan-out parent)",)__"
        R"__("ext_uid":"Imported UID (matches this to its source calendar)"}}})__"));
    // a DAY is a taggable rune keyed by date (author, 2026-08-04): tag a
    // calendar day WITHOUT scheduling anything on it. Named `day-YYYY-MM-DD`
    // so it is findable from a date; carries only `date` + `name` fields and
    // its tags. Flows into Allomone as an ordinary thing (glyph `day`), which
    // is the whole point — a cheap surface to test scripts against dates.
    core.register_glyph(
        R"({"glyph":"day","label":"Day",)"
        R"("fields":["date","name"],)"
        R"("hints":{"color":"#8a7a2f","face":{"w":150,"h":40},"category":"Events",)"
        R"("editors":{"date":"date"},)"
        R"__("labels":{"date":"Date","name":"Label"}}})__");
    // a MAP is itself a rune: one database, many maps (author, 2026-07-22).
    // source names the base ("osm" = Earth tiles; an image path = any world);
    // center/zoom are the map's home viewport (the live camera is view-state).
    // a VIEW of the map (UX name: "view"; glyph name kept for continuity).
    // channel = the position channel it reads/writes: views sharing a channel
    // are LOCKED together by construction; "main" is the default channel and
    // every other channel falls back to it (copy-on-write divergence).
    // display fields (map config, 2026-07-22): label_scale/show_labels size and
    // toggle marker labels; layer_* adjust this view AS a ghosted layer under
    // another (opacity/brightness). All are ordinary fields so they log, undo,
    // ride the org, and shape the PNG export — but they must be DECLARED here or
    // projection drops them (the same seam that bit position channels).
    core.register_glyph(
        R"({"glyph":"map","label":"View",)"
        R"("fields":["source","center","zoom","rules","channel","visible",)"
        R"("label_scale","show_labels","layer_opacity","layer_brightness",)"
        R"("no_overlap","label_color"],)"
        R"("hints":{"color":"#2e6b4f","face":{"w":200,"h":52},"category":"Territory",)"
        R"__("labels":{"source":"Base source (legacy; see Settings)",)__"
        R"__("center":"Home center (lat,lon)","zoom":"Home zoom",)__"
        R"__("rules":"Style rules (managed in the Map panel)",)__"
        R"__("channel":"Position channel (main = locked to main)",)__"
        R"__("visible":"Layer visibility (1 = shown)",)__"
        R"__("label_scale":"Label size","show_labels":"Show labels (1)",)__"
        R"__("layer_opacity":"Layer opacity","layer_brightness":"Layer brightness",)__"
        R"__("no_overlap":"Labels dodge each other (1)",)__"
        R"__("label_color":"Label color name"}}})__");
    // a CALENDAR VIEW (C4d, author 2026-07-23): a saved, named calendar — its
    // FILTER (kind + tag query), default granularity, and (future) its own
    // rules. The filter makes privacy publishable: a "Public Events" view can
    // exclude incidents by construction, not by session state. Lives with the
    // data (like map views) so the calendar, which reads the data mantle,
    // finds it.
    core.register_glyph(
        R"({"glyph":"calview","label":"Calendar view",)"
        R"("fields":["title","filter","kind","mode","rules"],)"
        R"("hints":{"color":"#b3592e","face":{"w":200,"h":52},"category":"Territory",)"
        R"("editors":{"kind":"combo:all,events,incidents",)"
        R"("mode":"combo:month,week,3day,agenda","rules":"hidden"},)"
        R"__("labels":{"title":"Name","filter":"Tag filter (query)",)__"
        R"__("kind":"Kinds shown","mode":"Default view",)__"
        R"__("rules":"Style rules (shared with the map for now)"}}})__");
    // a MAP SHAPE (author 2026-07-24): a drawn rectangle or ellipse over the
    // map — an annotation that is a RUNE (taggable, colored by the same rules
    // engine as markers). geo1/geo2 are the bounding-box corners (lat,lon),
    // set by the draw tool. A shape can BESTOW a tag on entities inside it
    // (the "spatial tag" — derive-then-materialize; see okf/concepts/sections/territory.md).
    core.register_glyph(
        R"({"glyph":"mapshape","label":"Map shape",)"
        R"("fields":["kind","geo1","geo2","label","bestows"],)"
        R"("hints":{"color":"#2e6b4f","face":{"w":190,"h":52},"category":"Territory",)"
        R"("editors":{"kind":"combo:rect,ellipse","geo1":"hidden","geo2":"hidden"},)"
        R"__("labels":{"kind":"Shape","geo1":"Corner 1 (lat,lon)",)__"
        R"__("geo2":"Corner 2 (lat,lon)","label":"Label",)__"
        R"__("bestows":"Bestows tag (on entities inside)"}}})__");
    // a REFERENCE POINT (author 2026-07-24): an editor-only gizmo marking a
    // location; NEVER drawn in exports/webview. Entities whose `ref` field
    // names it are its CHILDREN — fanned out around the shared point on the
    // generated map instead of overlapping. A clean answer to "10 orgs at the
    // same building."
    core.register_glyph(
        R"({"glyph":"refpoint","label":"Reference point",)"
        R"("fields":["geo","label"],)"
        R"("hints":{"color":"#8a6d3b","face":{"w":180,"h":44},"category":"Territory",)"
        R"__("labels":{"geo":"Location (lat,lon)","label":"Name"}}})__");
    // ── ALLOMONE (okf/concepts/allomone/): a rule rune. Phase A is derive-only:
    // a tag CONDITION (`cond` = compiled tag-grammar; `condjson` = the editable
    // FilterTerm structure) → an appearance ACTION (`color` = the card color).
    // `enabled` gates it. All fields are edited in the Allomone tab, not the
    // generic inspector, so they are editor:"hidden". ────────────────────────
    core.register_glyph(
        R"({"glyph":"rule","label":"Rule",)"
        R"("fields":["label","cond","condjson","color","enabled"],)"
        R"("hints":{"color":"#7a5cc0","face":{"w":200,"h":48},"category":"Allomone",)"
        R"("editors":{"cond":"hidden","condjson":"hidden","color":"hidden",)"
        R"__("enabled":"hidden"},"labels":{"label":"Rule name"}}})__");

    // ── ALLOMONE BLOCKS (the custom stack editor — okf/concepts/allomone/
    // block-editor.md). Plain DATA runes: glyph = block type, fields = the
    // block's arguments + the AST-tree structure (`parent`/`slot`/`order`). NO
    // shape:block/ports — the node canvas is gone; a custom view
    // (draw_allomone_stack) renders these. This rune tree IS the AST that
    // Allomone Script (text) also projects. ──────────────────────────────────
    core.register_glyph(
        R"({"glyph":"allo_when","label":"When a thing matches",)"
        R"("fields":["order"],)"
        R"("hints":{"color":"#7a5cc0","category":"Allomone",)"
        R"("editors":{"order":"hidden"}}})");
    core.register_glyph(
        R"({"glyph":"allo_hastag","label":"has tag",)"
        R"("fields":["tag","parent","slot","order"],)"
        R"("hints":{"color":"#3f8f6f","category":"Allomone",)"
        R"("editors":{"parent":"hidden","slot":"hidden","order":"hidden"},)"
        R"("labels":{"tag":"Tag"}}})");
    core.register_glyph(
        R"({"glyph":"allo_setcolor","label":"set card color",)"
        R"("fields":["color","parent","slot","order"],)"
        R"("hints":{"color":"#c06a2f","category":"Allomone",)"
        R"("editors":{"color":"color","parent":"hidden","slot":"hidden","order":"hidden"},)"
        R"("labels":{"color":"Color"}}})");

    // ── ALLOMONE. Two glyphs, because there are two dialects and telling them
    // apart by GLYPH rather than by a flag means a stored script cannot change
    // meaning under a migration.
    //
    // `script` is the FROZEN legacy dialect (src/allomone_legacy.hpp): a text
    // document in the imperative Hormiga-local language, kept because twelve
    // seeded examples and any user script in an existing database are written
    // in it. `allo-script` is the Void Maiz language we adopted 2026-08-10 and
    // is where new rules go; its descriptor lives in the domain library, next
    // to the code that reads it, along with the resolution glyph upstream's
    // own commands name. ─────────────────────────────────────────────────────
    core.register_glyph(
        R"__({"glyph":"script","label":"Allomone Script (legacy)",)__"
        R"("fields":["name","body","enabled"],)"
        R"("hints":{"color":"#6b5a8a","category":"Allomone",)"
        R"("editors":{"body":"hidden","enabled":"hidden"},"labels":{"name":"Name"}}})");
    allomone::register_glyphs(core);
    // ── THE CIVIC RECORD (okf/concepts/projects/civic-record.md): policy / provision /
    // revision / term / statement. Their descriptors live in src/civic.hpp,
    // next to the resolution logic that reads them — the same rule the Allomone
    // glyphs follow, and the reason neither schema is written twice.
    civic::register_glyphs(core);
}

/* THE GLYPH'S DECLARED FIELDS, ASKED OF VOID CORE (0.2.14, their ask 1).
 *
 * This function used to be a hand-written table -- a second copy of every
 * `"fields":[...]` in `register_glyphs` above, kept in step by nobody. It was
 * already wrong when it was deleted, and the way it was wrong is the argument:
 * `job` appeared TWICE in it, so the first arm won and answered `{org,
 * deadline, url}` for a glyph that declares twelve fields and no `url` at all.
 * A CSV of jobs therefore mapped two real columns, wrote a `url` field nothing
 * declares, and told the importer that `pay`, `location`, `description` and six
 * others were "no such job field" -- a wrong answer delivered with a
 * diagnostic. Nothing could have caught it, because a duplicate of a
 * declaration never disagrees with the declaration; it disagrees with the
 * other duplicate, and only a reader who happened to scroll past both would
 * ever see the two.
 *
 * Void Core 0.2.14 makes the descriptor a documented host contract: `glyphs
 * <name>` returns it as `data` with `fields`, `kind` and `source` always
 * present, resolved across the declared and registered registries. So the
 * schema is read from the one place that holds it, and a glyph an ORGANIZATION
 * declared (`glyph declare`, travelling in `state.glyphs`) answers here exactly
 * like one we registered -- which is the property that unblocks Q59.
 *
 * Order is the declaration's order, because an importer maps columns by it. */
inline std::vector<std::string> glyph_fields(maiz::Core& core,
                                             const std::string& glyph) {
    std::vector<std::string> out;
    if (glyph.empty()) return out;
    const auto r = core.dispatch("glyphs " + glyph);
    if (!r.ok) return out;
    nlohmann::json d = nlohmann::json::parse(r.data, nullptr, false);
    if (d.is_discarded()) return out;
    /* `glyphs <name>` answers with the one descriptor; `glyphs` with a map of
     * them. Accept both shapes so a caller that passed a name it should not
     * have does not get a plausible-looking wrong answer. */
    if (d.is_object() && !d.contains("fields") && d.contains(glyph)) d = d[glyph];
    if (!d.is_object()) return out;
    const auto it = d.find("fields");
    if (it == d.end() || !it->is_array()) return out;
    for (const auto& f : *it)
        if (f.is_string()) out.push_back(f.get<std::string>());
    return out;
}

/* DECLARE A GLYPH INTO THE STATE DOCUMENT (Void Core 0.2.14, `glyph declare`).
 *
 * ONE DOOR, AND THE REASON IS THE ARGUMENT ITSELF. `register_glyph` takes its
 * JSON across the C ABI, where a string is a string; `glyph declare` is a
 * DISPATCHER COMMAND, so its descriptor is one SPEC 6.1 argument and has to be
 * quoted as one. A descriptor whose label is "Garden bed" tokenizes into three
 * arguments if it is pasted in raw. The failure is a refusal at the door rather
 * than anything subtle -- but it is a refusal every host writing this command
 * by hand hits once, which is why Core's Python binding grew
 * `declare_glyph(dict)` doing exactly this. This is that, for us.
 *
 * `maiz::arg` is Void Core's own exported encoder reached through Void Maiz --
 * never a quoting function written here. Five independent implementations of
 * 6.1 have been wrong, two of them ours. */
inline maiz::Result declare_glyph(maiz::Core& core,
                                  const std::string& descriptor_json) {
    return core.dispatch("glyph declare " + maiz::arg(descriptor_json));
}

/* THE GLYPH'S RUNE KIND (0.2.14 §3.3): "entity" | "act" | "measure".
 * Always present on a resolved descriptor; "entity" if the core is older or
 * the glyph is unknown, which is the same default the core itself applies. */
inline std::string glyph_kind(maiz::Core& core, const std::string& glyph) {
    if (glyph.empty()) return "entity";
    const auto r = core.dispatch("glyphs " + glyph);
    if (!r.ok) return "entity";
    nlohmann::json d = nlohmann::json::parse(r.data, nullptr, false);
    if (d.is_discarded() || !d.is_object()) return "entity";
    if (!d.contains("kind") && d.contains(glyph)) d = d[glyph];
    if (!d.is_object()) return "entity";
    const auto it = d.find("kind");
    return (it != d.end() && it->is_string()) ? it->get<std::string>() : "entity";
}

/* The block glyphs (v0 of the builder's vocabulary — the full inventory and
 * the email renderer pack land in phase D). Same pattern Node Blocks proved:
 * prev/next as a hidden adjacency chain, so snapping IS document order.
 * Bilingual fields are parallel (_en/_es), per the engine
 * (okf/concepts/sections/blocks-and-domains.md). `hero` is the hat block. */

} // namespace hormiga
