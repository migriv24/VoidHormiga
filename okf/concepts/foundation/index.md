---
type: Index
title: Foundation — what Void Hormiga is made of
description: "The concepts that hold before any section exists: what the application's parts are, which of them are separable, and what the data actually is."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-08-27T00:00:00Z
---

**The layer everything else assumes.** These three pages answer questions that
are prior to any feature: what kind of thing is this application, what are its
seams, and what is the shape of the data underneath.

If you are reading the OKF for the first time, read this folder before
[sections](/concepts/sections/index.md).

- [Application boundaries](/concepts/foundation/application-boundaries.md) —
  the three kinds of dependency (runtime / capability package / sibling
  application), what a separation costs a *user* rather than a developer, and
  the four-layer decomposition of the map that says which part of it is
  actually separable. The page to read before proposing that anything be split
  out.
- [Hormiga's three DSLs](/concepts/foundation/dsls.md) — **Builder** = layout
  DSL (a grid, no logic), **Antfarm** = interface/protocol DSL (a node graph,
  the I/O boundary), **Allomone** = logic/scripting (text) that consumes the
  other two. Answers "does changing a card's colour need the Antfarm?" — no,
  the app is the interior default output.
- [Data model](/concepts/foundation/data-model.md) — the five kinds of thing an
  outreach org runs on, as glyphs; tag axes and temper hygiene; relations as
  edges.

# What does not live here

The **`.miga` bundle**, the **Antfarm**, **security** and the **data planes**
are in [platform](/concepts/platform/index.md) — they are the machine
underneath rather than the definition of the thing. The open question of what
Hormiga *is* at all (the builder? the antfarm? the database?) is
[Q45](/developer_questions.md), and it is filed as a philosophical question
because the answer changes what this folder should contain.
