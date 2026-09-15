---
type: Log
title: Log
description: The development history of Void Hormiga — the decisions that still shape the code, condensed by arc rather than by day.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-02T00:00:00Z
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
- **Void Palabra** — the system layer for Void Core: how state is remembered
  (history), named (versions), merged (convergence), stored (persistence) and
  spoken between devices (sync). Its merge was consumed 2026-08-27.
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
---

# 2026-09-02 — a second client, GitHub Pages, and a decision about language

Two things happened on the same day and they belong in one entry, because the
second is the answer to a question the first asked.

## The Click LaFont report

The first field report from a client that **is not an outreach organization**:
one person's music project — a character, two albums, and a website meant to be
explored. Adopting it was the point, which is to say finding where the
vocabulary runs out. It ran out in twelve places on day one, and the report
keeps six **defects** apart from six **absences** on purpose. It is kept in
`okf/reports/` rather than folded away, per the rule the index states: each
report is a measurement of what an outside caller, following the documentation
in good faith, actually got.

**Two of the six defects were live on a real community organization's website
at the time**, which is the strongest argument the report makes for adopting a
client whose needs are wrong on purpose.

Fixed the same day:

- **D1 — every page this renderer has ever produced was invisible without
  JavaScript.** `.reveal` starts at `opacity:0` and only `app.js` ever added
  `.in`, so a visitor with scripting off got the header, the hero and the
  buttons over an empty page, with nothing to indicate anything was missing.
  Every event card, directory entry and flier on the community site was a
  `.reveal`. The fix is the three-line `<noscript>` that `src/render/site.cpp`
  had already argued for in a different context, and it turns the animation
  *off* rather than faking it: a no-JS visitor is owed the content, not the
  choreography.
- **D2 — `.site-head` hardcoded white.** The one rule in the stylesheet that
  stated a colour instead of reading a token, sitting directly under the block
  that computes label colours from WCAG relative luminance. Its own contents
  were tokens, rescued against `--bg`, so on any dark ground the brand sat at
  about 1.3:1 — and not only on a dark `theme.bg`: with the default `theme.dark
  1`, every dark-mode visitor to the live site got it.
- **D3 — `.meta a` had no colour**, so the "Watch on YouTube" line under every
  `video` block rendered in browser-default `#0000EE`.
- **D4 — `narrative` behaved differently in the two renderers.** The email kept
  the author's line breaks and linkified bare URLs; the website did neither.
  `AGENT-GUIDE.md §8` documents the linkification without distinguishing them,
  and the guide is what an agent trusts — so the fix makes the guide true rather
  than narrowing it. What the divergence had cost, measured: a site with no
  lists at all, two eighteen-track tracklists as one hyphen-joined paragraph
  each, and every ordinary paragraph its own block rune. `linkify` grew a style
  seam at the same time, because the email's inline `color:inherit` is right
  there and wrong on a page that has a stylesheet.
- **D6 — the data mantle name is hardcoded, and being wrong about it was
  silent.** Naming the data mantle after the organization is the obvious first
  move for a new client, and it is what this one did: every command succeeded,
  `validate` said `valid`, and `render-site` reported `ok` with every gallery
  empty and no asset staged. The report explicitly asked for the *cheap* fix
  rather than the expensive one — a warning at render time, not a configurable
  name — and that is what shipped. Whether an organization's data namespace
  should permanently be called `demo-org` is now
  [Q57](/developer_questions.md).
- **Part 3 — there was no operator escape hatch for the stylesheet.** Three
  two-line cosmetic defects on a live public site, each unfixable by the person
  whose site it is, because `render_site` unconditionally writes `style.css`.
  A `custom.css` beside the database is now staged and linked after the built
  one. Not a loosening of the render seam and the distinction is the design: a
  *field* would be a way to get authored text into a public page through the
  model, which `video` already refuses; a *file* is the operator's own machine
  and their own hand, and a `<link rel=stylesheet>` cannot execute anything
  whatever it contains. It is the pattern `fonts/` has used since it shipped.

**D5 was a misread, and the guide was the reason.** `related` is Void Core's
*tag*-proximity verb (`usage: related <tag>`) and has never reported edges;
`links <rune>` is the verb that does. `AGENT-GUIDE.md` warned that `link` does
not appear in `related` without ever naming the verb that shows it, which is
how an agent following the guide gets a false negative from the recommended
check. The guide now names `links`, and the observation that Void Core's
`(no neighbors)` is misleading when the ref names a rune with edges went
upstream as `MESSAGE_FOR_VOIDCORE_hormiga-related-verb-2026-09-02.md`.

Also from A6, the "small things, no ask attached", all three of which had an
ask attached the moment anyone looked:

- **`image_grid.columns` was declared, labelled `combo:2,3,4`, offered in the
  inspector, and read by nothing.** Layout came entirely from the stylesheet's
  `auto-fill`, so getting one album cover to fill its column meant discovering
  empirically that `span 4` yields one tile and `span 5` yields two. The
  report's sentence for it is the one to keep: *a field that does nothing is
  worse than no field.*
- **`og:title` was the page title**, so a home page shares to social as "Home".
  There is a share title on `page` now, and an unset one falls back to
  "<page> - <site>" rather than the bare word.
- **`divider_style` gained `bar`** — a validated coloured swatch strip. A
  brand's repeating device and an organization's banner stripe are the same
  request. The colours are validated rather than escaped, because there is no
  such thing as a safely escaped arbitrary CSS value.

## The language decision

The report's A3 asked for `config set site.languages 'en'` — a way to build one
language, on the grounds that a solo artist with no Spanish copy gets a nav
button promising Spanish and a sitemap of duplicate pairs.

**The author declined it**, and the reasoning is now a commitment rather than a
preference:

> instead of opting to NOT have multi language, the ask should've been "have
> better and more robust translation tools", so its easier for a site to be in
> english and spanish. … the website itself should still prioritize multiple
> languages. We don't want to be lazy in our development. We want MORE
> features, not less.

Two things followed from taking the evidence seriously and the remedy not:

1. **The duplicate-pair worry was already answered and the report did not know
   it.** Every page carries `<link rel="alternate" hreflang="…">` in both
   directions plus `x-default`, which is exactly the mechanism search engines
   document for one page served in two languages. It is not an SEO liability;
   it is the SEO answer.
2. **The real defect is that nobody is told.** `text()` falls back from
   `title_es` to `title_en` silently, so a site can be 0% translated, render
   `ok`, publish, and put a Spanish URL in front of a Spanish-speaking reader
   with an English page behind it — the same class of silence as D6.

So: a render now reports how much of the page was written in the language it
was asked for, and **`effect translation-report [lang]`** writes a *replayable
script* rather than a list — every gap as a `set <rune> <field>_es '<the
English text>'` line with the source text already in place, grouped under the
`use <mantle>` that makes it apply. A translator edits the right-hand sides and
replays it with `--script --atomic --actor`, so translating is a logged,
attributed, replayable batch like every other change. See `src/app/translate.cpp`.

## GitHub Pages

Phase E has said "deploy holidays (folder, GitHub Pages)" since the roadmap was
written, and `hol_github` has been in the Antfarm palette — a label, a colour, a
`site` input port and a `repo` field — with nothing reading any of it. That is
D6's and A6's lesson at the scale of a whole holiday.

`src/publish/github.cpp` speaks the Git Data API: blobs, a tree, a commit, a
ref move. Notes worth keeping:

- **No `base_tree`.** With one, a publish is a patch over the last one and a
  page deleted from the model stays live forever. Without one the tree *is* the
  built folder and the deploy is a mirror — which is what `site/` already is of
  the database.
- **`.nojekyll` goes in the tree, not in `site/`.** GitHub runs Jekyll over the
  branch otherwise and silently drops every path beginning with an underscore.
  It is a property of this host, so a Cloudflare deploy has no business carrying
  it.
- **`CNAME` is written whenever a `hol_dns` node is wired in.** GitHub reads
  that file, not an API field, as the authority on the custom domain — so a
  deploy that omits it silently unsets a domain configured in the web UI.
- **The commit sha is the `vendor_id`.** Rollback is a force-update of the ref
  back onto it and uploads nothing. On Cloudflare the id has to be recovered
  from a preview URL; here the history Hormiga keeps and the thing the host
  needs are the same string, which is the shape `web-platform.md` wants from
  every host.
- **It enables Pages on first publish**, which is half of what A5 said
  `deploy-site` could not do. The other half — creating a Cloudflare Pages
  project — is still manual.

Two host glyphs meant lifting `find_host` and `host_token` out of the two
functions that had a copy of each, and that **fixed a real asymmetry rather
than only removing duplication**: `rollback_site` read `token_file` only, so a
host configured the recommended way — `token_key` in the vault — could publish
and could not undo. The one operation an operator reaches for when something
has gone wrong in public was the one that could not find the credential.

**`effect check-host`** is A5's actual ask: the sibling of `check-store` for the
other one-way door. It performs the smallest *real* reads a deploy performs and
never asks a vendor whether a token is valid — the report supplies fresh proof
for that rule, an account-scoped Cloudflare token that answers `Invalid API
Token` to `/user/tokens/verify` while working perfectly against every account
endpoint. It reports several lines rather than a verdict, because "can I
publish?" is four questions with four different fixes and only one of them is
the credential. The GUI's "Test this token" button became "Test this host" and
calls the same verb.

## What was deferred, and where it went

A1 (an `audio` block, a `release` glyph, a `track_list`), A2 (media into the
object store), A4 (a gated block and a little per-visitor state) are design
conversations, not fixes. They are [Q54–Q56](/developer_questions.md) with a
lean on each. The report's own note that a solver counter is analytics — a
different product and a privacy question this project has not asked — is
recorded there too, so nobody builds it on the way to the other two.

## And a second note arrived the same day

While the above was being built, the author left seven items from a hands-on
session with the app: renaming a note, an error linking images, choosing brand
images from the asset library rather than uploading, a Save button in the
Builder, text wrap on the canvas, a deleted page's old URL, and — the big one —
**registering a custom component with the Builder the way a host registers a
widget with Void Maiz.** Six of the seven were built the same day; what each
turned out to be is below.

## The seven items, and what they turned out to be

Six of the author's seven are built. Written up in
[builder-roadmap](/concepts/sections/builder-roadmap.md); the two that are worth
recording here are the ones where the reported symptom was not the defect.

**"Cant rename notes"** was not a missing widget. The Data tab's detail pane has
had a rename box since it was written, and `draw_data_body` deliberately skips
`note` and `rule` runes because both have their own tab — so the one surface
that could rename a rune never showed the runes in question. The control is in
`ui/widgets.cpp` now and both tabs call it. It returns a bool meaning *your node
reference is now dangling*, because a rename reprojects and a caller that keeps
reading `sel` is reading freed memory; making that the return value is the only
way a third caller cannot get it wrong.

**"Cant link images together? or there's a weird error"** was not about images
and the CLI path was fine. The Data tab built its command by string
concatenation, so a relation typed as `goes with` reached Void Core as
`--relation goes` — the link written, under a relation nobody asked for, in
silence — and one containing an apostrophe produced *unterminated quote (SPEC
§6.1)* at somebody who had typed a word into a text box. That is almost
certainly the weird error. Both halves go through `json_arg` now.

**"A deleted page should route to 404"** contained its own diagnosis in a
parenthetical — *"cuz i guess the link still exists"*. `render_site` wrote one
file per page and removed nothing, so deleting a `page` rune took it out of the
nav, the sitemap and the model, and left its HTML in the folder the deploy
uploads. The old page stayed live at its old URL, forever, showing content the
database no longer contained. There was already a themed 404; what was missing
is that **`site/` is a mirror of the document** — the property the GitHub
deployer builds its commit with (no `base_tree`), applied one layer earlier so
it holds for every host.

The other three: brand images are chosen from the organization's own `image`
runes and a browsed file now mints one (an upload that does not become a rune is
an asset the organization cannot find again); the Builder has a Save button with
a live unsaved count, because the writing was never the missing part and being
told was; and documents can be renamed and deleted, the Void Core verbs that
were listed as blocking having landed some time ago with nothing to tell the
roadmap.

