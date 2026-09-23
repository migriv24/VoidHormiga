---
type: Concept
title: The Antfarm across devices
description: "Why the Antfarm does not sync today (the host's wins, keys go stale, and the absolute-to-relative rewrite would flow back), what the author asked for instead (Q78: sync the wiring, private nodes), and what Void Maiz's September collaborative-canvas work changes about doing it safely: wires as runes, device-scoped names, claims, presentational joins, and the physics rule's 'one device drives' pattern, which answers the question nobody had asked yet: which device performs a synced node's effects?"
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

# Today: the Antfarm stays home

Members sync their databases automatically since 0.1.5, through Void Maiz's
`voidmaiz_net` over Void Palabra's session ([LAN sharing](/concepts/platform/lan-sharing.md)
§3b). **The `antfarm` mantle is excluded**: `share_mantle` in
`app/lan_net.cpp` refuses it, and the splice puts this device's own Antfarm
back. Three reasons, all from 2026-09-16:

1. **The host's Antfarm wins on credentials and wiring** (§3a). The person who
   shared the database decides where it publishes and with whose keys.
2. **Keys are meant to go stale.** Hormiga cannot revoke a vendor key it
   handed out. The operator's practice is short-lived tokens, and the design's
   job is to make handing over the *next* one cheap (a refresh, decided and not
   built).
3. **The join plan rewrites paths.** A key file that was absolute on the host
   is sent as a plain file name, set on a throwaway core. If the Antfarm
   synced, that rewrite would flow back and change the host's own nodes.

**What a join carries is the Antfarm's decision** (§3): the plan is built from
the nodes. Files with no online copy go, hosted files do not, a personal SQLite
store goes, key files and unlocked vault secrets go, and private runes never
do.

# What the author asked for (Q78)

After the two-machine test, 2026-09-19:

> if i add a node on one antfarm, it should do stuff on the other. maybe private
> nodes can exist?

and, bigger than the question:

> potentially isolate it in its own void based thing

[Q78](/developer_questions.md)'s lean: **yes for wiring, no for secrets.** Nodes
sync like any other rune. A `private` tag keeps a node on its device. Key files
and vault secrets travel only at join time and, later, on refresh.

# What Void Maiz's September work changes

Void Maiz spent 2026-09-20 to 09-22 on *several people editing one node graph*
([their collaborative canvas](../../../../../VoidMaiz/okf/concepts/collaborative-canvas.md)),
tested on Interaction Combinators across a PC and a phone. Almost every bug they
found is one a synced Antfarm would have hit. In order of how directly it
applies:

## 1. Two devices minting one name (applies now, partly fixed)

Their worst bug: `unique_name` minted `gamma-1` on both devices, so two
different runes shared a name. Wires name their ends, so every wire touching
that name became ambiguous and the merge dropped it. The nodes looked fine.
That is why it took two sessions to see.

Hormiga met the same problem first (2026-09-19, Q74) and fixed it for its own
palettes: `mint_name()` adds four hex of the device fingerprint in a shared
database. **But Void Maiz's canvas mints names too** (its add palette, the
long-press-to-create on glass, paste), through `CanvasStyle::device_tag`, and
its header says *"a host that shares its document MUST set it."* **Hormiga did
not set it**, and both of Hormiga's canvases (the Builder's and the Antfarm's)
place nodes through Void Maiz's palette. So on the Antfarm it was a future bug,
and **on the Builder, which does sync, it was a live one**: two members adding
the same block kind from the palette each minted `<glyph>-1`. Set on 2026-09-22
from the same `device_tag()` that `mint_name` uses. It is a candidate cause for
the author's still-undiagnosed note 6 of 2026-09-19 (the Builder on the Linux
member showing content that differs and does not update). See the
[log](/log.md).

## 2. Wires as runes (decide after wiring authority)

Antfarm edges are plain relations stored inside the source rune. Void Maiz
measured the difference (`net_smoke`, the §4.2 case under 30% loss): two
partitioned rewires that share a wire **lose it** with plain edges, and keep it
with **wire runes** (Palabra SPEC 5.11, `voidmaiz/wires.hpp`, written through
`CanvasStyle::wires`).

