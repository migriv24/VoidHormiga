---
type: Concept
title: Allomone — the adoption (Hormiga as Void Maiz's first host)
description: "What changed when Allomone moved into Void Maiz on 2026-08-10: the two dialects and why they are told apart by glyph, what our domain library owns and what it must never grow, the property vocabulary and its merge laws, the twenty-two domain predicates and the purity rule that governs them, surfaces as a property prefix, the privacy seam enforced at the read, the editor we adopted after building a worse one, how the frozen legacy interpreter keeps shipping as one producer among several, and what we deliberately have not taken yet."
resource: src/domain/hormiga_allomone.hpp
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-10T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). Audience: **anyone touching the
rules engine.** Read this before any other page in this folder — the pages
written between 2026-08-03 and 2026-08-06 describe the language *we* were
building, and the language is no longer ours.

# What happened

We proposed on 2026-08-06 that Allomone did not belong in Hormiga: a scripting
layer that derives over a Void Core mantle is not outreach-specific, and every
feature we wanted next was Void-Maiz-native. **The author agreed and Void Maiz
built it.** The handover arrived 2026-08-10 and is folded into the
[log](/log.md).

The upstream specification is authoritative and lives there. The single page to
read is `../VoidAllomone/okf/concepts/start-here.md`; the contract for a
host is `host-protocol.md`; the shortest working example is
`../VoidMaiz/examples/allomone_minimal.cpp`. **If a claim on any page in this
folder contradicts those, they win.**

What stayed ours is the part only we can know, and it is small on purpose:

    Hormiga                     the tab, the cards, what a property MEANS
    hormiga_allomone            subjects, properties + laws, predicates, glyphs
    voidmaiz_allomone           grammar, parser, evaluator
    voidmaiz                    projection, gestures, merge()
    Void Core                   model, dispatcher, tags, undo, log

# Two dialects, told apart by glyph

There are two languages in the tree, and the distinction is a **glyph, not a
flag on one glyph**:

| glyph | dialect | where | status |
|---|---|---|---|
| `script` | the original Hormiga-local language — imperative, `for each rune do … rune:color(…)` | `src/domain/allomone_legacy.hpp` | **frozen 2026-08-10** |
| `allo-script` | Void Maiz's — declarative, `when has "x" then color "…"` | `voidmaiz_allomone` + `src/domain/hormiga_allomone.*` | **current** |

Using the glyph is what makes the migration safe: **a stored script cannot
change meaning**, because nothing re-interprets an old body under new rules. The
twelve seeded legacy examples still parse and still derive; any script in an
existing database still works. A flag would have made every one of them a
guess.

The legacy interpreter is **read-only history** — no new features, no fixes
beyond crashes. When the last legacy script is migrated it can be deleted
outright, and nothing else has to change.

# What the domain library supplies

`src/domain/hormiga_allomone.hpp`, built as the `hormiga_allomone` target, linking
`voidmaiz_allomone` and **no view module** — which is a real claim, pinned by
`tests/allomone_smoke.cpp` running the whole loop with no window. If a
derivation needs the GUI to be correct, it is not a derivation.

## 1. Subjects

The data mantle's runes, addressed **by rune name**, because annotations,
subjects and (if we ever ship one) the user graph must share one address space —
the protocol's one hard requirement. Scaffolding is excluded: maps, calendar
views, map shapes, both script glyphs, resolutions, the retired block runes. A
rule about "every contact" should not have to say "and not a map".

## 1b. Surfaces — one annotation, many interpretations

[domains](/concepts/allomone/domains.md) has always said an annotation is
domain-neutral and the *interpretation* is per (glyph × domain). Void Maiz's
engine carries properties as opaque strings and has no idea surfaces exist,
which is correct for a base language. So a domain is **a prefix on the property
name, and nothing more**:

    when has "urgent" then color "#c0392b"       # every surface
    when has "urgent" then map-color "#ff0000"   # the map only

Resolution for surface D is `D-<prop>` if that cell exists, else plain `<prop>`.

