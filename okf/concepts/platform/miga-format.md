---
type: Concept
title: The .miga v3 database bundle — the whole org, portable
description: "The .miga file redefined (v3): not a credential vault but the ENTIRE database — every mantle (data, all documents, maps, calendars, the Antfarm topology), the irreplaceable local assets, and the encrypted secrets — packaged as one portable, switchable, backup-able file. Working-copy vs bundle (a git-shaped model); re-derivable vs irreplaceable data; online/offline node protocols; backups; LAN/P2P sharing (future). Fully specified because it is the most important file the app owns."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-23T00:00:00Z
---

**A `.miga` file is a whole DATABASE** (author, 2026-07-23): all-encompassing —
every project and save (maps, calendars, documents), the Antfarm topology, the
org's data, its assets, its secrets. You switch between `.miga` files the way
you switch between projects. This supersedes **v2** (a credentials-only vault),
which is now **LEGACY**. v3 is a new format; the app reads v2 for migration and
writes v3 going forward.

# The mental model: a bundle and its working copy (git-shaped)

The author's own framing: *"saving is basically a commit; sharing is a push."*
So the architecture is git-shaped:

- **The working copy** = `base_dir` — the live, fast, local storage the running
  app uses: the SQLite `.db` (all mantles), `assets/` (images), `tiles/`
  (cache), `site/` (last render), `documents/`, `templates/`. This is the
  *working tree* — always concrete, always local, mutated every edit.
- **The bundle** = the `.miga` file — a packaged snapshot of everything
  IRREPLACEABLE in the working copy, plus the protocols to rebuild what isn't.
  This is the *repository/bundle* — portable, shareable, versioned.

**Save = pack the working copy into the bundle** (a commit). **Open = unpack a
bundle into the working copy** (a checkout) and reload the core. Two `.miga`
files are two independent databases: each carries its OWN local SQL data, its
OWN assets, its OWN Antfarm — opening one replaces the working copy with its
contents. (A local-hosted SQL node in database A and one in database B are
genuinely different stores; the bundle is why.)

# What the bundle stores — irreplaceable vs re-derivable

The author's key insight: the `.miga` stores **protocols, not just data** — and
some things are reconstructable from protocols rather than bundled. The line:

- **IRREPLACEABLE — bundled verbatim** (losing it loses work):
  - **The core STATE** — every mantle: `demo-org` (data), every document mantle,
    the `antfarm` topology, map view runes, `calview` runes. One
    `export_state()` captures all of it.
  - **Local ASSETS** — `assets/` images, and any user files a node references by
    path. These are user data; they travel in the bundle (base64), large or not.
  - **The SECRETS** — the encrypted credential section (the v2 vault, folded in).
- **RE-DERIVABLE — NOT bundled, rebuilt from protocols** (a protocol is a
  recipe, usually an Antfarm node):
  - **`tiles/`** — re-fetched from the map SOURCE node (OSM/CARTO). The protocol
    is "fetch from this tile URL"; the cache rebuilds on demand.
  - **`site/`, `exports/`, `preview-*.html`** — re-rendered from the documents
    + render packs. The protocol is "render these documents."
  - A manifest lists what to rebuild and how (which node re-derives it), so a
    freshly-opened database repopulates its caches lazily.

**The Antfarm topology IS the protocol layer.** It already lives in the state
(the `antfarm` mantle: nodes + typed edges). A local-SQL node says "the data
lives in a local db"; an ImgBB node says "images publish to this cloud, keyed
by a vault secret"; a tile-source node says "fetch tiles here." Reconstructing
a database = replaying the topology against the bundled state + assets, then
letting each node rebuild its re-derivable outputs. No separate protocol format
is invented — the graph is the protocol.

# The on-disk format (v3)

A single file, a JSON envelope (large is fine — the author accepts big files
for a format that "actually stores everything"):

```
{ "magic":"MIGA", "version":3,
  "meta":   { "name","created","saved","app":"hormiga","generator" },
  "state":  { …full export_state(): every mantle… },
  "assets": { "assets/flyer.png":"<base64>", … },   // irreplaceable local files
  "rebuild":{ "tiles":{"source":"…"}, "site":{"from":"documents"} }, // recipes
  "secrets":{ …v2 vault envelope (argon2id + XChaCha20-Poly1305)… | null } }
```

**Encryption (E2EE pillar).** v3.0 encrypts the **secrets** section exactly as
the v2 vault did (passphrase → argon2id key → sealed). **Whole-bundle
encryption** (state + assets sealed too) is the E2EE end state and a first-class
option: `Save As` may take a passphrase that seals the ENTIRE envelope (magic
stays readable; everything after is ciphertext), and `Open` prompts for it. A
database is `Neighborhood-grade PII` ([security](/concepts/platform/security.md)); an
encrypted bundle is safe to hand to anyone. Crypto is libsodium only, never a
fallback secret. *(v3.0 ships secrets-encrypted; whole-bundle-encrypted is the
documented next increment, not a redesign.)*

# Online vs offline (loading without wifi)

