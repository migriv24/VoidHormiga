/* reyna_import_smoke.cpp — the SEAM test: does a Void Reyna transcript actually
 * become a valid civic model?
 *
 * Reyna's own tests prove its archive holds evidence and its parser finds
 * provisions. This proves the last hop — that what it *emits* replays into
 * Hormiga and produces something the Civic Record window can read. Neither side
 * can check that alone, and a seam nobody tests is a seam that drifts.
 *
 * IT USES A REAL TRANSCRIPT, checked in at tests/data/. It was produced by
 * running Reyna over Springfield's genuine Operating Policies PDF — so this is
 * real government structure, not a mock of one. What it is NOT is a claim about
 * any person: the document is a set of procedural rules, it names no votes and
 * attributes nothing to anyone, which is exactly why it is safe to commit.
 *
 * If Reyna changes what it emits and stops being importable, this fails here
 * rather than in front of a user.
 */
#include "../src/domain/civic.hpp"

#include "voidmaiz/project.hpp"

#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

namespace civic = hormiga::civic;

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

int main(int argc, char** argv) {
    const std::string path =
        argc > 1 ? argv[1] : std::string(TEST_DATA_DIR) + "/opp.voidscript";
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "cannot open transcript: " << path << "\n";
        return 1;
    }

    maiz::Core core;
    civic::register_glyphs(core);

    // The SAME filter the app applies (app.cpp: import_reyna_transcript). A
    // transcript is executable input from outside, so only model-building verbs
    // pass — and the test enforcing the identical rule is the point: if the app
    // ever widens it by accident, this notices.
    static const std::set<std::string> allowed = {
        "mantle", "rune", "set", "setjson", "link", "tag", "relate", "place"};

    std::vector<std::string> cmds;
    std::string line;
    int refused = 0;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (!allowed.count(line.substr(0, line.find(' ')))) { ++refused; continue; }
        cmds.push_back(line);
    }
    CHECK(cmds.size() > 500); // a real policy is not three commands
    CHECK(refused == 0);      // Reyna should not be emitting refused verbs at all
    std::cout << cmds.size() << " commands, " << refused << " refused\n";

    for (const std::string& c : cmds) {
        maiz::Result r = core.dispatch(c);
        if (!r.ok) {
            ++failures;
            std::cerr << "FAIL dispatch: " << c.substr(0, 90) << " -> " << r.text() << "\n";
            if (failures > 5) { std::cerr << "(stopping)\n"; break; }
        }
    }

    maiz::ProjectOptions po;
    po.mantle = "civic";
    maiz::Scene s = maiz::project_scene(core, po);

    // ── it produced a civic model, not just runes ──────────────────────────
    int policies = 0, provisions = 0, revisions = 0, cited = 0;
    for (const maiz::SceneNode& n : s.nodes) {
        if (n.glyph == "policy") ++policies;
        if (n.glyph == "provision") ++provisions;
        if (n.glyph == "revision") ++revisions;
        // EVERY rune must carry its citation. A civic dataset whose claims
        // cannot be traced back to a document is a rumour with better
        // typography, and `Transcript.cite` exists so forgetting is impossible.
        if (!civic::field_of(n, "snapshot").empty() &&
            !civic::field_of(n, "source_url").empty())
            ++cited;
    }
    CHECK(policies == 1);
    CHECK(provisions > 100);
    CHECK(revisions > 100); // one dated assertion per provision that has text
    CHECK(cited == (int)s.nodes.size()); // no exceptions, not one
    std::cout << policies << " policy, " << provisions << " provisions, "
              << revisions << " revisions, " << cited << "/" << s.nodes.size()
              << " cited\n";

    // ── the containment GRAPH came through ─────────────────────────────────
    CHECK(!civic::children_of(s, "opp").empty());
    const maiz::SceneNode* notif = nullptr;
    for (const maiz::SceneNode& n : s.nodes)
        if (n.glyph == "provision" && civic::field_of(n, "number") == "3.3.1")
            notif = &n;
    CHECK(notif != nullptr); // the real document has a 3.3.1
    if (notif) {
        std::vector<std::string> up = civic::containers_of(s, notif->name);
        CHECK(up.size() == 1);
        if (up.size() == 1)
            CHECK(civic::field_of(*s.find(up[0]), "number") == "3.3");
        std::cout << "3.3.1 '" << civic::field_of(*notif, "heading")
                  << "' nests under 3.3\n";
    }

    // ── and it resolves as of a date, which is the whole point ─────────────
    civic::Resolved before = civic::resolve_at(s, "2020-01-01");
    civic::Resolved after = civic::resolve_at(s, "2026-08-16");
    CHECK(before.in_force.empty());   // the assertion is dated 2025-10-06
    CHECK(!after.in_force.empty());
    if (notif) {
        CHECK(before.merged.value(notif->name, "text").empty());
        CHECK(!after.merged.value(notif->name, "text").empty());
        std::cout << "3.3.1 as of 2026: "
                  << after.merged.value(notif->name, "text").substr(0, 70) << "...\n";
    }
    CHECK(after.merged.conflicts().empty()); // one source cannot disagree with itself

    if (failures == 0) {
        std::cout << "OK - a Reyna transcript replays into a valid civic model\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