| prefix | surface |
|---|---|
| `card-` | the Data list and card view — the interior, default surface |
| `map-` | Territory: marker colour and icon, drawn-geometry colour |
| `cal-` | the Calendar: day and event chips |
| `web-` | the OUTPUT domain — newsletter, website, and the Builder preview |

Four things about it worth writing down:

- **It cost upstream nothing.** `map-color` is just another property string,
  merged by the same laws, conflicting the same way, explained by the same
  inspector. Getting a whole Hormiga concept with no upstream ask is the
  clearest evidence the constraint map was the right seam.
- **Every property is declared BOTH ways**, so `map-weight` accumulates exactly
  like `weight`. A law that applied to one spelling and not the other would be
  a trap of the silent kind — the qualified spelling would quietly fall back to
  `Unique` and start surfacing conflicts where things used to add up.
- **A conflicted qualified cell does NOT fall through to the plain one.** "Two
  sources disagree about the map" is not an argument for showing them the
  card's answer instead; substituting a value would hide the disagreement
  behind something that looks deliberate.
- **The separator is `-`, not `.`.** That was not the first choice: upstream's
  lexer takes alnum, `_` and `-` in an identifier, so `map.color` tokenizes as
  three things and the rule dies on an Invalid token. A dot is what anyone
  reaches for first, so it went upstream as a bruise rather than being quietly
  absorbed here.

This is **roadmap phase F's first real step** — the map and calendar now read
derived annotations alongside their own bespoke rules, and derived styling wins,
because it is the only one of the three that *composes*. A disagreement among
scripts already resolved to "nothing derived" before it reached the renderer, so
nothing is being overridden that anybody chose.

## 2. Properties and their merge laws

The laws are **the real surface area of the language** for someone writing a
rule — more than the grammar, which is fixed upstream. Someone has to know that
`weight` accumulates and `color` does not, so the Reference pane generates this
table from the same list `merge_options()` is built from and cannot go stale.

| property | law | why | rendered as |
|---|---|---|---|
| `color` | `Unique` | one right answer; two sources disagreeing is a question, not a race | card tint + border, map marker, calendar chip |
| `icon` | `Unique` | — | card corner, map marker glyph, calendar chip |
| `label` | `Unique` | shadows the caption for display; **does not write the field** | card caption, map label |
| `weight` | `Sum` | contributions accumulate | card border thickness, marker radius |
| `priority` | `Max` | the loudest opinion wins | calendar entry order (descending) |
| `badge` | `All` | every source's marker is collected | chips on the card |
| `note` | `All` | why this looks like this | the card's hover tooltip |
| `hide` | `Unique` | see below — **contested does not hide** | `web-hide "1"` drops the rune from every query-backed block |

**Anything undeclared is `Unique`**, which surfaces disagreement rather than
inventing a combination rule nobody asked for. Declaring only ever adds, so the
list is not a gate on writing rules.

Two consequences worth knowing before they bite:

- **`Sum` dedupes.** Two sources both saying `weight 2` total **2**, not 4.
  Wanting 4 is wanting a multiset, which is not idempotent and therefore not a
  lattice.
- **A broad `when all` default is free under `Unique` and is an *ingredient*
  under a combining law.** If we ever declare a `Custom` join, every `when all`
  in every script needs auditing.

**Deriving `label` does not write the rune's `label` field.** Allomone is
derive-only; the derived value shadows the field for display and disabling the
script restores it with nothing to undo. That is a feature (restyle without
touching the model) and a reasonable thing for a user to get wrong.

## 3. The domain predicates

The kernel gives us `has`/`tag`, `glyph`/`kind`, `rune`/`name`, `mantle`, and
the two that read the *person* rather than the data (`with`, `device`). Our
condition vocabulary is everything else, and it exists for the case tags encode
badly: **parameterized or continuous** conditions.