**Two files split, on the ratchet's evidence rather than on a hunch.**
`ui/documents.cpp` took document and template management out of `builder.cpp`
(nothing in it draws a canvas or reads a selection — it is the "documents as
projects" enabler, living in the file it was first typed into), and three
controls moved to `ui/widgets.cpp`, whose header has said since it was written
that a widget every section uses belongs to none of them.

## And the direction that came with them

The seventh item is a registry for custom page elements, and the author opened a
second question in the same breath: a registry for custom **data types**.

> Like do we really think that a 3D object type is as needed as a contact,
> event, or image? Contacts, events, and images are super universal. But 3D
> object is a bit more specific. Heck, what if its a unity game showcase, and
> the data type is webgl Unity games? that's super specific!

That reframes [Q54](/developer_questions.md) — the audio block the Click LaFont
report asked for — from *should we add audio* into *is a glyph per medium the
right shape at all*. Both are written up: [Q58](/developer_questions.md) for
elements, [Q59](/developer_questions.md) for types.

The finding worth carrying into that work is that **most of it already exists**.
A glyph declaration is data; placement, tags, undo, merge, `.miga` and replay
are Void Core behaviour that does not know which glyphs exist. The first rung of
both questions is the same one: let a declaration come from a file beside the
database. What is genuinely new is a render per (glyph × domain) with an
explicit *skip* for email, a canvas preview that resembles the output — the same
wall the canvas text-wrap item runs into — and, only for Q58, script on a public
page. That last one is where every refusal in this codebase already points, and
the shape that survives is the one `custom.css` took the same day: a file beside
the database, never a model field, because model data arrives by import and by
sync from a device somebody else was using.

## Void Core answered the same day, and both "neither blocks us" items were worse

`MESSAGE_FOR_VOIDHORMIGA_related-signpost-and-two-holes-2026-09-02.md`, shipped
in **0.2.13**, consumed and deleted per rule 4.

**The signpost is in, worded as we suggested.** `related <rune>` now answers
`(no tag neighbors; 'x' is a rune with 1 link - try 'links x')` — but *only*
where the answer was already empty and the ref names a rune with at least one
edge. They checked the case our own reasoning implies hardest: a rune name that
doubles as a real tag still answers about the tag and never mentions the link.
They declined to make `related` report edges, which is what we asked for. It
still returns `ok: true`, because it found what it was asked for; nothing that
branches on `ok` moves.

**Q50.2 was not "ignores the flag".** `relate <tagA> <tagB> [weight]` takes the
weight *positionally* and there is no flag parsing at all, so `--relation`
landed in the weight slot and reached `atof("--relation")` — **0.0**. The
association was written at weight zero, "not near at all", which in a proximity
graph is the inverse of the intent, recorded as success. A silently inverted
fact rather than a dropped name, and plain typos (`relate a b abc`) went the
same way. It refuses now. **We are not exposed**: nothing in `src/`, the seeds,
the templates, the demo transcripts or the tests calls `relate` at all, and the
one hit is `reyna_import_smoke.cpp`'s list of allowed verb names.

Their framing is worth keeping, because it is a correction to how we reported
it: *"neither blocks us"* was too generous, and a defect we had characterised as
cosmetic was writing wrong data.

**Q50.1 was four verbs, not one.** `link`, `links`, `unlink` and `journal` were
all missing from the flat `verbs` string `--describe` prints — including the
verb we had just told an agent to use instead of `related`. They acted on the
*diagnosis* rather than the symptom: there is nothing to introspect (the router
is an if/else chain across five files with no registry), so instead of
pretending otherwise the list is now **checked** by a CI test that extracts every
verb the families answer to and diffs both directions, phantoms included.

**And one back to us.** They point out that the half of the incident that
actually misled the field agent was ours: their empty answer was ambiguous, but
our guide sent the agent to the wrong verb and told them to expect nothing from
it. They also note that `relate`/`link` have the same shape-confusion one layer
up, with no fix available on their side that would not be guessing at intent —
an agent writing `relate a b` when it means `link a b` gets tag proximity, `ok:
true`, and no signpost. The guide now says plainly that `relate` is about tags
and takes no flags.

Upgrading is drop-in — additive, ABI unchanged. The vendored DLL is picked up by
the ordinary build.

## Sound, and icons that were already in the building

**The `audio` block.** The Click LaFont report's A1 rung 1, and the reason it
went in ahead of the registry questions is that it is the part that is certainly
right: an organization with a podcast, a recorded meeting or a
Spanish-language radio spot has the same absence a music project does, and for
that organization the recording is often the most accessible thing on the site
because it does not require reading.

Two decisions worth keeping:

- **No facade, which is the opposite of `video` and for the same reason.**
  `video` ships a click-to-load facade because a YouTube iframe reports every
  visitor to Google whether or not they press play. Nothing here leaves the
  organization's own site, so the honest thing is the plain element:
  `preload="none"` fetches no audio until somebody presses play — the same
  promise, kept by the standard rather than by our JavaScript — and a native
  `<audio controls>` works with scripting off, which after the `.reveal` finding
  earlier the same day is a property to choose on purpose.
- **`src` is a file and not a data rune, and that is a decision with a
  deadline.** There is no `audio` glyph to tag, query or link to the event it
  was recorded at, because [Q59](/developer_questions.md) asks whether a glyph
  per medium is the right shape at all. Adding one now would pre-empt that
  question in the direction it argues against. The block that plays the file is
  certainly right; where the file's metadata lives is still being decided.

The MIME tables in `publish/cloudflare.cpp` and `platform/preview_server.cpp`
both learned audio at the same time. Neither knew what a `.mp3` was, so the
block would have played locally and offered a download on the deployed site —
the worst place to find that out.

**Icons, and nothing was downloaded.** The ask was *"if we can download some
icon assets from somewhere"*, and the answer is that both halves were already
vendored, each for the surface it suits: **Font Awesome 6 Solid** merged into
the ImGui atlas (right for a GUI — one glyph, one draw call, inherits colour and
DPI) and **Lucide** as SVG path data for the web renderer (right for a page,
where an icon font blocks first paint, renders as a box when it fails, and is
invisible to a screen reader). Font Awesome had been loaded since the map
markers shipped and used for nothing else.

So `glyph_icon()` is one table in `app/app_internal.hpp` rather than a tenth
parameter on `block()` — an icon is a property of THIS front-end, and putting it
in the glyph declaration would push a desktop concern into the model that the
web renderer, the newsletter and every headless run have to ignore. An unlisted
glyph falls back to a neutral square, because a palette where some rows have an
icon and some have blank space reads as broken rather than as sparse.

The one mechanic worth remembering: window titles are
`ICON " Name###Name"`. ImGui hashes the part after `###`, so every window kept
the id it has always had and nobody's saved dock layout moved. Without it,
adding an icon would have silently renamed four windows and reset the workspace
of everyone who upgraded.

Still owed: the Antfarm and Allomone palettes are drawn by Void Maiz
(`maiz::edit_canvas`), so icons there are an upstream ask rather than something
to patch in here.

## The portfolio client, and the evening the app would not open

A third client, and the difference that produced every finding: **one person
publishing about themselves.** LON is a network, Click LaFont is a brand; both
are organizations. Five pages, 62 blocks, deployed to GitHub Pages — and
`check-host` passed all four checks on the first try, which is the first
independent use of the day's earlier work.

### The crash, which was ours twice over

The author, in the middle of it: *"i can't open hormiga. i double click the
desktop shortcut … it briefly opens for a bit (with the terminal) then it
closes."*

`std::clamp(v, lo, hi)` has a precondition — `hi >= lo` — and violating it is UB
that libstdc++ with assertions turns into `abort()`. `draw_data_section`
computes two pane widths as *at least this readable, at most this share of the
space*, and both bounds cross in a narrow window: the first below 275px, the
second whenever the remaining width is under 270px, **which its own 160px floor
guarantees**. The pattern is the natural way to write that constraint, it is
everywhere in layout code, and it is a hard abort rather than a visible glitch.
`clamp_fit` now takes `lo` when the bounds cross, on the reasoning that a
minimum readable size that overflows can still be read and dragged back, while
taking `hi` collapses the pane to nothing — which looks exactly like the crash
it replaced. A third crossing site in the calendar (an event longer than the
visible hour range) was found by the same grep and fixed.

**Why it fired that evening was the second half, and that half was mine.** The
same day's icon work put icons on window titles as `ICON " Data###Data"`, on the
reasoning that ImGui hashes what follows `###` so the window id would not move
and no dock layout would either. The id was right and **the reasoning was
wrong**: `imgui.ini` keys settings by a different hash —
`CreateNewWindowSettings` skips to the `###` marker and hashes from *there*, so
a saved `[Window][Data]` is `hash("Data")` while a window named
`"X Data###Data"` looks itself up as `hash("###Data")`. Every saved entry was
orphaned. Position, size and dock assignment gone, silently, and the Data
section came up narrow enough to cross the clamp.

The titles are plain names again. Icons stay everywhere they cost nothing — the
Builder palette and its drag ghost, the document toolbar, the Data "+ New" menu,
the Notes tab. **Putting one on a window title needs an `imgui.ini` migration
first**, and that is a deliberate piece of work rather than a decoration; the
note lives at the call site so nobody re-adds it casually.

Two lessons worth separating. The clamp was a latent crash any narrow window
would have found, and it was not mine. The rename was mine, and its shape is the
one to remember: **a change that "keeps the id stable" is a claim about two
hashes agreeing, and I checked one of them.**

### The `download` block

The portfolio's blocking gap, and their diagnosis was unusually complete: three
of the four pieces already existed. `link` emits the right markup,
`stage_site_asset` is type-agnostic and would have staged a PDF correctly, and
`resource` (`path`, `topic`) was declared, editable in the GUI, and **rendered
by nothing** — the third instance of that trap in a week, after
`image_grid.columns` and the whole of `hol_github`.

Their four leans were all taken, and two are worth recording:

- **`file` accepts either a path or a `resource` rune name.** One field, two
  forms, and the dead glyph becomes reachable without inventing a second block —
  the way `audio.cover` already resolves an `image` rune name.
- **`.html`, `.svg`, `.js` and friends are refused, out loud.** They raised this
  against their own proposal, and the comparison they drew is exact:
  `custom.css` is a file and not a model field precisely because model data
  arrives by import and by merge, and a merge that can publish an executable
  page on the organization's own origin is a hole. The refusal is a closed list
  of dangerous extensions rather than an allow-list, because an allow-list would
  refuse the `.zip` of photos an organization actually wants to publish.

**No clearance gate, deliberately.** `directory` publishes a query result so it
must ask permission per person; this publishes the one file an author pointed
at, and their sentence for it is the right one: *naming the file is the
consent.* A `download_grid` over `resource` runes would inherit `directory`'s
question and can arrive later with that conversation attached.

A résumé surfaced it; the thing is general. A flier PDF, the bylaws, an annual
report, a know-your-rights sheet, a printable calendar — the documents an
outreach organization is most often asked for, and until now the answer was to
host them somewhere Hormiga does not manage.

### `site.languages`, which was accepted, stored and ignored

The trust bug, and it is partly a documentation-shape failure of mine.
`app/translate.cpp` opens by QUOTING the Click LaFont ask — `config set
site.languages 'en'` — in order to record that the author declined it. An
unlabelled blockquote reads as a contract, the portfolio agent set the key, Void
Core's free-form `config` accepted and returned it, and nothing anywhere read
it. Their framing is the one to keep:

> A config key that accepts, stores and returns a value it does not act on is
> worse than a missing one, because the read-back confirms it.

The feature is still declined, so the fix is that the key stops pretending:
`render_site` warns once, names the decision, and points at
`effect translation-report`. The quote in `translate.cpp` is now labelled as
declined. Void Core's config cannot know which keys an application honours — the
check has to live at the seam that would have honoured it, which is why it sits
in `translate.cpp` rather than in the renderer.

### What was recorded rather than built

`role` before `project` ([Q60](/developer_questions.md),
[Q61](/developer_questions.md)), on the report's own ordering: *"`role` removes
a parser that will break; `project` removes labels that are merely wrong."* The
résumé builder currently parses work history back out of `narrative` prose with
a line-shape convention — a parser over presentation, which inverts this repo's
first ground rule and breaks the day a person edits the block by hand.

And one diagnostic worth stealing, from A2: **a privacy control that is
meaningless for the data it guards means the data is not what the block is
for.** `directory` refusing to publish a Blender project without
`clearance:public` is not the gate misbehaving; it is the gate correctly
reporting that the block is being borrowed.

## 2026-09-03 — the portfolio's second round, and four asks that were already on the list

Five of the seven items were built the day they arrived. What makes this round
worth its own entry is that **most of it was already somewhere in the OKF**, and
the report is what made the connections legible.

### D2 — a bundle that was correct, and a site that was correct, and a broken combination

The worst-shaped bug in a while, and the report's own sentence for it is the one
to keep: *"the site was correct, the bundle was correct, and the combination was
broken."*

`download.file` is documented as "a path relative to the database", so they put
a resume in `resume/` beside the database. The render staged it, the deploy
published it, and `effect pack-database` wrote a `.miga` that **silently did not
contain it** — because `pack()` bundled the assets folder and nothing else.
Opened anywhere else, the block reported "This file is not available", correctly
and unhelpfully. Every individual step said `ok`, and the AGENT-GUIDE's own
table *documents* that only `assets/` travels — so nothing was wrong except that
no one connected the rule to the block that invites you to break it.

`pack()` now takes every file the MODEL points at, and the interesting part is
where the list comes from. Not a hardcoded set of field names — that is exactly
the trap this codebase has paid for three times (`image_grid.columns`,
`hol_github`, `resource` were all declared-and-read-by-nothing) and pointing it
at data loss would be worse. It asks the **glyph declaration**: a projected
`SceneField` carries its `editor`, so a field an operator picks a file for is
declared `path`. Add a glyph tomorrow with a `path` field and it is bundled
without anybody editing that function.

**And a credential does not come along.** `hol_static_host.token_file`,
`hol_object_store.secret_file` and `hol_sqlite.file` are all paths beside the
database, and a `.miga` is *not encrypted* — its own docstring says it carries
the organization's data in readable form. None of them is declared with a `path`
EDITOR, because an operator types those through the Antfarm rather than through
the widget registry, so asking the declaration is what keeps them out. That is
not luck but it is not a guarantee either, so there is a second lock by file
shape (`.key`, `.token`, `*secret*`, …). Both would have to be wrong at once.

### A5 — the bilingual commitment, one layer down

The sharpest argument in the report, and it is ours quoted back at us:

> Given the position you took declining `site.languages` — that Spanish is not a
> translation of the site, for many readers it *is* the site — this is the same
> commitment applied one layer down.

`organization` declared `bio` and no `bio_es`; `image` declared `description` and
`alt` and no `_es` for either. So `directory` and `image_grid` — the two blocks
that put an organization's own **content** on a page — published one language of
prose whatever page they were on, and `translation-report` called the site 96%
done while every project description printed in English on the Spanish page.
The one text a visitor came to read was the one text that could not be
translated.

The rule that resolves it was already written in the guide: *per-language
THINGS get sibling runes and a `lang:` tag, per-language STRINGS on one thing get
the `_en`/`_es` suffix.* A description is a string on one thing. `bio_en`/
`bio_es`, `alt_en`/`alt_es`, `description_en`/`description_es`, with the legacy
field still read last so no existing database loses a word, and one shared
`lang_text` helper so `published.hpp` and the renderer cannot drift.

**And the warning can be cleared now**, which was the other half. An email
address has no Spanish, so counting `label_es "someone@example.org"` as
untranslated made 100% unreachable — and their observation is the one that
mattered: *"a warning that cannot be cleared is a warning people stop reading."*
That costs every other warning this renderer prints, several of which were
expensive to earn. `untranslatable()` excludes addresses, URLs and
number-shaped values; a rune tagged `lang:none` opts out wholesale. A site whose
only untranslated values are a phone number now renders in silence.

### A6 and A9 — one field each, and each deletes a client's CSS

`hero` had `image`, `image_filter`, `image_dim`, `band_image`, `band_bg` — **every
one of them the background** — so a portrait put there was cropped to a 340px
band. `hero.portrait` is a round inset in front of it, which is the shape
`.card.person` already draws for a contact: a pattern the codebase had, on a
block that lacked it. It replaces 23 lines of their `custom.css`.

`narrative` rendered as one `<p>`, so a role, an employer, a date range and four
bullets were one paragraph at one weight. Their workaround was
`::first-line{font-weight:800}` — *"the first line of this paragraph is secretly
a heading."* `heading_en`/`heading_es` says it instead. Line breaks (2026-09-02)
had made the structure expressible and left the hierarchy not.

### Where this lined up with what was already planned

The author asked for exactly this reading, and four of the six absences turned
out to be things already on the list:

| the report's ask | what it meets |
|---|---|
| **A10** a glyph registry | **[Q59](/developer_questions.md)**, opened by the author the day before. Same question, from a client instead. |
| **A7** materials and gradients | **`🔨 presets`** in [builder-roadmap](/concepts/sections/builder-roadmap.md), started in July and never finished — now [Q63](/developer_questions.md). |
| **A8** a card that opens | `event_grid`'s `detail` axis, which already exists at the block level; `directory` has `display` (layout) and no depth axis. |
| **A9** heading hierarchy | **[Q60](/developer_questions.md)** (`role`) arriving as a visual complaint. |

Two of those collapse further. **A10's `record_grid` idea is what unblocks
Q59**: the hard half of [Q58](/developer_questions.md) is that a block needs a
renderer per output domain, and a *generic* grid over a declared type needs one
renderer total — so the data-glyph registry can ship without opening the
block-registry door at all. And **A7's preset-as-authored-bundle wants the same
loader** the registries want: a declared thing that ships beside the database.
Building either one carelessly builds half of the other badly.

They also supplied the constraint that makes a registry safe, unprompted, and it
is now recorded in Q59 as a hard rule: **the privacy seam must not become
configurable.** `directory`'s clearance gate is trustworthy *because* it knows it
is publishing people; a generic grid over user-declared types cannot make that
promise, so a declared glyph must never flow through `directory`.

### What they are owed

An answer, not a feature. Their closing question — *is a declared data glyph a
thing Void Hormiga wants to have, or does the glyph set grow by hand?* — is a
decision only the author can make, and it has now been asked twice in two days
from two directions. They maintain workarounds that are written differently
depending on whether they are temporary, which is a fair reason to want to know.

## 2026-09-03 — what a glyph actually is, and a message that is architecture

A long design conversation with the author, sent upstream as
`MESSAGE_FOR_VOIDCORE_hormiga-rune-kinds-and-the-glyph-split-2026-09-03.md`. It
started from [Q59](/developer_questions.md) — may an organization declare its own
record type — and did not stay there, because the honest answer turned out to
depend on what a glyph *is*.

**The finding: `glyph` is doing two unrelated jobs.** It is the SCHEMA — which
fields exist, what shape `content` has, a fact true in every mantle and every
context — and it is the PRESENTATION — `hints.color`, `face`, `ports`,
`editors`, `labels`, plus our own `else if (n->glyph == …)` chains, which are
facts about a rune *in a modality*, one per modality.

The conflation was natural: for every application built so far there is exactly
one presentation per type per surface, so the two are in bijection and the
distinction never pays rent. It stops working the moment a rune has more than one
representation — a sprite, a sound, a card, a paragraph in an email — because
then the presentation is not a property of the rune, it is a **function from the
rune to a modality**. And once it is a function it can be *derived* rather than
authored, which is the author's original intent for the whole structure: a rune
is the representation-independent thing, and a glyph is what some renderer —
eventually a model — makes of it.

**This changed the Q59 recommendation's reasoning, and strengthened it.** A
hand-written renderer per type is a stopgap standing in for a generative one, so
hand-adding `project` with bespoke render cases deepens the stopgap. A generic
`record_grid` is a rendering *derived from the declaration* — a dumb deriver in
the right shape, swappable later. It is not a compromise made to keep the
registry cheap; it is the only option pointed where the architecture is going.

**Three kinds of rune** ([Q64](/developer_questions.md)). Entity, action,
attribute — because *"superman flying across the sky"* is two entities, an
action and a domain, and Void Core can express only the entities. The argument
that carries it is **arity**: an edge label can express only a binary relation,
and that sentence is ternary. Reifying the verb as a node with typed ports is the
standard answer (RDF reification; neo-Davidsonian event semantics) and is exactly
an interaction net agent — the model Void Maiz already implements. Not a new
mechanism; the existing one applied to verbs instead of only to nouns.

The author's first naming — Gamma/Delta/Epsilon — was withdrawn on evidence:
`../VoidMaiz/include/voidmaiz/reduce.hpp` already uses those letters in Lafont's
own sense *about glyphs*, and ε is the arity-zero **eraser**, close to the
opposite of "a concept carrying a value". "Verb" is likewise taken by the
dispatcher. Suggested `entity` / `act` / `measure`.

**Values on edges** ([Q65](/developer_questions.md)). When an edge points at an
attribute rune, the weight is the value: `player --[5.0]--> speed`. The attribute
rune supplies the **unit**, which is the strongest thing about it. The author's
criterion for when to use it is the useful part — *how much the number actually
interacts with the mantle* — sharpened here to: **if a number is read by rules
that produce new structure it belongs on an edge; if it is read only by renderers
it belongs in a field.**

**Hormiga's answer is no, for a mathematical reason rather than a conservative
one.** A weight is a magnitude; a date has no magnitude, it has a position. Dates
and coordinates are **points in an affine space** — subtract two to get a
duration or a displacement, add one of those to a point, but never add or scale
two points — while health and speed are **vectors**. That is exactly why "half of
September 3rd" is meaningless while "a quarter past twelve" is fine. Most of
Hormiga's numbers are points, so most of Hormiga keeps fields.

**A belief of ours that was wrong, corrected here.** We had assumed Void Core
stored numbers as text. It does not: `verbs_edit.c` parses every `set` value and
stores a JSON *number* when the token parses completely. The stringiness is our
own read side — `field_value()` stringifies and `doc_field_int()` re-parses. That
is one accessor to fix locally, not an upstream overhaul, and no change to number
storage was requested.

What we asked for instead is that a value **say what it is**: a field-level
annotation naming its measurement level (nominal / ordinal / interval / ratio)
and its unit. Four different things are all called "number", distinguished by
which operations are legal, and nothing in the model currently records which.

The two older asks went in the same message: glyph descriptors as a documented
host contract (which deletes our hand-maintained `glyph_fields()` duplicate), and
**glyph declarations living in the state document** — the one that actually
blocks Q59, because descriptors live on `VC_Manager` and a `.miga` therefore
carries runes without their meaning.

**Where the weight of that message actually is, on the author's correction.** The
first draft measured every ask in diff size and kept calling them small, which
understated it: the code changes really are additive, and **what is being asked
for is a moderate rethink of Void Core's own OKF.** Their `concepts/glyph.md`
opens with the conflation verbatim — *"it declares the rune's content fields,
which editor drives it, and how to summarize it"*, one schema clause and two
presentation clauses — and `concepts/rune.md`, `concepts/links.md`,
`concepts/tag-system.md` and `interaction-nets.md` each carry a sentence this
changes. The message now names those pages and separates the two costs in its own
table, because a small diff landing against documentation that says otherwise is
the worse outcome.

We also offered to draft a **quantity** concept page for them — measurement
levels, points versus vectors, units — since no such page exists and we worked
the material out for our own dates and coordinates. Offered as a message for them
to take or discard, not as an edit: ground rule 4 runs both directions.

## 2026-09-04 — Void Core answered all five, Void Mago tried to ship us, and a font path that was never resolved

Two messages arrived and both were consumed here (ground rule 4: the log is the
durable record, not the message file).

### Void Core 0.2.14 — all five asks shipped, one name declined on our evidence

`MESSAGE_FOR_VOIDHORMIGA_voidcore-all-five-shipped-0.2.14-2026-09-03.md`,
answering our message of the day before. Everything in
[the 2026-09-03 entry](/log.md) is now built: `glyphs` as a documented host
contract, `state.glyphs` + `glyph declare`, field-level `kind`, `kind` on the
descriptor, and attribute assertions with the `values` verb.

They varied us in three places and each variance is better than the ask:

1. **`glyphs` grew a resolver**, so a host reads one shape whether a descriptor
   was declared or registered, with `fields`, `kind` and `source` always
   present. `source` is what makes a declaration shadowing a registration
   visible rather than silent.
2. **Declarations joined the undo slice** (`mantles + active + glyphs`). Their
   reasoning: a schema is *authored content*, so a rune must never survive an
   undo that removed the declaration explaining it. The cost is O(the type
   vocabulary), not O(the data).
3. **The unit lives on the RUNE, not the glyph** — a new `measure` verb writing
   a `quantity` object beside `content`. We had written that the attribute rune
   supplies the unit and then asked for it on the glyph anyway; `health`,
   `speed` and `strength` share one schema and differ only in what they
   measure, so a glyph could not have carried it.

**Q64 and Q65 are answered and cleared from
[developer_questions](/developer_questions.md).** The naming is
`entity` / `act` / `measure`, taken with our reasoning — γ/δ/ε was declined
because Void Maiz already uses those letters in Lafont's own sense *about
glyphs*, and ε is the arity-zero eraser. They also declined the one thing we
suggested that would have quietly broken something: kinds are **not** a reserved
`kind:<k>` tag, because `kind:` is already an ordinary application namespace and
reserving it would have changed what every existing `kind:vegetable` tag
matches.

They offered to take our **quantity** page and wrote it themselves —
`../VoidCore/okf/concepts/quantity.md`. The offer was the useful part.

### What we did with it

- **`hormiga::glyph_fields()` is deleted**, which was ask 1's whole point. It
  was a hand-maintained second copy of every `"fields":[...]` in
  `register_glyphs`, and it was **already wrong**: `job` appeared twice, so the
  first arm won and answered `{org, deadline, url}` for a glyph that declares
  twelve fields and no `url`. A CSV of jobs mapped two real columns, wrote a
  field nothing declares, and reported that `pay`, `location`, `description`
  and six others were "no such job field" — a wrong answer delivered with a
  diagnostic. Nothing could have caught it, because a duplicate of a
  declaration never disagrees with the declaration; it disagrees with the other
  duplicate. It now reads `glyphs <name>` from the core.
- **`hormiga::declare_glyph()`** is a one-line helper and it earns its keep for
  the reason Core's Python binding grew the same thing: `register_glyph` takes
  JSON across the C ABI where a string is a string, but `glyph declare` is a
  *dispatcher command*, so the descriptor is one SPEC §6.1 argument. A label of
  `"Garden bed"` pasted in raw is three arguments and a refusal. It calls
  `maiz::arg` — Core's own exported encoder — never a quoter written here.
- **Their one question, answered with a test rather than a reading.** They
  asked whether Hormiga ever *reconstructs* a state document rather than
  round-tripping it, because reconstruction drops `glyphs`. Reading the code
  says we round-trip (`Storage::save` stores `export_state()` verbatim; the
  Palabra merge replaces only `mantles`). "We currently round-trip" is a
  property of today's code and a dropped declaration does not error — it turns
  an organization's own record type into runes nobody can read. So
  `tests/spine_smoke.cpp` now declares a glyph, saves through SQLite, reopens on
  a **core that registers nothing**, and asserts the fields come back and the
  descriptor is stamped `document`. It also asserts the other half: a merely
  *registered* glyph must NOT travel.
- **Five glyphs gained `"kind":"act"`** — `statement`, `revision`,
  `submission`, `deployment`, `incident`. Each records a happening whose subject
  is another rune, and each was already reified for Q64's arity reason. `event`
  deliberately is not one: an event here is the thing a person attends, and its
  fields are read by renderers rather than filled as the roles of a verb.
  Marking it would be reading the English word rather than the model. `measure`
  is unused, which is Q65's answer standing: Hormiga's numbers are points.
- `link --weight`'s new refusal touches nothing — we never call it. The floor in
  `void.json` moved to `>=0.2.14`, because a 0.2.13 core answers `glyphs` with
  an empty descriptor rather than an error, which is the shape of failure a
  floor exists to refuse.

### Void Mago — two blockers, and the half of updating that is ours

`MESSAGE_FOR_VOIDHORMIGA_mago-shipping-and-the-update-client-2026-09-04.md`.
Mago staged this repository for the first time and refused to produce a package.
Both refusals were right and both fixes are here. The whole distribution story
is now [a concept page](/concepts/platform/distribution.md).

**`libwinpthread-1.dll`.** Both executables imported it. It lives in the ucrt64
toolchain directory and on no other machine in the world, and **no manifest
could have mentioned it** — it is a fact about how the binary was linked, not
about what the project declares. Found by reading the PE import directory, which
is a check nothing here does. `-static` under MinGW; `objdump -p` afterwards
names only Windows system DLLs and `libvoidcore.dll`.

**`fonts/` was promised and did not exist**, and fixing it found a third
blocker nobody had reported. `ships_beside_binary` resolves against the
checkout; there is no `fonts/` in this repository. But `desktop.cpp` had been
loading Lato, JetBrains Mono and Font Awesome from
`current_path()/vendor/fonts` — and an installed copy has no `vendor/` and is
not launched from a checkout. **Every `if (exists)` would have fallen through to
`AddFontDefault()`**, so the first device this was ever tested on would have
opened a window in a bitmap face with **no icons at all**: every section tab,
toolbar button and map marker is a Font Awesome codepoint, and a merge that
never happened draws them as blanks.

That is August's empty `site/fonts/` again, in the other front-end, and it is
worth stating as a rule:

> **A path that resolves on the developer's machine because two folders happen
> to be the same folder is not resolved; it is unfalsified.** `ship_dir` and
> `base_dir` are equal in a build tree and different everywhere else.

The whole vendored family is now staged as `vendor/fonts/` — the name it
actually has — so the build tree and the install tree have one shape, and
`fonts/` beside the binary stops colliding with `fonts/` beside the *database*,
which `void.json` declares as the organization's own faces.

`mago stage voidhormiga --platform windows-x64` now reports six files, no
unresolved imports and nothing missing.

### The update client

Built, in three pieces with one decision-maker.

- **`src/update/`** holds every decision and links the standard library,
  libsodium and the vendored JSON — no window, no Void Core, no session, no
  org. `tools/check_layering.py` enforces that, and the reason is not tidiness:
  *the one machine you cannot attach a debugger to is the one an update broke*,
  and the ordinary reason somebody wants a newer Hormiga is that this one will
  not open their database.
- **`voidhormiga-cli update`** runs in `main()` before any session exists — the
  same argument as `--restore-backup`: **a recovery tool that requires a
  working system is not a recovery tool.**
- **`src/ui/updates.cpp`** contains a thread, a modal and a settings block, and
  no decisions. Both surfaces print `update::describe()` on the same offer, so
  the window and the terminal say the same words.

`void.json` said *"no silent updates: the user is told an update exists and
chooses it"*, and taking the first half seriously is what shaped the design. **A
check is a network request a person did not make.** So the preference starts at
`unasked` and a fresh install's first update-related event is a question about
*the check*, before one exists. Two things fell out of that:

1. **The preference is not `config set`.** Everything else in Settings is
   config-tier and rides the saved org — so "don't ask me about updates" would
   have been attached to whichever database was open and would have travelled to
   another device on the next merge, answering a question that device was never
   asked. It lives in `%LOCALAPPDATA%/VoidHormiga/updates.json`, the suite
   folder, which is also the parent of each side-by-side version folder, so the
   answer survives the update it was given for.
2. **A near-miss, recorded because it was one line from shipping.** The obvious
   guard for "is a person looking at this?" was `on_shell_capture` — and the
   headless front-end sets it too, building a throwaway `HormigaApp` to render a
   newsletter from. An agent asking for a newsletter would have made a network
   request. It is an explicit `offer_updates` flag that only the desktop shell
   sets.

`tests/update_smoke.cpp` runs with no network at all. Most of its cases are the
*wrong* inputs, because every failure this client can have is silent: a version
compare that reads `0.1.10` as older than `0.1.9` does not error, it just means
nobody is ever told. A login page where a feed should be prints "up to date". A
mismatched installer left on disk is how a bad download gets run a week later by
somebody who found it in Downloads — so the digest check deletes the file.

`void.json` gained the `release` block Mago asked for. Without it a prompt can
only say "a new version is available"; with it, it says what changed and who it
affects — and `behavior_changes` is exactly the category **a version number
cannot express and a dependency resolver cannot see**.

### What is not proven, said plainly

`makensis` has not been run, no second machine has installed anything, and
nothing is signed. `voidhormiga-cli update --check` reaches GitHub and gets a
404, which is the correct answer while no release exists — a real exercise of
the transport and the status handling, and no exercise at all of a published
feed. The exit test is the same shape as collaboration's: **a second computer.**
The author's instruction was to get the installer ready and not launch it while
Allomone and Palabra are still moving, and that is exactly where this stops.

### One thing that is not ours

`ctest` reports `reduce_conformance` failing 8 of 25 cases. It is **Void
Maiz's** test, registered into our build by `add_subdirectory`, running Void
Core's reduce corpus. That corpus grew boxes, adapter ports and a `patch` rule
through Core 0.2.8–0.2.12; `../VoidMaiz/src/reduce/reduce.cpp` is untouched
since Maiz's initial commit. It is drift between two siblings, it fails our
build for a reason that has nothing to do with us, and per ground rule 4 it is
reported rather than patched. Every one of Hormiga's own 33 tests passes.

## 2026-09-05 — the download page, planned rather than built

The author asked for a plan for the other end of
[distribution](/concepts/platform/distribution.md): a stranger, a web page, a
button. Written as [download-page](/concepts/platform/download-page.md) and
deliberately tagged `status:proposed` — nothing on it exists yet, and a concept
page that reads like a record when it is a proposal is how an OKF starts lying.

It is addressed to two readers at once, which is unusual here and was the ask:
the reasoning is written out for the author, and §6 is a build brief an agent
can run.

**The finding that made it worth writing down.** The author's instinct was that
downloads should be simple, *"but due to the nature and structure of the void
core, it's possible"* they are not. That was right, and the specific place is
narrower and more interesting than expected:

> The website is GENERATED FROM THE DATABASE, so a download button is a rune
> with fields. Which means **a version number on the download page is data, and
> data goes stale** — the page, the update feed and `void.json` become three
> places one number lives, and only one of them is checked by anything.

**The fix is not to solve it but to remove it.** Attach a second, byte-identical
copy of each release's installer under a version-less name, and
`releases/latest/download/VoidHormiga-windows-x64-setup.exe` is permanently
correct. The page then states no version at all, cannot be stale about one, and
**a new release never requires redeploying the website.** The versioned asset
stays attached beside it, so a specific build is still citable.

**The blocks needed already exist**, which was worth checking rather than
assuming — the author's question was whether Hormiga can even put a download
button on a static site.

- `link` (`target` takes a page slug **or an external URL**, `link_style
  button`) is the installer button.
- `download` (2026-09-02, from the portfolio field report) stages a named file
  into the site, emits `<a download>`, and prints its type and size. It is right
  for `SHA256SUMS.txt` and for PDFs.

`download` is deliberately **not** the installer's route, and the reason is
hosting rather than capability: release assets sit outside git's object
database, while a 25 MB binary in a Pages branch is in that repository's history
forever, once per release — and `hol_github` base64-encodes every file into a
JSON body, so it would be a ~33 MB request on every deploy of the whole site.

**Two things the page states because the application already does.** Nothing is
signed, so SmartScreen will warn; the page has to say what Windows will say
*before* Windows says it, in the same words the update prompt already uses —
*the checksum proves the download arrived intact, not who built it.* And there
is no macOS or Linux build, which a page should say rather than let a visitor
discover by clicking.

**One HTML fact recorded so nobody rediscovers it as a bug:** the `download`
attribute on an anchor is ignored cross-origin, so a link to GitHub cannot force
a save on its own. It saves anyway, because GitHub sends `Content-Disposition:
attachment`. The result is right and the reason is the header, not the
attribute — which matters the day the hosting moves.

**Left open with a lean, not decided.** Whether the page should state the
current version at all. Lean: no, for the staleness reason above. The third
option — a little JavaScript reading `void-updates.json` at load time — is
named and explicitly *not* improvised: an author-controlled `<script>` is the
thing `download_refusal` and the `custom.css`-is-a-file decision both exist to
prevent, so it would be a block proposal with a design conversation attached,
the way `download` itself arrived.

---

# 2026-09-08 — the page detects your computer, and hides nothing

The author's instruction, in full: *"it should detect the system (linux,
windows, mac), and then provide the correct download for hormiga."* Also, the
same day: **the first download button is going on clicklafont.com**, because
that site is experimental enough to be safe to break — so the first page that
offers Void Hormiga to a stranger is hosted by a client that is not an outreach
organization, and rendered by the same binary it is offering.

