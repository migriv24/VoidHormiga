---
type: Concept
title: Application boundaries — what separates, and what a separation costs a user
description: "The three kinds of dependency a Void Core project can have (runtime, capability package, sibling application), why the Allomone precedent does not support carving out an app, and the four-layer decomposition of the map that says which part of it is actually separable."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-08-21T00:00:00Z
---

Opened 2026-08-21 by the author's question — *should the map become its own Void
Maiz application that Void Hormiga integrates?* — which turned out to be a
specific case of a question the family has not written down: **what does it mean
to separate something, and who pays?**

This page is the general answer. The map's own decision is
[Q42](/developer_questions.md); the map's design is
[territory](/concepts/sections/territory.md).

# 1. Three kinds of dependency, distinguished by what a USER experiences

The author's framing, which is right and is the load-bearing idea here:

> If a user downloads Void Hormiga, they are ALSO downloading Void Maiz and Void
> Core. […] Void Palabra uses the Void Core, but it doesn't have a use on its
> own. […] Void GIS would be usable on its own theoretically.

Sharpened into three kinds, because the difference is not technical — every one
of these is "a C++ library in a sibling folder" — it is **what a person
experiences when the boundary exists**:

| kind | example | can a user *use* it? | what separation costs a user |
|---|---|---|---|
| **Runtime** | Void Core, Void Maiz | no — it has no independent existence for them | **nothing.** They never learn the name |
| **Capability package** | Void Palabra | no — you develop *with* it, like git | **an optional install**, and a feature that appears when present |
| **Sibling application** | Void GIS *would be* | yes — it has its own users and its own reason to exist | **a second application**: a second window, a second mental model, a second thing to install and update, and a version skew the day the two disagree |

The third row is the one that is easy to under-price, and it is the whole cost
side of the map question. The first two rows are nearly free to a user; the
third is not free at all, and it is paid by the person least equipped to pay it
— the volunteer at an outreach organization who wanted to put four pins on a map.

## Why the Allomone precedent does not carry

The author cites Allomone moving to Void Maiz as evidence that separations go
well, and it is evidence — for **row 1**, not row 3.

**Allomone was a headless engine**: a language, a parser, an evaluator. It had
no UI opinion, no user-facing identity, and nothing about it was *ours*. Moving
it down into the runtime meant every Maiz host got it and no user ever noticed a
boundary appeared. That is the cheapest kind of separation there is, and it is
why it was the right call.

**A map is a view.** Carving it out is a different move in a different direction
— sideways into row 3, not down into row 1 — and the precedent does not transfer.
Naming that is not an argument against doing it. It is an argument against doing
it *because Allomone went well*.

# 2. What Void Palabra does and does not buy us here

> now that palabra is a bit more mature, its easier than ever to integrate these
> different applications into each other

True, and worth being precise about, because the two halves land differently:

- **Palabra makes STATE integration easy.** One content-addressed store, one
  canonical name for a cut, one merge law. Two applications over one
  organization's data is now a solved problem rather than a research project.
  That is real and it is new.
- **Palabra does not make VIEW integration easy, and cannot.** If Hormiga wants
  a map *inside its own window*, no amount of Palabra helps: that is a Maiz view
  in Hormiga's process, and it stays Hormiga's to build.

So Palabra's maturity is an argument for **separating the data and the engine**,
and no argument at all for separating the window. Those are different products
and it is worth not letting one justify the other.

# 3. The Void Maiz ruling that decides most of this

Void Maiz ruled on 2026-07-18 that **custom views are HOST-BUILT**: the blessed
seam is `project_scene` in, compiled commands out, and the host draws. Maiz
provides the substrate, the widgets and the interaction grammar; it does not
provide the views. Hormiga's map canvas and calendar both proved the pattern.

That ruling has a consequence people keep missing, including this document's
first draft: **"move the map canvas into Void Maiz" contradicts Maiz's
architecture.** It is not a matter of Maiz being unwilling; views are
deliberately not theirs. What Maiz *should* own — and what
[territory](/concepts/sections/territory.md) §The canvas already flags as a gap — is the
**gesture and interaction vocabulary** a custom view hand-rolls today. That is
generic, benefits every host, and is a row-1 separation, i.e. free.

# 4. So the map is four layers, not two

The question "separate the map or not" has no good answer because the map is not
one thing. It is four, and they separate differently:

| layer | what it is | where it belongs | separation cost |
|---|---|---|---|
| **1. Interaction vocabulary** | pan/zoom/drag/select gestures a custom view currently hand-rolls | **Void Maiz** — already an identified gap | free (row 1) |
| **2. GIS engine** | projections, tiles, geometry, spatial predicates, formats (GeoJSON/GPX/shapefile), non-Earth sources, the map *builder*'s model | **separable, headless** — the Allomone-shaped piece | cheap (row 1 or 2) |
| **3. The map VIEW** | markers, drawing tools, layer UI, the inspector, what a clearance tag means on a pin | **the host** — Maiz's ruling. Hormiga builds Hormiga's; Void GIS would build its own | not separable |
| **4. Void GIS, the application** | a standalone Maiz host over layer 2 | **its own repo, later** | expensive (row 3) |

