---
type: Concept
title: Void Snape (archived) — networking is Reticulum, through Void Palabra
description: "Void Snape was founded and archived on 2026-09-23. The author's final ruling: all device-to-device networking runs over Reticulum, with Void Palabra as its Void translation. What that means for Hormiga (its member sharing is re-implemented on Palabra; who may join and what stays private remain Hormiga's), and the one piece Reticulum does not cover: encrypting files at rest (Q88)."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-23T00:00:00Z
---

**Superseded the same day it was written.** Void Snape was founded as the
family's cryptography library, then archived when the author saw that
Reticulum already is one: *"let's not use void snape at all! ... let's just use
reticulum for everything! all our netoworking needs, with palabra as its void
based translation! ... hormiga shouldn't need to actually ddeal with too much
of ths, just re-implementing things."* The plan is Void Palabra's:
`../VoidPalabra/okf/concepts/reticulum.md`.

# What this means for Hormiga

| today, in Hormiga | becomes |
|---|---|
| the sealed UDP beacon, the join code, the short authentication string, `secretstream` (`sync/peer.cpp`, `app/lan_wire.cpp`) | **Reticulum**, through Palabra's `voidpalabra_reticulum`: announces for discovery, links for the sealed stream, Reticulum identities for who is who |
| the profile's device key (`platform/profile.cpp`) | a Reticulum identity (Ed25519 + X25519) |
| `app/lan_net.cpp` driving `voidmaiz_net` over our own sealed session | the same `voidmaiz_net`, over Reticulum links |
| **who may join, the members registry, private tags, what a join carries** | **unchanged, Hormiga's** |
| publishing, object stores, image hosts, the update feed | unchanged: HTTPS, not Reticulum |
| the credential vault and encrypted backups (Argon2id + XChaCha20-Poly1305) | **not Reticulum's to hold**: [Q88](/developer_questions.md) |

So Hormiga *re-implements* its member sharing on Palabra's Reticulum layer, and
owns no network cryptography of its own afterwards. That keeps the author's
point that network cryptography is not what Hormiga is about.

**For the phone**, this dissolves most of Q67: a phone that joins over
Reticulum and never holds credentials (it is a member, not the host) needs no
libsodium at all. Only the desktop's vault and backups still do.

The founding research (why no single crypto library covers Reticulum, the
license notes, Meshtastic) is kept in `../VoidSnape/okf/` as history.
