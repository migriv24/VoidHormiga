/* seeds.hpp - the DEMO TRANSCRIPTS: a starting database as commands.
 *
 * Split out of the glyph declarations on 2026-08-20. These are not glyphs;
 * they are DATA expressed the only way this project allows data to be
 * expressed - as a list of dispatcher commands, so the demo org, the cat
 * colony and the civic sample are built through exactly the door a person or
 * an agent uses. That is why they are long, and why the length is fine.
 *
 * Keeping them beside the glyph REGISTRATION was the accident: one file said
 * what a rune may be, and what some particular runes are.
 */
#pragma once

#pragma once
#pragma once
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

inline std::vector<std::string> seed_antfarm_transcript() {
    return {
        "mantle new antfarm",
        "rune new org_core core",
        R"(set core org_name "demo-org")",
        "setjson core pos [430,220]",
        // record agents (left): a local store + a local CSV source
        "rune new hol_sqlite data-sqlite",
        R"(set data-sqlite file "demo-org.db")",
        "setjson data-sqlite pos [80,60]",
        "rune new hol_csv import-csv",
        "setjson import-csv pos [80,200]",
        // asset agent: a local files folder
        "rune new hol_fs_assets assets-fs",
        R"(set assets-fs dir "assets")",
        "setjson assets-fs pos [80,320]",
        // the PUBLISH PIPELINE: core (records+assets) → HTML publisher → local server
        "rune new hol_html out-html",
        "setjson out-html pos [790,120]",
        "rune new hol_localhost out-localhost",
        R"(set out-localhost port "8780")",
        "setjson out-localhost pos [1120,120]",
        // wiring: records/assets ports fan out to their agents; the publisher's
        // `site` out feeds the server (a real chain, not siblings on "output")
        "link core data-sqlite --relation 1:1", // records → SQLite store
        "link core import-csv --relation 1:1",   // records → CSV source
        "link core assets-fs --relation 2:1",    // assets  → local files
        "link core out-html --relation 1:1",     // records → publisher.records
        "link core out-html --relation 2:2",     // assets  → publisher.assets
        "link out-html out-localhost --relation 3:1", // publisher.site → server
        "use demo-org",
    };
}

/* A starter SUMMER issue — a stack of query-backed blocks that AUTO-UPDATE:
 * hero → narrative → events section → live event grid → fliers section →
 * live image grid → jobs section → live job grid → footer. Every grid holds
 * a tag expression resolved at render time, so tagging a new summer flier
 * `@summer` makes it appear here on the next render with no edit to the
 * newsletter. Ends back in the data mantle so the app boots into Data. */
inline std::vector<std::string> seed_issue_transcript() {
    return {
        "mantle new issue-demo",
        "rune new hero masthead",
        R"(set masthead title_en "Summer 2026 Newsletter")",
        R"(set masthead title_es "Boletin de verano 2026")",
        "setjson masthead pos [80,60]",
        "rune new narrative intro",
        R"(set intro text_en "Here is what our community has going on this summer.")",
        R"(set intro text_es "Esto es lo que nuestra comunidad tiene este verano.")",
        "setjson intro pos [80,150]",
        "rune new section_header h-events",
        R"(set h-events title_en "Summer Events")",
        R"(set h-events title_es "Eventos de verano")",
        "setjson h-events pos [80,260]",
        "rune new event_grid eventos-verano",
        R"(set eventos-verano query "type:event")",
        R"(set eventos-verano caption_en "Recurring and one-off gatherings")",
        "setjson eventos-verano pos [80,340]",
        "rune new section_header h-fliers",
        R"(set h-fliers title_en "This Summer's Fliers")",
        R"(set h-fliers title_es "Volantes del verano")",
        "setjson h-fliers pos [80,440]",
        "rune new image_grid fliers-verano",
        R"(set fliers-verano query "type:image AND flier")",
        R"(set fliers-verano columns "3")",
        "setjson fliers-verano pos [80,520]",
        "rune new section_header h-jobs",
        R"(set h-jobs title_en "Job Opportunities")",
        R"(set h-jobs title_es "Oportunidades de empleo")",
        "setjson h-jobs pos [80,630]",
        "rune new job_grid empleos",
        R"(set empleos query "type:job AND status:active")",
        "setjson empleos pos [80,710]",
        "rune new footer pie",
        R"(set pie text_en "Casa Comunal - see you next month.")",
        R"(set pie text_es "Casa Comunal - nos vemos el proximo mes.")",
        "setjson pie pos [80,810]",
        "link masthead intro --relation 1:1",
        "link intro h-events --relation 2:1",
        "link h-events eventos-verano --relation 2:1",
        "link eventos-verano h-fliers --relation 2:1",
        "link h-fliers fliers-verano --relation 2:1",
        "link fliers-verano h-jobs --relation 2:1",
        "link h-jobs empleos --relation 2:1",
        "link empleos pie --relation 2:1",
        "use demo-org",
    };
}

/* The demo org: two contacts, one organization, two events (one with two
 * presenters — 1–N native, as edges), a flyer, a note. Axis-typed tags from
 * day one; relations are edges the canvas draws as wires. */
