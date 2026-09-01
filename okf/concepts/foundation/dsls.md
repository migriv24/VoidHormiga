---
type: Concept
title: Hormiga's three DSLs — layout, interface, logic
description: "Hormiga already has three domain-specific languages, and they must not be conflated: the BUILDER is a LAYOUT DSL (spatial, no logic), the ANTFARM is an INTERFACE/PROTOCOL DSL (port-mapping externals to Hormiga's fixed typed interfaces — the I/O boundary), and ALLOMONE is the LOGIC/scripting language (computation) that consumes the other two. Each has the right visual surface for its domain: grid, node-graph, text."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-08-04T00:00:00Z
---

The author's clarification (2026-08-04): Hormiga already contains several
"DSL"-shaped constructs, and treating them as one thing causes confusion. They
are **three distinct domain-specific languages**, each answering a different
question, each with the visual surface its domain deserves. Naming the split is
the point.

# The three, side by side

| | **Builder** | **Antfarm** | **Allomone** |
|---|---|---|---|
| answers | *what does it look like?* | *how do we talk to the outside?* | *what should happen?* |
| kind | **layout** DSL (spatial, declarative) | **interface / protocol** DSL (structural) | **logic** DSL (computation) |
| has logic? | **no** — placement + content only | **no** — it defines the interface, doesn't compute | **yes** — variables, conditionals, iteration |
| natural surface | a **layout grid** (WYSIWYG) | a **node graph** (dataflow/wiring) | **text** (a scripting language) |
| output | documents (newsletter / website) | typed I/O interfaces (Data/Asset/Output/Import/…) | annotations now; later mutations + driving I/O |
| concept | [builder](/concepts/sections/builder.md) | [the Antfarm](/concepts/platform/antfarm.md) | [Allomone](/concepts/allomone/index.md) |

# What each one is (and is NOT)

1. **The Builder is a LAYOUT DSL — no logic.** It is an advanced document/website
   layout editor: components on a grid, bands, pages, content, and (for
   websites) interactive widgets. But it is **placement and content editing**,
   not computation — there are no conditionals, no variables. It authors *what
   things look like*. (Its widgets can be interactive on a published site, but
   that interactivity is the *output medium's*, not a logic the Builder runs.)

2. **The Antfarm is an INTERFACE / PROTOCOL DSL — the I/O boundary.** Its job is
   **port-mapping**: giving Hormiga a **consistent way to talk to arbitrary
   external things** (cloud platforms, local servers, spreadsheets) by mapping
   each to one of Hormiga's **fixed typed interfaces** (Data / Asset / Output /
   Import / LLM / Geo / Auth / Translate). It **defines the language of I/O**;
   it does not run logic. It is already conceived and partly built as a **node
   graph** — holidays as nodes, typed ports, wiring-is-configuration
   ([the Antfarm](/concepts/platform/antfarm.md)) — which is exactly right, because I/O is
   **dataflow**, and dataflow's natural surface is a node graph. This is the
   interaction-net **I/O boundary** the literature says to keep separate from the
   pure core ([allomone/foundations](/concepts/allomone/foundations.md)):
   source holidays = input agents, output holidays = output agents.

3. **Allomone is the LOGIC / scripting language — it USES the other two.**
   *(Adopted from Void Maiz 2026-08-10 — [adoption](/concepts/allomone/adoption.md).
   The three-DSL distinction below survives the move intact, but "the only one
   with computation" is now precise in a way it was not: Allomone **derives** and
   does not execute. There is no run button, no order, and no write path.)* It
   reads the **data + tags** (input), and will
   later **use the Builder's layouts** (modify document content, export layouts)
   and **use the Antfarm's interfaces** (extract data from any source, export to
   any output) — precisely *because* the Antfarm made those a consistent
   interface. Allomone sits **on top**: `Builder` gives it *surfaces*, `Antfarm`
   gives it *reach*, and Allomone supplies the *logic*.

# The right visual surface per domain (and the blocks lesson)

A theme worth stating: **each DSL wants a different surface, and that's why one
visual paradigm can't serve all three.**

- **Layout → a grid** (the Builder). Spatial content wants spatial editing.
- **Dataflow → a node graph** (the Antfarm). Wiring providers to interfaces is
  literally a graph; a node editor is the honest surface.
- **Logic → text** (Allomone). Control flow (`if/else`, variables, sequence,
  counting) is where **node graphs and blocks both fail** — visual control flow
  turns to spaghetti (the reason Scratch uses stacked *blocks*, not a free node
  graph, and the reason our Allomone blocks felt wrong). **Text is the right
  surface for logic.**

So the author's musing — "maybe a node graph would've been better than blocks for
Allomone" — resolves cleanly: a node graph is better for **the Antfarm** (it's
dataflow), and it's already there; for **Allomone** (logic), **text won**. Blocks
were the wrong surface for logic; the node graph is the right surface for I/O.
(The Antfarm *could* also gain a textual DSL that converts to/from its node graph
— plausible, like Allomone's blocks⇄text — but the **node graph is its primary,
correct surface**, so a text form is a convenience, not a rewrite.)

# How they compose — and the card-color question answered

The clarifying question — *"what does it mean to change the color of a contact
card? does it require something in the Antfarm, or is it a default thing?"* — has
a clean answer that defines the whole boundary:

- **Styling the app is a DEFAULT (interior) operation — no Antfarm.** The
  Hormiga **application itself** is the **one output that does not change**
  ([allomone/domains](/concepts/allomone/domains.md)); it is the interior render
  target, *part of Hormiga*, not an external system. Allomone emits a `color`
  annotation; the app's card/marker/calendar renderer reads it. **Nothing crosses
  a boundary; no holiday is involved.** Changing a card's color is as built-in as
  the app drawing the card at all.
- **Reaching anything external DOES require the Antfarm.** The moment the same
  styling (or data) must leave for a **website, a newsletter, a cloud store, a
  spreadsheet** — that is a **boundary crossing**, routed through an Antfarm
  holiday (output or import). Late-bound, per the interaction-net boundary model.

So: **Allomone → the app is direct (interior, default); Allomone → the world is
through the Antfarm (boundary).** That single line is the architecture.

# What this means for sequencing (should we build the Antfarm alongside Allomone?)

- **Allomone's CURRENT phase — styling the app — needs NOTHING from the
  Antfarm.** It is interior. Build the editor and the interpreter independently;
  do not gate them on Antfarm work.
- **Allomone's LATER phases — extract/export data, style external outputs,
  operate on Builder documents — DO depend** on (a) the Antfarm maturing from
  its current *dashboard-of-cards + a few working holidays* into the **consistent
  interface Allomone can call programmatically** (the "Allomone invokes a holiday"
  seam does not exist yet), and (b) the Builder exposing its documents to scripts.
  So **co-develop the Antfarm with Allomone's I/O phase, not before it.**
- **Honest Antfarm status:** the *vision* (typed-port protocol node graph) is
  right and matches this framing; the *implementation* is early — real holidays
  exist (SQLite Data, FS Assets, ImgBB, the Supabase import), but most nodes are
  status cards and the uniform "any external ↔ a fixed typed interface" mapping,
  and the program-callable holiday seam, are **not built yet**. That is fine:
  nothing in Allomone's current work needs them, and they get built when Allomone
  reaches the boundary. See [the Antfarm](/concepts/platform/antfarm.md) for the plan.