| group | predicates |
|---|---|
| tags & fields | `under "ns:"` · `field "k=v"` · `field-has "k=text"` · `role` · `named-like` · `untagged ""` |
| dates | `before` · `after` · `overdue "30"` · `upcoming "30"` · `undated ""` |
| geography | `near "lat,lon,km"` · `near-rune "name,km"` · `located ""` |
| the graph | `linked "rel"` · `linked-to "name"` · `degree-over "3"` · `central "0.5"` · `isolated ""` |
| output & content | `published ""` · `has-image ""` · `internal ""` |

Notes that are not obvious:

- **Every predicate takes an argument**, including the ones that ignore it:
  `when isolated "" then …`, not `when isolated then …`. Two seeded examples
  shipped without it and the example test caught them.
- **`under` exists because the kernel's `has` is exact** where the legacy
  dialect's `has "ns:"` matched a prefix. That is our tag convention, so it is
  correctly our predicate rather than an upstream gap.
- **`near` is the Earth reading**, and [Territory](/concepts/sections/territory.md) is
  explicit that the map source is swappable and not assumed to be Earth. A
  non-Earth map wants its own predicate, not a rescaled constant.
- **`near-rune "food-bank,5"` names a rune, and that is fine here.** Naming a
  specific rune is normally discouraged — logic should survive the data
  changing — but here the rune *is* the parameter, and "every volunteer within
  5km of the food bank" keeps meaning the same thing after the food bank moves.
- **`published ""` is answered by the caller, not by looking**: the Builder's
  documents live in other mantles and a derivation must not go hunting for one.
  The app resolves every document's direct `ref`s and every query-backed block's
  expression through the same `node_matches` the preview uses, and passes the
  set in as a frame input.
- **An unregistered predicate kills its whole rule, including under `not`** —
  "I don't understand this term" is never read as "this term is false" — and it
  is reported as a *diagnostic*, not a parse error, so a script written for
  another domain stays readable and diffable here.

### The privacy seam, enforced at the READ

Ground rule 6 and [security](/concepts/platform/security.md) §3: internal-notes-class
fields never reach an Output-interface holiday, checked at the seam and
testable rather than left to template convention.

Allomone is derive-only, so it cannot leak a field by *writing* one. It can leak
by **copying**: a rule that put a contact's notes into a derived `web-note`
would land private text in an exported page, and no export-side check would
catch it, because by then the value is an ordinary derived string with no
provenance. Provenance-tracking every string was the obvious fix and the wrong
one — it makes every downstream consumer responsible for a rule it cannot see.

So the enforcement is at the read, and it is absolute:

> **No predicate can observe the CONTENT of an internal-class field.** The only
> thing a script may learn is `internal ""` — that this rune has notes at all.

Three properties make it hold rather than merely be stated:

1. **The content is not in the frame.** `make_frame` blanks internal-class
   values and keeps only the boolean. A predicate written next year cannot read
   what was never carried, so this is not a guard a new predicate can forget to
   call.
2. **Equality is not a loophole.** `field "notes=…"` and `field-has "notes=…"`
   are refused too, because repeated equality tests are a way to reproduce a
   value one guess at a time.
3. **The refusal is visible.** A rule naming an internal field gets a
   diagnostic. A seam that reads as "your data must be wrong" is a bad seam;
   this one says what it did and why, and points at `internal ""`.

`internal ""` is exactly enough to write `when internal "" then web-hide "1"` —
to *exclude* a rune from publication — and never enough to reproduce what it is
excluding. That sentence is the whole point: the privacy rule becomes something
the organization writes once, in the open, instead of a convention someone has
to remember in every template.

**`web-hide "1"` is the output domain arriving** — domains.md's third domain,
content management at the export seam, "which runes flow". It drops the rune
from every query-backed block in the newsletter, on the website, and in the
Builder's live preview, which shows what will be published and therefore has to
agree.

**A contested `hide` does NOT hide.** Deliberately, and it is the one place the
conservative direction is *not* to suppress: two scripts disagreeing about
whether something may be published is precisely when a person should decide, and
silently withholding content nobody agreed to withhold is indistinguishable from
the export being broken. The complementary guarantee — that no derived value can
carry internal content in the first place — is a layer down and does not depend
on this one.

