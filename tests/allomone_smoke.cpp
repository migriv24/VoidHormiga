/* allomone_smoke.cpp — the Allomone adoption test, headless.
 *
 * Void Maiz's `allomone_minimal` proves the LIBRARY's loop works. This proves
 * HORMIGA's loop works: our glyphs, our subjects projected out of a real
 * dispatcher transcript, our merge laws, our domain predicates, and the two
 * behaviours the whole adoption was for —
 *
 *   1. two independent scripts disagreeing produces ⊤ rather than a winner
 *      picked by evaluation order, and the colour a renderer sees is EMPTY; and
 *   2. settling it is a dispatcher command, so it is logged and replayable.
 *
 * It also pins the properties that would break silently: order-independence,
 * idempotence, the within-vs-across split (a sharper rule beating a broader one
 * inside ONE script must NOT be a conflict), the frozen `today` (a date
 * predicate that read the clock would make this test drift into passing and
 * failing by calendar), and the legacy interpreter surviving as one producer
 * among several.
 *
 * Links no view module. If a derivation needs a window to be right, it is not
 * a derivation. */
#include "../src/domain/hormiga_allomone.hpp"
#include "../src/domain/seed.hpp"

#include "voidmaiz/project.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace allo = hormiga::allomone;

static int failures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            ++failures;                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                           \
    } while (0)

/* One command argument, single-quoted: inside single quotes Void Core escapes
 * only \' and passes everything else through literally — including the double
 * quotes an Allomone script is full of, and real newlines. A double-quoted
 * argument could carry neither. */
static std::string arg(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "\\";
        out += c;
    }
    return out + "'";
}

static void script(maiz::Core& core, const std::string& name, const std::string& src,
                   bool enabled = true) {
    CHECK(core.dispatch("rune new allo-script " + name).ok);
    CHECK(core.dispatch("set " + name + " source " + arg(src)).ok);
    CHECK(core.dispatch("set " + name + " enabled \"" + (enabled ? "1" : "0") + "\"").ok);
}