The author's requirement: a database must load even without a network. Because
the bundle carries all IRREPLACEABLE data locally, **an offline open always
succeeds** — the org, its documents, its maps' markers, its calendars are all
present. What degrades is only the RE-DERIVABLE-from-cloud and the
network-dependent nodes:

- On open, the app walks the Antfarm and classifies each node **online** or
  **offline** (a node property). Offline nodes (local SQL, local FS assets,
  local host) work unconditionally.
- Online nodes (ImgBB publish, a future cloud sync) are marked **unavailable**
  when there's no connectivity or no unlocked secret — surfaced as a node
  status, never a crash. Their re-derivable outputs (e.g. published image URLs)
  fall back to local paths at the render seam (the resolver-seam discipline,
  [Q9](/developer_questions.md)).
- The bundle records each node's online/offline requirement so a viewer can say,
  before doing anything, "this database uses 2 cloud nodes; you're offline —
  local features work, publishing is paused."

# Backups (author #5)

Forgetting to back up must not lose work.

- **On every Save**, the previous bundle is copied to `backups/<name>-<stamp>.miga`
  before the new one is written (write-temp-rename for the primary, copy-aside
  for the backup) — so a bad save never destroys the last good bundle.
- **Retention**: keep the last N (default ~10) per database; prune older. A
  setting controls N and whether backups are on.
- Backups are ordinary `.miga` files — "Open database" opens one, so recovery is
  just opening yesterday's bundle. `backups/` is gitignored.
- This is the poor-man's history until the commit-graph (below) lands; each
  backup is effectively a past commit.

# Sharing a database (author #4 — designed now, built later)

"Share database" opens a window to find devices on the LAN and share the
database. This is genuinely hard and genuinely important, so it's **documented
now, an empty 'in development' window in the UI, built later** — it needs a
sync model, and that reopens the no-server-in-the-runtime boundary.

**The git question, honestly.** The author is right that Save≈commit and
Share≈push, and right to worry that "our own git server" implies running a
server. The options, and what to learn from each:

- **Dolt** — *"git for data"*: a SQL database with commits, branches, merges,
  push/pull. The closest match to what a shared `.miga` wants (structured data
  with history + merge). Study its merge model; too heavy to vendor whole.
- **Fossil** — a self-contained DVCS (single binary, SQLite-backed, built-in
  server + web UI). Proof that "distributed version control with a friendly
  server" fits in one embeddable, dependency-light artifact. The closest
  *architectural* precedent for a Hormiga-native store.
- **Syncthing** — continuous **P2P file sync, no central server**, LAN
  discovery built in. The likeliest answer for LAN sharing WITHOUT building a
  git server: two devices with the same database converge. Study its discovery
  + block-exchange.
- **Automerge / Yjs (CRDTs)** — conflict-free merge of concurrent edits without
  a coordinator; the right primitive if two people edit the same database
  offline and reconcile. Heavier conceptually; the eventual answer to "shared
  nodes update each other" ([Q19](/developer_questions.md)).
- **git-annex / Git LFS** — how to version-control large binary assets (our
  images) alongside a small history. Relevant because bundles are large.