inline std::vector<std::string> seed_transcript() {
    return {
        "mantle new demo-org",

        "rune new organization casa-comunal",
        R"(set casa-comunal kind "community center")",
        "setjson casa-comunal pos [430,60]",
        "tag casa-comunal +type:org +status:active",

        "rune new contact marisol",
        R"(set marisol role "presenter")",
        R"(set marisol bio "Hosts the monthly platica series.")",
        R"(set marisol notes "internal: prefers evening slots")",
        "setjson marisol pos [80,40]",
        "tag marisol +type:contact +lang:es +status:active",

        "rune new contact ray",
        R"(set ray role "coordinator")",
        "setjson ray pos [80,230]",
        "tag ray +type:contact +lang:en +status:active",

        "rune new event junta-junio",
        "set junta-junio date 2026-06-12",
        R"(set junta-junio venue "Casa Comunal")",
        "setjson junta-junio pos [430,230]",
        "tag junta-junio +type:event +month:june",

        "rune new event taller-julio",
        "set taller-julio date 2026-07-25",
        R"(set taller-julio venue "Casa Comunal")",
        "setjson taller-julio pos [430,420]",
        "tag taller-julio +type:event +month:july",

        "rune new image flyer-taller",
        R"(set flyer-taller path "assets/flyer-taller.png")",
        R"(set flyer-taller alt "Taller de julio flyer")",
        "setjson flyer-taller pos [790,420]",
        "tag flyer-taller +type:image +month:july",

        "rune new note bienvenida",
        R"(set bienvenida text "Demo org. Everything on this canvas was built through the dispatcher - open the log strip, or type `history` in the command bar.")",
        "setjson bienvenida pos [790,40]",
        "tag bienvenida +type:note",

        "link marisol casa-comunal --relation member-of",
        "link ray casa-comunal --relation member-of",
        "link marisol junta-junio --relation presents",
        "link marisol taller-julio --relation presents",
        "link ray taller-julio --relation presents",
        "link flyer-taller taller-julio --relation flyer-of",
    };
}

/* The CAT COLONY — Hormiga's shipped DEFAULT dataset (okf/concepts/projects/cat-dataset.md).
 * A public, fictional, freely-editable demo so anyone can try Hormiga (and so we
 * can exercise Allomone on data we're free to reshape, unlike a real org). Built
 * the same way as every org: a replayable transcript. Deterministic (seeded RNG),
 * heavily OVERLAPPING tags (for set-theoretic Allomone tests), each cat located
 * (geo → map) with a birthday event on the calendar, plus colony-wide events.
 * Photos live in the committed demo-assets/ folder (cataas.com cats). */
