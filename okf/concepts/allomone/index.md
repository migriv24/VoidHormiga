---
type: Concept
title: Allomone — the rules engine
description: "Allomone is Hormiga's rules engine and visual scripting language: a declarative, rule-based system over the rune graph (a production / graph-rewriting system, not Scratch's actor model), edited as typed blocks with a dual textual surface (Allomone Script) that lowers to Void Script. This folder is its full specification."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

> # ⚠ ALLOMONE IS ITS OWN PROJECT — read [adoption](/concepts/allomone/adoption.md) first
>
> **The language lives in `../VoidAllomone` (2026-08-29).** It was Hormiga's,
> then Void Maiz's, and is now its own repository — the extraction was prompted
> by a second consumer that is not C++, and it reached us as forwarding headers
> in Void Maiz, so **Hormiga compiled unchanged.** We still write `maiz::Script`
> and `maiz::allo_eval`; those names now resolve through
> `voidmaiz/allomone.hpp` into `allomone::`.
>
> The one behavioural change: `with` and `device` stopped being *kernel*
> predicates and became host predicates that Void Maiz registers. A kernel that
> needed one host's data structure was a DSL wearing a language's clothes.
> Hormiga never adopted the user graph, so the two never matched here and still
> never do — now for the ordinary reason that an unregistered predicate is
> silent.
>
> **The authoritative specification is `../VoidAllomone/okf/concepts/start-here.md`,
> then `host-protocol.md`.** Design questions about the *language* go there.
> `editor.md` and `user-graph.md` stayed with Void Maiz, because a widget canvas
> and an attention graph were never part of a language.
>
> ---
>
> ## How it got here (2026-08-10)
>
> **Allomone stopped being Hormiga's language before it left Void Maiz.** We
> proposed moving it up on
> 2026-08-06; the author agreed, Void Maiz built it, and the handover landed
> 2026-08-10. The grammar, the parser, the evaluator, the composition engine
> (7 merge laws, ⊤ as a first-class conflict, provenance, `explain_cell`) and
> the analysis layer are **upstream's**. What is ours is a small domain library
> — `src/domain/hormiga_allomone.hpp`: subjects, our property vocabulary and its merge
> laws, twenty-two domain predicates, our glyphs — and the tab.
>
> **[adoption](/concepts/allomone/adoption.md) is the current page.** Everything
> below it in this folder was written between 2026-08-03 and 2026-08-06 and
> describes the language *we* were building. It is kept because the reasoning is
> what earned the adoption — four of our five design calls came back adopted and
> two came back **corrected**, which is the better outcome. Read it as history.
> The upstream specification is authoritative:
> `../VoidAllomone/okf/concepts/start-here.md`, then `host-protocol.md`.
>
> The original Hormiga interpreter is **frozen, not deleted**
> (`src/domain/allomone_legacy.hpp`): it still derives, as one producer feeding the
> same merge, so no stored `script` rune broke on the day the language moved.
> The two dialects are told apart by **glyph** — `script` legacy, `allo-script`
> current.
>
> ---
>
> **Status (historical, to 2026-08-06).** Named + specified 2026-08-03. **Reframed (author, 2026-08-04):
> Allomone is HORMIGA'S SCRIPTING LANGUAGE**, not merely a rules engine — a
> general, Lua-flavored, typed language over the whole database, of which
> derive-only styling is the first use (the design + type system:
> [language](/concepts/allomone/language.md)). **The blocks are RETIRED —
> Allomone is a TEXT IDE.** A script is a text document (enable/disable-able),
> edited in a monospace text editor (copy/paste/cut/undo, live syntax-highlight
> preview + parse diagnostics). **Built (2026-08-04): a REAL interpreter** —
> tokenizer + parser + tree-walking evaluator (`src/domain/allomone_legacy.hpp`): `local`
> variables, lists, `for each`, `if/elseif/else`, `count`, `matching`, `is`,
> `thing:color` — verified running the counting example over the data (v0's
> line-parser is superseded). **Derive-only is a DEV GATE, not a language
> invariant** (mutation is designed, gated). **Editor tightened (2026-08-05):**
> the highlighted view and the editable text are now **one box** (transparent
> `InputTextMultiline` + colored overlay + manual caret, synced via
> `GetInputTextState`); a shared **tag editor** now sits on contacts, orgs, and
> **scripts themselves**; a new **`day` glyph** lets you **tag a calendar day**
> with no event (it flows into scripts as an ordinary thing). Design directions
> captured that day: **links/connections as a distinct primitive**
> ([inputs](/concepts/allomone/inputs.md) §3) and an **R/tidyverse-inspired
> data-verb layer** ([language](/concepts/allomone/language.md)). **Grew a lot
> (2026-08-05): functions + recursion + a fuel budget; then the language dropped
> `thing` for VOID CORE VOCABULARY — a `rune` (instance) typed by its `glyph` is
> a first-class value (`for each rune`, pass to functions, `rune.tags`/`.glyph`/
> `has`/`is`), plus SET THEORY & QUANTIFIERS (`runes where …`, `tags_of`,
> `union`/`intersect`/`minus`/`overlaps`, `any`/`all`). The author's two-hop
> garfield set-query runs on the cat colony.** Execution model named:
> **deriving is an unlogged projection; only mutation is a logged dispatch**
> ([execution](/concepts/allomone/execution.md)). Next: link traversal, generic
> dispatch (Q23), the R data-verb layer, the
> dedicated code editor + inline IntelliSense widgets. See the
> [roadmap](/concepts/allomone/roadmap.md); the block docs
> ([block-editor](/concepts/allomone/block-editor.md),
> [blocks](/concepts/allomone/blocks.md)) are **superseded** but kept for the
> record. Allomone will **subsume the map/calendar rules engine** and reach
> **every tab**. Index below.

