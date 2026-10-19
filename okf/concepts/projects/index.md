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
- [Void Snape (archived)](/concepts/projects/void-snape.md) — founded and
  archived 2026-09-23. **All device-to-device networking runs over Reticulum,
  with Void Palabra as its Void translation.** Void Hammer now owns the reaching
  and sync engine that implements the family side of that decision; who may join
  and what stays private remain Hormiga's, and at-rest encryption is Q88.
- [Void Maiz, September 2026 — what we take up](/concepts/projects/void-maiz-uptake.md)
  — **2026-09-22, with a 2026-10-18 update.** Void Maiz absorbed networking,
  updates and tag suggestions from Hormiga and proved a collaborative canvas and
  a mobile kit on Interaction Combinators. On 2026-10-17 its networking engine
  moved to Void Hammer, and its navigation bar and HTTP seam were lifted. Piece
  by piece: adopted, proven compatible, waiting, or declined, with why.
- **Void Hammer** (`../VoidHammer`, founded 2026-10-17) — the family's
  **networking** since 2026-10-17: reaching devices (sockets, LAN facts, the
  multicast lock, the radios) and the sync engine over Void Palabra (`Network`,
  `LanSession`, `RnsSession`, `RadioBridge`), plus hardware documentation and the
  field kit. See [the networking stack](/concepts/platform/networking-stack.md)
  and `VoidHammer:/concepts/networking.md`.
- [Void Verguenza](/concepts/projects/void-verguenza.md) — **a sibling
  application founded 2026-10-01 at `../VoidVerguenza`.** Secrets several people
  share, sealed per device, with actions several devices must approve. A
  single-device vault runs; the client seam Hormiga needs is phase 2, so the
  Antfarm Key node still uses Hormiga's vault. This answers Q88's condition (a
  second application wanting the vault format).
- [Void Chisme](/concepts/projects/void-chisme.md) — **a sibling application
  founded 2026-10-01 at `../VoidChisme`.** Seeing and managing a network one
  owns, as a graph. A phase-0 library, CLI and window exist; the observer role
  Hormiga would offer (Q99) and the Reticulum adapter are later phases. It found
  that the Reticulum announces carry names in the clear (Q98).
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