int main() {
    maiz::Core core;

    // ── glyphs: ours, plus the ones UPSTREAM's own commands name ────────────
    CHECK(core.register_glyph(
        R"({"glyph":"contact","label":"Contact","fields":["role","email","geo"]})"));
    CHECK(core.register_glyph(
        R"({"glyph":"event","label":"Event","fields":["date","venue","geo"]})"));
    CHECK(core.register_glyph(R"({"glyph":"map","label":"Map","fields":["title"]})"));
    allo::register_glyphs(core); // allo-script + maiz::resolution_glyph()

    // ── the data mantle: a tiny colony, built through the real dispatcher ───
    CHECK(core.dispatch("mantle new data").ok);
    CHECK(core.dispatch("rune new contact ada").ok);
    CHECK(core.dispatch("tag ada +role:volunteer +coat:orange").ok);
    CHECK(core.dispatch("set ada role \"volunteer\"").ok);
    CHECK(core.dispatch("set ada geo \"40.7128,-74.0060\"").ok);
    CHECK(core.dispatch("rune new contact bo").ok);
    CHECK(core.dispatch("tag bo +role:staff").ok);
    CHECK(core.dispatch("set bo role \"staff\"").ok);
    CHECK(core.dispatch("rune new event potluck").ok);
    CHECK(core.dispatch("set potluck date \"2026-08-20\"").ok);
    CHECK(core.dispatch("rune new event audit").ok);
    CHECK(core.dispatch("set audit date \"2026-05-01\"").ok);
    CHECK(core.dispatch("link ada potluck --relation attends").ok);
    // Scaffolding: a map rune is NOT a subject. A rule about "every contact"
    // must not have to say "and not a map".
    CHECK(core.dispatch("rune new map the-map").ok);

    maiz::ProjectOptions dp;
    dp.mantle = "data";
    maiz::Scene data = maiz::project_scene(core, dp);

    std::vector<maiz::Subject> subjects = allo::subjects_from(data);
    CHECK(subjects.size() == 4); // ada, bo, potluck, audit — not the-map
    for (const maiz::Subject& s : subjects) CHECK(s.kind != "map");

    // ── the scripts. `theme` and `alerts` are INDEPENDENT sources that
    // disagree about ada's colour at equal strength; `theme` also contains a
    // broad default its own sharper rule beats, which must stay SILENT. ─────
    CHECK(core.dispatch("mantle new allomone").ok);
    script(core, "theme",
           "# a palette\n"
           "when all                 then color \"#8a9199\"\n"
           "when glyph \"contact\"     then color \"#2e8b57\"\n"
           "when under \"role:\"       then weight 1\n"
           "define staffer(t) = glyph t and role \"staff\"\n"
           "when staffer(\"contact\")  then badge \"staff\"\n");
    script(core, "alerts",
           "# urgency, written by someone who has never seen `theme`\n"
           "when glyph \"contact\"     then color \"#f85149\"\n"
           "when overdue \"60\"        then priority 3, badge \"overdue\"\n"
           "when upcoming \"30\"       then priority 1\n"
           "when under \"role:\"       then weight 1\n");
    script(core, "off-by-default", "when all then color \"#000000\"\n", false);

    maiz::ProjectOptions sp;
    sp.mantle = "allomone";
    maiz::Scene scripts = maiz::project_scene(core, sp);

    allo::Options opts;
    opts.inputs.today = "2026-08-10"; // FROZEN. A predicate that read the clock would
                               // make this test pass or fail by calendar.
    allo::Derivation d = allo::derive(data, scripts, opts);

    CHECK(d.sources.size() == 2);  // the disabled one contributes nothing
    CHECK(d.scripts.size() == 3);  // but it is still listed, with its rule count
    for (const allo::SourceInfo& s : d.scripts) {
        if (!s.diagnostics.empty())
            for (const maiz::Diagnostic& x : s.diagnostics)
                std::cerr << "  diag " << s.id << ":" << (x.line + 1) << " "
                          << x.message << "\n";
        CHECK(s.diagnostics.empty()); // every predicate we used is registered
    }

    // 1. THE CONFLICT. Two sources, equal strength 1, different colours.
    std::vector<const maiz::MergedCell*> conflicts = d.merged.conflicts();
    CHECK(conflicts.size() == 2); // ada.color and bo.color
    for (const maiz::MergedCell* c : conflicts) {
        CHECK(c->property == "color");
        CHECK(c->value.empty()); // ⊤ renders as NOTHING, never as a guess
        std::cout << "conflict: " << c->subject << "." << c->property << " — "
                  << allo::explain(*c) << "\n";
    }
    unsigned rgba = 0;
    CHECK(!allo::color_for(d.merged, "ada", "card", rgba)); // the renderer gets no colour

    // 2. WITHIN a source is silent: `theme`'s own `when all` default lost to its
    // own contact rule with no conflict, and the events keep the default.
    CHECK(d.merged.value("potluck", "color") == "#8a9199");
    CHECK(allo::color_for(d.merged, "potluck", "card", rgba));

    // 3. THE MERGE LAWS. Both sources say `weight 1` for a role-tagged contact:
    // Sum DEDUPES, so that is 1, not 2 — the gotcha worth pinning in a test
    // rather than discovering in the UI.
    CHECK(d.merged.value("ada", "weight") == "1");
    CHECK(d.merged.value("potluck", "weight").empty());
    CHECK(d.merged.value("bo", "badge") == "staff"); // via `define`
    // Max over priority; the domain predicates decided who gets one.
    CHECK(d.merged.value("audit", "priority") == "3");   // 101 days overdue
    CHECK(d.merged.value("potluck", "priority") == "1"); // 10 days out
    CHECK(d.merged.value("audit", "badge") == "overdue");

    // 4. ORDER-INDEPENDENCE and IDEMPOTENCE — the two equalities everything
    // else rests on. If moving a script changed the output, that is the bug.
    maiz::MergeOptions mo = allo::merge_options();
    std::vector<maiz::ConstraintMap> flipped(d.sources.rbegin(), d.sources.rend());
    std::vector<maiz::ConstraintMap> doubled = d.sources;
    doubled.insert(doubled.end(), d.sources.begin(), d.sources.end());
    maiz::Merged a = maiz::merge(d.sources, mo), b = maiz::merge(flipped, mo),
                 c = maiz::merge(doubled, mo);
    CHECK(a.cells.size() == b.cells.size() && a.cells.size() == c.cells.size());
    for (size_t i = 0; i < a.cells.size() && i < b.cells.size(); ++i) {
        CHECK(a.cells[i].subject == b.cells[i].subject);
        CHECK(a.cells[i].property == b.cells[i].property);
        CHECK(a.cells[i].value == b.cells[i].value);
        CHECK(a.cells[i].conflicted == b.cells[i].conflicted);
    }
    for (size_t i = 0; i < a.cells.size() && i < c.cells.size(); ++i)
        CHECK(a.cells[i].value == c.cells[i].value);

    // 5. SETTLING IS A COMMAND, so it is logged, attributed and replayable.
    for (const std::string& cmd :
         maiz::compile_resolution("allomone", {"ada", "color", "alerts", ""}))
        CHECK(core.dispatch(cmd).ok);
    scripts = maiz::project_scene(core, sp);
    opts.resolutions = allo::resolutions_from(scripts);
    CHECK(opts.resolutions.size() == 1);

    allo::Derivation settled = allo::derive(data, scripts, opts);
    CHECK(settled.merged.value("ada", "color") == "#f85149");
    CHECK(allo::color_for(settled.merged, "ada", "card", rgba));
    CHECK(settled.merged.conflicts().size() == 1); // bo is still open
    if (const maiz::MergedCell* cell = settled.merged.find("ada", "color")) {
        CHECK(cell->settled_by == "resolution");
        std::cout << "settled: ada.color — " << allo::explain(*cell) << "\n";
    }
    // Undo it: a resolution is model content like everything else.
    CHECK(core.dispatch("undo").ok);

    // 6. THE LEGACY INTERPRETER AS ONE PRODUCER. A ConstraintMap built by hand
    // is indistinguishable from a script's, which is the whole reason the old
    // engine can keep shipping while the new one takes over.
    maiz::ConstraintMap hand;
    hand.id = "legacy:01-hello";
    hand.set("potluck", "color", "#3f6fae", 1, "legacy");
    allo::Options with_legacy = opts;
    with_legacy.resolutions.clear();
    with_legacy.extra_sources.push_back(hand);
    allo::Derivation mixed = allo::derive(data, scripts, with_legacy);
    // It disagrees with `theme`'s strength-0 default and is SHARPER, so it wins
    // outright — no conflict, and the account says who beat whom.
    CHECK(mixed.merged.value("potluck", "color") == "#3f6fae");
    if (const maiz::MergedCell* cell = mixed.merged.find("potluck", "color")) {
        CHECK(cell->settled_by == "strength");
        CHECK(cell->source == "legacy:01-hello");
        std::cout << "legacy source: potluck.color — " << allo::explain(*cell) << "\n";
    }
    bool listed = false;
    for (const allo::SourceInfo& s : mixed.scripts)
        if (s.legacy && s.id == "legacy:01-hello") listed = true;
    CHECK(listed);

    // 7. APOSTROPHES SURVIVE the trip through the dispatcher. Inside single
    // quotes args.c's only escape is `\'`; emitting a backslash AND a second
    // quote closes the argument early, so a comment saying "don't" silently
    // lost the rest of the script (Hormiga's json_arg did exactly that until
    // 2026-08-10). A script is prose as much as code, so this is not exotic.
    CHECK(core.dispatch("use allomone").ok);
    script(core, "apostrophe",
           "# don't drop everything after this word\n"
           "when glyph \"contact\" then note \"Ada's card\"\n");
    scripts = maiz::project_scene(core, sp);
    allo::Derivation quoted = allo::derive(data, scripts, {});
    CHECK(quoted.merged.value("ada", "note") == "Ada's card");

    // 8. AN UNKNOWN PREDICATE is a diagnostic, not a parse error — and it kills
    // its rule rather than being read as false.
    script(core, "foreign", "when louder \"0.5\" then color \"#ffffff\"\n");
    scripts = maiz::project_scene(core, sp);
    allo::Derivation with_foreign = allo::derive(data, scripts, {});
    bool reported = false;
    for (const allo::SourceInfo& s : with_foreign.scripts)
        if (s.id == "foreign") {
            CHECK(s.rules == 1); // it PARSED — a foreign script stays readable
            CHECK(s.diagnostics.size() == 1);
            reported = true;
        }
    CHECK(reported);
    CHECK(with_foreign.merged.value("ada", "label").empty());

    // 9. DOMAINS. A property prefixed with a surface reaches only that surface;
    // an unprefixed one reaches all of them. This is Hormiga's own concept
    // (one annotation, many interpretations) riding upstream's opaque property
    // strings with no upstream change, which is the clearest evidence the
    // constraint map was the right seam.
    CHECK(core.dispatch("use allomone").ok);
    script(core, "surfaces",
           "when glyph \"event\"   then color \"#111111\"\n"
           "when glyph \"event\"   then map-color \"#222222\"\n"
           "when glyph \"event\"   then map-weight 2\n");
    scripts = maiz::project_scene(core, sp);
    allo::Derivation dom = allo::derive(data, scripts, opts);
    CHECK(allo::value_for(dom.merged, "potluck", "map", "color") == "#222222");
    CHECK(allo::value_for(dom.merged, "potluck", "cal", "color") == "#111111");
    CHECK(allo::value_for(dom.merged, "potluck", "web", "color") == "#111111");
    CHECK(allo::cell_for(dom.merged, "potluck", "map", "color") == "map-color");
    CHECK(allo::cell_for(dom.merged, "potluck", "cal", "color") == "color");
    // A domain-qualified property carries the SAME law as the plain one — a law
    // that applied to `weight` and not `map-weight` would silently turn a Sum
    // into a Unique and start surfacing conflicts where things accumulated.
    CHECK(allo::merge_options().law_for("map-weight") == maiz::Lattice::Sum);
    CHECK(allo::merge_options().law_for("cal-badge") == maiz::Lattice::All);
    CHECK(allo::number_for(dom.merged, "potluck", "map", "weight") == 2.0);
    CHECK(allo::number_for(dom.merged, "potluck", "card", "weight", -1) == -1);
    // The `All` law's ", " join, split back into items for a renderer.
    std::vector<std::string> badges = allo::list_for(dom.merged, "audit", "card", "badge");
    CHECK(badges.size() == 1 && badges[0] == "overdue");

    // 10. THE PRIVACY SEAM. An internal-notes field is not readable by ANY
    // predicate — not even for equality, because repeated equality tests are a
    // way to reproduce a value. The only observable is `internal ""`, which is
    // what a rule needs to EXCLUDE a rune and never enough to leak one.
    CHECK(allo::is_internal_field("notes"));
    CHECK(!allo::is_internal_field("bio"));
    CHECK(core.register_glyph(
        R"({"glyph":"person","label":"Person","fields":["bio","notes"]})"));
    CHECK(core.dispatch("use data").ok);
    CHECK(core.dispatch("rune new person cyd").ok);
    CHECK(core.dispatch("set cyd bio \"runs the pantry\"").ok);
    CHECK(core.dispatch("set cyd notes \"do not contact before noon\"").ok);
    data = maiz::project_scene(core, dp);
    CHECK(core.dispatch("use allomone").ok);
    script(core, "privacy",
           "when field \"bio\"                     then badge \"public\"\n"
           "when internal \"\"                      then web-hide \"1\"\n"
           "when field \"notes=do not contact before noon\" then badge \"LEAKED\"\n"
           "when field-has \"notes=noon\"           then badge \"ALSO LEAKED\"\n");
    scripts = maiz::project_scene(core, sp);
    allo::Derivation priv = allo::derive(data, scripts, opts);
    CHECK(priv.merged.value("cyd", "badge") == "public"); // NOT the leak badges
    CHECK(priv.merged.value("cyd", "web-hide") == "1");   // but the boolean works
    // And the refusal is VISIBLE. A seam that reads as "your data is wrong" is
    // a bad seam, so naming an internal field is a diagnostic, one per rule.
    int privacy_diags = 0;
    for (const allo::SourceInfo& s : priv.scripts)
        if (s.id == "privacy") privacy_diags = (int)s.diagnostics.size();
    CHECK(privacy_diags == 2);
    // The frame itself does not carry the content — a predicate written next
    // year cannot read what was never stored.
    CHECK(priv.frame->field("cyd", "notes").empty());
    CHECK(priv.frame->field("cyd", "bio") == "runs the pantry");
    const allo::Frame::Row* cyd = priv.frame->find("cyd");
    CHECK(cyd && cyd->has_internal);
    for (const auto& f : cyd->fields)
        if (f.first == "notes") CHECK(f.second.empty());

    // 11. EVERY WORD WE OFFER CAN TEACH ITSELF. The editor's right-click
    // explain is the difference between a vocabulary you can type and one you
    // can learn, and ours is large enough that a gap would be invisible: nine
    // kernel conditions, twenty-two predicates, eight properties across four
    // surfaces. A predicate added without an explanation is the exact failure
    // this catches — it would complete, work, and be unexplainable.
    {
        std::vector<std::string> words = allo::vocabulary_words();
        CHECK(words.size() >= 60);
        int unexplained = 0;
        for (const std::string& w : words)
            if (allo::explain_word(w).empty()) {
                ++unexplained;
                std::cerr << "  no explanation for `" << w << "`\n";
            }
        CHECK(unexplained == 0);
        // A surface-qualified property explains the SURFACE and the law, not
        // just the property — that is the whole reason the spelling exists.
        std::string me = allo::explain_word("map-weight");
        CHECK(me.find("sum") != std::string::npos);
        CHECK(me.find("Territory") != std::string::npos);
        CHECK(allo::explain_word("no-such-word").empty());
        std::cout << "vocabulary: " << words.size()
                  << " words, all explained\n";
    }

    // 12. EVERY SEEDED EXAMPLE PARSES, with every predicate it names registered.
    // A shipped example that stops parsing is the first thing a new user sees,
    // and it would break silently: the script list would just show red.
    {
        maiz::Core fresh;
        hormiga::register_glyphs(fresh); // brings allo-script + the resolution glyph
        for (const std::string& cmd : hormiga::seed_allomone_scripts_transcript())
            fresh.dispatch(cmd);
        maiz::ProjectOptions ep;
        ep.mantle = "allomone";
        maiz::Scene examples = maiz::project_scene(fresh, ep);
        allo::Derivation ed = allo::derive({}, examples, {});
        CHECK(ed.scripts.size() == 10);
        int rules = 0, diags = 0;
        for (const allo::SourceInfo& s : ed.scripts) {
            rules += s.rules;
            diags += (int)s.diagnostics.size();
            CHECK(s.rules > 0);
            CHECK(!s.enabled); // seeded off — enabling one is how you explore
            for (const maiz::Diagnostic& x : s.diagnostics)
                std::cerr << "  example " << s.label << ":" << (x.line + 1) << " "
                          << x.message << "\n";
            CHECK(s.diagnostics.empty());
        }
        std::cout << "examples: " << ed.scripts.size() << " scripts, " << rules
                  << " rules, " << diags << " diagnostic(s)\n";
    }

    if (failures == 0) {
        std::cout << "OK - hormiga allomone adoption: " << subjects.size()
                  << " subjects, " << d.merged.cells.size() << " derived cells\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
