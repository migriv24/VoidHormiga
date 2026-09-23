---
type: Concept
title: The Antfarm graph as built
description: "What is in the antfarm mantle on 2026-09-22, read from the code: six payloads, twenty node kinds with their ports, fields and whether anything reads them, the default local-only colony, what an edge means and how its port indices work, where secrets live, the .miga as the door, local-first storage, and publish/mirror."
tags: [status:current, audience:all, confidence:measured]
timestamp: 2026-09-22T00:00:00Z
---

This page describes the Antfarm as the code has it, not as it was planned. The
source is `src/domain/glyphs_antfarm.hpp` (the node declarations),
`src/domain/seeds.hpp` (the default colony), `src/domain/hosting.hpp` and
`src/domain/collab.hpp` (the capability tables), and the faces in
`src/app/app.cpp`. Where this page and the code disagree, the code is right and
this page is stale.

# The mantle

The Antfarm is an ordinary Void Core mantle named `antfarm`. Its runes are
ordinary runes, so every verb works on them: `rune new`, `set`, `link`,
`undo`, `cat`, `tree`. Two things make it special, and neither is structural:

- **Its glyphs are declared with ports.** A glyph's `hints.ports` lists named,
  typed, directed ports. Void Maiz's canvas draws them as sockets and refuses a
  plug whose type does not match.
- **Code reads it.** Publishing, uploading, sharing and importing each look up
  the node they need in this mantle (`project(core, kAntfarmMantle)`) and read
  its fields. The mantle is the configuration, not a picture of it.

# Six payloads

A port's `type` is the payload that flows through it. Q25 (2026-08-05) started
with three, and the web platform (2026-08-19) added two. The sixth is
`records` again in a different role: collaboration nodes plug into it rather
than inventing a "sync" type.

| payload | what flows | who emits it | who consumes it |
|---|---|---|---|
| **`records`** | the organization's runes | `org_core.records` | stores, sources, the publisher, LAN share and membership, accounts |
| **`assets`** | bytes: images, files | `org_core.assets` | the local file store, ImgBB, an object store, the publisher, visitor uploads |
| **`site`** | a built website (a folder) | `hol_html.site` | the local server, GitHub Pages, a static host |
| **`domain`** | a name the org owns, and its zone | `hol_dns.domain` | GitHub Pages, a static host |
| **`identity`** | "who is this visitor" | `hol_auth.identity` | the accounts backend |

**Reserved, with no node yet:** `model` (an LLM, renamed from "the Queen" by
Q38), geo (Territory), and translate (the bilingual engine runs today with no
node). Auth/peer is reserved too, and LAN peers took `records` instead.

Direction belongs to the agent, not the port. A store *persists* records and a
source *imports* them, and both plug into the same `records` port. Submissions
from a website are a cloud records source, exactly like Sheets.

# The node kinds

Twenty glyphs are declared. **"Read by"** says what code consumes the node's
fields, because a node nothing reads is a promise, and the 2026-09-02 field
report's rule applies: *a field that does nothing is worse than no field.*

## The hub

| glyph | ports | fields | read by |
|---|---|---|---|
| `org_core` (Hormiga Core) | 1 `records` out · 2 `assets` out | `org_name` | the face ("wiring IS configuration") |

## Records

| glyph | ports | fields | locality | read by |
|---|---|---|---|---|
| `hol_sqlite` | 1 `records` in | `file` | local | storage: the SQLite mirror; the face shows counts or "UNAVAILABLE - running on snapshot" |
| `hol_csv` | 1 `records` in | none | local | nothing; the face points at *Data > Import CSV*, which does not read the node |
| `hol_supabase` | 1 `records` in | `dump_dir` | cloud | the *Import now* face button (`effect import-rescue`, answered by the GUI only) |
| `hol_sheets` | 1 `records` in | `sheet_url` | cloud | nothing; the face says "not configured" (import-only lean, Q4) |
| `hol_lan_peer` | 1 `records` in | `display`, `port`, `auto`, `peer_host` | local | nothing since `hol_lan_share` (2026-09-16); placeable from the palette |
| `hol_lan_share` | 1 `records` in | `allow`, `presence`, `port`, `key_file`, `private_tags`, `send_hosted` | local | `collab::share_settings`: sharing, presence, the private filter, the join plan |
| `hol_membership` | 1 `records` in | `store`, `file`, `default_role`, `precedence` | local | `collab::membership_settings`; only `store: local-file` is built |
| `hol_accounts` | 1 `records` in · 2 `identity` in | `provider`, `project_url`, `key_file` | cloud | nothing yet (web platform phase) |

