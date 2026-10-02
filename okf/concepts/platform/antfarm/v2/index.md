---
type: Index
title: Antfarm v2
description: "The reimagined Antfarm (designed and prototyped 2026-09-28; the migration is not built): a .miga is a set of chambers (Data, Assets, Network, Documents) and the Antfarm is where a person configures how those chambers speak to each other and to the world. Mantles flow and are filtered like Blender geometry, rivers are where data rests, keys are shared credentials, documents render to domains, and the canvas draws the effect boundary as the ground surface of an ant farm. A new version, not compatible with v1, reached by a logged migration command. This index holds what changed, what was kept, the vocabulary, the reading order, and the status."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

# Antfarm v2

> **Built, in two passes (2026-09-28).** The chambers are real mantles, v2 nodes
> run real effects through the effect gate, and the Cat Dataset ships with a
> showcase Antfarm. What runs, and what does not yet, is in §"What is built" at
> the end of this page. Where a page below says more than that section, it is
> still design.

These pages are the design the author asked for on
2026-09-28, written from the author's brief and the leans the author delegated
("go with your lean for now, then I'll provide more feedback"). Every lean that
the author has not confirmed is marked **lean**. The v1 pages one folder up
([the graph as built](/concepts/platform/antfarm/model.md) and the rest) still
describe the code, and stay true until the [migration](/concepts/platform/antfarm/v2/migration.md)
ships.

## What the Antfarm becomes

The author, 2026-09-28:

> the antfarm is where we are configuring those holidays, and defining how
> things actually speak to each other. the interactions between the mantles.

In v1 the Antfarm was a list of backends with typed plugs. In v2 it is the
**configuration of a `.miga` as a whole**:

- **A `.miga` is a set of chambers.** Every chamber is a mantle: **Data** (the
  organization's runes), **Assets** (the files, by content), **Network** (the
  profiles and devices that share this database) and **Documents** (every
  newsletter, website, calendar and map). The Antfarm and the Allomone rules are
  mantles too. The author: *"everything we control is on a mantle of some
  sort."* See [chambers](/concepts/platform/antfarm/v2/mantles.md).
- **Mantles flow.** A mantle leaves its chamber on a wire and is narrowed by
  operator nodes (by name, glyph, tag, date), the way Blender's Geometry Nodes
  pass geometry and select from it with fields.
- **Tunnels join chambers.** A contact's photo is a reference from Data into
  Assets. A document reads Data. A change is attributed to a Network profile.
  These mappings are pure, and they are configured and checked here.
