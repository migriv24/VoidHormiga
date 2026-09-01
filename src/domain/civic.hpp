/* civic.hpp — the civic record's glyphs and its time resolution
 * (okf/concepts/projects/civic-record.md).
 *
 * WHAT THIS IS FOR. Void Reyna's phase-0 exit test is "hand-build one meeting
 * and see whether the data model survives contact with reality." This is that
 * test's vocabulary: three glyphs, a reified term, a reified assertion, and one
 * pure function that answers *what did this provision say on date D*.
 *
 * THE THREE THINGS IT PROVES, each of which was a design claim before it was
 * code:
 *
 *  1. NO TREES. A provision's containment is a `part-of` LINK, so a provision
 *     may sit under two parents (a model code incorporated by reference) and
 *     nothing in the model objects. The tree is a view, drawn from the links.
 *
 *  2. NO TAXONOMY OF CHANGE. There is no `amendment` glyph, no `suspension`
 *     glyph, no `repeal` glyph. There are dated ASSERTIONS with validity
 *     intervals, and the kinds are emergent: an amendment asserts text from D,
 *     a suspension asserts non-application over D1..D2, a repeal is a
 *     suspension with no end.
 *
 *  3. THE RESOLUTION IS A MERGE, NOT A LOOP. `resolve_at` builds one
 *     ConstraintMap per assertion in force and hands them to `maiz::merge` —
 *     the same engine Allomone uses. Which means order-independence,
 *     idempotence and CONFLICT SURFACING are inherited rather than
 *     re-implemented: two amendments adopted on the same day that disagree
 *     produce ⊤, exactly as two Allomone scripts would, and a renderer gets
 *     "" rather than a guess.
 *
 * WHY STRENGTH IS A DATE. `merge`'s Unique law is "highest strength wins;
 * distinct values at equal strength are ⊤". Set strength = the adoption date in
 * days, and that reads exactly as the real rule: **the later adoption governs,
 * and two adoptions on the same day that disagree are a question for a human.**
 * Nothing was invented; the lattice already had the shape.
 *
 * This is ordering by time, which non-linearity.md would normally refuse — and
 * it is the case that page explicitly allows: *"the objection was never to
 * order per se, but to order nobody chose."* A council voting on a date is the
 * most authored order there is.
 *
 * UI-free and header-only: Scene in, answers out, testable with no window.
 */
#pragma once