## The report that said no, and was right

The Click LaFont agent built the download page from
[download-page.md](/concepts/platform/download-page.md) §6 and then declined the
detection ask, which is the correct answer to a request that cannot be
expressed. Their report is
`okf/reports/click-lafont-download-page-2026-09-05.md`. Three walls, and the
third is the one worth keeping:

1. It needs JavaScript, and there is no seam for author-supplied script. That is
   not an oversight — `custom.css` is a *file* and CSS-only for a stated reason,
   and an OS-detecting `<script>` in a rune field is the same shape as the
   `<style>` field that was refused. **They explicitly did not ask us to loosen
   it.**
2. §5(d) argued against detection on its own merits, and the argument is the one
   D6 and the `image_grid` language filter both taught: silence that looks like
   emptiness.
3. There is one build. Detection today can only tell a Mac visitor there is
   nothing for them, which is a sentence, not a feature.

So the page states all three platforms in three cards, and a Mac visitor learns
the truth without clicking. **Until the capability existed they did not fake
it**, because the fake would have been the thing §5(d) warns about. That is the
second time a client has been more disciplined about this codebase's own rules
than a shortcut would have been.

## What was built: platform sets

Their proposed shape, taken almost unchanged, with one deliberate widening.

- **`download` and `link` gained a `platform` field** — `windows-x64`, `macos`,
  `macos-arm64`, `linux-x64`, or `any` (the default, so nothing changed for the
  résumé and the flier PDF `download` was built for). It is the vocabulary
  `void.json` and `mago plan <app> --platform <name>` already speak, which is
  what makes it compose past the page that asked for it.
- **The widening: `link` carries it too, and had to.** The report asked for
  `download` only. But §3 puts the 7 MB installer on GitHub Releases, so **the
  installer button is a `link`** — a navigation to another origin, not a staged
  file. A `platform` field that reached only `download` could not have served
  the one page that motivated it.
- **A grid row holding two or more of them is a platform set**, decided by the
  renderer at build time and stated in the markup: `class="wrow platform-set"`,
  `data-yours="For your computer"`, `data-platform` per candidate. The badge
  text rides the markup rather than the script because one `app.js` serves both
  the English and the Spanish page.
- **`app.js` moves the matching card first and labels it. It has no branch that
  hides anything.** §5(d) is now satisfied by construction rather than by an
  author remembering it, which is the difference between a rule and a design.
  With scripting off none of it runs and the author's order stands — the same
  guarantee D1's `<noscript>` bought.

Two decisions inside it that are easy to get wrong:

- **Only the OS family is matched, never the architecture.** A browser will not
  tell you what architecture it is on; `navigator.platform` still says `Win32`
  on a 64-bit machine. Matching a family is a guess that is usually right and
  costs a mislabelled badge when it is wrong. Matching an arch would be a guess
  that is often wrong about the one thing the visitor cannot check. The arch is
  carried for the author, the filename and the release.
- **When the guess is unsafe, nothing is said.** Android and Chrome OS both
  report themselves as Linux, and a phone, a tablet and a Chromebook run none of
  these installers — badging the Linux card *"for your computer"* on one of them
  would be exactly the confident wrongness the whole design is arranged against.
  Unsure means silent, and silent means the page as rendered.

A `platform` value the renderer does not know is **reported and treated as
`any`**, because a typo that read as `any` in silence would give an author a row
that *looks* like a platform set and marks nobody's computer. That is the
`image_grid.columns` failure in a shape that matters more.

`src/render/site.cpp`'s budget went 2270 → 2295 for it, with everything that
could leave already gone: the vocabulary, the attribute and the complaint are in
`render/download.hpp`, the behaviour is `app.js`'s, and what is left in the
driver is the part that needs to know what shares a row.

## Three corrections to a page that had never been run

All measured by the client, all folded in:

- **The installer is 7.0 MB, not ~25 MB.** NSIS `/SOLID lzma` compresses the
  46 MB staged tree to 7,327,237 bytes — 15.8%. §3's argument for Releases over
  the site's `assets/` still stands on permanent git history and base64
  inflation, but 25 MB was carrying most of its rhetorical weight, and the page
  now says so about itself.
- **The apostrophe warning was over-cautious.** It read as though escaping were
  unavailable; a backslash-escaped apostrophe inside single quotes works exactly
  as `--describe`'s house rule #3 documents, and ten of them round-tripped
  through it on the live page. The true, narrower claim is that an *unescaped*
  apostrophe ends the argument.
- **§6.2's transcript is runnable**, which it was not when written. It was used
  nearly verbatim. That is the standard a build brief should be held to.

