/* civic_smoke.cpp — Void Reyna's phase-0 exit test, run against Hormiga.
 *
 * THE GATE: "hand-build one meeting and see whether the data model survives
 * contact with reality." There is no point emitting commands for a model nobody
 * has stress-tested, so this is the thing that has to pass before Void Reyna
 * starts.
 *
 * SYNTHETIC PEOPLE, REAL STRUCTURE. The council here is fictional. The document
 * structure is not: it is modelled on Springfield's actual *Operating Policies
 * and Procedures, October 2025* — the nested numbering, §9.5.1 naming
 * supersession, and §10's split between permanent amendment and temporary
 * suspension are all read from the real PDF.
 *
 * The people are invented **on purpose, and the reason is this project's own
 * design rule.** Committing fabricated votes attributed to real named officials
 * would be exactly the harm okf/concepts/projects/civic-record.md exists to prevent — a
 * false statement about a person, published, in a public repo. The Cat Dataset
 * precedent is the same: synthetic data for tests, real data only at runtime.
 * So the STRUCTURE is verified against reality and the CLAIMS are fictional,
 * which is the combination that tests the model without libelling anyone.
 *
 * What it proves, each of which was a design claim before it was a test:
 *   1. containment is a graph, and a shared provision does not break it
 *   2. there is no taxonomy of change — amendment and suspension are the same
 *      mechanism with different fields
 *   3. "what did it say on date D" is a projection, and it is `maiz::merge`
 *   4. two same-day amendments that disagree are ⊤, not a coin flip
 *   5. valid time is not transaction time
 *   6. votes as edges make agreement a graph question
 *   7. an inferred attribution cannot reach the website
 */
#include "../src/domain/civic.hpp"
#include "../src/domain/seed.hpp" // the shipped civic demo must not drift from this test

#include "voidmaiz/project.hpp"

#include <iostream>
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

static maiz::Core core;
static void run(const std::string& cmd) {
    maiz::Result r = core.dispatch(cmd);
    if (!r.ok) {
        ++failures;
        std::cerr << "FAIL dispatch: " << cmd << "  -> " << r.text() << "\n";
    }
}
/* Single-quoted: Void Core escapes only \' inside them and passes everything
 * else through, including the double quotes and newlines real text carries. */
static std::string arg(const std::string& s) {
    std::string out = "'";
    for (char c : s) { if (c == '\'') out += "\\"; out += c; }
    return out + "'";
}
static void set(const std::string& rune, const std::string& key, const std::string& v) {
    run("set " + rune + " " + key + " " + arg(v));
}