inline std::vector<std::string> seed_cat_transcript() {
    std::vector<std::string> cmds;
    auto say = [&](std::string c) { cmds.push_back(std::move(c)); };
    std::mt19937 rng(1234567);
    auto roll = [&](int n) { return (int)(rng() % (unsigned)n); };
    auto chance = [&](double p) { return (rng() / (double)rng.max()) < p; };

    say("mantle new demo-org");

    const std::vector<std::string> names = {
        "garfield","whiskers","luna","simba","mittens","shadow","tigger","oreo",
        "felix","cleo","milo","nala","jasper","pepper","socks","binx","salem",
        "pumpkin","biscuit","noodle","waffles","mochi","pickles","gizmo","toby",
        "chairman-meow","sir-pounce","dr-mittens","admiral-fluff","captain-whisker",
        "professor-paws","duchess","tibbers","marmalade","clementine","hobbes",
        "bandit","ziggy","cricket","olive","hazel","poppy","bean","tofu","dumpling",
        "goose","moose","biscotti","espresso","latte"};
    const std::vector<std::string> coats = {"orange","black","white","gray",
        "calico","tabby","tuxedo","tortoiseshell"};
    const std::vector<std::string> breeds = {"tabby","siamese","persian",
        "maine-coon","bengal","ragdoll","sphynx","shorthair"};
    const std::vector<std::string> temper = {"lazy","playful","grumpy","friendly",
        "shy","curious","cuddly","mischievous"};
    struct Place { const char* code; const char* city; double lat, lon; };
    const std::vector<Place> places = {
        {"CA","Los Angeles",34.05,-118.24},{"NY","New York",40.71,-74.01},
        {"TX","Austin",30.27,-97.74},{"FL","Miami",25.76,-80.19},
        {"IL","Chicago",41.88,-87.63},{"WA","Seattle",47.61,-122.33},
        {"CO","Denver",39.74,-104.99},{"MA","Boston",42.36,-71.06},
        {"OR","Portland",45.52,-122.68},{"GA","Atlanta",33.75,-84.39}};
    const std::vector<std::string> affinities = {"naps","mice","yarn","fish",
        "boxes","sunbeam","catnip","laser","birdwatching","knocking-things-over"};
    const std::vector<std::string> orange_boys = {
        "garfield","marmalade","pumpkin","biscuit","tigger","milo"};

    char buf[128];
    for (int i = 0; i < (int)names.size(); ++i) {
        const std::string& name = names[i];
        // A cat IS a CONTACT (author, 2026-08-05): the point is to exercise the
        // contact datatype and contact-to-contact relationships, not a bespoke
        // glyph. Cat flavor rides as TAGS + role/bio; structured cat attributes
        // (coat/breed/sex/state) are tags because `contact` doesn't declare those
        // fields (projection drops undeclared fields anyway).
        say("rune new contact " + name);
        say("tag " + name + " +type:contact");
        say("tag " + name + " +cat"); // the bare species tag (author's example)
        std::snprintf(buf, sizeof buf, "demo-assets/cat-%02d.jpg", i + 1);
        say("set " + name + " avatar \"" + std::string(buf) + "\"");

        bool canon = std::find(orange_boys.begin(), orange_boys.end(), name) !=
                     orange_boys.end();
        bool male = canon ? true : chance(0.5);
        say("tag " + name + " +" + (male ? "male" : "female"));

        std::string coat = canon ? std::string("orange") : coats[roll((int)coats.size())];
        say("tag " + name + " +" + coat);
        std::string breed = breeds[roll((int)breeds.size())];
        say("tag " + name + " +breed:" + breed);
        std::string t = temper[roll((int)temper.size())];
        say("tag " + name + " +" + t);

        // real contact fields: role (a subtitle) + a bio
        say("set " + name + " role \"" + t + " " + coat + " cat\"");
        say("set " + name + " bio \"A " + coat + " " + breed + " who is " + t +
            ".\"");

        const Place& pl = places[roll((int)places.size())];
        say("tag " + name + " +state:" + pl.code);
        std::snprintf(buf, sizeof buf, "%.4f,%.4f",
                      pl.lat + (roll(200) - 100) / 1000.0,
                      pl.lon + (roll(200) - 100) / 1000.0);
        say("set " + name + " geo \"" + std::string(buf) + "\"");
        say("tag " + name + " +located");

        say("tag " + name + " +" + affinities[roll((int)affinities.size())]);
        if (chance(0.5)) say("tag " + name + " +" + affinities[roll((int)affinities.size())]);

        // a birthday EVENT on the calendar, linked to the contact
        int mo = 1 + roll(12), d = 1 + roll(28);
        std::string ev = name + "-birthday";
        say("rune new event " + ev);
        say("tag " + ev + " +type:event +birthday");
        std::snprintf(buf, sizeof buf, "2026-%02d-%02d", mo, d);
        say("set " + ev + " date \"" + std::string(buf) + "\"");
        say("set " + ev + " summary \"" + name + "'s birthday\"");
        say("link " + ev + " " + name + " --relation celebrates");

        if (coat == "orange" && male) { // the set-theory seam (author's example)
            say("tag " + name + " +lasagna");
            say("tag " + name + " +mondays");
            say("tag " + name + " +garfield-type");
        }
    }

    // ── cat-to-cat RELATIONSHIPS — the whole point of using `contact`: the
    // colony relates to ITSELF, so the connections/relations UI has real,
    // varied edges to test (friendships, siblings, rivalries, grooming). ───────
    const std::vector<std::string> rels = {
        "friend-of", "sibling-of", "rival-of", "grooms", "plays-with", "mentor-of"};
    for (int k = 0; k < 55; ++k) {
        const std::string& a = names[roll((int)names.size())];
        const std::string& b = names[roll((int)names.size())];
        if (a == b) continue;
        say("link " + a + " " + b + " --relation " + rels[roll((int)rels.size())]);
    }

    // ── colony-wide EVENTS (flesh-out): dated, located, some cats linked ───────
    struct Colony { const char* slug; const char* title; const char* date;
                    const char* tag; double lat, lon; };
    const std::vector<Colony> colony = {
        {"cat-show-2026","Annual Cat Show","2026-09-19","catshow",34.05,-118.24},
        {"adoption-fair","Spring Adoption Fair","2026-05-10","adoption",40.71,-74.01},
        {"vax-clinic","Vaccination Clinic","2026-03-14","clinic",41.88,-87.63},
        {"caturday","Caturday Meetup","2026-08-15","meetup",45.52,-122.68},
        {"nap-championship","World Napping Championship","2026-11-07","catshow",47.61,-122.33}};
    for (const auto& c : colony) {
        say("rune new event " + std::string(c.slug));
        say("tag " + std::string(c.slug) + " +type:event +" + c.tag);
        say("set " + std::string(c.slug) + " date \"" + std::string(c.date) + "\"");
        say("set " + std::string(c.slug) + " summary \"" + std::string(c.title) + "\"");
        std::snprintf(buf, sizeof buf, "%.4f,%.4f", c.lat, c.lon);
        say("set " + std::string(c.slug) + " geo \"" + std::string(buf) + "\"");
        say("tag " + std::string(c.slug) + " +located");
        // link a few nearby cats as attendees
        for (int k = 0; k < 4; ++k) {
            const std::string& who = names[roll((int)names.size())];
            say("link " + who + " " + std::string(c.slug) + " --relation attends");
        }
    }

    // a shelter organization, on the map, that many cats belong to
    say("rune new organization whisker-haven");
    say(R"(set whisker-haven abbreviation "WHS")");
    say(R"(set whisker-haven bio "A no-kill shelter and the colony's home base.")");
    say(R"(set whisker-haven geo "34.0490,-118.2500")");
    say("tag whisker-haven +type:organization +shelter +located");
    for (int k = 0; k < 12; ++k)
        say("link " + names[roll((int)names.size())] + " whisker-haven --relation member-of");

    // a default MAP VIEW over OpenStreetMap, centered on the USA, so the located
    // cats show on the map out of the box (the colony spans ten US cities)
    say("rune new map usa");
    say(R"(set usa source "osm")");
    say(R"(set usa center "39.8,-98.6")");
    say(R"(set usa zoom "4")");
    say(R"(set usa channel "main")");
    say(R"(set usa visible "1")");

    return cmds;
}

/* A sample NEWSLETTER for the cat colony — the Builder's demo document (a
 * cat-themed replacement for the original newsletter), so a fresh Hormiga shows a real,
 * query-backed newsletter to play with. Same block grammar as any issue:
 * hero → narrative → section headers → event_grid (tag-queried) → footer,
 * chained by adjacency links. */
inline std::vector<std::string> seed_cat_issue_transcript() {
    return {
        "mantle new issue-demo",
        "rune new hero masthead",
        R"(set masthead title_en "The Cat Colony Gazette")",
        R"(set masthead title_es "La Gaceta de la Colonia Felina")",
        "setjson masthead pos [80,60]",
        "rune new narrative intro",
        R"(set intro text_en "Whisker Haven news: who's turning a year older, and where to find us this season.")",
        R"(set intro text_es "Noticias de Whisker Haven: quien cumple anos y donde encontrarnos.")",
        "setjson intro pos [80,150]",
        "rune new section_header h-events",
        R"(set h-events title_en "Upcoming Events")",
        R"(set h-events title_es "Proximos eventos")",
        "setjson h-events pos [80,260]",
        "rune new event_grid all-events",
        R"(set all-events query "type:event")",
        R"(set all-events caption_en "Cat shows, adoption fairs, and the annual nap championship")",
        "setjson all-events pos [80,340]",
        "rune new section_header h-bdays",
        R"(set h-bdays title_en "Birthdays")",
        R"(set h-bdays title_es "Cumpleanos")",
        "setjson h-bdays pos [80,440]",
        "rune new event_grid bdays",
        R"(set bdays query "type:event AND birthday")",
        R"(set bdays caption_en "Many happy returns to these fine cats")",
        "setjson bdays pos [80,520]",
        "rune new footer pie",
        R"(set pie text_en "Whisker Haven - a no-kill shelter. Adopt, don't shop.")",
        R"(set pie text_es "Whisker Haven - refugio sin sacrificio.")",
        "setjson pie pos [80,620]",
        "link masthead intro --relation 1:1",
        "link intro h-events --relation 2:1",
        "link h-events all-events --relation 2:1",
        "link all-events h-bdays --relation 2:1",
        "link h-bdays bdays --relation 2:1",
        "link bdays pie --relation 2:1",
        "use demo-org",
    };
}

/* A LIBRARY of commented example Allomone scripts, seeded (DISABLED) into the
 * cat colony's `allomone` mantle — a learn-by-reading set covering the whole
 * language, from a first `for each rune` to set theory and quantifiers. Enable
 * one at a time to see it style the cards. The user-facing walkthrough is
 * okf/concepts/allomone/guide.md. */
inline std::vector<std::string> seed_cat_scripts_transcript() {
    std::vector<std::string> cmds;
    auto say = [&](std::string c) { cmds.push_back(std::move(c)); };
    say("mantle new allomone");

    struct Ex { const char* name; std::string body; };
    const std::vector<Ex> ex = {
        {"01-hello-allomone", R"CAT(-- HELLO, ALLOMONE. A script styles your data (derive-only: it never changes
-- it). This one colors every contact green. Tick the checkbox on the left to
-- ENABLE it and see it apply; untick to remove the color.
for each rune do
  if rune is contact then
    rune:color("#2e8b57")
  end
end
)CAT"},
        {"02-color-by-coat", R"CAT(-- BRANCHING with if / elseif / else, and `rune has "tag"`.
-- Color each cat by its coat.
for each rune do
  if rune has "orange" then rune:color("#e8890c")
  elseif rune has "black" then rune:color("#2b2b2b")
  elseif rune has "calico" then rune:color("#c46b3b")
  elseif rune has "tabby" then rune:color("#9c7a3c")
  else rune:color("#9aa0a6")
  end
end
)CAT"},
        {"03-busiest-cats", R"CAT(-- COUNTING. `count(rune.tags)` is how many tags a rune carries.
-- Highlight the busiest (most-tagged) cats.
for each rune do
  if count(rune.tags) >= 8 then rune:color("#7a2f8a")
  elseif count(rune.tags) >= 6 then rune:color("#3f6fae")
  end
end
)CAT"},
        {"04-lists-and-matching", R"CAT(-- VARIABLES, LISTS, and `matching`.
-- A `local` names a value; a list is written { "a", "b" }. `matching` keeps the
-- tags matching a set of patterns; a pattern ending in ":" matches any
-- namespaced tag (so "state:" matches "state:CA").
local regions = { "state:" }
for each rune do
  local here = rune.tags matching regions
  if count(here) > 0 then rune:color("#3f6fae") end
end
)CAT"},
        {"05-functions", R"CAT(-- FUNCTIONS name a reusable piece of logic. You can pass a rune to a function
-- and read its tags inside.
function heavy(r)
  return count(r.tags) >= 7
end
for each rune do
  if heavy(rune) then rune:color("#b3592e") end
end
)CAT"},
        {"06-recursion", R"CAT(-- RECURSION: a function can call itself. `fib` computes Fibonacci numbers.
-- (A runaway loop can't hang the app: the interpreter runs on a safety budget.)
function fib(n)
  if n < 2 then return n end
  return fib(n - 1) + fib(n - 2)
end
for each rune do
  if rune is contact and fib(7) == 13 then rune:color("#4e8d85") end
end
)CAT"},
        {"07-set-theory", R"CAT(-- SET THEORY — the powerful one. Find the orange male cats, DISCOVER the other
-- tags they share, then color everyone carrying those tags. The script never
-- names "lasagna" or "mondays" — they are found automatically.
--   runes            = all runes in the database
--   runes where ...  = only the ones matching (a set)
--   tags_of(set)     = the union of all their tags
--   minus(a, b)      = set difference;  overlaps(a, b) = do they share anything?
local gs = runes where (rune has "orange" and rune has "cat" and rune has "male")
local spread = minus(tags_of(gs), { "orange", "cat", "male", "type:contact", "located" })
for each rune do
  if overlaps(rune.tags, spread) then rune:color("#ff8800") end
end
)CAT"},
        {"08-quantifiers", R"CAT(-- QUANTIFIERS: `all(list, test)` is true only if EVERY element passes (for-all);
-- `any(list, test)` is true if SOME element passes (there-exists). The test is a
-- function. Here: color cats only if EVERY orange cat in the colony is male.
local oranges = runes where rune has "orange"
function isMale(r) return r has "male" end
for each rune do
  if rune has "cat" and all(oranges, isMale) then rune:color("#cc6600") end
end
)CAT"},
        {"09-color-by-glyph", R"CAT(-- GLYPHS are Void Core's types. `rune is <glyph>` tests a rune's type.
-- Color the whole database by kind.
for each rune do
  if rune is contact then rune:color("#b3592e")
  elseif rune is event then rune:color("#3f6fae")
  elseif rune is organization then rune:color("#8a6d3b")
  end
end
)CAT"},
        {"10-traversal-and-clusters", R"CAT(-- GRAPH TRAVERSAL — refer to runes by STRUCTURE, not by name. This is what
-- Allomone is built for: logic that still works when the data grows to
-- thousands, or when who-is-connected-to-whom changes.
--   neighbours(rune)     = the runes directly linked to it
--   linked(rune, "rel")  = neighbours via a relation (e.g. "friend-of")
--   cluster(rune)        = the whole connected group it belongs to (a set)
--   degree(rune)         = how many neighbours (a simple centrality)
for each rune do
  -- a "hub": well-connected cats, whoever they happen to be
  if degree(rune) >= 6 then rune:color("#7a2f8a")
  -- cats with at least one rival, wherever that rival is
  elseif count(linked(rune, "rival-of")) > 0 then rune:color("#c0392b")
  -- cats in a big friend-group (their connected cluster is large)
  elseif count(cluster(rune)) >= 20 then rune:color("#2e8b57")
  end
end
)CAT"},
        {"11-fields", R"CAT(-- FIELDS: read a rune's own values with rune.<field> (the actual data, not
-- tags) — a contact's role/bio, an event's date/summary. ISO dates like
-- "2026-08-15" compare correctly as text, so date ranges just work.
for each rune do
  -- events happening in August 2026
  if rune is event and rune.date >= "2026-08-01" and rune.date <= "2026-08-31" then
    rune:color("#3f6fae")
  -- contacts that actually have a bio written
  elseif rune is contact and rune.bio != "" then
    rune:color("#2e8b57")
  end
end
)CAT"},
        {"12-graph-measures", R"CAT(-- GRAPH MEASURES — the sophisticated, whole-graph view. These describe a
-- rune's PLACE in the network, so the logic keeps meaning as the colony grows.
--   centrality(rune)  = how important/well-connected (0..1), the whole graph
--   community(rune)    = the sub-group it clusters into (a set of runes)
--   within(rune, k)    = every rune within k hops (its neighbourhood)
--   distance(a, b)     = hops between two runes
for each rune do
  if centrality(rune) >= 0.6 then rune:color("#b3592e")        -- a central figure
  elseif count(community(rune)) >= 10 then rune:color("#7a2f8a") -- in a big community
  elseif count(within(rune, 1)) >= 5 then rune:color("#3f6fae")  -- a busy neighbourhood
  end
end
)CAT"},
    };

    int i = 0;
    for (const auto& e : ex) {
        std::string id = "ex" + std::to_string(++i);
        say("rune new script " + id);
        say("set " + id + " name \"" + std::string(e.name) + "\"");
        say("set " + id + " enabled \"0\""); // disabled — enable to explore
        say("tag " + id + " +example");
        // the body is a JSON string wrapped as ONE single-quoted arg; a literal
        // apostrophe inside (e.g. "rune's") must escape as \' or the tokenizer
        // truncates the arg (args.c convention, same as app.cpp json_arg)
        std::string arg = "'";
        for (char c : nlohmann::json(e.body).dump()) { if (c == '\'') arg += '\\'; arg += c; }
        arg += "'";
        say("setjson " + id + " body " + arg);
    }
    say("use demo-org");
    return cmds;
}

