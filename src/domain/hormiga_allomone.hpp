/* hormiga_allomone.hpp — Hormiga's DOMAIN Allomone: the small library the
 * Void Maiz host protocol asks every adopter to write
 * (../VoidMaiz/okf/concepts/allomone/host-protocol.md).
 *
 * WHY THIS EXISTS. Allomone used to be ours — a Lua-flavoured interpreter in
 * src/allomone_legacy.hpp that walked the runes and set a colour directly. It
 * moved into Void Maiz on 2026-08-10 and became a base language with a
 * composition engine underneath it: several independent sources each state what
 * *should* be true, a lattice merge composes them, and a genuine disagreement
 * across sources becomes ⊤ — a first-class conflict a human settles with a
 * logged command — instead of a winner picked by evaluation order.
 *
 * The engine, the grammar and the analysis are all upstream's now. What stays
 * ours is the part only we can know:
 *
 *   1. SUBJECTS      — what an Allomone rule is allowed to talk about here.
 *   2. PROPERTIES    — our vocabulary, and each one's MERGE LAW.
 *   3. PREDICATES    — our condition words: dates, geography, roles, the graph.
 *   4. MEANING       — turning a derived string into a card colour.
 *
 * The governing rule from the protocol is *"discover the vocabulary, declare
 * the interactions"*: we do not enumerate every property a script may write
 * (a property exists because some rule writes it), we declare the ones where
 * COMBINING is the meaning. Everything undeclared is `Unique`, which surfaces
 * disagreement rather than inventing a combination rule nobody asked for.
 *
 * NO EVALUATOR LIVES HERE, deliberately. The protocol's warning — *"if your
 * domain library is growing an evaluator, something has gone wrong"* — is the
 * test this file has to keep passing. Anything missing is a
 * MESSAGE_FOR_VOIDMAIZ_*, not a local workaround.
 *
 * HEADLESS AND UI-FREE. No ImGui, no app state. `tests/allomone_smoke.cpp`
 * drives the whole loop — glyphs, subjects, laws, predicates, derive, conflict,
 * settle — with no window, which is what keeps the app's rendering honest: if
 * a derivation needs the GUI to be correct, it is not a derivation.
 */
#pragma once

#include "voidmaiz/allomone.hpp"
#include "voidmaiz/annotate.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/scene.hpp"

#include <memory>
#include <string>
#include <vector>

