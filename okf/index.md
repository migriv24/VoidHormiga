---
okf_version: "0.1"
type: Index
title: Void Hormiga
description: The front door — what Void Hormiga is, the founding commitments, and where every other document lives.
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

# Void Hormiga

**The outreach organization's application.** A native C++20 desktop app, founded
2026-07-15, built on Void Core (`../VoidCore`, the engine) and Void Maiz
(`../VoidMaiz`, the view — we are its client #2, the first data-heavy one).

One sentence: **Void Hormiga keeps a community-outreach organization's people,
events, images, and resources as tagged runes — and composes newsletters and
static websites out of that data as snapping content blocks — local-first,
encrypted, and eventually collaborative.**

> **Name.** *Hormiga* — ant. **Void Hormiga is the full official name; the UI
> drops the "Void"** (author, 2026-07-16) — like Adobe Photoshop: the family
> prefix is formal, "Hormiga" is what users see and say. Repo and stack-facing
> artifacts stay `VoidHormiga`/`voidhormiga`. The ant metaphor is structural,
> not decorative: the organization's backends form the
> **[Antfarm](/concepts/platform/antfarm.md)**, and its colony of small tagged things
> does the work.

# The founding commitments

1. **The model lives in Void Core; the dispatcher is the only door.** Every
   contact edit, block snap, tag pass, and deploy is a logged, replayable
   dispatcher command. The CLI (the command bar *inside* the app), the GUI, and
   any agent are three callers of the same verbs — one interaction surface.
2. **Local-first is the resting state, not a feature.** The default install
   works forever with no network: embedded SQLite, local filesystem assets, a
   JSON snapshot fallback so no failure mode is a dead app. The network is
   something an admin *adds*, as [Antfarm](/concepts/platform/antfarm.md) holidays.
3. **End-to-end encryption is a pillar, not a feature.** One vendored crypto
   dependency (libsodium); a mandatory passphrase on the org registry; privacy
   enforced at seams, not by template convention; collaboration protocols are
   E2EE by design. See [security](/concepts/platform/security.md). Corollary: **the repo
   root is a public artifact** — no member data, exports, or credentials beside
   the code, ever.
4. **One block graph, many outputs.** A document is a mantle of snapping block
   runes; rendering is per (glyph × domain) — the same `event_grid` becomes
   table-layout email HTML or a responsive web page. A theme is a renderer
   pack. See [blocks & domains](/concepts/sections/blocks-and-domains.md).
5. **A conformant client, never an upstream editor.** Void Core and Void Maiz
   gaps go through their `MESSAGE_*` files; this repo never patches them. We
   consume the sibling-repo pattern (`add_subdirectory`) like every Void Maiz
   host, and we report real measurements upstream (scale, gaps) as evidence.

# Map

**The concepts live in five folders** (restructured 2026-08-27 — seventeen
files in one directory had stopped being a structure and started being a pile).
Each folder has its own index explaining its boundary, and
`tools/lint_okf_links.py` runs in CI so a moved page takes its inbound links
with it.

| folder | what belongs there |
|---|---|
| [foundation](/concepts/foundation/index.md) | what the application *is*, prior to any feature |
| [sections](/concepts/sections/index.md) | the rooms: Data, Builder, Calendar, Territory |
| [platform](/concepts/platform/index.md) | the machine underneath: storage, backends, privacy, network |
| [allomone](/concepts/allomone/index.md) | the rules engine, adopted from Void Maiz |
| [projects](/concepts/projects/index.md) | the neighbours: sibling repos and the test colony |

## foundation — what this is

- [Application boundaries](/concepts/foundation/application-boundaries.md) —
  the three kinds of dependency (runtime / capability package / sibling
  application), what a separation costs a *user*, and the four-layer
  decomposition of the map that says which part of it is actually separable.
- [Hormiga's three DSLs](/concepts/foundation/dsls.md) — **Builder** = layout
  DSL (grid, no logic), **Antfarm** = interface/protocol DSL (node-graph, the
  I/O boundary), **Allomone** = logic/scripting language (text) that consumes
  the other two. Answers "does changing a card color need the Antfarm?" (no —
  the app is the interior/default output).
- [Data model](/concepts/foundation/data-model.md) — the five kinds of things
  an outreach org runs on, as glyphs; tag axes + temper hygiene; relations as
  edges.

## sections — the rooms

- [Workspace & sections](/concepts/sections/workspace-and-sections.md) — the
  application's shape: Data / Builder / Antfarm / Territory / Calendar as the
  main sections over one core; the panels/windows overhaul; the widget-toolkit story.
- [Builder](/concepts/sections/builder.md) — the Q20 pivot (a data-bound
  Figma): components on a grid, Newsletter (HTML) vs Website (JS), live
  preview, bands, multi-page, authored navigation, a real theme; every gesture
  a `doc` verb.
- [Builder roadmap](/concepts/sections/builder-roadmap.md) — the do-list: every
  planned Builder feature with a ✅/🔨/⬜ status.
- [Blocks & domains](/concepts/sections/blocks-and-domains.md) — documents as
  block stacks (Node Blocks applied); query-backed blocks and `materialize`;
  renderer packs per (glyph × domain); themes; the website story and the
  dogfood target.
- [Calendar](/concepts/sections/calendar.md) — dated runes on a time grid:
  3-day/week/month views styled by the map's rules engine; static + web
  exports; grounded in RFC 5545 (model) and FullCalendar (view vocabulary).
- [Calendar roadmap](/concepts/sections/calendar-roadmap.md) — the calendar's
  do-list, reorganized 2026-09-10 around the author's identity statement: Hormiga
  is **the compatible calendar, a hub of all other calendars**, not a super
  calendar. The X-track (exchange) makes "we are not exclusive" a property of the
  mechanism — one `VEVENT ⟷ rune` lens, and every calendar system a transport
  onto it.
- [Territory](/concepts/sections/territory.md) — the map: location-faceted
  runes on a canvas over a swappable map source (not assumed to be Earth); the
  Neighborhood analysis and its clean mapping to runes/holidays/tags/Scry;
  reactive visuals as interaction-net-flavored projections.

## platform — the machine underneath

- [.miga v3 format](/concepts/platform/miga-format.md) — the whole database as
  one portable, switchable, backup-able bundle: every mantle + assets +
  encrypted secrets; working-copy vs bundle (git-shaped); re-derivable vs
  irreplaceable; online/offline load. Resolves Q17 + Q19.
- [The Antfarm](/concepts/platform/antfarm.md) — holidays as the visible
  nervous system: one rune per backend, typed ports, live status faces; the
  registry; the local-first storage defaults.
- [Security](/concepts/platform/security.md) — the E2EE posture: at-rest
  encryption, the render-seam privacy rule, the three collaboration modes,
  signed releases.
- [Data planes](/concepts/platform/data-planes.md) — the admin database, the
  publication and the inbox: what is authoritative in each, why they must never
  merge, the two kinds of "cloud save", and why claiming a contact must not
  search the database.
- [The web platform](/concepts/platform/web-platform.md) — domain and hosting;
  what a visitor is allowed to do; the AWS mapping and the order to build it.
- [Identity](/concepts/platform/identity.md) — **opened 2026-08-27**: the admin
  profile, the signed-in identity and the contact as three different things,
  and why a profile is a credential on a device rather than a person. Many
  profiles, one contact; never a `contacts.is_admin` column.
- [The download page](/concepts/platform/download-page.md) — **built and live
  (2026-09-08), on a client's site**: where the installer is hosted and how a
  stranger gets it. GitHub Releases rather than the website (release assets live
  outside git's history; a 7 MB binary in a Pages branch does not, once per
  release forever), and **the button carries no version number** — a
  stable-named copy of each release makes one URL permanently correct, so
  shipping a new version never means redeploying the site. **§5(d) is the OS
  detection the author asked for**, in the only shape it survives: a `platform`
  field on `download` and `link`, a grid row of them as a *platform set*, the
  visitor's own moved first and labelled, and **none of them ever hidden** — a
  rule turned into a property of the mechanism. Also the honest part: nothing is
  signed, so the page says what SmartScreen will say before it says it.
- [Distribution](/concepts/platform/distribution.md) — **opened 2026-09-04**:
  how the application reaches a machine that is not the developer's, and how the
  person on it hears about a newer one. Void Mago compiles `void.json` into a
  side-by-side per-user installer; the update client is ours. **The hub is a
  file** — one `void-updates.json` beside the installers, read by each
  application about itself — because a hub is an application you must install
  before the application you actually wanted. The rule the whole design turns
  on: *a check is a network request a person did not make*, so the preference
  starts at "unasked" and lives beside the install rather than in the org.
- [Collaboration](/concepts/platform/collaboration.md) — **opened 2026-08-27**:
  one database, several devices. The merge is Void Palabra's and is built; the
  transport is ours for now, LAN first, with a UDP beacon, an out-of-band short
  authentication string, and a sealed TCP stream. Sync writes through the
  dispatcher like everything else.

## allomone — the rules engine

- [Allomone](/concepts/allomone/index.md) — a folder. **Adopted from Void Maiz
  2026-08-10** (we proposed the move; they built it): the grammar, evaluator
  and composition engine are upstream's, and what is ours is a domain library —
  subjects, our property vocabulary and its **merge laws**, twenty-two domain
  predicates (dates, geography, roles, the graph), our glyphs. Rules are a
  **set, not a sequence**: many independent sources state what should be true, a
  lattice merge composes them, and a genuine disagreement *across* sources
  surfaces as a conflict a human settles with a logged command instead of being
  decided by evaluation order. Derive-only. Start at
  [adoption](/concepts/allomone/adoption.md) — the rest of the folder is the
  history that earned the move.

## projects — the neighbours

- [Void Reyna](/concepts/projects/void-reyna.md) — **a sibling project, founded
  2026-08-12 at `../VoidReyna` with its own OKF** (this page is Hormiga's view
  of the seam; that repo is the source of truth): the dataset generator, and
  the place **holidays get taken seriously as the transformation primitive**.
  Harvest → archive → extract → emit, with the impure crossing quarantined at
  the boundary and everything interesting on the pure side (Void Core's own
  invariant). A holiday is *an effect boundary plus a pure `Lens`*, lenses
  compose, and that makes the **pivot rule** — never write a direct A→B adapter
  when A→pivot→B exists — a theorem rather than a preference.
- [The Civic Record](/concepts/projects/civic-record.md) — **proposed
  2026-08-12**: a public website monitoring one city's government (Springfield,
  Oregon) — council members as contacts, meetings as mantles of attributed
  statements, and `policy`/`revision`/`provision`: a versioned DAG over a tree,
  because a policy is defined by its delta. Votes are edges onto a revision.
  Also where the "should Hormiga become five applications?" question is
  answered: four views over one model stay together, the dataset generator does
  not.
- [The Cat Dataset](/concepts/projects/cat-dataset.md) — a public, synthetic
  test colony (50 tag-rich fictional cats + linked birthday events) built by a
  C++ generator through the real dispatcher; replaces testing Allomone on the
  private partner organization, with the set-theory example baked into its tags.

## The documents that are not concepts

- [Verb & noun inventory](/verbs.md) — the CLI vocabulary Hormiga adds to the
  voidscript surface (Q8); seed status, validated in phase C.
- [Roadmap](/roadmap.md) — phases A–F, gated by exit tests, not calendar.
- [Developer questions](/developer_questions.md) — open decisions with leans.
- [Developer explanations](/developer_explanations.md) — a living tutor doc:
  concepts unpacked in plain language for the author (derive vs materialize,
  conflict resolution, reactivity, round-tripping, …).
- [Log](/log.md) — the running history.
- `okf/reports/` — **field reports from agents running the shipped binary**
  against real data. They are kept rather than folded away because each one is a
  measurement: what an outside caller, following the documentation in good
  faith, actually got. Every claim in them is reproduced against the binary
  before anything is decided, and the decisions land in the [log](/log.md) and
  the concepts. The folder was created on 2026-09-02 to hold the first report
  from a client that is **not** an outreach organization — a music project,
  adopted deliberately to find where the vocabulary runs out. It ran out in
  twelve places, two of which were live on a real community website at the
  time, which is the argument for keeping a client whose needs are wrong on
  purpose.
- `DESIGN.md` (repo root) — the dated founding design document, the one place
  that references the predecessor project freely. This OKF supersedes it
  concept by concept as things stabilize.
- **Inter-project messages** (repo roots, relayed by the author) — the seam
  with Void Maiz / Void Core. **Uniquely titled since 2026-07-21** (a bare
  generic name lets an agent read a stale message as the live one): outgoing
  `MESSAGE_FOR_<RECIPIENT>_hormiga-<topic>-<YYYY-MM-DD>.md`, replies arrive
  as `MESSAGE_FOR_VOIDHORMIGA_<sender>-<topic>-<date>.md`. A consumed message
  is folded into the [log](/log.md) and deleted — the log is the durable
  record of every exchange (founding 2026-07-15 through the canvas-actions
  reply 2026-07-21), not the message files.

# Status

**As of 2026-09-14.** **Void Hormiga 0.1.1 shipped on 2026-09-10**, and an
installed 0.1.0 found it and updated itself through the feed, which is the half
of phase G's exit test one computer can prove. Since then the work has followed
the author making real newsletters:
- the calendar became authorable and exchangeable: quick-add, recurrence, and
  iCalendar in and out (see the [calendar roadmap](/concepts/sections/calendar-roadmap.md));
- the newsletter learned rows and card grids (see the
  [builder roadmap](/concepts/sections/builder-roadmap.md));
- colon tags and clearance became visible in the GUI;
- a Niche Tools window now repairs a database copy that landed in the wrong
  folder.

The paragraphs below are the status as each phase opened, kept for the
reasoning; the [log](/log.md) is the running record.

**Phase C — the data spine — in progress (since 2026-07-19); phase F started
early (2026-08-27).** Phase A exited
2026-07-16 (OKF + founding exchange; all eight founding questions answered);
phase B's exit test went green the same week (GUI transcript replays headless)
and the sections shell + widget-protocol UI grew through 2026-07-16/18 with
Void Maiz shipping our asks same-day. Now landed: the vendored-SQLite Data
holiday (state + normalized rows, snapshot fallback), CSV import compiling to
one replayable batch, content-hashed asset ingestion with tag suggestion,
live Antfarm cards.

**Phase F (collaborate) was started out of order on 2026-08-27**, on the author's
direction that working from several devices is the main thing. Void Palabra's
merge is adopted (Q40 answered) and measured: two divergent copies of a database
converge to the same cut, conflicts surface as values, and syncing structurally
cannot move a credential. A LAN transport — discovery, an out-of-band pairing
code, a sealed stream — is built and verified up to the socket. The exit test
(two separate laptops) needs a second machine rather than more code. See
[collaboration](/concepts/platform/collaboration.md).

Remaining for C's exit: the rescue-dump import (deadline
**2026-08-02**, run right rather than rushed — the import infrastructure is
ready for it), Sheets import, temper hygiene, the table view
(see [roadmap](/roadmap.md) and the [log](/log.md)).