**Whether the Antfarm needs this depends on a question it has not answered.**
No code reads Antfarm wiring today ([model](/concepts/platform/antfarm/model.md)
§"What an edge is"), so a lost edge would be a lost *drawing*. If the redesign
makes wiring authoritative, a lost edge becomes a lost *configuration*: a site
that silently stops publishing to its host. So: **decide wiring authority
first ([redesign](/concepts/platform/antfarm/redesign.md) A2). If wiring
becomes authoritative, adopt wire runes before the Antfarm syncs**, and use
`compile_upgrade`, which Interaction Combinators proved preserves a net
exactly.

## 3. Claims: two admins, one node

First to select holds it, for people and agents alike. A claim made after
seeing another loses, and a person outranks an agent (`voidmaiz/claims.hpp`,
Lamport order). For the Antfarm this is the difference between two admins
typing into one node's `repo` field at once, and one of them seeing
"Gary is editing this" first. Hormiga already takes the human-floor half of
this through `voidmaiz_headless`. The canvas half arrives with the N1 drawing
upstream, which is not built.

## 4. Positions converge, content conflicts

`presentational_joins()` (the default in `NetOptions::joins`, which Hormiga
already uses) resolves `content.pos`, `placement`, sizes and routes by
Lamport-latest. **Antfarm node positions are `content.pos`**, so moving nodes
on two devices converges silently, as the author ruled (*"i don't see an issue
with last one wins"*). A `repo` or `bucket` edited on two devices is content,
and surfaces as a conflict. That is right: which bucket the website publishes
from is not a thing to settle by clock.

## 5. "One device drives": the pattern for effects

The least obvious and most useful precedent. Interaction Combinators' live
physics used to be a switch per device, and every device streamed positions.
The author: *"if live physics is turned on, it should be turned on for all
synced devices, rather than a constant update of position information."* Now
it is **a rule of the mantle**, `{"rule":"physics","driver":"<device>"}`
(`voidmaiz/rules.hpp`). It crosses as one command, and **one** device drives
while the others receive the results as ordinary edits.

A synced Antfarm needs exactly this, for a question nobody has asked yet:
**when a node syncs, which device performs its effects?** If the backup node
syncs to four laptops, four laptops must not all push the backup. If the
deploy node syncs, a deploy must not start from a device whose key is stale.
The answer is the same shape:

- **Describing a backend syncs.** The node, its target, its wiring.
- **Performing it is driven.** A node (or the mantle) names the device that
  runs its automatic effects: backups, scheduled publishes, a future relay's
  upkeep. A person pressing *Publish* on any device is still a person's
  explicit act, gated as always. But a node whose `driver` is another device
  says so on its face: "Maria's laptop publishes this".
- **Readiness is per device.** `host_problem(node)` on a device without the
  key answers "no key on this device", which is true, local and not a
  conflict.

# The proposed shape, for the author

Assembled from Q78's lean and the five points above. **A proposal, not a
decision.**

| part of a node | syncs? | why |
|---|---|---|
| existence, glyph, position | **yes** | describing the organization's backends is shared knowledge |
| target fields (repo, bucket, zone, port) | **yes**, as content (conflicts surface) | two admins disagreeing about where the site goes must see it |
| wiring | **yes**, as wire runes once wiring is authoritative | §2 |
| credential *references* (`token_key`, `key_file`) | **yes**, the name only | a reference is not a secret |
| credentials themselves | **never by sync**; at join, and later on refresh | §3a: keys go stale on purpose |
| `private` tag on a node | keeps the whole node home | the same `ShareFilter` presence and sync already ask |
| who drives its automatic effects | a field or a mantle rule, synced | §5 |
| readiness | computed per device, never synced | it is a fact about this device |

**The host still wins on credentials**, and now that is all it wins on. The
rewrite in point 3 of "today" stops being a hazard, because file names are
already relative and the absolute path is a per-device binding.

# Where the database's profile comes in

[Q79](/developer_questions.md) asks for a database profile carrying *"a
summary of what its Antfarm can do"*, shown on the Discover card before
someone joins. With capabilities declared on node kinds
([capabilities](/concepts/platform/antfarm/capabilities.md)), that summary
reads directly from the synced Antfarm: "publishes a website to GitHub Pages,
hosts images on R2, shares on the LAN". It shows no keys and no targets, only
what the organization can *do*, which is what a person deciding whether to join
needs to know.