int main() {
    civic::register_glyphs(core);
    core.register_glyph(
        R"({"glyph":"contact","label":"Contact","fields":["role","email"]})");
    core.register_glyph(
        R"({"glyph":"event","label":"Meeting","fields":["date","venue"]})");

    // ── the colony: a fictional council, a real document shape ─────────────
    run("mantle new civic");
    for (const char* who : {"ada-brenner", "boone-castellanos", "cyd-nakamura",
                            "dov-eriksson", "esme-whitfield"})
        run(std::string("rune new contact ") + who);

    // TERMS ARE RUNES, because an edge carries only (relation, directed) — no
    // dates. Reification is forced by the model, and it is the right answer:
    // one person, many terms, and "who sat here in 2023" stays a query.
    struct T { const char* id; const char* who; const char* seat;
               const char* from; const char* until; };
    for (const T& t : std::vector<T>{
             {"t-mayor-1", "ada-brenner", "mayor", "2019-01-01", "2023-01-01"},
             {"t-mayor-2", "boone-castellanos", "mayor", "2023-01-01", ""},
             {"t-ward1-1", "cyd-nakamura", "ward-1", "2019-01-01", ""},
             {"t-ward2-1", "dov-eriksson", "ward-2", "2019-01-01", "2024-06-30"},
             {"t-ward2-2", "esme-whitfield", "ward-2", "2024-07-01", ""}}) {
        run(std::string("rune new term ") + t.id);
        set(t.id, "seat", t.seat);
        set(t.id, "from", t.from);
        if (*t.until) set(t.id, "until", t.until);
        run(std::string("link ") + t.id + " " + t.who + " --relation holder");
    }

    // ── the policy, as a GRAPH of provisions ───────────────────────────────
    run("rune new policy opp");
    set("opp", "title", "Council Operating Policies and Procedures");
    set("opp", "citation", "OPP");
    set("opp", "body", "Common Council");

    struct P { const char* id; const char* num; const char* head; const char* parent; };
    for (const P& p : std::vector<P>{
             {"opp-3",     "3",     "Regular Meetings",         "opp"},
             {"opp-3-3",   "3.3",   "Mayor and Councilor Attendance", "opp-3"},
             {"opp-3-3-1", "3.3.1", "Notification",             "opp-3-3"},
             {"opp-3-3-3", "3.3.3", "Remote Participation",     "opp-3-3"},
             {"opp-9",     "9",     "Boards and Commissions",   "opp"},
             {"opp-9-5-3", "9.5.3", "Public Meetings Law",      "opp-9"}}) {
        run(std::string("rune new provision ") + p.id);
        set(p.id, "number", p.num);
        set(p.id, "heading", p.head);
        set(p.id, "source_url", "https://example.gov/opp-2025-10.pdf");
        set(p.id, "snapshot", "a1b2c3d4e5f60718");
        run(std::string("link ") + p.id + " " + p.parent + " --relation part-of");
    }
    // (1) A SHARED PROVISION. §9.5.3 restates the state Public Meetings Law, so
    // it belongs to the boards chapter AND to the meetings chapter. A tree
    // cannot hold this; the graph does not even notice.
    run("link opp-9-5-3 opp-3 --relation part-of");

    maiz::ProjectOptions po;
    po.mantle = "civic";
    maiz::Scene s = maiz::project_scene(core, po);

    CHECK(civic::containers_of(s, "opp-9-5-3").size() == 2);
    CHECK(civic::containers_of(s, "opp-3-3-1").size() == 1);
    // The tree is a VIEW: §3's children, in stored number order, including the
    // borrowed one.
    std::vector<const maiz::SceneNode*> kids = civic::children_of(s, "opp-3");
    CHECK(kids.size() == 2);
    if (kids.size() == 2) {
        CHECK(civic::field_of(*kids[0], "number") == "3.3");
        CHECK(civic::field_of(*kids[1], "number") == "9.5.3");
    }

    // ── (2) change, with no taxonomy of change ─────────────────────────────
    // Four assertions. None of them is an "amendment object" or a "suspension
    // object" — they differ only in their fields.
    struct R { const char* id; const char* summary; const char* text;
               const char* from; const char* until; const char* about; };
    for (const R& r : std::vector<R>{
             {"rev-2019", "Original adoption",
              "A councilor shall notify the Recorder before the meeting.",
              "2019-02-04", "", "opp-3-3-1"},
             // an amendment: new text, no end
             {"rev-2025a", "Notification window extended to 24 hours",
              "A councilor shall notify the Recorder at least 24 hours before "
              "the meeting.",
              "2025-10-06", "", "opp-3-3-1"},
             // a suspension: no text, and an end date. Same glyph.
             {"rev-2026-susp", "Remote participation suspended pending review",
              "", "2026-01-05", "2026-07-01", "opp-3-3-3"},
             // and something for §3.3.3 to fall back to
             {"rev-2019-remote", "Original adoption",
              "A councilor may participate remotely with the President's consent.",
              "2019-02-04", "", "opp-3-3-3"}}) {
        run(std::string("rune new revision ") + r.id);
        set(r.id, "summary", r.summary);
        if (*r.text) set(r.id, "text", r.text);
        set(r.id, "from", r.from);
        if (*r.until) set(r.id, "until", r.until);
        set(r.id, "source_url", "https://example.gov/minutes/");
        set(r.id, "snapshot", "0f1e2d3c4b5a6978");
        run(std::string("link ") + r.id + " " + r.about + " --relation amends");
    }
    s = maiz::project_scene(core, po);

    // ── (3) "what did it say on date D" is a projection ────────────────────
    civic::Resolved before = civic::resolve_at(s, "2024-01-01");
    civic::Resolved after = civic::resolve_at(s, "2026-02-01");
    CHECK(before.merged.value("opp-3-3-1", "text").find("24 hours") == std::string::npos);
    CHECK(after.merged.value("opp-3-3-1", "text").find("24 hours") != std::string::npos);
    std::cout << "2024: " << before.merged.value("opp-3-3-1", "text") << "\n";
    std::cout << "2026: " << after.merged.value("opp-3-3-1", "text") << "\n";

    // The suspension: in force in Feb 2026, expired by August. No special case
    // anywhere — an empty text won on strength, which IS "suspended".
    CHECK(after.merged.value("opp-3-3-3", "text").empty());
    civic::Resolved later = civic::resolve_at(s, "2026-08-01");
    CHECK(later.merged.value("opp-3-3-3", "text").find("remotely") != std::string::npos);
    std::cout << "suspended 2026-02: [" << after.merged.value("opp-3-3-3", "text")
              << "]  restored 2026-08: "
              << later.merged.value("opp-3-3-3", "text").substr(0, 24) << "...\n";

    // Order-independence and idempotence, inherited from the merge rather than
    // re-implemented — the two equalities everything rests on.
    CHECK(civic::resolve_at(s, "2026-02-01").merged.cells.size() ==
          after.merged.cells.size());

    // ── (4) two same-day amendments that disagree are ⊤ ────────────────────
    // The real clerk's problem: two motions adopted at one meeting that both
    // rewrite 3.3.1. Nothing should pick a winner.
    run("rune new revision rev-2025b");
    set("rev-2025b", "summary", "Notification window extended to 48 hours");
    set("rev-2025b", "text",
        "A councilor shall notify the Recorder at least 48 hours before the meeting.");
    set("rev-2025b", "from", "2025-10-06"); // SAME DAY as rev-2025a
    run("link rev-2025b opp-3-3-1 --relation amends");
    s = maiz::project_scene(core, po);

    civic::Resolved clash = civic::resolve_at(s, "2026-02-01");
    std::vector<const maiz::MergedCell*> conflicts = clash.merged.conflicts();
    CHECK(conflicts.size() == 1);
    CHECK(clash.merged.value("opp-3-3-1", "text").empty()); // never a guess
    if (conflicts.size() == 1) {
        CHECK(conflicts[0]->subject == "opp-3-3-1");
        std::cout << "conflict: " << conflicts[0]->subject << " — ";
        for (const maiz::CellVerdict& v : maiz::explain_cell(*conflicts[0]))
            std::cout << v.source << " ";
        std::cout << "\n";
    }
    // And an amendment adopted LATER settles it, with no human needed — the
    // lattice already knew that a sharper opinion clears a disagreement.
    run("rune new revision rev-2026-fix");
    set("rev-2026-fix", "summary", "Reconciling the October amendments");
    set("rev-2026-fix", "text",
        "A councilor shall notify the Recorder at least 24 hours before the meeting.");
    set("rev-2026-fix", "from", "2026-03-02");
    run("link rev-2026-fix opp-3-3-1 --relation amends");
    s = maiz::project_scene(core, po);
    civic::Resolved fixed = civic::resolve_at(s, "2026-04-01");
    CHECK(fixed.merged.conflicts().empty());
    CHECK(fixed.merged.value("opp-3-3-1", "text").find("24 hours") != std::string::npos);

    // ── (4b) a RETROACTIVE amendment: decision time ranks, valid time filters ──
    // Q31, answered 2026-08-17. Both of these are in force on the query date and
    // both take effect the same day, so ranking by `from` makes them equally
    // strong and reports ⊤ — a conflict the record does not have. The council
    // decided the second one three weeks later; the second one governs.
    //
    // This is not a hypothetical shape: "adopted in March, effective back to
    // January" is ordinary in civic data, and it is exactly the case where the
    // two dates that used to be one field come apart.
    run("rune new revision rev-retro-a");
    set("rev-retro-a", "summary", "Retroactive notification rule, first pass");
    set("rev-retro-a", "text",
        "A councilor shall notify the Recorder at least 72 hours before the meeting.");
    set("rev-retro-a", "from", "2027-01-01");     // effective
    set("rev-retro-a", "adopted", "2027-03-10");  // decided, retroactively
    run("link rev-retro-a opp-3-3-1 --relation amends");
    run("rune new revision rev-retro-b");
    set("rev-retro-b", "summary", "Retroactive notification rule, as corrected");
    set("rev-retro-b", "text",
        "A councilor shall notify the Recorder at least 96 hours before the meeting.");
    set("rev-retro-b", "from", "2027-01-01");     // SAME effective date
    set("rev-retro-b", "adopted", "2027-03-31");  // decided three weeks later
    run("link rev-retro-b opp-3-3-1 --relation amends");
    s = maiz::project_scene(core, po);

    civic::Resolved retro = civic::resolve_at(s, "2027-06-01");
    CHECK(retro.merged.conflicts().empty());   // ranking by `from` alone => ⊤
    CHECK(retro.merged.value("opp-3-3-1", "text").find("96 hours")
          != std::string::npos);
    std::cout << "retroactive pair resolves to: "
              << retro.merged.value("opp-3-3-1", "text").substr(0, 46) << "…\n";

    // And valid time still FILTERS independently of it: asked before either took
    // effect, neither applies, however recently the council decided them.
    CHECK(civic::resolve_at(s, "2026-12-31").merged.value("opp-3-3-1", "text")
              .find("hours") != std::string::npos);   // the 2026 rule still
    CHECK(civic::resolve_at(s, "2026-12-31").merged.value("opp-3-3-1", "text")
              .find("96 hours") == std::string::npos); // ...and not the retro one

    // ── (5) valid time is not transaction time ─────────────────────────────
    // Everything above was entered in one session, so replaying the log would
    // say "2026" for all of it. The terms know better.
    std::vector<civic::Holder> in2021 = civic::holders_on(s, "2021-06-01");
    std::vector<civic::Holder> in2026 = civic::holders_on(s, "2026-06-01");
    CHECK(in2021.size() == 3 && in2026.size() == 3);
    auto seat_of = [](const std::vector<civic::Holder>& hs, const char* seat) {
        for (const civic::Holder& h : hs) if (h.seat == seat) return h.contact;
        return std::string();
    };
    CHECK(seat_of(in2021, "mayor") == "ada-brenner");
    CHECK(seat_of(in2026, "mayor") == "boone-castellanos");
    CHECK(seat_of(in2021, "ward-2") == "dov-eriksson");
    CHECK(seat_of(in2026, "ward-2") == "esme-whitfield");
    std::cout << "mayor in 2021: " << seat_of(in2021, "mayor")
              << " | in 2026: " << seat_of(in2026, "mayor") << "\n";

    // ── (6) votes as edges ─────────────────────────────────────────────────
    run("rune new event mtg-2025-10-06");
    set("mtg-2025-10-06", "date", "2025-10-06");
    run("link rev-2025a mtg-2025-10-06 --relation adopted-at");
    for (const char* who : {"boone-castellanos", "cyd-nakamura"})
        run(std::string("link ") + who + " rev-2025a --relation voted-yes");
    run("link dov-eriksson rev-2025a --relation voted-no");
    run("link esme-whitfield rev-2025a --relation absent");
    s = maiz::project_scene(core, po);

    civic::Tally t = civic::tally_of(s, "rev-2025a");
    CHECK(t.yes == 2 && t.no == 1 && t.absent == 1 && t.carried());
    CHECK(civic::agreement_between(s, "boone-castellanos", "cyd-nakamura") == 1);
    CHECK(civic::agreement_between(s, "boone-castellanos", "dov-eriksson") == 0);
    std::cout << "rev-2025a: " << t.yes << "-" << t.no << " carried\n";

    // ── (7) an inferred attribution cannot reach the website ───────────────
    run("mantle new meeting-2025-10-06"); // statements get their own mantle
    run("rune new statement st-0001");
    set("st-0001", "text", "I move we adopt the amendment as read.");
    set("st-0001", "offset", "00:41:12");
    set("st-0001", "method", "labeled"); // the minutes said so
    run("rune new statement st-0002");
    set("st-0002", "text", "Twenty-four hours is not enough for working members.");
    set("st-0002", "offset", "00:43:05");
    set("st-0002", "method", "diarized"); // a model grouped it; nobody named it
    set("st-0002", "confidence", "0.61");

    maiz::ProjectOptions mp;
    mp.mantle = "meeting-2025-10-06";
    maiz::Scene ms = maiz::project_scene(core, mp);
    const maiz::SceneNode* a = ms.find("st-0001");
    const maiz::SceneNode* b = ms.find("st-0002");
    CHECK(a && civic::attribution_publishable(*a));
    CHECK(b && !civic::attribution_publishable(*b));
    // Confirming is an ordinary command, so it is logged, attributed, undoable.
    run("set st-0002 method \"human\"");
    ms = maiz::project_scene(core, mp);
    CHECK(civic::attribution_publishable(*ms.find("st-0002")));
    run("undo");
    ms = maiz::project_scene(core, mp);
    CHECK(!civic::attribution_publishable(*ms.find("st-0002")));

    // ── (8) the SHIPPED seed produces the same model ───────────────────────
    // The window reads `seed_civic_transcript()`, so if that transcript drifts
    // from the model this file tests, the demo silently degrades and nobody
    // notices until they open it. Replay it into a fresh core and check the
    // properties the UI depends on.
    {
        maiz::Core fresh;
        hormiga::register_glyphs(fresh);
        for (const std::string& cmd : hormiga::seed_civic_transcript()) {
            maiz::Result rr = fresh.dispatch(cmd);
            if (!rr.ok) {
                ++failures;
                std::cerr << "FAIL seed: " << cmd << " -> " << rr.text() << "\n";
            }
        }
        maiz::ProjectOptions cp;
        cp.mantle = "civic";
        maiz::Scene c = maiz::project_scene(fresh, cp);

        CHECK(civic::containers_of(c, "opp-9-5-3").size() == 2); // the shared one
        // The seeded clash is live today and unsettled — which is the point of
        // seeding it: the demo shows a ⊤ rather than describing one.
        civic::Resolved now = civic::resolve_at(c, "2026-08-16");
        CHECK(now.merged.conflicts().size() == 1);
        CHECK(now.merged.value("opp-3-3-1", "text").empty());
        // The suspension has expired by then, so remote participation is back.
        CHECK(now.merged.value("opp-3-3-3", "text").find("remotely") !=
              std::string::npos);
        // ...and was in force in February.
        CHECK(civic::resolve_at(c, "2026-02-01").merged.value("opp-3-3-3", "text").empty());
        // Terms answer valid time.
        CHECK(civic::holders_on(c, "2021-06-01").size() == 3);
        CHECK(civic::tally_of(c, "rev-2025-24h").carried());
        std::cout << "seed: " << c.nodes.size() << " runes, " << c.wires.size()
                  << " links, 1 live conflict\n";
    }

    if (failures == 0) {
        std::cout << "OK - the civic model survived one hand-built meeting\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
