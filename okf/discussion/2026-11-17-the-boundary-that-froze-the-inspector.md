---
type: Discussion
title: The boundary that froze the inspector
description: An imported set of administrative boundaries opened in 0.2.7, and selecting one and opening the map's Inspector took most of a second a frame. Every polygon measure had been made on shapes of four to six corners, and the position writer's precision was a claim in a comment. Hypotheses.
tags: [status:current, audience:all, confidence:speculative]
timestamp: 2026-11-17T00:00:00Z
---

# The boundary that froze the inspector (2026-11-17)

## What happened

- **A set of public administrative boundaries was imported** into a database by
  a script outside Hormiga: polygons of up to a thousand-odd corners each. A
  boundary with holes became one shape with "keyhole" seams; a boundary with
  detached parts became one shape per part.
- **It opened and drew.** Selecting one and opening the map's Inspector froze
  the application; the author first reported it as a crash.
- **Measured in the window harness** (`HORMIGA_FRAME_TIMING`, `perf`): the Data
  tab 6 ms a frame, the map 54 ms, the map with the Inspector on one shape
  900 ms.
- **Why the Inspector.** Its "gives these tags" card counted what is inside the
  shape every frame (`bestow::receivers`), and the tag editor asked who gives
  the shape its tags (`bestow::covering`). Both test the shape against every
  other shape; two shapes whose boxes meet are sampled at 256 points, and each
  sample parsed both shapes' `pts` text from the start.
- **Why the map.** Every frame parsed every polygon's `pts` twice, projected
  every vertex on or off screen, and ImGui ear-clipped every polygon again.
- **The precision was a comment.** `format_geo` wrote `%.7g` and said "~1 cm".
  Seven significant digits are four decimals at a three-digit longitude: up to
  ~11 m. Moving one corner of an imported shape would have rewritten all its
  corners at that precision.

## Hypotheses

**1. We designed for many runes, not for one large rune.** The infinite-dataset
premise (2026-11-03) is about the count of runes; the measures since used many
runes of a few fields each. One rune of tens of KB, read every frame, was
outside every measure. *Wrong if:* a fixture with a polygon of a thousand
corners existed before today. The polygon tests used four to six.

**2. A pure function's cost is invisible at its call.** `receivers(scene,
shape)` reads as a lookup. It is a scan with an overlap test inside, which
calls `shape_contains`, which parses. Each layer was reasonable; the frame loop
multiplied them. *Wrong if:* the call site said what it cost and was ignored.

**3. The release measured what we drew, not what a client would bring.** 0.2.7
was checked with polygons drawn by hand. The first imported boundary was the
first test at real size. *Wrong if:* a public boundary had been tried and
found fast.

**4. A number in a comment is a claim nobody checks.** The test of
`format_geo` used a value with two decimals, which `%.7g` writes exactly, so it
could not fail. *Wrong if:* a test with more than four decimals at a
three-digit longitude existed.

## What changed

- Parsed once per change (`shape::geometry`); triangles cut once
  (`gis::triangulate`, `gis::triangulate_rings`).
- Asked once per change (`bestow::receivers_memo`, `covering_memo`).
- Drawn to the pixel: off-screen shapes skipped, outlines and handles thinned.
- Several rings in one shape (`rings`): no seams, one shape per boundary.
- `format_geo` writes seven decimals; seven hand-written formats call it.
- Measured on the same database: the map 54 → 7.5 ms, the Inspector 900 → 6 ms.

## Still unknown

- How a shape of 10^5 corners behaves (a coastline): its triangles are cut on
  the first frame that draws it, on the frame's thread.
- Whether Hormiga should import GeoJSON itself, so what a client brings arrives
  in the shape Hormiga expects rather than as a script's best guess.
