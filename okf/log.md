---
type: Log
title: Log
description: The development history of Void Hormiga — the decisions that still shape the code, condensed by arc rather than by day.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-01T00:00:00Z
---

# How to read this

This was a day-by-day journal: 147 entries and about 8,000 lines written
between 2026-07-15 and 2026-09-01, one per working session.

**It was condensed on 2026-09-01, before the repository went public.** Two
reasons, and the second is the important one:

1. Most of those entries recorded *what happened that afternoon* — a slider
   that snapped back, a palette that was hand-maintained, a batch of Builder
   niceties. That detail is in the code and its comments, which is where it
   belongs and where it cannot go stale in a different direction from the thing
   it describes.
2. A running journal of work done *with a specific partner organization* is not
   this project's history, it is theirs. Hormiga is an application for
   community-outreach organizations in general. Any single one of them is a
   deployment, not a chapter of the design. Where a real deployment found a
   real bug, the *finding* is recorded here and in the code; the organization is
   not named. **The rule the author set for the code holds for the record too:
   Hormiga is not designed for any one organization.**

What survives below is the part a new developer needs: the decisions that are
still load-bearing, the reversals worth knowing about, and the reasons behind
choices that would otherwise look arbitrary. Dates are kept so the order is
recoverable.

The full journal is not in this repository. Nothing that follows is a summary
of a summary — each arc was rewritten from the entries it covers.

---

# The origin: a newsletter tool that had to become an application

**Void Hormiga replaced a working predecessor.** The earlier Hormiga was a
Python application, and its job was narrow and real: **it made a bilingual
community newsletter, and it shipped them.** That is the seed of everything
here. The block model, the render seam, the two output domains — all of it
grows out of "compose an issue out of the organization's data, and send it."

Three earlier decisions were reversed to found this project (2026-07-15):
*no new app*, *keep Python*, and *adopt Void Maiz only as a web protocol*. The
premise changed underneath all three — **Void Maiz existed now**, and an audit
of the predecessor's language-specific surface came back 100% writable in C++
with the vendor-don't-depend policy intact.

`DESIGN.md` at the repo root is the dated founding document and the one place
allowed to reference the predecessor freely. The concepts here describe what
Void Hormiga **is**, not what it replaced.

The predecessor kept publishing newsletters throughout this project's first
two months. It was never a rewrite-and-pray; it was a replacement that had to
earn its exit test.

---

# Founding: built on Void Core and Void Maiz

**2026-07-15 → 2026-07-16.** Void Hormiga is a *client*, and the discipline
that follows from that is the single most consequential structural decision in
the project.

- **The model lives in Void Core; the dispatcher is the only door.** Every
  change — a contact edit, a block snap, a tag pass, a deploy — is a logged,
  replayable dispatcher command. The command bar inside the app, the GUI, and
  any agent are three callers of the same verbs. A feature that cannot be
  expressed as commands, and therefore replayed headless, is designed wrong.
  This is why the headless front-end (below) took a week rather than a quarter.
- **Void Maiz is the view.** We are its client #2 and the first data-heavy one.
  The founding exchange completed same-day: Node Blocks Phase A shipped
  upstream in full (the `block` shape kind, snap/tear/heal/splice as one-batch
  gestures, stack re-flow, C-block containment) along with three of five
  face-widget asks.
- **A conformant client, never an upstream editor.** Gaps go upstream as
  messages, never as patches. This has held for the entire project and is the
  reason four sibling repositories can move independently.

Phase A exited 2026-07-16 with the OKF skeleton written before any C++ —
deliberately. Phase B (the skeleton app that builds, runs, and replays) landed
the same week.

---

# The data spine, and a deadline

**2026-07-19.** Phase C: embedded SQLite, CSV import, and the ingestion path.
The urgent part was a **data rescue** — moving a real organization's records
off a platform before a deadline. It ran copy-only and beat the deadline by two
weeks.

The lesson that outlived it: the rescue exposed how much of the app assumed
data it had authored. Images had no previews because the demo data had no
images, not because the library was missing — the kind of bug that looks like a
gap and is really a fixture problem.

Also from this period: the **ImgBB publish holiday** (opt-in, off by default)
and the first shape of the Antfarm as a real graph.

---

# Territory: the map is native

**2026-07-22.** A native slippy map, built rather than embedded. Over the
following days it grew base-map sources and treatments, layers, PNG export,
box-select, search, marker shapes and icons, and drawable **map shapes** — rune
annotations that are rule-colored and bestow tags, which is what makes the map
a *view of the data* rather than a picture beside it.

**Canvas actions became first-class commands** (2026-07-21), asked of Void Maiz
and delivered: a volunteer's click and an agent's `map place contact @here` are
the same transcript entry. That equivalence is the whole reason the headless
front-end could exist later.

`src/gis/` was carved out as its own folder on 2026-08-21, and the question
*"should the map be its own Void project?"* was measured before it was
answered — the measurement reordered the question and the map stayed.

---

# The Builder pivot: documents as a canvas