/* A LIBRARY of commented example scripts in the VOID MAIZ dialect (adopted
 * 2026-08-10), seeded DISABLED into the same `allomone` mantle as the legacy
 * set above — so the two sit side by side and the difference is readable
 * rather than described.
 *
 * The teaching order is deliberate and is the order the model has to be learnt
 * in: a rule set is not a sequence; strength decides; a sharper rule beats a
 * broader one SILENTLY inside one script; two SCRIPTS disagreeing is a
 * question, not a race. The last two exist to be enabled TOGETHER — that is
 * the only way to see a conflict, and seeing one is the point of the whole
 * engine. */
inline std::vector<std::string> seed_allomone_scripts_transcript() {
    std::vector<std::string> cmds;
    auto say = [&](std::string c) { cmds.push_back(std::move(c)); };
    say("mantle new allomone");

    struct Ex { const char* name; std::string body; };
    const std::vector<Ex> ex = {
        {"a1-rules-are-a-set", R"ALLO(# THE FIRST THING TO UNLEARN: this is not a program. It is a SET of rules.
# Moving a line changes nothing. There is no first-match-wins, no fallthrough,
# no else. Each rule says what SHOULD BE TRUE of a whole class of things.
#
# A rule's STRENGTH is how many terms its condition has, and the sharper rule
# wins. Inside one script that happens silently — defaults-and-exceptions is
# the entire idiom.

when all                    then color "#8a9199"   # strength 0: the default
when glyph "contact"        then color "#2e8b57"   # strength 1: beats it
when glyph "contact" and has "orange" then color "#e8890c"   # strength 2

# Derive-only: none of this touches your data. Untick the box and it is gone,
# with nothing to undo, because nothing was ever written.
)ALLO"},
        {"a2-tags-and-not", R"ALLO(# CONDITIONS join with `and`. `not` negates one term. There is deliberately no
# `or` — write two rules instead, and they then carry their own strength, which
# tells you more than one merged rule would.
#
# `has` is Void Core's spelling of `tag`, `glyph` of `kind`, `rune` of `name`.
# They are exact synonyms, so a script can read like the system it describes.

when has "orange"                    then color "#e8890c"
when has "black"                     then color "#2b2b2b"
when has "calico"                    then color "#c46b3b"
when has "tabby"                     then color "#9c7a3c"
when has "cat" and not has "male"    then badge "she"
)ALLO"},
        {"a3-namespaces-and-fields", R"ALLO(# `has "x"` matches a tag EXACTLY. Hormiga adds the conditions our data needs
# and tags encode badly — a namespace, a field, a role, a name fragment.

when under "coat:"                   then badge "coated"   # any coat: tag
when field "role=volunteer"          then color "#2e8b57"
when field "bio"                     then badge "has bio"  # non-empty field
when field-has "email=@"             then badge "reachable"
when named-like "mittens"            then color "#7a2f8a"

# `under` exists because the old dialect's `has "ns:"` matched a PREFIX and the
# kernel's does not. It is our tag convention, so it is our predicate.
)ALLO"},
        {"a4-define", R"ALLO(# `define` names a CONDITION so you can reuse it. It is a macro, not a
# function: it is expanded where it is called, so a call contributes its
# expanded term count to strength — a definition can never secretly make a rule
# broader than it looks.
#
# Definitions are a set too: write one below its use if you prefer. They are
# script-local, permanently, which is what keeps parsing a pure function of the
# text in front of you.

define coated(c) = has c and has "cat"

when coated("orange")   then color "#e8890c", weight 1
when coated("black")    then color "#2b2b2b", weight 1

# `not f(...)` only works when f expands to ONE term — negating a conjunction
# needs `or`, and there is no `or`. Define the negated condition directly.
)ALLO"},
        {"a5-merge-laws", R"ALLO(# HOW TWO OPINIONS COMBINE is declared per property, and it is the real thing
# to know before writing rules (the Reference tab lists every law):
#
#   color, icon, label   unique — one right answer; disagreement SURFACES
#   weight               sum    — contributions accumulate
#   priority             max    — the loudest wins
#   badge, note          all    — every opinion is collected
#
# Two gotchas worth meeting here rather than in the wild:
#   1. `sum` counts each DISTINCT value once. Two sources both saying 2 total 2.
#   2. Under a combining law a broad `when all` default becomes an INGREDIENT
#      instead of being harmlessly overridden. Audit your defaults.

when has "cat"                       then weight 1
when has "orange"                    then weight 2, priority 1
when has "orange" and has "male"     then priority 3, badge "orange tom"
when has "cat"                       then note "a colony member"
)ALLO"},
        {"a6-dates", R"ALLO(# DATES are the case the predicate seam exists for: parameterized and
# continuous, which is exactly what a tag encodes badly. These read the rune's
# `date` (or a job's `deadline`) against TODAY — frozen once per derivation, so
# the result never depends on when it was asked.

when upcoming "14"      then color "#3f6fae", badge "soon", priority 2
when upcoming "3"       then color "#c0392b", priority 3
when overdue "30"       then color "#6b6b6b", badge "past"
when before "2026-01-01" then badge "history"
when glyph "event" and undated "" then badge "recurring"
)ALLO"},
        {"a7-the-graph", R"ALLO(# THE GRAPH, structurally — never by naming a rune. Logic written this way
# still means something when the colony grows or the links change.
#
# `central` is eigenvector centrality over the whole graph, computed once per
# frame by the host; `degree-over` counts distinct neighbours.

when central "0.6"          then color "#b3592e", badge "central"
when degree-over "5"        then weight 2
when linked "rival-of"      then badge "has a rival"
when isolated ""            then color "#4a4a4a", note "nobody is linked to this"

# `isolated` is the outreach gap made visible — a rule nobody has to remember
# to run, because a constraint is simply present.
)ALLO"},
        {"a8-surfaces", R"ALLO(# ONE ANNOTATION, MANY SURFACES. An unprefixed property reaches every
# surface that draws this rune. Prefix it and only that surface listens:
#
#   card-   the Data list and cards        cal-   the Calendar
#   map-    Territory's markers            web-   the newsletter and website
#
# The law is the same either way, so `map-weight` accumulates like `weight`.

when has "cat"          then color "#9c7a3c"        # everywhere
when has "orange"       then map-color "#e8890c"    # the map only
when has "orange"       then map-weight 2           # a fatter marker
when upcoming "7"       then cal-color "#c0392b", cal-priority 5

# `cal-priority` decides which entries a crowded day shows first - Max, so
# several scripts can shout and none of them silences the others.
)ALLO"},
        {"a9-the-privacy-seam", R"ALLO(# WHAT A RULE MAY NEVER READ, and the one thing it may.
#
# Internal notes are invisible to every condition. Not hidden - INVISIBLE:
# `field "notes"` reads empty, and so does `field-has "notes=anything"`,
# because repeated equality tests are a way to reproduce a value. Naming
# one in a rule tells you so instead of quietly matching nothing.
#
# The single observable is `internal ""` - that this rune HAS notes. That is
# exactly enough to keep it out of an export, and never enough to leak one.

when internal ""        then web-hide "1", badge "internal"
when internal ""        then note "kept out of the newsletter and the site"

# `web-hide` drops a rune from every query-backed block: the newsletter, the
# website, and the Builder preview, which shows what will be published and so
# must agree. A CONTESTED hide does not hide - two scripts disagreeing about
# whether something may go out is a question for a person, and silently
# suppressing content nobody agreed to suppress looks like a broken export.
)ALLO"},
        {"a10-a-conflict", R"ALLO(# ENABLE THIS ONE TOGETHER WITH a2-tags-and-not.
#
# This script is a different SOURCE with a different opinion about orange cats,
# at the same strength. That is not a race the engine should win on someone's
# behalf, so the cell becomes ⊤ — a first-class conflict. The colour is CLEARED
# rather than guessed (look: those cards go back to the default), and the
# Conflicts tab shows who said what and lets you settle it.
#
# Settling is a dispatcher command, so your ruling is logged, attributed,
# replayable and undoable like every other gesture.
#
# Two rules inside ONE script never do this. Only two scripts can.

when has "orange"   then color "#0a84ff"
when has "orange"   then note "the alerts script has an opinion here"
)ALLO"},
    };

    int i = 0;
    for (const auto& e : ex) {
        std::string id = "allo" + std::to_string(++i);
        say("rune new allo-script " + id);
        say("set " + id + " title \"" + std::string(e.name) + "\"");
        say("set " + id + " enabled \"0\""); // disabled — enable to explore
        say("tag " + id + " +example");
        // ONE single-quoted arg; a literal apostrophe escapes as \' or the
        // tokenizer truncates the value (args.c convention).
        std::string arg = "'";
        for (char c : nlohmann::json(e.body).dump()) { if (c == '\'') arg += '\\'; arg += c; }
        arg += "'";
        say("setjson " + id + " source " + arg);
    }
    say("use demo-org");
    return cmds;
}