# The name

An **allomone** is a *semiochemical* — a chemical signal — emitted by an
organism to its own benefit, and characteristically read by a receiver of a
**different species**, which changes its behavior in response. It sits in the
same family as the **pheromone** (an intra-species signal), the family behind
**stigmergy** — the ant-colony coordination the whole engine is modeled on
([paradigm](/concepts/allomone/paradigm.md)).

The name is the design in one word. Where **tags are pheromones** (intra-colony
signals coordinating Hormiga's own data), an **Allomone rule emits a signal that
a different *domain* — the map, the calendar, the website, a future output
holiday, each a different "species" of surface — reads and responds to in its
own way** ([domains](/concepts/allomone/domains.md)). One signal, many species'
responses. Coordination is **stigmergic**: signals are written to and read from
the shared environment (the rune graph), never message-passed.

# Two documentation tracks (author, 2026-08-05)

Allomone's docs serve **two distinct audiences**, and we keep **both**, even
where they repeat:

- **For a Hormiga USER** who wants to *write* scripts (a "programmer of Hormiga",
  not a developer of it): **[guide](/concepts/allomone/guide.md)** — a practical,
  example-driven walkthrough of the whole language, plus the shipped library of
  commented example scripts in the cat colony (`01-hello-allomone` …
  `09-color-by-glyph`, seeded disabled). Start here if you just want to make
  scripts.
- **For a Hormiga DEVELOPER** (us): how the language is *computed* and how it
  *manipulates* Hormiga — [language](/concepts/allomone/language.md) (design +
  the real grammar), [execution](/concepts/allomone/execution.md) (derive =
  unlogged projection vs mutate = logged dispatch),
  [traversal](/concepts/allomone/traversal.md) (graph traversal **by structure,
  not by name**), [editor](/concepts/allomone/editor.md) (the friendly-vs-raw /
  name↔id / inline-widget editor vision — a view layer, engine intact),
  [foundations](/concepts/allomone/foundations.md) / [paradigm](/concepts/allomone/paradigm.md)
  (the interaction-net model), [inputs](/concepts/allomone/inputs.md) /
  [domains](/concepts/allomone/domains.md) (I/O), and the rest of this folder.

# One paragraph

Allomone's **input is the database itself** — the rune graph of a mantle: tags
(first), fields, links/relations, and derived graph-analytic measures — plus
**deltas** (tag changes) as reactive triggers. Its **output is per-domain
annotations** that renderer packs interpret (a card's color, a marker's icon, a
calendar chip, which content a holiday publishes). It is **edited as typed
blocks** (a Scratch-style shape grammar) with an **isomorphic textual surface,
Allomone Script**, both projections of one AST that **lowers to Void Script**
(the dispatcher command language — our IR). It is **not** Scratch's paradigm:
Scratch is imperative and object/actor-oriented; Allomone is **declarative and
set-at-a-time**, a **production / graph-rewriting system** whose runes never
coordinate by message — only **stigmergically**, through shared tags.

# The specification (this folder)

- **[adoption](/concepts/allomone/adoption.md) — CURRENT.** What changed when
  Allomone moved into Void Maiz: the two dialects and why glyph rather than a
  flag tells them apart; what our domain library owns and the evaluator it must
  never grow; the property vocabulary and its merge laws; the twenty-two domain
  predicates and the purity rule that governs them (and why `today` is frozen);
  the frozen legacy interpreter as one producer among several; what we
  deliberately have not taken yet. **Every page below this one is history.**
- [foundations](/concepts/allomone/foundations.md) — the research grounding:
  Allomone as an **external DSL** (own syntax/AST/tooling) over Void Core's
  **interaction-net** substrate; what to take from the DSL literature (LSP-style
  tooling) and the interaction-net literature (Lafont's combinators; Mackie's
  language-for-inets; Jiresch / Fernández–Mackie on I/O = the Antfarm boundary).
- [intellisense](/concepts/allomone/intellisense.md) — Hormiga-native editor
  affordances: inline color picker, tag dropdown, type-driven completion, list
  multi-select, live diagnostics — LSP-shaped, shared with Notes.
- [paradigm](/concepts/allomone/paradigm.md) — what Allomone *is*, academically:
  production-rule systems, Datalog, graph transformation / interaction nets,
  stigmergy; and the precise contrast with Scratch (set-at-a-time declarative vs
  object-at-a-time imperative).
- [inputs](/concepts/allomone/inputs.md) — the database as input: tags, fields,
  links, graph-spectral measures (a rune's eigenvalue), and change **deltas**.
- [domains](/concepts/allomone/domains.md) — the outputs: **effect domains**,
  renderer packs per (glyph × domain), **derive vs materialize**, **shared vs
  local** scope, and the output/Antfarm intersection.
- [effects](/concepts/allomone/effects.md) — the **codomain catalog**: every
  visual aspect a rule can change, per surface (Data cards first, then map,
  calendar, fonts) — the domain-neutral annotation set and its interpretation.
- [blocks](/concepts/allomone/blocks.md) — the visual language: Scratch's typed
  shape grammar carried over, the block categories, operators, variables, lists,
  custom blocks, and `if`/`if-else`.
- [block-editor](/concepts/allomone/block-editor.md) — **the blocks overhaul
  (2026-08-04):** a purpose-built custom stack editor (like the map), NOT the
  node-graph `edit_canvas`; the rule as a `parent`/`order`/`slot` rune tree (the
  AST); view replaced, model (runes + commands) kept.
- [block-catalog](/concepts/allomone/block-catalog.md) — the **buildable spec**:
  each concrete block and each in-slot **value widget** (tag chips, dropdowns,
  color swatches, number fields), with a worked example.
- [reactivity](/concepts/allomone/reactivity.md) — the computational-model split:
  a quiescent least-fixpoint by default, delta-reactivity always on, and an
  **opt-in application clock** for loops/animation (q21c).
- [language](/concepts/allomone/language.md) — **Allomone Script**: the textual
  surface, projectional editing (blocks ⇄ text), and lowering to Void Script.
- [conflicts](/concepts/allomone/conflicts.md) — **layering & specificity**:
  overlapping rules as a *feature* (defaults + exceptions), resolved by
  specificity/"logical depth", no warnings; scripts as stored objects,
  enable/disable (q21d).
- [roadmap](/concepts/allomone/roadmap.md) — the phased build plan A–F and the
  own-tab decision.
- [tag-recommender](/concepts/allomone/tag-recommender.md) — **built
  2026-08-05**: suggest tags over the tag co-occurrence graph (the substrate run
  in reverse), in three settings-selectable modes — similarity, dissimilarity,
  comprehensive (connect stranded nodes).

Plain-language explanations of the trickier ideas (derive vs materialize,
conflict resolution, round-tripping, reactivity) live in the tutor doc
[developer_explanations](/developer_explanations.md).

# Relationship to the shipped rules (q21a — answered)

Hormiga already has a **narrow rules engine** in the **map** and **calendar**
(tag → color/icon on one surface; "first-rule-wins" + shadowing detection).
**Allomone will replace both.** Per the author (2026-08-03): **do not delete them
now, but do not develop them further** — they are **frozen** (maintenance only).
All rules-engine effort goes to Allomone; the map/calendar rules migrate onto it
once it reaches parity ([roadmap](/concepts/allomone/roadmap.md) phase F), then
retire. Until then they run in parallel, sharing the one tag grammar.
