---
type: Concept
title: Allomone — the block & widget catalog
description: "Concrete designs for each Allomone block and value widget: the shapes (hat/stack/hexagon-boolean/oval-reporter/C/cap), the in-slot widgets (tag-picker chips, dropdowns, color swatches, number/text fields), and what each does — the buildable spec behind the shape grammar."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). [blocks](/concepts/allomone/blocks.md)
gives the shape *grammar*; this doc gives the **concrete catalog** — each block
and, crucially, each **value widget** that lives *inside* a block's slot. Scratch
is not just blocks: it is blocks with **ovals, hexagons, dropdowns, and typed
input fields** nested in them, and getting those right is most of the UX. The
action vocabulary here is drawn from [effects](/concepts/allomone/effects.md);
we start with the **Data-card appearance** subset (phase A).

# Value widgets (what fills a slot)

A slot's **shape** says what *type* it accepts; the **widget** is how you fill it.

- **Boolean slot (hexagonal notch)** — accepts only hexagon blocks
  (`has tag …`, `and`, …). Empty = "false".
- **Value slot (rounded notch)** — accepts oval reporter blocks *or* an inline
  literal widget:
  - **tag chip** — the existing `draw_tag_filter` chip / tag-picker: type-ahead
    over the tag vocabulary, click to pick. This is the single most-used widget.
  - **dropdown (enum)** — a fixed list: field names, relation names, colors-by-
    name, shape names, sizes. Renders as Scratch's rounded dropdown.
  - **color swatch** — opens a small picker; shows the current color.
  - **number field** — inline numeric entry (drag or type), optional min/max.
  - **text field** — free text (rare; e.g. a literal string to compare).
- **Statement slot (puzzle notch)** — accepts stack blocks (actions).
- **Substack slot (inside a C-block)** — accepts a stack of statements.

Widgets **stage locally and commit one edit** (the widget-protocol discipline):
editing a swatch doesn't fire until you release; Escape abandons.

# Triggers (hat blocks)

| block | slots | fires |
|---|---|---|
| `always` | — | standing rule; re-evaluated on any relevant change |
| `on add [tag]` | tag chip | when that tag is added to a rune |
| `on remove [tag]` | tag chip | when that tag is removed |
| `on tag change` | — | on any tag change; exposes `previous tags` / `new tags` |

(Delta triggers read the rune's stored delta log —
[reactivity](/concepts/allomone/reactivity.md). `always` is the default and the
only trigger needed for phase A.)

# Conditions (hexagonal boolean blocks)

| block | slots | true when |
|---|---|---|
| `has tag [tag]` | tag chip | the rune carries that tag (the filter atom) |
| `field [field▾] is empty` | dropdown | that field is blank |
| `field [field▾] [=,<,>] (value)` | dropdown, op, value | field comparison |
| `linked via [relation▾]` | dropdown | the rune has such an edge |
| `linked to [tag]` | tag chip | a neighbor matches the tag |
| `centrality [>] (number)` | op, number | graph-spectral test (opt-in, budgeted) |
| `(bool) and (bool)` | 2 boolean | both |
| `(bool) or (bool)` | 2 boolean | either |
| `not (bool)` | boolean | negation |

`has tag`, `and`, `or`, `not` compose into exactly the tag-filter grammar — the
same subtree `draw_tag_filter` already builds, so a filter expression **is** a
condition subtree.

# Queries (oval reporter blocks)

| block | returns |
|---|---|
| `field [field▾] of (rune)` | a field value |
| `tags of (rune)` | a tag set |
| `count of (neighbors where (bool))` | a number |
| `centrality of (rune)` | 0–1 |
| `previous tags` / `new tags` | tag sets (in delta triggers) |
| `(num) [+ − × ÷] (num)` | arithmetic |
| plain literal widgets | number / text / color / tag |

Ovals plug into value slots; they are the operands of conditions and actions.

# Actions (stack blocks) — Appearance first

Phase-A subset targets the **Data card** ([effects](/concepts/allomone/effects.md));
the same blocks gain map/calendar meaning via renderer packs in phase B.

| block | slot | annotation emitted |
|---|---|---|
| `set color [swatch]` | color | `color` |
| `set accent [role▾]` | enum | `accent` |
| `highlight [swatch]` | color | `highlight` |
| `set emphasis [low/normal/high▾]` | enum | `emphasis` |
| `set size [small/medium/large▾]` | enum | `size` |
| `set icon [icon▾]` | enum/asset | `icon` |
| `set shape [shape▾]` | enum | `shape` (map) |
| `dim from this view` | — | `hidden` (local only) |
| `animate [pulse/cycle▾]` | enum | `animate` (**clock only**; inert when clock off) |

All actions are **derive-only** ([domains](/concepts/allomone/domains.md)) — they
attach an annotation the renderer reads; **none writes the model**. (Structure/
materialize actions are intentionally **absent** from the catalog until Allomone
matures.)

# Control (C-blocks)

| block | shape | body runs |
|---|---|---|
| `if (bool) { … }` | C, one boolean + substack | when the boolean holds |
| `if (bool) { … } else { … }` | C with two substacks | then / otherwise |
| `for each (neighbor where (bool)) { … }` | C | over a neighborhood (bounded) |
| `repeat (n) { … }` / `forever { … }` | C | **clock only**; inert when clock off |

`for each` is bounded (a neighborhood query), not an open loop, so it is safe
without the clock; `repeat`/`forever` need the clock.

# A worked example (blocks → meaning)

The author's example, as blocks (Data card domain, derive-only):

```
always
  if ⬡(has tag [dog])
      set color [🟦 blue]
      if ⬡( (has tag [small]) and (has tag [european]) )
          set color [🟦 light-blue]
```

Both `set color` fire on a small european dog; **specificity** makes the inner
(deeper, more conditions) rule win *for that subset*
([conflicts](/concepts/allomone/conflicts.md)) — the outer default still colors
every other dog. No warning; the overlap is the point.

# The same, in Allomone Script

```allomone
rule "dogs" {
  when has tag "dog"
  looks color: blue
  when has tag "dog" and has tag "small" and has tag "european"
  looks color: light-blue
}
```

(Two projections of one AST — [language](/concepts/allomone/language.md).)

# Build order for the catalog

Phase A ships the **bold rows only**: `always`, `has tag`/`and`/`or`/`not`,
`if`/`if-else`, and `set color`/`highlight`/`set size` on Data cards — the
smallest set that makes a real, testable rule. Everything else in this catalog is
added as later phases reach its surface and its input class
([roadmap](/concepts/allomone/roadmap.md)).