namespace hormiga {
namespace allomone {

/* Where scripts and resolutions live. Both are model content — runes reached by
 * dispatcher commands — so editing a script and settling a conflict are logged,
 * attributed, replayable and undoable like every other gesture. */
inline constexpr const char* kMantle = "allomone";

/* The glyph of a Void-Maiz-dialect script. The dialects are told apart by GLYPH
 * rather than by a flag on one glyph: a `script` rune is the frozen legacy
 * interpreter's (imperative, `for each rune do … rune:color(…)`), an
 * `allo-script` rune is this one's (declarative, `when has "x" then color "…"`).
 * A stored script therefore cannot change meaning under a migration, and both
 * dialects can derive at once as two independent sources. */
inline constexpr const char* kScriptGlyph = "allo-script";

/* Register every glyph this library's own commands and storage name — ours
 * (`allo-script`) AND the ones UPSTREAM's emitted commands reference
 * (`maiz::resolution_glyph()`). Missing the latter is the failure the handover
 * called out by name: `rune new` says "unknown glyph", or, worse, the name is
 * right and a field is missing, in which case `set` quietly succeeds and only
 * the projection drops the value. Idempotent. */
void register_glyphs(maiz::Core& core);

// ── 1. subjects ─────────────────────────────────────────────────────────────

/* Project the data mantle into subjects. `id` is the rune NAME, which is the
 * one hard requirement of the protocol: annotations, subjects and (if we ever
 * ship one) the user graph must share an address space, so a rule that matched
 * something can annotate what it found.
 *
 * Scaffolding is excluded — maps, calendar views, scripts, block runes. A rule
 * about "every contact" should not have to say "and not a map". */
std::vector<maiz::Subject> subjects_from(const maiz::Scene& data);

/* Is this glyph a thing a rule may talk about? (The exclusion above, exposed so
 * the UI can count what a script covers without rebuilding the subject list.) */
bool is_subject_glyph(const std::string& glyph);

// ── 2. properties and their merge laws ──────────────────────────────────────

/* One declared property: what it means here, and how two opinions about it
 * combine. This list is documentation as much as configuration — the protocol
 * is explicit that the laws, not the grammar, are the real surface area of a
 * domain language, because a script author needs to know that `weight`
 * accumulates while `color` does not. The Allomone tab renders this table. */
struct Property {
    const char* name;
    maiz::Lattice law;
    const char* meaning;
};

/* The declared vocabulary. NOT exhaustive by design: a script may write any
 * property it likes and get `Unique`. */
const std::vector<Property>& vocabulary();

// ── 2b. domains: one annotation, many surfaces ──────────────────────────────
//
// Hormiga's own long-standing concept (okf/concepts/allomone/domains.md): a
// rule states something domain-neutral and each SURFACE interprets it — a card
// tint, a map marker, a calendar chip, an exported page. Void Maiz's engine
// carries properties as opaque strings and has no idea surfaces exist, which
// is correct for a base language and leaves the mapping to us.
//
// So a domain is a PREFIX on the property name, and nothing more:
//
//     when has "urgent" then color "#c0392b"        # every surface
//     when has "urgent" then map-color "#ff0000"    # the map only
//
// Resolution for surface D is `D-<prop>` if present, else plain `<prop>`. That
// is the whole mechanism, and it costs upstream nothing: `map-color` is just
// another property string, merged by the same laws, conflicting in the same
// way, explained by the same inspector. Getting this without an upstream ask
// is the clearest evidence the constraint map was the right seam.
//
// THE SEPARATOR IS `-`, NOT `.`, and that was not the first choice: upstream's
// lexer takes alnum, `_` and `-` in an identifier, so `map-color` tokenizes as
// three things and the rule dies with an Invalid token. `-` reads fine and
// matches the predicate names already in this file (`field-has`, `near-rune`),
// but a dot is what anyone would reach for first, so it is going upstream as a
// bruise rather than being quietly absorbed here.
//
// It is also the first real step of roadmap phase F (migrate the map and
// calendar rules onto Allomone), which got easier rather than harder when the
// engine moved: "several independent rule sets composing" is now native.

/* The surfaces that read derived annotations. `web` is the OUTPUT domain and
 * the one the privacy seam binds to — see `internal_fields()`. */
struct Domain {
    const char* prefix; // "card", "map", "cal", "web"
    const char* what;
};
const std::vector<Domain>& domains();

/* Our laws + policies, ready for merge(). Declares every property both plain
 * and domain-qualified, so `map-weight` accumulates exactly like `weight` —
 * a law that applied to one spelling and not the other would be a trap.
 * `resolutions` is filled by the caller from the resolution runes. */
maiz::MergeOptions merge_options();

/* The settled value of `property` for `domain`: the domain-qualified cell if
 * one exists, otherwise the plain one. Returns "" when absent OR conflicted —
 * a ⊤ cell must never render as a value, and a domain-qualified conflict does
 * NOT silently fall through to the plain property, because "these two sources
 * disagree about the map" is not an argument for showing them the card's
 * answer instead. */
std::string value_for(const maiz::Merged& merged, const std::string& subject,
                      std::string_view domain, std::string_view property);

/* Which cell `value_for` would read — the qualified name if that cell exists,
 * else the plain one. For the UI's "why does this look like this" answer. */
std::string cell_for(const maiz::Merged& merged, const std::string& subject,
                     std::string_view domain, std::string_view property);

// ── 3. predicates: our domain's condition words ─────────────────────────────

/* Everything a domain predicate is allowed to read, computed ONCE per frame and
 * frozen.
 *
 * A predicate MUST be a pure function of its arguments — no clock, no I/O, no
 * mutable state — because an impure one makes the merge depend on *when* it was
 * asked and silently destroys order-independence for every downstream consumer.
 * `today` is the one thing that would tempt us into the clock, so it is read
 * once by the caller and frozen HERE: inside a predicate it is an argument in
 * everything but name, which is exactly the sanctioned shape.
 *
 * Held by shared_ptr because the registry's lambdas capture it, and a registry
 * outliving the scene it was built from is a use-after-free that no test would
 * reliably catch. */
struct Frame {
    struct Row {
        std::string glyph;
        std::vector<std::string> tags;
        /* Decoded field values — with internal-notes-class values BLANKED, see
         * the privacy seam below. What is not in the frame cannot be read out
         * of it by a predicate written next year. */
        std::vector<std::pair<std::string, std::string>> fields;
        std::vector<std::pair<std::string, std::string>> links; // (relation, other)
        double lat = 0, lon = 0;
        bool has_geo = false;
        bool has_internal = false; // carries internal notes — the boolean, only
        int degree = 0;
        double centrality = 0; // eigenvector, normalised to [0,1]
    };
    std::vector<std::pair<std::string, Row>> rows; // by subject id, sorted
    std::string today;                             // "YYYY-MM-DD", frozen
    std::vector<std::string> published;            // sorted; see FrameInputs

