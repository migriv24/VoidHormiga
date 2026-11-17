---
type: Concept
title: Generation — geometry made from what is selected
description: "Built 2026-11-14. One system for every shape made from other things: seeds (what was selected, in order), a ground (what the canvas lets you travel on), and generators composed of the two (perimeter, path, loop). How a generated shape is stored in Void Core (vertices plus a recipe of fields and ordered links), how it is regenerated, how long work gets a bar, and how a new generator or a new ground is added."
tags: [status:current, audience:all, confidence:measured]
timestamp: 2026-11-14T00:00:00Z
---

# What the author asked

> *"let's take a closer look at the system that creates [the perimeter]. Making
> sure that the system could generalize to other geometric generation as well.
> ... 'create path' between N set of points. So we have areas, but also points
> are like contacts, orgs, and kinds in general. If we select 3 of them we could
> either 'generate perimeter' OR 'generate path'. Which would use the sequence of
> the points selected to create a path. ... the modules and code that create
> this SHOULD work for both."*

> *"We need to consider how the path generation code fits into the void core
> structure as well. Not just the CLI, but also like, how things are stored on a
> system of interaction at its core."*

# Three pieces

`src/gis/generate.hpp`, pure and dependency-free like the rest of the engine:

- **Seeds**: what was selected, **in the order it was selected**. Each is a
  **point** (a contact, an organization, an event, any placed kind: where it is
  drawn) or an **area** (a shape: its ring, and its centre). A generator reads
  seeds, never runes.
- **A ground** ([the ground](/concepts/sections/gis/ground.md)): `snap` a point,
  `trace` from one point to another, `cover` a region with blocks, and
  `cover_cost`, what a cover will cost before it runs. Two exist:
  - **free space**: a trace is the straight segment; a cover is the region itself;
  - **a plan's grid**: a trace is the staircase along the grid's lines (across,
    then down, the same every time: the author's *"step map"*); a cover is the
    outline of the cells the region holds, holes filled.

  Earth's **streets** will be the third, with the same four calls.
- **Generators**, each a composition of the two:

  | generator | made of | makes |
  |---|---|---|
  | `perimeter` | `cover(padded hull of every seed)` | a ring, drawn as a perimeter |
  | `path` | `trace(stop 1, stop 2) + trace(2, 3) + …` | a line through the stops in order |
  | `loop` | a path back to its first stop | a closed line (a round) |

  A point is a stop where it is drawn; an area is a stop at its centre. A path
  given a path as a seed goes through that path's vertices.

**Adding one is a composition, not a subsystem.** A corridor (a path grown into
a ring) would be `cover(buffer(path))`; a nearest-first tour would order the
seeds before `path`. A new ground (streets, a plan's drawn aisles) makes every
generator work on a new kind of map without touching them.

# Where they meet the database

`src/domain/generated.hpp` turns runes into seeds and results into commands. It
is called by the canvas's menus and by `map generate` (`map_actions.hpp`), so a
click and an agent write the same transcript (CLAUDE.md rule 3):

```
map generate which=path name=route seeds=hall,library,park
map generate which=perimeter name=zone seeds=aisle-1,aisle-2 canvas=store ground=1
```

**The ground follows snap.** With snap on, a generator stands on the canvas's
ground (a plan's grid; Earth's streets once loaded); with snap off, on free
space. The author tied them together: generation *"will greatly utilize the
snap to grid stuff"*.

**The order of a selection.** Shift-click adds one thing at a time, in the
order clicked, and that is a path's order. A shift-drag box adds many at once;
they join chained nearest-first from what was selected last.

# Stored as Void Core stores things

A generated shape is an ordinary `mapshape`, plus a recipe:

| what | where | why |
|---|---|---|
| its vertices | `pts`, and its box in `geo1`/`geo2` | it never needs the ground again: replay, the phone, the PNG, an older Hormiga all draw it from these |
| its kind | `kind = polygon` (a perimeter) or `path` | a path is a line: no area, no fill, hit along its length |
| its generator | `made_by` (`perimeter`, `path`, `loop`) | to run it again |
| its ground | `ground` (`free`, `grid`, `streets`) | to run it again on the same ground |
| its seeds | one **link** to each: `stop-1`, `stop-2`, … for a path; `around` for a perimeter | see below |

**Why links, and why the order is in the relation's name.** A seed's *name*
follows its title, so a field holding names would point at nothing after the
next rename ([naming](/concepts/foundation/data-model.md); the Builder's blocks
were bitten this way). Void Core **repoints links on a rename and drops them on
a removal** (Void Core, *links*), so a link is the one reference that stays
true. An edge carries a relation, a direction and a weight, and a weight is a
magnitude, which a place in a sequence is not; so a path's order is the
relation's label (`stop-3`), and anything richer about a stop would be a rune of
its own, as Void Core advises.

**Regenerate.** A generated shape's menu has *Regenerate from what it was made
from*: its recipe is read back (`recipe_of`: the stops in the order of their
numbers), run again on its ground with its seeds where they are now, and
written as new vertices. A seed that was deleted took its link with it, and the
rest still make a shape. Nothing regenerates by itself (Q134).

# Long work, and the bar

`prepare` reads the database on the thread that owns it and asks the ground
what the work will cost; `make` (pure) may run on a worker; `finish` writes the
commands. Past 200,000 units (a grid's cells; a street ground's tiles) the work
goes behind the busy curtain with a determinate bar (the author: *"always
remember to use loading bars"*). Free space and a plan's grid at a person's
scale take microseconds and show none.

# What is built, and what is not

| | |
|---|---|
| ✅ | seeds, free and grid grounds, perimeter, path, loop (`gis_smoke`) |
| ✅ | `map generate`, the recipe as fields and ordered links, `recipe_of`, `regenerate` (`spine_smoke`: written through Core, read back in order, regenerated after a stop moved) |
| ✅ | the canvas: *Generate perimeter* and *Generate path* on several selected; *Regenerate* on a generated shape; paths drawn on the desktop, the phone and the PNG, and edited by their points |
| ✅ | the bar for long work (wired; nothing built yet is long enough to raise it) |
| ⬜ | the streets ground ([the ground](/concepts/sections/gis/ground.md)) |
| ⬜ | a loop on the menu (`map generate which=loop` works) |
| ⬜ | paths on the website ("complex features of the map are currently unsupported for web export", the author) |
| ⬜ | Q133, Q134 ([developer questions](/developer_questions.md)) |

How generated shapes, paths above all, meet each other and everything else is
[interactions](/concepts/sections/gis/interactions.md).