**Layer 3 is where every complaint the author has about the map actually
lives.** "It is easier to drop pins in Google Maps and screenshot it" is a
statement about markers, gestures, and affordances — layer 3, and layer 3 stays
in Hormiga under every possible separation. That is the finding that reorders
the work: **separating the map cannot fix the reason the author wants to
separate the map.**

**Layer 2 is the part that is genuinely separable, and it is separable cheaply**
— because Hormiga's map is *already generic*. Measured 2026-08-21: `ui/map.cpp`
is 1,905 lines that draw **anything carrying a `geo` field**. It has its own
glyphs (`map`, `mapshape`, `refpoint`), it special-cases no Hormiga domain
glyph, and its coupling is to host plumbing (`pending_cmds`, `scene`, `toast`,
`dispatch_and_reproject`) rather than to contacts or events. `geo_<channel>`
already gives one entity several positions in different views, which is a
GIS-shaped idea nobody has to invent.

That is the good news, and it is the reason there is **no urgency premium**: the
seam is already clean, so it will be as cheap to cut in six months as today.

# 5. The stance

1. **Do the UX work first, in place.** It is layer 3, it is the actual
   complaint, and no separation touches it.

   > **The author chose otherwise, 2026-08-21: structure first.** Recorded
   > rather than quietly rewritten, because the reasoning above still stands and
   > a stance that changes silently is not a stance. The choice is coherent
   > given the other answer given the same day — that the **non-Earth map
   > builder is a real intention** — because that turns layer 2 from speculative
   > into load-bearing: an engine with a known second client is worth building
   > before the UX that will sit on it, rather than after. What does NOT change
   > is §4's finding: the structural work does not make the map nicer, and the
   > UX sitting is still owed.
2. **Build it against the seam, not against `HormigaApp`.** New map work —
   drawing tools, marker system, layer filtering — should take a scene and emit
   commands, and should reach for a *named* interaction vocabulary rather than
   another `map_*` member on the app struct. There are 18 of those today. Every
   one added against the struct is a line item in a future extraction; every one
   added against the seam is not. **This is what makes deciding the destination
   now worth anything, even if the move is years away.**
3. **Let layer 2 accumulate as a folder before it is a repo.** **Done
   2026-08-21** — `src/gis/` exists, depends on nothing but the standard
   library, and is enforced by `check_layering.py`. Projection math,
   tiles, geometry and format IO can live under `src/gis/` with no dependency on
   `HormigaApp`, checked by `tools/check_layering.py`. If it earns a second
   client it lifts out in an afternoon; if it never does, nothing was wasted and
   the code is better organized regardless. **A folder is a hypothesis; a repo
   is a commitment.**
4. **Void GIS is a product decision, not an architecture decision**, and it
   should be made when there is a second user — the map builder for non-Earth
   maps is the most plausible one — not to make Hormiga's map better, which it
   will not.

# 6. The Antfarm is a different question and should not be bundled

The author raised separating the Antfarm in the same breath. It is not the same
shape and pairing them would get both wrong:

- The map is a **view over the org's own data**. The Antfarm is a **configuration
  surface for a Void Core concept** (holidays) — it is closer to being *runtime*
  than to being an application, which is the opposite end of §1's table.
- Its UX problem is real and is also layer 3, so separation does not fix that
  either.
- Its natural gravity is toward **Palabra**, since peers, transport and
  capability are Palabra's, and a topology editor over Palabra's concepts is a
  coherent thing. That is a conversation to have with Palabra at their Phase 4,
  not a Hormiga refactor.

# 7. Latin-OS raises the stakes and should not drive the decision

Latin-OS is `status:planned`, concept phase, no code, and gated on Void Core's
maturity. It matters here for one reason: in an OS, "application" is a much
heavier word — the compositor's scene graph is a mantle and applications
interact through port mappings, so a boundary drawn today becomes a *process*
boundary later. That is an argument for **keeping layer 2 separable**, which is
§5.3, and it is the same conclusion reached without it. It is not an argument for
building Void GIS now.

The author's larger point — that these are not merely apps that can open each
other's file types, but apps that share whole functionality — is what §1's table
is for. "An SDK with a GUI" is a good frame with one correction worth keeping:
**the GUI is not a thin shell over the SDK.** It is where domain judgement
lives. Hormiga's map knows that a contact carries a clearance tag and must not
be published; a generic GIS cannot know that and should not. That asymmetry is
exactly why layer 3 is not separable, and it is a feature of the architecture
rather than a limit on it.