**2026-07-22.** The Builder pivoted from a form-driven editor to a **document
canvas** — near-WYSIWYG, every gesture a verb. The live preview landed the same
week and serves artifacts only.

The website then split from the newsletter (**the website ≠ the newsletter**,
2026-07-23) and became its own track: multi-page sites, authored navigation, a
page manager, band styling with full-bleed themed sections, templates, image
grids with display modes, site chrome and meta (favicon, Open Graph, sitemap,
404), and a Style tab backed by real style axes.

**One block graph, many outputs** is the rule that makes this tractable: a
document is a mantle of snapping block runes, and rendering is per
(glyph × domain), so the same `event_grid` becomes table-layout email HTML or a
responsive web page.

---

# Allomone: a rules engine that became a language, then left home

This is the longest arc in the project and the one most worth understanding.

**Phase 1 — a rules engine (2026-08-03).** It started as a way to color cards
by rule. It was named, given a folder, and framed as a declarative
graph-rewriting language. Two decisions from the first week still hold:
**derive-only** (rules compute, they never mutate the model) and
**conflicts-as-a-feature** (two rules that disagree produce ⊤, not a winner
picked by evaluation order).

**Phase 2 — blocks, then text (2026-08-04).** A Scratch-style block canvas was
built, and then *retired within a day* in favour of text. The block editor
worked; text was simply the better substrate for a language, and the entry that
records this is candid that the block work was a detour.

**Phase 3 — a real language (2026-08-04 → 08-05).** A tree-walking interpreter,
then functions, recursion and mutual recursion with a termination budget; graph
traversal so rules refer to runes by *structure* rather than by name; set theory
and quantifiers; graph measures (centrality, community, distance); reading
fields; a tag recommender. Hormiga's **three DSLs** were distinguished from each
other in this period, which is what stopped the language from absorbing jobs
that were not its own.

**Phase 4 — the editor we should not have built (2026-08-05 → 08-11).** A
from-scratch code editor with inline widgets was built in the app. It turned
out upstream already had one. The entry title is the lesson:
*"the editor was upstream's all along, and we built a worse one."*

**Phase 5 — Allomone moves UP (2026-08-06 → 08-11).** Proposed to Void Maiz:
the language belongs in the view library, not in one of its clients. **The
answer came back yes.** Ours was frozen and the upstream engine adopted. Our
domain layer (`src/domain/hormiga_allomone.*`) kept Hormiga's *vocabulary* —
the predicates, the glyphs, the merge laws — and became a thin, view-free,
headless-provable client of the engine.

**Phase 6 — Allomone leaves Void Maiz too (2026-08-29).** The language became
its own repository, `../VoidAllomone`, prompted by a second consumer that is
not C++. Void Maiz kept forwarding headers, so **Hormiga compiled unchanged**;
`with` and `device` stopped being kernel predicates and became host predicates,
which is what let the language stop depending on one host's data structure.
Our namespace alias `namespace allo = hormiga::allomone` is what caught an
upstream naming collision — the rule they wrote down afterwards was that a
library extracted out of its host does not get to claim the short name.

The whole arc is one idea arriving in stages: **a language that belongs to a
domain should not live inside an application, and a language that belongs to
everyone should not live inside a view library.**

---

# Publishing: native, and no runtime anybody has to install

**2026-07-20 → 2026-08-25.** The web domain began as "the same blocks, a real
static site." Making that *publishable* took considerably longer than making it
render.

- **The email domain was fixed first** (2026-07-20): table layout and public
  image URLs, because email HTML is not web HTML.
- **`deploy-site` and the hosted-site holon** (2026-08-19).
- **Publishing went native** (2026-08-20): no npm, no Node, no wrangler, no
  Python. The transport is `curl`, which ships with the OS. A dependency that
  the operator has to install is a dependency that fails on the day of the
  deploy.
- **AWS** (2026-08-21 → 08-25): researched, mapped onto the existing holons,
  SigV4 signed by hand, an object-store holon, and a push that answers the
  "why do I have to redeploy everything" complaint.
- **Preflight, and the green light that lies** (2026-08-20): a credential
  validator that reports success without proving anything is worse than no
  validator. Several entries in this period are variations on that one lesson,
  each found the same way — by a real deploy failing after a check had passed.

**Publishing is not a per-language operation** (2026-08-20). A bilingual site
whose two halves are rendered separately will eventually publish one current
language and one stale one, under one URL. `site/` is one artifact carrying
every language; the render that precedes a publish renders all of them, and the
record names all of them.

---

# Headless: an agent, in another folder, drives Hormiga

**2026-08-18.** A second front-end with no window. Because the model had lived
in Void Core since day one, this was a *declaration* — which glyphs, which
actions, which predicates, which effects — rather than a second code path.
Void Maiz's `voidmaiz_headless` supplies the session, the advisory lock, the
journal, the effect gate and the briefing.

**Headless is the removal of a projection, not a second application.** The
newsletter an agent generates is byte-identical to the one the button
generates, because it is the same function over the same state.

