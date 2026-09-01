---
type: Roadmap
title: Roadmap
description: "Phases A–F (founding → skeleton → data spine → builder → publish → collaborate), each gated by an exit test, not a calendar; the predecessor app keeps shipping until phase D exits."
tags: [status:current, audience:dev, confidence:asserted, roadmap]
timestamp: 2026-07-16T00:00:00Z
---

Gated by **exit tests, not calendar**. The predecessor app (see `DESIGN.md`,
repo root) keeps shipping the monthly newsletter until phase D's exit test
retires it — no rewrite stops the presses.

**Prerequisite, urgent, external:** the **data rescue before 2026-08-02** —
runs in the predecessor's repo and produces the database dump that phase C's
import holiday consumes. The author's call (2026-07-16): do it right, not
rushed — the confidence bet is that Void Hormiga's import infrastructure is
good enough before the deadline to receive the download. Everything else can
slip; this cannot.

# A — Founding (EXITED 2026-07-16)

`DESIGN.md`; the founding message exchange with Void Maiz (complete
2026-07-15 — Node Blocks Phase A + our three widget asks shipped upstream
same day); the OKF skeleton. Exit conditions met: the OKF stands on its own,
and the author answered the eight founding questions (2026-07-16; see
[developer questions](/developer_questions.md)).

# B — The skeleton app (EXITED; its exit test, the replay smoke, is green in CI)

Window + core + canvas, the sibling-host pattern (`add_subdirectory`, CMake +
Ninja, vendored deps only, MIT, public from day one). Platform-free `app.cpp`
+ GLFW desktop shell from birth. A toy mantle rendered and editable; the
command bar (the CLI *inside* the app); replay smoke test in CI.
*Exit: a transcript built in the GUI replays headless to an identical state
document.*

# C — Data spine + the Data section

SQLite Data holiday; Import holidays (rescue dump, Sheets, CSV) land the real
org data; **the Data section stands up** ([workspace & sections](/concepts/sections/workspace-and-sections.md)):
the table view (upstream's [vh]-queued library table, or interim host-side
tables), forms/detail panes speaking the widget protocol as far as it exists
(Q12), image ingestion with tag suggestion via the Asset FS holiday; tag
axes + temper hygiene; the Antfarm section as a card list (v0). Resolve the
verb surface here — the predecessor's route inventory is the checklist.
*Exit: the whole org database lives locally, fully tagged, queryable by one
grammar from CLI and GUI; no cloud BaaS or image host is configured anywhere.*

# D — The Builder section (rides Node Blocks Phase A)

Block glyphs + the `email` renderer pack (porting the 14 section templates);
the Scratch-shaped chrome (palette dock — drag-from-panel per the upstream
answer, categorized/searchable); snap-stacked issue building with
query-backed blocks; property-rich inspectors; the bilingual engine's
translate pass; `materialize`; export.
*Exit: this month's real newsletter is built in Void Hormiga, snapping
blocks, and the rendered HTML passes the same eyeballs the predecessor's
output does. The predecessor retires from newsletter duty.*

# E — Publish

`web` domain renderer pack + theme #1; site mantle + pages; deploy holidays
(folder, GitHub Pages); **the project's own website ships from inside the
app** (the dogfood target — [blocks & domains](/concepts/sections/blocks-and-domains.md));
the signed self-updater ([security](/concepts/platform/security.md) §6). Distribution:
GitHub Releases first (CI builds, conformance + replay tests, signed
artifacts), then the dogfooded site with a download button. Windows first;
macOS/Linux as CI targets once the app has users there.
*Exit: a stranger downloads Void Hormiga from a page Void Hormiga deployed,
and it updates itself.*

# F — Collaborate (STARTED 2026-08-27, ahead of D/E)

`.miga` sharing, LAN peer sync E2EE, roles
([security](/concepts/platform/security.md) §4–5).
*Exit: two laptops on one Wi-Fi edit the same org, offline-tolerant, with the
relay/mesh decision then made on evidence.*

**Started early, and deliberately out of order**, on the author's direction
("the main thing is i want to be able to work on an hormiga database from
multiple devices"). The justification is the one
[data planes](/concepts/platform/data-planes.md) §7 already made: the *merge* is
the only part of sync that can be silently wrong, it needs no network, and
finding out it duplicates data is cheapest on one machine with two folders. That
half is now built and measured — see
[collaboration](/concepts/platform/collaboration.md) §7.

**Where it stands against the exit test:** everything up to the socket is
verified, including a full sealed exchange between two databases and independent
agreement on the pairing code. What is *not* done is the exit test itself — two
separate laptops. That needs a second machine, not more code.

# Planned sections (post-spine, evidence-ordered)

- **Analysis** — the mini-Tableau: charts/pivots over tag-grammar queries,
  host-computed. Starts inside the Data section (Q13); needs phase C.
- **Territory** — the Neighborhood successor: location-faceted runes on an
  interactive map view; tile/geocoding/export holidays per Q14. Needs the
  spine, the custom-view answer from upstream, and the Geo holiday.

# Continuous

The Antfarm section grows from card list (C) to the full node editor with
its user-friendliness layer — status dashboard, guided add-a-holiday flows
(E-ish). The reserved holidays — `model` (LLM), Territory (geo), Courier
(email dispatch) — arrive on the finished spine in whatever order real use
pulls them. Scale measurements go upstream as they're taken
([data model](/concepts/foundation/data-model.md)).
