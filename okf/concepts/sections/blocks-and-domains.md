---
type: Concept
title: Blocks & domains
description: "Documents are mantles of snapping block runes (Node Blocks applied); query-backed blocks resolve tag expressions at render time; renderer packs map (glyph × domain) to output; a theme is a renderer pack + assets."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

A document is a structure, not a program. One newsletter issue = one mantle of
**block runes**; one website page = one mantle of block runes; the block glyph
set is shared between them. Composition, rendering, and publication are three
separate seams.

# Composition: Node Blocks applied

The builder is Void Maiz's `"block"` shape kind used as intended:

- **Vertical document order is the `prev`/`next` linguine chain**, rendered as
  adjacency — flush blocks draw no wire, because adjacency IS the order.
  **Snapping a block into the stack is the ordering gesture**; tear, heal, and
  splice compile to one batch, one undo frame.
- **Typed value sockets carry connector-shape-as-type**: an `events` socket
  accepts only an `events` query block; the wrong shape doesn't fit, and the
  same verdicts gate snapping.
- **Layout containers** (two-column, grouped sections) are C-block containment
  via mantle-enter. (Inline body rendering is an open upstream view
  capability; we ask for it if layout blocks need it early.)
- **Arguments are face widgets**: multiline text, image, and date faces
  shipped upstream on our founding asks; each edit commits one `set`, staged
  so re-projection never yanks an active edit.
- **Blocks are property-rich, by charge** (author, 2026-07-16): the
  predecessor's cards carried many more properties than they exposed well,
  and every one of them must be modifiable here — the common few as face
  widgets on the block body, the full set in the inspector, and
  data-shaped arguments as typed sockets. A block glyph's field schema is
  its contract; nothing is trapped in a template.

Every placement, reorder, and argument edit is a logged, undoable, replayable
dispatcher command — an issue's construction is a transcript.

The seed block inventory: `hero`, `event_grid`, `image_grid`/`flyer_grid`,
`narrative`, `presenter_cta`, `job_grid`, `attendee_list`, `meeting_schedule`,
`footer`, plus query blocks per data glyph.

# Query-backed blocks and `materialize`

The killer mechanic. An `event_grid` block's source is a **tag expression**
(`@month:june AND type:event`) resolved through the Data holiday at render
time — the document stays live against the database. **`materialize`** is the
explicit, undoable bake: "snapshot these six events into the issue so it never
changes under me." Live-by-default, frozen-by-choice, and the freeze is a
logged command you can undo.

## `date:` — the predicate that made the mechanic true (2026-08-28)

A query-backed block's promise is that it gets **more correct as the database
fills in**. For a year it could not keep that promise about the one dimension
almost every block is actually about, because the filter grammar had no idea
what today is. `flier AND issue:aug2026` says exactly what it says; there was no
way to say *the ones that have not happened yet*. Measured, from the field: on
2026-08-28 a live home page carried a flier for an August 19 open house, and the
operator's workaround was two more hand-maintained tags plus a script to warn
when they went stale — three places to remember instead of zero.

**It is not a new grammar.** `maiz::node_matches` builds a *bag* of strings for a
rune (its tags, its name, `glyph:<g>`) and hands the bag to `Core::tag_match`.
`glyph:event` already proves the bag may carry facts nobody typed as a tag. So
the date predicates are more strings in the bag, computed at the render seam
from the clock — the grammar, the operators and Void Core are all untouched
(ground rule 4).

| in the bag | means |
|---|---|
| `date:past` | its day is strictly before today |
| `date:today` | its day **is** today — and it also answers `date:future` |
| `date:future` | it has not happened yet |
| `date:recurring` | no `date`, a `days` recurrence — and it also answers `date:future` |
| `date:undated` | no date of its own and no dated neighbour: answers none of the above |

**Derived, never stored.** A `temper` pass that wrote `date:past` into the
database would be a snapshot: right the day it ran and wrong every day after,
which is the hand-maintained tag again with a nicer name. These exist for the
length of one match.

Three rules the data dictated, two of which the field report named before we
could get them wrong:

1. **A recurring event is not in the past.** An event with only
   `days: "Last Friday of the Month"` must not be swept into `date:past` by a
   missing date read as year zero — that would drop every standing meeting off
   "Coming up", which is the thing a naive implementation gets wrong.