The first real agent runs found a class of bug that this project now watches
for specifically: **silent, wrong, and reported as success.** Network holidays
that were no-ops headless because a platform seam was unset. An effect that
rendered an empty mantle and returned ok. A `.miga` bundle accepted as a state
document, which would have merged an empty document into somebody's archive.
Each of these is recorded in the code at the seam where it happened.

---

# The one-database-one-folder rule

**2026-08-21.** The most expensive bug of the project was not a crash. Launched
from the source tree, the app opened — and then saved — the database sitting
beside the *source*, while the organization's real database sat in its data
folder. Two copies of one database were edited for two days and merged by hand
twice.

`--state <path>` now sets both the document and the base directory, so
everything an organization owns hangs off the folder its document lives in, and
the resolved absolute path is in the title bar where a person can see it.

**2026-09-01** completed the thought: the eight folder names (`assets/`,
`tiles/`, `site/`, `exports/`, `backups/`, `documents/`, `templates/`,
`fonts/`) were string literals at forty-odd call sites, so a database could not
put its map cache on another disk. Each is now `config set paths.<name>`,
absolute or relative to the database. See `src/app/paths.cpp`.

---

# Security, and the seams that enforce it

- **libsodium vendored, the credential vault ships** (2026-07-21). One crypto
  dependency, never a hand-rolled primitive, never a fallback secret.
- **The `.miga` v3 database bundle** (2026-07-23): the whole organization as one
  portable file. Assets are bundled; re-derivable caches (`tiles/`, `site/`,
  `exports/`) deliberately are not.
- **The encrypted backup** (2026-08-21), and a recovery tool that could not
  recover — caught before it mattered. Chunked
  `crypto_secretstream_xchacha20poly1305`, so a *truncated* backup is detected
  rather than silently restoring a prefix.
- **A newline in a value is a command injection** (2026-08-21). Our own
  tokenizer agreed with nobody, and the fix went upstream into Void Core's
  §6.1 quoting rules.
- **Privacy is enforced at a seam, not by convention.** Internal-notes-class
  fields never reach an Output-interface holiday, and the check is at the
  render/export seam where it is testable. Publishing a person requires
  `clearance:public`, and the renderer says how many people were withheld — an
  index that comes out empty because nobody carries the tag looks identical to
  a broken exporter otherwise.

---

# The sibling projects

- **Void Core** — the engine, C ABI. Adopted **0.2.7** on 2026-08-25, at which
  point we stopped implementing the §6.1 codec and started calling it.
- **Void Maiz** — the view. Client #2; the blessed host pattern comes from
  `../InteractionCombinators`.
- **Void Allomone** — the language, extracted 2026-08-29 (above).
- **Void Palabra** — translation. The merge was consumed 2026-08-27.
- **Void Reyna** — founded 2026-08-12 as a sibling out of the Civic Record work
  (below), when the research showed the problem was not Hormiga's.

The **Civic Record** (2026-08-12 → 08-16) is worth its own note: a project to
model policies over time. The research sessions mostly *deleted* the designs
that preceded them, `policy` won as the modelling primitive, and reading a real
document taught two things no amount of design had. The Civic Record window
made the projection visible.

---

# Sync

**2026-08-27.** Built merge-first: the merge is the hard part and everything
else is transport, so the merge was built and tested before any wire existed.
Device identity is an X25519 keypair that never travels, and pairing shows a
short authentication string so a person can see what the machines agreed on.

---

# Dates, and a query language that knows what today is

**2026-08-28.** `date:` predicates threaded through every query-backed block —
`date:future`, `date:recurring`, `date:undated`. Void Core's query verb cannot
know what today is, so the clock enters at Hormiga's seam, and the golden
render pins only the two predicates that do not depend on it. A golden that
expires is a golden nobody trusts.

---

# 2026-09-01 — preparing for publication

The repository was made ready to be public. Changes worth recording:

- **The site colophon is editable.** "Built with Void Hormiga" was hardcoded in
  the footer — the one string on a generated site the operator could not
  change. It is `config set site.colophon` now; unset prints the credit, any
  value replaces it, `none` prints nothing.
- **`render-site` stopped guessing its document.** With no document named, the
  headless renderer inherited the GUI's `cur_doc` initialiser — a hardcoded
  mantle name — and would render a *newsletter* as a website, write it as
  `index.html`, shrink the sitemap to one page, and return ok. It now falls to
  the active mantle. Naming the document still wins and is still the advice.
- **`effect pack-database`.** A `.miga` could previously only be made from the
  GUI, so a database an agent operates could not produce a portable bundle at
  all. Bundle keys now preserve the assets folder's real name, because image
  fields resolve relative to the *database* — a bundle that renamed the folder
  on the way in would restore photos the document could no longer find.
- **The record was de-identified.** A partner organization's field reports,
  data, and name were removed from the repository and this log. Their findings
  remain, credited to "the field agent," because the findings are the useful
  part and several of them are the reason particular code is shaped as it is.
- **Publication hygiene.** `.gitattributes` (the tree was mixed LF/CRLF and a
  stray `sed -i` had silently rewritten an entire file); `.gitignore` extended
  to cover runtime sidecars, the agent config, and two vendored libsodium
  metadata files that hard-code the builder's home directory.
