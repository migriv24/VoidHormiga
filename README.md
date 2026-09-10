# Void Hormiga

**The outreach organization's application.** Native C++20 desktop, founded
2026-07-15, built on four sibling projects: **Void Core** (the engine, C ABI),
**Void Maiz** (the view and node-graph library — we are its client #2),
**Void Allomone** (the rules language) and **Void Palabra** (the system layer:
history, versions, merge, persistence and device-to-device sync).

Void Hormiga keeps a community-outreach organization's people, events, images,
and resources as **tagged runes** — and composes newsletters and static
websites out of that data as **snapping content blocks** — local-first,
encrypted, and eventually collaborative. Every edit, block snap, and deploy is
a logged, replayable dispatcher command; the CLI, the GUI, and any agent share
one interaction surface.

- **Documents are block stacks.** A newsletter issue or a website page is a
  mantle of block runes; snapping a block into the stack *is* the ordering
  gesture. Query-backed blocks stay live against the database; `materialize`
  bakes them on purpose. One block graph renders to many outputs: the same
  `event_grid` becomes table-layout email HTML or a responsive web page. See
  [blocks & domains](okf/concepts/sections/blocks-and-domains.md).
- **The Antfarm is the org's backends made visible** — one node per configured
  holiday (data, assets, deploy, import, translate…), typed ports, live
  status. The default Antfarm is entirely local: embedded SQLite, filesystem
  assets, a snapshot fallback so no failure mode is a dead app. Cloud is
  something an admin *adds*. See [the Antfarm](okf/concepts/platform/antfarm.md).
- **Encryption is a pillar.** One vendored crypto dependency, a
  passphrase-sealed org registry, privacy enforced at the render seam, and
  E2EE collaboration designed-for from day one. See
  [security](okf/concepts/platform/security.md).

Start reading at **[okf/index.md](okf/index.md)**. The dated founding design
document — the one place that references the predecessor project freely — is
[DESIGN.md](DESIGN.md).

## Status

Working software, still pre-1.0. What runs today:

- **Two front-ends over one document.** A desktop GUI, and a headless CLI so an
  agent in another folder can drive the same database. Neither is a second code
  path — the newsletter an agent builds is byte-identical to the one the button
  builds, because it is the same function over the same state.
- **The Data section** — contacts, events, organizations and resources as
  tagged runes, over an embedded SQLite spine with CSV import.
- **The Builder** — documents as a canvas of snapping block runes, with a live
  preview, rendering to bilingual email HTML *and* a multi-page static website.
- **Territory** — a native slippy map with layers, drawable shapes that bestow
  tags, and PNG export.
- **Allomone** — a rules language over the rune graph, derive-only, where two
  rules that disagree produce a visible conflict rather than a winner picked by
  evaluation order.
- **Publishing** — static-site deploy to Cloudflare Pages or GitHub Pages, plus
  S3-compatible storage, all spoken natively. No Node, no npm, no Python at
  runtime. Two deploy targets rather than one on purpose: the property this
  design rests on is that every cloud host is disposable, and one target makes
  that a claim instead of something you can exercise.
- **Sync** — device-to-device, merge-first, with an X25519 identity that never
  leaves the machine.
- **Updates** — `voidhormiga-cli update` reads a static feed, compares versions,
  verifies the download's checksum and launches the installer. It never checks
  without being asked and never installs without being told; see
  [Installing](#installing).

The development history is [okf/log.md](okf/log.md); open design questions are
[okf/developer_questions.md](okf/developer_questions.md).

## Building

Needs a C++20 compiler, CMake and Ninja. The sibling projects are expected
beside this one — `../VoidCore`, `../VoidMaiz`, `../VoidAllomone`,
`../VoidPalabra` — which is the Void family's host pattern:
`add_subdirectory(../VoidMaiz)`, and it locates the rest. A missing sibling
fails configuration with a message naming the variable to set.

```
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
build\bin\voidhormiga.exe          # the GUI; or VoidHormiga.bat
build\bin\voidhormiga-cli.exe --describe   # the headless briefing
```

Layout: `src/app` (the shell) · `src/main` (`desktop.cpp` GLFW, `headless.cpp`
CLI) · `src/domain` · `src/render` · `src/publish` · `src/gis` · `src/sync` ·
`src/update` · `src/platform` · `src/ui` — with `tests/`, `tools/` (linters and the render
golden), `okf/` (the design), and `vendor/` (every third-party dependency,
with its license). No package manager, no CDN, no framework.

## Installing

**Not yet — this is ready rather than released.** The package stages cleanly and
the update client is built; no installer has been compiled, nothing is signed,
and no second machine has installed anything. The full status, including what
is *not* proven, is in
[okf/concepts/platform/distribution.md](okf/concepts/platform/distribution.md).

When it ships it will be a standalone per-user installer — `$LOCALAPPDATA`, no
elevation, a Start Menu shortcut, an uninstall entry — built by
[Void Mago](../VoidMago/README.md) from `void.json`:

```
mago stage voidhormiga --platform windows-x64
makensis ../VoidMago/stage/voidhormiga-0.1.0-windows-x64/voidhormiga.nsi
```

**Side by side.** Each version installs into its own folder under
`%LOCALAPPDATA%\VoidHormiga\`, so installing a new one leaves the old one
working. That is what makes taking an update a decision you can take back, and
nothing in this repository ever deletes a version folder.

### Updates

There is no updater to install and no hub to sign into. The "hub" is one static
`void-updates.json` beside the releases, and each application reads its own
entry:

```
voidhormiga-cli update            what is installed, and the setting
voidhormiga-cli update --check    ask the feed (a network request)
voidhormiga-cli update --install  check, download, verify, launch
voidhormiga-cli update --startup  let the app ask the feed when it opens
voidhormiga-cli update --never    never offer updates again
```

The desktop application does the same thing behind a prompt, and both print the
same words about the same release. **Two rules it keeps:**

- **It never checks without being asked.** A check is a network request a person
  did not make, so the setting starts at *unasked*: the first thing a new
  install does is ask permission to check, before it has checked. "Don't ask
  again" is real, is stored beside the installation rather than inside your
  database, and is never re-asked.
- **It never installs without being told.** The download is verified against the
  feed's SHA-256 before anything can be run, and a file that fails is deleted.
  Nothing is signed yet, so the prompt says so: the checksum proves the download
  arrived intact, not who built it.

## The family manifest

[`void.json`](void.json) declares what this project is, which Void projects it
requires and at what versions, and what it provides — the build-time twin of
`voidhormiga-cli --describe`. It is the convention
[Void Mago](../VoidMago/okf/index.md) reads to resolve the family and to build
the installer; every Void repository carries one at its root. Its `release`
block is what the update prompt shows a person — including
`behavior_changes`, the category a version number cannot express and a
dependency resolver cannot see.

## License

MIT — see [LICENSE](LICENSE). Third-party components are vendored, never
fetched, and each ships its license beside the code it covers; they are indexed
in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