2. **A rune with no date borrows its event's**, one hop, by any relation and in
   both directions — the same generality `related_runes` keeps, for the same
   reason: which way somebody wired an edge is an authoring accident a reader
   should not pay for. When several events are linked, the latest wins.
3. **Today has not happened yet**, so an event at 6pm belongs on "Coming up" at
   9am. Nothing reads a time of day; `start_time` is free text in this model.

`today` is read **once per render**, not once per block — otherwise a render
starting at 23:59:59 can put an event in "Coming up" on one language's page and
in the archive on the other's.

**`ls --tag` cannot follow, and that is a boundary rather than a bug.** It is
Void Core's verb over Void Core's grammar, and that grammar has no clock. Since
the AGENT-GUIDE tells every caller to check an expression with it before putting
it in a block, `effect query '<expr>'` exists as the date-aware door: the same
evaluation the renderers and the Builder preview run, over the same data mantle,
printing each hit's `date:` verdict.

Implementation: `src/domain/date_query.hpp`. Pinned in `tests/dates_smoke.cpp`
with today held fixed, which is the only way a date rule is testable at all.

## `video` — the one block that reaches off the site (2026-08-28)

Asked for from a real content day: *"it'd be nice to include the youtube video
from the email."* Nothing took a video — `image_grid` takes images, the embeds
take widgets, and `link` navigates away.

Two decisions carry the whole block, and both are about what a field is allowed
to accept and what a page is allowed to send.

**A URL, not an embed code.** A volunteer asked for an embed code pastes markup,
and a field that accepts markup either escapes it (the block silently does
nothing) or interpolates it (any block author can inject script into a public
page). The field takes what people actually paste — `youtu.be/…`, `watch?v=…`,
`shorts/`, `embed/`, `live/`, Vimeo, a bare id — and yields `{provider, id}`
with the id shape-checked character by character. Nothing that passes can carry
a quote, a bracket or a slash, so the markup this project writes is the only
markup the block can produce.

**Click-to-play, not an iframe.** A plain provider iframe loads with the page:
every *visitor* is reported to the host whether or not they press play, and no
embed setting changes that. The page ships the organization's own poster and a
`data-embed` attribute; the iframe is created on click and not before, and the
rendered markup contains none. `youtube-nocookie.com` narrows what is stored but
does not stop the request, so it is the second line of defence rather than the
first. This is [security](/concepts/platform/security.md)'s render-seam rule in
its outward-facing form — at the seam where data leaves, what leaves here is the
reader's address. The organization's members include immigration and
survivor-services groups; that is the reason, and it is a reason to build the
careful version once rather than leave it to whoever places the block.

Two providers only. Each additional one is another URL shape to get right,
another privacy posture to check, and another thing that breaks silently when a
vendor changes a path.

Email gets a poster-and-button: no mail client plays anything, so the
newsletter's job is to get the reader **to** the video, not to pretend it can
hold one.

Implementation: `src/render/video.hpp` (parser and web markup together, because
the facade and the shape check are one decision).

## Saying what a block held back (2026-08-28)

A query-backed block that filters silently is indistinguishable, to a reader,
from a block with nothing in it. Measured: an Archive page gave an English
reader five tiles and a Spanish reader one, with no indication anything was
missing.

`image_grid` now prints *"Showing 1 of 5 - the other 4 are only in English."*
Three answers were available — filter silently, do not filter (hand a Spanish
speaker four sheets they cannot read), or filter and say so — and only the third
respects the reader. **Naming the language is what makes it worth printing**: a
bilingual reader can act on "only in English" and can do nothing with "not
available".

The rewrite that made counting possible also separated two silences that had
been identical from the outside and have completely different fixes: *hidden
because it is in another language* (a fact for the reader, printed on the page)
and *skipped because the file is not on this machine* (a fact for the operator,
named in the render log). The website is self-hosted; a rune whose `path` points
at nothing cannot be published in any language, and that had been reported
nowhere.

## `directory` — the block that publishes a person (2026-08-19)

`event_grid`, `image_grid` and `job_grid` were the only query-backed blocks,
and `map_embed` deliberately excludes contacts, so for eleven months there was
**no way to put a person or an organization on a Hormiga output at all**. The
first real website built with `render-site` therefore shipped a placeholder
where a member network's centrepiece goes. `directory` closes that.

It is the one query-backed block whose **query is not sufficient, and cannot be
made sufficient**. A rune reaches an output only if it also carries
`clearance:public`; no field, flag or query overrides that.

