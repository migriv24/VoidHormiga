---
type: Concept
title: Allomone — the editor as a projection (friendly vs raw, references, inline widgets)
description: "The Allomone 'text editor' is really a WIDGET CANVAS disguised as text: a friendly surface (names, colors, inline pickers, thumbnails, intellisense) projected over a raw, ID-based script the engine runs. A rune reference reads as its name but stores its ID (survives renames). A friendly/raw toggle shows either. The engine is untouched — this is a view layer."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-05T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). This is **direction**, not built
yet. It sits alongside [intellisense](/concepts/allomone/intellisense.md).

> **The engine stays intact (the author's check, 2026-08-05).** Everything here
> is a **view layer** over the *same* script and the *same* interpreter
> (`src/domain/allomone_legacy.hpp`). The tokenizer/parser/evaluator, the rune-value model,
> derive-only, and the fuel budget do not change. What changes is how the script
> is *presented and edited* — and, for references, how a name maps to an id.

# Two layers: the friendly surface and the raw script

The author's framing: there is a **user side** (what you write and see) and a
**backend** (what Allomone actually stores and runs). They are two projections of
one script:

- **Friendly Allomone** — what a user writes: names for runes, a color swatch you
  can click, tag chips, inline pickers, thumbnails, syntax colors, intellisense.
  Compact and forgiving.
- **Raw Allomone** — the literal underlying script: ids instead of names,
  everything spelled out, no sugar. Longer and error-prone to write by hand.

A **toggle** switches the editor between them. **Nobody writes raw by hand** —
it's pointless and mistake-prone — but exposing it is invaluable for
*development* (seeing exactly what the friendly surface compiles to) and for
trust (the friendly view is honest sugar, not magic). This is the same
**projectional-editing** idea already in the language design
([language](/concepts/allomone/language.md) "One AST, two projections"), applied
to *presentation*: friendly and raw are two renderings of one AST.

# References: type a NAME, store an ID

The one place specific runes appear (kept rare on purpose —
[traversal](/concepts/allomone/traversal.md) is the norm), a reference must be
**stable across renames**:

- **On the surface** you type the rune's **name** — and intellisense predicts it
  as you type, the way an IDE completes a symbol. It *reads* like a string.
- **Underneath** the reference stores the rune's **id** (`spirit.id`). Rename the
  rune later and the reference still points at the same rune; the surface just
  shows the new name.

So a reference is a **name↔id binding**, not a string literal. (If the referenced
rune is *deleted*, the reference is dangling — surfaced as a diagnostic, another
reason the language leans on structural queries instead.) This is the concrete
mechanism behind the author's "user side vs backend": the friendly token is a
name; the raw token is an id; the editor round-trips between them.

# The "text editor" is a widget canvas

The bigger idea: the Allomone editor is **a widget canvas disguised as a text
editor.** Certain tokens are **live, clickable widgets** embedded in the text —
more computationally expensive than plain text, but worth it for UX:

- **A color** (`"#ff8800"`) → click it, get a **color wheel**; the hex updates in
  place.
- **A rune reference** → click it to **pick a different rune**; show a **mini
  thumbnail** beside it (a contact's/organization's photo), so the reference is
  visually recognizable, not just a name.
- **A tag** → a chip with the tag picker / recommender already built
  ([tag-recommender](/concepts/allomone/tag-recommender.md)).
- **A glyph name** → a dropdown of the registered glyphs.
- **A number / list** → the right small editor.

This is the **inline value-widget** idea from
[intellisense](/concepts/allomone/intellisense.md), realized inside the script
itself: the editor renders the token stream, and where a token has a richer type,
it draws an interactive widget instead of plain glyphs. The current combined
editor (transparent `InputTextMultiline` + colored overlay, already shipped) is
the first rung of exactly this — an overlay drawn over the text. The next rungs
add hit-testing and popovers on specific token types.

# How it fits without touching the engine

- **Parse once, project twice.** The interpreter parses the raw script to an AST
  and runs it. The friendly surface is a *rendering* of that AST (or of the raw
  text with name/id substitution); editing the surface edits the underlying
  script. No new evaluator.
- **References resolve at the seam.** Name→id happens when the reference is
  entered (via intellisense) and id→name when it's displayed. The stored script
  holds ids; the interpreter already deals in rune identity.
- **Widgets are input, not semantics.** A color wheel just edits the `"#hex"`
  string a `rune:color(...)` already takes; a rune picker just edits a reference
  token. The language grammar is unchanged.

So the roadmap here is **editor work**, layered on the intact engine: (1) the
friendly/raw toggle over the current script; (2) name↔id references with
intellisense completion; (3) inline widgets on color/rune/tag/glyph tokens
(hit-test the overlay, pop the right picker); (4) thumbnails for
contact/organization references. Each is additive and independently shippable.

# What Void Maiz provides — and what it doesn't (checked 2026-08-05)

Void Maiz has **no text/code editor** — `editor.hpp` is node-canvas *view state*
(camera + gesture staging), and `edit_canvas` is the node graph. A from-scratch
text editor is therefore a **Hormiga build** (on ImGui draw-lists + input), and a
candidate to **offer upstream** later (like the block-editor ask).

But Void Maiz *does* give us the piece that matters for inline widgets:
**`widget.hpp` — the field-editor protocol** (`kind → renderer`, with the VLS
staging discipline). A **`"color"` editor kind already exists** (it edits a
`"#rrggbb"` field on a contact/marker). The inline-widget architecture reuses it:
the custom editor recognizes a **token span** (a color, a rune reference, a tag,
a glyph) and renders **that same registered field-editor** in place — so the
color wheel in a script *is* the color wheel on a contact, the rune picker *is*
the connections search. One widget kit, many surfaces.

# Shipped: the color palette (first rung, 2026-08-05)

Two safe steps that need **no** custom editor and **don't touch native editing**:
- **Inline color rendering** — every `"#rrggbb"` token draws in *its own color*
  in the overlay, so you *see* colors in the script, not just hex.
- **A color palette toolbar** — one swatch per distinct color in the script;
  click it → a color **wheel** → **Apply** rewrites that hex everywhere (a single
  logged `setjson`). Safe over the ImGui `InputTextMultiline` because clicking the
  swatch deactivates the input, so the buffer edit shows next frame. This proves
  the **token → widget → buffer-rewrite pipeline** the inline version will reuse.

The *truly inline* clickable widget (the wheel popping from the color **in the
text**) is where the ImGui `InputTextMultiline` black box stops us — you cannot
place an interactive item mid-text — which is exactly why the custom editor is
needed.

# The custom editor — what it takes (the honest list)

A from-scratch text editor is a real build; it must reach **parity before it
replaces** the ImGui one, so the basics the author called out never regress:

- **Text model**: a UTF-8 buffer with line index; insert/delete; **undo/redo**.
- **Cursor & selection**: caret movement (arrows, word, home/end, page), **click
  + drag select**, shift-select, double/triple-click, multi-line selection
  highlight.
- **Clipboard**: **copy / cut / paste** via the platform seam (ImGui provides
  `SetClipboardText`/`GetClipboardText`).
- **Rendering**: syntax-colored spans (we already have the tokenizer), a blinking
  caret (already drawn), selection rects, scrolling, and **inline widget spans**
  (a token can reserve a box the editor lays out around).
- **Input**: keyboard (incl. **IME** for non-Latin), mouse, autoscroll on drag.
- **Fonts**: a mono font loader with the needed glyph ranges (JetBrains Mono is
  vendored; wider ranges/fallback as content demands — the author's "font loaders
  and stuff").

**Parity-gating** is the rule: keep the working ImGui editor as the default;
build the custom editor behind a flag; switch only when copy/paste/select/undo/
IME all match. Until then, every inline-widget win we *can* get over the ImGui
editor (like the palette) we take; the rest waits for the custom canvas. **The
Allomone engine is untouched throughout** — this is all view-layer.

# Shipped: the "Allo Dev" tab + custom editor v1 (2026-08-05)

The author's call: build the custom editor on a **separate, experimental tab**
(`win_allomone_dev`, "Allo Dev"), so the working Allomone tab is never at risk;
**merge later** once at parity. Built:

- **A new dockable tab** — a lean script picker + the custom editor + a live
  parse status. The production tab (`draw_allomone_body`) is untouched.
- **`code_editor(id, text, size)`** — the from-scratch editor (`app.cpp`): its
  **own** text buffer, caret (`ce_caret`), selection anchor (`ce_sel`), and
  rendering. v1 handles **text input (UTF-8 encoded)**, **caret movement**
  (arrows / home / end / up / down, column-preserving), **click + drag and shift
  selection** with a highlight, **copy / cut / paste / select-all** via the ImGui
  clipboard seam, **syntax coloring** (reusing `allo_draw_line`), a **blinking
  caret**, and **scrolling with auto-scroll to the caret**. Confirmed rendering
  live (01-hello-allomone with full coloring + caret).

**Inline widgets — first three, BUILT 2026-08-05** (the reason the tab exists —
owning the layout is what lets these live *in the text*):

1. **Click a color → the wheel, in place.** Clicking a `"#rrggbb"` token opens a
   color wheel positioned on it (hovering it outlines the token + a hint, and the
   cursor becomes a hand); the wheel **live-edits** that hex in the buffer as you
   drag. The click is intercepted before caret placement, so it doesn't fight
   editing.
2. **A rune's photo, inline.** When a **string literal names a `contact` or
   `organization`** rune, its **avatar thumbnail** draws right after the token —
   so a referenced rune is visually recognizable, not just a name. (No avatar set
   ⇒ nothing drawn.)
3. **Preview a set on hover.** Hovering a `runes where …` line **runs the
   predicate over the data** and tooltips the count + the first few matching runes
   (with thumbnails) — a rough idea of *which* runes the set refers to, without
   naming any.

**Editing niceties + more widgets, BUILT 2026-08-05:**

4. **Auto-indentation.** Enter carries the current line's leading whitespace and
   adds a level after a block opener (`do` / `then` / `else`); Tab is a soft
   2-space (or completes, below).
5. **Date → a mini calendar.** Clicking a `"YYYY-MM-DD"` token opens a compact
   month-grid picker (prev/next month, click a day) that rewrites the date in
   place — the color-wheel pattern, for dates (times will reuse it).
6. **Tag autocomplete (intellisense seed).** Typing inside a string right after
   `has` shows a drawn suggestion box of matching **tags** (from the vocabulary);
   **Tab** completes the top match. This is the first cut of type-aware
   suggestions — later, a **typed subject** (`contact has …` vs `event has …`)
   narrows the list via the [tag-recommender](/concepts/allomone/tag-recommender.md)
   (Q28).

**Still next**: name↔id references, `:`-completes-the-effect-palette (Q28's `.`
data / `:` verbs split, so `:color` pops the wheel by completion), the
**friendly/raw toggle**, and the parity polish (undo/redo, IME, word-motion,
wider fonts). When the editor reaches parity, it replaces the ImGui one and the
two tabs merge.
