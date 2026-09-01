/* spine_smoke.cpp — the phase C spine, headless.
 *
 * 1. SQLite round-trip: seed an org, save it through the Storage holiday
 *    (state blob + normalized rows), reload the blob into a fresh core, and
 *    the projected scenes must fingerprint identically. Stats must count
 *    what the seed built.
 * 2. CSV import: a messy spreadsheet (quoted commas, name collisions,
 *    unknown columns, a tags column) compiles to commands; dispatched as ONE
 *    compile_commit batch it lands the runes tagged and fielded — and one
 *    `undo` removes the entire import.
 */
#include "../src/domain/doc_actions.hpp"
#include "../src/domain/import.hpp"
#include "../src/domain/map_actions.hpp"
#include "../src/domain/rescue_import.hpp"
#include "../src/domain/seed.hpp"
#include "../src/platform/storage.hpp"
#include "../src/domain/temper.hpp"

#include "voidmaiz/embed.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/project.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            ++failures;                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                          \
    } while (0)

static std::string fingerprint(const maiz::Scene& s) {
    std::ostringstream out;
    out << s.mantle << "\n";
    for (const auto& n : s.nodes) {
        out << n.name << "|" << n.glyph << "|";
        for (const auto& t : n.tags) out << "@" << t;
        for (const auto& f : n.fields) out << f.key << "=" << f.value_json << ";";
        out << "\n";
    }
    for (const auto& w : s.wires)
        out << w.from << ">" << w.to << "|" << w.relation << "\n";
    return out.str();
}

static maiz::Scene project(maiz::Core& c, const char* mantle) {
    maiz::ProjectOptions po;
    po.mantle = mantle;
    return maiz::project_scene(c, po);
}

