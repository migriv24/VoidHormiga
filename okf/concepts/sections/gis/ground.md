---
type: Concept
title: The ground — what a map lets you travel on
description: "Designed 2026-11-14. Snapping to streets, paths that follow streets, blocks, and perimeters that form around streets all need the geometry under the map, not its picture. Decided: possible offline from an OpenStreetMap extract a person imports as a file; impossible from the raster tiles the map draws. One graph per canvas (streets on Earth, the grid on a drawn plan) and five operations on it; the lowest surface grade wins; the .miga keeps a ledger of what was loaded; OpenStreetMap is credited everywhere. The plan's grid is a full ground; the street half is planned."
tags: [status:planned, audience:all, confidence:researched]
timestamp: 2026-11-14T00:00:00Z
---

# What the author asked (2026-11-14)

> *"snap to grid features, however, instead of snapping to a 'grid' we need to
> abstract this a bit further. for google maps based maps, i would want it to be
> 'snap to street' ... For grocery store maps or any other abstract map ... maybe
> it's literally just a grid. ... Roads can be curvey though. roads can go in on
> themselves. ... we are using an offline library, and we don't wanna use an API
> ... if this isn't possible, that should be decided first."*

> *"instead of making a straight line from one house to another, it actually makes
> a step map ... a path that uses the road geometries itself ... we aren't looking
> for 'the best path' or like 'the fastest path' though, we are simply looking at
> the map like a piece of geometry with different rules for 'travel'."*

> *"shift select multiple areas, and right click, with an option to 'generate
> perimiter' ... it'll actually use the grid to generate the geometry (the map
> roads and stuff) ... we want to use the domain underneath the actual maps."*

> *"always remember to use loading bars if those are being generated."*

# Decided first: is it possible?

**Yes, from street data. No, from the map Hormiga draws today.**

- The base map is **raster tiles** (OSM, CARTO): pictures of streets. A picture
  has no street in it to snap to. Recovering streets from pixels would be wrong
  wherever a label, a bridge or a style covers them, and would lean on tile
  servers in ways their usage policies forbid
  ([research](/concepts/sections/gis/research.md), "the ground").
- The geometry the tiles are drawn **from** is OpenStreetMap's vector data:
  every street a polyline of nodes, intersections as shared nodes. It is
  published as **extracts**: one file per region (`.osm.pbf`), free under the
  ODbL, with attribution.
- **The author chose (2026-11-14): a person imports a file.** *"Load street
  data…"* takes an extract they downloaded once (Geofabrik, BBBike, or their
  own). Hormiga makes no network request for it. A street network is a public
  fact, so this is not member data, but it is a large one, and it is the
  person's decision when to fetch it.
- **The precedent:** QGIS has shipped exactly the author's "path that uses the
  road geometry" since 2.14 as its *tracing* tool: snap to existing vector
  features, then Dijkstra along them between the clicked points. It works only
  on vector features, never on imagery, which is the same line drawn here.

# The ground: one graph per canvas

The **ground** of a canvas is a graph embedded in its plane: **nodes** (a
street's ends and its intersections) and **edges** (the polyline between two
nodes), each edge with a **class** (what kind of way it is) and a **grade**
(its height relative to the roads it crosses: a bridge's above, a tunnel's
below; [roads](/concepts/sections/gis/roads.md)). *Grade*, never *layer*: a
layer is one of the map's drawing layers. Everything the author asked for is an operation
on this graph, so it is written once, in `src/gis/` (dependency-free, tested in
`gis_smoke`), and each world supplies its own graph
([worlds](/concepts/sections/gis/worlds.md)):

| canvas | its ground | built |
|---|---|---|
| a drawn plan (a store, a hall) | its **grid**, `Canvas::grid` in its unit: a lattice | ✅ 2026-11-14: snap, trace (the staircase), cover (whole cells) |
| Earth | its **streets**, from an imported OSM extract | ⬜ |
| a plan, later | its drawn aisles as edges | ⬜ (no one has asked) |

