---
type: Concept
title: Interactions — how drawn things meet
description: "Opened 2026-11-14. How paths, areas, perimeters and placed things relate (touch, cross, share a run, lie within, come near), measured on demand and never stored; what a path gives to what lies within its reach; why interactions are derived rather than Void Core bindings; and what Allomone will be able to ask about them."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-11-14T00:00:00Z
---

# The question

The author (2026-11-14): *"how things are stored on a system of interaction at
its core. How will paths 'interact' with each other? If they overlap? If they
are near each other?"*

# The vocabulary is the field's

Geographic information systems name how two shapes relate by how their
interiors, boundaries and exteriors meet: the **dimensionally extended
nine-intersection model** (DE-9IM, Clementini and Egenhofer), standardized in
the OGC Simple Features as **equals, disjoint, touches, crosses, overlaps,
within, contains, intersects**. Hormiga uses those words, so a rule, a tooltip
and a GIS person mean the same thing by them.

`src/gis/relate.hpp` measures, for a point, a line (a path) and a ring (an area
or a perimeter), the quantities those names are made of:

| measure | meaning |
|---|---|
| `distance` | how far apart, 0 when they meet |
| `crossings` | points where two outlines cross properly |
| `shared` | the length two lines run together (a shared run, along, not across) |
| `a_in_b`, `b_in_a` | one wholly inside the other |

and names the relation: **within, contains, overlaps** (a shared run), **crosses,
touches, near** (ours: apart by at most a distance asked about), **disjoint**.
Two routes down the same street *overlap* by that street's length; two that
cross at a corner *cross* once; a route inside a neighbourhood is *within* it.
On Earth the plane is local metres (`local_metres`), so a distance is a
distance.

# Derived, never stored

**A relation is worked out when something asks, from the shapes as they are
now.** It is not a fact in the database, for the reason tags given by a shape
are not ([bestow](/concepts/foundation/data-model.md)): stored, it would be a
second copy of what the geometry already says, and stale the moment either
shape moved. The geometry is stored (vertices, and a generated shape's recipe
as links: [generation](/concepts/sections/gis/generation.md)); how two pieces of
it meet is computed.

**Why not Void Core bindings.** Void Core has a reactive kind of link, a
*binding*, that fires (interaction nets). Interactions between shapes are not
that: nothing should *happen* because two paths cross; a person, a rule or a
view asks whether they do. Derived is also what scales: a binding per pair of
shapes is quadratic in what is drawn, while a question is asked about the
shapes in view or in one area (nothing O(database)). If a time comes when a
crossing should *do* something (a route's volunteers told that another route
now shares their street), that is an Allomone rule over these relations, and
then, if it must act, a binding on the one rune that acts.

# What a path gives

Shapes give their tags (and colours) to what they cover
([territory](/concepts/sections/gis/territory.md)). A path has no inside, so:

- **A path gives to what lies within its `reach`** (metres on Earth, the plan's
  unit on a plan): a canvass route with reach 40 gives `route:north` to every
  house within 40 m of it. Blank reach gives only to what it touches.
- **A path meets the shapes it touches or comes within reach of**, and they give
  to each other as overlapping areas always have.
- **Two paths that cross or share a run meet**: each gives the other its tags.

Built and tested (`spine_smoke`: a point 14 m from a path with reach 40 is given
its tag, one a kilometre away is not).

# What Allomone will ask

Allomone's geography inputs are `near`, `near-rune` and `located`
([adoption](/concepts/allomone/adoption.md)). The relations above are the
natural next ones, as derived inputs a rule can read (planned):

| predicate | true when |
|---|---|
| `on-path "route,40"` | within 40 of the path `route` |
| `crosses "route"` | a path crosses `route` |
| `shares "route,100"` | a path runs along `route` for at least 100 |
| `within "zone"` | wholly inside the area `zone` |

Each is answered from the shapes in the area asked about, never a scan of the
database.

# On the ground, overlap becomes exact

Two paths traced on [the ground](/concepts/sections/gis/ground.md) follow the
same street edges when they share a street, so once streets exist, *shares a
run* can be answered by the edges two paths have in common, exactly and
without tolerances. The measured version here is what free space and hand-drawn
paths need, and stays the fallback.
