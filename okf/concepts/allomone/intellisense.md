---
type: Concept
title: Allomone — IntelliSense (Hormiga-native editor affordances)
description: "Because Allomone is our own external DSL in our own editor, its IntelliSense can be RICHER than a normal code editor — inline, domain-aware widgets: click a color literal → a color picker; a tag position → a dropdown of real tags; type-driven completion from the type system; a list literal → a tag multi-select; live diagnostics from the parser/validator. LSP-shaped (a 'language service'), shared with Notes."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-04T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). The author's insight (2026-08-04):
because Allomone is **our external DSL in our own editor**, "we have the luxury of
implementing whatever we want" — the IntelliSense need not be a plain code
editor's. It can be **inline, domain-aware, and interactive**: literals become
little widgets, positions know what values are legal, and the help comes from the
**type system** ([language](/concepts/allomone/language.md)). This is the payoff
of the external-DSL choice ([foundations](/concepts/allomone/foundations.md)).

# The model: a "language service" (LSP-shaped), Hormiga-flavored

The DSL literature's headline lesson is *tooling is first-class*, via the
language-server model (highlighting, diagnostics, completion). We adopt the
**shape** — a **language service** that answers queries about a cursor position —
but we're **not constrained to LSP's text-only interactions**: because we own the
render, a completion can be a **live widget**, not just a text list. The service
is the Allomone half of the **shared editor engine** (the Notes half is a markdown
service — [language](/concepts/allomone/language.md#the-shared-text-editor-engine-with-notes--author-2026-08-03)).

# The Hormiga-native affordances (what makes this special)

These come straight from the **types** — the service knows what each slot wants:

- **Color literal → inline color picker.** A `#rrggbb` isn't just colored text
  ([the highlighter already draws it in its own color](/concepts/allomone/language.md));
  **click it and a mini color-chooser pops up**; picking edits the literal. The
  swatch *is* the value.
- **Tag position → a tag dropdown.** Where the grammar expects a tag (`when …`,
  `has_tag(…)`, a list of tags), offer a **type-ahead dropdown of the real tag
  vocabulary** (reusing `draw_tag_filter`/`tag_picker`), namespaced axes included.
  No memorizing tag spellings; drift is prevented at authoring time.
- **List literal → tag multi-select.** `{ "month:", "season:" }` can be edited as
  a **chip multi-select** (the author's "time tags" list), not raw string typing.
- **Type-driven completion.** After `thing.` the service offers `tags`,
  `field(…)`, `is`, `color(…)`; after a `list`, `count`, `matching`, `contains`;
  `thing is (` offers the **entity types** (contact/organization/event/…). The
  completion set is *computed from the type at the cursor*, not a flat keyword
  list.
- **Field/relation pickers.** `thing:field("…")` → a dropdown of the glyph's
  declared fields; a relation traversal → the known relation names.
- **Live diagnostics (squiggles).** The parser/validator underlines unknown tags,
  ill-typed comparisons, a `when` with no effect, an undefined variable — inline,
  as you type, from the same **validator** that guards the AST
  ([language](/concepts/allomone/language.md#the-reconstruction-engines--blocks--text-author-2026-08-04)).
- **Effect-aware hints.** In a `color`/`highlight` slot, the picker; in an `icon`
  slot, the icon palette; in an `emphasis`/`size` slot, the enum. Each effect
  advertises its widget.
- **Auto-indent & formatting.** `do … end` / `if … end` blocks auto-indent; the
  **serializer** ([language](/concepts/allomone/language.md)) doubles as the
  canonical formatter (format-on-save), so style is uniform and diffs are clean.
- **Hover = provenance & meaning.** Hover a tag → how many things carry it; hover
  a variable → its inferred type/value; hover a rule → what it currently styles
  (the live counts we already compute).

# Why this is honest, not gimmicky

Every affordance above is **derived from data we already have**: the type system
(what a slot accepts), the tag vocabulary (the data mantle), the glyph field
descriptors, and the parser's AST + diagnostics. None of it invents behavior; it
**surfaces** the model at the point of authoring — the same "surface the model"
principle as the map's marker menus and the Data tab's typed inspector. It is
*Hormiga* IntelliSense precisely because it knows *Hormiga's* data.

# Build note (and the dependency)

Inline interactive widgets in the editable text need a **dedicated code-editor
widget** (ImGui's `InputTextMultiline` can't host per-token widgets or per-token
color — the current highlighting lives in a preview pane for exactly this
reason). So the intellisense affordances land **with** the dedicated editor
(vendor an ImGui code-editor widget, shared with Notes), not before it. Order:
(1) the editor widget + real syntax coloring + auto-indent; (2) the tag dropdown
and color picker (highest value, reuse existing widgets); (3) type-driven
completion + diagnostics from the language service; (4) hovers/provenance. The
tokenizer (`allo_draw_line`) and the type system already exist to feed it.
