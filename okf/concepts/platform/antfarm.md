---
type: Concept
title: The Antfarm
description: "The org's backends as a mantle you can open — a TYPED DATAFLOW GRAPH: ports typed by PAYLOAD (records / assets / site), publishing as a pipeline (core → publisher → server/deployer), locality (local vs cloud) badged. The .miga v2 encrypted registry; local-first defaults; live status faces."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

> **Redesigned 2026-08-05 (Q25) — a typed dataflow graph.** The old model typed
> ports by a coarse **input/output bucket** (Data/Asset/Output/Import), which
> lumped unrelated agents onto one socket — Supabase + Sheets both on "import";
> the HTML site-generator + the ImgBB image-host both on "output." Now **ports
> are typed by the PAYLOAD that flows** — **`records`** (the runes), **`assets`**
> (blobs), **`site`** (a rendered publication) — so incompatible agents can't be
> confused, and **publishing is a PIPELINE** (`core → HTML publisher → server /
> deployer`), not siblings on one socket. **Locality** (local vs cloud) is a
> first-class badge, and the shipped default is **local-only** (SQLite store, CSV
> source, local files, HTML publisher → localhost server). Details in §"The
> payload-typed model" below; the old bucket framing in this doc is superseded.

The **Antfarm** is the organization's nervous system made visible: a **mantle
you can open**, with one rune per configured holiday, protocol interfaces as
**payload-typed ports** (a `records` plug only fits a `records` port), live
`describe()` status on each node's face, and credentials in each node's
encrypted config. Configuring a backend and *seeing* your backends are the same
canvas.

# The payload-typed model (Q25, 2026-08-05)

Three payloads flow across the I/O boundary, and every agent handles one:

- **records** — the org's structured data (runes). **Stores** persist them
  (SQLite = local, Supabase = cloud); **sources** import them (CSV = local,
  Sheets = cloud). All plug into **core.records**.
- **assets** — blobs (images, files). A local **file store** or a cloud **image
  host** (ImgBB). Both plug into **core.assets**.
- **site** — a rendered publication. The **HTML publisher** is the pipeline head:
  it *consumes* `records` + `assets` and *emits* a `site`. A **server**
  (localhost) or **deployer** (GitHub Pages) then *consumes* that `site`. So
  `out-html → out-localhost` is a real **chain**, and a site-generator is
  categorically distinct from an image-host — the two critiques that motivated
  the redesign both dissolve.

**`core`** is no longer a featureless 4-socket hub; it exposes exactly the two
payloads it owns (**records**, **assets**), and the publish pipeline hangs off
them. **Locality** is badged (local = warm colors, cloud = teal), and **local
agents are the default** — the Antfarm only earns its node-graph when you wire in
external services. The canvas (Void Maiz `edit_canvas`) enforces the payload
types for free: a `site` plug will not fit a `records` port.

# The Antfarm's role: Hormiga's interface/protocol DSL (the I/O boundary)

Framed alongside Hormiga's other DSLs ([the three DSLs](/concepts/foundation/dsls.md),
2026-08-04): **the Antfarm is the INTERFACE / PROTOCOL DSL.** Its job is
**port-mapping** — giving Hormiga **one consistent way to talk to arbitrary
external things** (cloud, local servers, spreadsheets) by mapping each to a
**payload-typed interface** (records / assets / site; and future LLM / Geo /
Auth / Translate as their own payloads). It **defines the language of I/O**; it
runs no logic. Its natural
surface is precisely this **node graph** (I/O is dataflow), which is why the
canvas is right and why a text form for it would be a mere convenience, not a
rewrite. In interaction-net terms it is the **I/O boundary** — source holidays
are *input agents*, output holidays are *output agents* — deliberately kept
separate from the pure rune/tag core
([allomone/foundations](/concepts/allomone/foundations.md)).

**The Allomone seam (future).** [Allomone](/concepts/allomone/index.md) (the
LOGIC DSL) is the consumer that will make the Antfarm's consistency pay off:
scripts will **extract data from any source** and **export to any output** by
*calling a holiday* through its typed interface — the "Allomone invokes a
holiday" seam. Two consequences worth stating now, so we don't mis-scope:

- **Styling the app needs NO holiday.** The Hormiga application is the **interior
  render target**, part of Hormiga, not a boundary agent. Allomone coloring a
  contact card is a **default** operation — nothing crosses the Antfarm. Only
  reaching an *external* output (website, newsletter, cloud, spreadsheet) is a
  boundary crossing through a holiday.
