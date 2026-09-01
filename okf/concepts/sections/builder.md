---
type: Concept
title: The Builder — components on a grid, two builders, live preview
description: "The Q20 pivot, APPROVED 2026-07-22: the Builder as a multi-target document machine — Newsletter Builder (HTML components) and Website Builder (JS interactable components) — components placed on a grid, edited near-WYSIWYG against a live preview, every gesture a `doc` verb; styling lives in its own future Style tab."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-22T00:00:00Z
---

**STATUS: plan reviewed and decided by the author (2026-07-22) — the five
open questions below carry their answers inline; the B-track is the build
order.** Source directions: Q20 (the pivot), Q18 (grids + agent-legible
verbs), Q9/Q10 (render-as-if-real, local now / self-hosted domain later).

# The reframe

The author's founding insight for the pivot: **the Builder is another
visualization of the data — one whose purpose is export.** The map
visualizes space, the calendar visualizes time; the Builder arranges the
same runes for an audience. The consequences:

- Its spatial model is **grids, not layers** — documents are constrained
  media. Newsletters are the most constrained (email physics; "grids within
  grids"); websites are freer; future targets (PDF, presentations) each
  bring their own constraints.
- It is **one machine, many builders**: a Newsletter Builder producing
  **HTML components** and a Website Builder producing **JavaScript
  interactable components** — distinct by design, sharing bones.
- **Every gesture is a `doc` verb** (the Q18 elevation): place, move,
  resize, remove — one ActionDescriptor each, one logged command each,
  discoverable via `doc actions`, replayable headless. Agents build
  newsletters the way they place map markers.

# The model: documents, components, the grid

- **A document** is a set of component runes with a `kind`
  (newsletter | website). Documents are separate per target (❓ QC below).
- **A component** is a rune: glyph = component type (the existing block
  glyphs evolve in place — hero, text, image, event grid, jobs, map,
  calendar; new: button, spacer, columns), plus **grid placement fields**:
  `row` (vertical order), `col` + `span` (position and width within the
  row). Query-backed components keep their tag-query **data binding**
  untouched — an `event_grid`'s query IS its binding.
- **The grid**: rows stack vertically; a row is 12 units wide internally;
  components occupy col/span slots. The newsletter editor exposes rows of
  1–4 slots (email reality); the website editor may loosen toward finer
  spans later. Nested "grids within grids" = a `columns` container
  component whose children have their own row/col inside it (one level of
  nesting first).

**QA — grid granularity. DECIDED (author: "this is good"):** store as
12-unit spans, edit as 1–4 slots per row; the website editor may grow finer
control without a storage migration.

**QC — documents per target. DECIDED:** separate documents — *plus a
CONVERT operation*: "converting a newsletter document into a website
document" is a first-class `doc convert` (copy the components, translate
target-only ones to their sibling — static map ↔ interactive map,
table-calendar ↔ calendar widget; anything without a sibling carries over
disabled, flagged for review).

# The editing surface

- **The document canvas** (in-app): a vertical page outline — rows as
  strips, components as cards at their true proportions, real text
  snippets and image thumbnails where cheap. Select → the shared
  inspector; drag from the palette to insert; drag between slots to move;
  handles to change span; right-click menus throughout (the calendar/map
  menu discipline). Every operation dispatches a `doc` verb.
- **The live preview**: hot-reload in the browser — every edit re-renders
  the target output (email preview HTML / site) and the browser refreshes
  itself beside the app. The output previews AS the output; the native
  canvas is the arranger, not a second renderer.

**QB — preview refresh transport. DECIDED:** the **tiny localhost server**
(hand-rolled, no vendored dependency) serving the rendered output with
live-reload; the preview opens in the user's real browser beside the app.
The author's "browser inside the thing" musing is recorded as
considered-and-deferred: a true embedded browser means CEF/WebView2-class
machinery — the heaviest possible dependency against vendor-don't-depend;
WebView2 (a Windows system component, not a vendored lib) stays a noted
possibility if living beside a browser ever truly grates.

