---
type: Roadmap
title: Antfarm v2 — phases
description: "The order Antfarm v2 is built in, each phase gated by an exit test rather than a date, command line before canvas in every phase: V0 foundations (the separable layer, named ports, the checker, the version), V1 chambers and selection, V2 rivers, V3 keys, V4 documents and domains, V5 network and placement, V6 the canvas and Connections, V7 migration, V8 the wizard. What each phase touches outside the Antfarm, and what Void Maiz is asked for when."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Written as design on 2026-09-28; V0, most of V1, V2 and V4 were built the
same day** (each phase below says how far). The author: *"This will likely require many phases, for
the development of many different aspects of void hormiga."* It does, because
v2 reaches into Data (assets), the Calendar and Territory tabs (documents),
collaboration (keys, profiles) and Void Maiz (the canvas). Every phase builds
its CLI verbs first and its GUI second (the v1 rule 8), and each ends on an
exit test in the style of the [roadmap](/roadmap.md).

**Throughout V0 to V6, v1 keeps running every organization's database.** v2 is
built in databases created with v2 enabled: first the Cat Dataset, then a copy
of a real database on the author's machine. No organization is migrated before
V7. **Two corrections, 2026-09-29:** there is no "v2 enabled" switch; a `farm`
mantle is what makes a database v2, and the desktop application adds the
chambers to every database whatever it holds ([Q98](/developer_questions.md)).
The copy of a real database was tried on 2026-09-28; what it found is in the
[index](/concepts/platform/antfarm/v2/index.md) §"Found on a copy of a real
database".

## V0 — Foundations

- `src/antfarm/`: the protocol core. The type registry, port declarations with
  names, `max`, `writes` and `optional`; the checker; the readiness contract;
  the effect table with consequences. **No Hormiga domain or UI includes**,
  enforced by `tools/check_layering.py` from the first commit.
- The `farm` mantle, `farm version`, `farm ports`, `farm kinds`, `farm plug` /
  `unplug`, `farm show`, `farm status` (including the audit of raw `link`s).
- **Void Maiz:** named-port relations, socket shapes, input cardinality.
- The upstream question to Void Core: should port types be part of the glyph
  contract? Drafted 2026-09-22
  (`MESSAGE_FOR_VOIDCORE_hormiga-ports-at-the-door-2026-09-22.md`), **not yet
  relayed**: it is not in Void Core's repository as of 2026-09-29.

**Exit:** a script builds a v2 graph using only `farm` verbs; an ill-typed
`farm plug` is refused with both types named; a raw ill-typed `link` is
reported by `farm status`; the canvas draws named ports with their shapes.

**Passed 2026-09-28, except the upstream question** (not yet sent). All four
are in `tests/farm_smoke.cpp` or the [CLI transcript](/concepts/platform/antfarm/v2/cli.md)
§6. **Void Maiz needed no change for V0**: named wires are resolved in the
projected scene by the host (`farm::resolve_named_wires`), the canvas writes
them through its existing `WireWriter` hook (which is also where the one-wire
rule and the type check run for a drag), and socket shapes and colours are its
existing `port_types`. The read side of V1 to V5 was built early as part of the
prototype; see the [index](/concepts/platform/antfarm/v2/index.md)
§"What is built".

## V1 — Chambers and selection

**Built 2026-09-28**, except the query nodes `Kind`, `Near`, `Linked to` and
`Changed`, and the round-trip test (the tunnel's checks run; a law test does
not). Wiring is read by every v2 consumer.

- The Assets chamber: `asset` runes, `image.asset`, image-typed fields holding
  `asset:<sha256>`. **This changes the Data model**, so ingestion, the image
  editor, hosting and the renderers change with it.
- The Network and Documents chambers as mantles (their contents move in V4 and
  V5; here they exist and are counted).
- Miga, Separate chambers, Filter, Join, Count, Measure, the query nodes, the
  tunnels. `farm eval`, `farm mantles`.
- Wiring is read: the first consumer follows edges (A2).

**Exit:** `farm eval` on a Filter over the Cat Dataset returns the same runes as
`ls --tag` with the same expression; the Data→Assets tunnel reports 0 missing
on a freshly imported database; the round-trip test for Data→Assets passes.

## V2 — Rivers