### The purity rule, and `today`

A predicate must be a **pure function of its arguments**. No clock, no I/O, no
mutable state. An impure one makes the merge depend on *when* it was asked and
silently destroys order-independence for every downstream consumer — silently
being the operative word: the failure is a result that differs by how it was
assembled, which no test catches unless it is looking for it.

Dates are the one place that rule is under pressure, so **`today` is read once
per derivation, by the caller, and frozen into the frame**. Inside a predicate
it is an argument in everything but name. Nothing under
`hormiga::allomone::predicates()` can reach a clock, and that is a property to
preserve, not a convenience.

The same frame carries the expensive, host-computed graph measures — degree and
eigenvector centrality, once per derivation — which is what keeps
`central "0.5"` an O(1) lookup inside a pure function.

## 4. What it must never grow

> **If your domain library is growing an evaluator, something has gone wrong.**

That is the protocol's warning and it is the test `hormiga_allomone.cpp` has to
keep passing. Anything missing is a `MESSAGE_FOR_VOIDMAIZ_*` (ground rule 4),
not a local workaround.

# The two old tabs are unshipped (2026-08-11)

The legacy Allomone tab and the Allo Dev editor are **gone from the
application**. Two extra surfaces for a dialect nobody should write in taught
the wrong thing to anyone who opened the View menu, and the new tab is the only
editor.

**The code is kept and still compiles**: `-DHORMIGA_LEGACY_ALLOMONE=1` brings
both windows back, and that is checked rather than hoped — a flag guarding code
that no longer compiles is a comment with extra steps. Two reasons it earned a
flag instead of deletion: `code_editor` is ~370 lines of from-scratch text
editing (own buffer, caret, selection, clipboard, inline widgets) that the
inline-widget work will want, and if a database turns up with a legacy script
nobody can read, one flag beats a git archaeology session.

**Unshipping an editor must not strand data**, so three things came with it:

- The legacy **interpreter** still derives (below), so an existing database's
  enabled `script` runes keep colouring their cards.
- The new tab **lists legacy sources and can enable, disable and delete them**,
  with the source visible on hover. Not editable — the language is frozen and we
  are not shipping a second editor for it. A script you cannot *see* is worse
  than one you cannot edit: it colours cards for reasons nothing explains.
- The twelve legacy example scripts are **no longer seeded**. Uneditable
  scaffolding in every new database is a worse introduction than none, and the
  ten `allo-script` examples replace them. Existing databases keep theirs.

# The legacy interpreter as one producer

The frozen interpreter no longer writes a colour. Each enabled `script` rune
yields a `ConstraintMap` at uniform strength 1 with origin `legacy`, and that
map merges alongside the `allo-script` sources. This is step 2 of the handover's
adoption path applied to our own migration, and it buys the thing the adoption
was for:

> **An old script and a new one disagreeing about a card is now a surfaced
> conflict rather than whichever ran last.**

Strength 1 uniformly is the honest number: the legacy dialect is imperative and
has no notion of specificity to report, so claiming one would be a lie. It beats
a `when all` default and ties with any single-term rule, which reads as "this
script decided to colour this thing".

# Conflicts are settled through the dispatcher

A ⊤ cell renders as **nothing** — `Merged::value()` returns `""` on purpose so a
renderer cannot show an arbitrary winner, and `color_of()` refuses it a second
time. The card keeps its default. This is the protocol's one non-optional
obligation and the single behaviour the whole design exists to eliminate.

Settling one is `maiz::compile_resolution()` → dispatcher commands → an
`allomone-resolution` rune in the `allomone` mantle. So a human's ruling is
**logged, attributed, replayable and undoable** like every other gesture, and
the library only ever reads it back.

`maiz::resolution_glyph()` must be registered — it is, in
`hormiga::allomone::register_glyphs()`. The handover flagged this as the day-one
trap for a reason: a wrong glyph *name* fails loudly, but a missing *field* is
silent (`set` accepts it, only the projection drops it).

# The editor is upstream's too (2026-08-11)

