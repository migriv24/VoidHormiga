---
type: Concept
title: Canvases with data — Earth is one canvas, a floor plan is another
description: "The author's 2026-10-06 direction: maps generalize into canvases with data, as documents generalize newsletters and websites. A canvas is a world (Earth, a drawn plan in metres, an authored world in its own units) with its own layers, its own geometry and its own positions. Geometry meets data three ways: a thing placed on it, a region that gives a category to what is inside, and a region that is a thing. The first drawn canvas is an indoor layout; the first demo is a grocery store with aisles and products. What is built, and what waits."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-06T00:00:00Z
---

# What the author asked

> "Since currently we have maps as abstracted, we want to generally move towards
> 'canvases with data'. Similar to documents ... Next I wanna work on a 2D map
> builder specifically. I want this to essentially be a 2D plane where we are
> placing geometry and associating geometry with data and such. However, more so
> with data categories. There'll be a difference between drawn maps and stuff.
> Essentially a normal earth map is a very useful canvas. And we would wanna make
> maps of anything. I'm more so considering indoor layouts right now, of
> different rooms and locations. Another good template demo would be of a grocery
> store with aisles and product locations."

Two things follow. **Earth stops being the map and becomes one canvas.** And a
canvas is not decoration: its geometry exists to say something about the data
(this aisle holds these products, this room holds these devices, this hall is
where these volunteers are posted).

# A canvas

A canvas is **a world, with its own layers, geometry and positions**. It is a
`canvas` rune (title, world, size, unit, grid). Layers (`map` runes), regions
(`mapshape`) and reference points belong to one canvas through a `canvas` field.
**Blank means the Earth canvas**, so every database made before canvases is
unchanged: its territory is the Earth canvas, as it always was.

| world | coordinates | distance | background | GPS | example |
|---|---|---|---|---|---|
| **Earth** | latitude, longitude | metres (haversine) | online tiles | yes | the territory, a canvassing route |
| **plan** (drawn) | x, y in metres from a corner | metres, flat | a grid, every metre, heavier every five | **no** (until anchored) | a store floor, a house, a shelter, a community hall |
| **authored** | x, y in the world's own unit | the unit ("ft", "squares", "u") | a grid, or an image | never | a D&D map, a fictional city |
| *anchored* | a plan or an image, pinned to Earth by control points | metres, through the transform | the image | yes, through the transform | a resource fair's site map |

The engine already had the flat world (`gis/source.hpp`, `Flat`, `TileScheme::None`,
`Metric::Euclidean`) with no caller; a plan canvas is its first. The rule from
[worlds](/concepts/sections/gis/worlds.md) holds unchanged: **a feature asks the
world whether it can exist there and is absent where it cannot.** No locate
button on a floor plan; distances in metres on a plan and in the world's unit on
an authored map; no tile picker where there are no tiles.

## Positions belong to their canvas

A thing can be on several canvases at once: a grocery store's *organization*
rune sits on Earth at its address, and its *products* sit on the store floor.
Each canvas keeps its own position, in the field `geo_<canvas>` (the position
channel mechanism layers already use). **A drawn canvas never falls back to the
Earth position**: latitude 47.6 is not 47.6 metres into a store. A thing with no
position on a canvas is simply not on it, and *Put on this canvas* is how it gets
there.

# Geometry meets data, three ways

The author: "placing geometry and associating geometry with data ... more so
with data categories".

1. **Placed**: a thing (a product, a device, a person, an event) has a position
   on the canvas and is drawn there as a marker, styled by the layer's rules.
2. **Contained, giving a category**: a region gives tags to whatever is inside
   it (`bestows`, built 2026-07-24, a list since 2026-10-05). On a plan this is
   the main idea: aisle 3 gives `aisle:3` and `dept:snacks` to every product
   placed in it, so "what is in aisle 3" and "where are the snacks" are the same
   query as every other search in the application (`@aisle:3`). Moving a product
   to another aisle and giving again moves its category with it.
3. **A region that is a thing**: a room, an aisle, a shelf is data in its own
   right (a name, a label, notes, tags of its own), not only an outline. Regions
   are already runes with a label and tags; that is the start.

**Data categories are tag namespaces**: `aisle:3`, `dept:produce`, `room:kitchen`,
`zone:intake`. Nothing new is invented for them; the one search, layer rules,
filters and Allomone already understand tags.

# The geometry a drawn map needs

| | shape | status |
|---|---|---|
| ✅ | rectangle (a room, an aisle, a shelf run) | built (`mapshape`, kind `rect`) |
| ✅ | ellipse | built |
| ✅ | a label on a region, a colour, the categories it gives | built |
| ⬜ | polygon by corners (an L-shaped room) | [Q102](/developer_questions.md) |
| ⬜ | a line with thickness (a wall), a door gap | after polygons |
| ⬜ | snapping to the grid while drawing | the grid exists; snapping is next |
| ⬜ | a background image to trace (a floor plan photo) | then anchored worlds |

# Canvases as documents

Websites and newsletters are documents: each its own mantle, opened and edited
in the Builder. The author wants canvases to become the same kind of thing.
**Today a canvas lives beside the data** (its rune, layers and regions are in the
data mantle), because placement and containment work on one scene and moving
them across mantles is a larger change than the canvas itself. A canvas as its
own mantle, opened like a document and published like one, is
[Q106](/developer_questions.md).

# The grocery store demo

The [grocery store workspace](/concepts/platform/workspaces.md) seeds a store
floor: a plan canvas 40 m by 24 m; aisles as regions, each giving `aisle:N` and
its department; produce, dairy and bakery along the walls; checkout lanes at the
front; and products placed in them, each a `product` rune with price, unit and
stock. Search `@dept:dairy`, filter a layer to `@aisle:4`, or ask where milk is.

# What is built (2026-10-06)

Measured in the desktop and phone harnesses; see the [log](/log.md).

| | | |
|---|---|---|
| ✅ | the `canvas` glyph; `canvas` on layers, regions and reference points; blank is Earth | `domain/canvas.hpp`, `seed.hpp` |
| ✅ | positions on a drawn canvas in their own channel (`geo_cv_<canvas>`), **never falling back to Earth** | `view_geo`, tested in `spine_smoke` |
| ✅ | a region gives its categories to what is on **its own** canvas | `bestow::covering`, tested |
| ✅ | the desktop map: a canvas picker beside the search ("New floor plan" in it); the floor drawn with its grid and edge; pan, zoom, regions, reference points, placing and moving on the plan; its own camera per canvas | `ui/map.cpp` |
| ✅ | Layers: on a plan, the floor's name, width, depth and grid where Earth has its base map; own positions per canvas | `ui/map_panels.cpp` |
| ✅ | the phone: the canvas list on the Layers screen; the viewport over a flat world (`SlippyView::flat`, tested in `gis_smoke`); the floor fitted to the screen; no tiles, no credit, no locate button; "Add here" offers a product first | `phone/phone_map*.cpp` |
| ✅ | the `product` glyph, and the grocery store workspace | [workspaces](/concepts/platform/workspaces.md) |
| ⬜ | the PNG and the website's map block of a plan | both draw Earth's tiles today; the Actions window says so on a plan |
| ⬜ | polygons, walls, snapping, a background image to trace, anchored worlds | the geometry table above |
| ⬜ | authored worlds in other units (the D&D campaign's feet and squares) | the plan's machinery with a unit field |
| ⬜ | canvases as documents | Q106 |