#include "voidmaiz/annotate.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/scene.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace hormiga {
namespace civic {

// ── glyphs ──────────────────────────────────────────────────────────────────
//
// EDGES CARRY ALMOST NOTHING, which is the first thing hand-building taught us.
// `link` accepts only `--relation`, `--weight` and `--undirected`, and the
// PROJECTION drops even the weight (`maiz::SceneWire` has no weight field). So
// an edge is `(relation, directed)` and nothing more.
//
// Everything richer must be REIFIED as a rune. That is why `term` exists rather
// than a `held` edge with dates on it, and why an assertion is a rune rather
// than an annotated edge. This is a real constraint of the model, it is not a
// workaround, and reification is the standard answer — but it was invisible
// until something was built.

inline void register_glyphs(maiz::Core& core) {
    // The identity that persists. Deliberately thin: almost nothing true about
    // a policy is true of the policy rather than of one of its assertions.
    // NOTE `source_url` and `snapshot` on EVERY civic glyph. They were missing
    // here at first and the failure was the exact one annotate.hpp warns about:
    // **a missing field is silent.** `set opp source_url '...'` succeeded, `get`
    // returned it, and only `project_scene` dropped it — so the rune looked
    // uncited in the app while the transcript plainly cited it. Caught by a test
    // that asserts *every* imported rune carries its citation; nothing else
    // would have noticed.
    core.register_glyph(
        R"({"glyph":"policy","label":"Policy",)"
        R"("fields":["title","citation","body","adopted","source_url","snapshot"],)"
        R"("hints":{"color":"#5c6f8a","face":{"w":210,"h":54},"category":"Civic",)"
        R"__("labels":{"citation":"Citation","body":"Adopting body"}}})__");

    // A numbered piece of a policy. `number` is STORED, not derived from
    // position: the council wrote "3.3.1", every citation in the world points at
    // it, and it must survive its neighbours being renumbered.
    core.register_glyph(
        R"({"glyph":"provision","label":"Provision",)"
        R"("fields":["number","heading","text","source_url","snapshot"],)"
        R"("hints":{"color":"#6f7f96","face":{"w":220,"h":60},"category":"Civic",)"
        R"("editors":{"text":"multiline:70"},)"
        R"__("labels":{"number":"Number","source_url":"Source","snapshot":"Snapshot id"}}})__");

    // ONE DATED ASSERTION. Not "a version of the document" — one statement, by
    // one body, on one date, about one provision.
    //   text    = what it now says ("" with an `until` = a suspension)
    //   from    = the date it takes effect (ISO)      ─┐ VALID time
    //   until   = when it stops (ISO; "" = indefinitely)┘
    //   adopted = the date the body DECIDED (ISO)       DECISION time
    //
    // THREE TIME AXES, AND ONLY TWO OF THEM BELONG ON A RUNE (Q31, answered
    // 2026-08-17):
    //
    //   1. TRANSACTION time — when *we* learned it. Lives on Void Core's
    //      command log and on Reyna's archive record, and NEVER here. Importing
    //      2019's minutes today must not make 2019 look like 2026.
    //   2. VALID time — `from`/`until`. When the assertion is in force out in
    //      the world.
    //   3. DECISION time — `adopted`. When the council voted.
    //
    // `from` was doing jobs 2 and 3 at once, which is invisible while adoption
    // is prospective and immediate — the ordinary case, and the only one this
    // corpus contains. It breaks on a RETROACTIVE amendment, which is entirely
    // ordinary in civic data: an ordinance adopted in March, effective back to
    // January. Two such assertions in force on the same day then carry the same
    // strength and merge to ⊤ — a conflict the record does not actually have,
    // reported to a reader as though the law were unclear.
    //
    // So they are separated. `adopted` is a FIELD rather than a second interval
    // because it is a fact the source states about the act; where the assertion
    // sits in time is `from`/`until`. Absent, it falls back to `from`, so
    // everything already stored keeps its current meaning exactly.
    core.register_glyph(
        R"({"glyph":"revision","label":"Revision",)"
        R"("fields":["summary","text","from","until","adopted","source_url","snapshot"],)"
        R"("hints":{"color":"#7a5cc0","face":{"w":220,"h":60},"category":"Civic",)"
        R"("editors":{"text":"multiline:70","from":"date","until":"date",)"
        R"("adopted":"date"},)"
        R"__("labels":{"from":"In force from","until":"Until",)__"
        R"__("adopted":"Adopted on"}}})__");

    // A person's tenure in a seat. Reified because an edge cannot carry dates —
    // and needed because "who held this in 2023" must be a query, not a replay
    // of the command log (which records when we LEARNED a fact, not when it was
    // true).
    core.register_glyph(
        R"({"glyph":"term","label":"Term of office",)"
        R"("fields":["seat","from","until"],)"
        R"("hints":{"color":"#3f8f6f","face":{"w":190,"h":48},"category":"Civic",)"
        R"("editors":{"from":"date","until":"date"},)"
        R"__("labels":{"seat":"Seat"}}})__");

    // One attributed thing said on the record. Speaker attribution lives HERE
    // rather than on the edge, because a statement has exactly one speaker and
    // because the evidence about that claim (method, confidence) must travel
    // with it — see okf/concepts/projects/civic-record.md and Reyna's attribution.md.
    // `method` is one of: labeled | human | diarized | inferred.
    core.register_glyph(
        R"({"glyph":"statement","label":"Statement",)"
        R"("fields":["text","offset","method","confidence","source_url","snapshot"],)"
        R"("hints":{"color":"#b3592e","face":{"w":230,"h":62},"category":"Civic",)"
        R"("editors":{"text":"multiline:70"},)"
        R"__("labels":{"offset":"At","method":"Attribution"}}})__");
}

// ── dates ───────────────────────────────────────────────────────────────────

/* Days since the civil epoch (Howard Hinnant's days_from_civil). Dates compare
 * as day counts, never as strings, so an interval means what it says. */
inline long civil_days(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + (long)doe - 719468L;
}

/* Parse ISO `YYYY-MM-DD`. Returns false for empty or malformed — an unparseable
 * date is silence, never a guess at what was meant. */
inline bool iso_days(const std::string& s, long& out) {
    int y = 0, m = 0, d = 0;
    if (s.size() < 10) return false;
    if (std::sscanf(s.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = civil_days(y, (unsigned)m, (unsigned)d);
    return true;
}

inline std::string field_of(const maiz::SceneNode& n, const char* key) {
    for (const auto& f : n.fields) {
        if (f.key != key) continue;
        std::string v = f.value_json;
        if (v == "null") return {};
        if (v.size() >= 2 && v.front() == '"') v = v.substr(1, v.size() - 2);
        return v;
    }
    return {};
}

// ── who held a seat on a date ───────────────────────────────────────────────

struct Holder {
    std::string contact, seat, term;
};

/* Every seat's holder on `date`. This is VALID time — when it was true in the
 * world — as distinct from the command log's transaction time, which only
 * records when we typed it in. Importing 2019's minutes today must not make
 * 2019 look like 2026, and this is why terms are runes. */
inline std::vector<Holder> holders_on(const maiz::Scene& s, const std::string& date) {
    long d = 0;
    std::vector<Holder> out;
    if (!iso_days(date, d)) return out;
    for (const maiz::SceneNode& n : s.nodes) {
        if (n.glyph != "term") continue;
        long from = 0, until = 0;
        if (!iso_days(field_of(n, "from"), from) || from > d) continue;
        std::string u = field_of(n, "until");
        if (!u.empty() && iso_days(u, until) && until <= d) continue;
        Holder h;
        h.term = n.name;
        h.seat = field_of(n, "seat");
        for (const maiz::SceneWire& w : s.wires) // term --holder--> contact
            if (w.relation == "holder" && w.from == n.name) h.contact = w.to;
        out.push_back(std::move(h));
    }
    std::sort(out.begin(), out.end(),
              [](const Holder& a, const Holder& b) { return a.seat < b.seat; });
    return out;
}

// ── what a provision said on a date ─────────────────────────────────────────

struct Resolved {
    maiz::Merged merged;               // the composed answer, conflicts and all
    std::vector<std::string> in_force; // the revision runes that applied
};

/* Compose every assertion in force on `date` into one answer per provision.
 *
 * THE WHOLE MECHANISM IS `maiz::merge`, and that is the point. Each assertion
 * becomes a one-cell ConstraintMap whose STRENGTH is its adoption date in days,
 * so the Unique law reads as the real rule — the later adoption governs, and
 * two adoptions on the same day that disagree are ⊤ rather than a coin flip.
 *
 * TWO DIFFERENT DATES DO TWO DIFFERENT JOBS here, and conflating them was the
 * bug Q31 found: `from`/`until` decide WHETHER an assertion applies on `date`,
 * and `adopted` decides WHICH ONE WINS among those that do. They coincide
 * whenever a council adopts something effective immediately, which is why the
 * distinction stayed invisible; they diverge on a retroactive amendment, and
 * there ranking by `from` gives two assertions equal strength and reports a
 * conflict the record does not have.
 *
 * A suspension is not a special case: it is an assertion with an empty `text`
 * and an `until`. In force, it wins on strength like anything else and the
 * provision resolves to nothing, which is what "suspended" means.
 *
 * Pure: no clock is consulted, `date` is an argument. Order-independent and
 * idempotent, inherited from the merge. */
inline Resolved resolve_at(const maiz::Scene& s, const std::string& date) {
    Resolved r;
    long d = 0;
    if (!iso_days(date, d)) return r;

    std::vector<maiz::ConstraintMap> sources;
    for (const maiz::SceneNode& n : s.nodes) {
        if (n.glyph != "revision") continue;
        long from = 0, until = 0;
        if (!iso_days(field_of(n, "from"), from) || from > d) continue;
        std::string u = field_of(n, "until");
        if (!u.empty() && iso_days(u, until) && until <= d) continue;

        // Decision time, falling back to the effective date. The fallback is
        // what keeps every assertion stored before 2026-08-17 meaning exactly
        // what it meant then: for an immediate adoption the two are equal, so
        // the fallback is not an approximation, it is the same number.
        long decided = from;
        const std::string a = field_of(n, "adopted");
        if (!a.empty()) iso_days(a, decided);

        // Which provisions does this assertion speak about?
        for (const maiz::SceneWire& w : s.wires) {
            if (w.relation != "amends" || w.from != n.name) continue;
            maiz::ConstraintMap m;
            m.id = n.name;
            m.recency = (int)decided;
            // strength = the DECISION date. Later governs; same-day
            // disagreement is a real question. See the header note.
            m.set(w.to, "text", field_of(n, "text"), (int)decided,
                  a.empty() ? field_of(n, "from") : a);
            sources.push_back(std::move(m));
        }
        r.in_force.push_back(n.name);
    }
    maiz::MergeOptions o;
    o.lattices = {{"text", maiz::Lattice::Unique, {}}};
    r.merged = maiz::merge(sources, o);
    std::sort(r.in_force.begin(), r.in_force.end());
    return r;
}

// ── the provision graph, which is NOT a tree ────────────────────────────────

/* The containers of a provision — plural, deliberately. A provision may be
 * `part-of` more than one parent (a model code incorporated by reference is the
 * ordinary case), which is precisely what a tree cannot represent and why
 * containment is a link. A caller drawing a tree must decide what to do with a
 * second parent; the MODEL declines to decide for it. */
inline std::vector<std::string> containers_of(const maiz::Scene& s,
                                              const std::string& provision) {
    std::vector<std::string> out;
    for (const maiz::SceneWire& w : s.wires)
        if (w.relation == "part-of" && w.from == provision) out.push_back(w.to);
    std::sort(out.begin(), out.end());
    return out;
}

/* Direct children, in stored `number` order — the tree as a VIEW, derived from
 * links each time it is drawn and never stored. */
inline std::vector<const maiz::SceneNode*> children_of(const maiz::Scene& s,
                                                       const std::string& parent) {
    std::vector<const maiz::SceneNode*> out;
    for (const maiz::SceneWire& w : s.wires) {
        if (w.relation != "part-of" || w.to != parent) continue;
        if (const maiz::SceneNode* n = s.find(w.from)) out.push_back(n);
    }
    std::sort(out.begin(), out.end(),
              [&](const maiz::SceneNode* a, const maiz::SceneNode* b) {
                  return field_of(*a, "number") < field_of(*b, "number");
              });
    return out;
}

// ── votes ───────────────────────────────────────────────────────────────────

struct Tally {
    int yes = 0, no = 0, abstain = 0, absent = 0;
    bool carried() const { return yes > no; }
};

/* Votes are EDGES — `contact --voted-yes--> revision` — so a tally is a scan
 * and "who votes together" is a graph question over machinery that exists. */
inline Tally tally_of(const maiz::Scene& s, const std::string& revision) {
    Tally t;
    for (const maiz::SceneWire& w : s.wires) {
        if (w.to != revision) continue;
        if (w.relation == "voted-yes") ++t.yes;
        else if (w.relation == "voted-no") ++t.no;
        else if (w.relation == "abstained") ++t.abstain;
        else if (w.relation == "absent") ++t.absent;
    }
    return t;
}

/* How often two people voted the same way — the question a graph makes cheap
 * and a report makes expensive. Counts only decisions (yes/no), because
 * agreeing to be absent is not agreement. */
inline int agreement_between(const maiz::Scene& s, const std::string& a,
                             const std::string& b) {
    int same = 0;
    for (const maiz::SceneWire& wa : s.wires) {
        if (wa.from != a) continue;
        if (wa.relation != "voted-yes" && wa.relation != "voted-no") continue;
        for (const maiz::SceneWire& wb : s.wires)
            if (wb.from == b && wb.to == wa.to && wb.relation == wa.relation) ++same;
    }
    return same;
}

// ── the publishability line ─────────────────────────────────────────────────

/* May this statement's speaker attribution be published?
 *
 * Only `labeled` (the source document said so) and `human` (a person
 * confirmed it). `diarized` and `inferred` are visible in the app and never on
 * the website — a wrong attribution there is a false statement about a real
 * person, not a data-quality issue. Same shape as the internal-notes rule:
 * enforced at a seam, testable, not left to template convention. */
inline bool attribution_publishable(const maiz::SceneNode& statement) {
    std::string m = field_of(statement, "method");
    return m == "labeled" || m == "human";
}

} // namespace civic
} // namespace hormiga