- **Status, honestly.** The *vision* above is right and matches the design; the
  *implementation* is early. Real holidays work (SQLite Data, FS Assets, ImgBB,
  the Supabase import); but most nodes are status cards, the uniform "any
  external ↔ a fixed typed interface" mapping is partial, and the
  **program-callable holiday seam Allomone needs does not exist yet**. That is
  acceptable: Allomone's current phase (styling the app) is interior and needs
  none of it — so the Antfarm's interface build is **co-developed with Allomone's
  I/O phase, not before it** ([the three DSLs](/concepts/foundation/dsls.md) §sequencing).

# The graph, concretely (v1 — live since 2026-07-19)

The `antfarm` mantle ships wired in every fresh org: one **core hub** rune
(the consumer — the predecessor's ANTFARM.md drew exactly this) whose out
sockets are the protocol interfaces, and one rune per holiday provider with
a single typed plug. **Wiring IS configuration**: the edge from `core.data`
to a provider is "this is where the org's data lives"; connector-shape-as-
type means a `data` plug only fits a `data` socket. Node faces are live
`describe()` views (the SQLite node shows rune counts and last save; the
Supabase node shows whether the rescue dump is present and carries the
**Import now** button — a face widget that compiles `effect import-rescue`
like any gesture). Secrets are NEVER node fields (fields are exported
state): nodes show key *presence*; keys live in gitignored files until
`.miga` v2.

The predecessor's components, translated:

| old component | Antfarm node | interface |
|---|---|---|
| SQLite / Supabase / Postgres repositories | `hol_sqlite` (default; others addable) | Data |
| local FS + ImgBB image store | `hol_fs_assets` (default), `hol_imgbb` (opt-in Output) | Asset / Output |
| newsletter HTML render | `hol_html` | Output |
| **local web host** (website as a hosted SITE, not a file) | `hol_localhost` — serves `site/` clean on 127.0.0.1:8780, a real-domain stand-in (2026-07-23) | Output |
| Supabase REST sync | `hol_supabase` — now an **Import** node over the copy-only rescue dump | Import |
| Google Sheets repositories | `hol_sheets` (unconfigured; import-only lean, Q4) | Import |
| CSV/spreadsheet entry | `hol_csv` (live via the Data toolbar) | Import |

The Supabase node's Import now button runs BOTH rescue passes behind
independent config-tier guards (`import.rescue_done`,
`import.rescue_media_done`) — clean tables (contacts/orgs/events/presenters)
first, then `json_store`'s freeform payloads (connections, the image
library, the jobs board, per-row tags) — each its own `compile_commit`
batch, each checked for `Result::ok` before its guard is set (a rolled-back
batch must never be mistaken for a completed one; see log 2026-07-20).

# Holiday interfaces

> **Renamed 2026-08-12 (Q38 — answered): the LLM interface is `model`, not "the
> Queen."** The cute name collided with the new sibling project **Void Reyna**
> (*reyna* = queen), and "Reyna talks to the Queen" is a sentence worth not
> having to explain. The author's call was to rename the interface, on the
> grounds that the project name carries an argument while the interface name was
> a placeholder with no client — *and* that LLM integration is likely to become a
> Void Maiz concern with its own name, the way Allomone did. So Hormiga's side of
> it should be **the plain payload word**, matching `records` / `assets` / `site`.
> Naming a placeholder is what caused the collision; the fix is to stop.

| Interface | v1 nodes | later nodes |
|---|---|---|
| **Data** | SQLite (embedded, default) | MeshDB (mesh sync), Postgres |
| **Asset** | Local FS (default) | self-hosted static host (org domain), content-addressed store |
| **Output** | HTML export, folder deploy, GitHub Pages | self-hosted deploy (rsync/SFTP/S3-class, org-owned), Sheets 2-way sync, Courier (email dispatch), RSS, webhook |
| **Import** | Supabase-dump import, Sheets import, CSV | — |
| **`model`** (was "the Queen") | — (interface reserved) | local endpoint (Ollama-class), Claude API |
| **Geo (Territory)** | — (interface reserved) | a geo holiday, when its client exists |
| **Auth/Peer** | — (interface reserved) | LAN peer, login server, mesh |
| **Translate** | — (engine present, offline-degraded) | Google REST, local model |