    const Row* find(std::string_view id) const;
    /* A field's value, or "" — including for internal-class keys, which read
     * empty by design rather than by absence. */
    std::string field(std::string_view id, std::string_view key) const;
};

/* Everything a frame needs that is not in the data mantle. */
struct FrameInputs {
    /* ISO `YYYY-MM-DD`. Empty disables every date-relative predicate — they
     * then simply never match, which is silence rather than an error, the same
     * stance the kernel takes for a missing user graph. */
    std::string today;
    /* Rune names any Builder document currently references, so a script can ask
     * `published ""`. The Builder lives in another mantle and the derivation
     * must not go looking for it; the caller knows and passes it in. */
    std::vector<std::string> published;
};

std::shared_ptr<const Frame> make_frame(const maiz::Scene& data, FrameInputs in);

// ── the privacy seam ────────────────────────────────────────────────────────
//
// Ground rule 6 and [security](/concepts/platform/security.md) §3: internal-notes-class
// fields never reach an Output-interface holiday, checked at the seam and
// testable rather than left to template convention.
//
// Allomone is derive-only, so it cannot leak a field by writing one. It CAN
// leak by copying: `when all then web.note field-of-notes` would put private
// text into an exported page, and no export-side check would catch it, because
// by then the value is an ordinary derived string with no provenance.
//
// The enforcement is therefore at the READ, not the write, and it is absolute:
// **no predicate can observe the CONTENT of an internal-class field.** The only
// thing a script may learn is the boolean `internal ""` — that this rune has
// internal notes at all — which is exactly what a rule needs to EXCLUDE it from
// an export and never enough to reproduce it. A rule naming an internal field
// gets a diagnostic saying so, rather than silently matching nothing.

/* The internal-notes-class field keys, per glyph-agnostic convention. */
const std::vector<std::string>& internal_fields();
bool is_internal_field(std::string_view key);

/* One documented predicate, for the tab's reference panel and for completion. */
struct PredicateDoc {
    const char* name;
    const char* arg;
    const char* meaning;
};
const std::vector<PredicateDoc>& predicate_docs();

/* Hormiga's condition vocabulary over that frame. Registering a kernel name is
 * ignored upstream, so nothing here can shadow `has`/`glyph`/`rune`/`tag`/
 * `kind`/`name`/`mantle`/`device`/`with`. */
maiz::PredicateRegistry predicates(std::shared_ptr<const Frame> frame);

// ── 4. the derivation ───────────────────────────────────────────────────────

/* What one source turned out to be, for the UI's script list. */
struct SourceInfo {
    std::string id;    // the rune name — also the ConstraintMap id a Resolution names
    std::string label; // display name
    bool enabled = false;
    bool legacy = false; // came in through `extra_sources` (the old interpreter)
    int rules = 0;
    int cells = 0;
    std::vector<maiz::Diagnostic> diagnostics; // parse errors AND unknown predicates
};

struct Options {
    FrameInputs inputs;                           // today, published set
    std::vector<maiz::Resolution> resolutions;    // read back from resolution runes
    /* Sources that are not scripts. `merge()` does not care where a
     * ConstraintMap came from, which is precisely how the frozen legacy
     * interpreter keeps shipping: it becomes ONE PRODUCER among several,
     * feeding a composition engine it no longer owns. That is step 2 of the
     * adoption path in the handover, and it is the step that buys conflict
     * surfacing, provenance and the inspector for free. */
    std::vector<maiz::ConstraintMap> extra_sources;
};

struct Derivation {
    maiz::Merged merged;
    std::vector<maiz::Subject> subjects;
    std::vector<maiz::ConstraintMap> sources;
    std::vector<SourceInfo> scripts;
    std::shared_ptr<const Frame> frame; // kept alive: the registry closed over it
};

/* The whole loop: subjects from the data mantle, scripts read back from the
 * `allomone` mantle, parse → check → eval → merge.
 *
 * Pure with respect to the model: it dispatches nothing and writes nothing.
 * Re-deriving is always safe, and disabling a script removes its effects with
 * nothing to undo, because nothing was ever stored. */
Derivation derive(const maiz::Scene& data, const maiz::Scene& scripts,
                  const Options& opts = {});

/* Read the resolution runes out of the scripts mantle. */
std::vector<maiz::Resolution> resolutions_from(const maiz::Scene& scripts);

// ── 5. meaning: derived strings → what Hormiga draws ────────────────────────

/* Parse "#rrggbb" into 0xAARRGGBB (ImGui's IM_COL32 order), false if malformed.
 * Kept here rather than in the app because it is the domain's encoding — the
 * library carries values as opaque strings and never interprets them. */
bool parse_hex(const std::string& hex, unsigned& rgba);

/* The derived colour for one subject on one surface, or false if none.
 *
 * THE ONE NON-OPTIONAL OBLIGATION of the protocol lives in this function: a
 * conflicted cell must never render as a value. `Merged::value()` already
 * returns "" for ⊤ so a renderer cannot accidentally show an arbitrary winner;
 * this returns false, and the caller shows the conflict or its own default —
 * never a guess. Silently picking one is the single behaviour the whole design
 * exists to eliminate. */
bool color_for(const maiz::Merged& merged, const std::string& subject,
               std::string_view domain, unsigned& rgba);

/* The `badge`/`note` values for a surface, split on the `All` law's ", " join
 * so a renderer gets the list back rather than one long string. */
std::vector<std::string> list_for(const maiz::Merged& merged,
                                  const std::string& subject,
                                  std::string_view domain,
                                  std::string_view property);

/* A numeric property (`weight`, `priority`), or `fallback` when absent,
 * conflicted or unparseable. */
double number_for(const maiz::Merged& merged, const std::string& subject,
                  std::string_view domain, std::string_view property,
                  double fallback = 0.0);

/* A one-line, human-readable account of how a cell settled — "theme (line 4)
 * beat alerts (line 2)", "3 = 1 + 2", "unsettled: theme vs alerts". Built on
 * `explain_cell`, so the classification comes from the merge's own record
 * rather than being re-derived (and got subtly wrong) here. */
std::string explain(const maiz::MergedCell& cell);

/* The starter script a new `allo-script` rune gets. */
const char* starter_script();

// ── 6. teaching the vocabulary ──────────────────────────────────────────────

/* What one word MEANS, for the editor's right-click explain.
 *
 * Completion tells you a word exists; this tells you what it does, which is the
 * difference between a vocabulary you can type and one you can learn. Ours is
 * big — 9 kernel conditions, 22 domain predicates, 8 properties × 4 surfaces —
 * and nobody is going to hold that in their head, so every part of it has to be
 * reachable from the word itself.
 *
 * Lives here rather than in the tab because it is domain KNOWLEDGE, it is the
 * same text the Reference pane shows, and a test can check that every predicate
 * and property we register can explain itself. Returns "" for a word we have
 * nothing to say about. Multi-line is expected — this is a panel, not a
 * tooltip. */
std::string explain_word(std::string_view word);

/* Every word `explain_word` should answer for: keywords, kernel conditions,
 * our predicates, our properties, and every surface-qualified spelling. Used
 * by the completion list and by the test that keeps the two in step. */
std::vector<std::string> vocabulary_words();

} // namespace allomone
} // namespace hormiga
