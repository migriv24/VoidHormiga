---
type: Concept
title: Roads — how they are built, their edge cases, and the places that test them
description: "Opened 2026-11-14. What the ground's streets are made of (OpenStreetMap's ways, classes and vertical order), Hormiga's word for that order (grade) and the author's ruling (the lowest surface grade wins), the edge cases that break naive street geometry, each with a real place that shows it, and how those places become tests."
tags: [status:planned, audience:all, confidence:researched]
timestamp: 2026-11-14T00:00:00Z
---

# Why this page

[The ground](/concepts/sections/gis/ground.md) snaps to streets, traces paths
along them and finds the blocks they enclose. Each of those is a few lines of
geometry on a clean grid of streets and goes wrong on a real city. The author
(2026-11-14): *"We should also extensively document concepts and features of
road design ... the edge cases of roads ... using specific areas and streets as
edge cases could be smart. [A small grid city is] a bit of a basic [case]. We
may want to create unit tests on more complex cities and areas."* So: what a
road is in the data, the cases that break things, and a real place for each.

# What a road is in the data

Street data comes from OpenStreetMap extracts (the author's choice: imported
from a file). In it:

- **A road is a *way***: an ordered list of nodes, each a coordinate. A long
  street is usually several ways, split wherever its tags change (a bridge, a
  speed limit, a name).
- **An intersection is a shared node.** Two ways that meet share the node
  where they meet. Two ways that cross without a shared node do **not** meet:
  one passes over the other.
- **`highway=*` is the road's class**: `motorway`, `trunk`, `primary`,
  `secondary`, `tertiary`, `unclassified`, `residential`, `service`, the
  `*_link` ramps, `living_street`, `pedestrian`, `footway`, `cycleway`, `path`,
  `steps`, `track`. Hormiga's travel rules (any, walkable, drivable) are read
  from the class and from `access`.
- **Vertical order is `layer=*`**, with `bridge=*`, `tunnel=*`,
  `location=underground` and `covered=*`. OpenStreetMap is explicit that
  **layer is relative, not an elevation**: it only says which of two crossing
  features is on top *at that crossing*; its default is 0 and it runs about -5
  to 5; a bridge is usually +1 and a tunnel -1, but the tag should be stated
  rather than assumed; and two ways crossing at different layers must not
  share a node, since *"joining the ways with a shared node at the point where
  they cross would destroy the vertical order"*. `level=*` is for floors inside
  a building and is not about roads at all (OSM wiki, *Key:layer*).

# Grade: Hormiga's word for it, and the ruling

