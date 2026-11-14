---
type: Concept
title: The connections view — a graph drawn in bounded work
description: "The Data tab's force-directed view of how runes link: groups found from the links, opened one at a time, at most 1,500 bodies on screen, Barnes–Hut forces; links that leave the loaded part counted, not drawn; built 2026-11-05 after the stress database's 6×10^4 loaded runes froze the old view."
tags: [status:current, audience:all, confidence:measured]
timestamp: 2026-11-05T00:00:00Z
---

# What it is

The Data tab's **connections view** is a picture of how runes link to each
other. It is a visualization, not the node editor: its positions are view
state, never dispatched and never stored.
See [workspace & sections](/concepts/sections/workspace-and-sections.md),
"The physics graph view" (2026-07-22).

# Why it was rebuilt (2026-11-05)

After the stress database opened (5×10^5 runes, 59,719 of them held), the old
view froze. It had two faults:
- **Its repulsion was all-pairs, through string-keyed maps.** That is
  3.6×10^9 steps an iteration at 6×10^4 runes, four iterations a frame.
- **It reset itself every frame on a partial load.** A link to an unloaded
  rune added a position for a name it did not hold, so its position count never
  matched the scene, and every frame started the layout again.

# How it works now

**It draws what Core holds**, which the store's plan keeps within the
machine's budget ([store](/concepts/platform/store.md)). Its cost is then
bounded twice: by the plan, and by what it shows.

- **Groups found from the links.** Void Maiz's overview tree
  (`voidmaiz/overview.hpp`, its `okf/concepts/overview.md`) works in four steps:
  1. Leiden finds communities, split until a group opens onto at most 250
     things;
  2. communities merge along their strongest ties, under a size cap, until at
     most 400 bodies show with nothing open;
  3. what modularity will not merge, because nothing links it, is **bundled**;
  4. a group still too wide is **sliced**.

  A view can say which groups mean something (a community) and which are only
  packing (a bundle, a slice). A graph of at most 400 runes gets no groups and
  is drawn rune by rune, as before.
- **Opening is the gesture.** Click a group to open it in place, and right-click
  a body to close the group it is in. At most **1,500 bodies** are on screen;
  opening past that closes the oldest-opened groups first.
- **Forces are Fruchterman–Reingold with Barnes–Hut repulsion**, O(n log n + m)
  an iteration, with a body's mass growing as the log of its size. The layout
  settles within 6 ms a frame.
- **Links that leave the loaded part are mojones**
  ([frontier](/concepts/platform/frontier.md)): counted on the rune and on every
  group above it (`+37`), never drawn to nowhere. On a partial load, a selected
  rune with mojones offers *Load the part around it*.
- **Rebuilt off the UI thread** when the scene changes. Open groups and
  positions carry over by name.
- **Proximity springs** (the map's distance, the calendar's dates) are drawn only
  when the graph is drawn rune by rune, at most 2,000 runes. They are pairwise.

# Measured (2026-11-05, the stress database, 59,719 runes held)

| | |
|---|---|
| reading the scene into the view's graph | 48 ms, once per change |
| frames while the view shows it | median 6.0 ms, p95 6.3 ms |
| linked runes / bodies with nothing open | 58,674 / 379 |

# What it cannot show yet: the whole database

The view shows the corral, the part that is loaded. An overview of the
**potrero**, the whole database, is the same picture one level up: a body per
store cluster, with the links between clusters. Rancho has the clusters, but
not that picture:
- **Its cluster tree has no upper levels.** The stress database has 23,607
  top-level clusters, most of them a handful of runes, so no overview can start
  from its root.
- **Nothing counts the links between clusters.** That is the *reseña* (the
  frontier's per-cluster summary). Computing it means a pass over every edge,
  which belongs in the store, done once at clustering time.

Both are asked of Void Rancho (`hormiga-the-overview-2026-11-05`). Until then,
the strip's *Other parts...* list and its search are the way into the rest.
