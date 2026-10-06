---
type: Roadmap
title: GIS roadmap — the map's do-list
description: "Opened 2026-10-05. Every map feature, built or wanted, with a status: ✅ built, 🔨 built but unwitnessed or partial, ⬜ not built. Grouped by layer (engine, views, data, worlds) and each tagged with the kind of world it needs (U universal, A anchored, E Earth-only; see worlds.md). Gathered from territory.md's phasing, the phone page's open items and the developer questions."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-05T00:00:00Z
---

**World:** U = any map, A = needs real coordinates (Earth or an anchored image),
E = Earth only ([worlds](/concepts/sections/gis/worlds.md)).

# The engine (`src/gis/`)

| | item | world | note |
|---|---|---|---|
| ✅ | sources as values: projection, metric, wrapping, edges | U | `source.hpp`; the flat half has no caller yet |
| ✅ | web-mercator and flat projections | U | `projection.hpp`, `gis_smoke` |
| ✅ | marker outlines (pin, balloon, circle, square, diamond) | U | `marker.hpp`, 2026-10-04 |
| ✅ | a continuous-zoom viewport | E | `view.hpp`; mercator only |
| ✅ | one containment test (rectangle, ellipse) | U | `domain/bestow.hpp` |
| ⬜ | the viewport over a flat world | U | when a flat world first has a phone |
| ⬜ | point-in-polygon | U | with polygons (Q102) |
| ⬜ | georeferencing: control points → a transform | A | the first piece of the map builder; unlocks GPS on a floor plan |
| ⬜ | formats: GeoJSON / GPX in and out | E (A through a transform) | import as a holiday, export as a render |

# The views

| | item | world | note |
|---|---|---|---|
| ✅ | the desktop canvas: pan, wheel and + / - zoom, markers labelled by name, rules | U | [territory](/concepts/sections/gis/territory.md) |
| ✅ | layers (were views): eye, order, name, opacity, own positions, duplicate; four docked windows (Inspector, Overview, Layers, Actions) | U | 2026-10-05 |
| ✅ | find and filter in the one search grammar | U | 2026-10-05 |
| ✅ | a region gives several tags, edited as chips | U | 2026-10-05 |
| ✅ | add a layer to a newsletter or website from the map (Actions) | U | 2026-10-05 |
| ⬜ | a layer that holds only what carries a tag: the UI | U | the `filter` field exists and is honoured; no UI by the author's choice |
| ✅ | regions (rectangle, ellipse), colour, a tag they give | U | both front-ends |
| ✅ | reference points and the fan-out | U | both front-ends |
| ✅ | notes on the map | U | both front-ends, 2026-10-04; never exported |
| ✅ | the PNG export (supersampled pins, watermark) | U | desktop |
| ✅ | the website's read-only map widget | E | CARTO tiles; skips contacts and notes |
| 🔨 | **the phone's map** | U | built 2026-10-04, run in the harness, **not on a device** ([phone](/concepts/sections/gis/phone.md)) |
| 🔨 | the phone's location, accuracy circle, follow | A | built; the system prompt and a real fix are unwitnessed |
| ⬜ | ghosted layers of other visible layers on the phone | U | desktop has them |
| ⬜ | share a map picture from the phone | U | needs the platform's share intent |
| ⬜ | polygons, drawn by tapping corners (Q102) | U | never by crosshair |
| ⬜ | clustering on the desktop | U | the phone has it |
| ⬜ | a continuous zoom on the desktop | E | `view.hpp` is ready for it |
| ⬜ | live containment highlight before a region's tag is applied | U | territory, 2026-07-24 |
| ⬜ | image markers (photos as markers), behind the render seam | U | territory #2 |
| ⬜ | the Builder's `map` block referencing a view | U | territory, "still directed" |
| ⬜ | smarter label placement | U | see research, "to read next" |
| ⬜ | scale bar and distances in the world's unit | U | metres today, only drawn on Earth |
| ✖ | rotation (heading-up) | — | decided against: not navigation |

# The data

| | item | world | note |
|---|---|---|---|
| ✅ | the location facet on contacts, organizations, events, incidents, notes | U | `geo`, `geo_<channel>`, `ref`, `ref_off` |
| ✅ | `place` / `move` as named actions, one definition for the CLI and the canvas | U | `map_actions.hpp` |
| ✅ | per-view positions (channels), locking as channel equality | U | territory |
| ⬜ | a GPS place remembers its accuracy (`geo_acc`) (Q100) | A | |
| ⬜ | the channel asymmetry: a region's tag reads the active channel, the tag editor reads `geo` | U | territory, 2026-09-15 |
| ⬜ | conditions with a radius, and derived influence (T3) | U | |

# The world outside (holidays)

| | item | world | note |
|---|---|---|---|
| ✅ | online tiles with a disk cache; Android's HTTP on a phone | E | OSM, CARTO |
| ⬜ | offline areas for field work | E | not from OSM's servers (their policy); an extract or vector tiles |
| ⬜ | geocoding (an address to a point), opt-in | E | an Antfarm holiday |
| ⬜ | weather and external conditions as a layer (T3) | E | |
| ⬜ | anchored image worlds (a floor plan, a site map) | A | see worlds, "what an anchored world would take" |
| ⬜ | authored worlds and the map builder ("Void Maps") | U | out of Hormiga's scope as a *tool*; Hormiga consumes the result |
| ⬜ | location-personalized newsletters (T4) | A | the north star |

# The boundary

| | item | note |
|---|---|---|
| ✅ | the GIS folder in the OKF | 2026-10-05: this folder |
| ⬜ | Void GIS as its own application (Q42) | not yet; the engine stays separable by keeping `src/gis/` dependency-free |