**QD — canvas fidelity. DECIDED — near-WYSIWYG, per target:** the canvas
"should 100% hold shape and form." Query-generated components (image
grids, event grids) render their GENERATED results — the actual rectangles,
with **mini previews of the images themselves** (the texture cache already
serves thumbnails); the map shows as a static image in-canvas. The
**newsletter canvas ≈ very close to the final newsletter**; the **website
canvas may take simplification liberties** (motion, JS interactivity, and
fancy type render as placeholders — the live preview holds that truth).

# The two palettes

- **Shared**: heading, text, image, spacer, columns.
- **Newsletter (HTML components)**: email-safe grids, the static map
  image, the table-calendar — everything renders in a mail client with no
  script, "as if real" (Q9): local assets stand in for the future
  self-hosted domain behind the same resolver seam.
- **Website (JS interactable components)**: the interactive map widget,
  the interactive calendar, filterable card grids, nav, buttons/links —
  everything is a read-only JS element (the embed-block contract).
- Components declare target compatibility (the blocks-declare-domains rule
  kept); the palette filters by the open document's kind.

**QE — where does styling live? DECIDED — the STYLE TAB, its own place
outside the Builder.** The author's split: the Builder is the *daily* tool
("drag and drop, edit some writing, check the Spanish translates, boom —
update website and send newsletter"); the Style tab is the *first-run* tool
("you'll want custom themes, colors, shapes, banners, footers… your own
logos, your own fonts — make it look your own"). Colors, themes, padding,
fonts, logos ARE editable — there, later. **Now**: a placeholder Style tab
with one or two live theme colors, but the THEME is structurally real from
day one — a first-class style set both render packs read, so the tab grows
into an existing socket instead of a retrofit.

# Rendering

The render packs survive whole: the email pack walks the grid into nested
tables; the web pack walks it into CSS grid + the JS widgets. The exit test
for the model swap: the migrated demo newsletter renders **equivalently**
before and after (same content, same order, table-for-table).

**The modern-web mandate (author, 2026-07-22):** "I really want the website
to be able to look really nice and modern… cool modern texts and fonts and
banner images and motion and animations." The web pack is therefore built
as a **themed modern renderer**, not a minimal one: fluid type scale,
full-bleed hero banners, scroll-reveal animations, hover motion — all
PARAMETERIZED BY THE THEME (the Style tab's socket) and all degradable (a
`prefers-reduced-motion` visitor gets the still version; the GUI-animations
accessibility principle extends to the published site). Modern look is a
render-pack property — components stay declarative; nothing in the model
knows what a scroll-reveal is. The newsletter stays simpler by physics, but
its pack keeps the same theme socket ("maybe we'll want cooler shapes and
stylizations later").

**Icons are assets (author, 2026-07-22):** many images in real org data are
tagged `icon` — icons and shapes are building material for both targets.
The `icon` tag becomes a first-class asset category: the palette's icon/
image pickers offer `@icon`-tagged images first, and small decorative
components (dividers, badges, footer marks) draw from that pool. (The tag
picker already treats bare tags as vocabulary; this makes `icon` a
*meaningful* one.)

**Sending is a NODE (author, 2026-07-22):** "we aren't just rendering these
documents — we'll be SENDING them with the corresponding nodes in the
antfarm." A document's outbound path is an Antfarm holiday: the newsletter
doc wires to a future **Courier/send node**, the website doc to the
**deploy node** (Q10's rung). Render produces the artifact; the NODE moves
it — the compute/holiday boundary, kept. This is design-now, build-later
(it waits on the dedicated Antfarm session the author called for in Q19).

# Migration (blocks → components)

- Block glyphs gain grid fields; nothing is deleted.
- One logged, undoable `doc migrate` converts an adjacency chain to rows
  (each block → one full-width row, order preserved).
- The Scratch-palette canvas stays until the component surface covers its
  jobs, then retires (the Q20 promise).

# Phasing (B-track)

- **B1 — the model + verbs**: grid fields, `doc` ActionDescriptors
  (place/move/resize/remove/migrate; convert lands with the palette split),
  the `document` glyph (kind), `doc actions` manifest; renderers order by
  the grid (chain fallback); headless tests (migrate the demo chain, assert
  render-order equivalence). The **Style tab placeholder** ships here — a
  couple of live theme colors, the theme socket real in both packs.
- **B2 — the live preview loop**: the hand-rolled localhost server +
  browser auto-reload on every edit (both targets).
- **B3 — the document canvas**: near-WYSIWYG rows (real text, image
  thumbnails, generated grids), selection, insert, reorder (buttons
  first), then drag-and-drop and span handles.
- **B4 — the palette split** (newsletter vs website kinds), `doc convert`,
  the website's modern component growth (banners, motion via theme), the
  newsletter's inner grids, the `icon` asset pool.
- **B5 — horizon**: PDF/presentation targets (recorded only); the
  self-hosted domain rung (Q10) picks up the resolver seam; the send/deploy
  NODES (with Q19's dedicated Antfarm session).

# The website is not a tall newsletter (author, 2026-07-23)

The author's correction after seeing the web output: "consider just how
different the nature of a website is from a newsletter… websites can be built
horizontally as well as vertically. websites can have multiple pages." A
newsletter is **one vertical document, email-constrained**; a website is **a
navigable set of pages, each a free 2-D layout**. Same component vocabulary,
a genuinely different container model. The website builder earns its own
feature inventory (the **W-track**, paralleling the calendar's C-track) — and
its own container model, below.

## The container model: BANDS (the horizontal answer)

Newsletters stack rows; websites stack **bands**. A band is a **full-width
horizontal section** — its own background (color, image, gradient, or none),
its own vertical rhythm — containing a grid of components laid out **across**
it. This is where horizontal layouts live:

- a **split band** (text | image, 6/6) — the classic alternating marketing row;
- a **cards band** (3–4 components across);
- a **full-bleed hero/banner band** (image behind, title over — already built);
- a **feature band** (icon + heading + blurb, repeated across).

So the grid model gains one level: **page → bands (vertical) → components
(horizontal within a band, the 12-unit grid we already store).** The
newsletter is the degenerate case — every band is one full-width column, no
backgrounds — so ONE model serves both; the newsletter editor just hides the
band-styling and multi-column affordances email can't honor.

❓ **QF — is a band a rune, or a property of its components?** (a) a `band`
container rune whose children carry `col/span` within it (clean nesting,
matches "grids within grids"); (b) a `band` INTEGER on each component (flat,
like `row` today — components with the same band index share a band).
**Lean: (a) a band rune** — bands have their OWN styling (background,
padding, full-bleed), which needs somewhere to live; a flat integer has no
home for that. Bands nest one level; components keep `col/span` relative to
their band.

## The W-track — website builder features

**W1 — bands & horizontal layout. BUILT (2026-07-23).** col/span translate
to the web render as horizontal bands (`.wrow`/`.wcol` CSS grid, responsive
stack under 720px), AND a row can be a **styled section**: the row's leader
carries `band_bg` (none/tint/card/accent/dark) and `band_full` (edge-to-edge,
content centered) — the alternating-colored-section look of a modern site,
set in the inspector's "Band" section (applied to every element in the row)
and previewed on the canvas. Full-bleed uses the `width:100vw;margin-left:
calc(50% - 50vw)` break-out with a centered `.band-inner`. *Still to build:*
band background IMAGES/gradients + min-height, and named split/cards/feature
presets (the layouts are drag-reachable already; presets are convenience).

**W2 — multiple pages & navigation. BUILT (2026-07-23).** `page` runes +
a `page` field on components; one HTML file per page; the local host serves
them at real URLs. See QG. The newsletter stays single-document.

**Navigation is AUTHORED, not automatic (author redesign, 2026-07-23).** The
first cut auto-generated a header nav from every page — the author: nav
"shouldn't be default built in… anything could be a button… we should
control the flow of navigation, where the buttons lead to what," and pages
should be able to "stand unconnected to the original site." Rebuilt:
- **Pages are NOT views** (they don't stack; they're separate) and are
  managed in the builder's **nothing-selected panel** — a map-style overview
  (`draw_page_manager`): create / reorder / set-home / toggle-header-nav /
  edit-settings (selects the page rune → its inspector) / delete (a deleted
  page's components fall back to home). A slim tab-strip only *switches* the
  page being edited.
- **The `link` / button component** is an explicit, placeable navigation
  element — target = a page slug or a URL, style = button or text link. Nav
  is a thing you PLACE.
- **`link_to` on any component** ("anything can be a button") wraps it in an
  `<a>` — a hero, an image, a section can navigate.
- **The header auto-nav is now opt-in** (`in_nav` per page, default OFF);
  a page appears in a generated header only if you say so. A fresh page is
  unconnected until you link to it.
- Target resolution is one helper: a known page slug → its file; a URL
  (`http…`/`/`/`#`) → itself. *Still ahead:* reusable nav components (a
  site-wide menu built from chosen links, not re-placed per page) and a
  flow overview (the author's "seems similar to nodes" — the framing, not
  the node-graph UI).

This is the "navigate to other webpages" the author named as the Builder's
direction — the data-bound Figma now has real pages AND authored flow between
them. Full status list: [builder-roadmap.md](/concepts/sections/builder-roadmap.md).

**W2 — multiple pages & navigation.** A website is a `site` with many
`page`s (home, events, about, resources, contact…). The nav bar is
generated from the page list (already generated from section anchors — this
promotes it to pages). Page-to-page links, active-page state, a shared
header/footer across pages, per-page slug → `site/<slug>.html`. In-page
anchor nav stays for long pages. **Now enabled by the LOCAL WEB HOST**
(2026-07-23, `hol_localhost`): the site is served on localhost like a domain,
so multi-page navigation between real URLs is testable exactly as it will be
when deployed — W2 became the clear next builder step the moment the website
stopped being a single file. *This is the direction the author named the
Builder heading toward: "our own data-centered Figma… for navigation to
other webpages."*

**QG — how do pages relate to the model? DECIDED + BUILT (2026-07-23).**
`page` runes carry each page's title/slug/order/nav-visibility; a component's
`page` FIELD names the page it belongs to (empty = the home page, ordered 0).
This is lighter than the "page owns bands via links" lean — it mirrors how
`row/col/span` already work (a flat field, declare-or-vanish), needs no new
ownership mechanism, and the doc canvas filters to the current page. The doc
canvas has a **page tab-strip** (switch pages; "+ Page" mints one, and the
first mint also establishes Home for the existing components). `render_site`
emits one file per page (`index-<lang>.html` for home, `<slug>-<lang>.html`
for the rest); nav is generated from the `page` runes with active-page state;
no `page` runes = the legacy single-page site (backward-compatible). The
**newsletter render ignores pages** (single document by nature — it renders
only home-page components). A future `site` rune (owning pages, for multiple
sites per org) is the natural extension; one document → one site for now.

**W3 — richer sections & modern feel** (extends the modern mandate to
structure, not just type/motion): alternating split layouts, feature grids,
call-to-action bands, testimonial/quote blocks, stat strips, logo/partner
rows — all theme-driven, all from the same declarative components. The
`@icon`-tagged assets feed feature icons, dividers, badges (icons are
building material — author).

**W4 — responsive control.** Bands reflow at breakpoints (desktop → tablet →
mobile); the editor previews widths (the live browser already does this by
resizing). Per-band "stack on mobile" is the default; the 12-unit grid
collapses to 1 column on narrow screens (the web pack already emits
`auto-fill`/`clamp`; W4 makes it intentional per band).

**W5 — site chrome & meta.** Favicon, per-page `<title>`/description, social
(Open Graph) cards, a 404, a sitemap — the difference between "a page" and
"a site." Mostly render-pack + a few page fields.

**W6 — interactivity beyond widgets.** The map/calendar widgets exist; add
image carousels, accordions/FAQ, tabs, lightbox galleries (have), smooth
in-page scroll (have) — all read-only JS, the embed contract. Filterable
card grids already ship.

**Newsletter stays deliberately narrow** by contrast: vertical bands only,
no backgrounds, email-safe tables, no pages, no scripts. The two builders
share components and diverge exactly where the media do. `doc convert`
(newsletter → website) lifts each newsletter row into a full-width band and
swaps static embeds for interactive ones (the plan's QC).

❓ **QH — build order.** W1 (bands) unlocks the most ("build horizontally");
W2 (pages) is the next-biggest leap but larger. **Lean: W1 first** — it's
the smaller change (extends the grid we have) and delivers the horizontal
layouts the author asked for; W2 (multi-page) second, since it restructures
the model (QG) and the canvas (page switching). W3–W6 accrete after.

# What this really is: a DATA-BOUND Figma (author insight, 2026-07-23)

The author named the shape of the thing: "we might be making a weird version
of Figma. In Figma the blocks are AGNOSTIC to what you put in them; here the
blocks actively have DATA associated with them and generative behavior (based
on tags or interactions)." That is the whole identity of the Builder and why
it isn't just a website maker:

- A Figma frame holds whatever you draw. A Hormiga component holds a **query**
  — an `event_grid` is bound to `@type:event`, an `image_grid` to `@icon` or
  `@flier`; add a matching rune and the component updates itself. The layout
  is authored; the CONTENT is derived (Scry's discipline, in the builder).
- So this is a **GUI builder for data-bound documents**. It rhymes with Void
  Maiz's planned **Allmusely** (a "custom Figma" / GUI maker for Void Core
  apps) — we are NOT building Allmusely, but we are building a specialized
  ancestor of it, and should keep the boundary clean so work transfers when
  Allmusely lands (the layout engine, the component protocol, the theme
  socket are the candidate hand-offs).

# The advanced-capabilities horizon (author, 2026-07-23 — planned, not now)

Directions recorded so the architecture leaves room; none are built yet.
Each is a **render-pack + theme property**, never a change to the declarative
model (the modern-web mandate, generalized):

**Style axes the author wants controllable (via the Style tab). The Style
tab and Builder develop in PARALLEL — one system, two faces (author,
2026-07-23): the Builder arranges components, the Style tab decides how they
look. Three axes shipped 2026-07-23; the rest are planned.**
1. **Experimental navigation** — beyond a top nav bar (side rails, overlays,
   scroll-driven, section-to-section). *Relates to W2/QG: the author notes
   Figma's navigation "seems similar to nodes" — page/section links ARE a
   graph; we keep that framing but do NOT use the node-graph UI for it.*
   *Planned.*
2. **Bold typography** — **BUILT OUT 2026-07-31** (started 2026-07-23): the
   Style tab now carries a **real font system** — separate **heading** and
   **body** font axes drawn from one palette (`kFontStacks`), plus a
   **type-scale** axis (Compact…Large → root `font-size`). The first three
   palette entries are **VENDORED, EMBEDDED webfonts** — Inter (sans), Source
   Serif 4 (serif), Space Grotesk (display) — shipped as OFL `.woff2` under
   `site/fonts/` and declared with `@font-face` in `style.css`, so a generated
   site renders **identically on every visitor's machine** (no CDN, no
   OS-font-stack lottery; ground-rule-5 clean). The rest are OS stacks. The
   NEWSLETTER pack keeps stacks (email clients strip `@font-face`). The `bold`
   preset still oversizes/uppercases display type. *Per-band font overrides
   still come later.* **The APP's own UI + the map PNG exports also gained a
   real face** — vendored OFL **Lato** loaded into the ImGui atlas replaces the
   pixelated default, and because the PNG export rasterizes labels/watermark
   straight from that atlas, exports sharpened for free.
3. **Browser-theme reactivity** — **BUILT 2026-07-23**: the Style tab's
   "Dark mode" toggle emits a `prefers-color-scheme:dark` variant; a visitor
   in dark mode sees a dark site. The theme socket now carries `ink` and a
   dark palette, not just accent+bg.
4. **Motion design with graphics** — richer than scroll-reveal: parallax,
   animated SVG/graphics, choreographed entrances — all `prefers-reduced-
   motion`-gated. *Planned.*
5. **Neumorphism** and 6. **Maximalism** — **STARTED 2026-07-23**, **EXPANDED
   2026-07-31** as named PRESETS (Style tab dropdown, now five: Clean /
   Soft-neumorphic / Bold-maximal / **Editorial** / **Glass**). Soft =
   same-color panels with dual soft shadows; Bold = hard edges, offset accent
   shadows, huge uppercase type; **Editorial** = print-like hairline rules, no
   card shadow, roomy prose; **Glass** = frosted translucent cards over the
   page (backdrop-blur). The socket also gained standalone **corner-radius**, a
   **secondary accent** (`--accent2`, used by gradient bands + hover), and a
   subtle **page texture** axis (none / dots / grid / hatch — self-contained
   CSS gradients, no image). Proves the socket carries shadow model + radius +
   density + texture, not just colors. More presets = templates (below).
7. **Accessibility (general)** — a first-class axis, not an afterthought:
   contrast, focus order, reduced-motion (the render pack already gates all
   motion), semantic structure, alt text (the `image` glyph already has
   `alt`) — the render pack owns the floor, the Style tab must not let a theme
   break it. *Ongoing floor; explicit controls planned.*

**Structural capabilities to build the above:**
- **TEMPLATES — BUILT (2026-07-23).** A template is a **named transcript**
  (dispatcher commands that build the elements + pages + links) plus a theme —
  the replayable-command philosophy, not a binary blob. Applying one clears
  the current document and replays the transcript as ONE undoable batch; the
  theme lands as config (kept in sync with the Style tab). Two built-ins ship
  and embody the newsletter/website divide the author called for: a
  **newsletter** template (one clean vertical column, email physics) and a
  **richer website** template ("Modern Community Site" — 3 pages, a two-column
  welcome band, a CTA link, event/image/job grids incl. a photo carousel, a
  bold dark theme). "Save as template" captures the current document →
  `templates/*.json`; `templates.hpp` holds the built-ins as data. *Next:*
  thumbnails, single-page/single-band templating, aesthetic-mode presets in
  the Style tab.
- **Click-to-edit-in-place** — click a heading/text on the canvas and edit it
  THERE (not only in the inspector). A near-term canvas nicety (B3 extension).
- **Shapes & graphics** — add geometry (rectangles, circles, lines, dividers,
  badges) as components. **Reuse target: the same geometry-creation engine the
  map's annotation feature will need** (territory.md "directed future" #1) —
  one geometry engine, two surfaces (documents + map), the one-definition
  discipline again.
- **Image-grid display MODES** — an image grid can present MANY ways (masonry,
  carousel/swipe, justified rows, single-focus, grid, collage). The author:
  "we may need to abstract the concept even more." Direction: `image_grid`
  gains a `display` field (a mode enum), each mode a render-pack layout + (for
  interactive modes like carousel) a JS behavior. The **swipe/carousel** mode
  is also what the author means by web "horizontal" at the CONTENT level:
  swipe left/right to reveal hidden content — distinct from the horizontal
  BAND layout (W1). Both are "horizontal"; one is page structure, one is a
  component behavior.

**Two senses of "horizontal" (author, 2026-07-23), kept distinct:**
- **Layout-horizontal** = bands with side-by-side components (W1, BUILT for
  the web render — col/span now translate).
- **Content-horizontal** = swipe/carousel components that hide content behind
  a gesture (an `image_grid`/gallery display mode, W6/advanced).

# Upstream (Void Maiz)

Nothing blocks B1–B3 or W1: ImGui + the shipped ActionRegistry cover the
canvas, bands, and verbs. Candidate asks (only if): cross-window drag
(palette → canvas — the author's one missing builder nicety), and — further
out — whether a `site` (multi-page, multi-band) or the advanced style engine
wants any library-level document primitives. **Allmusely is the eventual
convergence point** (Void Maiz's GUI maker for Core apps); until it exists we
stay host-composed and keep the layout engine / component protocol / theme
socket cleanly separable so they can inform it. Lean: host-composed now.
