---
type: Index
title: GIS — the map, and everything spatial
description: "A folder since 2026-10-05. Everything Hormiga knows about places: the Territory section (the desktop map), the map on a phone, which ideas only exist on Earth and which exist on any map, the research the map's design rests on, and its do-list. The first step toward a separable GIS: modular in the OKF now, not yet in the code."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-05T00:00:00Z
---

# GIS

**Everything spatial, in one place.** The author, 2026-10-05: *"the map should
have its own folder in the OKF, and that map related things should kinda be
under a GIS general okf stuff ... we previously thought that we could separate
the GIS aspect of hormiga into its own application. we won't do that yet, but
we should begin modularizing in the okf. I do plan on there being a lot more
features in the map."*

So this folder is the map's **documentation boundary**, drawn where a code
boundary would one day be drawn. It does not move any code and it does not
create Void GIS. It makes the eventual question ("what would leave with it?")
answerable by reading one folder.

# The boundary

[Application boundaries](/concepts/foundation/application-boundaries.md) §4
splits the map into four layers, and this folder is organized around them:

| layer | what | where it is documented | where it is in the code |
|---|---|---|---|
| 1. interaction vocabulary | gestures: pan, pinch, long press, quick zoom… | Void Maiz's `touch.md`; how Hormiga uses them in [phone](/concepts/sections/gis/phone.md) | Void Maiz (`TouchGate`) |
| 2. **the GIS engine** | worlds, projections, tiles, viewport, marker geometry, distance, containment | [worlds](/concepts/sections/gis/worlds.md), and the engine notes in [territory](/concepts/sections/gis/territory.md) | `src/gis/` (the strictest folder in the tree: it may include nothing but itself), `domain/bestow.hpp`, `domain/map_actions.hpp` |
| 3. the map views | the desktop canvas, the phone screen, the PNG export, the website widget | [territory](/concepts/sections/gis/territory.md) (desktop, exports), [phone](/concepts/sections/gis/phone.md) | `ui/map.cpp`, `phone/phone_map*.cpp`, `render/map_png.cpp`, `render/web/app.js` |
| 4. Void GIS, an application | a standalone host over layer 2 | not started (Q42) | — |

**The rule for new map work** (unchanged from application-boundaries §5): build
it against the seam, not against `HormigaApp`. Anything that is geometry or a
world's rule goes in `src/gis/` with a test in `gis_smoke`; anything that is a
command goes through `map_actions`; a view only draws and compiles commands.

# Reading order

1. [Worlds](/concepts/sections/gis/worlds.md) — **read first.** A map in
   Hormiga is not assumed to be Earth. Which ideas exist on any map (a pin, a
   region, a label), which need real coordinates (GPS, geocoding, metres,
   weather), and which need only an anchor to them (a floor plan). The rule
   that keeps Earth-only features from appearing on a fantasy map.
2. [Canvases with data](/concepts/sections/gis/canvases.md) — **2026-10-06.**
   Earth is one canvas; a floor plan in metres is another, with its own layers,
   regions and positions. How geometry meets data (placed, contained, being),
   data categories as tag namespaces, and the grocery store demo.
3. [Territory](/concepts/sections/gis/territory.md) — the map as a section:
   location-faceted runes, the source as a holiday, layers as tags, views and
   channels, rules, shapes, reference points, exports, notes on the map, and the
   redrawn pins. The desktop map.
4. [The map on a phone](/concepts/sections/gis/phone.md) — the phone's own
   interaction model, every desktop feature with what it became, location, and
   what Void Maiz gained for it.
5. [Research](/concepts/sections/gis/research.md) — what the field and HCI
   literature says, what each finding decided, and what to keep in mind (and
   read next) before adding a feature.
6. [Roadmap](/concepts/sections/gis/roadmap.md) — the do-list, with ✅ / 🔨 / ⬜.

# Status (2026-10-05)

The desktop map is a mature section (views, channels, rules, shapes, reference
points, PNG and web exports). The phone has a map since 0.1.11, run in the
harness and not yet on a device. Markers are one outline across every surface.
Every world Hormiga ships is Earth (OSM and CARTO tiles); the flat-world half of
the engine is tested and has no caller yet. Open: Q42 (Void GIS), Q100
(storing a GPS place's accuracy), Q101 (notes on public maps), Q102 (polygons),
in [developer questions](/developer_questions.md).
