---
type: Concept
title: Worlds — what exists on any map, and what only on Earth
description: "Opened 2026-10-05. A Hormiga map is not assumed to be Earth: it may be OpenStreetMap, a scanned floor plan, or a D&D world. Every spatial idea is classified as universal (any map), anchored (needs real coordinates, which a floor plan can be given) or Earth-only. The rule: a feature asks the world whether it can exist there, through src/gis/source.hpp, and is absent (not broken) where it cannot. What the code assumes today, and what a georeferenced image would unlock."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-05T00:00:00Z
---

# The question

The author, 2026-10-05: *"remembering the abstract nature of maps. right now we
are implementing gps, that won't really be possible if we wanted a fantasy dnd
map (obviously). so keeping in mind as well, and documenting, which concepts
could only exist in a normal earth map."*

This has been a commitment since 2026-07-20 ("don't assume Earth",
[territory](/concepts/sections/gis/territory.md)): the map is a **source**, and
the source may be Portland, a building's floor plan, or the Sword Coast. The
engine already models it: a `MapSource` (`src/gis/source.hpp`) carries its
**projection** (web mercator or flat), its **metric** (haversine metres or plain
euclidean units), whether it **wraps** east–west, and its edges. What was
missing was a written classification of every *feature*, so that each new one is
placed before it is built rather than discovered to be Earth-only afterwards.

# Three kinds of world

| world | example | coordinates | distance | can a phone's GPS land on it? |
|---|---|---|---|---|
| **Earth** | OpenStreetMap, CARTO | latitude, longitude | metres (haversine) | yes |
| **anchored** (georeferenced) | a building floor plan, a festival site map, a scanned neighbourhood map, each pinned to Earth by two or three known points | the image's own pixels, with a transform to latitude/longitude | metres, through the transform | **yes**, through the transform |
| **authored** | a D&D world, a fictional city, a game board | the world's own units | the world's units ("u"), never metres | **no**: there is no latitude to land on |

The middle row matters most for an outreach organization: a shelter's floor plan
or a resource fair's site map is not Earth's imagery, but the people standing in
it are on Earth, so "where am I" can work on it once the image knows where it
is. Nothing builds anchored worlds yet (it is a roadmap item); the
classification exists so that GPS is not written as if the only two choices were
"Earth" and "fantasy".

# Every spatial idea, classified

**Universal** — exists on any map, and must keep working on an authored world:

| idea | note |
|---|---|
| markers (pin, balloon, circle, square, diamond), their colour, icon, form | `gis/marker.hpp` is pure geometry in screen space |
| placing, moving, removing a thing; `place` / `move` actions | a position is a pair of numbers; the world says what they mean |
| regions (rectangle, ellipse; polygons later) and the tag a region gives | containment (`domain/bestow.hpp`) is geometry, not geography |
| reference points and the fan-out | screen-pixel offsets |
| labels, label dodging, clusters, the pick-which sheet | screen space |
| views, position channels, layers, style rules, Allomone's map style | the model, not the world |
| notes on the map | a note is about a place in whatever world the place is |
| pan, zoom (all the gestures), the camera | the zoom pyramid works for flat worlds too (`flat_x`, `gis_smoke`) |
| the PNG export | composes whatever tiles the source has |
| search the map, the list "on this map" | ordering by distance uses the world's own metric |

**Anchored** — needs real-world coordinates, which an anchored world can supply
through its transform:

| idea | note |
|---|---|
| **the phone's location** (GPS, the blue dot, follow, "where I am") | `voidmaiz/location.hpp` returns latitude/longitude |
| **the accuracy circle** and "give or take N m" | a radius in metres |
| distances in metres, "N m away", the scale bar in metres | an authored world shows its own unit or no scale bar |
| proximity in metres (the "hidden connections", `near "lat,lon" <m>`) | the metric is the source's (`MapSource::distance`), so this degrades correctly already |
| location-personalized newsletters (territory's north star) | a subscriber's real position |

**Earth-only** — exists only where the map *is* Earth's imagery:

| idea | why |
|---|---|
| **online tile servers** (OSM, CARTO) and their attribution and usage policies | they serve Earth |
| **geocoding** (an address to a point) | an address is an Earth thing |
| **weather and external-condition feeds** (T3) | they report about Earth's places |
| wrapping at the antimeridian, the ±85.05° latitude clamp | properties of web mercator |
| importing GPX / GeoJSON / shapefiles in WGS84 | Earth coordinates (an anchored world can take them through its transform; an authored one cannot) |
| a "labeled vs no-labels" base-map choice | a property of the tile styles we use, not of maps |

**Not on any map we make:** rotation (heading-up). Hormiga is not navigation;
the map is always north-up (or "image-up" on an authored world). See
[research](/concepts/sections/gis/research.md).

# The rule

**A feature asks the world whether it can exist there, and is absent, not
broken, where it cannot.** Not a toggle the person must find, and not an error
after they press it: on a D&D map there is simply no locate button.

The question is asked of the `MapSource`, never answered by a global or a
hard-coded check in a view. Today the test is the metric, because it is the
field that already separates the worlds that exist: `Metric::Haversine` means
Earth coordinates. When anchored worlds are built, the source gains a
georeference and the test becomes "has Earth coordinates", which is true for
Earth and for anchored worlds. The universal features must never ask at all.

**Built so far (2026-10-05):** the phone map's locate button, blue dot, *Here*
and *Where I am instead* exist only when the world's metric is haversine
(`phone_map.cpp`). Distances already go through `MapSource::distance`, whose
unit is "u" on a flat world.

# What the code assumes today, honestly

- **Every shipped source is Earth** (`kBuiltinSources`: OSM, CARTO light, CARTO
  dark). The flat world exists in the engine and in its tests and has no caller.
- **The phone's viewport (`gis/view.hpp`, `SlippyView`) is web mercator only.**
  An authored world on a phone needs the same struct over `flat_x`/`flat_y`;
  written when a flat world first has a phone, not before.
- **The scale bar and "N m away" on the phone print metres** without asking the
  source. They are only drawn on Earth today because every source is Earth; when
  a flat source exists they must use `distance_unit()` or hide.
- **The desktop canvas and the PNG export** call `merc_*` directly in places
  that predate `MapSource`; `tile_column` was the first of them to ask the world.
- **Marker geometry, shapes, rules, channels, notes, reference points** assume
  nothing about the world, by construction.

# What an anchored world would take

A source with an image (one file, or tiled) and **two or three control points**,
each a pixel and the latitude/longitude it is. From those, an affine transform
(three points; a similarity transform from two) maps a GPS fix into the image,
and the image's pixels into metres. That is all a floor plan needs for the blue
dot, distances and the accuracy circle. It is the smallest piece of the
"map builder" the author has said they want (territory, "Void Maps"), and the
first one with an outreach use: a resource fair's site map with volunteers'
positions on it. Recorded in the [roadmap](/concepts/sections/gis/roadmap.md).
