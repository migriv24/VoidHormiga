---
type: Index
title: Projects — the neighbours
description: "Things that are adjacent to Void Hormiga rather than part of it: a sibling repo, a proposed sibling application, and the synthetic colony we test against instead of a real organization."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-08-27T00:00:00Z
---

**Not features.** Each of these is a separate thing that Hormiga borders on,
and each page here is *Hormiga's view of the seam* rather than the thing
itself.

- [Void Reyna](/concepts/projects/void-reyna.md) — **a sibling project at
  `../VoidReyna` with its own OKF; that repo is the source of truth.** The
  dataset generator, and the place holidays get taken seriously as the
  transformation primitive: harvest → archive → extract → emit, with the impure
  crossing quarantined at the boundary. A holiday is an effect boundary plus a
  pure `Lens`, lenses compose, and that makes the pivot rule a theorem rather
  than a preference.
- [The Civic Record](/concepts/projects/civic-record.md) — **proposed
  2026-08-12**: a public website monitoring one city's government —
  `policy`/`revision`/`provision` as a versioned DAG over a tree, because a
  policy is defined by its delta. Also where "should Hormiga become five
  applications?" is answered: four views over one model stay together, the
  dataset generator does not.
- [Void Maiz, September 2026 — what we take up](/concepts/projects/void-maiz-uptake.md)
  — **2026-09-22.** Void Maiz absorbed networking, updates and tag suggestions
  from Hormiga and proved a collaborative canvas and a mobile kit on
  Interaction Combinators. Piece by piece: adopted, proven compatible, waiting,
  or declined, with why. Hormiga builds against it unchanged, and Void Maiz's
  update client already reads Hormiga's live feed.
- [The Cat Dataset](/concepts/projects/cat-dataset.md) — a public, synthetic
  test colony: 50 tag-rich fictional cats and linked birthday events, built by
  a C++ generator through the real dispatcher. **This is what the pipeline is
  designed against**, and that is a feature rather than a limitation: a
  pipeline that only works because it knows what a real contact looks like is
  the failure mode, and cats make it visible immediately.

# Why the cats are in this folder

Because they are a *stand-in for an organization*, and putting them beside the
real neighbours keeps the reason visible. Ground rule 2 makes the repo root a
public artifact and CLAUDE.md forbids test fixtures containing real people; the
Cat Colony is how those rules are obeyed without giving up on testing against a
realistic corpus. It is the only "organization" this repo may ship.