**Lean (recorded, not committed):** the shareable database is a **content-
addressed store + a commit log** (git's core idea) with **P2P/LAN transport
(Syncthing-shaped), no central server**; CRDT merge for concurrent edits is the
end state. Sharing is push/pull over that. This is the biggest future build and
warrants its own concept when it starts; the empty window holds its place.

**Escalated to an upstream proposal (2026-07-23):** the author's insight —
this VCS+sync layer is not Hormiga's to own, it is **Void Core's**, because
Core's command-log IS a patch history, its deterministic replay makes merge
sound, and interaction-net *confluence* gives a principled conflict-free merge
that generic VCS can't assume. Drafted as
`MESSAGE_FOR_VOIDCORE_hormiga-versioning-and-sync-2026-07-23.md` (Part 2):
formal commits + content-addressing over the existing log, patch-algebra merge
(Pijul-shaped, not git-snapshot-shaped), a device-agnostic have/want sync
(IoT↔laptop↔server), all CLI-native. Hormiga volunteers as the forcing client;
Core owns the generalization. Until it exists, the `.miga` snapshot + backups
here is the local stand-in.

# The commit-graph future (the git backend, user-friendly)

v3.0 Save is a **snapshot** (full state overwrites the bundle; backups are the
history). The direction: Save becomes a **commit** appending to a content-
addressed log inside the bundle (unchanged assets dedupe by hash — the reason
"large files now, fine" is acceptable: dedup comes with the commit graph). The
UI never says "commit/rebase/merge" — it says Save, Backups (history), Share
(push/pull). User-friendly on top of a git-shaped backend, the Fossil lesson.

# Operations (the File menu) — BUILT 2026-07-23/24

- **New database** *(2026-07-24)* — a fresh, empty database: a new working copy
  with empty org data, one blank newsletter document (`masthead` only), and the
  Antfarm topology (the protocol layer). No bundle yet (Save As names & places
  it). The multi-database "new project" entry the app was missing.
- **Save database** — pack the working copy into the current `.miga` (backing up
  the old one first). **No bundle yet → behaves as Save As** (author #1).
  `Ctrl+Shift+S`.
- **Save database as** *(a real save DIALOG, 2026-07-24)* — the OS "save file"
  picker (`on_save_file` seam → `GetSaveFileNameA`, `.miga` filter, overwrite
  prompt) so the author **chooses WHERE** the `.miga` lands, not just its name.
  Bundles the full state + all `assets/`; becomes the current database.
- **Open database** — the OS open picker (`.miga`), unpack into the working
  copy, reload the core. Legacy v2 (creds-only) is detected and refused for now.
- **Share database** — the empty "in development" window (above).
- **Backups** — timestamped copies on Save; "Open database" recovers one.

**Working-copy ISOLATION (the subtle correctness fix, 2026-07-24).** The
working copy has ONE `assets/`, `tiles/`, `site/` — so New and Open must RESET
it or one database's images bleed into the next. `reset_working_copy(clear_assets)`
drops the re-derivable caches always, and `assets/` on a database SWITCH (New
clears it; Open clears it before the bundle extracts its own), and invalidates
the texture cache so freed image paths can't return stale GPU textures.
Verified: New bundles 0 assets (was 76); Open restores exactly the bundle's own.

# The abstraction seam — Void Palabra (author 2026-07-24)

A separate library, **Void Palabra** (`../VoidPalabra`), is in development and
may provide the versioning / save-load / project-store machinery this concept
sketches — the real answer to the upstream VCS proposal.

**Measured 2026-08-21 (was: "it is not ready", untested).** Palabra's `archive`
builds, passes its 7 suites, links alongside Void Maiz with no cJSON collision,
and holds its numbers at our shape: **21 versions of a 456 KB document kept in
464 KB, against 9.6 MB for a `.miga` per version**; a 400 KB asset edited by one
byte costs +64 KB. `import_miga` already reads our v3 bundle.

**The one blocker was reported and is fixed** (same day). `Archive::save` had
stored the `mantles` slice and discarded the rest, so `config` (with
`site.base_url`), `scripts`, `domains` and `bindings` did not survive a round
trip — and because a version is named from `mantles` alone, a config-only change
produced no save at all, silently. Palabra took the split we proposed: **name
from `mantles`, store the whole document, dedup on stored content rather than on
the version name.** Re-verified here independently — all eight of our top-level
keys survive, a config-only change now saves and loads back, and the storage
numbers are unchanged (21 versions of a 456 KB document in 464 KB).

It is now normative rather than one implementation's habit: Palabra's SPEC gained
**§7, Naming versus storing**, which says an implementation MUST NOT collapse the
two jobs — with our `site.base_url` case recorded as the reason.

**Nothing blocks adoption from Palabra's side.** It remains unadopted because
Q40 is the author's decision, not because it is unready.

**Encryption is settled and it is NOT ours to build.** Palabra's container will
carry it (Phase 4, once libsodium is in their tree — they refuse to hand-roll an
AEAD, correctly), and **keys are never Palabra's**: we supply key material, they
encrypt the container. So Hormiga **keeps its vault** — it is doing the right job
— and does not build whole-bundle encryption. The eventual shape is
`write_file(path, key)`. The discipline: keep the seam
clean so Palabra can slide under it. The seam is the **`HormigaApp` database
methods** (`new_database` / `save_database` / `save_database_as` /
`open_database`) — the app never reaches into bundle internals; today they call
`hormiga::miga::pack`/`open` (`src/platform/miga.{hpp,cpp}`), tomorrow they call Palabra.
`src/platform/miga.*` is the swappable implementation, not an interface other code binds
to. When Palabra lands, reimplement those four method bodies; nothing else moves.

# A possible future SPLIT (author 2026-07-24 — noted, not now)

The author is considering splitting Hormiga from one monolith into **several
applications that still communicate through a shared Antfarm** (the Data app,
the Builder app, the Map app… each a Void Maiz host, one shared topology + data
store). Not done yet — recorded so the architecture keeps the seams that make it
possible: the Antfarm-as-shared-substrate, sections that already talk only
through the dispatcher + mantles (never direct calls between Data/Builder/Map),
and the `.miga`/working-copy split (a shared database the separate apps open).
The cleaner those seams stay, the cheaper the split is if the author takes it.

# Boundaries

- **The working copy stays authoritative at runtime.** The app always runs off
  the local `.db`/`assets`; the bundle is packed/unpacked at the seams, never
  read live. (Fast local storage is the point of the working copy.)
- **No server in the runtime** — sharing, when built, is P2P/opt-in, argued for
  on evidence (the Q10 boundary). The empty window promises nothing running.
- **Re-derivable data is never authoritative** — tiles/site/exports can always
  be rebuilt; if a bundle carries them (a future "include caches" option) it's
  convenience, never truth.
- **Secrets never leave encrypted** — a shared or backed-up bundle carries the
  vault sealed; the passphrase is never in the file.
