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
- [The Antfarm](/concepts/platform/antfarm.md) — holidays as the visible
  nervous system: one rune per backend, typed ports, live status faces. The
  Antfarm topology *is* the protocol layer the bundle stores, which is why a
  database can be reconstructed from it.
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
- [Identity](/concepts/platform/identity.md) — **opened 2026-08-27.** Who is
  making a change: the admin profile, the signed-in identity, and the contact.
  Three different things that a conventional design would collapse into one
  users table, and why this one does not.
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