**Holidays are used throughout the application, not just as backend config**:
translation is a holiday call from the builder, deploy is a holiday call from
the site view, a `model` holiday is what an assistant surface invokes, import
wizards drive Import holidays. Utility wrappers — **Fallback** first (it is
the offline story's second half), then Cache and Logger — are holiday
wrappers: resilience as a graph property.

Cloud is opt-in by construction: every non-local node is something an admin
adds with eyes open. The default Antfarm is entirely local — and **v1 ships
entirely local** (author, 2026-07-16): most of Hormiga's features must work
with no internet connection at all. Online holidays (image hosting, Sheets,
email dispatch) are the exception, never the spine.

# Defaults, not assembly

The author's charge (2026-07-16, answering Q5 and generalizing it): the
Antfarm is where the org's behavior is **seen and reconfigured, not where it
must be built**. A fresh org comes with a working colony — SQLite data, FS
assets, snapshot fallback, the bilingual engine, render/export — already
wired. Nobody sets up nodes to get a bilingual newsletter; the app ships
configured and the canvas exists so you can *understand* and *change* the
configuration, not so you can bootstrap it. Features that need a holiday
ship with their default holiday in place.

# The self-hosted horizon (the org's own BaaS)

The direction that motivated the Antfarm in the first place, made explicit
(author, 2026-07-16): **no subscription services**. Where going online is
genuinely needed — newsletter images a mail client can load, a published
website, email dispatch — the preference is the org running its own
infrastructure with tools Hormiga provides, on a domain the org purchases
(domain registration stays an external purchase; everything after it should
not be). One self-hosted static host serves BOTH needs: website deploy and
newsletter asset URLs — the same infrastructure, one holiday.

The seam that makes this tractable: **asset URL resolution is a holiday
call at the render seam** ([developer questions](/developer_questions.md)
Q9) — export-with-embedded-images, self-hosted URLs, and (opt-in)
third-party URLs are just three resolvers, and the block model never knows
the difference. "Hormiga itself as the serving process" is a recorded
horizon, not a plan — it would reopen the no-server-in-the-runtime boundary
and must be argued for on evidence (Q10).

# Publish & mirror: what cloud↔local means here

Two effects, twins, both logged like every world-facing op:

- **Publish (local → cloud)**: a rune's LOCAL bytes (already in `assets/` —
  local-first means the file exists at home before it can leave) are handed
  to an Output holiday; the returned public URL lands as ONE `set` command.
  Model change, undoable.
- **Mirror (cloud → local)**: walk every rune field referencing a remote
  URL and make the bytes local by the cheapest honest route — mirror cache
  hit → settled; the rescue's media/ has it → copy in; else download.
  Results land in **two tiers, deliberately**:
  1. bytes in content-hashed `assets/` plus `assets/mirror.json`
     (url → local path) — **holiday cache**, reconstructible by re-running
     the effect, and therefore NOT model truth;
  2. an image rune whose bytes just became local gets ONE `set path`
     command — that IS model truth (the org now owns a local copy), logged
     and undoable. Dead URLs are recorded as `""`: evidence, never
     re-hammered.

The invariant the pair maintains: **every cloud host is disposable.** The
model's URLs are content (imported facts); the local mirror makes losing
the host a non-event — the exact failure the rescue documented (a hundred
images already dead on a free CDN) structurally cannot recur for anything
mirrored. The render seam (Q9) later prefers local bytes and treats the
URL as one resolver among several.

# The `.miga` is the database's DOOR, not its warehouse (author, 2026-07-22)

The author's clarification, resolving Q17's direction: **"create new
database" creates a `.miga` file, and what the `.miga` saves is the
Antfarm** — the node topology (+ encrypted credentials). The data itself
(runes, maps, newsletter/website blocks, media) lives *behind the nodes*,
wherever they point — today the local SQLite + FS nodes, tomorrow possibly
cloud nodes. Opening a `.miga` = wiring up to the org. Consequences:

- **One database, many artifacts**: multiple maps, multiple newsletters,
  multiple websites, (later) multiple users — all belong to one database,
  reachable through its nodes. "Where is X saved?" is always answered by
  the graph, never hardcoded.
- **Same database, different topologies.** Two people share a `.miga`; one
  rearranges nodes (videos local for me, cloud for you) — and where a node
  is SHARED (the same cloud contacts store), the two setups update each
  other. The topology is personal; the shared nodes are the collaboration.
- **Read vs write nodes.** A node's edge should declare whether the host
  merely *receives* data through it or *modifies* the backing store — the
  distinction that makes shared-node collaboration reasoned-about (and that
  roles/permissions later hang off). Recorded as design (→ Q19); "sandbox /
  simulate" modes are noted-for-later, not planned.
- **A fresh install ships a demo database** — fake people, everything
  local — so the app is explorable before any real org exists.

Today's static site render is the degenerate case ("writing data out to a
local website"); the same graph is meant to grow into a live setup.

# The `.miga` v2 file: the holiday registry

