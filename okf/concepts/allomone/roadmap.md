---
type: Concept
title: Allomone — roadmap
description: "The phased build plan A–F for Allomone, from a first Condition→Appearance loop on Data cards to the full language, output domain, and migrating the map/calendar rules onto the engine. Allomone gets its own tab (q21e)."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). The build order follows the
dependency structure: prove the smallest end-to-end loop first, then widen the
palette, then add time, then the language, then outputs, then unify.

> **⚠ PHASES A–D ARE NOW UPSTREAM'S, 2026-08-10.** Allomone moved into Void
> Maiz ([adoption](/concepts/allomone/adoption.md)), so the language phases
> below are no longer ours to build — the grammar, the evaluator, layering and
> specificity, and the script surface all shipped in `voidmaiz_allomone`, and
> what they shipped is more than these phases described (a real composition
> engine with ⊤ as a first-class conflict, rather than a specificity winner).
> **Reactivity (C) did not ship and is not planned upstream** — derivation is a
> full recompute per frame, fine at our scale.
>
> **What remains ours** is E and F plus the domain library, and they are
> unchanged in substance:
>
> - **E — the output domain**, through the privacy seam. Untouched by the move.
> - **F — unify**: migrate the map/calendar rules onto Allomone. This got
>   *easier*, because "several independent rule sets composing" is now the
>   engine's native shape rather than something we would have had to invent.
> - **The domain library** — more predicates, more properties, the user graph if
>   we ever want `with`/`device`, inline editor widgets.
>
> **Materialization** (below) is upstream's tiers 1–3, and tier 3 —
> self-modifying scripts — is **refused there as a theorem**, not deferred: a
> system that generates rules to restore symmetry is Knuth–Bendix completion,
> which may not terminate. The sanctioned shape is propose-don't-generate, and
> the Weaver already does it.

# Home: its own tab (q21e — answered)

The author's call (2026-08-03): **Allomone gets its own section/tab** — it is too
large to live as a Data Tools entry. It joins Data / Builder / Antfarm / Territory
/ Calendar as a dockable section ([workspace & sections](/concepts/sections/workspace-and-sections.md)),
with the block canvas as its centerpiece (palette | canvas | inspector | a live
"what this styles now" preview). A quick entry point may still appear in other
tabs ("edit the rules affecting this view"), but the engine's home is the
Allomone tab.

# Phases

**A — Substrate & the first loop.** Split into two increments so the runtime is
proven before the block canvas (which depends on the Void Maiz block asks):

- **A1 — the derive loop (BUILT 2026-08-03).** The `rule` glyph, the engine
  (`rule_color_for`: enabled rules → `node_matches` → specificity winner,
  **derive-only**), the Data-card tint, and a **minimal Allomone tab** whose
  condition editor is the **reused `draw_tag_filter` chip bar** (not blocks yet)
  and whose one action is `set card color`. Exit test met: a `type:org` rule
  colors org cards live, replayable.
- **A1.1 — dedicated mantle (BUILT 2026-08-03).** Rules moved to their **own
  `allomone` mantle**; the engine caches a projection of them
  (`refresh_allo_rules` each reproject) so evaluation is **decoupled from the
  active tab** (and can style any surface). Rule edits go through `allo_cmd`
  (wrapped `use allomone … use <prev>`). Verified: the count is correct with the
  Allomone tab itself active (`type:org` → 72). *Remaining proxy:* specificity is
  still a literal-count approximation of the concept's subsumption + logical
  depth.
- **> BLOCKS RETIRED (author, 2026-08-04).** After both the node-graph blocks
(A2.1) and the custom stack editor, the author judged blocks the wrong direction
entirely and pivoted to a **text IDE** — Allomone Scripts. **Built:** the
`script` glyph, the text editor IDE (copy/paste/cut, enable/disable), and the
**v0 parser** (`rule`/`when`/`color`) feeding the same derive-only engine
(verified: colors Data cards live; a `field_value`/`setjson` newline round-trip
bug was found and fixed with `unjson_str`). **Next:** the fuller Lua-flavored
grammar (variables, `if/else`, more effects/conditions) + a serializer
(rules→text) for full round-tripping ([language](/concepts/allomone/language.md)).
The block phases below are **historical** (superseded).