**This is `allo_web_hidden` inverted, and inverted on purpose.** `web-hide` is a
subtraction from a default of publishing, which is the right default for an
event and the wrong one for a person. The measured reason: a real database holds
84 contacts, 70 of them carrying a personal email — a block that trusted
`type:contact` would publish a phone book, and it would do it the first time
anyone wrote the obvious query.

**Two annotations, not two ranks**, which is [web-platform](/concepts/platform/web-platform.md) §4's
rule (*clearance is an annotation, not a rank*) doing real work rather than
being restated:

| tag | what it releases |
|---|---|
| `clearance:public` | the rune appears — name, role, bio, photo, website |
| `clearance:contact` | **additionally** its `email` and `phone` |

Either can be carried without the other, because "list me" and "print my phone
number" are different consents. A rank would have collapsed them into one and
then needed an exception to un-collapse them.

The render **reports what it withheld**. A directory that comes out empty
because nobody has been tagged is otherwise indistinguishable from a broken
block, and the person who has to fix it is the one reading the log.

## A rune name is a slug; what is printed is a field

Established for events on 2026-08-19 (`title_en`) and extended to people and
organizations on the same day (`display_name`), for the same reason and one
sharper one:

A rune name is a command argument — lowercase, hyphenated, ASCII. It is allowed
to carry a typo, an internal suffix, and a flattened acronym, because none of
those stop it identifying a rune. Published prose is allowed none of them. And
for an organization whose members write their names with diacritics the ASCII part is fatal on its own:
`jose-garcia` is not how José García writes their name, and no amount of
title-casing recovers the accent.

So: **no output renders a data rune's name.** Every display goes through the
field, which falls back to the humanized slug so nothing existing breaks and a
database is correctable one rune at a time.

# The bilingual engine

Bilingual output (EN/ES first) is a **founding feature with an engine, not a
configuration exercise** (author, 2026-07-16). The model: **parallel content
fields per block** — `text.en` / `text.es`-shaped pairs — filled by a
Translate-holiday call and hand-editable afterward, so translation is content
you can fix, not a render side-effect you can't. The engine ships wired
(defaults, not assembly — [the Antfarm](/concepts/platform/antfarm.md)): a language
axis (`lang:`) on documents, a "translate what's missing" pass as a logged
batch command, per-field provenance (machine vs hand-edited, so a re-translate
never clobbers a human fix), and domain renderers that pick the field for the
target language. Offline, the engine still stands — fields sit untranslated
and marked, and fill in when a Translate holiday is reachable or by hand.

# Domains: one block graph, many outputs

Rendering is per **(glyph × domain)**. A **renderer pack** maps each block
glyph to output for one domain, invoked by an `effect render <mantle>
<domain>` walk of the chain (host compute, per the compute boundary):

- **`email`** — table-layout HTML, inlined CSS, image constraints. Static
  cards. The newsletter's domain. Its hard physics: images must be loadable
  by the recipient's mail client — so **image `src` resolution is a holiday
  call at the render seam** (embedded/CID export, self-hosted URLs, or
  opt-in third-party URLs are three resolvers behind one seam;
  [developer questions](/developer_questions.md) Q9).
- **`web`** — a modern static site: the same `event_grid` renders as a
  responsive CSS grid; cards may carry the theme's JS sprinkle (filter,
  expand, lightbox). Pages, nav, and asset copying are web-domain concerns.
- **`print`/`pdf`** — recorded, not planned: same blocks, print stylesheet.

**A field honoured in one domain and ignored in the other is the failure mode
this design has to keep watching for.** Measured 2026-08-19: `title_en`,
`summary_en`, `detail`, `limit`, `sort` and `color` all shipped for `email` and
stopped there, so the first real website printed rune slugs, no summaries, and
document order under an explicit `sort date` — and the Spanish page printed the
same English cards, i.e. the bilingual site had one language. The same class hit
`caption_en`/`caption_es`, declared on three grids and rendered by none of them.

That is worse than an undeclared field, because `--describe` advertises it and
the Builder's inspector edits it. The rule that follows: **a declared field is a
promise made by the glyph, and every domain that renders the glyph owes it.**
Where the two domains genuinely differ — an `image_grid` `display` mode is
web-only because email has no JS — the glyph's label says so.

Blocks that make no sense in a domain declare it (an `attendee_list` may be
email-only). **A theme is a renderer pack + assets** — which is how site
themes ship without a page editor. Template engine lean: a vendored
header-only Jinja-like engine (inja) or a plain string renderer per glyph;
host compute either way.