One file holds the org: node topology + per-field encrypted credentials +
admin/roles, under a **mandatory user passphrase**
(see [security](/concepts/platform/security.md)). The lean, adopted:
**topology in plaintext, secrets encrypted** — the graph is shareable and
diffable; the keys never are. There is no fallback secret of any kind
baked into the binary; no passphrase, no unlock.

# Local-first storage (the default nodes, and why)

- **Data: embedded SQLite** — vendored amalgamation, in-process, zero sidecar.
  Runes, tags, and edges in a normalized schema (tags and links as join
  tables), so the one tag grammar compiles to SQL and graph-shaped queries
  are fine at the target scale (see [data model](/concepts/foundation/data-model.md)).
  Full offline, single file, trivially backed up.
- **MeshDB stays the mesh-collaboration growth path** behind the same Data
  interface — revisited when peer-sync work actually starts, which is when
  its distributed modes stop being speculative. The holiday seam makes this
  a swap, not a rewrite.
- **Snapshot fallback regardless**: every clean session mirrors to a local
  JSON snapshot; a corrupted database opens the app read-only on the snapshot
  with a banner. **No failure mode is a dead app** — the Antfarm's founding
  principle.
- **Assets: local FS holiday** — content-hashed filenames; per-asset metadata
  runes in the images/resources mantles; a web deploy copies what a published
  site needs.

"Offline-first" is not a milestone; it is the resting state of the
architecture. The network is something you add — as a node in this mantle.

# A holiday as a function call: "host it online" (2026-09-15)

The author: *"a button to automatically do something like 'host it online'
should have its protocols be called upon via the antfarm. Because then, it
should return with a link. essentially its kinda like a function call, where we
expect a link to be given in return."*

This is the Antfarm's first **capability**: a question with a typed answer that
more than one kind of node can give. The rest of the application asks the
question and never names a vendor.

| | |
|---|---|
| the call | a local file in, a public link out (`HostedLink`: `url`, `node`, `after_publish`, or `error`) |
| who can answer | `domain/hosting.hpp`, one row per protocol: `hol_imgbb`, `hol_object_store`, `hol_static_host`, `hol_github` |
| who answers | `config hosting.images` names a node; blank means the first that can answer now, with hosts whose link works at once ahead of hosts whose link waits for a publish |
| can it answer | `host_problem(node)`: an empty string, or the one thing it is missing |
| the implementations | `HormigaApp::host_online` in `publish/push.cpp`, one branch per row |

**`after_publish` is part of the answer.** A website host copies the file into
`site/assets/` and answers at once with `site.base_url/assets/<file>`. That
link is real but works only after the next publish. Saying so is the difference
between hosting on your own domain and a newsletter full of broken images.

**Every surface reads the same table:**
- the CLI (`effect host-online`);
- the console;
- the image editor's "online / not online yet" line and its Host it online
  button;
- the Data tab;
- the Antfarm faces, which show whether a node can host images now, with a "Use
  for images" button;
- the inspector's "Hosting images online" panel: the choice, what the chosen node
  does and needs, the count of images online versus not, and one button for the
  rest.

**A new host is one table row plus one branch.** Nothing else changes. So are the
next capabilities of this shape. "Send this email" and "shorten this link" should
each be a table and a call like this one, not a new vendor button.
**The larger Antfarm overhaul** should treat capabilities as declared node ports
rather than a table in a header. The table is the honest first version: it is
data, the GUI and CLI already consume it through one function, and moving it
into glyph declarations later changes where the rows live, not who asks.

# Growth shape

The Antfarm view starts as a card list (roadmap phase C) and grows into the
full node editor (phase E-ish). The reserved interfaces (`model`, Territory,
Auth/Peer) arrive as holidays on the finished spine, in whatever order real
use pulls them — futures, no longer entangled with the architecture.

# The screen says it is not ready (2026-09-15)

The author, running 0.1.2: *"antfarm in general should have like a little warning
in the GUI that it's not really ready for human users yet. Sure we have the node
graph, but i'll be honest, it does NOT work ... the bones of the antfarm works,
and it can currently be driven by agents in a fine enough way. it's mostly the
UI/UX of the antfarm is horrible to a point where it might be unusable."*

So the Antfarm tab opens with a warning that says exactly which half is which:
the node graph's editing surface misbehaves, while what it configures - every
publish, upload and import in the application - works and is driven from the
command line and from the panels. The tab is also two windows now (Node graph,
Inspector), which is the [workspace](/concepts/sections/workspace-and-sections.md)
change rather than the redesign.

**The redesign is its own piece of work**, and the author has said it will come
with detailed direction. Until then the warning is what keeps the screen honest,
and the capability table (above) is the shape the rest of it should follow.