## Assets

| glyph | ports | fields | locality | read by |
|---|---|---|---|---|
| `hol_fs_assets` | 1 `assets` in | `dir` | local | ingestion (content-hashed, sha256 since 2026-09-19); face counts files |
| `hol_imgbb` | 1 `assets` in | `key_file` | cloud | `host_online`, the face's "Use for images" |
| `hol_object_store` | 1 `assets` in | `bucket`, `region`, `endpoint`, `access_key_id`, `secret_key`, `secret_file`, `prefix`, `public_url` | cloud | `host_online`, `check-store`, `push-store` (index, backup) |
| `hol_uploads` | 1 `assets` in | `provider`, `bucket`, `key_file` | cloud | nothing yet (visitor uploads; a different bucket on purpose) |

## Publishing

| glyph | ports | fields | locality | read by |
|---|---|---|---|---|
| `hol_html` | 1 `records` in · 2 `assets` in · 3 `site` out | none | local | the face; rendering does not read the node |
| `hol_localhost` | 1 `site` in | `port` | local | `serve-site` (the preview server, 127.0.0.1:8780) |
| `hol_github` | 1 `site` in · 2 `domain` in | `repo`, `branch`, `token_key`, `token_file`, `message` | cloud | publish (Git Data API), `check-host`, `host_online` (copies into `site/assets/`) |
| `hol_static_host` | 1 `site` in · 2 `domain` in | `provider`, `account_id`, `project`, `token_key`, `token_file`, `deploy_cmd`, `rollback_cmd` | cloud | `deploy-site`, `rollback-site`, `check-host`, `host_online` |
| `hol_dns` | 1 `domain` out | `domain`, `provider`, `zone_id`, `token_file` | cloud | the Publish panel |

## Visitors

| glyph | ports | fields | locality | read by |
|---|---|---|---|---|
| `hol_auth` | 1 `identity` out | `provider`, `client_id`, `redirect` | cloud | nothing yet |

## Not placed by a person

| glyph | what it is | where it lives |
|---|---|---|
| `deployment` (kind `act`) | one per publish: host node, url, when, document, language, `live`/`superseded`, the vendor's id | the `antfarm` mantle, written by a publish |
| `member` | a profile that joined this database | the members registry (`members.json`), not the org's data |
| `peer` | a device paired with, public key pinned on first use | the state document |

**Count:** of the 18 kinds a person can place, 11 have code that reads their
fields. `hol_csv` and `hol_html` are live features whose node is decorative.
Five are placeholders for planned work (`hol_sheets`, `hol_lan_peer`,
`hol_auth`, `hol_accounts`, `hol_uploads`). That ratio is the first thing a
person meets in the palette, and the [redesign](/concepts/platform/antfarm/redesign.md)
has to decide how placeholders present themselves.

# The default colony

Every fresh database is wired by `seed_antfarm_transcript()`, which is plain
dispatcher commands (see [CLI examples](/concepts/platform/antfarm/cli-examples.md)
§1 for it running). Entirely local:

```
                     ┌──────────────┐
  data-sqlite  ◀─1───┤              ├───1─▶ out-html ──3─▶ out-localhost
  import-csv   ◀─1───┤  core        │        ▲  (records, assets → site)
  assets-fs    ◀─2───┤  records=1   ├───2────┘
                     │  assets=2    │
                     └──────────────┘
  lan-share    (hol_lan_share: allow yes, presence yes, port 47733, private_tags "private")
  members      (hol_membership: local-file members.json, everyone admin)
```

The collaboration nodes are placed unwired. Nothing reads their edges. Their
existence is what `share_settings()` looks for.

# What an edge is

An edge is a Void Core relation with a **relation label `i:j`**: port `i` on
the source, port `j` on the target. **The numbers are 1-based positions in the
glyph's port list, counting inputs and outputs together, in declaration
order.** So `hol_html`'s ports are 1 `records`, 2 `assets`, 3 `site`, and the
publish chain is `link out-html out-localhost --relation 3:1`.

Three things follow, and all three matter to the redesign:

- **The CLI and the log speak in indices.** `core -2:2-> out-html` means
  assets to assets, and nothing on screen says so.
- **Reordering a glyph's ports would silently rewire every database.** The
  index is the contract, and nothing pins it.
- **The type check lives in the canvas, not at the door.** The dispatcher
  accepts any `i:j`, including ports that do not exist
  ([CLI examples](/concepts/platform/antfarm/cli-examples.md) §8).