**The embed blocks (built 2026-07-22): the map and the calendar cross into
the site as INTERACTIVE, READ-ONLY widgets.** The author's contract: "it
can't write or create any new information, but it should still be a whole
javascript element." Both blocks honor the per-domain physics:

- **`map_embed`** references a saved map VIEW (its home center/zoom, its
  rules). *Web*: a hand-rolled canvas slippy map (~90 lines vanilla JS, no
  framework, no CDN script — OSM tiles fetched by the visitor's browser with
  attribution) with pan, zoom, and marker name popups. Read-only **by
  construction**: the page receives positions and colors as data; no write
  path exists. *Email*: a static PNG composed at the view's home viewport.
  **The privacy seam is enforced in the renderer**: contacts NEVER reach
  either output — personal coordinates stay on the device
  (territory.md boundaries); the seam is the render loop, not a checkbox.
- **`calendar_embed`** renders the dated runes. *Web*: an interactive
  month / week / 3-day widget (vanilla JS, same three views as the native
  tab), each entry linking to a pre-filled **"add to Google Calendar"**
  template URL, and the toolbar offering **`calendar-<lang>.ics`** — a real
  RFC 5545 file generated beside the page (import/subscribe from
  Google/Apple/Outlook; the model grounding paying off). **One calendar per
  language**, because a VEVENT's `SUMMARY` is prose and prose has a language;
  `calendar.ics` remains as the English one, because it is a URL people paste
  into a calendar app and a subscription that 404s is worse than one in the
  wrong language. Times are **parsed, not sliced** — every time in a real
  community database is 12-hour with a meridiem, because that is what a flier
  prints, and fixed-offset arithmetic over `3:00 PM` puts a meeting at three in
  the morning. *Email*: an
  email-safe TABLE month (the discipline that fixed the newsletter once
  already). **Default query = events only** — incidents are published only
  when the block's query names them; publishing sensitive data is a choice,
  never a default.

Style resolution is the map's one engine (glyph default → view rules →
explicit tags), computed host-side into the JSON — the page's JS stays dumb
on purpose (data in, pixels out, nothing decided in the browser).

# The theme as a design system (2026-08-20)

The author's direction: *"revamp the style tab … think outside the box … mostly
think about websites right now. always remember that now we are developing for a
headless mode as well as the GUI,"* and separately *"remember we are kinda
remaking figma/squarespace here."*

**A theme is a set of AXES, and every axis is a `config set`.** That is what
makes the Style tab a *view* of the theme rather than an editor holding its own
copy: an agent writing `config set theme.contrast 1` and a person moving the
control are one change in one log. The tab contains no theme logic — the
arithmetic lives where the *renderer* calls it, so the number the tab shows is
by construction the number the website uses.

## Contrast is computed, not chosen

The failure this replaces: `color:#fff` was written into a dozen rules sitting
on `var(--accent)`. Correct for a navy brand, **invisible for a yellow one** —
and the volunteer who picks the yellow cannot fix it, because the fix is in a
compiled string.

So the renderer computes it. WCAG relative luminance and the contrast ratio, run
host-side over the theme the organization actually chose, emitted as finished
tokens (`--on-accent`, `--on-accent-dark`, `--prose`, a rescued `--muted`). The
page receives answers; **the browser decides nothing**, which is the compute
boundary, and which is also what makes the GUI's preview, the email and a
headless render agree without anyone remembering to.

Two features, deliberately kept apart:

- **`accent_lite` / `accent_dark` — the partners.** The same hue at two other
  lightnesses, for tints, hovers and chips. Empty means derived, so one chosen
  colour yields a coherent set; stated means stated, for an org with real brand
  values.
- **`contrast` — the floor.** Off / AA (4.5:1) / AAA (7:1). When on, quiet text
  is walked away from its background until it clears the ratio, keeping as much
  of the authored colour as the floor allows.

The dark variant **re-derives** rather than inheriting. A `--muted` chosen
against a white page, reused on a near-black one, is the same bug arriving from
the other direction.

## The frame is a promise

A card that a long URL pushes wider than its grid cell is not a styling
imperfection; it is text under other text. Three independent causes, all closed
as a floor over the whole page rather than per component — a rule you must
remember to add to each new block is a rule that will be forgotten:

1. an unbreakable run has no break opportunity → `overflow-wrap:anywhere`;
2. a grid/flex child defaults to `min-width:auto`, i.e. *never smaller than my
   content*, so even wrappable text refuses to shrink → `min-width:0`;
3. replaced elements ignore both → `max-width:100%`, and wide tables scroll
   inside their own frame.

## Spacing is a scale, not a literal

One `--gap` step for the whole page, and **equal heights across a row** by
default (the "distribute" alignment a design tool gives you). A ragged row is
almost never what somebody drawing a layout wanted, and a spacing value repeated
per component is a rhythm nobody can change.

## Icons are a WEB-domain feature, and that is a physics call

Inline SVG on the website: it inherits `currentColor` (so the contrast work
above applies to icons for free, with no second palette), scales with the type,
costs no request, and reaches no CDN — which matters for the same reason the
webfonts are vendored.

**The email domain does not get them, and the reason is the medium rather than
taste**: Gmail strips inline SVG. An icon that renders as nothing in the client
most of an organization's readers use is worse than the text separator it
replaced. This is the declared kind of per-domain difference — the same block,
honestly different output, because the two mediums are not the same machine.

Vendored: a curated 41-icon subset of **Lucide** (ISC, `vendor/icons/`),
regenerated by `tools/gen_icons.py`. Path data, not an icon font: a font is a
blocking download that renders as a box when it fails and is invisible to a
screen reader.

## Mobile is a shape question, not a font-size question

The month grid was the proof. Seven columns across ~340px is 48px a cell — room
for a date number and an ellipsis — so shrinking the type makes it unreadable
instead of illegible. **On a phone the month view becomes an agenda**: the same
data, the list shape, which is what the device is good at and what "what is on
this week" actually wants. The CSS hides the table at the same breakpoint so a
browser without JS still gets something legible, and the widget re-renders on
resize so rotating a phone does not leave the wrong shape.

# The website story (no separate site builder)

A site is a mantle of pages, a page is a stack of blocks, a theme is a
renderer pack, and **deploy is an Output holiday** (folder copy, GitHub Pages
push; rsync/SFTP later). What is genuinely new in the `web` domain is only
page/nav structure, asset copying, and the theme's JS — everything else is the
same pipeline the newsletter already exercises.

**Dogfood target, set at founding:** Void Hormiga's own website — downloads,
docs, screenshots — is built and deployed *by Void Hormiga*. First site the
`web` domain ships, zero PII, and it gives the project its distribution
surface. Eating our own cooking is the exit test for the publish phase
([roadmap](/roadmap.md) E).

# The builder's next form (author direction, 2026-07-22)

The author, after using the embed blocks: the Builder as it stands is
"EXTREMELY lacking… not blocks, but actual **components**. A **live
preview**. Similar to many website builders that exist today… there will
need to be a difference between a newsletter mode and a website mode, but we
KINDA have the essence of it already — dragging and dropping elements,
everything already withheld in a 'rectangle' or container… **we may be
ditching the blocks soon**."

Read carefully, the pivot is about the **editing surface, not the model**.
What survives untouched: content units in an ordered structure, query-backed
sources (`event_grid`'s tag query IS a component's data binding), bilingual
fields, per-domain rendering (email physics vs web physics — the author
names the two modes explicitly), the dispatcher (every edit still a logged
command). What changes: the Scratch-palette + abstract-block canvas gives
way to **direct manipulation of the rendered thing** — components arranged
in a live preview that looks like the output. The blocks' "rectangles" were
always a proxy for the containers the output actually has; the author is
asking for the proxy to become the thing.

The hard technical question is the live preview in a native ImGui app
(→ developer_questions Q20, with leans): approximate the output in ImGui
(fast, but two renderers drift), vendor an HTML renderer (litehtml-class —
faithful, heavy), or a **hot-reload browser preview** (render-on-edit to
site/, the browser auto-refreshes beside the app — honest WYSIWYG at near
zero cost, the web page previews AS a web page). Recorded as the direction;
the map/calendar widgets and the render packs carry over whole either way.

# Boundaries

- **Execution stays out.** Blocks never run; reduction-as-execution is a Node
  Blocks Phase B story upstream, not ours.
- **HTML preview is our compute**: `effect render` → open in the system
  browser as a viewer. No browser in the runtime, no embedded webview.
- **Not a CMS.** The editing surface is blocks; themes are code that ships,
  not a page-editor artifact.
