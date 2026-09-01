---
type: Concept
title: Allomone — inputs
description: "What Allomone reads: the rune graph of a mantle — tags first, then fields, links/relations, graph-spectral measures (a rune's eigenvalue), and change deltas — plus far-future external sources."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). The **input is the database
itself** — the rune graph of a mantle (its runes, their tags and fields, and the
links between them). We list inputs by scope (how much of the graph a condition
must read) and cost, since that shapes both the block palette and performance.

# 1. Tags — the primary signal (local, cheap)

Axis-typed tags (`type:`, `month:`, `status:`, `lang:`, `icon:`, `color:` …) and
bare tags. Tags are the **pheromones** of the system
([paradigm](/concepts/allomone/paradigm.md)) and the first-class condition atom.
Allomone's condition blocks are built directly on the **one tag grammar** — a
boolean condition subtree *is* a filter expression, so the shipped
`draw_tag_filter` widget is already the front half of a condition block. This is
why Allomone is sequenced right after the tag-filter builder.

# 2. Fields — a rune's own values (local, cheap) — BUILT 2026-08-05

Declared field values: `role`, `date`, `bio`, `email`, whether a field is empty,
a numeric value. **Shipped**: read any field as **`rune.<field>`** (e.g.
`rune.date`, `rune.email`) or **`field(rune, "key")`** — "" when absent, so
emptiness is `rune.email == ""`. Values are strings; **ISO dates compare
correctly** (`rune.date >= "2026-08-01"`) because ordering comparisons are
lexicographic on strings. The host decodes each declared field value and passes
it in with the rune. Conditions like "events in August", "contacts with no bio"
now express directly (see the shipped `11-fields` example).

# 3. Links & relations — the neighborhood (bounded graph query)

Edges to other runes: "orgs this contact is `member-of`", "events with a
presenter", "images attached to this event". A condition may **traverse one or
more hops** — a bounded subgraph query. This is where Allomone earns "graph
rewriting": conditions are subgraph *patterns*, not just per-rune predicates.

**A link is NOT a tag (author, 2026-08-05).** The author flagged this as
important and easy to conflate — the UI elsewhere sometimes calls a link a
"connection", which is the same thing. The distinction is primitive:

- A **tag** is a **pheromone** — a value written into the *shared environment*
  that any rune can carry and any condition can read by **set membership**
  (`thing has "type:contact"`). It coordinates stigmergically; no two runes are
  "joined" by sharing a tag.
- A **link / connection** is a **directed edge between two specific runes** —
  identity-to-identity, with a role (`member-of`, `presents`, `attached-to`).
  You **traverse** it (`thing.links("member-of")` → the runes on the far end);
  you don't test it by membership.

So tags answer "*what kind of thing is this?*" and links answer "*what is this
thing connected to?*". They compose — "color contacts (tag) who are `member-of`
(link) a `type:partner` (tag) org" is a one-hop pattern mixing both. **Link
traversal shipped (2026-08-05)**: `neighbours(rune)`, `linked(rune, "rel")`,
`cluster(rune)` (connected component), `degree(rune)` — and, crucially,
Allomone traverses **by structure, not by name** ([traversal](/concepts/allomone/traversal.md)).
The richer graph measures below (centrality, community) are the host-computed
next step.

# 4. Graph-analytic / spectral measures — the global view (expensive, opt-in)

"A rune's eigen value" = **eigenvector centrality** (spectral graph theory —
importance measured by the importance of one's neighbors, the PageRank family),
plus degree, betweenness, and **community membership**. Hormiga **already
computes these** in the physics / connections view (Fruchterman–Reingold layout
+ community detection); Allomone reads them as higher-order condition inputs
("highlight the most central organizations"). They are **global and costly**, so
they are **opt-in** condition sources, recomputed on a budget like the physics
view — never on every keystroke.

# 5. Deltas — change as a trigger (reactive, always on)

Because **every mutation is a dispatcher command**, Allomone can observe the
**delta** of a change and compare **previous vs new** state: **`on tag change`**,
**`on add <tag>`**, **`on remove <tag>`**. This is **incremental view
maintenance / differential dataflow** (react to the *change*, don't recompute
everything) — and it is **not** Scratch's broadcast events: the trigger is *the
shared environment changing*, still stigmergic
([reactivity](/concepts/allomone/reactivity.md)). It enables rules like "when
`dog` is added, highlight the card" (an appearance change — Allomone is
**derive-only**, [domains](/concepts/allomone/domains.md), so it reacts by
*styling*, not by writing).

**Deltas live on the graph, not on a side queue** (author's idea): each rune (and
mantle) keeps its own append-only **delta log** of tag/field changes, so the
network structure itself remembers its history. `previous tags` is then a local
read of the rune's last delta. This keeps the reactive substrate inside the graph
and consistent with stigmergy; the mechanism is detailed in
[reactivity](/concepts/allomone/reactivity.md). **Delta reactivity is always on
and cheap** — written on change, read locally — independent of the optional
animation clock.

# 6. External sources — inputs come from ANYWHERE (the open boundary)

The author's framing (2026-08-04): **new data and new inputs can come from
anywhere** — spreadsheets (CSV import), a cloud source, and sources not yet
imagined. The input surface is therefore **open-ended**, and the seam that keeps
it manageable is the **Antfarm**: every external source is an **Antfarm *source*
holiday** that lands data into the mantles as ordinary runes+tags. Allomone never
talks to a spreadsheet or a cloud API directly — it reads the **graph** that
those source holidays fill.

This is exactly the **interaction-net I/O-boundary** result
([foundations](/concepts/allomone/foundations.md)): pure interaction nets are
closed, so real systems put **I/O agents at the boundary** (Jiresch 2012;
Fernández & Mackie 2001). Read a **source holiday as an input agent** feeding the
net. The consequence for Allomone: **the interior stays pure** (tags, fields,
links — confluent, re-derivable), and *all* the "from anywhere" messiness lives
at the Antfarm boundary. New source types (a new spreadsheet shape, a new cloud
backend, login/analytics for a future dynamic site) are **new boundary agents**;
Allomone reads their output as more facts, with **no language change**. The
symmetric output side is [domains](/concepts/allomone/domains.md).

# The input hierarchy, at a glance

```
local  ── tags ──▶ fields ──▶ links (1 hop) ──▶ links (n hops) ──▶ spectral/community  ── global
                                                                                (opt-in, budgeted)
reactive ── deltas (tag add/remove/change; previous vs new)  ── always on
future  ── external sources (login, analytics) via Antfarm  ── not designed yet
```

Every input is **read-only** to a condition; a rule changes the world only
through an **action** in some [effect domain](/concepts/allomone/domains.md).