So a grocery floor and a city answer the same five questions, as the author
wanted (*"keep this abstract so the techniques we use could be potentially
useful in other areas"*).

# The five operations

1. **Snap.** A point goes to the nearest point on the nearest edge within a
   radius, preferring a node (an intersection, a corner) inside a smaller one.
   On a grid this is rounding. Off by default; a toggle on the canvas.
2. **Trace** (the step map). A path from A to B **along edges only**: both
   points snapped onto the graph, then the shortest path by length (Dijkstra,
   or A* with straight-line distance as its estimate). Shortest by length is
   chosen because it is deterministic and geometric, not because it is "best":
   the author asked for neither the best nor the fastest path. **Travel rules**
   are which edge classes may be used (any way, walkable, drivable) and nothing
   else: no speeds, no turn costs, no one-way streets, because this is geometry,
   not navigation ([research](/concepts/sections/gis/research.md): Hormiga is
   not navigation). On a grid the path is the Manhattan staircase.
3. **Polygons that follow streets.** With snap on, each edge of a polygon
   between two clicked corners is a *trace*, so the polygon forms itself around
   the streets. This is QGIS's tracing, inside the polygon tool.
4. **Blocks.** The faces of the street graph: the areas the streets enclose.
   Found by walking half-edges (at each node turn to the next edge clockwise),
   the doubly connected edge list of computational geometry.
   **Streets are not quite planar:** a bridge crosses a street without meeting
   it. Boeing (2020) measured how far city networks are from planar and how a
   planar model misstates them. **The author's ruling (2026-11-14): the lowest
   surface grade wins.** Where two roads cross without meeting, the lower one
   bounds the blocks and the higher one does not, so a freeway over a
   neighbourhood leaves its blocks whole; a tunnel is below the surface and
   never bounds a block, though a path may go through it. Two roads at one
   grade that cross without a shared node are split where they cross. A dead
   end inside a block is a spur, walked down and back, and it stays inside its
   face. The cases this has to survive, each with a real place:
   [roads](/concepts/sections/gis/roads.md).
5. **Perimeter around regions** (item 6). The regions (shapes and markers),
   which need not touch, are enclosed by their padded hull, covered by the
   ground's blocks ([generation](/concepts/sections/gis/generation.md): on free
   space the hull, on a plan's grid the cells it holds, built). **On the ground:** every block that the hull meets
   is taken, and the outer boundary of their union, with holes filled, is the
   perimeter. It follows streets, and it spans the gaps between regions because
   the hull does. Named as made ("Around A, B and 2 more"), drawn as a
   perimeter.

Every result is **an ordinary polygon**: its vertices are written with
`shape::vertex_commands`, so the database, replay, the phone, the PNG and the
website never need the ground. Only *making* a shape needs it (CLAUDE.md
rule 3: replayable headless).

# The data, and nothing O(database)

**Reading.** An `.osm.pbf` is a sequence of zlib-compressed blobs of
protobuf-encoded blocks: string tables, *dense* nodes (delta-coded ids and
coordinates), ways (delta-coded node references), relations. Hormiga needs the
ways tagged `highway=*` (with `bridge`, `tunnel`, `layer`) and the coordinates
of their nodes. The zlib decoder is already vendored (`stb_image.h` carries
one), so the reader is a small file of our own; if it grows past that,
`protozero` (BSD-2, header-only, what libosmium uses) is the vendor candidate
(rule 5).

**Scale.** A city's extract is tens of megabytes; a US state's is hundreds, with
10^7 nodes. That is far more than the infinite-dataset premise allows in
memory, so:

- **Import is a streaming pass** that writes a **tiled index** on disk (tiles
  of a fixed zoom, each holding its edges' polylines, with edges that cross a
  tile in both), under the busy curtain's **determinate bar, measured in bytes
  read** ([busy](/concepts/platform/scale.md)). Memory is bounded by a tile,
  not by the region.
- **Every operation is bounded by an area**, and reads only the tiles that
  cover it: snap reads the tiles around the pointer; trace searches outward
  with a cap (a path not found within it is reported as not found nearby, an
  *open* answer, never a scan of the state); blocks and perimeters read the
  tiles under the hull plus a margin. Cost grows with the area asked about,
  which is what the author expected (*"compute time increases based on how
  large the area is"*), and never with the database or the region file.
- **Bars.** Import: bytes. Perimeter: tiles done of tiles needed. Trace: a
  spinner with the area searched, because its end is not known in advance. The
  geometric perimeter built today takes microseconds and shows none.

**The ledger: what was loaded, from where, and how often (the author's answer
to Q131, 2026-11-14).** *"the miga should store ... some way of tracking the most
loaded areas and the things that should be downloaded when the miga is loaded.
We should be tracking what we are downloading. Which chunks are most frequent
... for smarter loading between shared files. And also smarter loading when
reopening the same miga file. We should be tracking where we are downloading
things from."* So the `.miga` carries a **ground ledger**, small and always
travelling with the database, while the street tiles themselves are a cache
beside it that can be rebuilt:

| in the ledger | why |
|---|---|
| every **source**: the extract's file name, where it came from (the provider and the address a person got it from), its date and its hash | a database says where its streets came from, and another machine can tell whether it has the same file |
| every **chunk** (a tile) ever loaded, with how often and when it was last used | the hot chunks load first when the database opens, and are the ones worth preparing on a machine the database is shared to |
| the **preload set**: the chunks to have ready on open | reopening the same `.miga` starts where the work is |

The counts are view state (config tier, undo-exempt, never a command per load):
they are written when the database is saved, not every time a tile is read. A
database shared to a machine without the extract says which file it needs and
where it came from, and the person imports it there (no download by Hormiga,
the author's earlier choice). This is the same idea as the store's plan of what
to load first (Void Rancho's working set, by budget and heat) applied to space;
whether the ledger becomes Rancho's is a question for Rancho once the chunks
exist.

**Credit (the author's answer to Q132, 2026-11-14).** *"we can be liberal in
crediting open street map. We are already open source, so let's just credit it
everywhere."* The canvas, the phone and the website's map already show the
base map's credit; the PNG export now stamps "(c) OpenStreetMap contributors"
on every Earth picture (built). When street data is loaded, every surface
that draws a shape made from it carries the same line, and the About and
third-party notices name OpenStreetMap and the ODbL.

# Built, and the order of the rest

| | step | note |
|---|---|---|
| ✅ | polygons, perimeters, selecting several, *Generate perimeter* (padded hull) | 2026-11-14, [territory](/concepts/sections/gis/territory.md) "Polygons and perimeters" |
| ✅ | snap to a plan's grid (corners, vertices, handles) | off by default; on Earth the toggle says street data is needed |
| ✅ | the generation system: seeds, grounds, generators (perimeter, path, loop) | [generation](/concepts/sections/gis/generation.md), 2026-11-14 |
| ✅ | the plan's grid as a full ground: the staircase trace, whole-cell covers | the abstraction proved on a second world, before streets |
| ✅ | OpenStreetMap credited on the PNG | Q132 |
| ⬜ | 1. the PBF reader and the tiled index; *Load street data…* with its bar | `gis_smoke` gets a small hand-made extract |
| ⬜ | 2. the streets drawn over the map, as an overlay that can be hidden | so a person sees what snapping will use |
| ⬜ | 2b. the ground ledger in the `.miga` (sources, chunks, heat, preload) | Q131's answer |
| ⬜ | 3. snap to street | the toggle comes alive on Earth |
| ⬜ | 4. trace, inside the polygon tool | travel rules: any, walkable, drivable |
| ⬜ | 5. blocks (faces), with bridges and tunnels left out | |
| ⬜ | 6. *Generate perimeter* on the ground | behind the busy bar |
