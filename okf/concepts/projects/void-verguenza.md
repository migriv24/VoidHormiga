---
type: Concept
title: Void Verguenza — Hormiga's view of the seam
description: "A sibling application founded 2026-10-01 at ../VoidVerguenza, with its own OKF (that bundle is the source of truth): secrets several people share, sealed per device, synced over Reticulum, with actions that can need several devices to approve. The author: 'eventually the antfarm will reference Verguenza's structures to use instead of our current api key node.' What Hormiga's Key node becomes, what stays Hormiga's, and how this answers Q88."
tags: [status:planned, audience:dev, confidence:asserted]
timestamp: 2026-10-01T00:00:00Z
---

**This page is Hormiga's view of the seam.** Void Verguenza's own bundle
(`../VoidVerguenza/okf/index.md`) is the source of truth for what Verguenza is.

# What it is, in one paragraph

A Void Maiz desktop application for secrets that several people share. A
secret is a description every member can read and a value sealed separately to
each device allowed to open it. Devices enrol in person with a code compared on
two screens (Hormiga's LAN pairing). Actions can need *k of n* devices to
approve, enforced by honest software (*guarded*) or by mathematics (*split*),
and labelled as which. Every use is a signed, logged command. It syncs over Void
Palabra and Reticulum, like Hormiga.

# Why it matters to Hormiga

The author, founding it: *"I want this to use similar structures as the API Key
node there is in the antfarm, where eventually the antfarm will reference
Verguenza's structures to use instead of our current api key node."*

Verguenza is designed so that Hormiga is its first **client**
(`../VoidVerguenza/okf/concepts/clients.md`): an application enrolled in a vault
with a **grant** (which secrets, for which uses), which asks for a value at the
moment an effect needs it and wipes it after.

| Hormiga today (Antfarm v2 Key node) | Hormiga with Verguenza |
|---|---|
| the Key rune holds provider, expiry, who added it | it holds `verguenza: <vault>/<secret>`; the rest is read from Verguenza |
| the value sealed in Hormiga's own vault (`src/platform/vault.cpp`) | the value in Verguenza's envelope; Hormiga holds nothing |
| `farm key set`, the Inspector's password box | *"add or change it in Verguenza"* |
| readiness: *"no value on this device"* | *"Verguenza is locked"*, *"no grant"*, *"needs 2 approvals"* |
| key monitors (usage, expiry) designed in `keys.md` §4 | Verguenza's audit and expiry, read through its `describe` operation |
| sharing key values between members (Antfarm v2's phase V3, not built) | Verguenza's sharing: end-to-end, signed, per device |

**The consequence sentence travels with the ask.** When Hormiga asks to use a
key for `deploy-site`, Verguenza shows whoever must approve the effect's own
sentence (*"PUBLISHES THE WEBSITE…"*). The approval and the action are about the
same words.

# What this changes in Hormiga's plans

- **Antfarm v2's phase V3 (keys shared between members)** should wait for
  Verguenza's client seam rather than grow Hormiga's own vault into a sharing
  system. The Key node keeps working with Hormiga's vault meanwhile; both kinds of
  reference can coexist.
- **Q88** (where does at-rest encryption live?) named what would change its lean:
  *"a second application wanting the same vault format."* Verguenza is that
  application. Its envelope library (`voidverguenza`, no UI) is the natural home for
  the sealed-secret format, and Hormiga's vault can migrate onto it.
- **The migration** of Hormiga's Key values into Verguenza is a Hormiga command,
  designed when Verguenza's phase 2 (clients) exists.

# What stays Hormiga's

The Antfarm, its graph and its wiring; which node uses which key; the effect
gate and its consequence sentences; the decision to ask. Verguenza decides
whether the ask is allowed, and keeps the value.
