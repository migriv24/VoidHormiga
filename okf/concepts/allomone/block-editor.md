---
type: Concept
title: Allomone — the block editor (custom view over a rune model)
description: "The blocks overhaul: a purpose-built Scratch-style stack editor drawn custom (like the map), NOT Void Maiz's node-graph edit_canvas. The MODEL stays runes + dispatcher commands (CLI-logged, Void-Script-buildable, Void-Core-rooted); only the VIEW is replaced. A rule is a parent/order/slot tree of block runes — the AST that Allomone Script also projects."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-04T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). **Supersedes the A2.1 approach**
of rendering blocks with Void Maiz's node-graph `edit_canvas` (author, 2026-08-04:
"the blocks we have right now are terrible … blocks built on the node-graph
framework makes little sense"). This document is the overhaul.

# The one idea: overhaul the VIEW, keep the MODEL

The A2.1 blocks failed because of a **paradigm mismatch in the view**:
`edit_canvas` is a *node graph* (free-floating nodes + wires), and a Scratch
editor is a *syntax tree* rendered as snapping puzzle pieces with in-slot
widgets. Grafting one onto the other is why it fought us.

But the **model** was already correct and already honors every Void philosophy:
a rule is **runes in the allomone mantle, edited by dispatcher commands**. That
does not change. We replace only the **view** with a **custom Scratch-style
renderer** — exactly the move the **map** made (location-runes drawn by a custom
slippy-map view; every edit a logged command). Blocks get the same treatment.

Consequently the three non-negotiables hold **for free**:

- **Every UI action is a CLI command.** Add a block = `rune new`; edit an
  argument = `set`; reorder = `set order`; nest = `set parent`/`set slot`;
  delete = `rm`. The view emits the same commands a person could type — a rule
  was already buildable from the command line (the boot-command rules proved it).
- **Void Script can build the same thing.** The model is runes+commands, so a
  script yields an identical rule. Blocks are one emitter of those commands.
- **Rooted in Void Core.** Blocks are runes; their structure is graph
  links/fields; the engine evaluates the graph. The rune tree **is the AST** —
  the same structure [Allomone Script](/concepts/allomone/language.md) projects
  as text. Two projections, one Void-Core model (projectional editing, for real
  this time).

# The model — a rule is a rune tree (the AST)

Each block is a rune in the allomone mantle. Its **glyph names the block type**
(`allo_when`, `allo_hastag`, `allo_setcolor`, `allo_if`, …). Its fields are:

- the block's **arguments** (`tag`, `color`, an operator, …);
- **`parent`** — the block this one sits inside (the `when` hat, or an `if`'s
  branch); the root `when` has an empty parent;
- **`slot`** — which branch of the parent it's in (`body` default; `else` for the
  second branch of an `if`);
- **`order`** — its position among its siblings in that slot.

That parent/order/slot triple is the **entire structure** — no node-graph ports,
no adjacency wires (the artifacts that fought A2.1). It expresses a linear stack
*and* if/else nesting, and it is much simpler than a general graph because a rule
is a **short stack with light nesting**, not an arbitrary diagram. It mirrors the
Builder's proven `row`/`page` grid-order model, one level richer for nesting.

Editing gestures → commands (all logged, undoable, replayable):

| gesture | command(s) |
|---|---|
| add a block from the palette | `rune new <glyph> <name>` + `set <name> parent …` + `set order …` (+ `slot`) |
| edit an argument | `set <name> tag "…"` / `set <name> color "#…"` |
| reorder within a stack | `set <name> order …` (renumber siblings as needed) |
| move into an `if` body | `set <name> parent <if>` + `set <name> slot body` |
| delete | `rm <name>` (siblings' order closes up) |

# The view — a custom vertical stack (like the map's custom canvas)

A single custom function (`draw_allomone_stack`) drawn with ImGui + ImDrawList,
**not** `edit_canvas`:

- Blocks are **colored rounded strips** that stack in a column; an `if/else`
  **indents a nested column** for each branch.
- **Widgets live INSIDE the block** — the whole point, and now trivial because we
  own the render: a **tag-picker chip** on `has tag` (reuse `draw_tag_filter`'s
  atom / `tag_picker`), a **color swatch** on `set color` (reuse the `color`
  editor), dropdowns for enums. No FaceRegistry; we place the widgets ourselves.
- A **palette** (a `+` menu / side rail, categorized per
  [block-catalog](/concepts/allomone/block-catalog.md)) adds blocks.
- **Drag-to-reorder** within and between stacks; delete; hover + selection
  highlight; the derive-only effect updates live in Data > Cards.
- It reads the parent/order/slot tree and renders it; every gesture compiles to
  the commands above.

Because rules are stacks (not free canvases), layout is a simple recursive
vertical walk — no force layout, no wire routing, no camera math. This is closer
in spirit to the map's bespoke canvas than to anything in the node library.

# What we drop / add

- **Drop** (for Allomone): `edit_canvas`, the `shape:block` hint + prev/next flow
  ports, the per-glyph `FaceRegistry` faces, the adjacency-link snapping.
- **Add:** the `parent`/`order`/`slot` fields on the block glyphs; the reusable
  **`color`** widget editor (already built A2.1); `draw_allomone_stack`.
- **Unchanged:** the engine's derive-only evaluation and the `allo_rules` cache —
  it just reads the parent/order/slot tree instead of the adjacency chain.

# Relationship to Void Maiz (the upstream question)

The node library keeps its `"block"` shape kind for graph-flavored hosts; Allomone
simply doesn't use it. Separately, we hold that a **standalone, first-class
block-editor primitive** (not grafted onto the node canvas) is a legitimate
*future* Void Maiz responsibility — blocks are general enough that other apps want
them. So we **build our own now** (to get Allomone right and to not block on
upstream), and **offer it as the forcing-client reference** to Void Maiz for if/
when the library makes a proper block subsystem first-class — the same
build-then-upstream pattern as the widget protocol and the table view. Drafted in
`MESSAGE_FOR_VOIDMAIZ_hormiga-allomone-blocks-*`.

# Build order

1. **Model — BUILT 2026-08-04.** Block glyphs are plain data runes with
   `parent`/`slot`/`order` (no `shape:block`/ports).
2. **View — BUILT 2026-08-04.** `draw_allomone_stack` renders the tree as a
   vertical stack with in-slot widgets (tag combo, color swatch); replaced
   `edit_canvas` in the Allomone tab.
3. **Gestures — BUILT 2026-08-04 (add / delete / new-rule → commands via
   `allo_cmd`).** *Still to add:* drag-reorder (v1 relies on ANDed conditions +
   one action, so order rarely matters yet).
4. **Engine — BUILT 2026-08-04.** `refresh_allo_rules` reads the parent/order
   tree; verified cards color live.
5. **Nesting:** `allo_if` with `body`/`else` branches (next).
6. Later: more block types (actions, operators), then the **serializer +
   parser** for Allomone Script (see [language](/concepts/allomone/language.md)
   §"reconstruction engines").