**Words (decided by the author, 2026-11-14).** Hormiga's *layers* are the map's
drawing layers ([territory](/concepts/sections/gis/territory.md)) and nothing
else. The vertical order of real roads is their **grade**, the road engineer's
word: roads that cross without meeting are *grade-separated*, roads that meet
on the flat are *at-grade*. The importer reads OpenStreetMap's `layer`,
`bridge`, `tunnel` and `location` into a grade; the word *layer* never names a
road's height anywhere in Hormiga's code or this OKF. (The author: *"We
shouldn't mix up terminologies here."*)

**The ruling: the lowest surface grade wins.** The author: *"there's often
highways and freeways that go over neighborhoods. We want to use the bottom
most layer of things as our baseline."* So:

- **Drawing, snapping and blocks use the lowest road at or above the ground.**
  Where a freeway crosses a neighbourhood street on a bridge, the street is the
  baseline: blocks are bounded by it, snapping finds it, and the freeway above
  does not cut the neighbourhood into pieces.
- **Tunnels do not win** (the author's answer, asked with the case below).
  Taken literally, "lowest" would pick a tunnel wherever one runs under a
  street, and a downtown with a highway beneath it would have its blocks cut by
  a road nobody can see from above. A road with `tunnel=*` or
  `location=underground` is below the surface and is left out of drawing,
  snapping and blocks.
- **Every road still counts for paths.** A path between two points may cross a
  bridge or go through a tunnel; grade decides what the map is *made of*, not
  where one may travel.
- **Grade is compared only where two roads cross.** Because layer is relative,
  a road is never "high" or "low" in general; at each crossing without a
  shared node the higher one is dropped from the blocks there.

# The edge cases

Each row: what it is, what goes wrong with naive geometry, what Hormiga does
(or will), and a real place that shows it ([test areas](#the-test-areas)).

| case | what goes wrong | Hormiga | a place |
|---|---|---|---|
| **Stacked interchange** | Planar face-finding sees ramps crossing as junctions and shatters the area into slivers | grade: only the lowest surface road bounds blocks; ramps are traced, never faces | Huangjuewan, Chongqing: five levels, up to 37 m, 14–15 ramps |
| **A highway in a tunnel under a downtown** | "Lowest wins" picks the tunnel and cuts every block above it | tunnels never bound blocks (the author's ruling) | Boston's O'Neill Tunnel: I-93, 2.4 km under the city centre |
| **A road over itself** | One way crosses itself at different heights; a per-way grade cannot say which part is on top | split at the crossing (as OSM is supposed to be) and compare the parts' grades; a self-crossing with one grade is treated as a junction | Nanpu Bridge's Puxi approach, Shanghai: a spiral ramp 180 m across and 7.5 km long |
| **Roundabouts and ring junctions** | The island inside becomes a tiny block; five mini-roundabouts make a cluster of slivers | blocks under a minimum area merge into a neighbour; a roundabout's ring is ordinary edges | the Magic Roundabout, Swindon: five mini-roundabouts round a central one |
| **Divided roads** | Each carriageway is its own one-way way, so the median is a long thin "block" | a face much longer than it is wide, between two roads of one name and class, joins the road | any divided highway (a place to pin it is still to choose) |
| **A regular grid with cut corners** | None, if the faces are found right: it is the clean case of a non-square block | the face walk handles any polygon; a test checks the blocks are octagons | Barcelona's Eixample: 113 m blocks, every corner cut at 45° |
| **Dead ends and alleys** | A spur pokes into a face; a walk that fails to come back out stops early | the half-edge walk goes down a spur and back; the spur stays inside its block | Fez el-Bali: about 9,400 alleys, car-free, lanes that dead-end and double back |
| **Streets that are stairs** | A legal street no car can drive | class `steps`: walkable, never drivable; it bounds blocks like any street | Pittsburgh: 700+ sets of city steps, 332 of them legal streets |
| **The antimeridian** | Longitude jumps from +180 to −180; a road across it looks 360° long and boxes span the world | coordinates unwrapped about the area asked for before any geometry | Taveuni, Fiji: the only place a road crosses 180° near houses |
| **Ways that should meet and do not** | A gap in the data: two streets touch on the map and share no node | at the same grade, a crossing without a node is joined for blocks; a trace never turns there; a path with no way through says so | wherever an extract has mapping errors; JOSM calls them "crossing ways" |
| **Areas mapped as roads** | A pedestrian square is a polygon (`area=yes`), not a line | left out of the first build; its outline could become walkable edges | to be chosen |
| **Roads that are not there yet** | `construction`, `proposed`, `abandoned` | left out | in any extract |
| **Long ways across tiles** | One way in several tiles is read twice | an edge is named by its way and its two nodes, so a second copy is the same edge | any long road at a tile boundary |
| **Rivers and railways** | In urban morphology blocks are also bounded by water and rail; streets alone make a block that spans a river | the first ground is streets only; waterways and railways as barriers are a later choice | a canal city, to be chosen |

# The test areas

**Two kinds of test, and why both.**

- **Hand-made graphs** in `gis_smoke`, a few dozen nodes each, for exact
  answers: this face has these corners, this path goes here.
- **Real places**: small extracts clipped from OpenStreetMap, a few hundred
  kilobytes each, kept under `tests/data/ground/` with their date and the ODbL
  credit. OpenStreetMap changes daily, so a test pins its file and asserts
  **invariants**, not today's counts: every edge bounds at most two blocks; no
  block is cut by a road that passes over it; no block contains a lower surface
  road; a path between two connected points exists and stays on roads it is
  allowed on; the blocks plus the outer face satisfy Euler's formula for the
  planar part.

**The contenders**, each chosen for what it breaks:

| place | what it tests |
|---|---|
| a small grid city | the baseline: a regular grid, few bridges, almost planar. Everything must be right here first |
| Barcelona, Eixample (and one superblock) | a perfect grid with chamfered corners: octagonal blocks, a superblock as a union of nine |
| Huangjuewan interchange, Chongqing | five grades of ramps: grade separation at its worst |
| Boston, downtown over the O'Neill Tunnel | the tunnel exception to "the lowest wins" |
| Nanpu Bridge, Puxi approach, Shanghai | a road that spirals over itself |
| Magic Roundabout, Swindon | roundabout islands and slivers |
| Fez el-Bali | an organic street network: alleys, dead ends, no cars, walk rules only |
| Pittsburgh, South Side Slopes | steps that are streets: walkable and not drivable, on steep ground with bridges |
| Taveuni, Waiyevo, Fiji | a road across the antimeridian |

**Still to choose:** a canal city (water as a barrier), a mountain road of
hairpins, and a divided highway with frontage roads.

**Not yet built:** the extracts and their tests come with the importer
([the ground](/concepts/sections/gis/ground.md), step 1). Clipping is done once
on a developer's machine (any OSM tool that cuts a bounding box), never by
Hormiga at run time.

Sources: [OSM wiki, Key:layer](https://wiki.openstreetmap.org/wiki/Key:layer);
[Huangjuewan interchange](https://en.wikipedia.org/wiki/Huangjuewan_interchange);
[O'Neill Tunnel](https://en.wikipedia.org/wiki/O%27Neill_Tunnel);
[Nanpu Bridge](https://en.wikipedia.org/wiki/Nanpu_Bridge);
[Magic Roundabout (Swindon)](https://en.wikipedia.org/wiki/Magic_Roundabout_(Swindon));
[Eixample](https://en.wikipedia.org/wiki/Eixample);
[Fes el-Bali](https://story-lecarrousel-rabat.com/blog/travel/fes-el-bali-guide/);
[Steps of Pittsburgh](https://en.wikipedia.org/wiki/Steps_of_Pittsburgh);
[180th meridian](https://en.wikipedia.org/wiki/180th_meridian),
[Taveuni](https://en.wikipedia.org/wiki/Taveuni);
[Boeing, planarity of street networks](https://arxiv.org/abs/1806.01805).