**The download page went live 2026-09-08**, on `clicklafont.com` — the author's
choice, because that site is experimental enough to be safe to break. It was
built by that client's agent from §6 of the concept, which is how three of that
page's numbers got corrected (the installer is 7.0 MB, not 25) and how the OS
detection arrived as a *design* rather than a script: the agent refused to fake
it, proposed platform sets, and platform sets are what shipped. **Void Hormiga 0.1.0 shipped on 2026-09-08** — installer, update feed,
checksums, a live download page, and `update --check` answering *up to date*
from an installed copy instead of 404. The road there:
the forward slash in Void Mago's generated `.nsi` that installed the fonts into
a folder called `vendorfonts` was reported there and **fixed the same day** (mago
0.1.6), at a chokepoint, verified here against this repository's own manifest.
What is left is a second computer, and then a 0.1.1 — because one release
proves the feed parses and nothing more, and the exit test is that it *updates
itself*.

**Linux and macOS are asked for and gated upstream.** The code is more portable
than the platform list suggests — every Windows-touching file carries a `#else`
but one, the HTTP layer shells out to `curl`, and the update client already
knows `XDG_CONFIG_HOME` and `xdg-open` — and two real gaps were closed on
2026-09-08 (`ws2_32` linked unconditionally; `HORMIGA_PLATFORM` answering
`"unknown"`, which is the key the update client looks itself up under in the
feed). But **Void Maiz declares `windows-x64` and `android-arm64`**, and both
our binaries link it, so the question is theirs before it is ours; Void Mago's
`wizard` emits NSIS and nothing else, which is the next gate after that. See
the [log](/log.md).