The first cut of the tab drew a **transparent `InputTextMultiline` with a
coloured overlay on top** — the trick the two old tabs used. It was the wrong
call twice over: the overlay drifts out of alignment with the real glyphs
(character-grid arithmetic against a font that is not quite a grid), and
InputText is a black box, so nothing interactive can ever sit mid-text.

`maiz::code_editor` (`voidmaiz/code.hpp`, in `voidmaiz_view`) is the editor Void
Maiz shipped **with** Allomone, and it owns its layout. Adopting it was mostly
deletion, and it brought:

| | |
|---|---|
| **ctrl+wheel zoom** | with a level-of-detail switch — below ~7px per line the glyphs become coloured bars, so a zoomed-out script is a readable *shape*: where the colours are, how the rules mass |
| **right-click explain** | any word, in a panel that can afford to teach |
| **hover** | "how many subjects does this line match, at what strength" |
| **completion** | ctrl+space, or while typing a word |
| **inline widgets** | a `#rrggbb` literal **is** a colour wheel where it sits, editing the script rather than a copy of it; an ISO date is a month grid |
| **the rest** | word-wise caret motion, double-click select, undo/redo of the local buffer, proper selection and scrolling |

Only the five callbacks are ours, and they are the whole point of having a
*domain* Allomone:

- **`highlight` and `widgets`** run off `maiz::allo_tokens` — upstream's own
  lexer, which is what the parser consumes and what the editor hit-tests. A
  highlighter built on a second lexer would drift from the language by
  construction.
- **`hover`** counts matching subjects with `allo_matches`, which upstream
  exposes for exactly this and which derives nothing.
- **`complete`** offers **the real org**: every tag with how many runes carry
  it, every glyph in use, rune names, link relations, roles, field keys — and
  every property, plain and surface-qualified, with its merge law in the detail
  column. Properties are *discovered* from what scripts actually write, per
  discovery.md, not enumerated. **Internal-notes field keys are not offered**,
  which is the privacy seam showing up in the completion list.
- **`explain`** comes from `hormiga_allomone::explain_word`, in the library
  rather than the tab, because it is domain knowledge and because a test can
  then check it: **every one of the 77 words we offer must explain itself.** A
  predicate added without an explanation would complete, work, and be
  unexplainable — invisible without that check.

**One bug worth recording**, because it compiled and ran: the completion
callback originally iterated `maiz::allo_tokens(src)` directly and kept
`cur`/`prev` pointers into it. A range-for extends a temporary's lifetime for
the *loop*, not one line past it, so both pointers dangled immediately after.
It worked. Name the token vector.

# What we have not taken

Each of these is available and deliberately not wired, so that a future session
does not mistake "absent" for "unavailable":

- **The user graph.** No coherence tracking, so `with` and `device` never match
  — silence, not an error, by upstream's design.
- **`ConflictPolicy::Recency`.** Shipped on our own ask. It stays off until a
  person has seen a conflict and said they want the newer one; resolving power
  nobody asked for is just a quieter way of guessing.
- **The Weaver and the Sentinel.** `influence()` is in the Reference pane;
  proposals and the effect graph are not surfaced yet.
- **Any write tier.** Derive-only, tier 0. Amend/create are upstream's planned
  tiers and gated behind an analyzer that already exists.

# Where the older pages stand

[language](/concepts/allomone/language.md), [effects](/concepts/allomone/effects.md),
[inputs](/concepts/allomone/inputs.md), [traversal](/concepts/allomone/traversal.md),
[execution](/concepts/allomone/execution.md) and
[conflicts](/concepts/allomone/conflicts.md) describe the **legacy dialect** and
the reasoning that led to the proposal. They are kept because the reasoning is
what earned the adoption — several of those decisions came back adopted
verbatim, and two came back corrected, which is a better outcome than either
being ignored. Read them as history, not as specification.

[guide](/concepts/allomone/guide.md) is the user-facing walkthrough and is
**stale**: it teaches the legacy dialect. The eight seeded `allo-script`
examples are the current teaching path until it is rewritten.
