---
type: Index
title: The Antfarm
description: "A folder since 2026-09-22. The organization's backends as a mantle you can open: one rune per backend, ports typed by the PAYLOAD that flows, wiring as configuration, every crossing a gated effect, and capabilities so the rest of the application asks a question instead of naming a vendor. This index holds the invariants any redesign must keep, the honest status, the dated decision ledger, and the reading order."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

# The Antfarm

**The organization's nervous system, made visible.** Every backend the
organization uses has one rune in the `antfarm` mantle: the local SQLite store,
the assets folder, the website publisher, GitHub Pages, an S3 bucket, the LAN
share, the members list. The ports on those runes are typed by what flows
through them. The edges between them are the configuration. Configuring a
backend and seeing your backends happen on the same canvas.

The name is structural, not decorative (see [the front door](/index.md)): the
colony of small tagged things does the work, and the Antfarm is where you watch
it.

## Why this is a folder (2026-09-22)

The author: *"the antfarm is important enough to deserve another subfolder
within the 'platform' folder. I will be studying this documentation of the
antfarm specifically for its redesign."* The author had held back its design
precisely *"because of the very fact of how important the antfarm is"*.

The single page had grown to 342 lines of layered decisions, each correct for
its date and some superseded by later ones in the same file (the Q25 banner
said "the old bucket framing in this doc is superseded", and the old bucket
table was still below it). A redesign needs the opposite: what is true now, the
reasoning behind it, the research, and the open questions, each in its own
place. Nothing was dropped in the split. The history that no longer describes
the code is in the [ledger](#the-decision-ledger) below with its date.

# Reading order

| page | what it answers |
|---|---|
| [The graph as built](/concepts/platform/antfarm/model.md) | What is in the `antfarm` mantle today: the six payloads, every node kind with its ports and fields and whether any code reads it, the default colony, what an edge means, where secrets live, local-first storage, publish and mirror. |
| [Holidays](/concepts/platform/antfarm/holidays.md) | What a holiday is (Void Core's word), how it differs from a node, a payload and a capability, the effect gate every crossing goes through, and the "effect boundary plus a Lens" account with an honest note on how much of it Hormiga uses. |
| [Capabilities](/concepts/platform/antfarm/capabilities.md) | The function-call pattern ("host it online" returns a link), the two capability tables that exist, how readiness is answered, and what capabilities should become. |
| [How the outside APIs map on](/concepts/platform/antfarm/mappings.md) | Research: the pivot per payload, how each family of vendor API maps onto it (row stores, object stores, image hosts, static deploys, DNS, sign-in, calendars, email, models), the common anatomy of a vendor API, and prior art in other tools. |
| [Across devices](/concepts/platform/antfarm/collaboration.md) | Why the Antfarm does not sync today, what the author wants instead (Q78), and what Void Maiz's September work (wires as runes, claims, device-scoped names) changes about doing it safely. |
| [CLI example usage](/concepts/platform/antfarm/cli-examples.md) | Real transcripts, run on 2026-09-22 against the shipped `voidhormiga-cli`: building the default colony, reading it, wiring a deploy target, the effect gate, and the gaps the session found. Written as the specification the GUI overhaul must match. |
| [The redesign workbook](/concepts/platform/antfarm/redesign.md) | For the author's study: everything the author has said about the Antfarm, what is broken and why, the design questions the redesign must answer (each with options and a lean), and an anatomy to react to. |

# What any redesign must keep

These rules come from different dates and pages. Together they are what makes
the Antfarm this application's I/O boundary rather than a settings screen with
a graph drawn on it.

1. **One rune per backend, and wiring is configuration.** The edge from
   `core.records` to a SQLite node *is* the statement "the organization's data
   lives here". There is no separate settings file the graph merely depicts.
2. **Ports are typed by payload.** A `site` plug does not fit a `records` port.
   An image host and a site generator can never be confused, because they
   handle different things.
3. **A holiday is the only impure arrow.** Every crossing of the boundary is a
   logged effect. Effects are refused unless granted, and each one carries a
   sentence saying what it will do to the world.
4. **Secrets are never fields.** Fields are exported state. A node names a
   vault entry or a key file. It never holds the key.
5. **Capabilities, not vendors.** The rest of the application asks "put this
   image online" and gets a link back. It never asks for ImgBB.
6. **Local-first defaults, and every cloud host is disposable.** A fresh
   database is wired and entirely local. Cloud is something an admin adds,
   knowing what it is. Nothing may be adopted that the organization cannot
   walk away from with its data.
7. **The Antfarm is where behaviour is seen and changed, not where it is
   built.** Nobody should have to assemble nodes to get a bilingual newsletter.
8. **Every GUI gesture has a CLI twin.** The canvas, the inspector, a wizard
   and an agent are four callers of the same commands. The Antfarm's
   "dependable way in" today is the command line, which is only acceptable
   because the rule held.

# Status, honestly (2026-09-22)

- **The bones work and agents can drive them.** Every publish, upload,
  import, deploy and LAN share in the application goes through Antfarm nodes,
  and the CLI drives all of it.
- **The GUI does not work well enough for people.** The tab opens with a
  warning saying so (2026-09-15). The author: *"it's mostly the UI/UX of the
  antfarm is horrible to a point where it might be unusable."*
- **Payload types are enforced only by the canvas.** Found while writing the
  [CLI examples](/concepts/platform/antfarm/cli-examples.md): `link core
  site-pages --relation 1:1` wires `records` into a `site` port and succeeds,
  and `--relation 9:1` names a port that does not exist and also succeeds.
  Ground rule 3 says the dispatcher is the only door. So a type the door does
  not check is not a type. This is the first item in the
  [redesign workbook](/concepts/platform/antfarm/redesign.md).
- **Wiring is not read.** Also found on 2026-09-22: no code follows an
  Antfarm edge. Every consumer finds its node by glyph, so an unwired node
  works and rule 1 above is a commitment the code does not yet keep
  ([model](/concepts/platform/antfarm/model.md) §"What an edge is").
- **Capabilities are tables in headers** (`domain/hosting.hpp`,
  `domain/collab.hpp`), not declarations on node kinds.
- **No program-callable holiday seam exists** for Allomone. That is on
  purpose. It is co-developed with Allomone's I/O phase (see
  [the three DSLs](/concepts/foundation/dsls.md)).
- **The Antfarm syncs between members (since 2026-09-25, Q78).** Nodes that
  name a credential file stay on their device. New device nodes, and an
  Arrange button that lays the graph out by flow. See
  [across devices](/concepts/platform/antfarm/collaboration.md).

# The decision ledger

The dated history, one line each, so the pages can describe what is true now
without losing why.

| date | decision | where it lives now |
|---|---|---|
| 2026-07-16 | Founded. Holidays as nodes, typed ports, live `describe()` faces; **defaults, not assembly** (Q5); v1 ships entirely local (Q4). | [model](/concepts/platform/antfarm/model.md) |
| 2026-07-16 | **No subscription services**: the self-hosted horizon, the org's own BaaS on its own domain. | Reversed 2026-08-19, below |
| 2026-07-19 | v1 graph live: the core hub, one rune per provider, Supabase's *Import now* face button. | [model](/concepts/platform/antfarm/model.md) |
| 2026-07-22 | **The `.miga` is the database's door, not its warehouse**: it saves the Antfarm, and the data lives behind the nodes. One database, many artifacts. Topology is personal and shared nodes are the collaboration. Read vs write nodes (Q19). | [model](/concepts/platform/antfarm/model.md), [across devices](/concepts/platform/antfarm/collaboration.md) |
| 2026-07-23 | `hol_localhost`: the website served as a site, not a file. | [model](/concepts/platform/antfarm/model.md) |
| 2026-08-04 | The Antfarm is Hormiga's **interface/protocol DSL**: port-mapping, no logic. Styling the app needs no holiday. | [holidays](/concepts/platform/antfarm/holidays.md), [three DSLs](/concepts/foundation/dsls.md) |
| 2026-08-05 | **Q25: ports typed by payload** (records / assets / site), publishing as a pipeline, locality badged, local-only default colony. Replaced the Data/Asset/Output/Import buckets. | [model](/concepts/platform/antfarm/model.md) |
| 2026-08-12 | The LLM interface is **`model`**, not "the Queen" (Q38). | [mappings](/concepts/platform/antfarm/mappings.md) |
| 2026-08-12 | **A holiday is an effect boundary plus a pure Lens**, and the pivot rule. | [holidays](/concepts/platform/antfarm/holidays.md), [mappings](/concepts/platform/antfarm/mappings.md) |
| 2026-08-19 | Self-hosting reversed, because nothing is always on. Two new payloads: **`domain`** and **`identity`**. Submissions ride `records`. | [model](/concepts/platform/antfarm/model.md), [web platform](/concepts/platform/web-platform.md) |
| 2026-08-20 | `deployment` runes: publish history kept by Hormiga, not the vendor. | [model](/concepts/platform/antfarm/model.md) |
| 2026-08-27 | `hol_lan_peer`: LAN peers as a records source *and* store. No new port type for sync. | [across devices](/concepts/platform/antfarm/collaboration.md) |
| 2026-09-02 | `hol_github` publishes natively. Every cloud host is disposable, now as a property the org can exercise. | [model](/concepts/platform/antfarm/model.md) |
| 2026-09-15 | **Host it online: the first capability**, a holiday as a function call. | [capabilities](/concepts/platform/antfarm/capabilities.md) |
| 2026-09-15 | The tab says it is not ready, and becomes two windows (Node graph, Inspector). | [redesign](/concepts/platform/antfarm/redesign.md) |
| 2026-09-16 | `hol_lan_share` and `hol_membership`, and the second capability table. **What a join carries is the Antfarm's decision.** | [across devices](/concepts/platform/antfarm/collaboration.md) |
| 2026-09-16 | **The host's Antfarm wins** on credentials and wiring; keys are meant to go stale. | [across devices](/concepts/platform/antfarm/collaboration.md) |
| 2026-09-19 | The author wants the Antfarm to sync, with private nodes (Q78), and floated isolating it *"in its own void based thing"*. | [redesign](/concepts/platform/antfarm/redesign.md) |
| 2026-09-22 | A folder. The CLI found to accept ill-typed links, and no code found to read wiring. | this page |
| 2026-09-25 | **The Antfarm syncs** (Q78: "all devices share the complete antfarm"), except credential-naming nodes. `hol_device` and `hol_device_paths`; Arrange; the tab's code moves to `ui/antfarm.cpp`. | [across devices](/concepts/platform/antfarm/collaboration.md) |

# Where the Antfarm meets the rest

- [The `.miga` bundle](/concepts/platform/miga-format.md): what the Antfarm
  topology and its sealed secrets look like on disk.
- [Security](/concepts/platform/security.md): the vault, the render seam, and
  why there is no fallback secret.
- [Data planes](/concepts/platform/data-planes.md): which way each node's data
  is allowed to flow.
- [The web platform](/concepts/platform/web-platform.md): the cloud nodes and
  the order they are built in.
- [LAN sharing](/concepts/platform/lan-sharing.md): the collaboration nodes in
  full.
- [The three DSLs](/concepts/foundation/dsls.md): the Antfarm beside the
  Builder and Allomone.
- [Void Reyna](/concepts/projects/void-reyna.md): the sibling project where
  holidays are taken most seriously as a transformation primitive.