**Phase G — ship it — opened 2026-09-04**, when Void Mago staged this
repository for the first time and refused to produce a package. Both refusals
were right and both fixes are here; fixing the second found a third blocker
nobody had reported, which would have opened the application on the first test
device with no icons at all. The package now stages clean and the **update
client is built** — the feed reader, the version comparison, the prompt, the
digest check, and `voidhormiga-cli update`. Nothing is signed, `makensis` has
not been run, and no second machine has installed anything: the exit test is a
second computer, like phase F's, and the author's instruction was to have this
ready rather than launched while Allomone and Palabra are still moving. See
[distribution](/concepts/platform/distribution.md).

**Void Core 0.2.14 landed the same week** and answered all five of our asks
(2026-09-03). Glyph descriptors are a documented host contract, so the
hand-maintained `glyph_fields()` duplicate is deleted — it was already wrong —
and glyph **declarations travel in the state document**, which is what makes a
`.miga` self-describing and unblocks [Q59](/developer_questions.md). Runes now
have a `kind` (`entity` / `act` / `measure`); five of ours are `act` and none is
`measure`, because Hormiga's numbers are points and a point has no magnitude.
Q64 and Q65 are answered and cleared.

**Phase E gained its second deploy holiday on 2026-09-02**: `hol_github`
publishes to GitHub Pages natively, alongside the Cloudflare Pages path, which
is what turns *every cloud host is disposable* from an argument into a property
an organization can exercise. The same day brought the first field report from a
client that is **not** an outreach organization (`okf/reports/`), six one-line
render fixes it found — two of them live on a real community website — and the
author's decision that a site may never be published in one language: the answer
to "translating is tedious" is **better translation tooling**, not a switch. See
the [log](/log.md).