**Mostly built 2026-09-28**: River, Folder, Bucket and Image-host reservoirs,
Store (copies, and puts online through v1's `host_online`), Gauge, Check.
Distributary, Dam and Peer are placed and *planned*.

- Folder, Bucket, Image host and Website-as-host reservoirs; the River node;
  Store, Gauge, Check, Distributary, Dam, Warn below.
- "Host it online" answers from rivers. `config hosting.images` names a river.
- The home river replaces `hol_sqlite` and `hol_fs_assets` in v2 databases.
- Peer reservoirs are **researched**, not built (the four questions in
  [rivers](/concepts/platform/antfarm/v2/rivers.md) §7).

**Exit:** a photo added offline is in the home river at once and in R2 after
`farm run store`; with R2 unreachable, reads fall back to home and the face
says so; a Gauge reports used and free for both.

## V3 — Keys

- Key runes, providers with `serves`, `farm key add|set|reveal`, values sealed
  in the vault, key sharing over the sealed session to admins.
- Usage from the dispatcher log; Expiry; Budget. Vendor usage for one provider
  (Cloudflare) as the example.
- `collab::device_only` is retired for v2 databases.

**Exit:** a key added on one machine is usable on a second after one sync,
without its value ever appearing in the state document, the log or an
unencrypted pack; `farm key` on either machine shows who added it and where.
**This needs two machines**, like phase F's exit.

## V4 — Documents and domains

**Mostly built 2026-09-28**: the Documents chamber, document nodes with preview
and publish, local and web domains, mounts, one site built from every document
mounted on a domain (a calendar as `.ics`, a map as GeoJSON), and publish through
v1's deploy path. Not yet: the tabs' document switchers, and the grant shown in
each tab's filter panel.

- The Documents chamber holds one `document` rune per newsletter, website,
  calendar and map. **The Calendar and Territory tabs gain a document
  switcher**, and a database can have several of each.
- Document nodes with `preview` and `publish`; site and message renditions;
  the site manifest (incremental publish); one privacy seam for all four kinds.
- Local, web and mail (planned) domains; mounts; *nowhere to go*.
- The grant / choose / remove rule, with the grant shown in every tab's filter
  panel.

**Exit:** one web domain serves the website at `/` and a calendar's `.ics` at
`/events.ics` from one publish; a rune tagged `internal` reaches neither, even
when a Builder block asks for it by name; the phone shows *nowhere to go* and
no button.

## V5 — Network and placement

- Profiles in the Network chamber with the device facet; `same-person` links
  from both sides; the shared-username suggestion.
- Placement (`each`, `any`, a profile), the ring, automatic rules that refuse
  to run until placed.
- The admin guard on the Network chamber.

**Exit:** two profiles link only after both accept; a nightly backup placed on
one device runs on that device only while three members are online.

## V6 — The canvas and Connections

- The strata, the surface line, gates with consequences, live faces, strands.
- Connections as the tab's first view; the phone's read-only Connections.
- **Void Maiz:** strands, strata bands, gates, placement rings.
- B10, the v1 canvas's own failures, rechecked against the new canvas with the
  author at it.

**Exit:** the author opens the tab and can say, without reading a manual, what
the organization is connected to, what is not working, and what a button will
do before pressing it. **This exit test is the author's to pass.**

## V7 — Migration

**Built 2026-10-01**, except `retire-v1` and the version check in the sync
handshake (see the migration page for why each waits). The exit test's
byte-for-byte publish comparison has not been run against a real database.

- `farm migrate` as [the migration guide](/concepts/platform/antfarm/v2/migration.md)
  specifies; `retire-v1`; the version check in the sync handshake.
- The agent guide gets real transcripts.

**Exit:** a copy of a real v1 database migrates, `farm status` shows nothing
failing, a publish from it matches the v1 publish byte for byte except for
paths that v2 changed on purpose, and a v1 peer is refused with the sentence.

## V8 — The wizard

"Add a connection" from a question, last, because it is the sum of V2 to V5.

**Exit:** a person who has never seen the Antfarm publishes a map document to a
web domain with an existing key, from the wizard alone.

## Not in any phase yet

Stations (a router, a NAS), live renditions, mail sending, peer reservoirs
over Reticulum, Allomone query nodes. Each waits on the author or on another
project's phase.
