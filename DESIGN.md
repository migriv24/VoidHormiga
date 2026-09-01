# Void Hormiga — Founding Design Document

> **Status: founding draft, 2026-07-15.** This is the pre-OKF design document —
> the one place allowed to reference the existing Hormiga project
> (`../Hormiga`, its `VOIDCORE_INTEGRATION.md`, `ANTFARM.md`, `FUTURES.md`)
> freely, because its job is to carry everything worth keeping across the
> boundary. When this document's concepts stabilize, they are rewritten into
> Void Hormiga's own OKF **as their own things** — the OKF will describe what
> Void Hormiga *is*, not what it replaced. This file then becomes a dated
> reference, like Void Maiz's archived draft.

One sentence: **Void Hormiga is a native C++20 desktop application, built on
Void Core (the engine) and Void Maiz (the view), for community-outreach
organizations to keep their people, events, images, and resources as tagged
runes — and to compose newsletters and static websites out of that data as
snapping content blocks — local-first, encrypted, and eventually collaborative.**

---

## 1. The decision this founds (and the two it reverses)

Old Hormiga's convergence plan (`../Hormiga/VOIDCORE_INTEGRATION.md`, v2)
explicitly decided **against** a new application (§0, "too expensive") and
**against** a C++ rewrite (§2, "keep Python"). Void Maiz's substrates concept
leaned toward Hormiga adopting Void Maiz **as a protocol** in the browser, not
as a binary. All three calls were correct *when made* — and are reversed now,
by the author, 2026-07-15, because the premise changed:

- **Void Maiz exists.** When those documents were written it didn't. Today it
  has a working canvas, gesture→command compilers, faces/widgets, undo
  surfacing, a shipped desktop demo (InteractionCombinators), and a shipped
  Android APK built with zero Java. "New application" no longer means "build a
  UI framework, then an app" — it means "build an app on a framework."
- **The Python-specific surface audited small** (§3 below). Nothing old
  Hormiga does is Python-bound; the big Python costs (PyInstaller freezing,
  Flask process management inside Electron) are costs *created by* the stack,
  and they simply vanish in a native binary.
- **The strangler migration's endgame was always ambiguous** ("rebuild
  builder-only, or the whole shell? decide after the dispatcher convergence").
  This document is that decision, made early: the whole shell, on Void Maiz.

What is deliberately **kept** from the convergence plan, because it was right:
the model mapping (block/event/contact = rune, newsletter = mantle, Antfarm
node = holiday, `.miga` v2 = holiday registry), the undo boundary (owned
mantle state undoable; holiday writes logged, not undoable), the compute
boundary, tag hygiene as `temper` passes, the fresh-public-repo posture (this
repo is public from day one; the old repo stays private as the archive), and
the working rule that **the repo root is a public artifact** — no member data,
no exports, no credentials beside the code, ever.

Old Hormiga does not stop: it keeps shipping the monthly newsletter until
Void Hormiga's builder exits its parity test (§10). And the **Supabase rescue
(old plan Phase 0, hard deadline 2026-08-02) remains prerequisite and urgent**
— the recovered dump is Void Hormiga's import source.

## 2. What the application is (the domain, restated without the old stack)

An outreach organization runs on five kinds of things:

- **People** — contacts, presenters, and their organizations. Public bio vs
  internal notes is a hard privacy line (§8).
- **Events** — meetings, presentations (1–N presenters each), with flyers and
  images attached in context.
- **Assets** — images and resource documents (PDFs), tagged at ingestion.
- **Documents** — the things composed *from* the above: monthly newsletters
  today; a public website next; both built from the same blocks (§5).
- **The organization itself** — its backends, credentials, collaborators, and
  trust topology: the **Antfarm** (§6).

Everything is a rune with a glyph, tagged on axes (`month:june`,
`type:event`, `status:active`, `lang:es`), living in mantles (the contacts
mantle, the June-2027 issue mantle, the website mantle, the antfarm mantle).
The dispatcher is the only way anything changes; the CLI, the GUI, and any
agent are three callers of the same verbs. That is the whole architecture,
inherited rather than built.

## 3. The C++ feasibility audit (what old Hormiga actually uses Python for)

Route-by-route and service-by-service, old Hormiga's Python reduces to:

| Old dependency | Role | Void Hormiga answer |
|---|---|---|
| Flask (~100 routes) | HTTP API for the browser UI | **Dies.** There is no server and no browser; routes collapse into dispatcher verbs + effects. The route list is still valuable — it is the verb inventory (§9). |
| Jinja2 section templates | newsletter HTML render | A vendored C++ template engine (inja is the candidate — header-only, Jinja-like syntax, so the 14 existing section templates port nearly verbatim) or a plain string renderer per glyph. Host compute either way. |
| Supabase REST / PostgREST | cloud DB | **Removed by design** (author's call). Local SQLite (§7); the rescued dump is a one-time import. |
| Google Sheets API | contacts/events source of truth | Demoted: an optional **import/output holiday** over plain REST + a service-account JWT (signable with vendored crypto). Sheets stops being a source of truth. |
| ImgBB API | image hosting | **Removed by design.** Assets live on the local FS holiday; a web-deploy copies what a published site needs. |
| Google Translate service | EN/ES translation | An optional cloud holiday over REST. Same seam as the Queen (§6) — and a local-model holiday can implement the same interface later. |
| PIL compositing (`compositor` plans) | banners/collages/PDF previews | stb_image + stb_image_write class single-file libs (grid/banner layouts are ~200 lines of blitting); PDF page rasterization is the one genuinely annoying piece — pdfium or "ship without PDF previews at v1" (open question §11). |
| PyInstaller + Electron + electron-updater | packaging, shell, updates | **The biggest win.** One native binary; GLFW window; own updater (§9). The splash screen, port juggling, health-poll/kill dance, and 200MB installer all evaporate. |
| cloudflared tunnel, server tab | ad-hoc remote access | Not carried. Superseded by real collaboration protocols (§8) when their time comes. |

Verdict: **100% writable in C++**, with the vendor-don't-depend policy intact —
every candidate above is a single-file or vendorable library, matching how
Void Maiz already vendors ImGui and GLFW. The things that were "free" in
Python (Sheets, translation) are exactly the things becoming *optional cloud
holidays*, so their cost is deferred and isolated.

## 4. The model (runes, mantles, domains — used hard)

### 4.1 Data mantles

`contact`, `event`, `organization`, `job`, `image`, `resource`, `note` glyphs.
Facets carry the six-facet story where natural (who = contact name, when =
event date…). Tags are axis-typed from day one — the namespace→axis map
(`month:`→when, `type:`→what, `status:`→state, `lang:`…) plus `temper`
normalization passes IS the tag-management panel old Hormiga wanted; drift
(`may`/`May`/`may2026`) is fixed by a registered idempotent rule, not a UI.

Relations that old Hormiga kept in a bespoke graph store (contact↔org,
image↔event, resource pairs, multi-presenter events) become what they always
were: **edges** — link relations between runes, rendered by Void Maiz as
wires when you want to see them (the Connections tab becomes an actual canvas
for free).

### 4.2 Document mantles and the block model

One newsletter issue = one mantle of **block runes**. One website page = one
mantle of block runes. The block glyph set is **shared**: `hero`,
`event_grid`, `image_grid`/`flyer_grid`, `narrative`, `presenter_cta`,
`job_grid`, `attendee_list`, `meeting_schedule`, `footer`, … (the existing 14
section templates are the seed inventory).

The builder is **Node Blocks applied**: block bodies with notch/tab geometry;
vertical document order is the hidden `prev/next` linguine chain — **snapping
a block into the stack IS the ordering gesture**; typed value sockets carry
connector-shape-as-type (an `events` socket accepts only an `events` query
block); layout containers (two-column, grouped sections) are C-block
containment via mantle-enter. Every block placement, reorder, and argument
edit is a logged, undoable, replayable dispatcher command.

**Query-backed blocks** are the killer mechanic, unchanged from the
convergence plan: an `event_grid` block's source is a tag expression
(`@month:june AND type:event`) resolved through the data holiday at render
time, and `materialize` is the explicit, undoable bake ("snapshot these six
events into the issue so it never changes under me").

### 4.3 Domains: one block graph, many outputs

This is where Void Core's **domain** concept earns its keep, and the
newsletter/website unification lives:

- **`email` domain** — table-layout HTML, inlined CSS, image constraints:
  what the existing renderer produces. Static cards.
- **`web` domain** — a modern static site: same `event_grid` renders as a
  responsive CSS grid, cards may carry JS interactivity (filter, expand,
  lightbox). Pages, nav, and theme are web-domain concerns.
- (later) **`print`/`pdf`** — same blocks, print stylesheet. Recorded, not planned.

Rendering is per **(glyph × domain)**: a renderer pack maps each block glyph
to output for that domain, invoked by an `effect render <mantle> <domain>`
walk of the chain. Blocks that make no sense in a domain declare it (an
`attendee_list` may be email-only). A **theme** is a renderer pack + assets —
which is how site themes ship without a page editor.

Precedent honored: **PortfolioManager** (`../PortfolioManager`) already runs
this pattern in miniature — portfolio projects as runes behind a
LocalJsonHoliday, tag-queried, deployed to GitHub Pages, where the content
file is itself the deploy artifact and saves merge-preserve unknown fields.
Mound (§5) is that pattern grown up: updater → builder.

## 5. Mound, absorbed: the website story

Old FUTURES.md ranked Mound "architecturally the most complex — do not start
until Antfarm is solid." The block/domain model collapses most of that
complexity: there is no separate site builder — **a site is a mantle of pages,
a page is a stack of blocks, a theme is a renderer pack, and deploy is an
output holiday** (GitHub Pages push, plain folder copy, rsync/SFTP later).
What remains genuinely new for `web` domain: page/nav structure, asset
copying, and the JS sprinkle shipped by the theme.

**Dogfood target, set now:** Void Hormiga's own website — the download page
with releases, docs, screenshots — is built and deployed *by Void Hormiga*.
It is the first site the `web` domain ships, it exercises the whole
pipeline with zero PII, and it gives the project the distribution surface §9
wants. Eating our own cooking is the exit test for Mound-absorbed.

## 6. The Antfarm: holidays as the visible nervous system

The Antfarm stops being a metaphor and becomes a **mantle you can open**: one
rune per configured holiday, protocol interfaces as typed ports
(connector-shape-as-type again — a `data` plug only fits a `data` socket),
live `describe()` status on each node's face, credentials in each node's
encrypted config. The old ANTFARM.md's node anatomy sketch was a Void Maiz
node all along; now it's rendered by one.

Holiday interfaces (the old protocol types, now Void Core holiday interfaces):

| Interface | v1 nodes | later nodes |
|---|---|---|
| **Data** | SQLite (embedded, default) | MeshDB (mesh sync), Postgres |
| **Asset** | Local FS (default) | S3-compatible, content-addressed store |
| **Output** | HTML export, folder deploy, GitHub Pages | Sheets sync, Courier (email dispatch), RSS, webhook |
| **Import** | Supabase-dump import, Sheets import, CSV | — |
| **LLM (the Queen)** | — (interface reserved) | local endpoint (Ollama-class), Claude API |
| **Geo (Territory)** | — (interface reserved) | the geo holiday Neighborhood grows |
| **Auth/Peer** | — (interface reserved) | LAN peer, login server, mesh (§8) |
| **Translate** | — or Google REST | local model |

**Holidays are used throughout the application, not just as backend config**:
translation is a holiday call from the builder, deploy is a holiday call from
the site view, the Queen is a holiday the Nest invokes, import wizards drive
import holidays. Utility wrappers from the old doc (Fallback, Cache, Logger)
remain good ideas *as holiday wrappers* — resilience as a graph property —
with Fallback first (it is the offline story's second half).

The **`.miga` v2 file is the holiday registry**: node topology + per-field
encrypted credentials + admin/roles, under a mandatory user passphrase (§8).
Topology-in-plaintext-with-encrypted-secrets is the lean (old open question,
now leaned): the graph is shareable and diffable; the keys never are. The old
`APP_SECRET` design is burned and stays burned — no hardcoded fallback
secret, ever.

## 7. Local-first data (and the SQLite/MeshDB call, revisited honestly)

**Default: embedded SQLite, vendored amalgamation, in-process.** The
convergence plan v2 chose MeshDB as primary — for a *Python host* where the
MeshDB holiday already existed and any embedded option still meant managing a
child process (meshdb-server) anyway. A native C++ app flips the trade:
SQLite links into the binary as one C file (the most deployed database on
earth, zero sidecar, zero lifecycle management, perfect fit for
vendor-don't-depend), while MeshDB from C++ means writing a Bolt client and
shipping a per-platform Rust server binary on day one.

The honest resolution — because the *reasons* for MeshDB were real:

- **v1: SQLite as the default Data holiday.** Runes, tags, and edges in a
  normalized schema (tags and links as join tables — the tag grammar compiles
  to SQL, and the graph-shaped queries old Hormiga's Connections tab needs are
  fine at 10³ rows). Full offline, single file, trivially backed up.
- **MeshDB remains the mesh-collaboration growth path**, behind the same Data
  interface, revisited when §8's peer-sync work actually starts — which is
  also when its distributed modes (the reason it was chosen) stop being
  speculative. The holiday seam makes this a swap, not a rewrite.
- **Snapshot fallback holiday regardless**: every clean session mirrors to a
  local JSON snapshot; a corrupted database opens the app read-only on the
  snapshot with a banner — no failure mode is a dead app (the Antfarm's
  founding principle, kept).

Assets: local FS holiday, content-hashed filenames, per-asset metadata runes
in the images/resources mantles. "Offline-first" stops being a feature with a
milestone — **it is the resting state of the architecture**; the network is
something you *add*.

## 8. Security: E2EE as a pillar, not a feature

Author's directive: end-to-end encryption is a MAJOR aspect of development,
especially for collaboration. Design consequences, ordered by when they bite:

1. **One crypto dependency: libsodium** (vendored, audited, misuse-resistant).
   No home-rolled primitives, no OpenSSL sprawl. argon2id for passphrase KDF,
   XChaCha20-Poly1305 secretstream for files/blobs, Ed25519 for signatures,
   X25519 for key agreement.
2. **At rest (v1):** the `.miga` v2 registry under a mandatory passphrase;
   credentials field-encrypted. The data store itself encryptable at the
   app level — content columns sealed with a master key derived at unlock —
   with "encrypt everything" as an org-level switch (an org holding sensitive
   internal notes flips it; the perf cost is honest and measured). OS keychain
   integration for convenience unlock is an open question, not an assumption.
3. **Privacy is a property of the holiday and the render rule, not the app.**
   Internal notes (the public-bio/internal-notes split, kept from old
   Hormiga's backlog) are a field that **no Output-interface holiday ever
   receives** — enforced at the render/export seam, testable, not a template
   convention. The same mechanism later expresses "this data may not leave
   the device" for Neighborhood-grade PII.
4. **Collaboration (later, but designed-for now):** different protocols for
   different work, all behind the Peer/Auth interface, all E2EE:
   - **LAN mode** — mDNS discovery on shared Wi-Fi, pairing via QR/short
     authentication string (Noise-protocol-style handshake), then encrypted
     sync of dispatcher logs/state between trusted devices. No server at all.
   - **Login mode** — a shared relay for distributed teams; the relay stores
     and forwards **ciphertext only** (the server is dumb by design — running
     one is not a position of power over the org's data).
   - **Mesh mode** — the MeshDB path, when multi-editor demand is real.
   The sync unit is the thing Void Core already gives us: the **command log**.
   Log-shipping between peers (with materialized snapshots for bootstrap) is
   the CRDT-adjacent starting point; conflict UX is a declared hard problem,
   localized behind the seam, not solved on paper here.
5. **Trust topology lives in `.miga` v2**: admin builds the Antfarm and invites
   peers; members hold keys, not passwords-to-a-cloud. Key loss = data loss is
   a real trade — recovery codes at org creation are the mitigation lean.
6. **Releases are signed** (Ed25519; §9) and the updater verifies before it
   swaps anything.

## 9. The application itself: shape, distribution, updates

- **Repo pattern:** a sibling host app, exactly like InteractionCombinators —
  consumes `voidcore` (vendored prebuilt DLL or compiled-from-C11-source, as
  Void Maiz already does per-platform) and `voidmaiz`/`voidmaiz_view`. CMake +
  Ninja, vendored deps only. Public from day one; MIT, matching the family.
- **App/platform split from birth:** platform-free `app.cpp` + a desktop
  shell (GLFW), the split the IC APK proved. Nobody is promising a tablet
  build; the split costs nothing now and keeps the door open.
- **The UI at v1:** a workspace of panes — data tables (contacts/events/
  images/resources), the block-builder canvas, the Antfarm canvas, inspector,
  log strip, command bar. The command bar matters doubly here: **the CLI is
  inside the app**, and an agent driving `voidhormiga` verbs from a terminal
  sees the exact same dispatcher. Verb inventory grows from old Hormiga's
  route list — every `/api/...` route either becomes a verb, an effect, or
  dies with the server.
- **Distribution:** GitHub Releases first (CI builds, conformance + replay
  tests, signed artifacts). Then the dogfooded website (§5) with a download
  button — the site that Void Hormiga itself deploys.
- **Self-updater** (the one Electron amenity worth rebuilding, ~simply):
  check the releases feed, download the new binary + signature, verify
  Ed25519, stage, swap on next launch, keep N-1 as rollback. No silent
  updates; the update is a visible, logged event like everything else.
- **Windows first** (the author's platform and the org's), macOS/Linux as CI
  targets once the app has users there.

## 10. Phasing (concept → spine → data → builder → publish → collaborate)

Gated by exit tests, not calendar. Old Hormiga ships newsletters until D exits.

- **A. Founding (now):** this document; relay `MESSAGE_FOR_VOIDMAIZ.md`; the
  OKF skeleton (index, concepts for the block/domain model, the Antfarm, the
  security posture — written as their own things, per the founding rule).
  **Prerequisite, unchanged, urgent: the Supabase rescue before 2026-08-02**
  (runs in the old repo; produces the import dump this project consumes).
- **B. The skeleton app:** window + core + canvas; a toy mantle rendered and
  editable; command bar; replay smoke test in CI (the Void Maiz exit-test
  shape, inherited). *Exit: a transcript built in the GUI replays headless to
  an identical state document.*
- **C. Data spine:** SQLite Data holiday; import holidays (Supabase dump,
  Sheets, CSV) land the real org data; table view (or host tables, per the
  Void Maiz reply); tag axes + temper hygiene; asset FS holiday + image
  ingestion with tag suggestion. *Exit: the whole org database lives locally,
  fully tagged, queryable by one grammar from CLI and GUI; Supabase and ImgBB
  are not configured anywhere.*
- **D. The builder (rides Node Blocks Phase A):** block glyphs + email-domain
  renderer pack (porting the 14 section templates); snap-stacked issue
  building with query-backed blocks; materialize; export. *Exit: this month's
  real newsletter is built in Void Hormiga, snapping blocks, and the rendered
  HTML passes the same eyeballs the old app's output does. Old Hormiga
  retires from newsletter duty.*
- **E. Publish:** `web` domain renderer pack + theme #1; site mantle + pages;
  deploy holidays (folder, GitHub Pages); **the project's own website ships
  from inside the app**; the signed self-updater. *Exit: a stranger downloads
  Void Hormiga from a page Void Hormiga deployed, and it updates itself.*
- **F. Collaborate:** `.miga` v2 sharing, LAN peer sync E2EE, roles.
  *Exit: two laptops on one Wi-Fi edit the same org, offline-tolerant, with
  the relay/mesh decision then made on evidence.*
- **Continuous:** the Antfarm canvas grows from a card list (v0, phase C) to
  the full node editor (phase E-ish); the Queen/Nest/Territory/Courier arrive
  as holidays on the finished spine, in whatever order real use pulls them —
  they are FUTURES no longer entangled with the architecture.

## 11. Open questions (with leans, OKF-style)

- **Name & identity:** repo `VoidHormiga`; does the user-facing app stay
  "Hormiga" (a version-2 of the brand) or ship as "Void Hormiga"? *Lean:
  users see "Hormiga"; "Void" is the family prefix for the stack's repos.*
- **Schemas:** carry old field shapes (contact/event dataclasses) or redesign?
  *Lean: redesign lightly at import time — this is the once-ever chance to fix
  field debt (bio vs internal notes born separate, presenters 1–N native).*
- **PDF previews** (resources tab): pdfium vendored, or v1 ships without and
  resources render as typed cards? *Lean: without, revisit on demand.*
- **Sheets:** import-only, or ongoing two-way sync holiday? *Lean:
  import-only until a real workflow demands sync; the sheet was a database of
  convenience, and the database now exists.*
- **Bilingual model:** EN/ES as parallel content fields per block (translated
  by holiday call, hand-editable) vs render-time translation like today.
  *Lean: parallel fields — translation becomes content you can fix, not a
  render side-effect you can't.*
- **Whole-store encryption default:** on or off for a fresh org? *Lean: off by
  default, one switch, loudly offered at org creation; internal-notes fields
  encrypted regardless.*
- **Core linkage:** prebuilt `libvoidcore` per platform vs compile-from-source
  into the app (the APK did the latter). *Lean: follow Void Maiz's per-target
  pattern; don't invent a third.*
- **The verb surface:** how much of the CLI is Void Core verbs vs
  `effect`-routed app verbs? Resolve while building phase C — the route
  inventory is the checklist.

## 12. What this project is NOT

- Not a web app, not an Electron app, not a server. No browser anywhere in
  the runtime (the system browser is used for HTML preview — as a viewer).
- Not a Supabase/ImgBB/cloud-BaaS client. Cloud things are optional holidays
  an admin adds with eyes open.
- Not a CMS with a page editor. Themes are renderer packs; the editing
  surface is blocks.
- Not an upstream editor: Void Core and Void Maiz gaps go through their
  message files; this repo never patches them.
- Not a rewrite that stops the presses: the old app runs until phase D's exit
  test says otherwise.