int main() {
    namespace fs = std::filesystem;

    // ── seed an org ─────────────────────────────────────────────────────────
    maiz::Core a;
    hormiga::register_glyphs(a);
    hormiga::register_block_glyphs(a);
    hormiga::register_antfarm_glyphs(a);
    for (const auto& cmd : hormiga::seed_transcript()) a.dispatch(cmd);
    for (const auto& cmd : hormiga::seed_issue_transcript()) a.dispatch(cmd);

    // ── 1: the SQLite round-trip ────────────────────────────────────────────
    fs::path db = fs::temp_directory_path() / "hormiga-spine-smoke.db";
    fs::remove(db);
    {
        hormiga::Storage st(db);
        CHECK(st.ok());
        std::vector<maiz::Scene> mantles = {project(a, "demo-org"),
                                            project(a, "issue-demo")};
        CHECK(st.save(a.export_state(), mantles));
        auto stats = st.stats();
        CHECK(stats.runes == (long long)(mantles[0].nodes.size() +
                                         mantles[1].nodes.size()));
        CHECK(stats.tags > 0);
        CHECK(stats.links > 0);
        CHECK(!stats.saved_at.empty());
        CHECK(stats.bytes > 0);

        maiz::Core b(st.load_state());
        hormiga::register_glyphs(b);
        hormiga::register_block_glyphs(b);
        CHECK(fingerprint(project(b, "demo-org")) == fingerprint(project(a, "demo-org")));
        CHECK(fingerprint(project(b, "issue-demo")) ==
              fingerprint(project(a, "issue-demo")));
    }
    fs::remove(db);

    // ── 2: the CSV import ───────────────────────────────────────────────────
    std::string csv =
        "Name,Role,Email,nickname,tags\r\n"
        "Luz Maria,\"presenter, host\",luz@example.org,,lang:es status:active\r\n"
        "ray,coordinator,ray2@example.org,Ray,status:active\r\n" // collides with seed's ray
        ",organizer,,,\r\n";                                     // nameless row
    maiz::Scene before = project(a, "demo-org");
    auto res = hormiga::compile_csv_import(
        csv, "contact", hormiga::glyph_fields("contact"),
        [&](const std::string& n) { return before.find(n) != nullptr; });
    CHECK(res.error.empty());
    CHECK(res.rows == 3);
    CHECK(res.notes.size() == 1); // "nickname" column ignored, reported

    a.dispatch("use demo-org");
    maiz::Result r = a.dispatch(maiz::compile_commit(res.commands));
    CHECK(r.ok);
    maiz::Scene after = project(a, "demo-org");
    const maiz::SceneNode* luz = after.find("luz-maria");
    CHECK(luz != nullptr);
    if (luz) {
        CHECK(std::find(luz->tags.begin(), luz->tags.end(), "lang:es") != luz->tags.end());
        CHECK(std::find(luz->tags.begin(), luz->tags.end(), "type:contact") !=
              luz->tags.end());
        bool role_ok = false;
        for (const auto& f : luz->fields)
            if (f.key == "role" && f.value_json.find("presenter, host") != std::string::npos)
                role_ok = true;
        CHECK(role_ok); // the quoted comma survived
    }
    CHECK(after.find("ray-2") != nullptr);      // collision suffixed, seed's ray intact
    CHECK(after.find("contact-row3") != nullptr); // nameless row got a slug
    CHECK((int)after.nodes.size() == (int)before.nodes.size() + 3);

    // the whole import is ONE undo frame
    a.dispatch("undo");
    maiz::Scene undone = project(a, "demo-org");
    CHECK((int)undone.nodes.size() == (int)before.nodes.size());
    CHECK(undone.find("luz-maria") == nullptr);

    // ── 3: the rescue import (synthetic dump — no real data in this repo) ──
    std::string orgs_j = R"([{"name":"Casa Comunal","website":"https://cc.example",)"
                         R"("contact_email":"hola@cc.example","description":"A center."}])";
    std::string contacts_j =
        R"([{"name":"Luz Maria","title":"host","email":"luz@cc.example",)"
        R"("office_phone":"","work_cell":"555-1","organization":"Casa Comunal",)"
        R"("receive_newsletter":true,"notes":"internal note"},)"
        R"({"name":"","email":"Email"}])"; // header-echo row: must be skipped
    std::string events_j =
        R"([{"title":"Cafecito","days":"Tuesday","start_time":"10:00",)"
        R"("end_time":"11:00","location":"Patio","organization":"Casa Comunal"}])";
    std::string presenters_j =
        R"([{"name":"Dr. Soto","presentation_month":"July","organization":"Nowhere Org"}])";

    maiz::Scene pre = project(a, "demo-org");
    auto ri = hormiga::compile_rescue_import(
        contacts_j, orgs_j, events_j, presenters_j,
        [&](const std::string& n) { return pre.find(n) != nullptr; });
    CHECK(ri.error.empty());
    CHECK(ri.contacts == 1 && ri.organizations == 1 && ri.events == 1 &&
          ri.presenters == 1);
    CHECK(ri.links == 2);          // luz-maria + cafecito → casa-comunal-2
    CHECK(ri.notes.size() == 1);   // dr-soto's unknown org, reported not fatal
    a.dispatch("use demo-org");
    CHECK(a.dispatch(maiz::compile_commit(ri.commands)).ok);
    maiz::Scene post = project(a, "demo-org");
    CHECK(post.find("casa-comunal-2") != nullptr); // seed collision suffixed
    const maiz::SceneNode* luz2 = post.find("luz-maria");
    CHECK(luz2 != nullptr);
    if (luz2) {
        CHECK(std::find(luz2->tags.begin(), luz2->tags.end(), "newsletter:yes") !=
              luz2->tags.end());
        bool phone_ok = false;
        for (const auto& f : luz2->fields)
            if (f.key == "phone" && f.value_json.find("555-1") != std::string::npos)
                phone_ok = true;
        CHECK(phone_ok); // work_cell fallback when office_phone empty
    }
    bool edge_ok = false;
    for (const auto& w : post.wires)
        if (w.from == "luz-maria" && w.to == "casa-comunal-2" &&
            w.relation == "member-of")
            edge_ok = true;
    CHECK(edge_ok);
    a.dispatch("undo"); // the whole rescue lands (and leaves) as ONE frame
    CHECK(project(a, "demo-org").find("luz-maria") == nullptr);

    // ── 3b: the json_store second pass (graph/images/jobs/meta) ────────────
    a.dispatch("use demo-org");
    a.dispatch("rune new organization casa-comunal-2"); // synthetic org for the job
    a.dispatch(R"(tag casa-comunal-2 +type:org)");
    std::string contacts_j2 =
        R"([{"id":1,"name":"Luz Maria"},{"id":2,"name":"Ray Coord"}])";
    std::string events_j2 = R"([{"id":10,"title":"Cafecito"}])";
    std::string presenters_j2 = R"([])";
    std::string graph_j =
        R"({"nodes":{"c1":{"type":"contact","label":"Luz Maria"},)"
        R"("c2":{"type":"contact","label":"Ray Coord"},)"
        R"("c3":{"type":"contact","label":"Nobody Here"}},)"
        R"("edges":[{"relation":"connected_to","from_id":"c1","to_id":"c2"},)"
        R"({"relation":"connected_to","from_id":"c1","to_id":"c3"},)"
        R"({"relation":"member_of","from_id":"c1","to_id":"c2"}]})"; // member_of ignored
    std::string images_j =
        R"({"images":[{"id":"abc123","name":"Cafecito Flyer",)"
        R"("url":"https://i.ibb.co/x/flyer.png","alt":"a flyer",)"
        R"("tags":["july","reoccuring"],"event_ids":["10"]}]})";
    std::string jobs_j =
        R"({"jobs":[{"id":"j1","title":"Coordinator","org":"Casa Comunal 2",)"
        R"("pay":"$20/hr","active":true,"tags":["healthcare"]}]})";
    std::string cmeta_j = R"({"1":{"tags":["march","reoccurring"]}})";
    std::string emeta_j = R"({"10":{"tags":["healthcare"]}})";

    // rune names must match slug(graph label) / slug(source name) exactly —
    // that IS the resolution strategy under test (no fuzzy matching)
    a.dispatch("rune new contact luz-maria");
    a.dispatch("tag luz-maria +type:contact");
    a.dispatch("rune new contact ray-coord");
    a.dispatch("tag ray-coord +type:contact");
    a.dispatch("rune new event cafecito");
    a.dispatch("tag cafecito +type:event");

    maiz::Scene before2 = project(a, "demo-org");
    auto js = hormiga::compile_json_store_import(
        graph_j, images_j, jobs_j, cmeta_j, emeta_j, "{}", contacts_j2, events_j2,
        presenters_j2,
        [&](const std::string& n) { return before2.find(n) != nullptr; },
        [&](const std::string& n) {
            const maiz::SceneNode* nd = before2.find(n);
            return nd ? nd->glyph : std::string();
        });
    CHECK(js.error.empty());
    CHECK(js.images == 1);
    CHECK(js.jobs == 1);
    CHECK(js.graph_edges == 1); // "nobody-here" edge dropped, one note
    CHECK(js.notes.size() >= 1);
    CHECK(a.dispatch(maiz::compile_commit(js.commands)).ok);
    maiz::Scene after2 = project(a, "demo-org");

    const maiz::SceneNode* flyer = after2.find("cafecito-flyer");
    CHECK(flyer != nullptr);
    if (flyer)
        CHECK(std::find(flyer->tags.begin(), flyer->tags.end(), "recurring") !=
              flyer->tags.end()); // "reoccuring" normalized
    bool flyer_edge = false, connect_edge = false, job_edge = false;
    for (const auto& w : after2.wires) {
        if (w.from == "cafecito-flyer" && w.to == "cafecito" && w.relation == "flyer-of")
            flyer_edge = true;
        if (w.from == "luz-maria" && w.to == "ray-coord" && w.relation == "connected-to")
            connect_edge = true;
        if (w.from == "coordinator" && w.to == "casa-comunal-2" &&
            w.relation == "posted-by")
            job_edge = true;
    }
    CHECK(flyer_edge);
    CHECK(connect_edge);
    CHECK(job_edge);
    // per-row meta landed on the right runes (id → slug resolution, positive path)
    const maiz::SceneNode* luz3 = after2.find("luz-maria");
    CHECK(luz3 != nullptr);
    if (luz3) {
        CHECK(std::find(luz3->tags.begin(), luz3->tags.end(), "march") != luz3->tags.end());
        CHECK(std::find(luz3->tags.begin(), luz3->tags.end(), "recurring") !=
              luz3->tags.end()); // "reoccurring" normalized too
    }
    const maiz::SceneNode* caf = after2.find("cafecito");
    CHECK(caf != nullptr);
    if (caf)
        CHECK(std::find(caf->tags.begin(), caf->tags.end(), "healthcare") != caf->tags.end());

    // ── 3c: the date-tag temper pass (event date → month/season) ───────────
    a.dispatch("use demo-org");
    // seed's junta-junio is 2026-06-12 (June → summer); taller-julio 2026-07-25
    maiz::Scene ds = project(a, "demo-org");
    auto dtags = hormiga::temper::compile_date_tags(ds);
    CHECK(!dtags.empty());
    CHECK(a.dispatch(maiz::compile_commit(dtags)).ok);
    maiz::Scene tagged = project(a, "demo-org");
    const maiz::SceneNode* jj = tagged.find("junta-junio");
    CHECK(jj != nullptr);
    if (jj) {
        CHECK(std::find(jj->tags.begin(), jj->tags.end(), "month:june") != jj->tags.end());
        CHECK(std::find(jj->tags.begin(), jj->tags.end(), "season:summer") !=
              jj->tags.end());
    }
    // idempotent: a second pass emits nothing
    auto dtags2 = hormiga::temper::compile_date_tags(tagged);
    CHECK(dtags2.empty());
    // @season:summer now matches dated summer events
    int summer = 0;
    for (const auto& n : tagged.nodes)
        if (n.glyph == "event" && maiz::node_matches("season:summer", n)) ++summer;
    CHECK(summer >= 2); // junta-junio + taller-julio at least

    // ── 4: the antfarm colony replays like everything else ─────────────────
    for (const auto& cmd : hormiga::seed_antfarm_transcript()) a.dispatch(cmd);
    maiz::Scene farm = project(a, "antfarm");
    CHECK(farm.nodes.size() == 6);  // core + 5 LOCAL holidays (no cloud default)
    CHECK(farm.wires.size() == 6);  // records×3, assets×1, publisher.site→server

    // ── 5: map actions (one definition → batch → located rune; declines) ───
    a.dispatch("use demo-org");
    maiz::ActionRegistry acts = hormiga::make_map_actions();
    CHECK(acts.manifest().find("\"place\"") != std::string::npos); // discoverable
    maiz::Scene before_map = project(a, "demo-org");
    auto place = acts.run("place", before_map,
                          {{"glyph", "contact"},
                           {"name", "casa-hq"},
                           {"geo", "45.5231,-122.6765"}});
    CHECK(place.size() == 3); // rune new + set geo + tag
    CHECK(a.dispatch(maiz::compile_commit(place)).ok);
    maiz::Scene after_map = project(a, "demo-org");
    const maiz::SceneNode* hq = after_map.find("casa-hq");
    CHECK(hq != nullptr);
    if (hq) {
        CHECK(hormiga::temper::field_value(*hq, "geo").find("45.5231") !=
              std::string::npos);
        CHECK(std::find(hq->tags.begin(), hq->tags.end(), "located") !=
              hq->tags.end());
    }
    // move updates geo; place declines a taken name; run() declines unknowns
    auto mv = acts.run("move", after_map,
                       {{"name", "casa-hq"}, {"geo", "45.6,-122.7"}});
    CHECK(mv.size() == 1); // already tagged located → just the set
    CHECK(a.dispatch(maiz::compile_commit(mv)).ok);
    CHECK(acts.run("place", after_map,
                   {{"glyph", "contact"}, {"name", "casa-hq"},
                    {"geo", "1,1"}})
              .empty()); // name taken
    CHECK(acts.run("nope", after_map, {}).empty()); // unknown action
    a.dispatch("undo"); // the whole move is one frame; place likewise
    a.dispatch("undo");
    CHECK(project(a, "demo-org").find("casa-hq") == nullptr);

    // ── 6: the near-query math (the effect's engine) ────────────────────────
    // Portland city hall → Powell's Books ≈ 1.1 km; the geometry must agree
    double d = hormiga::geo_distance_m(45.5155, -122.6793, 45.5231, -122.6812);
    CHECK(d > 700 && d < 1000); // ~850 m as the crow flies
    double zero = hormiga::geo_distance_m(45.5, -122.6, 45.5, -122.6);
    CHECK(zero < 0.001);
    double lat, lon;
    CHECK(hormiga::parse_geo("45.5,-122.6", lat, lon) && lat == 45.5);
    CHECK(!hormiga::parse_geo("not-a-place", lat, lon));

    // ── 7: the JSON-argument round-trip (map RULES corruption regression) ───
    // The dispatcher's arg tokenizer STRIPS bare quote characters, so raw JSON
    // reaches `setjson` with its quotes gone → cJSON rejects it → it lands as a
    // broken string and the map's rules silently vanish (found 2026-07-22, when
    // a rule named with a space refused to apply). The fix: single-quote-wrap
    // structured JSON args (app.cpp json_arg). This test pins BOTH ends: the
    // bare form is provably corrupted, the wrapped form provably survives — so
    // the bug can never quietly return. json_arg is replicated here (it's a
    // pure string transform; keeping the test standalone beats exporting it).
    auto json_arg = [](const std::string& j) {
        std::string out = "'";
        for (char c : j) { if (c == '\'') out += "\\'"; out += c; }
        return out + "'";
    };
    a.dispatch("use demo-org");
    a.dispatch("rune new map rules-probe");
    // a rule whose NAME contains a space — the exact shape that first broke
    const std::string rules_json =
        R"([{"name":"test one","tags":["vip"],"icon":"users","color":"green"}])";
    // BARE: the tokenizer eats the quotes; cJSON can't parse it back
    a.dispatch("setjson rules-probe rules " + rules_json);
    std::string bare = a.dispatch("get rules-probe rules").data;
    CHECK(bare.find("\"name\"") == std::string::npos); // quotes gone → corrupt
    // WRAPPED: quotes survive, JSON parses, the value comes back intact
    a.dispatch("setjson rules-probe rules " + json_arg(rules_json));
    std::string good = a.dispatch("get rules-probe rules").data;
    CHECK(good.find("\"name\"") != std::string::npos);
    CHECK(good.find("test one") != std::string::npos); // the space survived
    CHECK(good.find("\"green\"") != std::string::npos);

    // ── 8: the Builder's grid model (B1) — `doc migrate` converts the demo
    // issue's adjacency chain to rows, and the RENDER ORDER is EQUIVALENT
    // before and after (the pivot's exit test, okf/concepts/sections/builder.md) ─────
    a.dispatch("use issue-demo");
    maiz::Scene issue_before = project(a, "issue-demo");
    auto chain_before = hormiga::doc_chain(issue_before);
    CHECK(chain_before.size() >= 5); // the seeded newsletter is a real chain
    maiz::ActionRegistry docs = hormiga::make_doc_actions();
    CHECK(docs.manifest().find("\"migrate\"") != std::string::npos);
    auto mig = docs.run("migrate", issue_before, {});
    CHECK(!mig.empty()); // every legacy block gets row+col+span (3 sets each)
    CHECK(a.dispatch(maiz::compile_commit(mig)).ok);
    maiz::Scene issue_after = project(a, "issue-demo");
    auto order_after = hormiga::doc_order(issue_after);
    // equivalence: same components, same order, now grid-driven
    std::vector<std::string> names_before, names_after;
    for (auto* n : chain_before)
        if (n->glyph != "document") names_before.push_back(n->name);
    for (auto* n : order_after) names_after.push_back(n->name);
    CHECK(names_before == names_after);
    // re-running migrate is a no-op (all placed)
    CHECK(docs.run("migrate", issue_after, {}).empty());
    // place/move/resize compile against the grid
    auto pl = docs.run("place", issue_after,
                       {{"glyph", "narrative"}, {"name", "b1-note"},
                        {"row", "99"}, {"col", "0"}, {"span", "6"}});
    CHECK(pl.size() == 4); // rune new + row + col + span
    CHECK(a.dispatch(maiz::compile_commit(pl)).ok);
    maiz::Scene issue2 = project(a, "issue-demo");
    CHECK(hormiga::doc_order(issue2).back()->name == "b1-note"); // sorts last
    auto rs = docs.run("resize", issue2, {{"name", "b1-note"}, {"span", "20"}});
    CHECK(rs.size() == 1 &&
          rs[0].find("\"12\"") != std::string::npos); // clamped to the grid
    a.dispatch("undo"); // the whole place is one frame
    CHECK(project(a, "issue-demo").find("b1-note") == nullptr);

    if (failures == 0) {
        std::cout << "OK — sqlite round-trip + csv import + rescue import + "
                     "antfarm colony + map actions + geo math + json-arg + "
                     "doc grid\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