**A2.1 — the block canvas (BUILT 2026-08-04, then RETIRED).** A real Void Maiz `edit_canvas`
  in the Allomone tab (Blocks/Form toggle): block glyphs `allo_when`/`allo_hastag`
  /`allo_setcolor` (`shape:block`, prev/next flow ports) that snap into a
  statement stack; a palette (Shift+A); faces showing in-block values; a color
  widget editor + block inspector. **The engine reads the snapped stack**
  (`block_stack_below`) → `AlloRule`, so blocks drive the derive-only coloring.
  Verified: `When → has tag type:contact → set color` tints contact cards live.
  *Conditions are stacked statements (ANDed), not hexagonal boolean slots — by
  design, to not block on upstream.*
- **A2.2 — REPLACED by the custom block editor (author, 2026-08-04).** The A2.1
  `edit_canvas` blocks were judged terrible (node-graph paradigm mismatch). Rather
  than chase the upstream refinements, we **build our own** Scratch-style stack
  editor — see [block-editor](/concepts/allomone/block-editor.md). The **model
  stays runes + commands** (CLI-logged, Void-Script-buildable, Void-Core-rooted);
  only the **view** is replaced (a custom vertical-stack renderer with in-slot
  widgets, like the map's bespoke canvas). Build steps: simplify the block glyphs
  to a `parent`/`order`/`slot` tree → `draw_allomone_stack` custom renderer →
  palette + drag gestures → engine reads the tree → `if/else` nesting. The
  hexagon-slots / in-block-widget upstream asks are folded into "a standalone
  block-editor primitive is a future Void Maiz responsibility" — we offer ours as
  the reference (`MESSAGE_FOR_VOIDMAIZ_hormiga-allomone-blocks`).

**B — Control & more actions.** `if`/`if-else` C-blocks; more **Appearance**
actions across **map + calendar** via renderer packs (domain-neutral annotations
→ per-(glyph × domain) interpretation). **Layering & specificity** resolution —
overlap as a feature, no warnings ([conflicts](/concepts/allomone/conflicts.md)).
Still **derive-only** — no writes to the model.

**C — Reactivity.** Delta triggers (`on tag change`) off the dispatcher stream
(incremental, always on); then the **opt-in application clock** behind
`ui.allomone.clock` for loops/animation ([reactivity](/concepts/allomone/reactivity.md)).

**D — The language.** Operators, variables, lists, custom blocks
([blocks](/concepts/allomone/blocks.md)); **Allomone Script** text surface
(projectional, blocks ⇆ text; [language](/concepts/allomone/language.md));
save/load/enable/disable scripts on the database; `.allo` import/export.

**E — Output domain & Antfarm.** Content-management rules at the export seam
(newsletter/website first), **late-bound** to connected holidays via the Antfarm
graph, **through the privacy seam** ([domains](/concepts/allomone/domains.md)).

**F — Unify.** Migrate the **map/calendar rules** onto Allomone; retire the
bespoke editors once parity holds (specificity/layering, per-view scope). Until
F, those engines are **frozen** — maintained, not extended (q21a).

**(far future, maturity-gated) — Materialization.** Only once Allomone is
proven — with **sandboxing, dry-run/preview, error prevention, and testing** —
does it earn the ability to **write** the model (create tags, fill fields, add
links). Everything above is **derive-only**: it changes how the app *looks*,
never the underlying data ([domains](/concepts/allomone/domains.md)). Mixing
writes with enable/disable + the clock is a soundness minefield, so this stays
off the near-term map entirely (author, 2026-08-03).

# Near-term increment ladder (author-driven, 2026-08-05)

The phases above are the strategic arc; this is the **concrete build order** for
the language itself, chosen so each increment unlocks the next. **Functions are
the keystone** — recursion, reuse, imports, and the R data-verbs all sit on them.

0. **A public test dataset — BUILT 2026-08-05.** The [cat dataset](/concepts/projects/cat-dataset.md)
   (`tools/catgen.cpp`): 50 tag-rich fictional cats + linked birthday events, so
   Allomone is exercised on data we can freely reshape instead of a real
   organization's private data. Engineered with the set-theory example baked in (orange males carry a
   discoverable `{lasagna, mondays}` set).