`translation-report` was accepted as the better answer to their withdrawn A3 —
and specifically the warning it prints (*"the es site fell back 90 times out of
90"*), because the silence was the problem, not the second build.

## What is actually blocking the button

**Not us, and this is worth stating plainly because it looks like it is.** There
are zero releases on `migriv24/VoidHormiga`, so the stable
`releases/latest/download/…` URL 404s. The page ships with a sentence where the
button goes, saying so.

And the release is blocked on a real bug that §7 step 3 caught, which is the
argument for that step existing: **Mago's generated `.nsi` writes a
`SetOutPath` with a forward slash in `vendor/fonts`, NSIS collapses it, and the
fonts install into a folder called `vendorfonts`.** An installed Hormiga
therefore has no icons and warns `no webfonts found beside the binary` on every
`render-site`. Diagnosed by rebuilding with backslashes; the write-up went to
the Mago agent. **No release until that lands**, because an install with no
icons is the first thing a stranger would see.

Phase E's exit test is *"a stranger downloads Void Hormiga from a page Void
Hormiga deployed, and it updates itself."* The page half is live and now knows
which computer it is talking to. The other half needs one character in Mago and
a second machine.

## Two messages drafted for the author to relay

Both at the repo root, both uniquely titled per the 2026-07-21 convention, and
both to be folded into this log and deleted when their replies land.

- `MESSAGE_FOR_CLICKLAFONT_hormiga-platform-sets-2026-09-08.md` — the capability
  they proposed, the one place it differs from their proposal (`link` carries
  `platform` too, and had to), the §6.2b transcript, the three corrections, and
  the D6 question back to them: **does their `demo-org` have anything in it?**
  That single answer decides whether the narrow gap above is what they hit or
  whether there is a third thing going on.
- `MESSAGE_FOR_VOIDMAGO_hormiga-installer-paths-and-the-stable-asset-name-2026-09-08.md`
  — the slash, with what Click could not see from outside: that `mago stage` is
  correct because `fs::path` forgives a forward slash and NSIS does not, so **a
  correct staging tree compiles into an installer that puts the files somewhere
  else** and nothing that inspects the tree can find it. Plus the survey that
  explains why nobody hit it: `vendor/fonts/` is the **first nested
  `ships_beside_binary` entry in the family** — every other project declares a
  single top-level segment — so it is the next project's bug as much as ours.
  A chokepoint `nsis_path()` is the suggested fix rather than the two lines.

  A second item is marked an **ask, not a defect**, and declinable: Mago emits
  only the versioned installer name, so the version-free copy the download page
  needs is a human duplicating a file at release time. It is the one unversioned
  manual step in a generated pipeline and its failure mode is the worst
  available — **updates keep working while the website button 404s**, because
  the feed points at the versioned name and only the page uses the stable one.

Recorded as NOT asked for, so it does not come back as somebody else's ask:
`mago feed` already warns correctly about a feed with no hashes and no base URL,
and the sha256 warning is load-bearing at our end — `update.cpp` refuses to
download a release with no usable digest, so a feed built without `--artifacts`
fails closed here and Mago's warning is what explains why.

## D6 is reported still open, and did not reproduce as written

The report says a data mantle not named `demo-org` still renders an empty site
with no warning. Reproduced against the binary, per the rule for reports: it
**warns correctly** — `mantle new clicklafont`, an image rune, `render-site`,
and the output says *"the data mantle 'demo-org' is empty, but 1 image rune(s)
live in 'clicklafont'"*.

What is true is narrower, and is a real gap: `warn_if_data_is_elsewhere`
(`render/assets.cpp`) returns early when the data mantle is **not** empty, so a
database with anything at all in `demo-org` and its real data somewhere else
gets no warning and a nearly-empty site. That is almost certainly the state the
report was written from. Not fixed here — lifting the guard costs one scene
projection per mantle on every render, which is exactly what the guard is for,
so the fix is a design call rather than a line. Recorded so it is not
rediscovered as new.

---

# 2026-09-08, later — Mago fixed it in a day, and we checked rather than believed

The reply to `MESSAGE_FOR_VOIDMAGO_hormiga-installer-paths-and-the-stable-asset-name-2026-09-08.md`
arrived the same day, shipping **mago 0.1.6**, and it opens with *"Item 1 has
landed. Go and release."* The message is folded in here and deleted, per the
convention. What follows is what we verified ourselves, because a claim that
something is fixed is a claim, and this project's rule for reports applies to
replies too.

## The separator: fixed, and checked against OUR manifest

They took the chokepoint rather than the two lines. `src/emit_nsis.cpp` has one
`nsis_path()` and every path written into the script goes through it. Their
argument for **not** normalizing at manifest parse is better than our suggestion
was, and it is worth keeping:

> `ships_beside_binary` entries are *portable* paths, and the declared form is
> correct exactly as written. The error was never in the manifest — it was in
> treating a script as a filesystem. Normalizing upstream would make `stage`,
> `feed` and `wizard` all speak NSIS's dialect to make one of them happy.

They verified by diffing the regenerated script against the hand-fixed one Click
LaFont compiled and installed, and reported it identical. **We verified
differently, on purpose** — a diff against a tested file is a claim about a file,
not about our manifest:

- `mago wizard voidhormiga --platform windows-x64` against this repository's own
  `void.json` now emits `SetOutPath "$INSTDIR\voidhormiga-0.1.0\vendor\fonts"`
  and `File /r "..\dist\voidhormiga\vendor\fonts\*.*"`. Both backslashed.
- Ran their general assertion over that real script rather than the toy: **23
  quoted path arguments, zero forward slashes.**
- Their whole suite passes, `nsis_smoke` included, and their test carries a
  `checked > 0` guard — *"an assertion that inspected nothing passes for the
  wrong reason."*

The toy family now declares a nested `vendor/fonts/` of its own. Our survey
table was what earned that: every declaration in the family had been one
top-level segment, so the fixtures only ever exercised the shape that cannot
break. The toy carries the hard shape now, permanently.

## The stable name: they took option 1 and added the part that matters

`stable_file` is in the feed beside `file`, produced by one rule differing in one
segment so the two cannot drift. They declined `stable_url`, correctly: the
versioned asset and the stable one do not share a base, so a URL composed from
`--base-url` would be confidently wrong. **The feed states the name; the page
that knows its own hosting composes the URL.**

The part we did not ask for is the part that fixes the failure:

> `stable_file` alone would have made the name checkable by a person. It would
> not have made anything *check* it — and your own argument was that the failure
> is invisible from the update side.

`mago feed --artifacts` now hashes the version-free copy. Exercised all three
states here against a stand-in binary: absent → it says so; present and
identical → silence; **present and different → it says the download page is
handing out a different build than the feed describes, most likely the previous
release.** That second one is the case that matters. A stale copy is present and
plausible, passes anything that only looks for the name, and hands every new
visitor the old application while everything reports success.

Option 2 — a `wizard` flag emitting both names, so the last hand-run step
disappears — they deliberately did not build, and the reason is right: *"we
would rather one release actually went through the checked version first, so
that the flag is designed against what the release process turned out to be."*
Ours to ask for again after 0.1.1.

## What we added on our side

`tests/update_smoke.cpp` §10: **the feed `mago feed` actually wrote**, verbatim,
as a fixture. The existing `kFeed` was typed by hand from a message, which is the
kind of fixture that keeps passing after the other side changes.

The assertion that earns it is not *"we read `stable_file`"* — we do not read it
at all. It is that `r.file` is still the **versioned** name. `file` is what an
update downloads and `stable_file` is what a website links, and confusing them
fails in the direction nobody checks: updates would fetch the URL of an asset the
feed does not describe. The stable name is pinned there too, because this
repository is the only place that sees both halves — the feed the application
reads and the button a person types into a `link` rune.

## What is left, and it is not code

Nothing in any repository is blocking. Step 3 needs a second computer and step 4
needs a person to create a release; `download-page.md` §7 is the order, and it
gained an **eighth step**, because none of the first seven tests an *update*.
One release proves the feed parses and says *up to date*. The exit test — *"a
stranger downloads Void Hormiga from a page Void Hormiga deployed, and it updates
itself"* — needs a 0.1.1 for the second half to have anywhere to go.

Mago asked for two things back: the installer's real size from a build we ran
(they have Click's 7,327,237 bytes and say a measured figure is worth more than
an inferred one), and one plain sentence saying the fonts landed in
`vendor\fonts` and the icons are there. Both are owed after step 3.

`MESSAGE_FOR_CLICKLAFONT_hormiga-release-unblocked-2026-09-08.md` went out at the
repo root: their bug is fixed upstream, the family's fixtures are permanently
safer because of their report, and **their button's URL was checked against the
name Mago actually emits, and matches.** Worth running rather than assuming — a
page linking a name the release does not carry fails only for people who do not
have the application yet, which is every visitor the page exists for and nobody
who would notice.

---

# 2026-09-08, third entry — 0.1.0 shipped, and the question of the other two computers

## Void Hormiga 0.1.0 exists

Cut as `v0.1.0` after Mago 0.1.6, with the four assets §3 specifies. The Click
LaFont agent verified it the way a stranger would rather than the way a builder
would, and the numbers are theirs:

- `VoidHormiga-0.1.0-windows-x64-setup.exe`, **7,355,451 bytes**; the
  version-free copy byte-identical, same sha256.
- They followed the URL **the live page actually serves**, not the one in the
  documentation: `http=200`, and the sha256 matches `SHA256SUMS.txt`.
- `voidhormiga-cli update --check`, run **from the installed copy**: *"up to
  date (the feed's latest is 0.1.0)."* That line was an HTTP 404 an hour before.
- The fonts fix verified a third way — they regenerated with 0.1.6, compiled,
  uninstalled the previous install, clean-installed, and ran `render-site` from
  the installed copy. No webfont warning; all seven `.woff2` staged.

**Still owed and still not code:** a second computer, and a 0.1.1 so the feed
proves something other than *up to date*.

## The platform set, measured on somebody else's machine

Six platforms, by stamping a `navigator.platform` override into a copy of the
**real rendered page** immediately before the real `app.js` and letting shipped
code run against shipped markup — spoofing from outside does not move
`navigator.platform`, so a `--user-agent` flag would have tested nothing.

Windows, macOS and Linux each get moved first and badged. **Android and Chrome
OS are correctly silent**, which is the pair worth having built: both report a
Linux platform string and both would otherwise have been told an installer they
cannot run is "for your computer". And in all six, `data-platform` occurs three
times in the post-script DOM — counted, rather than inferred from the absence of
a hiding code path.

## The defect they found, which is a documentation defect

**A set reorders its own row and nothing else.** They built the row from §6.2b
and then put three detail cards on the next row, column-aligned underneath. On
their machine it was perfect, because they are on Windows and Windows was
already first. On a Linux visitor's screen the badge sits over a card describing
a different operating system. Nothing warns and the render log is clean.

It is the `image_grid` language filter and the `demo-org` mantle again:
**correct-looking output that is wrong for somebody who is not you.** §6.2b now
carries the warning as a block quote, because their own analysis is that this is
a sentence rather than a code change — §6.2b's example does not hit the trap,
and that is the shape working rather than luck. The trap only opens when an
author adds explanation, and the honest fix is to say so where they are reading.

**Their ask, which is not taken yet and is the author's call:** `caption_en` /
`caption_es` on `link`, the same two fields `download`, `video` and `image_grid`
already carry. Today a platform card is a label and nothing more — `link` has no
caption, and `download` has one but its `file` is a local path, so it cannot
point at a release. Two fields would let a card say *"Windows 10 or 11, 64-bit,
7.4 MB"* under its own label **and travel with the set**, which is the only
version of the fix that survives reordering. They explicitly did not ask for a
`platform_group` container, and explicitly did not ask for any way to make a
different row follow a set — *"that is a positional coupling between blocks and
it would be a worse thing than the bug."*

## §6.2 was telling agents to undo §4

They declined an instruction from the build brief while building the live page,
and were right. §6.2 said to publish `SHA256SUMS.txt` with a `download` block,
which stages a copy into the site — so it is version-specific and goes stale on
every release, which is exactly the maintenance §4 built the version-free URL to
abolish. They linked `releases/latest/download/SHA256SUMS.txt` instead.

The general rule now stated in §6.1: **`download` is for a file the SITE owns; a
`link` is for a file the RELEASE owns.** Anything that changes when a version
changes belongs to the release.

## D6 is closed, and the hypothesis was wrong for an interesting reason

Their `demo-org` has five runes in it today, which fits the early-return
hypothesis exactly — and that is not what happened. On 2026-09-02 there was **no
`demo-org` mantle at all**: they had deleted the auto-created file before
creating their own state document. The five runes are there now only because
`mantle rename clicklafont demo-org` is how they fixed it.

So they reproduced the original condition against the current binary instead of
arguing from memory, and it warns correctly, naming the count and carrying its
own fix command. D6 closed. **The early-return gap is still real** — leftover
rune in `demo-org`, real data elsewhere, silence — but nobody has hit it, and it
is recorded as a known shape rather than a reported bug.

## Linux and macOS: audited, and the answer is upstream

The author asked for working Linux and macOS builds. Audited rather than
guessed, and the code is far more portable than the platform list suggests:
every Windows-touching file in `src/` carries a `#else` except one, the HTTP
layer shells out to `curl`, and the update client already knows about
`XDG_CONFIG_HOME`, `gmtime_r` and `xdg-open`. Somebody wrote this expecting to
leave.

Fixed here, both of which were wrong regardless of whether a port ever happens:

- **`ws2_32` was linked unconditionally** on both targets, so the first thing a
  non-Windows configure hit was a missing library rather than the real obstacle.
  Guarded. The guard does not create a Linux build; it makes the failure name
  the right file.
- **`HORMIGA_PLATFORM` fell back to `"unknown"` off Windows.** That string is
  the key the update client looks itself up under in the feed's `artifacts`
  object, so a non-Windows binary would have matched no release ever published
  and reported *up to date* forever — a true-looking answer to a question never
  actually asked, which is this project's recurring failure. It is computed per
  platform in CMake now, and the compile-time fallback is
  `"unconfigured-platform"`: obviously broken rather than plausibly fine, the
  same treatment `HORMIGA_VERSION` already had.

**What is left is not ours, and the gate is Void Maiz.** Void Core declares
`windows-x64`, `macos-universal`, `linux-x64`. Void Maiz declares `windows-x64`
and `android-arm64` — and both our binaries link it, the GUI through
`voidmaiz_view` and the headless CLI through `voidmaiz_headless`. There is no
configuration of Void Hormiga that reaches a Linux or macOS desktop without Void
Maiz on it.

Our reading is that their `platforms` array records *what has shipped* rather
than *what compiles*: exactly one file in their `src/` mentions Windows and its
`_WIN32` is guarded, GLFW and ImGui are portable, and `android-arm64` says
somebody has already taken that codebase off Windows. But that is a guess, and
`MESSAGE_FOR_VOIDMAIZ_hormiga-linux-and-macos-2026-09-08.md` asks rather than
assumes, with three acceptable answers spelled out so that *"nobody has ever
tried"* is a first-class one.

Ours that remains, whatever they say: **`src/platform/preview_server.cpp` is
winsock with no POSIX branch.** Mechanical — winsock is BSD sockets with
different init, teardown and error names — but unwritten, and not worth writing
before the view's answer arrives.

**And downstream of all of it, Void Mago's `wizard` emits NSIS and nothing
else** (`--target` is `nsis | plan`), so a non-Windows release would have a
binary, no installer, and no artifact for the feed to point at. Deliberately not
raised with them yet: asking for AppImage or `.dmg` support before knowing
whether the view compiles is asking for work that might have nowhere to run.

**The thing no message fixes:** we have no Linux or macOS machine, and a
*working* build for a platform means it ran there. It is the same
second-computer constraint phase F has been carrying since 2026-08-27, one
platform wider. macOS adds its own: Gatekeeper on an unsigned, un-notarized app
is a harder wall than SmartScreen, and the download page's honesty problem gets
correspondingly bigger.

---

# 2026-09-08, fourth entry — a runner is the second computer

Void Maiz answered the Linux/macOS question with **(2): it should build, and
nobody had ever tried** — and then refused to leave the answer at that. Both
messages of that exchange are folded in here and deleted.

## They were right that our guess was right, and then made it moot

`platforms` was a record of what had been **built and shipped**, exactly as we
guessed. That is now stated in their manifest rather than implied, with a
`platforms_note` — *"a SHIPPING RECORD … not a claim about what compiles"* —
and **we have adopted the same field for the same reason.** Ours says
`["windows-x64"]` and will keep saying it until somebody has watched Hormiga
open a window on another operating system.

The important half of their reply is not the answer, it is the workflow file:

> **A runner is the second computer.** You wrote that you could not verify a
> Linux build even with a yes from us, and that the same constraint has been in
> front of your phase F exit test since 2026-08-27. For *compiling*, that
> constraint is now gone for both of us, and it did not need a machine — it
> needed a workflow file.

That reframing is worth more than the answer was. The second computer has been
treated here as a single blocking fact since August, and it turns out to be two
facts wearing one name: *compiling somewhere else* needs a runner, and *running
somewhere else* needs a person. Only the second was ever really scarce.

## The bug they found in our code before we did

They predicted it and told us where to look, because it was a bug **with a
distribution mechanism**: their examples are *"the first thing a new client
copies"*, and we had copied one.

`src/main/desktop.cpp` asked for **OpenGL 3.0** and initialised ImGui with
`#version 130` — the pair every host copies. macOS ships no OpenGL 3.0: it
offers legacy 2.1, or 3.2+ core profile with forward compatibility, and nothing
between. So the request **does not fail**. `glfwCreateWindow` succeeds, hands
back 2.1, and the `#version 130` shader will not compile against it. **The
window opens and stays blank** — which presents as a rendering bug in our own
draw code, in the one file whose interesting part we did not write.

Fixed by routing through `maiz::gl_context_hints()`
(`voidmaiz/glhost.hpp`, header-only), which sets the hints and *returns* the
matching GLSL version string, so the two halves cannot drift apart again —
there is no longer a second place to write either. Windows behaviour is
byte-identical. What it buys is a platform we have not shipped, which is exactly
when a fix like this is cheap.

**The general lesson, theirs:** *a code path no build exercises is a claim, not
a behaviour.* They found it in their own Android CMake, which had been missing a
Void Core source file for weeks because the desktop build links the prebuilt DLL
and never walks that list. Our `ws2_32` and `HORMIGA_PLATFORM` fixes from this
morning are the same species, and were equally unexercised.

## What we built on top of it

**`src/platform/preview_server.cpp` has a POSIX branch.** It was the one file in
`src/` with no `#else`, and once the view stopped being the gate it became the
first real obstacle. The port is a shim rather than a rewrite, because **winsock
IS BSD sockets** — Berkeley is what Microsoft copied — differing in
initialization, teardown, and the name of the failure value. Naming those three
once leaves the body of the file identical on every platform; a translation
layer touching every call site would have been a second thing to keep right.

The one place they genuinely disagree is worth the comment it got: on Linux a
`send` to a hung-up peer raises **SIGPIPE, whose default disposition is to kill
the process** — so closing the preview tab mid-response would have taken the
application down. `MSG_NOSIGNAL` per call where it exists, `SO_NOSIGPIPE` per
socket on macOS, nothing on Windows, which has no such signal.

**`.github/workflows/ci.yml`**, modelled on theirs: Void Core, then Hormiga —
library, CLI, **GUI** and tests — on `windows-latest`, `macos-latest` and
`ubuntu-latest`, `fail-fast: false` so a red leg cannot hide a green one. The
GUI is in the build deliberately: `voidhormiga` links `voidmaiz_view`, GLFW and
ImGui, and it is the only way the macOS GL fix gets checked at all.

Two details taken from their file rather than rediscovered: GLFW 3.4 builds both
an X11 and a Wayland backend on Linux, so configure needs both sets of headers
(`xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols`); and
`reduce_conformance` is excluded from the gating run and reported separately.
That last one closes a loose end here — the suite has been failing locally
throughout this arc and had only been established as *"not caused by our
changes"*. It is Void Core's extended reduce contract from 2026-09-01 (boxes, a
reserved separator, a `patch` rule) against a port that does not implement them
yet: **Void Maiz's backlog item, knowingly at 17/25, not a regression.** It must
not gate a platform verdict, and it must stay visible rather than be silenced.

And one assertion a runner *can* make about a platform it cannot display: that
the binary starts and knows which computer it is for. The CI greps
`voidhormiga-cli update` for the expected platform tag per leg, which is what
keeps this morning's `HORMIGA_PLATFORM` fix from rotting.

## What is still not true

**Nobody has seen Hormiga draw a pixel on a Mac or on Linux**, and CI
structurally cannot say otherwise. Their caveat is ours:

> CI proves the three desktops COMPILE and that the headless suite RUNS. It does
> not prove a window opens. No runner has a display, and for a GUI, "it ran
> there" means a person watched it.

So the macOS context fix is **reasoned and compiled, not witnessed**. The class
of thing that survives a green build is exactly the class that would bite first:
a blank window, retina scaling, a menu-bar convention.

Their sequencing advice, taken: **get one person to run it on each platform
before asking Void Mago for AppImage and `.dmg` support.** That is a smaller ask
than a packaging feature, it is the only thing that turns the caveat into a
claim, and it makes the Mago message specific instead of speculative. The Mago
message stays unwritten for now — the reason has changed (from "it might not
compile" to "nobody has run it"), but the conclusion has not.

**`platforms` stays one entry long, and the download page keeps saying "no build
yet" on two of three cards.** Both of those are now true *because* of a stated
distinction rather than by default, which is the difference between honesty and
not having gotten around to it.

`MESSAGE_FOR_CLICKLAFONT_hormiga-three-platforms-compile-2026-09-08.md` went out,
and **most of it is a message telling a client not to change anything.** Their
first instinct on hearing "it compiles on three platforms now" would reasonably
be to update the two cards that say *no build yet*, and those cards are still
exactly right. The optional copy in it — for the full-width narrative, not the
cards — is bounded by what it must never become: no "coming soon", no month, no
quarter, nothing a reader could mistake for a build that exists somewhere. Two
of three cards being honestly empty is the reason a visitor can believe the
third.

## The author has a Linux machine, and that changes what was worth doing next

Said the same day: *"i have a linux computer, i was gonna download hormiga on it
and test it."* Which turns the caveat above from a permanent condition into an
afternoon, and made two things worth fixing before that afternoon rather than
during it.

**Void Core's shared library was staged on Windows only.** `find_file` looked
for `libvoidcore.dll voidcore.dll` and the copy was `if(WIN32)` — so this file
contained a paragraph describing, at length, a failure that had cost a day
(*"the build is green and the program does nothing at all"*), and then left that
same failure in place for the two platforms nobody had built. `add_library(...
SHARED)` produces `libvoidcore.so` and `libvoidcore.dylib`, and neither was
looked for.

**The copy alone would not have been enough**, which is the part that differs
from Windows and would have been the actual lost afternoon. Windows looks for a
DLL beside the executable; ELF and Mach-O do not unless the binary carries an
rpath saying so. Both binaries now get `$ORIGIN` on Linux and `@loader_path` on
macOS, appended to the build rpath rather than replacing it, so running from the
build tree and running a moved binary both work — which is what a tarball, an
installer and somebody's Downloads folder all need.

**`AGENT-GUIDE.md` §10 was entirely Windows and MinGW**, so the person about to
do the one thing CI cannot had no recipe. There is one now: the sibling checkout
layout, the GLFW X11-and-Wayland headers a first-time Linux builder hits first,
Core before Hormiga, the `curl` dependency, and the two commands worth running
in order — `voidhormiga-cli update` to see the platform tag come back
`linux-x64` rather than `unknown`, then the window.

It ends by naming what the run is for, because it is easy to treat a working
program as unremarkable: **a platform moves into `void.json`'s `platforms` array
when somebody has looked at a screen, and not before.** The likely failures are
listed so a first run knows what it is looking at — a blank window (the OpenGL
context, fixed but unwitnessed), HiDPI scaling, missing icons if `vendor/fonts/`
did not travel, a file dialog that does nothing because the native one is
Windows-only and guarded out.

---

# 2026-09-09 — the repository is public with its history, and two platforms are waiting on one checkbox

## The author's instruction

> create the linux and mac builds now. you cannot test these … its okay to get
> these builds wrong, but they must exist and be released publically on github

Taken as stated. The concern raised in the previous entry — that nobody has
watched a window open — was answered directly by the author: they have a Mac and
a Linux device and intend to be that person. So the caveat is not a reason to
withhold a build; it is a reason to *label* one.

**A Windows machine cannot cross-compile either target.** macOS needs Apple's
SDK and toolchain, and a Linux GUI build needs the target's toolchain and its
X11/Wayland headers. What can produce a real binary is the thing already
producing green builds: **the runners are real Mac and Linux machines.**

## `.github/workflows/release.yml`

Three legs, and the third is the one worth explaining: `ubuntu-latest`,
**`macos-14` (Apple Silicon) and `macos-13` (Intel)**. A runner builds for the
architecture it is, so a single macOS artifact would either lie about which Macs
it runs on or have to be a universal binary — and universal requires every
vendored dependency *and* Void Core to be built the same way, which is a larger
bet than publishing both. `HORMIGA_PLATFORM` was corrected the same day for the
same reason: it said `macos-universal`, following Void Core's manifest, and an
Apple Silicon build calling itself universal is a claim an Intel Mac would
believe and then fail to execute. It is `macos-arm64` / `macos-x64` now.

Each archive is `bin/` minus the test executables — which already contains
everything that must travel, because the build stages `AGENT-GUIDE.md`, `okf/`,
`vendor/fonts/` (UI faces), `fonts/web/` (the webfonts a deployed site carries so
it reaches no CDN) and Void Core's shared library beside the binaries, and the
binaries carry the `$ORIGIN` rpath that makes that copy the one that loads.
Versioned and version-free names both, per §4, so a page could link one
permanently correct URL if one ever should.

**Each archive carries `READ-ME-FIRST.txt` saying it is untested**, in those
words, with the macOS right-click-to-open instruction and the `chmod +x` a
tarball needs. Publishing an unlabelled build would have been the thing the
whole download page exists to refuse; publishing a labelled one is just honest.
`void.json`'s `platforms` stays `["windows-x64"]`, and the two cards on the live
page stay `no build yet`, until a person has looked at a screen.

## The repository's real history is public now

Everything through this arc — the update client, `hol_github`, the platform
sets, `translate.cpp`, the reports folder, the portability fixes — had been
sitting uncommitted since before 0.1.0. Two commits, pushed. Checked first that
no member data, `.miga`, database, backup or credential file was in the set;
`.gitignore` was already covering all of them, which is ground rule 2 having
been designed in rather than remembered.

## Blocked on a token scope, which is not a code problem

`git push` is refused for the two workflow files:

> refusing to allow a Personal Access Token to create or update workflow
> `.github/workflows/ci.yml` without `workflow` scope

The code is pushed; the workflows are committed locally and cannot leave this
machine until the author's fine-grained PAT gains **Workflows: Read and write**.
Recorded here because it will be the same wall the next time any workflow is
touched, and because it is the sort of thing that reads like a broken push.

## Click LaFont, on prose that goes stale

Their third report in eight days, and the finding generalises past their page.
They verified §1 rather than acting on it — that CI really does matrix three
platforms, that `platforms` really does still say `windows-x64` — and left the
two cards untouched. *"A green build is not a window"* landed as intended, and
the OpenGL example is why: they said it would otherwise have read as licence to
soften the card.

**The finding:** their page said Linux *"has never been compiled for"*, true
when written and **false eighteen hours later**. Nothing caught it and nothing
could have — no link exists between `void.json` and a sentence on a downstream
site, and the page renders and deploys happily while asserting something the
manifest contradicts.

What makes it worth recording is that the same page is careful about exactly
this hazard everywhere else, and *by construction*: the button, the checksums
and the update feed all **point at** the authoritative thing and let GitHub
resolve it. Prose **restates**, and nothing resolves it. So the rule §4 already
argues for has exactly one place it cannot reach. Added there as a paragraph
rather than as a feature, which is their own read: *"I do not think this wants a
feature; I think it wants to be known."*

Their corrected copy is good and is what the live page now carries — no month,
no quarter, no "coming soon", and it explains the OpenGL bug in a sentence a
visitor can follow: *"a Mac was being asked for a graphics context it does not
have, handing back an older one without complaining, and drawing nothing at
all."*

They also flagged, without doing it, that a container with `Xvfb` would catch the
blank-window class cheaply — and did not attempt it because starting Docker or
installing WSL is a change to the author's machine nobody asked for. Worth
knowing for whoever wants a regression test for the bug that was found by
reasoning.

**And their read on Q66 moved against their own ask:** the full-width narrative
is doing the job, the page is not worse for it, and they now think that supports
the counter-lean more than their original request did. Left open; the author
decides.

---

# 2026-09-10 — the first Linux and macOS builds anybody has ever run, and the five things that broke

Linux and macOS archives are attached to `v0.1.0` and downloadable. **Five
failures stood between the workflow existing and an artifact existing, and none
of them was findable by reading.** That is the entry.

## What the "green CI" claims were actually worth

Before any of this: **Void Maiz's repository on GitHub was last pushed
2026-09-01.** It has no `.github/workflows`, no `glhost.hpp`, and no
`vendor/glfw/deps/wayland/`. Their message of 2026-09-08 described a
three-platform runner and we relayed that to the author as *"their CI is green
on all three"*. It cannot have been: the workflow was never on GitHub, so it had
never run. Their "verified cold, 11/11" was a local Windows run. The Click
LaFont agent's verification of our own `ci.yml` was likewise a local file read —
ours was not pushed either until today.

Nobody was lying. **Three agents each reported honestly on something local and
the composite read as a green matrix that did not exist.** The lesson is the one
Void Maiz themselves wrote and none of us applied: *a code path no build
exercises is a claim, not a behaviour* — and a workflow file that has never been
pushed is exactly such a path.

## The five failures, in the order they appeared

1. **`glhost.hpp` not found (macOS).** Our `desktop.cpp` hard-included a header
   that exists only in the author's working copy of Void Maiz. Guarded with
   `__has_include` plus a fallback that gives the same answer, and the fallback
   is written as one block — hints and version string adjacent — so this copy
   cannot drift the way the original pair did. It stops compiling the day
   upstream's commit lands.
2. **GLFW's Wayland backend (Linux).** GLFW 3.4 builds X11 *and* Wayland by
   default and generates client headers from protocol XML in
   `vendor/glfw/deps/wayland/`. **That directory is not in Void Maiz's vendored
   drop, locally or on GitHub.** Configure succeeds; the build stops on a
   missing `xdg-shell.xml`. X11 only, set as a cache variable by the
   superproject — an X11 build runs on a Wayland desktop through XWayland, and
   the alternative was vendoring files into somebody else's repository.
3. **`-static-libgcc` (macOS).** clang rejects it outright. It had spread to
   **nine call sites**, so fixing them one at a time is how
   `hormiga_civic_smoke` survived the first pass and failed the second. Now one
   `HORMIGA_STATIC_RUNTIME`, empty on Apple. The flags were always a GNU/MinGW
   concern — `0xc0000139` from a stale runtime, and the `libwinpthread-1.dll`
   Mago found in the PE import directory.
4. **The vendored libsodium is a Windows binary.** This is the one worth
   keeping. `vendor/libsodium/` holds headers and a prebuilt `lib/libsodium.a`
   and **no sources at all**, and that `.a` is a ucrt64 build — the Linux link
   failed on `__imp_EnterCriticalSection`, `__imp__errno` and `___chkstk_ms`.

   Ground rule 5 is *vendor, don't depend*, and this looked like compliance for
   two months. Every other vendored piece — SQLite, BLAKE3, the single-file
   headers — is **source**, and source is portable by construction. **A vendored
   binary is a dependency on one toolchain wearing a vendor's clothes**, and it
   is single-platform whether or not anybody notices. Nobody could have noticed,
   because only a non-Windows build could have asked the question.

   Off Windows the system copy is used, the deviation is stated at configure
   time rather than silent, and **[Q67](/developer_questions.md) asks for the
   real fix**: re-vendor the sources the way SQLite already is. Deliberately not
   done inside a change whose purpose was getting a build onto two platforms —
   that is the refactor riding along that this project has learned about once.
5. **The archive shipped six of Void Maiz's binaries**, including
   `maiz_reduce_conformance` — a knowingly-failing conformance test — to people
   who asked for an outreach application. They land in our `bin/` because we
   `add_subdirectory` their repo, and the existing `*smoke*` rule did not match
   `maiz_*`.

## What exists now

Attached to `v0.1.0`, versioned and version-free, with checksums:

| | |
|---|---|
| `VoidHormiga-linux-x64.tar.gz` | built on `ubuntu-latest` |
| `VoidHormiga-macos-arm64.tar.gz` | built on `macos-14`, Apple Silicon |
| `VoidHormiga-windows-x64-setup.exe` | unchanged, still the supported build |

The Intel Mac leg (`macos-13`) sat queued rather than failing — those runners are
scarce — so `macos-x64` may arrive late or not at all on a given run.

Each archive carries `READ-ME-FIRST.txt` saying in the first line that it is
**untested**, with the macOS right-click-to-Open step (unsigned, so Gatekeeper
blocks a double-click) and the `chmod +x` a tarball needs. Contents verified by
downloading the artifact rather than trusting the packaging step: both binaries,
`libvoidcore.so`, `vendor/fonts/` with the UI faces and the web woff2, `okf/`,
`AGENT-GUIDE.md`, the licence and the notices.

**`void.json`'s `platforms` still reads `["windows-x64"]`** and the two cards on
the live page still say *no build yet*. Compiling is not running, an artifact
existing is not a platform being supported, and the author is about to be the
first person to find out which. That is exactly the sequence the shipping-record
distinction was written for.

## A note on how this was pushed

The two workflow files could not be pushed with the token in the environment:
GitHub refuses a PAT without `workflow` scope. The author supplied a classic
token **in the conversation**, which puts a live credential in a transcript —
flagged immediately, used only through `gh auth login --with-token` on stdin
rather than on a command line, and **it must be revoked.** Its scopes were far
wider than this needed: `admin:org`, `admin:enterprise`, `repo`, `workflow` and
a dozen more, where a fine-grained token with contents+workflows on one
repository would have done.

# 2026-09-10, second entry — the calendar is a hub, and four conformance defects nobody had looked for

## The direction

The author moved off release work and onto the Calendar, with an identity
statement rather than a feature request:

> "We are not trying to make the calendar on Hormiga *the super calendar with
> everything*, but rather **the compatible calendar, that can use any other
> existing calendar system and integrate it**. […] This calendar is a hub of all
> other possible calendars. We are NOT exclusive. We are the opposite of Apple
> or Microsoft for this."

— with the Antfarm's Google Sheets node named as the precedent for what a Google
Calendar integration should look like, and a note that the in-app UX still has
"a lot to be desired."

## What the reframe actually changed

[calendar.md](/concepts/sections/calendar.md) had already grounded the model in
RFC 5545 and called `.ics` "the calendar's cloud-interop holiday" back in July.
That was right, and it was **filed in the wrong place**: under **C5 — exports**,
one item among five tracks, as though interoperability were an output format.
Under the author's framing it is the thesis, so the do-list had to be
reorganized around it rather than extended. New page:
[calendar-roadmap.md](/concepts/sections/calendar-roadmap.md), same shape as the
[Builder roadmap](/concepts/sections/builder-roadmap.md) — an **X-track** (the
exchange) and the continuing **C-track** (the room).

**The architecture was already written down, in another folder.** A hub that
speaks *N* calendar systems is N² adapters written pairwise and N with a pivot
in the middle, and [Void Reyna](/concepts/projects/void-reyna.md) states the
**pivot rule** outright — *never write a direct A→B adapter when A→pivot→B
exists* — along with the shape that earns it: a holiday is an effect boundary
plus a pure `Lens`, and lenses compose. So the design is one sentence: **the
pivot is the dated rune, and RFC 5545's `VEVENT` is its interchange
serialization.** Google Calendar, Outlook, iCloud, Nextcloud, Radicale, a
school district's feed and a `.ics` somebody emailed are then all *transports*,
and none of them ever learns what a Hormiga `event` is.

That is worth saying precisely because it makes "we are not exclusive" a
**property of the mechanism** instead of a promise somebody has to keep — the
same move the [download page](/concepts/platform/download-page.md) made two days
ago when "never hide a platform" stopped being a rule and became platform sets.

It also *sharpens* the existing boundary rather than loosening it. `calendar.md`
says **not a scheduler** — no invitations, no attendees, no free/busy. The hub
framing keeps that: we translate and aggregate, we do not negotiate. A hub that
started emitting iTIP invitations would be competing with Google Calendar, which
is the one thing the author said not to do.

## Four defects, found by reading the writer rather than by a report

The `.ics` writer has been shipping since 2026-07-22 and lives **inline in the
render loop** (`src/render/site.cpp:1846`, inside the loop that builds the web
embed's JSON), which is why it can only ever serve one caller. Reading it to
plan the extraction turned up four things, all confirmed in the code, none of
them previously reported:

1. **No line folding.** RFC 5545 §3.1 caps a content line at 75 octets; there is
   no folding anywhere in the writer. A long `SUMMARY` emits an over-length line
   that strict parsers reject. And the fold has to count **octets, not code
   points**, without splitting a UTF-8 sequence — which this application hits
   immediately rather than theoretically, because it is bilingual by decision
   (2026-09-02) and every Spanish title carries multi-byte characters.
2. **`UID` is built from the rune's editable `name`** (`src/render/site.cpp:1849`).
   Void Core's rune spec is explicit that `spirit.id` is "minted once, never
   reused" and `name` is "editable", and `maiz::SceneNode` carries both. So
   **renaming an event today tells every subscriber that the old event was
   deleted and an unrelated new one created.** One field. Existing feeds change
   UID once, unavoidably, which is the argument for doing it now while the
   number of subscribers in the world is approximately zero.
3. **The `VCALENDAR` header carries `VERSION` and `PRODID` and nothing else.**
   Missing: `CALSCALE`, `METHOD`, `REFRESH-INTERVAL`/`X-PUBLISHED-TTL`, and
   `X-WR-CALNAME` — which for a hub is not cosmetic, it is how a person tells
   four subscribed calendars apart in their own client.
4. **Times are floating** — no `TZID`, no `Z`, no `VTIMEZONE`. Every source
   consulted agrees this is where the "off by N hours" bugs live: a floating
   time means "whatever o'clock it is where the reader is." Invisible until the
   first subscriber is in another state, and wrong twice a year besides.

There is also **no importer at all** — the one grep that matters, `BEGIN:VEVENT`
as a thing being *parsed*, has no hits. For a page whose thesis is now "hub",
that is the headline gap, and it is X3.

The pattern here is the one the 2026-09-02 field report already named about
`image_grid.columns`: *a field that does nothing is worse than no field*. A feed
that is 90% conformant is the same shape of problem — it works in the client you
tested and fails in the one you did not, and nobody tells you.

## What the research changed, and what it didn't

Surveyed the field because the author asked: libical, CalDAV, Nextcloud,
Radicale, Baïkal, DAVx5 / Etar / Fossify, the Google and Graph APIs, and the
commercial "calendar hub" products (Morgen, Cal.com). The table is in the
roadmap. Three findings worth the log:

- **The highest-leverage feature needs no credentials from anybody.** Google,
  Outlook, Apple, Nextcloud, Meetup, Eventbrite, city councils and school
  districts all publish an **ICS URL**. One `hol_ics_feed` holiday reads all of
  them, and it registers in the existing `glyphs_antfarm.hpp` table with no new
  mechanism — `CLOUD`-coloured, a `records` port, precisely the shape of
  `hol_sheets`, which is the analogy the author drew unprompted. Google
  Calendar's "secret address in iCal format" *is* that URL, so **the entire
  Google read integration is a text field.**
- **Google write cannot be shipped the obvious way, and that is a rule
  collision rather than a plumbing problem.** The Calendar API needs an OAuth
  client id and secret; ground rule 2 says no credentials in a public repo, and
  current OAuth guidance says an installed app is a *public client* that must
  not depend on an embedded secret staying secret. Opened
  [Q69](/developer_questions.md) — lean: the operator brings their own client,
  the same posture `config set tools.image_text` already takes for the OCR the
  binary deliberately does not contain. CalDAV goes first regardless, because
  most servers accept an app password, which is a string in the vault we
  already have.
- **libical is allowed by ground rule 5 and is still the wrong call.** MPL-2.0 /
  LGPL-2.1, so vendoring is permitted, and it is the implementation behind
  Evolution, Kontact and Cyrus. But it is a CMake project with generated sources
  and an optional ICU dependency — a different kind of vendoring than SQLite's
  one amalgamation `.c` — and [Q67](/developer_questions.md) is open **right
  now** because a vendored artifact turned out to be less portable than it
  looked. The subset we need is small and, more to the point, a hand-rolled
  parser can be *forgiving* in the way a hub must be: **never reject a file for
  containing something you do not model.** libical's honest role is a
  conformance oracle in tests.

## Opened

- **[Q68](/developer_questions.md)** — is an outreach org's calendar ever
  genuinely multi-timezone? Lean: no, one org timezone; but read any zone,
  normalize on import, author in one — because imported events arrive carrying a
  `TZID` we did not choose the moment X3 lands.
- **[Q69](/developer_questions.md)** — the Google OAuth secret, above.

## Not built

Nothing was compiled this session. The deliverable the author asked for was the
roadmap, and the four defects are recorded rather than fixed so that the
extraction in X0 happens once, with them, instead of twice. The recommended
order is at the foot of the roadmap; the first step is X0, which changes no
behavior and is what makes the rest reachable at all.

# 2026-09-10, third entry — the calendar grid had its own idea of what time it is

Building the C-track from the roadmap drafted earlier the same day, on the
author's direction to focus on UI/UX and specifically on *"better gestures that
actually create an event"*. Everything below is built, tested and in the binary.

## The bug that was not on any list

The C-track was supposed to start with navigation. It started here instead,
because opening `app_shared.cpp` to look at the date helpers turned up this:

```
float cal_parse_hhmm(const std::string& s) {   // the CALENDAR GRID
    if (std::sscanf(s.c_str(), "%d:%d", &h, &mi) != 2) return -1.0f;
```

while `render/text.hpp` — used by the site renderer and the `.ics` export since
2026-08-19 — has `parse_clock`, and says in its own comment why it exists:

> "Every time in a real community database is 12-hour with a meridiem, because
> that is what a flier prints."

So against the input the application is actually given:

| stored | export | **grid** |
|---|---|---|
| `3:00 PM` | 15:00 | **03:00** — drawn twelve hours early |
| `9 AM` | 09:00 | **all-day** — off the time grid entirely |
| `noon` | 12:00 | **all-day** |

`sscanf` took the 3, discarded the meridiem, and the block landed at three in
the morning. With no colon it failed outright and the entry fell into the
all-day chip lane. **The export was right and the screen was wrong**, which is
the worst arrangement available: what you check is correct and what you look at
is not. This had been shipping since the time grid was built on 2026-07-23.

## Why the second parser existed, which is the part worth keeping

`parse_clock` was already pure — string in, `(h, m)` out, no state. It was also
**unreachable**. It lives in `render/text.hpp`, whose docstring promises:

> "All pure: string in, string out, no `HormigaApp`, no ImGui, no I/O. That is
> what makes them testable on their own."

True of every function in that file and false of the file: line 21 is
`#include "app/app_internal.hpp"`, which reaches ImGui and the whole
application. A `ui/` file including it would have been fine — the layering
allows it — but `domain/` could not, and neither could a test that wanted to
check the grammar without standing up a window.

**A function can be pure and still not shareable, and the header is what
decides.** That gap is the entire explanation for how a second, worse time
parser came to exist thirty feet away from a correct one. So `parse_clock`
moved to `domain/clock.hpp`, which includes `<cctype>` and `<string>` and
nothing else. One parser is now a property of the build rather than a promise
in a comment — the same argument the OKF already makes about the map and
calendar sharing a rules engine, where the value is *"the absence of a second
styling system."*

## The three time-grid defects, fixed

All three, reported in this morning's roadmap, are the same failure: **entries
that are neither shown nor accounted for.** A view that runs out of room and
says nothing leaves the operator no way to learn the entry exists.

- **Overlap lanes.** Every timed block spanned the full column width, so two
  events at 3pm drew one exactly on top of the other — and the earlier one was
  not merely hidden, it was *unreachable*, because the hit test found the last
  one drawn. Now entries group into clusters of transitively-overlapping blocks,
  each takes the first lane free at its start time, and the column divides by
  the lane count **of its own cluster**. Per-cluster rather than per-day is what
  keeps one 9am collision from shrinking an empty afternoon to half width.
  Labels clip to their lane.
- **The all-day lane counted its overflow.** `if (++shown >= 2) break;` became a
  "+N more" / "less" toggle per column.
- **The hour range fits the day.** 06:00–22:00 stays the resting range — it is
  right for nearly every community organization, and 24 rows waste half a screen
  on hours nothing happens in — but it now expands to contain whatever the
  visible days hold, so a 05:30 setup call is drawn at 05:30 instead of clamped
  onto the top edge at 6, indistinguishable from something an hour later. A
  **24h** checkbox forces the whole day.
- The month grid had the same disease in a different shape: it drew *every*
  entry, so a day with eleven things grew the whole week's row and pushed the
  rest of the month off screen. Four, then "+N more".

## The gestures

The author asked for better ways to do the same thing. Creating an entry used
to be: right-click, menu item, then a trip to the inspector because the rune was
called `event-3`.

- **Quick-add** — one line, Enter, a named dated rune. `Food drive 3pm-5pm`,
  `Standup 9am` (a start alone means an hour), `Volunteer training` (all-day),
  `!Road closure 2pm` (an incident — the kinds stay visibly distinct at the
  point of entry, per the standing directive). It targets the **focused day**,
  which the month grid now marks in the accent, and which a click or the arrow
  keys move — so where the entry will land is never a guess.
- **Double-click to create** — an empty month cell for an all-day event, empty
  time in week/3-day for an hour. Drag-to-size is still better when you know the
  length; double-click is what people try first, and a short decisive click used
  to do *nothing*.
- **Keyboard** — arrows a day, shift+←/→ and ↑/↓ a week, PgUp/PgDn a month,
  Home/`T` today, `G` jump, `N` quick-add, `M`/`W`/`D`/`A` the granularities,
  Escape deselect. The month/year label became a button that opens a date field
  taking `2026-12-01`, `12/1/2026`, `12/1`, or a bare `14`. Reaching next April
  was seven clicks on "Next".

### What quick-add refuses is the design

`parse_clock` is deliberately permissive because it reads fliers, and that makes
it completely unfit to drive a creation gesture: it finds a time in "Ward 5
meeting". **A quick-add that guesses wrong is slower than one that does
nothing** — you have to notice it guessed, undo, and retype. So a trailing token
counts as a time only if it carries a colon or a meridiem or is
`noon`/`midnight`, and a line that is *only* a time is refused rather than
minting an all-day event named "3pm". Most of the new test is those negative
cases, and the test earned its keep twice while being written: it caught a
`continue` that had become a `break` (so `Food drive 3pm-5pm` — three tokens,
tried at three first — never reached the suffix that worked), and then the bare
`3pm` case the first fix exposed.

## What the file-length budget was for

`calendar.cpp` went 898 → 1339 and `tools/find_long.py` went red. The budget
did exactly the job it exists to do: the growth had a seam in it.

- `domain/quick_add.hpp` — the grammar, **pure, linking nothing**, which is what
  `tests/calendar_smoke.cpp` checks with no window and no Void Core.
- `ui/calendar_toolbar.cpp` — the chrome: views, navigation, filter, quick-add.
  It decides *what and when* the grid shows and touches no cell.
- `ui/calendar_export.cpp` — the PNG. Draws no ImGui; composes pixels and writes
  a file.

`calendar.cpp` is 879 and under budget without the number moving. `app.hpp`'s
budget did move, 1150 → 1160, and the reason is written into the table: a class
declaration grows when the class grows, and clawing four lines back out of
unrelated declarations to stay under a number makes the file worse rather than
smaller. The distinction the table is for is exactly the one this pass
demonstrates — a 1,300-line function file has a seam; a struct definition does
not.

## Not done

`reduce_conformance` fails, and failed identically at HEAD before any of this —
it is Void Core's reduce suite, not ours. The other 34 tests pass.

The X-track is untouched: the `.ics` writer still has the four conformance
defects recorded this morning (no line folding, `UID` from the editable `name`,
a bare header, floating times) and there is still no importer. `domain/clock.hpp`
was extracted with X0 in mind — the lens will want the one parser too.

# 2026-09-10, fourth entry — the lens exists, and the feed is conformant for the first time

X0 and X1 from the roadmap: the iCalendar writer is a lens in `domain/` instead
of forty lines inside a render loop, and the four conformance defects found this
morning are fixed, tested, and **measured**.

## What "measured" means here, because it is the whole point

The four defects were found by reading. Fixing them without a number would have
been the same act of faith the 2026-09-02 field report already criticised in
another shape. So: a conformance checker was run over the **real rendered
output** of a real fixture, before and after, and it reports a difference that
is not a matter of opinion.

**Before** — a 142-octet `SUMMARY` line against RFC 5545 §3.1's limit of 75, and
`UID:potluck@voidhormiga` built from the rune's editable name:

```
SUMMARY:Neighborhood Potluck & Know-Your-Rights Night\, with a deliberately long title to force RFC 5545 line folding past seventy-five octets
```

**After** — folded at 75 octets with a continuation line, UID from the frozen
`spirit.id`, and a header that has the six properties it was missing:

```
X-WR-CALNAME:Riverton Community Network
REFRESH-INTERVAL;VALUE=DURATION:PT60M
...
UID:rune_cbd5bc6011@voidhormiga
SUMMARY:Neighborhood Potluck & Know-Your-Rights Night\, with a deliberately
  long title to force RFC 5545 line folding past seventy-five octets
```

The checker also confirms every logical line is valid UTF-8 after unfolding,
every VEVENT carries `UID`/`DTSTAMP`/`DTSTART`, and no UID repeats.

## The lens

`src/domain/ical.hpp`: an `Event` type, `fold`, `escape_text`,
`public_categories`, `to_vevent`, `to_vcalendar`, `now_utc`. Pure — no ImGui, no
HTML, no I/O — and in `domain/`, which is what makes it reachable by the things
that need it next: the importer, the subscription holiday, CalDAV. None of those
renders a web page, which is why the writer being *inside* one was the actual
problem rather than a tidiness complaint.

**`Event` is the privacy seam, by shape rather than by discipline.** It carries
exactly the fields that may leave the machine, for the reason
`render/published.hpp` already gives about its own `Person` having no `notes`
member: *"a field that does not exist on the type cannot be leaked by a future
caller who forgets, and cannot be added by accident."* `site.cpp` now fills each
field deliberately at one call site.

## The decision that fell out of building it

`CATEGORIES` from tags is the most useful thing we can add to a VEVENT — tags
map onto it naturally and round-trip through every client — and the easiest
thing in this codebase to leak, because an organization invents its own
namespaces and nothing here can know what they mean.

**A denylist is therefore unsafe by construction.** It can only exclude the
namespaces that existed when it was written, so the first internal axis an
organization coins is published to the world *by default*, silently, on a URL
strangers poll. `okf/concepts/sections/calendar.md` names ICE activity as the
canonical sensitive case for this section, which is about as clear as the stakes
get.

So `public_categories` is an **allowlist**: only `kw:` reaches a feed, prefix
stripped. `clearance:`, `status:`, `type:` and everything an organization coins
stays home. Widening it is an edit to one function under a comment saying so —
enforcement by shape, the same as `published.hpp`, and the reason rule 6 says
the check lives at the seam rather than in a convention.

## What the VEVENT gained

Present before: `UID`, `DTSTAMP`, `SUMMARY`, `LOCATION`, `DTSTART`, `DTEND`.
Added, each already backed by a field we had: `DESCRIPTION` (the summary prose,
language-selected at the same seam the page uses), `URL`, `GEO` — with the
**semicolon** §3.8.1.6 actually specifies, not the comma the stored field uses —
`CATEGORIES`, `LAST-MODIFIED`, `SEQUENCE`, and `STATUS`.

`STATUS:CANCELLED` is the one worth explaining. A cancelled event that simply
stops appearing in a feed **stays on every subscriber's calendar forever**,
because a client cannot distinguish "cancelled" from "filtered out" or "the
server was down". Cancelling something has to be a tombstone or it is not
cancelling.

The header gained `CALSCALE`, `METHOD`, `X-WR-CALNAME`, `X-WR-TIMEZONE` (empty
until X2), and both `REFRESH-INTERVAL` and `X-PUBLISHED-TTL` — both spellings on
purpose, since Outlook reads the `X-` one and everything else reads the standard
one, and emitting both costs a line. `X-WR-CALNAME` is not cosmetic for a hub:
it is the difference between four legible calendars in somebody's sidebar and
four rows named after their URLs.

## The golden test earned its keep, and then taught something

`hormiga_golden_render` failed immediately, which is exactly right — and it
failed on **only the three `.ics` files**, with every HTML hash unchanged, which
is the evidence that rewiring the renderer moved nothing else. That is what that
test is for.

Then it failed a second time, after re-capturing, and the reason is more
interesting than a stale hash: **the UID is now minted per run.** It is built
from `spirit.id`, which is "minted once, never reused" *within a database* — and
the golden fixture builds its database from nothing on every run. The id is
exactly stable where stability matters (one organization, across every publish)
and necessarily different in a fixture, so pinning it would pin the mint rather
than the renderer. `UID` joined `DTSTAMP` in the normaliser, with that reasoning
written where the next person will hit it. What the golden still holds is that
the UID is present, well-formed, and one per event; the folding, the header and
every other property stay byte-compared.

## Also removed

`ics_text` and `ics_now_utc` are gone from `render/text.hpp` — 30 lines with no
callers left. They lived there because the `.ics` twin was written inline in the
site renderer, which is the thing that stopped being true.

## Still open

X2 (the org timezone — the `tzid` field exists and is empty, so times are still
floating), X3 (the importer, the reader half of the lens), X4 (`hol_ics_feed`,
the highest-leverage item in the track), X5, X6. `reduce_conformance` still
fails and still failed identically at HEAD; the other 34 pass.

# 2026-09-10, fifth entry — 0.1.1, and the field that makes an update prompt worth reading was silently empty

The author's call: ship what the calendar work amounts to, as a mini test of the
update client against a real installed 0.1.0. This is the 0.1.1 the phase G
notes have been asking for — *"one release proves the feed parses and nothing
more, and the exit test is that it updates itself."*

## The bow: the lens got its second caller

Before packaging, one addition, chosen because it *tests the claim the previous
entry made*. The argument for extracting `domain/ical.hpp` was that a writer
living inside a render loop "could only ever serve one caller". So: **Export
.ics**, from the Calendar toolbar and from `effect export-calendar-ics`.

It cost about thirty lines, because the folding, the escaping, the UID, the
all-day `DTEND` and the tag allowlist are already written down once. That is the
whole return on X0, collected in the same day.

**It exports what the FILTER shows, not what the viewport shows** — a saved
"Public Events" calview bakes its privacy choice into the file, exactly as it
already does for the PNG. The window is not the boundary; the filter is.

## And the defect that found

Wiring the CLI path turned up something older. `effect export-calendar` — the
PNG, registered since the calendar was built — **is in the DESKTOP effect table
and has never been in the headless one.** From the CLI it has answered `done`
and written no file for as long as it has existed.

That is the 2026-09-02 field report's *"a field that does nothing is worse than
no field"* one layer up, and it contradicts founding commitment 1 directly:
*the CLI, the GUI and any agent are three callers of the same verbs.* An effect
that works in one of them is that sentence being false.

The `.ics` twin does not repeat it: it goes through `render_from_state`, the
same seam the renderers use, so it inherits the replay, the glyph registration
and `refresh_allo_rules()` — the last of which matters because Allomone decides
what a calendar entry looks like and whether it is published at all. The
comment there already said why `publish-index` joined that seam rather than
getting its own boot; this took the invitation.

**The PNG stays desktop-only, and that is a real difference rather than an
oversight**: it blits from a baked ImGui font atlas, which does not exist
without a window. Said out loud in both files so the asymmetry is a decision
somebody can disagree with rather than a gap.

## `mago feed` wrote `"behavior_changes": []` and said nothing

0.1.1 **moves where existing events are drawn** — that is the entire category
`behavior_changes` exists for, and the reason this release needed four of them
rather than none. We wrote four. `mago feed` ran, reported success, and emitted
an empty array.

The cause, in `mago/src/manifest.cpp`: entries must be **objects** with `what`
and `who_is_affected`, and a non-object is `continue`d. An object missing
`what` throws a named `ManifestError`; a string is discarded in silence. **The
strict path and the silent path are the wrong way round** — the wrong shape is
the easier mistake and it is the one that says nothing.

It lands on the one field whose absence cannot be inferred from anything else.
A dropped `adds` entry is a missing bullet; a dropped behavior change is the
warning that never reached the person being asked to update. Mago's own feed
writes *"`behavior_changes` is what makes the asking worth anything"* into every
document it generates, which is precisely why this is the worst field for it to
happen on.

Caught by diffing the generated feed against the manifest by hand, and only
because the empty array looked wrong. Reported as
`MESSAGE_FOR_VOIDMAGO_hormiga-behavior-changes-dropped-silently-2026-09-10.md`
with a one-`else` fix and a permissive alternative; ours rewritten in the object
shape with `who_is_affected` on all four. Nothing in Mago patched — rule 4.

## The four behavior changes, because they are the interesting part of the release

1. **Times on the grid move, and it is a fix.** `3:00 PM` was drawn at 3 AM;
   `9 AM` was treated as all-day and never appeared on the time grid. Nothing in
   anybody's database changed — what changed is where those entries are drawn.
   A calendar that looked wrong should now look right, and one that looked right
   was already exporting correctly.
2. **Feeds carry `CATEGORIES`, from `kw:` tags only.** Additive, and errs toward
   withholding.
3. **A cancelled event is published as `STATUS:CANCELLED`** instead of
   disappearing. One that merely disappears stays on a subscriber's calendar
   forever.
4. **UIDs come from the permanent id, not the editable name.** Anyone already
   subscribed sees the entries re-added **once** — stated plainly, because it is
   a real cost and the person deciding whether to update is the one who pays it.

Item 4 is why the object shape matters: `who_is_affected` there is "anyone with
an existing published feed that people have already subscribed to", which is a
much smaller set than "everyone", and a string entry could not have said so.

## What was built

`mago doctor` clean for us (its two errors are VoidCore/VoidPalabra's seam
disagreement, unchanged and not ours). `mago stage` → 6 files, 27 MB →
`makensis` → **8.18 MB installer**, up from 7.36. `mago feed --artifacts`
hashed it and checked the version-free copy. Checksum verified against the file
by hand before anything left the machine.

The whole Mago pipeline behaved exactly as documented apart from the one finding
above, and the installer built on the first attempt — which is worth recording
because the last time this repository met Void Mago (2026-09-04) it was refused
twice and a third blocker turned up behind the second.

## Still not true

Nothing is signed. `platforms` stays `["windows-x64"]` — it is a shipping
record, and nobody has watched a window open on the other two. The exit test is
not this file; it is the installed 0.1.0 finding 0.1.1, showing those four
behavior changes to a person, and replacing itself.

## 0.1.1 shipped, and an installed 0.1.0 found it

Published to `v0.1.1`: the 8.18 MB Windows installer under both its versioned
and version-free names, `void-updates.json`, and `SHA256SUMS.txt`. The tag push
also started `release-builds` for the three non-Windows legs.

**The feed resolves through the URL compiled into 0.1.0.**
`releases/latest/download/void-updates.json` answers `"latest": "0.1.1"` with
all four behavior changes intact.

**And then the part that had never been done.** The copy installed at
`%LOCALAPPDATA%\VoidHormiga\voidhormiga-0.1.0\` — a real install, not a build
tree — was asked:

```
Void Hormiga 0.1.0  (windows-x64)
  last checked 2026-09-08T19:30:26Z

Hormiga 0.1.1  (2026-09-10)
you are running 0.1.0
…
things that behave differently
  - TIMES ON THE CALENDAR GRID MOVE, and it is a fix rather than a change of mind…
      affects: Anyone whose events store a 12-hour time with AM/PM, which is
               most organizations…
```

Phase G's note said *"one release proves the feed parses and nothing more, and
the exit test is that it updates itself."* The feed now parses **from a
different version than the one that wrote it**, which is the thing a single
release structurally could not show: 0.1.0's parser, compiled on 2026-09-04,
read a document generated six days later and rendered every field of it. That
is the compatibility claim, tested rather than asserted.

**The `affects:` lines are the vindication of the Mago finding.** Had the
entries stayed strings they would have been dropped silently and this prompt
would have read *"things that behave differently"* followed by nothing — on the
release whose headline is that events move. Instead the person deciding is told
that it affects "anyone whose events store a 12-hour time with AM/PM, which is
most organizations", and separately that only "anyone with an existing published
feed that people have already subscribed to" pays the UID cost. Two different
audiences, correctly distinguished, which is precisely what the object shape is
for and what a bare string could not have carried.

**What is still not done: `update --install`.** It downloads, checks the digest
and launches an interactive installer, which is a window on the author's screen
and a change to their installed software — left for them to run rather than
done on their behalf. So the exit test is proven up to the last command: the
check, the feed, the cross-version parse and the prompt are all witnessed; the
replacement is not.

# 2026-09-11 — Mago refused it by name, and took the stricter half of the ask

Void Mago fixed the silent drop reported yesterday, and — checked rather than
believed, which is this repository's habit with upstream claims since
2026-09-08 — it is fixed, and the fix is better than what was asked for.

## Reproduced, against the fixed binary

A manifest was written carrying exactly the shape 0.1.1 shipped with: four
`behavior_changes` as plain strings. Yesterday that produced
`"behavior_changes": []` and a success message. Now:

```
mago: …/void.json: release.behavior_changes[0] is a string; each entry is an
      object with "what" and optional "who_is_affected"
mago: 1 manifest was not read, and nothing below counts it.
```

Three things in that output were not in the ask.

1. **The index.** The message names `[0]`. The suggestion was a bare `else`
   beside the existing `continue`, which would have said *an entry* is wrong; a
   manifest with six of them would have sent somebody hunting. Naming the slot
   is the difference between a refusal and a useful refusal.
2. **The shape.** `json_shape()` says *"is a string"* rather than *"is not an
   object"* — it reports what was found, not merely what was absent, which is
   the same distinction the download page's platform sets turned on.
3. **It refuses at `scan`, not at `feed`.** So `mago doctor` catches it, which
   means the wrong shape is caught by the command you run *before* building a
   release rather than by reading the artifact afterwards. Yesterday's finding
   was made by diffing a generated feed against the manifest by hand; nobody has
   to do that now.

They also refuse a non-array `behavior_changes`, which was not in the report at
all and is the same class of mistake one level up.

The comment they left in `src/manifest.cpp` states the reasoning the report
argued for, in their own words: *"a dropped `adds` entry is a missing bullet, a
dropped behavior change is the warning that never reached the person being asked
to update. So every wrong shape here is refused by name."*

## And our own manifest still passes

`mago doctor` reports nothing new for `voidhormiga` — its two errors remain
VoidCore and VoidPalabra disagreeing about what to call their seam, which is
neither ours nor new. The 0.1.1 feed regenerates identically under the fixed
binary: `latest: 0.1.1`, four behavior changes, every one carrying a
`who_is_affected`. So the strictness costs us nothing, which is what it should
cost a manifest that was already right.

`MESSAGE_FOR_VOIDMAGO_hormiga-behavior-changes-dropped-silently-2026-09-10.md`
is consumed and deleted; this entry is the durable record (rule 4).

## What this pays for

The four `affects:` lines a person now sees when 0.1.0 asks whether to become
0.1.1 exist because the entries are objects. Under yesterday's Mago they would
have been dropped in silence, on the release whose headline is *events move*.
The report was worth writing and the fix is worth having, and the interesting
part is that neither would have happened if the empty array had looked ordinary.

# 2026-09-11, second entry — X3: Hormiga reads other people's calendars now

The reader half of the lens, which is the half that makes *"the compatible
calendar"* true rather than aspirational. `effect import-ics <path|url>
[apply]`.

## Measured against real feeds, not synthetic ones

A parser that passes its author's own test file has proved that the author is
consistent. So before wiring anything, the parser was pointed at two calendars
from the wild:

| feed | size | what it is | result |
|---|---|---|---|
| Google, "Holidays in United States" | 120 KB | 317 entries, all all-day | **0 failures** |
| FOSDEM 2025 schedule | 580 KB | 1,105 timed talks, parallel tracks | **0 failures** |

Zero unparseable dates, zero entries without a UID, zero components that caused
a refusal. Google's feed is the canonical interop target, and FOSDEM's is the
opposite shape — every entry timed, many concurrent — so between them they
exercise both halves of the model.

## The rule the parser is built around

**Never reject a file for containing something you do not model.** Real feeds
carry VTODOs, VTIMEZONEs, VALARMs, `X-` properties and parameters nobody has
seen; the correct response to all of them is to walk past. That is the behaviour
the author defined this section *against* — a hub that refuses a calendar
because it holds a task list is behaving like the vendors.

The one thing that fails an entry is having no usable `DTSTART`, because an
entry with no date is not a calendar entry.

Component nesting is tracked rather than assumed, and it is not pedantry: **a
VALARM lives inside a VEVENT and carries its own `DESCRIPTION`.** A parser that
keys on property names without knowing which component it is inside will
cheerfully overwrite a meeting's description with the text of its reminder.
There is a test for exactly that.

## Two bugs found by running it, which is why it was run

**The importer never reached a fixed point.** Re-importing an unchanged file
reported *174 updates*, every time, forever. The cause was one small function
doing two things wrong at once: a hand-rolled quoter flattened newlines to
spaces on the way in, while the comparison used the *unflattened* parsed value —
so the value written could never equal the value compared. 174 and not 317
because that is how many of Google's entries carry a `DESCRIPTION` with a
newline in it ("Observance", then "To hide observances, go to…").

The fix was to stop hand-rolling: `maiz::arg` is Void Core's own
`vc_arg_quote` (SPEC §6.1) and round-trips a newline intact — checked rather
than assumed, with a `set` carrying one. **One change closed two bugs**, a
silent data loss and a plan that could not converge, which is the tell that the
hand-rolled quoter was the actual mistake rather than a detail of it. It is also
the third time this repository has been bitten by not using Core's quoter; the
first two were the trailing-backslash case.

**`lint_glyph_fields` caught the two new fields immediately**, which is that
linter doing precisely its job — it is the encoded form of the 2026-09-02 field
report's *"a field that does nothing is worse than no field"*. `ext_uid` and
`rrule` are deliberately unrendered and are now exempt **with reasons**:
publishing a foreign system's opaque identifier tells a reader nothing and tells
a scraper which feed the organization subscribes to; and a renderer must not
print `FREQ=MONTHLY;BYDAY=3TU` at a person. When C3a lands, what gets rendered
is the occurrences, not the string.

## Identity, which is where a hub is won or lost

The foreign `UID` is stored as `ext_uid` and is **the only thing matched on**.
The way this goes wrong is fuzzy matching — guessing that an entry with the same
title and date "is probably" one you already have — and `data-planes.md` already
states the rule for contacts: *claiming a contact must not search the database*.
Same rule. Match the key you were given, or create. Never guess.

Verified end to end: 317 created; re-import 0/0/317 unchanged; edit **one**
`SUMMARY` in the file and the plan is exactly one `set` command. Precision in
both directions.

## It proposes; `apply` writes

The posture `read-flier` and the sync effects already established, and the
reason is arithmetic: a feed can hold a thousand entries and there is no second
step that puts them back. The dry run now also **shows the commands** (first
twelve), because a report that says "174 updates" without saying which ones is a
number rather than a report — and that is what made the fixed-point bug visible
in the first place.

Everything applies as **one batch**, so an import is a single undoable step
rather than a thousand.

## A note on method

Half an hour of this entry's work was spent recovering from a repair script I
wrote to fix a recurring editing problem — it treated prose quotes inside block
comments as unterminated string literals and joined most of `headless.cpp` into
one line. `git checkout` cost nothing because the work was committed in small
pieces, and the branch was re-applied from a scratch file in two minutes. The
lesson is not about the script: **a "general" fix written in a hurry against a
symptom is how you turn a typo into an outage**, and the only reason it was
cheap is that the tree was clean when it ran.

## Still open in the X-track

X2 (the org timezone — UTC instants are converted to this machine's local time
and named zones are read as written, both reported rather than done quietly; the
`tzid` field exists and is empty), X4 (`hol_ics_feed`, now a small step: the
transport already accepts a URL), X5, X6, and C3a/C3b — the recurrence and spans
this importer is currently storing faithfully and not yet drawing.

# 2026-09-11, third entry — "we shouldn't NEED other calendars", and the hole that proved it

The author, reading the X-track:

> *"Wait — we shouldn't NEED other calendars. Just like the database, where we
> can have a local version of our own data, the calendar data doesn't NEED to
> live somewhere outside of us. We can also create it."*

Correct, and the correction is about **precedence** rather than about deleting
anything. The interop work stands; what had started to drift was the framing.
"A hub of all other possible calendars" can be read two ways, and only one of
them is this application: **compatible is not dependent.** Founding commitment 2
is that local-first is the *resting state* — the default install works forever
with no network, and the network is something an admin adds. The calendar is no
different from the database. Import and export are doors. They are not the
floor.

## The measurement that made it concrete

The framing argument would have been worth having on its own. It turned out not
to be a framing argument at all, because Hormiga had a hole exactly where the
author was pointing. Give it a community organization's most ordinary recurring
thing:

```
rune new event standing
set standing title_en "Riverton Community Meeting"
set standing days "Last Friday of the Month"
```

and export the calendar. **One VEVENT comes out, and it is the other event.**
The standing meeting appears on no grid, in no export, and to no subscriber.
`days` is free text the newsletter prints and the calendar cannot read, and
`cal_entries_on` requires a parseable `date`, which a recurring event does not
have.

Meanwhile [X3](/concepts/sections/calendar-roadmap.md), shipped that morning,
will happily read a recurring event out of Google and store its `RRULE`.

**So Hormiga could express somebody else's standing meeting and not its own.**
That is not an argument for Google; it is a hole in our calendar. The rule this
establishes is worth more than the feature that closed it: *a capability you can
only obtain by importing it is a missing feature, not an integration.*

## What was built

`domain/rrule.hpp` — parse, expand, build, and **describe in plain English**,
because a rule nobody can read is a rule nobody can check.

The model is RFC 5545's, and deliberately so: a recurring event is `DTSTART` +
`RRULE`, which is what the standard says, what every other calendar stores, and
— not coincidentally — what our `date` + `rrule` fields already were. **This is
not a second recurrence model beside the interop one; it is the interop one,
authored from our side.** Round-tripping is then a property rather than a
feature.

The authored vocabulary is a deliberate subset — every N days, weekly on chosen
weekdays, monthly on a date, monthly on the Nth or LAST weekday, yearly —
because the roadmap's own research already recorded the reason to stop early:
*"Outlook desktop is the strictest. Recurring events with complex RRULEs often
fail to import."* A richer rule arriving from a feed is kept verbatim, expanded
when it is one of these shapes, and otherwise shown on its start date with a
sentence saying exactly that. **A wrong date is worse than an honest absence,
and silence is worse than both.**

## Modeled, not simulated — and the export is where that stopped being internal

The first working version expanded a monthly meeting into **twelve VEVENTs** in
the `.ics`. It looked right and was wrong: `calendar.md`'s standing rule is
*recurrence is modeled, not simulated*, and the export is where the difference
bites somebody else. A subscriber receiving twelve copies has twelve things to
edit and no series, the file is twelve times larger, and re-importing it creates
twelve events where there was one — which would break the exact round trip the
pivot format exists to provide.

Fixed: the grid expands (a projection, never stored runes), the file carries one
`VEVENT` with its `RRULE`. Verified end to end — export a monthly meeting,
import the result into an empty database, and **one** event comes back carrying
`FREQ=MONTHLY;BYDAY=-1FR`.

## Two bugs the tests caught before anything was wired to them

- **`add_days` silently ignored negative deltas** (`while (n > 0)` and no
  second loop). The weekly expander walks back to its own Sunday before stepping
  week by week, so every weekly rule was anchored to the wrong weekday — "every
  Tuesday and Thursday" produced Thursdays and Saturdays. Wrong by a *consistent
  offset*, which is the kind of wrong that looks like a plausible calendar right
  up until somebody misses a meeting.
- **The 31st must be SKIPPED, not clamped** (RFC 5545 §3.3.10). "The 31st" in
  November is not the 30th, it is nothing; clamping silently invents a meeting
  on a day nobody scheduled one.

Both were written as expectations first and both failed on the first run, which
is the argument for writing the expectations first.

## What is NOT done, and should be said plainly

`days` is **not** auto-converted. An organization whose recurring events are
written as free text still sees nothing on the grid until an `rrule` is set —
including the golden fixture's own standing meeting. Parsing the common
phrasings and *offering* the rule (proposing, never rewriting) is the next step
and is small now that `parse`/`describe` exist. Leaving it unstated would be the
same shape of defect as the one this entry is about.

Also: the GUI binary could not be relinked this session because the application
was **running** and holding the file. The CLI carries everything above; the
desktop build needs the app closed. Worth recording because the failure
presented as `collect2.exe: error: ld returned 5 exit status` with no
diagnostic, and `app_internal.hpp` already warns that *the gcc driver swallows
ld's stderr in this environment* — the same trap, a third time, and the tell was
that two consecutive identical builds returned different exit codes.


# 2026-09-13 -- the newsletter pass: rows reach the email, and nine asks from someone making one

The author sent nine asks in one message, written mid-task: *"for most of these
im thinking about the newsletter. because right now im making the newsletter."*
Shift-select; flier previews that show the tags; buttons that "don't stack
correctly horizontally"; icons on narratives and an easier way to choose them;
an explicit image + text block; an image-preview engine; a filter that opens as
an expression "especially useful with the 'and' and 'or' statements... idk if
allomone supports parenthesis"; image-grid fit options; and an even more compact
job listing, "remember icons!" The order taken was: what is broken in output
being shipped, then what blocks the work, then the systems. The per-item status
is in the Builder roadmap under "The newsletter pass".

## The newsletter never read `row`

Item 3 was the headline, and it was simpler than broken. The email renderer
walked the document and emitted every block as its own full-width table, one
under the next. The Builder let a person place two buttons side by side, the
website honoured it, and the newsletter stacked them. It is the "declared but
invisible" shape again -- and it arrived the same week Click LaFont reported
`col` doing the same thing on the web (it orders blocks, it does not position
them). The email now mirrors the website's row driver: consecutive blocks
sharing a `row` become one table row of cells sized from `span`, normalised to
the row's total so a short row still fills (an email table cannot hold a gap,
and inventing one is the surprise Click's report warned against). A `cell_px`
width reaches every `width=` attribute, because Outlook obeys that attribute
over CSS and a 572px image in a half-width cell breaks the 620px frame. `col`'s
label now says what it does: "Order within the row (0-11) - blocks fill left to
right".

## How the rest was proven, and the check that was wrong

A fixture was built through the real dispatcher -- two links in one row, a job
grid in `line` mode, a narrative with an icon, an image grid with `fit whole`,
an image + text block with a published picture and one without -- rendered to
both the newsletter and the website, and asserted on the bytes. Thirteen checks.
Twelve passed first time.

The thirteenth was item 3, and the render was right. **The assertion was
wrong**: it required no `</tr>` between the first cell and the second button,
and every email button is itself a one-cell table containing a `</tr>`. Reading
the emitted HTML showed exactly the intended shape -- one row, two 50% cells,
RSVP in the first, Donate in the second. Recorded because a wrong check that
FAILS is the good case. The bad case is a wrong check that passes, and nobody
reads a passing check.

## Re-capturing a golden without taking it on faith

`hormiga_golden_render` drifted on two files: `style.css` (new rules appended,
nothing removed) and the newsletter export. A golden re-capture is precisely
where a regression gets waved through -- "the output changed, that was
expected" -- so it was not re-captured on the expectation. The newsletter's only
intended change on the golden fixture was the new job-line emoji, which sit
behind `theme.icons`. So the fixture was rendered with `theme.icons 0`, and the
newsletter hashed to **5745842a..., byte-identical to the old golden**. That
proves the row driver, `cell_px`, `fit`, the narrative icon and the job-line
refactor change nothing on an existing document, and only then was the golden
re-captured. The method generalises: when a change adds output behind a switch,
turn the switch off and demand the old bytes.

## Parentheses were already there

Item 7's worry was the grammar, and the grammar needed nothing. A block query is
Void Core's tag grammar, which has supported AND / OR / NOT, `&&` `||` `!` and
parentheses all along, plus our `date:` predicates -- the Builder's own filter UI
already detected `(` and dropped to a raw text box. What was missing was a place
to write an expression with feedback. A `</>` button beside every Builder filter
now opens Void Maiz's code editor -- the widget the Allomone tab uses, not the
Allomone language, which is a different grammar -- with highlighting, tag
completion, a parse check, and a live "matches N of M" computed by
`query_matches`, the same evaluation the renderers run. Apply is a button and
nothing else, because the editor also reports a commit when focus leaves it,
which is what happens on the way to Cancel.

## The rest, briefly

- **9:** `detail: line` -- one line per posting -- and emoji on job lines behind
  `theme.icons`.
- **8:** `fit` crop / whole / natural / stretch. It replaced an email branch that
  read `display == "thumb"`, a value the glyph never offered, so it could not
  run. In email, blank stays natural size: `object-fit` is the one property here
  Outlook desktop ignores.
- **4:** one icon vocabulary of 36 names drawn three ways -- Font Awesome in the
  app, Lucide SVG on the web, emoji in email because Gmail strips SVG.
  `icons::svg` renders nothing for an unknown name, deliberately, which is
  exactly what would let a typo look fine in the app and vanish from a page;
  `tests/icon_smoke.cpp` holds every name to the Lucide set that ships.
- **5:** `image_text`. With no published picture the newsletter shows the text
  alone and the render log says why, rather than printing a broken image.
- **1, 2, 7 are built and compiled and have not been seen in a window.** They are
  GUI gestures and previews, and a headless check cannot witness one. Saying so
  is the difference between "done" and "done as far as anyone has looked".
- **6** is planned as its own system. `texture_for` was already an image cache;
  the flier blocks had simply never asked it for anything.

## Two things about the process

The email patch script refused to run. It guarded against a `return` inside the
loop body being turned into a lambda, where it would silently change meaning,
and it found four. All four were sort comparators, whose `return` exits the
comparator and is unaffected. The guard was right to stop and wrong about why,
so the four inspected lines were allow-listed by exact text and every other
`return` still aborts. A guard that refuses too much is a nuisance; a guard
loosened past what was inspected is not a guard.

Three file budgets were raised, each with its reason in `tools/find_long.py`:
`app.hpp` (declarations for the new file), `site.cpp` (+2, fields the glyph
linter needs to see read there), `builder.cpp` (multi-select lives inline in the
canvas loop and cannot move). What could move went to `ui/builder_ext.cpp`,
`render/image_text.hpp` and `render/icon_set.hpp`.

## Not done

The `</>` button on the Data tab's and the Calendar's tag filters (`data.cpp` is
two lines from its budget and wants a small split first); the image-preview
engine (item 6); a sentence beside `deploy-site` for Click's finding that a
deleted page can outlive its deployment on a Pages custom domain for up to a
week. Click's other report asks the author to confirm or reverse its choice to
label the macOS and Linux cards "untested" rather than "no build yet" -- that is
the author's call and was not made here. Void Maiz's touch message is unanswered.


# 2026-09-13, second entry -- colon tags stop hiding, and a tool for the copy that should not exist

Three asks from the author, the second arriving with a report from the agent
that works in LON's folder.

## Colon tags were not quiet, they were gone

*"There are colon tags, like 'something:state' -- one is the clearance tag ...
there should still be easier ways to assign them ... they shouldn't be so hidden
in the GUI. They should have a different color though. Also suggested tags
should be a different color as well because it gets confusing."*

The tag editor skipped every tag containing a colon (`if (t.find(':') != npos)
continue;`), and the tag picker's vocabulary skipped them too. So
`clearance:public` -- the tag that decides whether a person's name reaches a
public website -- could be neither seen nor set from the pane showing that
person. The most consequential tag in the database was hidden from the screen
where that decision is made.

Now every tag is shown, grouped and coloured by kind (`domain/tag_kinds.hpp`):
grey plain tags, teal `ns:value` tags, amber clearance, dimmed `type:`
housekeeping that cannot be removed from the editor because block queries
depend on it. On contacts and organizations the two clearances the publishing
seam reads are offered as **switches**, because a typed clearance with a typo
fails silently in one of two bad directions. Suggestions are outlined in green
and say "not added yet". The picker offers `kw:food` and friends, and creates
one typed.

The editor moved to `ui/tags.cpp`: it was never Data's alone (the Data pane,
both Allomone surfaces and the calendar's day tags all draw it), and `data.cpp`
was two lines from its budget.

## The launch trap, the second time

The LON agent's report: the dev app had been started through `VoidHormiga.bat`
with no database named, so it opened `demo-org.json` in the folder it was
launched from -- the source tree -- and LON's edits, a deploy record, and 157
assets went there. All of it is gitignored and none of it was committed. This
session did not touch those files: the LON agent is merging them back, and two
agents editing one database is the failure being repaired.

The cause was already documented in `desktop.cpp` from the first time, before
2026-09-01. What had been fixed then was `--state`; what had not been fixed was
that nothing said so when it was omitted. Three changes:

- **The menu bar says so, in red, on every frame**, when the open database is
  inside the Hormiga source folder. A warning living in a window nobody opens
  would have protected nobody.
- `VoidHormiga.bat` passes a database path through (or accepts a dropped file),
  and its header says why.
- Niche Tools > Where shows the open path and the fix.

## Niche Tools, off by default -- and Allomone joins it there

*"It would be useful to have a json merge feature with detections, testing,
automatic path changes in antfarm and such ... a new tab 'niche tools', a window
that is off by default (i also think that allomone should be off by default)."*

The merge is Void Palabra's, called with the same prefixes `sync_op` uses so the
preview predicts exactly what Apply produces; Apply is `gui_sync_effect`, the
console's own path. The detections: whether the two copies are the same database
diverged (shared runes) or two different ones; files the other copy references
that are only in its folder, with a copy button; absolute Antfarm paths into the
other folder, rewritten as one batch of logged `set` commands; and "Check the
result", which compares the open database's version name with the prediction.
Collaboration section 7.3 has the rest.

**Status, plainly: compiled, not exercised.** The merge underneath is the tested
one; the detections have only ever existed in a window nobody has opened, and
they were deliberately not tried against LON's real files while another agent is
merging them.

## A QR code, because the author wanted one

Project Nayuki's QR Code generator, vendored under `vendor/qrcodegen/` (MIT)
from master at commit 3c6d0b3cefb4; the newest tagged release is v1.8.0. QR
encoding is Reed-Solomon plus mask scoring, exactly the code where a hand-written
version produces a picture no phone reads. `void.json` declares it without a
version -- the drop is a commit, not a release -- and `mago doctor` notes it as
unversioned, the same as BLAKE3 and Lucide, rather than being handed an invented
one. `domain/qr.hpp` is ours: the quiet zone, the grid, the pixels, and refusing
empty or oversized input instead of producing a code that scans to nothing.
`tests/niche_smoke.cpp` checks it, and the tag kinds, with no window.

## Two self-inflicted breakages, both caught before anything left the machine

**The dev launcher was briefly broken.** A launcher rewrite written as a normal
Python string turned the `\t` and `\b` in `build\bin` into a tab and a
backspace, so `VoidHormiga.bat` pointed at a mangled path. It was restored from
git in the next step and rewritten from a raw-string script. It is the same
escaping trap this repository's own agents have hit in C++ string literals; a
launcher is simply the place it hurts soonest.

**The tag-editor move aborted.** Its anchor, "the tag recommender", first
matched `data.cpp`'s file header rather than the recommender. The script
refused to write because the anchors were out of order -- which is what the
ordering check was for -- and ran once the search started after the editor's
own comment.

## Verification

38/38 tests pass (`reduce_conformance` excluded, failing before this work). All
98 source files are within budget: `app.cpp` 2940 -> 2950 and `app.hpp`
1170 -> 1175, with reasons in the table. Layering passes with the two new UI
files. `mago doctor` accepts the manifest. The golden render did not move: no
renderer changed.

## Not done

Exercising the merge tool on a real stray copy (after the LON agent's merge, or
on a synthetic one); a detailed preview of a `.miga`; and, still open from the
previous entry, the `</>` filter button on the Data tab and the Calendar.

# Card grids, side by side (2026-09-14)

The author: *"event grids should be able to be side by side as well, like
actual grids, not just a list"*, and the same for job openings.

## What changed

- **`event_grid` and `job_grid` gained `columns`** (`combo:1,2,3`), declared in
  `domain/glyphs_blocks.hpp`.
- **Email** (`render/email.cpp`): two small lambdas, `grid_cell_open` and
  `grid_cell_close`, wrap each existing card table in a `<td>` of one
  presentation table.
  - The table gets N cells to a row; the last row is padded with empty cells so
    a lone card does not stretch to full width.
  - The card markup itself is untouched.
  - `detail: line` on a job grid ignores `columns`: a list of single lines is
    that form's purpose.
- **Website**: the `.cards` container gets `cols-N` from `grid_cols_class`
  (`render/text.hpp`, shared helpers). `style.css` fixes the column count and
  stacks below 640px.
- **Blank changes nothing in either domain.** Proof: the golden render moved
  only `style.css`'s hash; every HTML page and the email preview hashed as
  before. Re-captured on that basis.

## Verification

A scratch document rendered 5 events in 2 columns, 4 compact jobs in 3 columns,
and 4 `line` jobs with `columns 3` set:

| What was counted | Result |
|---|---|
| Event cells (2 columns) | 5, with 1 padding cell |
| Job cells (3 columns) | 4, with 2 padding cells |
| Line jobs | stayed a list |
| `<table>`, `<tr>`, `<td>` | all balanced |
| Website containers | carried `cols-2` / `cols-3` |

The full suite passes with `reduce_conformance` excluded, as before. Lint,
layering and budgets pass (`email.cpp` 877/1000).

The GUI executable did not re-link, because the app was running. The CLI and
every test did.

# An alignment scan of the code and the OKF (2026-09-14)

The author asked for *"a scan of the code and okf, making sure that everything
is aligned properly with each other. making sure that the code is well written
and such."* The six linters were already green, which is the point worth
recording: **every drift below was invisible to them.** They check that links
resolve and fields are rendered, not that a sentence is still true.

## Where the OKF had drifted from the code

- **A file layout that no longer exists.** `workspace-and-sections.md` still
  drew the flat `src/section_*.cpp` layout of 2026-08-17. The real one is the
  folders `app/ ui/ render/ domain/ main/`, and the page now shows that.
- **Paths from before the folder move**, in five concept pages (`civic.hpp`,
  `map_actions.hpp`, `rescue_import.hpp`, `miga.*`, `vault.*`). Also stale were
  the headers of seven source files, which still opened with their old names
  (`section_data.cpp`, `headless_main.cpp`, ...), and fourteen comments that
  cited those names as where code lives now.
- **A block inventory naming four blocks that never shipped** (`flyer_grid`,
  `presenter_cta`, `attendee_list`, `meeting_schedule`). `section_header` was
  mentioned nowhere in the OKF. `blocks-and-domains.md` now carries the twenty
  that exist, and AGENT-GUIDE a table of each with its main fields.
- **Three question numbers used twice.** Q40, Q41 and Q42 each named two
  different questions. Every reference in the OKF and the code meant the later
  set (archive, NFC, Void GIS), so the unreferenced 2026-08-19 set became
  **Q70 inbox, Q71 identity, Q72 decided submissions**.
- **The index's Status** still said 0.1.1 was owed; it shipped 2026-09-10. The
  roadmap's own summary said A–F with a phase G open.
- **`verbs.md`** described a `deploy <site|export> <holiday>` that was never
  built. It did not list eight effects that were: `pack-database`,
  `translation-report`, `check-host`, `check-store`, `push-store`,
  `publish-index`, `deploy-site`, `rollback-site`. The calendar's
  `import-ics` / `export-calendar-ics` were missing from both it and the CLI's
  effect briefing, so an agent was never told they exist.
- **Q49** (should a bare run refuse without a database?) gained the evidence of
  2026-09-13: the trap recurred through the GUI, where the CLI's note cannot be
  seen.

## Where the code disagreed with itself

- **An error message contradicted the line above it.** A missing `token_file`
  told the operator a relative path resolves against *the folder the app was
  started in*; the code resolves it against the database's folder.
- **`quick::slug`'s comment said it matches the CSV import's slug.** It does
  not (no `_`, a length cap), and the difference is deliberate, because it also
  names iCalendar imports. The comment now says so rather than the function
  changing.
- **Niche Tools' "rewrite Antfarm paths" was clickable before Apply.** Pressed
  then, it wrote to nodes the merge was about to replace wholesale. It is now
  disabled until the merge is applied.

## Compiler warnings

A rebuild of every unit under `-Wall -Wextra` reported warnings in our own code
for the first time anyone had looked. Nearly all were `/*` inside a block
comment (glob paths like `site/index/*.json`). Two were one-line
`if (x) continue; y;` in the frozen Allomone dialect: correct, but exactly the
shape that hides a bug. The rest were an unused parameter and a redefined
`NOMINMAX`. **Our code now builds with zero warnings;** the 48 left are inside
vendored `stb_image_write.h`.

## Golden re-captured, and why that is safe

One comment inside `src/render/web/app.js` named `section_web.cpp`, and that
file ships inside every site. Correcting it moved `site/app.js`'s hash and
nothing else, so the golden was re-captured on that basis.

## Found and deliberately left

- **Nine effects exist only in the CLI**: `pack-database`, `backup-database`,
  `restore-database`, `translation-report`, `check-host`, `check-store`,
  `push-store`, `publish-index`, `read-flier`. Typed into the app's own console,
  they do nothing. `app.cpp` says of sync that *"a verb that only one front-end
  can call is a broken surface."* Closing it means moving their bodies from
  `main/headless.cpp` into the app layer, as was done for publishing on
  2026-09-02, and is its own piece of work.
- **Answered questions still under Open** (Q29, Q30a, Q31, Q32, Q38, Q50, Q13,
  Q16). Q50 says it was kept there on purpose, so this looks like a practice
  rather than neglect, and was not tidied.
- **Two `add_days`** (`ical.hpp` on strings through `mktime`, `rrule.hpp` on a
  `Date` struct). Both are correct and each suits its caller's representation.

## Verification

Build clean; 38/38 tests (`reduce_conformance` excluded, as before); all six
linters pass; 98 files within budget.

# The newsletter's look: themes, bands, banners, lists, images (2026-09-15)

The author sent six asks in one message while making a newsletter, and then a
seventh. Each is below with its cause, where there was one.

## What was asked, and what was wrong underneath

1. **"Banner image doesnt work on newsletter thing."**
   - **Cause:** the email's hero branch had never read `image` or `portrait`.
   - **Fix:** it draws the banner above the title (a photo behind text is a web
     idea Outlook cannot place). The sleek and sharp shapes run it edge to edge
     of the frame, and `portrait` is a round inset.
2. **Lists in narratives.** A line starting `- `, `* `, `•`, `1. ` or `1)` becomes
   a real list in both domains (`render/text.hpp`). Text with no such line
   renders byte-identically, so no existing narrative moved.
3. **Bands in the newsletter:** *"kinda like holidays themselves, we match some
   protocol into a different domain."*
   - **Built as that lens:** a band is a `bgcolor` cell, and the blocks inside
     render with a re-derived theme.
   - **Where the meaning lives:** the table is in `render/email_theme.hpp` and
     the blocks-and-domains concept.
   - **Full-bleed bands:** they close the frame's padded cell, which is now
     opened lazily, and take a row of their own.
4. **A newsletter theme separate from the website's**, *"like an apple or nike
   website made into a newsletter."*
   - **Axes:** `newsletter.*` has a palette, a type and a shape.
   - **Presets:** classic, modern, bold, editorial and night.
   - **Accent:** shared with the website until `newsletter.accent` is set.
   - **Tokens:** they are the literal inline-style text. Classic's tokens are
     the old literals, so classic output did not move a byte.
   - **Style tab:** a Newsletter tab offers the same choices.
5. **Images from the gallery, uploaded through the Antfarm.**
   - **Cause:** browsing a file into a block's image field ingested it into
     `assets/` and minted no `image` rune. The picture was in no gallery, had no
     url, and could not be published.
   - **Fix:** the image editor has "Choose from the gallery" and a status line.
     Every image brought in is adopted as a rune (`adopt_image`), and uploads
     itself when an `hol_imgbb` node and a key exist.
   - **Where the code went:** the editors left `app.cpp` for `ui/widgets.cpp`,
     and the ImgBB transport is shared with `effect publish`.
6. **"Side by side images dont work in an email preview."** The cause was #5.
   An image with no url was a grey box or nothing, so the preview could not show
   the layout being built. Now:
   - the preview draws a local-only image from its file, outlined in dashed red;
   - a PREVIEW notice at the top counts those images;
   - the render log names them;
   - the preview server serves `/assets/` image files for it, image extensions
     only.
7. **The follow-up: the directory's order.** *"alphebetical as a baseline ...
   'positive' tags ... 'negative' tags."*
   - **Fields:** `rank_up` and `rank_down` on `directory`, `image_grid` and
     `job_grid`, in both domains.
   - **How it sorts:** a stable sort over the block's own order. Earlier tags
     outweigh all later ones combined, and a bare tag matches under any
     namespace.

## Two breakages caught before anything ran

- **A shadowed variable.** Two event branches in `email.cpp` had a local `et`
  (an end time) that hid the theme variable `et`. The compiler caught it; the
  locals are `etime` now.
- **Private members.** The image status line began as a file-local function and
  could not reach `HormigaApp`'s private members. It is a member lambda inside
  `register_image_editors`.

## Verification

- **Golden render:** it moved `site/style.css` only, for the new list rules.
  Every page and the email preview hashed as before, which is the proof that the
  classic theme and list-free prose are unchanged. Re-captured on that basis.
- **Smoke test:** `tests/headless_smoke.sh` gained twelve checks. They cover the
  banner through its public url, lists in email and web, an accent band, the
  local-image outline and notice, the ranked order in both domains, and the
  modern preset's font, pill buttons and edge-to-edge banner.
- **Suite and linters:** 38/38 tests pass (`reduce_conformance` excluded, as
  before). Layering, the glyph-field linter, i18n and OKF links all pass.
- **Budgets:** `site.cpp` → 2305, `app.hpp` → 1180, and `email.cpp` gets a new
  entry of 1080, each with a reason. `app.cpp` shrank by 59 lines.
- **A visual check this time, not only markup:** a sample issue was rendered in
  classic, modern and bold and screenshotted with headless Edge. It read as
  intended.

## Not done

- **The ImgBB upload** has not been run against the real service from this
  change.
- **The gallery picker and the Newsletter tab** have not been seen in a window.
- **`site/index/directory.json`** (the organization-wide index) stays
  alphabetical. The live fragment each directory block refreshes from is ranked.
- **`download` and `audio` cards** in email still draw in their own fixed style
  rather than the theme's.

## Follow-up the same day: ordering edited like a filter

The author: *"the GUI part of it should be pretty similar to the filter tags
... i still want that smart search for tags. i want the GUI stuf to easily
delete tags or move them arround ... where the filter determines what even
shows up, these list order determine the order of things."*

`rank_up` and `rank_down` got an inspector editor kind, `taglist`, registered
beside the icon picker in `ui/builder_ext.cpp`. That kept `builder.cpp` and
`app.hpp`, both at their budgets, unchanged.

- **Search:** it offers tags from the DATA vocabulary as you type, like the
  filter does, and shows beside each how many entries carry it. The counts use
  the renderer's own rule, so `leader` also counts `role:leader`.
- **Order:** the tags are an ordered, numbered column, because order is the
  meaning. Drag a row onto another, or use the arrows, to move it; ✕ removes it.
- **Storage:** every change is one `set` of the same comma-separated text an
  agent writes, so the CLI and the GUI edit one value.
- **Labels:** the three blocks' labels got shorter, since the editor now
  explains the ordering itself.

**Verification.** The editor compiled, and 38/38 tests and all linters pass.
The desktop executable did not re-link, because the app was running. The editor
has not been seen in a window.

## Second follow-up: the Order section sits under Filter

The author's screenshot showed a build from before the editor, which explains
the plain text boxes. It also showed a real problem that a rebuild would not
have fixed: the editor would have been drawn at the bottom of the generic
field list, far from the filter it is the other half of. And the Void Maiz
inspector's own header, the block's name with a "+ tag..." box, sat right under
Filter and read as part of it.

- **The Order section.** The Builder now draws "Order (who comes first)"
  directly under Filter, with List first and List last as the same ordered,
  searchable, draggable lists.
- **One implementation.** The list is one function (`rank_list_ui`), shared by
  the section and the `taglist` editor kind.
- **No duplicate.** `rank_up` / `rank_down` are `hidden` in the generic
  inspector, so they do not appear twice.
- **A heading over the header.** "This block's own name, tags and settings"
  now introduces the generic inspector. The header row itself is Void Maiz's,
  so it is labelled from outside rather than changed.

**Verification.** 38/38 tests and all linters pass, and everything is within
budget. `bin/voidhormiga.exe` could not be replaced because the app was running;
the file was locked. The same link, written to a throwaway file name, succeeded
with no errors, so the desktop build is sound. The section has not been seen in
a window.
