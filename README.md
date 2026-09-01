# Void Hormiga

**The outreach organization's application.** Native C++20 desktop, founded
2026-07-15, built on four sibling projects: **Void Core** (the engine, C ABI),
**Void Maiz** (the view and node-graph library — we are its client #2),
**Void Allomone** (the rules language) and **Void Palabra** (translation).

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
- **Publishing** — static-site deploy to Cloudflare Pages or S3-compatible
  storage, signed natively. No Node, no npm, no Python at runtime.
- **Sync** — device-to-device, merge-first, with an X25519 identity that never
  leaves the machine.

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
`src/platform` · `src/ui` — with `tests/`, `tools/` (linters and the render
golden), `okf/` (the design), and `vendor/` (every third-party dependency,
with its license). No package manager, no CDN, no framework.

## The family manifest

[`void.json`](void.json) declares what this project is, which Void projects it
requires and at what versions, and what it provides — the build-time twin of
`voidhormiga-cli --describe`. It is the convention
[Void Mago](../VoidMago/okf/index.md) will read to resolve the family; every
Void repository carries one at its root.

## License

MIT — see [LICENSE](LICENSE). Third-party components are vendored, never
fetched, and each ships its license beside the code it covers; they are indexed
in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## A note on what lives in this repo

**The repo root is a public artifact.** No member data, no exports, no
credentials beside the code, ever. Organization data lives where the app
puts it — locally, on the machine that runs it.