**What an edge means, per payload:** `core.records → store` is "the
organization's data is persisted here". `core.records → source` is "records
may be imported from here". `core.assets → store` is "files live here".
`publisher.site → server` is "this site is served here". `dns.domain → host`
is "this host answers for this name". **That is the intent, and no code reads
it.** Checked on 2026-09-22: publishing, hosting, sharing and importing all find
their node by glyph (the first `hol_github`, the node `config hosting.images`
names, the first `hol_lan_share`), and nothing in `src/` follows an Antfarm
edge. An unwired `hol_github` publishes exactly as well as a wired one. **So
today a node configures by existing, and wiring is a drawing.** Founding rule 1
of this folder ("wiring is configuration") is a design commitment the code
does not yet keep. Whether wiring should become authoritative, or the rule
should be restated, is the second question in the
[redesign workbook](/concepts/platform/antfarm/redesign.md).

# Where secrets live

**Never in a field.** Fields are exported state: they travel in the `.miga`,
in sync, and in `cat`. A node names its secret one of two ways:

| field shape | what it names | travels |
|---|---|---|
| `token_key`, `secret_key` | an entry in the passphrase-locked vault | yes, sealed, in the `.miga` |
| `token_file`, `key_file`, `secret_file` | a gitignored file beside the database | no, except in a LAN join plan |

A face shows a key's *presence*, never the key. There is no fallback secret
in the binary. No passphrase means no unlock ([security](/concepts/platform/security.md)).

# The `.miga` is the door, not the warehouse (2026-07-22)

The author: creating a database creates a `.miga`, and **what the `.miga`
saves is the Antfarm**: the node topology and its encrypted credentials. The
data lives behind the nodes, wherever they point. Opening a `.miga` means
wiring up to the organization. Consequences recorded then:

- **One database, many artifacts.** Many maps, newsletters, websites and
  (later) people, all reachable through one database's nodes. "Where is X
  saved?" is always answered by the graph.
- **Same database, different topologies.** Topology is personal. Shared nodes
  (one cloud store) are the collaboration. This is Q78's ancestor.
- **Read vs write nodes** (Q19). An edge should say whether the host only
  *receives* through it or also *modifies* the backing store. This is recorded
  and not built.
- **A fresh install ships a demo database**, all local.

The [v3 bundle](/concepts/platform/miga-format.md) later made the `.miga` carry
the data as well. The door idea survives as **topology in plaintext, secrets
encrypted**: the graph can be shared and diffed, and the keys never can.

# Local-first storage (the default nodes, and why)

- **Records: embedded SQLite.** The amalgamation is vendored and in-process,
  with no sidecar. Tags and links are join tables, so the tag grammar compiles
  to SQL. MeshDB is still the named growth path behind the same port.
- **Snapshot fallback regardless.** Every clean session mirrors to a JSON
  snapshot, and a corrupt database opens read-only on it with a banner. **No
  failure mode is a dead app.** That is the Antfarm's founding principle, and
  it is the first holiday *wrapper* the design has
  ([holidays](/concepts/platform/antfarm/holidays.md) §6).
- **Assets: the local file store.** Content-addressed by sha256, so a file
  that travels can be verified against its name.

Offline-first is the resting state, not a milestone. The network is something
you add, as a node.

# Publish and mirror

Two effects, twins, both logged:

- **Publish (local → cloud).** A rune's local bytes (local-first means they
  are home before they leave) are handed to a host, and the returned public
  URL lands as **one `set` command**. That is a model change, and it can be
  undone.
- **Mirror (cloud → local).** Every field holding a remote URL is made local by
  the cheapest honest route: mirror cache, then the rescue's `media/`, then a
  download. The results land in **two tiers on purpose**. Bytes plus
  `assets/mirror.json` are a *holiday cache*, rebuilt by re-running, and not
  model truth. An image rune gaining a local `path` gets **one `set`**, which
  *is* model truth. Dead URLs are recorded as `""`, as evidence, and are never
  retried endlessly.

The invariant the pair keeps: **every cloud host is disposable.** A hundred
images already dead on a free CDN (the rescue's finding) cannot recur for
anything mirrored.

# Publish history is ours

A publish writes a `deployment` rune through the dispatcher. It is logged,
attributed, replayable, and travels in the `.miga`. The vendor's deployment
list is still read, for `vendor_id` and the permanent per-deploy URL, but it
*enriches* a record Hormiga already holds. A history kept only in the vendor's
database is lost the day the organization leaves the vendor, and leaving has
to stay possible.
