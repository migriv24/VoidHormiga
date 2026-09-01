---
type: Concept
title: Allomone — effect domains (outputs)
description: "What Allomone writes: domain-neutral annotations interpreted per (glyph × domain) by renderer packs; derive-by-default vs materialize-on-demand; shared (model-tier) vs local (config-tier) scope; and late-bound output holidays via the Antfarm."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). Scratch organizes its palette by
**domain** (Motion / Looks / Sound) — each a set of primitive effects on a
target. Allomone's domains are its **effect domains**: the codomain of a rule's
action, and — true to the name — the different **"species" of surface** that read
the same signal and respond in their own way.

# The core move: one annotation, many interpretations

A rule does **not** emit "make this card's border `#3f6fae`". It emits a
**domain-neutral annotation** — e.g. `emphasis: high` or `accent: warning` — and
the **interpretation is per (glyph × domain)**, done by a **renderer pack** (the
seam already in [blocks & domains](/concepts/sections/blocks-and-domains.md)):

| annotation | Data card | Map | Calendar | Website |
|---|---|---|---|---|
| `emphasis: high` | thicker accent border | larger/bolder marker | bold chip | a CSS `.hot` class |
| `accent: warning` | red tint | red marker | red event | red band |

One rule, every surface — the allomone read by many species. New surfaces
(future output holidays) interpret existing annotations by adding a pack, without
touching the rules.

# The one fixed output vs. the open-ended rest (author, 2026-08-04)

Outputs mirror the inputs ([inputs](/concepts/allomone/inputs.md)): they can be
**anything**, and they grow. But there is exactly **one output that does not
change** — **the Hormiga application itself** (how the data looks *in the app*:
card colors, highlights, marker styles, calendar tints). That is the **stable
render target**, the first and default domain, and the reason styling-in-the-app
is where Allomone starts. *(Caveat: a future mobile version would add a second
in-app render target; noted, not now.)*

**Every other output is open-ended and lives at the Antfarm boundary** — the
website, the newsletter, and **outputs not yet imagined**. Just as source
holidays are the interaction-net's **input agents**, output holidays are its
**output agents** ([foundations](/concepts/allomone/foundations.md); Jiresch
2012; Fernández & Mackie 2001): a rule targets an **abstract effect domain**, and
a new output type is a **new boundary agent** that interprets the annotations it
understands — **no language change**. Allomone styles **one fixed surface (the
app)** directly and reaches **any number of growing surfaces** through the
Antfarm, late-bound (§ "the Antfarm intersection").

> **BUILT 2026-08-11, as a property prefix.** This page's core move — one
> domain-neutral annotation, interpreted per surface — is live. Void Maiz's
> engine carries properties as opaque strings and has no idea surfaces exist, so
> a domain is simply a prefix: `map-color`, `cal-priority`, `web-hide`, with the
> unprefixed spelling reaching every surface. It cost upstream nothing and it
> arrived with domains 1–3 all doing something:
>
> - **the local application** — cards, map markers and calendar chips all read
>   derived annotations, which is [roadmap](/concepts/allomone/roadmap.md) phase
>   F's first step;
> - **the output domain** — `web-hide "1"` drops a rune from every query-backed
>   block in the newsletter, the website and the Builder preview;
> - **the privacy seam** — bound where this page said it would be, and enforced
>   at the READ rather than the export: no predicate can observe the content of
>   an internal-notes field, only `internal ""`, the boolean.
>
> Details and the reasoning: [adoption](/concepts/allomone/adoption.md).
> The rest of this page is the design as written in 2026-08-03.

# The domains

1. **Organization / display filtering — out of scope.** Hormiga already has
   **filters** as a separate first-class thing (the tag-filter builder). Allomone
   does *not* re-implement "which rows show" for the on-screen views.
2. **Appearance in the local application — THE FIRST DOMAIN ("Looks").** Card
   size/color/highlight/animation; **map** marker color/icon and drawn-geometry
   color/shape; **calendar** day/event color and icon; fonts/theme accents. This
   is where development **starts** (author, 2026-08-03): simple, local, visible
   changes — "this contact card is blue", "this marker is red" — are the easiest
   to build, test, and reason about. The full catalog of what's editable per
   surface is [effects](/concepts/allomone/effects.md). Every surface already has
   a renderer that can read an annotation.
3. **Output / content management — the Antfarm domain (later).** When Hormiga
   **sends** content to a holiday (newsletter, website, future outputs), rules
   decide **which runes/fields flow** and **how they're shaped** at the export
   seam. This *is* filtering — but scoped to the **output** codomain, distinct
   from the on-screen display filters of (1). Same tag grammar, different domain.
   The **privacy seam** binds here: internal-notes-class fields must never reach
   an Output holiday ([security](/concepts/platform/security.md) §6); output-content rules
   run **through** that seam, never around it. This is a *later* domain — the
   local application comes first.

# Derive-only, for now (author, 2026-08-03)

Allomone is **derive-only in its current and near-term scope.** A rule is a
**standing query** computed at render/projection time; it **never changes the
database** — no creating tags, no writing fields, no adding links. Turn a rule
off and its effect vanishes cleanly, with nothing to undo. (Datalog's IDB; the
map rules already color markers this way without writing tags — Scry's "derive,
don't materialize".)

**Why no materialization now.** Writing to the model from a rule engine is
genuinely dangerous while the engine is young: mixed with **enable/disable**
scripts and (optional) **loops/clock**, a rule that *creates data* is "a disaster
waiting to happen" (author) — disable a script and does its created data vanish
or persist? Re-enable it in a loop and does it duplicate? These are real
soundness questions. So materialization waits until Allomone is **mature**:
tested, with **error prevention, sandboxing, and dry-run/preview** mechanisms —
and even then, entered cautiously. Until that bar is met, Allomone can make the
app *look* different but **cannot alter the underlying truth**. This keeps the
database honest and the blast radius zero: the worst a bad rule can do is make
something the wrong color, never corrupt data. Materialization is tracked as a
**far-future, maturity-gated** capability, not an early phase
([roadmap](/concepts/allomone/roadmap.md)).

# Scope: shared vs local (an existing tier, reused)

This maps onto Hormiga's **model-tier vs config-tier** split, so we invent
nothing:

- **Shared rules** live in the **model** (the Allomone/rules mantle): dispatched,
  logged, replayable, ride the `.miga`, **E2EE-synced** — so everyone on a shared
  database sees the same styling and structure.
- **Local views** live in the **config tier** (`ui.*`, like the camera and panel
  fractions): **per-machine**, undo-exempt, never dispatched to the shared model
  — "how *I* see the data," which does not change it for collaborators.

# The Antfarm intersection — late-bound outputs

Outputs are **dynamic**: Antfarm holidays connect and disconnect, and new output
types will appear. So a rule **never binds to a concrete output**. It targets an
**abstract effect domain** ("published content"); the **binding to a concrete
holiday is late** — resolved at export time through the **Antfarm graph**. In
dataflow terms a holiday is a **sink** that may or may not be connected; a rule
is a **total function over the model**, but its **realization** in a given output
is **partial**, gated by Antfarm topology. If the newsletter holiday is
disconnected, its content rules simply **don't fire** — no consumer, no effect,
no error. This is **publish–subscribe / content-based routing**: the model
publishes annotated content; holidays subscribe and pull what their content rules
admit at the seam. It is why "the newsletter is just an output we modify with
rules" is too naive, and why the honest scope is **content management** — rules
over *what flows* to whichever sink is connected. The **Builder** is the first
such consumer; its renderer packs already interpret annotations, so an Allomone
rule and a builder document meet at the same (glyph × domain) seam.
