---
type: Index
title: Platform — the machine underneath
description: "Where the data lives, who may see it, how it leaves the device, and how two devices holding the same database agree: the .miga bundle, the Antfarm, security, the data planes, the web platform, identity and collaboration."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-08-27T00:00:00Z
---

**The parts a user never names and every feature rests on.** Storage, the
backend graph, the privacy seams, the network, and — since 2026-08-27 — who is
making a change and how two devices converge on it.

Read in this order the first time:

- [The `.miga` v3 bundle](/concepts/platform/miga-format.md) — the whole
  database as one portable, switchable, backup-able file: every mantle, the
  irreplaceable assets, the encrypted secrets. Working copy vs bundle
  (git-shaped); re-derivable vs irreplaceable; offline load; backups. **The
  most important file the application owns**, and — per
  [Q45](/developer_questions.md) — arguably the best answer to "what is
  Hormiga?".
- [The Antfarm](/concepts/platform/antfarm/index.md) — **a folder since
  2026-09-22**, because it is the structure the author is redesigning. Holidays
  as the visible nervous system: one rune per backend, ports typed by payload,
  every crossing a gated effect, capabilities instead of vendors. The folder
  holds the graph as built, holidays, capabilities, research on how outside APIs
  map on, the Antfarm across devices, **real CLI transcripts**, and the redesign
  workbook. The Antfarm topology *is* the protocol layer the bundle stores,
  which is why a database can be reconstructed from it.
- [Security](/concepts/platform/security.md) — the E2EE posture: libsodium as
  the one crypto dependency, at-rest encryption, the render-seam privacy rule
  (subtractive for things, **additive for people**), the collaboration modes,
  signed releases.
- [Data planes](/concepts/platform/data-planes.md) — the admin database, the
  publication and the inbox: what is authoritative in each, why they must never
  merge, the two kinds of "cloud save", and why claiming a contact must not
  search the database. **Every security property in the system is a consequence
  of this page.**
- [The web platform](/concepts/platform/web-platform.md) — what is hosted and
  on which vendor; what a visitor is allowed to do; the AWS mapping and the
  order to build it.
- [LAN sharing](/concepts/platform/lan-sharing.md) — **opened 2026-09-16.**
  Profiles, joining a database over the local network, the members registry,
  presence and private data, and the Antfarm nodes that decide them.
- [Identity](/concepts/platform/identity.md) — **opened 2026-08-27.** Who is
  making a change: the admin profile, the signed-in identity, and the contact.
  Three different things that a conventional design would collapse into one
  users table, and why this one does not.
- [Distribution](/concepts/platform/distribution.md) — **opened 2026-09-04.**
  How the application reaches a machine that is not the developer's, and how the
  person on it learns a newer version exists. The seam with Void Mago (a
  build-time tool that never runs on a user's machine), why the hub is a file
  rather than a program, side-by-side installs, and the rule that shaped the
  whole update client: **a check is a network request a person did not make.**
- [The download page](/concepts/platform/download-page.md) — **half record,
  half plan (2026-09-08).** The other end of distribution: where the `.exe` is
  hosted, why the website's button must never carry a version number, which
  Hormiga blocks build it, and the four things that actually stand between a
  stranger and a working install. Written to be read by the author and run by an
  agent — and it has now been run, which corrected three of its numbers. **§5(d)
  is where the OS detection lives**: `platform` on `download` and `link`, a grid
  row of them as a platform set, the visitor's own moved first and labelled, and
  none of them ever hidden. What is still a plan is the release the button needs.
- [Collaboration](/concepts/platform/collaboration.md) — **opened 2026-08-27.**
  One database, several devices: the merge (Void Palabra's, and correct now),
  the transport (LAN first, S3 as a rendezvous), and the honest statement of
  which half is built.

# The rule this folder exists to protect

Stated once, here, because four of these pages restate it in their own terms:

> **The admin plane never leaves the device except encrypted, and the
> publication plane only ever carries what cleared the seam.** There is no
> third door.

A change that adds a way for data to leave is a change to this folder, and it
is reviewed as one — not as a feature of whichever section happened to want it.