- **Rivers are where data rests.** The author replaced "path" with **river**: a
  named place data goes into and comes out of, backed by one or more
  **reservoirs** (a local folder, a bucket, a peer's disk over Reticulum). Local
  first. See [rivers](/concepts/platform/antfarm/v2/rivers.md).
- **Keys are shared.** A credential is its own node, stored identically on every
  member's device, never shown raw, and monitored for cost and expiry. See
  [keys](/concepts/platform/antfarm/v2/keys.md).
- **Documents render to domains.** Every document has a `preview` and a
  `publish` output, and a domain is where they go. Calendars and maps are
  documents. See [documents](/concepts/platform/antfarm/v2/documents.md).
- **The canvas draws the effect boundary.** An ant farm is a cross-section of
  earth with the surface at the top. The v2 canvas is too: pure work happens in
  the chambers, this device is the ground, and everything that leaves the device
  happens at the surface. A wire that crosses the surface passes a **gate**. See
  [the canvas](/concepts/platform/antfarm/v2/canvas.md).

## The four words, revised

v1 kept four words apart: holiday, node, payload and capability
([holidays](/concepts/platform/antfarm/holidays.md)). v2 keeps them and adds
four, because the brief made them necessary:

| word | what it is | example |
|---|---|---|
| **chamber** | one of the `.miga`'s mantles, seen from the Antfarm | Data, Assets, Network, Documents |
| **tunnel** | a *pure* mapping between two chambers: no I/O, checkable, round-trippable | `contact.avatar → asset` |
| **holiday** | an *effectful* crossing to a system the application does not own (Void Core's word, unchanged) | commit a site to GitHub Pages |
| **river** | a named place data rests, over one or more reservoirs | `photos`: this device, then R2 |
| **reservoir** | one concrete backing of a river | a folder, a bucket, a peer's disk |
| **key** | a shared credential, as a node | `cloudflare-main` |
| **rendition** | what a document becomes for one output (after the privacy seam) | the site as a manifest, an `.ics` feed |
| **domain** | a place renditions are served or sent from | `localhost:8780`, `example.org`, a mail domain |

**Tunnel and holiday are deliberately different words.** The author called the
Data↔Assets mapping a holiday. It is the most important mapping in the design,
but it crosses nothing: both ends are inside the `.miga`. Void Core's
definition of a holiday is *reaching a system the core does not own*, and the
whole [effect gate](/concepts/platform/antfarm/holidays.md) depends on only
holidays being impure. Calling a tunnel a holiday would put a gate on something
that cannot harm anything, or teach people that holidays are sometimes safe.
Tunnel is also the ant word for it: chambers joined by tunnels.

## What v2 keeps from v1

The eight rules in the [v1 index](/concepts/platform/antfarm/index.md) survive,
with two changes:

1. **Wiring is configuration, and now code reads it.** v1 never followed an edge
   (B1). In v2 every consumer resolves its node *through the wires*, and an
   unwired node says "not connected, so nothing uses it" (the workbook's A2,
   answered yes).
2. **Types are checked at the door.** `farm plug` refuses an ill-typed wire in
   the CLI and the GUI alike (A1). Ports are named, never numbered (B8).
3. **A holiday is the only impure arrow.** Unchanged, and now visible: the
   surface line.
4. **Secrets are never fields.** Unchanged. **Changed:** secrets are no longer
   device-only. Keys are shared between members (the author, 2026-09-28), sealed
   in transit and at rest. This reverses the 2026-09-16 and 2026-09-25
   decisions; see [keys](/concepts/platform/antfarm/v2/keys.md) §"The reversal".
5. **Capabilities, not vendors.** Unchanged, and absorbed: a capability becomes
   an `answers` declaration on a node kind (A5).
6. **Local-first defaults, every cloud host disposable.** Unchanged, and now a
   property of rivers: every river has a home reservoir on this device unless it
   says, on its face, that it has none.
7. **Seen and changed, not built.** Unchanged. A fresh database is wired by its
   seed. Nobody assembles nodes to get a newsletter.
8. **Every GUI gesture has a CLI twin.** Unchanged, and extended to widgets:
   **no widget without a verb** ([canvas](/concepts/platform/antfarm/v2/canvas.md)).

## Reading order

| page | what it answers |
|---|---|
| [Types and connections](/concepts/platform/antfarm/v2/types.md) | The socket types, their shapes and colours, what may connect to what, cardinality, placement, and strands |
| [Chambers](/concepts/platform/antfarm/v2/mantles.md) | The `.miga` as a set of mantles; selecting and filtering mantles (the index feature); mantle size; tunnels between chambers |
| [Rivers](/concepts/platform/antfarm/v2/rivers.md) | Where data rests: rivers, reservoirs, reach, the S3 mapping, local-first, Reticulum, and the management nodes |
| [Keys](/concepts/platform/antfarm/v2/keys.md) | Shared credentials: storage, sharing, compatibility, monitors, and the reversal of "keys stay home" |
| [Network](/concepts/platform/antfarm/v2/network.md) | Profiles and devices as runes, linking profiles, and placement ("where does this node run?") |
| [Documents](/concepts/platform/antfarm/v2/documents.md) | Newsletters, websites, calendars and maps as one kind; preview and publish; domains; how Antfarm filters and Builder filters cannot conflict |
| [The canvas](/concepts/platform/antfarm/v2/canvas.md) | Strata and the surface line, gates, live faces, strands, the dashboard, the palette, the phone |
| [The CLI](/concepts/platform/antfarm/v2/cli.md) | The `farm` grammar: every verb, what it compiles to, and transcripts to build against |
| [Migrating from v1](/concepts/platform/antfarm/v2/migration.md) | For agents and people with a v1 database: what maps to what, the migration command, mixed-version networks |
| [Phases](/concepts/platform/antfarm/v2/phases.md) | The order of work, each phase gated by an exit test |

## Is it its own thing?

The author, 2026-09-28: *"other applications may eventually want to use some
functionality that the antfarm provides."* **Lean: a separable layer now, a
package when a second application asks** (the workbook's A9, sharpened). The
protocol core (types, the checker, capability declarations, readiness, the
effect gate) is written in `src/antfarm/` with no Hormiga domain or UI
includes, enforced by `tools/check_layering.py`. It leaves the repository the
day Portfolio Manager or Void Reyna wants it, and not before. A name is
[Q92](/developer_questions.md).

## Deferred, by the author

The author named three topics to elaborate later. The design leaves room for
each and decides none of them:

- **The network itself**, including a Wi-Fi router as a device in the Antfarm
  ([network](/concepts/platform/antfarm/v2/network.md) §"Stations").
- **Live data for hosted websites** ([documents](/concepts/platform/antfarm/v2/documents.md)
  §"Live renditions").
- **More operator nodes.** The pages name a first set per type. The author
  expects more, and the type system is built so that adding one is a
  declaration, not a redesign.

# What is built (2026-09-28)

**Two passes in one day.** The first prototype read the chambers it did not
have from what existed, and the author refused that as a shortcut: *"i don't see
how we can even have a prototype yet when the assets, network, and documents
dont even exist as mantles yet ... that's an integral aspect of this whole
overhaul."* The second pass built them. What runs now, desktop and phone alike:

| piece | where | state |
|---|---|---|
| **the chambers**: `assets`, `network`, `documents` as real mantles, kept in step by a reconcile that writes only what is out of step | `domain/chambers.hpp`, `app/farm_host.cpp` | built; every database gets them at boot; `tests/chambers_smoke.cpp` |
| the separable core: types, 38 node kinds (7 *planned*), named ports, the door, the audit, the evaluator, faces, the `farm` verbs | `src/antfarm/` | built; knows Void Maiz and nothing of Hormiga's; `tests/farm_smoke.cpp` |
| **effects**: `farm run` (Import CSV, Store, the tunnel), `farm check` (folder, domain, bucket, key), `farm preview`, `farm publish` | `app/farm_effects.cpp` | built; each is `effect farm-…`, gated in the CLI like `deploy-site` |
| the Cat Colony showcase | `farm showcase`, and every fresh database | built |
| `voidhormiga-cli farm …` | `main/farm_cli.cpp` | built; its own session, like `update` |
| desktop tab: Connections, Wiring with strata, Inspector with a key box, Arrange, Fit | `ui/farm.cpp` | built |
| phone: Connections, detail, graph | `phone/phone_farm.cpp` | built |
| the canvas: fan-in pills, strands, a two-level add box, port-filtered add-and-link, no move flicker | Void Maiz (`canvas.cpp`) | built there, logged in its OKF |

## The chambers, as built

- **Assets** holds one `asset` rune per file: `file`, `sha256`, `bytes`,
  `media`, `original`, `url` and `hosted_by` when a store puts it online. Names
  come from the content address (`a-<16 hex>`), so two devices mint one rune for
  one file. **References in Data stay paths**, because every path Hormiga writes
  already embeds the file's sha256 (`flier-<sha256>.jpg`) and every file goes
  through one door (`resolve_file`). The tunnel joins a path to its asset by the
  file or by the address inside the name. Rewriting every picture field to an
  `asset:` reference would buy nothing the name does not already carry.
- **Network** holds one `profile` rune per device, written by that device about
  itself from the profile on disk (so the first boot already knows its key
  fingerprint). It writes nothing when nothing changed.
- **Documents** holds one `chamber_doc` rune per Builder document, calendar view
  and map view: kind, title, mount, last published, published to. **Identity and
  deployment live here; content stays where its tab edits it**, so the Builder,
  the Calendar and the Map work unchanged. A document whose content is removed
  is marked `gone`, never deleted, so its publish history survives and a device
  that has not synced yet cannot delete another member's work.

The reconcile runs at boot, whenever the v2 Antfarm is on screen, after a file
is ingested, and on `farm chambers`. It writes only what is out of step, so a
second pass writes nothing.

## Effects, as built

**No second deploy path.** `farm publish` and `farm check` hand v1's code
(`deploy_site`, `check_host`, `check_store`, `host_online`) a v1-shaped host
built from the v2 web domain or bucket and its key.

- `farm run <import>` compiles the CSV with the same importer as *Data > Import
  CSV* and lands it as one batch.
- `farm run <store>` copies every file a mantle names into each folder reservoir
  of its river, and puts them online through a bucket or image host, writing
  each link onto its asset rune.
- `farm preview <document>` builds **everything mounted on that document's
  local domain** into one `site/`: the website at `/`, a newsletter at its
  mount, a calendar as a real `calendar.ics`, and a map as a GeoJSON layer (plus
  a picture of it when there is a window to draw with). In the application it
  serves it on localhost.
- `farm publish <document>` builds everything mounted on its web domain and
  deploys it, then writes the `deployment` rune and each document's
  `last_published`.

## The grant, enforced (2026-10-01)

Each document in a v2 preview or publish renders against **only the data runes
its `data` input carries**. The renderers themselves narrow their projection
(`farmhost::apply_grant`, in `render/site.cpp`, `render/email.cpp`,
`render/index.cpp`, the calendar and the map), so a Builder block that names an
excluded rune by name finds nothing. An unwired document is refused, not
published empty. Measured on the showcase: Garfield tagged `staff-only` and named
by a directory block appears in the Builder's own build and not in the v2
preview, whose grant says `NOT staff-only`. The Builder shows the grant under
its document picker (*"cat-news may publish 107 of 108 data runes, narrowed by
public"*).

## The migration, built (2026-10-01)

`farm migrate` rehearses and `farm migrate apply` builds the v2 graph from the
v1 one, as [the migration page](/concepts/platform/antfarm/v2/migration.md)
specifies, with two departures recorded there: the v1 mantle is not marked or
removed (LAN sharing and the Publish tab still read it), and a v1 peer is not yet
refused by version (v2 members stop sharing the v1 mantle instead).

## Still not built

Key sharing between members (V3); rings showing where a node runs; gates drawn
on the surface line; `farm key reveal`; mail sending (the mail domain is
*planned*); peer reservoirs; distributaries and dams (placed and *planned*); the
v1→v2 migration command (V7). Placement is enforced (a node placed on another
profile refuses to run here) but not drawn.

# Found on a copy of a real database (2026-09-29)

Phase V7 says v2 is tried on a copy of a real database before anyone migrates.
A partner organization's agent did that on 2026-09-28, by hand, with
[migration](/concepts/platform/antfarm/v2/migration.md) §4 as the table: a copy
in its own folder, 351 data runes and 132 files, no credentials, `farm init`,
then the v1 nodes translated line by line. The report was reproduced against
the source before it was written here. The organization is not named (the
[log](/log.md)'s rule).

**What held.** Every translated `farm plug` passed the door. `farm preview` of
the website is **byte-identical** to v1's `effect render-site` from the same
binary, except `?v=` cache stamps and `.ics` DTSTAMPs. Readiness travelled up
the wires in plain sentences.

**What did not, most serious first:**

1. **The map rendition bypasses the privacy seam** (ground rule 6). v1's
   renderer publishes a person only with `clearance:public`. The map branch of
   `farm-preview`/`farm-publish` (`app/farm_effects.cpp`) writes every rune the
   Antfarm filter keeps that has `geo`, with name, title and tags. The seeded
   filter is `NOT private`, and a database that marks consent with
   `clearance:` rather than `private` sent **8 people's coordinates** into
   `places.geojson`. Local in the trial and not wired to a web domain, so
   nothing was published. "One privacy seam for all four kinds" (V4) is not
   true of the map yet. Workaround in a graph: filter the map's data with
   `(NOT glyph contact) OR clearance:public`.
2. **The desktop application writes the chambers into every database it
   opens.** `UiState::chambers_dirty` starts `true` (`app/farm_host.hpp`), so
   the first frame reconciles and adds `assets`, `network` and `documents` to a
   v1 database. That contradicts [phases](/concepts/platform/antfarm/v2/phases.md)
   ("no organization's database is touched before V7"), splits what a database
   holds between a dev build and an installed 0.1.x, and syncs the new mantles
   to members. The CLI reconciles only on `farm init`, `farm chambers` and
   `farm showcase`. [Q98](/developer_questions.md).
3. **`farm preview` drops documents silently.** A calendar and a newsletter,
   both mounted on the local domain and *ready*, were neither built nor named:
   headless, `export_calendar_ics()` and `render_preview()` return empty and
   the loop skips them. A skipped document should be a line with its reason.
4. **`farm init` chose the wrong website**: a leftover demo document, not the
   one the organization publishes. With several Builder documents it should
   ask, or take the last one published.
5. **The Documents chamber calls every Builder document a `newsletter`**, a
   website included.
6. **A river says *ready* when its only reservoir is not** (an image host that
   *needs* its key, a bucket that is *unset*). Readiness does not travel from
   reservoir to river.
7. **`farm check pictures` answers "has no check yet"**, though
   [migration](/concepts/platform/antfarm/v2/migration.md) §7 names it as the
   verification step. `farm show pictures` has the numbers.