/* A small CIVIC RECORD, seeded into its own `civic` mantle so the window has
 * something to open (okf/concepts/projects/civic-record.md).
 *
 * SYNTHETIC PEOPLE, REAL STRUCTURE — and the split is deliberate. The council
 * is fictional; the document shape is modelled on Springfield's actual
 * *Operating Policies and Procedures, October 2025*: the nested numbering, the
 * fact that §9.5.1 names supersession, and §10's split between permanent
 * amendment and temporary suspension are all read from the real PDF.
 *
 * The people are invented because **committing fabricated votes attributed to
 * real named officials would be exactly the harm the concept page argues
 * against**, in a repo that is a public artifact. Verified structure, fictional
 * claims — the same rule the Cat Dataset follows. Real data lives only where
 * the app puts it at runtime.
 *
 * It is built to exercise the four things a list of runes cannot show: a
 * provision under two parents, an amendment, a suspension, and two same-day
 * assertions that disagree. */
inline std::vector<std::string> seed_civic_transcript() {
    std::vector<std::string> cmds;
    auto say = [&](std::string c) { cmds.push_back(std::move(c)); };
    auto set = [&](const std::string& r, const std::string& k, const std::string& v) {
        std::string q = "'";
        for (char c : v) { if (c == '\'') q += "\\"; q += c; }
        say("set " + r + " " + k + " " + q + "'");
    };
    say("mantle new civic");

    // the council
    struct Who { const char* id; const char* role; };
    for (const Who& w : {Who{"ada-brenner", "former mayor"},
                         Who{"boone-castellanos", "mayor"},
                         Who{"cyd-nakamura", "councilor"},
                         Who{"dov-eriksson", "former councilor"},
                         Who{"esme-whitfield", "councilor"}}) {
        say(std::string("rune new contact ") + w.id);
        set(w.id, "role", w.role);
    }
    // TERMS ARE RUNES: an edge carries only (relation, directed), so a tenure
    // with dates has to be a thing. That is what makes "who sat here in 2021" a
    // query rather than a replay of the command log.
    struct Term { const char* id, *who, *seat, *from, *until; };
    for (const Term& t : {Term{"t-mayor-1", "ada-brenner", "mayor", "2019-01-01", "2023-01-01"},
                          Term{"t-mayor-2", "boone-castellanos", "mayor", "2023-01-01", ""},
                          Term{"t-ward1", "cyd-nakamura", "ward-1", "2019-01-01", ""},
                          Term{"t-ward2-1", "dov-eriksson", "ward-2", "2019-01-01", "2024-06-30"},
                          Term{"t-ward2-2", "esme-whitfield", "ward-2", "2024-07-01", ""}}) {
        say(std::string("rune new term ") + t.id);
        set(t.id, "seat", t.seat);
        set(t.id, "from", t.from);
        if (*t.until) set(t.id, "until", t.until);
        say(std::string("link ") + t.id + " " + t.who + " --relation holder");
    }

    say("rune new policy opp");
    set("opp", "title", "Council Operating Policies");
    set("opp", "citation", "OPP");
    set("opp", "body", "Common Council");

    struct Prov { const char* id, *num, *head, *parent; };
    for (const Prov& p : {Prov{"opp-3", "3", "Regular Meetings", "opp"},
                          Prov{"opp-3-3", "3.3", "Attendance", "opp-3"},
                          Prov{"opp-3-3-1", "3.3.1", "Notification", "opp-3-3"},
                          Prov{"opp-3-3-3", "3.3.3", "Remote Participation", "opp-3-3"},
                          Prov{"opp-9", "9", "Boards and Commissions", "opp"},
                          Prov{"opp-9-5-3", "9.5.3", "Public Meetings Law", "opp-9"}}) {
        say(std::string("rune new provision ") + p.id);
        set(p.id, "number", p.num);
        set(p.id, "heading", p.head);
        set(p.id, "source_url", "https://example.gov/opp.pdf");
        say(std::string("link ") + p.id + " " + p.parent + " --relation part-of");
    }
    // A SHARED PROVISION: §9.5.3 restates the state public-meetings law, so it
    // belongs to the meetings chapter too. A tree cannot hold this.
    say("link opp-9-5-3 opp-3 --relation part-of");

    // Four assertions, and NONE of them is a distinct kind of object. An
    // amendment has text and no end; a suspension has an end and no text.
    struct Rev { const char* id, *sum, *text, *from, *until, *about; };
    for (const Rev& r :
         {Rev{"rev-2019-notify", "Original adoption",
              "A councilor shall notify the Recorder before the meeting.",
              "2019-02-04", "", "opp-3-3-1"},
          Rev{"rev-2025-24h", "Notification window set to 24 hours",
              "A councilor shall notify the Recorder at least 24 hours before "
              "the meeting.",
              "2025-10-06", "", "opp-3-3-1"},
          // adopted the SAME DAY and disagreeing — the clerk's real problem
          Rev{"rev-2025-48h", "Notification window set to 48 hours",
              "A councilor shall notify the Recorder at least 48 hours before "
              "the meeting.",
              "2025-10-06", "", "opp-3-3-1"},
          Rev{"rev-2019-remote", "Original adoption",
              "A councilor may participate remotely with the President's consent.",
              "2019-02-04", "", "opp-3-3-3"},
          Rev{"rev-2026-suspend", "Remote participation suspended pending review",
              "", "2026-01-05", "2026-07-01", "opp-3-3-3"}}) {
        say(std::string("rune new revision ") + r.id);
        set(r.id, "summary", r.sum);
        if (*r.text) set(r.id, "text", r.text);
        set(r.id, "from", r.from);
        if (*r.until) set(r.id, "until", r.until);
        set(r.id, "source_url", "https://example.gov/minutes/");
        say(std::string("link ") + r.id + " " + r.about + " --relation amends");
    }
    // votes, as edges
    for (const char* who : {"boone-castellanos", "cyd-nakamura"})
        say(std::string("link ") + who + " rev-2025-24h --relation voted-yes");
    say("link dov-eriksson rev-2025-24h --relation voted-no");
    say("link esme-whitfield rev-2025-24h --relation absent");
    for (const char* who : {"boone-castellanos", "esme-whitfield"})
        say(std::string("link ") + who + " rev-2026-suspend --relation voted-yes");
    say("link cyd-nakamura rev-2026-suspend --relation voted-no");

    // NOTE: this transcript deliberately does NOT end with `use demo-org`, the
    // way the older seeds do. A transcript that switches to a mantle it did not
    // create only replays in one particular order — which is precisely what a
    // transcript is supposed to stop mattering, and it failed the first time a
    // test replayed this into a fresh core. The caller restores its own mantle.
    return cmds;
}

} // namespace hormiga
