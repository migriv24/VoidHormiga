---
type: Concept
title: Void Chisme — Hormiga's view of the seam
description: "A sibling application founded 2026-10-01 at ../VoidChisme, with its own OKF (the source of truth): a desktop tool for seeing and managing a network the user owns, devices as runes in a Void Maiz graph, with bandwidth, alerts and logged admin actions, and Reticulum read natively. What it would measure about a Hormiga database's members, the observer role it asks Hormiga for, and a privacy finding about Void Maiz's Reticulum announces that matters before Hormiga moves its sharing there."
tags: [status:planned, audience:dev, confidence:asserted]
timestamp: 2026-10-01T00:00:00Z
---

**This page is Hormiga's view of the seam.** Void Chisme's own bundle
(`../VoidChisme/okf/index.md`) is the source of truth.

# What it is, in one paragraph

A Void Maiz desktop application for a network its user administers (the author
is about to run a store's): devices found by quiet senses and by asking the
router, switch and access points, kept as runes with evidence for every fact,
drawn as a graph linked by physical, logical, traffic and *association* links
(same kind, same subtype), with bandwidth, alerts, and admin actions as logged,
rehearsable effects. It reads Reticulum itself, so it recognises Void
applications.

# What it would measure about Hormiga

The author: *"we might have very many devices communicating on an hormiga
database, and such, we should be able to measure the activity of people on the
network (assuming like a single admin)."*

| level | Chisme sees | needs from Hormiga |
|---|---|---|
| from outside | which devices run Hormiga, Reticulum paths, link volumes if it relays | nothing |
| **as an observer member** (Chisme's lean) | who is present and where, sync exchanges and bytes per member, conflicts, devices that fell behind | a new **observer** role: a member that receives presence and sync statistics and never the database |
| from Hormiga's own report | per-peer sync counters | Void Maiz's `voidmaiz_net` to expose counters |

The observer role is [Q99](/developer_questions.md). It fits Hormiga's existing
membership and approval flow: a person allows the observer, as any join.

# A privacy finding to settle before the Reticulum move

Found while designing Chisme, read in the code on 2026-10-01: Void Maiz's
Reticulum session (`VoidMaiz/src/net/rnslink.cpp`, `RnsSession::about_json`)
puts the person's **display name**, **colour** and the **shared database's
name** in every announce, as plain JSON. Reticulum announces are public. Hormiga
today shares over Void Maiz's LAN path, with presence sealed to the room key so
outsiders cannot read who is present; moving to the Reticulum session as it
stands would undo that. [Q98](/developer_questions.md).

# Where else they meet

- **Equipment credentials** in Chisme come from Void Verguenza, like Hormiga's
  keys will (see [Void Verguenza](/concepts/projects/void-verguenza.md)).
- **Strands and many-inputs**, built in Void Maiz for Antfarm v2 on 2026-09-28,
  are what Chisme draws traffic volume and switch ports with.
- **The Antfarm's Network chamber** (profiles per device) and Chisme's device
  runes describe overlapping things from different sides: Hormiga knows who a
  device belongs to in a database; Chisme knows where it is on the network.
  Nothing joins them yet, deliberately.
