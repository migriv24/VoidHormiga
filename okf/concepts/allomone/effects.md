---
type: Concept
title: Allomone — the effect vocabulary (what's editable)
description: "The catalog of visual aspects Allomone can change, per surface (Data cards, list, map markers/geometry, calendar, fonts/theme). Each rule emits a domain-neutral annotation; this doc lists the annotations and how each surface interprets them — the action codomain the blocks are built to target."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). Before designing action blocks
([block-catalog](/concepts/allomone/block-catalog.md)) we must know **what is
actually changeable** — the codomain. A rule emits a **domain-neutral
annotation** (§ "The annotation set"); each **surface** interprets the annotations
it understands ([domains](/concepts/allomone/domains.md)). This doc is the
authoritative list, and it is where we **start local and simple** (author,
2026-08-03): the **local application's own appearance** — a card's color, a
marker's color — comes first; outputs (web/newsletter) come later.

Legend for value types: **color** (swatch), **enum** (fixed dropdown),
**number** (0–1 or px), **bool**, **asset** (image/icon ref).

# The annotation set (domain-neutral)

The small, shared vocabulary a rule can set. Surfaces map these to their own
properties (next sections). Keeping the set small is what lets one rule light up
many surfaces.

| annotation | type | meaning |
|---|---|---|
| `color` | color | the primary color of the thing |
| `accent` | enum | a semantic color role (`normal`/`info`/`warning`/`danger`/`success`) → resolves to a theme color |
| `emphasis` | enum | `low`/`normal`/`high` — importance, mapped to weight/size/border |
| `highlight` | color | a background/glow tint |
| `icon` | asset/enum | an icon or glyph to show |
| `shape` | enum | a shape choice where the surface supports it |
| `size` | enum | `small`/`medium`/`large` where the surface supports it |
| `hidden` | bool | de-emphasize/hide **from the local view only** (never a data change) |
| `animate` | enum | a temporal treatment (needs the clock; e.g. `pulse`, `cycle`) |

Anything not in this table is a **surface-specific extension** (listed per
surface below) — added sparingly, because each extension is one more thing a new
surface must ignore gracefully.

# Surface: Data tab — contact/org cards (FIRST TARGET)

The simplest place to build and test. A card's editable aspects:

| property | type | annotation → | notes |
|---|---|---|---|
| card **size** | enum small/med/large | `size` | also a manual view control (built 2026-08-03) |
| card **background** | color | `color`/`accent` | the card fill |
| **highlight / glow** | color | `highlight` | selection-independent tint (fx_highlights-aware) |
| **border** color + weight | color, number | `emphasis` | thicker/colored border = higher emphasis |
| **avatar ring** | color | `accent` | ring around the photo |
| **text / name color** | color | derived from contrast | usually auto from background |
| **badge / chip** | enum/asset | `icon` | a small mark on the card |
| **animation** | enum | `animate` | color cycle etc. — **clock only** |

# Surface: Data tab — list rows

| property | type | annotation → |
|---|---|---|
| row highlight | color | `highlight` |
| row text color | color | `accent` |
| leading icon/badge | asset/enum | `icon` |
| de-emphasize (dim) | bool | `hidden` (local view) |

# Surface: Map — markers & geometry

Markers already support most of this (`MShape`, rule-colored, icons); Allomone
subsumes the map's current rule engine here.

| property | type | annotation → | notes |
|---|---|---|---|
| marker **color** | color | `color`/`accent` | |
| marker **icon** | enum (Font Awesome set) | `icon` | |
| marker **shape** | enum Circle/Pin/Square/Diamond | `shape` | the existing `MShape` |
| marker **size/scale** | number | `size`/`emphasis` | |
| **label** shown | bool | — | surface-specific |
| **label** color / scale | color, number | — | surface-specific |
| **opacity / layer** | number | — | surface-specific |
| drawn **geometry** (rect/ellipse) fill | color | `color` | map shapes |
| geometry outline | color | `accent` | |

# Surface: Calendar — days & events

| property | type | annotation → | notes |
|---|---|---|---|
| event **color** | color | `color`/`accent` | events vs incidents already distinct |
| event **icon** | asset/enum | `icon` | |
| event emphasis | enum | `emphasis` | bold chip |
| **day cell** background | color | `highlight` | e.g. "days with a `deadline` tinted" |
| day marker/dot | enum | `icon` | |

# Surface: Fonts & theme (global, app-wide)

These are the Style-tab axes; a rule can set them **conditionally by scope**
(rare, but possible — e.g. a local view that bumps type scale). Mostly these stay
manual, listed for completeness.

| property | type | notes |
|---|---|---|
| heading / body **font** | enum | the vendored/OS font palette |
| **accent** / accent2 | color | |
| **preset** | enum | clean/soft/bold/editorial/glass |
| **radius**, **texture**, **scale** | enum/number | |

# Surfaces (later): newsletter & website (output domain)

Deferred ([domains](/concepts/allomone/domains.md) §output). A rule's annotation
becomes a **CSS class / band style** at the export seam, **through the privacy
seam**. Same annotation set (`accent`, `emphasis`, `hidden` → withhold from
output, etc.); the interpretation is a render-pack detail. Not built early.

# How this drives the blocks

Each row above is a candidate **action block** in the Appearance category, and
each value type dictates the **widget** in the block's slot (color → swatch, enum
→ dropdown, size → small/med/large dropdown). The
[block-catalog](/concepts/allomone/block-catalog.md) turns this table into
concrete blocks and value widgets. A surface that doesn't understand an
annotation simply ignores it — so adding a surface never breaks a rule, and
adding an annotation is a deliberate, catalogued act.