1. **Functions + recursion + termination budget — BUILT 2026-08-05.**
   `function/return`, calls, mutual recursion (hoisted), `for x in list`, `* / %`,
   builtins (`head`/`tail`/`push`/…), and a **fuel + call-depth guard** so the
   live editor can never hang ([language](/concepts/allomone/language.md)).
2. **A rune is a first-class value — BUILT 2026-08-05.** The `thing` keyword is
   retired for **Void Core vocabulary**: `rune` (instance) + `glyph` (type).
   `for each rune do` binds `rune`; runes pass to functions; `rune.tags`/`.glyph`/
   `.name` / `rune has "t"` / `rune is contact` read them. Unblocks generic
   dispatch (Q23), link traversal, and the set-theory example.
2b. **Set comprehension + quantifiers — BUILT 2026-08-05.** `runes` (all runes),
   `runes where <pred>` (filter), `tags_of` / `union` / `intersect` / `minus` /
   `overlaps`, and `any`/`all` (∃/∀, incl. function predicates). The garfield
   two-hop query runs on the cat dataset ([language](/concepts/allomone/language.md)
   "Set theory & quantifiers"). **Execution/logging philosophy captured**:
   derivation is an unlogged projection, only mutation is a logged dispatch
   ([execution](/concepts/allomone/execution.md)).
3. **Links / connections as inputs — BUILT 2026-08-05.** `neighbours` / `linked` /
   `cluster` / `degree`, traversing **by structure, not by name** — the general
   case Allomone is built for ([traversal](/concepts/allomone/traversal.md)). The
   richer host-computed graph measures (centrality/betweenness/closeness/
   community) are the next rung; the **editor vision** (friendly-vs-raw,
   name↔id references + intellisense, inline widgets) is captured in
   [editor](/concepts/allomone/editor.md) as additive view-layer work.
4. **Archetypes + generic dispatch ("classes", Q23).** Named predicate views
   (`define Partner as …`) and verbs that dispatch on archetype/tags (R S3/S4).
   Depends on (2).
5. **Modules / imports (Q24).** A script is a module; `import "other"` binds its
   exports and records a **link** between the two script runes; cycles rejected.
   Depends on functions (1) + an export marker.
6. **The R data-verb / pipe layer.** `things |> where(...) |> summarise(...)` —
   the piped query sublanguage over the rune set
   ([language](/concepts/allomone/language.md) "data-verb layer"). Builds on
   (1)–(3).
7. **More derive effects** (`highlight`/`icon`/`emphasis`/`size`) across map +
   calendar (phase B), still derive-only.
8. **Delta reactivity, then the opt-in clock** (phase C —
   [reactivity](/concepts/allomone/reactivity.md)). The clock is **designed, not
   built**; the budget from (1) is its safety prerequisite.

# Dependencies & risks

- **A depends on** the tag-filter builder (done) and Void Maiz's block kind
  (shipped) — low risk.
- **The renderer-pack seam** (per glyph × domain) must accept a generic
  annotation channel; it already exists for the builder — verify it generalizes
  in B.
- **The clock** (C) is the one place we add global cost; it is opt-out by
  construction, so the risk is contained.
- **Conflict resolution** (B→later) and **the full language** (D) are each large;
  ship the minimal honest version first, note the deep version.

Open sub-decisions and their leans live at
[developer questions](/developer_questions.md) (Q21 / q21a–f); explanations of the
trickier ideas at [developer_explanations](/developer_explanations.md).
