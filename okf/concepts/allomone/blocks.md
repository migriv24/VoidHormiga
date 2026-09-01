---
type: Concept
title: Allomone — the block language
description: "Scratch's typed shape grammar carried into Allomone: hat/stack/boolean/reporter/C/cap block shapes as triggers/actions/conditions/queries/control; the block categories; operators, variables, lists, custom blocks; if/else."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). Scratch's real genius is not the
colors — it is the **typed shape grammar**: a block's *shape* encodes its *type*,
so a value can only be dropped where a value fits and a boolean only where a
boolean fits. **Ill-typed programs are literally unbuildable.** Void Maiz already
ships the `"block"` shape kind, typed sockets, and snap/tear/heal — the substrate
is here; Allomone will exercise it **far harder than the newsletter builder did**,
because these blocks carry real logic and nesting.

# The shape grammar (Scratch → Allomone)

| Scratch shape | plugs into | Allomone role | examples |
|---|---|---|---|
| **Hat** (top, notch below) | starts a script | **Trigger** | `always` (standing rule); `on add <tag>`; `on tag change` |
| **Stack** (puzzle top+bottom) | statement slot | **Action** | `set color`, `highlight`, `add tag`, `link to`, `fill field if empty`, `publish`/`withhold` |
| **Boolean** (hexagon) | boolean slot only | **Condition** | `has tag …`, `field is empty`, `linked to …`, `centrality > …`, `and`/`or`/`not` |
| **Reporter** (rounded/oval) | value slot only | **Query** | `tags of`, `field <k> of`, `count of neighbors`, `previous tags`, arithmetic |
| **C-block** (wraps a stack) | control | **Control** | `if`, `if/else`, `for each matching` (and optional loops) |
| **Cap** (notch above only) | ends a script | **Terminal** | `stop` |

Shape *is* the type system. A hexagon (`has tag dog`) fits only a boolean slot;
an oval (`count of neighbors`) fits only a value slot; you cannot build "if 5".
This is a **syntax-directed structure editor** — the visual analogue of a typed
AST ([language](/concepts/allomone/language.md)).

# Categories (Scratch's Motion/Looks/Sound → Hormiga's domains)

Palette groups, color-coded like Scratch, organized by **input** near the top and
**effect domain** ([domains](/concepts/allomone/domains.md)) below:

- **Triggers** — `always`, `on add/remove/change <tag>` (see
  [reactivity](/concepts/allomone/reactivity.md)).
- **Conditions (tags & graph)** — the tag-grammar predicates (the filter chip as
  a block), field tests, link/neighborhood tests, spectral tests, and the
  logical combinators.
- **Queries (reporters)** — read values from a rune, its neighborhood, or the
  graph; the operands of conditions and actions.
- **Appearance** — the "Looks" domain, one sub-palette per surface (Data / Map /
  Calendar / Web), all emitting domain-neutral annotations.
- **Structure** — materializing actions (add tag / link / fill field).
- **Output / Content** — publish/withhold/shape for the export seam.
- **Operators** — arithmetic, comparison, boolean, set, aggregation (below).
- **Variables & Lists** — named values and collections (below).
- **My Blocks** — custom, author-defined procedures (below).

# if / if-else (control)

Core and cheap. C-blocks wrap a body that runs when a condition holds; `if/else`
adds the alternative. The author's example composes them:

> `if (has tag "dog")` → `if (field "logo" is empty)` → `set logo = dog-logo`

`if/else` is the everyday shape of a rule; loops are a separate, optional model
([reactivity](/concepts/allomone/reactivity.md)).

# Operators, variables, lists, custom blocks

This is what makes Allomone a real (small) language; mapping Scratch's operator
set onto our world is a deliberate, itemized piece of work (imitate Scratch's UX
closely — nested hexagons for boolean logic, ovals for reporters, inline inputs
and dropdowns):

- **Arithmetic / comparison** — over reporter values (counts, centrality, dates):
  `+ − × ÷`, `<`, `=`, `>`.
- **Boolean** — `and` / `or` / `not` over tag & graph predicates. *This is the
  same grammar as the tag-filter builder*, so a filter expression **is** a
  boolean block subtree — shared model, not a parallel one.
- **Set operators** — over tag sets (`union`, `intersection`, `difference`,
  `contains`); central to delta comparison (previous vs new tags).
- **Relational** — traverse links (`neighbors where …`, `linked via <relation>`).
- **Aggregation** — `count` / `any` / `all` over a neighborhood or the mantle.
- **Variables & lists** — named values and ordered collections; enable
  accumulation and lookup tables (e.g. a tag→color map).
- **Custom blocks (My Blocks)** — author-defined procedures: name a reusable
  condition or action subtree, use it like a primitive. This is how a rule set
  stays readable as it grows.

With variables + loops the language is **Turing-complete-ish**; that power is a
feature the author wants, but anything that needs iteration is gated by the
optional clock ([reactivity](/concepts/allomone/reactivity.md)), so the *default*
engine stays a terminating, declarative fixpoint.
