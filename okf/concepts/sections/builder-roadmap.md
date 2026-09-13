---
type: Reference
title: Builder — the feature roadmap (a living checklist)
description: "The Builder's planned features as one status-tracked checklist: the B-track (core), the W-track (website), navigation, the Style axes, and the enablers (templates, shapes, image-grid modes). The design lives in builder.md; this is the do-list."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-23T00:00:00Z
---

The design and rationale live in [builder.md](/concepts/sections/builder.md); **this
file is the do-list** — every planned Builder feature with a status, so the
next thing to build is always visible. Statuses: ✅ built · 🔨 partial · ⬜
planned. The author asked for this document explicitly (2026-07-23).

# The identity (why this is not just a website maker)

A **data-bound Figma**: components carry a query and generate their own
content from tags; the layout is authored, the content derived. A specialized
ancestor of Void Maiz's **Allmusely** — keep the layout engine, component
protocol, and theme socket separable so they can inform it.

**Vocabulary (author asked to firm it up, 2026-07-23):** the placeable
content pieces (what were loosely called "blocks") are **ELEMENTS** — a
hero, a section header, a narrative, a link, an event grid are all *elements*.
A **page** is a set of elements; a **document** (newsletter or website) is a
page or set of pages; a **band** is a horizontal group of elements in a row.
"Block" is retired as user-facing; "component" stays as the internal term.
(Decision is soft — the author said "either way"; labels migrate gradually.)

# B-track — the core builder

- ✅ **B1** grid model + `doc` verbs (place/move/resize/remove/migrate), tested
- ✅ **B2** live preview (localhost server + browser auto-reload)
- ✅ **B3** near-WYSIWYG document canvas (real text, image thumbnails,
  generated grids; select / right-click menu / drag-reorder / span grip)
- ✅ **B4a** modern web render pack (fluid type, hero banner, scroll-reveal,
  hover motion, reduced-motion)
- ⬜ **B4b** palette KIND filter (newsletter=HTML vs website=JS components) —
  waits on the document-picker; the `document` rune + `kind` exist
- ✅ **click-to-edit-in-place** — double-click a text element (hero title,
  section header, narrative, footer) on the canvas → edit right there;
  Enter/defocus commits, Escape cancels (2026-07-23)
- ✅ **deselect** — Escape or click empty canvas → back to the page manager
- ✅ **undo/redo shortcuts** — Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z); every edit
  AND deletion is undoable (rm snapshots the mantle slice)
- ✅ **clean inspector** (2026-07-23) — builder-internal fields
  (row/col/span/page/link_to/band_*/map-view) are `editor:"hidden"` (a
  host-registered no-op editor), so the raw fields drop out; the Builder
  gives them proper dropdowns/handles instead. Band background = dropdown,
  full-bleed = checkbox, navigation target = dropdown, map view = dropdown.
- ✅ **FILTER builder** (2026-07-23) — query-backed grids get a tag-chip UI
  (add via a data-vocabulary picker, remove via ×) + an **all(AND)/any(OR)**
  toggle; no more typing `AND`/`OR`. Complex expressions fall back to a raw
  field (advanced escape hatch). The raw `query` editor is hidden.
- ✅ **legacy tools gated** — the old block canvas + tidy/migrate live behind
  Settings › "Show legacy tools" (off by default; the document canvas is the
  face). "Migrate to grid" retired from the main toolbar.
- ✅ **palette drag-and-drop** onto the canvas (2026-07-23) — the author's
  stated missing nicety: palette buttons are ImGui drag sources; the document
  canvas is a `BeginDragDropTargetCustom` over its whole rect. Dropping shows
  a blue insertion line at the cursor's row gap and inserts the new element
  THERE (rows renumber contiguous with a gap at the drop, robust to prior
  gaps). Click still appends. Single-window (no upstream ask needed after
  all).

# W-track — the website

- ✅ **W1** bands & horizontal layout — col/span translate to horizontal
  bands (built); **band-level styling BUILT (2026-07-23)**: a row's leader
  gives the whole row a background (none/tint/card/accent/dark) and can go
  **full-bleed** (edge-to-edge, content centered) — the modern
  alternating-section look; previewed in the canvas, set in the inspector's
  "Band" section. **Band background IMAGES + gradient BUILT (2026-07-23):** a
  band leader's `band_image` (picked from the org's images) makes the whole
  row a full-bleed PHOTO section — image behind a dark legibility gradient,
  white text over it (the hero treatment, for any band); a "gradient"
  `band_bg` option gives a themed accent gradient with no image. In-canvas
  preview draws the faded photo behind the row. *Still ⬜:* min-height,
  and named split/cards/feature presets (the layouts are reachable by
  drag today)
- ✅ **W2** multiple pages (`page` runes + `page` field; per-page files;
  page manager) — see Navigation for the nav half
- 🔨 **W3** richer modern sections — **quote / stat / divider BUILT
  (2026-07-23)**: pull-quote (accent border + attribution), stat/metric (big
  accent number + label, placed 3-across in a band for a stat strip),
  divider (line/dots/space). Each renders in-canvas, on the web (themed +
  reveal), and email-safe. *Still ⬜:* feature grids, CTA bands (composable
  now from a band + heading + button), logo/partner rows, `@icon` assets.
- ⬜ **W4** responsive control (per-band breakpoints; the pack already emits
  `auto-fill`/`clamp` — make it intentional)
- ✅ **W5** site chrome/meta (2026-07-23) — a **generated favicon** (accent
  square + the site's initial, inline SVG data-URI, no asset); **per-page
  `<title>` + description** (page's own → first narrative → site default);
  **Open Graph + Twitter cards** (social-share preview); **sitemap.xml** and
  a themed **404.html**; a Style-tab "Site" box for the deploy base URL +
  default description. **Hardened 2026-08-19 from the first real site:** the
  sitemap is **omitted rather than emitted invalid** when `site.base_url` is
  unset (a relative `<loc>` is rejected outright) and carries **both languages**
  with `xhtml:link` alternates; every page carries `rel=alternate hreflang` to
  its twin; a **root `index.html`** language door exists, without which a static
  host serves nothing at `/`; and the nav carries an **en ↔ es switch**, which a
  `link` block structurally cannot express (`target` is single-valued).
  *Still ⬜:* per-page OG image override, JSON-LD
- ⬜ **W6** more read-only interactivity (carousels/swipe, accordions, tabs)
- ✅ **local web host** (`hol_localhost` Antfarm node — the site served on
  localhost like a domain)
- ✅ **remote deploy node** (2026-08-19) — `hol_static_host` + `effect
  deploy-site`, the command on the node as `deploy_cmd` rather than a vendor
  endpoint in the binary. First effect declared `reversible: false`.
- ✅ **`directory`** (2026-08-19) — the block that publishes a **contact or an
  organization**, and the reason a member network's website could not have a
  Network page. The only query-backed block whose query is not sufficient:
  `clearance:public` to appear, `clearance:contact` to release an email or
  phone, neither overridable. See [blocks & domains](/concepts/sections/blocks-and-domains.md)
  and [security](/concepts/platform/security.md) §3.

# Navigation — authored, not automatic (author, 2026-07-23)

The author's redesign: navigation is **explicit**, not built-in magic.
"Anything could be a button" pointing to a page; pages can exist unconnected
to the site; the author controls the flow of navigation.

- ✅ **page manager** in the builder's nothing-selected panel (like the map's
  "On this map" overview): list / create / delete / reorder / set-home /
  header-nav toggle / select-to-edit — plus **page TAGS** (pages are runes;
  tag them to group/query/theme). Pages are managed here, not a cramped strip
- ✅ **preview any page as a newsletter** — the email preview renders the
  CURRENT page (the About page can be a newsletter), not always home
- ✅ **explicit `link` element** (a button/text link) targeting a page slug
  or an external URL — navigation is a placeable element, not a default; the
  inspector offers a **target dropdown** of pages (+ type a URL)
- ✅ **`link_to` on any element** (hero, image, section header…) — anything
  can be a button that navigates (same target dropdown)
- 🔨 **the header nav** is now opt-in (`in_nav` per page, default OFF) — a
  convenience, no longer the only/automatic nav
- ⬜ **nav components** (a reusable header/menu built from chosen links,
  shared across pages) — so a site-wide menu isn't re-placed per page
- ⬜ **flow overview** (see how pages link to each other — the author noted
  this "seems similar to nodes"; keep the framing, NOT the node-graph UI)

# The Style axes (parallel with the Builder — one system)

The Style tab decides how components look; developed alongside the Builder.

- ✅ colors (accent / background / text)
- ✅ **dark/light browser reactivity** (axis 3)
- 🔨 **presets** — Clean / Soft-neumorphic / Bold-maximal (axes 5/6 started)
- 🔨 **bold typography** — heading font choice + bold preset (axis 2 started)
- ⬜ **experimental navigation** styling (axis 1)
- ⬜ **motion design with graphics** — parallax, animated SVG, choreography
  (axis 4; all reduced-motion-gated)
- ⬜ **accessibility controls** (axis 7) — the pack owns the floor already;
  explicit contrast/focus/semantic controls planned
- 🔨 **logos, banners, per-target themes** — `config org.logo` /
  `org.logo_dark` now reach the site header and the favicon (2026-08-19), as a
  **pair** because a dark header swallows a black wordmark, swapped by CSS
  rather than JS. *Still ⬜:* banners, per-target (email vs web) theme overrides,
  a Style-tab picker for the pair.
- ✅ **responsive images** (2026-08-19) — `stage_site_asset` makes a downscaled
  derivative for tiles and avatars; the lightbox keeps the original. JPEG, not
  WebP: no encoder is vendored and one is not worth a new dependency (rule 5).

# Enablers (cross-cutting)

- 🔨 **DOCUMENTS AS PROJECTS — save & load (author 2026-07-23, MAJOR).** "there
  are LOTS of mini projects within Hormiga… the builder needs to save and load
  documents (like different projects)… things need to be SAVED as a file on
  disk and LOADED; storage = wherever the database saves (Antfarm-decided)."
  **First rung BUILT (2026-07-23): multiple Builder documents as MANTLES.** A
  document = a mantle (everything except the Data + Antfarm mantles); the
  Builder has a **document picker** (dropdown of documents + "+ New") that
  switches `cur_doc` — the canvas edits it and the previews render it. New
  docs get a starter hero. They persist in the database like everything else
  (so save = the org save). Verified: a second document (`spring-website`)
  lives isolated beside `issue-demo`. **Delete + rename BUILT (2026-09-02)** —
  the Void Core verbs they waited on (`mantle rm`, `mantle rename`) had landed
  some time earlier and nothing told this document, which is the small lesson
  about a roadmap entry that names a dependency. Rename needs no ceremony;
  delete asks, names the document and its element count in the question, and
  refuses on the last remaining document. Both live in `ui/documents.cpp`, split
  out of `builder.cpp` the same day. *Still ⬜:* **export/import a document as a standalone
  FILE** (a user template already is one — unify); the **Calendar** as a
  project too; and the full **Q19** multi-database/`.miga` flow (dedicated
  session). A saved user template is 80% of a saved document.
  **Second rung BUILT (2026-07-23): document file EXPORT/IMPORT.** "Save to
  file" writes the current document to `documents/<name>.json` (the portable
  name/kind/theme/commands shape shared with templates, via one
  `capture_doc_json`); "Load file" opens a `.json` as a NEW document mantle
  (never clobbers the current one — unlike a template's replace). Verified:
  issue-demo exported to 35 build-commands + theme. `documents/` gitignored.
  Import binds data by query, so a document file re-binds to whatever the
  target org's contacts/events are. *Still ⬜:* delete/rename (Core verbs),
  Calendar-as-project, per-document theme, Q19.
- ✅ **TEMPLATES (2026-07-23)** — start from a designed layout+theme, or save
  the current document as one. A template is a **named transcript** (dispatcher
  commands that build the elements + pages + links) plus a theme; applying one
  clears the current document and replays it as ONE undoable batch. Built-ins
  ship (a simple **newsletter** + a richer multi-page **website** — 3 pages,
  bands, a CTA link, grids, a bold theme); "Save as template" writes the same
  shape to `templates/*.json`. `effect apply-template <i>` for agents. *Next:*
  per-template thumbnails; template packs; templating a single PAGE or BAND
  (not just the whole document).
- ⬜ **shapes & graphics** — geometry components (rect/circle/line/divider/
  badge); reuse the map's future annotation geometry engine (one engine, two
  surfaces)
- 🔨 **image-grid display MODES** — a `display` enum on `image_grid`: **grid /
  masonry / carousel BUILT (2026-07-23)** (masonry = CSS-columns wall;
  carousel = swipe track with snap + ‹/› buttons — the "content-horizontal"
  swipe). Email always renders a simple table grid. *Still ⬜:* justified /
  single-focus / collage modes
- ✅ **`hero.portrait` (2026-09-03)** — a round inset in front of the banner.
  Every image slot a hero had was the BACKGROUND, so a face put there was
  cropped to the band; this is the shape `.card.person` already draws for a
  contact, on the block that lacked it. Deletes 23 lines of a client's
  `custom.css`. A board page and a "meet the director" page want the same thing.
- ✅ **`narrative.heading_en/_es` (2026-09-03)** — an `<h3>` above the prose.
  `narrative` was one `<p>`, so a role, an employer, a date range and four
  bullets were one paragraph at one weight, and the workaround in the field was
  `::first-line{font-weight:800}` — "the first line of this paragraph is
  secretly a heading". Line breaks (2026-09-02) made the structure expressible
  and left the hierarchy not. *Still ⬜:* the richer answer is
  [Q60](/developer_questions.md), a `role` glyph, which removes the prose parser
  downstream as well as the flat look.
- ✅ **DATA RUNES ARE BILINGUAL (2026-09-03)** — `bio_en`/`bio_es` on `contact`
  and `organization`, `alt_*` and `description_*` on `image`, legacy fields
  still read last. `directory` and `image_grid` publish an organization's own
  PROSE, and until now that prose was the one text on a bilingual site that
  could not be bilingual — the author's `site.languages` commitment applied one
  layer down. `lang_text` in `render/text.hpp` is the shared reader.
  *Still ⬜:* `job.description`, `resource.topic` and the civic set.
- ✅ **`download` block (2026-09-02)** — a file a visitor can keep, which the
  portfolio client's first deploy was blocked on: *"the one control this whole
  site exists for is the one thing on it Hormiga does not own."* A flier PDF,
  the bylaws, an annual report and a printable calendar are the same ask for an
  organization. `file` takes a path **or a `resource` rune name**, which is what
  finally makes `resource` reachable after being declared-and-rendered-by-nothing
  since it was written. Type and size are read from the staged file; `.html`,
  `.svg`, `.js` and friends are refused out loud, because model data arrives by
  import and by merge and a page on our own origin acts with our own authority.
  Email becomes a link and needs `site.base_url`. Markup and the refusal list in
  `render/download.hpp`. *Still ⬜:* a `download_grid`, which needs
  `directory`'s clearance conversation attached — a single named file needs
  none, because naming it is the consent.
- ✅ **platform sets (2026-09-08)** — the author's *"it should detect the system
  (linux, windows, mac), and then provide the correct download for hormiga."*
  `download` **and `link`** take `platform` (`windows-x64` / `macos` /
  `macos-arm64` / `linux-x64` / `any`); two or more on one `row` make that row a
  set, and `app.js` moves the visitor's own to the front and labels it *"For
  your computer"* from a `data-yours` the renderer wrote in the page's language.
  `link` carries the field because **the installer button is a `link`** — the
  binary lives on GitHub Releases, not in `assets/`.
  **It has no branch that hides anything**, which is the design: the old rule
  *"do not detect their OS and hide things"* became a property of the mechanism
  instead of a sentence an author has to remember. Family only, never
  architecture — a browser will not tell you which one it is on — and a device
  that reports itself ambiguously (Android and Chrome OS both say Linux) is
  matched to nothing rather than to the wrong card. An unknown `platform` value
  is reported at render time and treated as `any`. Renderer-owned rather than an
  author `<script>`, at the same trust level as the lightbox: see
  `render/download.hpp` and
  [the download page](/concepts/platform/download-page.md) §5d. *Still ⬜:* a
  second build for either of the other two cards to point at.
- ✅ **`audio` block (2026-09-02)** — a recording the organization owns, played
  from its own site. File + title + artist + duration + optional cover, staged
  into `site/assets/` like any image. **No facade**, unlike `video`: nothing
  leaves this site, so `preload="none"` on a native `<audio controls>` keeps the
  same "fetch nothing until they press play" promise using the standard instead
  of our JavaScript — and it works with scripting off. Email gets a card and a
  Listen button (Gmail and Outlook both strip `<audio>`), which needs
  `site.base_url` and says so when it is missing. Markup in `render/audio.hpp`,
  beside its own concern, the way `render/video.hpp` did it. *Still ⬜:* a
  `track_list` / `release` (field report A1 rungs 2-3), and an audio DATA rune —
  both waiting on [Q59](/developer_questions.md), because a glyph per medium may
  be the wrong shape.
- ✅ **ICONS IN THE GUI (2026-09-02)** — the author: *"i'd really like to get
  some icons in this application. like not just emojis … especially for the
  buildeer, like little icons next to the drag and drop button."* Nothing was
  downloaded: **Font Awesome 6 Solid has been vendored and merged into the ImGui
  atlas since the map markers shipped**, and had never been used past them.
  One table, `glyph_icon()` in `app/app_internal.hpp`, maps a glyph to a
  codepoint; the Builder palette buttons and their drag ghosts, the document
  toolbar, the Data "+ New" menu, the Notes tab and every dockable window title
  read it. Window titles use `Label###StableId` so nobody's saved dock layout
  moved. *Still ⬜:* the Antfarm and Allomone palettes, which Void Maiz draws
  (`maiz::edit_canvas`) — an upstream ask, not something to patch here.
- ✅ **`video` block (2026-08-28)** — a pasted YouTube or Vimeo **address**
  (never an embed code: a field that accepts markup is a script-injection seam),
  rendered as a **click-to-play facade** so no visitor is reported to the video
  host until somebody presses play. Poster is the org's own still; the block
  fetches nothing from the provider. Email domain gets a poster-and-button,
  since no mail client plays anything. *Still ⬜:* a playlist / multiple videos
  in one block, and a self-hosted `<video>` for organizations that own the file.
- ✅ **`date:` predicates in every block query (2026-08-28)** — `date:past`,
  `date:today`, `date:future`, `date:recurring`, `date:undated`, derived at the
  render seam and reaching through a flier's edge to its event. This is what
  makes a query-backed block keep its promise about time instead of decaying
  until somebody re-tags it. *Still ⬜:* a Builder affordance for it — the query
  picker offers tags, so today the predicate is typed by hand or scripted.
- ✅ **an honest partial-result line (2026-08-28)** — a language-filtered
  `image_grid` says "Showing 1 of 5 - the other 4 are only in English" rather
  than hiding four fifths of a section in silence. *Still ⬜:* the same line for
  `event_grid` / `job_grid` / `directory`, which filter by language nowhere yet
  but will.
- ⬜ **`doc convert`** (newsletter → website: rows → bands, static embeds →
  interactive)

# Reported by the author, 2026-09-02

Seven items from a hands-on session with the built app. The first six were
straightforward and are done; the seventh is a design conversation and has its
own question, [Q58](/developer_questions.md), alongside the one it opened about
custom data types, [Q59](/developer_questions.md).

- ✅ **1. A note could not be renamed.** Not a missing widget — a structural
  gap. The Data tab's detail pane has had a rename box since it was written,
  and `draw_data_body` skips `note` and `rule` runes because both have their own
  tab. So the one surface that could rename a rune never showed the runes in
  question, and the Notes tab printed the name with `TextDisabled`. The control
  moved to `ui/widgets.cpp` and both tabs call it; it returns a bool meaning
  *your node reference is now dangling*, because a rename reprojects and the
  caller must stop reading `sel`.
- ✅ **2. Linking "errored".** Linking images is not special and the CLI path
  was fine; the defect was in how the Data tab's relation text box reached the
  dispatcher. The command was built by string concatenation, so `goes with`
  became `--relation goes` (silently dropping `with`, writing a relation nobody
  asked for) and `maria's` produced an *unterminated quote (SPEC §6.1)* error
  aimed at somebody who had typed a word into a text box — almost certainly the
  "weird error". Both halves now go through `json_arg`, this project's §6.1
  quoter. `unlink` also stopped emitting a bare `--relation ` with nothing after
  it, which Void Core skips as a flag and which therefore widened the unlink to
  *any* edge between the two runes.
- ✅ **3. Brand images come from the asset library.** `org.logo` now offers
  **Choose** (a picker over the organization's `image` runes, with thumbnails)
  before **Browse**, and Browse mints an `image` rune via `ingest_image_rune`
  rather than pointing config at wherever the file happened to live. The
  author's framing is the general rule and it matches what the render seam
  already enforces one layer down: an image on a page is a rune — tagged,
  queryable, linked to its event, in the `.miga`. An upload that does not become
  a rune is an asset the organization cannot find again.
- ✅ **4. A Save button in the Builder.** Every edit was already persisted and
  Ctrl+S already wrote the document, so the missing part was never the writing —
  it was *being told*, in the tab where the work happens. The button sits beside
  the document picker with a live count, and `edits_since_save` is incremented
  at `dispatch_and_reproject`, the one door every GUI edit passes through. It
  deliberately over-reports: an extra "1 unsaved" costs a free, idempotent
  button press, and a missed one lets somebody close the app believing their
  afternoon is on disk.
- ⬜ **5. Text wrap and typing in narrative blocks on the canvas.** Not done,
  and not a small fix. With the honest scope in the author's own words:

  > We still don't have the goal of something like squarespace or wix, where
  > the user can just directly edit what things look like. However, of course
  > it'll be difficult, because they are different languages.

  The canvas is ImGui and the output is HTML; a canvas that wraps text the way
  a browser will is a text-layout problem, not a widget problem. It is the same
  wall the "canvas preview resembles the output" half of Q58 runs into, and the
  two should be answered together rather than separately.
- ✅ **6. A deleted page routes to the 404.** The diagnosis was in the author's
  own parenthetical — *"cuz i guess the link still exists"*. `render_site` wrote
  one file per page and removed nothing, so deleting a `page` rune took it out
  of the nav, the sitemap and the model and left `about-en.html` in `site/`,
  where the next deploy uploaded it again. The old page stayed live at its old
  URL showing content the database no longer contained. `site/` is a mirror of
  the document now — the same property the GitHub deployer builds its commit
  with, applied one layer earlier so it holds for every host — and once the file
  is gone the host's own 404 serves that URL, which is what was asked for. The
  prune is narrow on purpose: only `<name>-<lang>.html` at the top level, only
  for a language this site publishes, and only for the language being rendered.
- ⬜ **7. Registering custom elements.** [Q58](/developer_questions.md), which
  grew a sibling the same day: [Q59](/developer_questions.md), custom data
  types. The author's argument is that `contact`, `event` and `image` are
  universal in a way that `audio file` is not, and `3D object` or `WebGL Unity
  game showcase` obviously are not — so the answer to "we need more glyphs" is a
  registry rather than a longer built-in list.

# The newsletter pass (2026-09-13)

The author sent nine asks in one message while making a newsletter:
*"for most of these im thinking about the newsletter. because right now im
making the newsletter."* So the order was what was broken in output being
shipped, then what blocked the work, then the systems. The statuses below keep
two things apart that are easy to blur: what was **verified by rendering** and
what was **built and compiled but not yet seen in a window**. Items 1, 2 and 7
are gestures in the GUI, and a gesture is the one thing a headless check
cannot witness.

- ✅ **3. Side-by-side blocks in the newsletter.** The email renderer walked the
  document and emitted every block as its own full-width table. It never read
  `row`. The Builder placed two buttons side by side, the website honoured it,
  and the newsletter stacked them. It now mirrors the website's row driver: a
  run of blocks sharing a `row` becomes one table row, with cells sized from
  `span` and normalised so a short row still fills the width (an email table
  cannot hold a gap). A `cell_px` width reaches every `width=` attribute,
  because Outlook obeys that over CSS and a 572px image in a half-width cell
  breaks the 620px frame. Verified in the rendered HTML: RSVP in one 50% cell,
  Donate in the other, one row. It is the same shape as Click LaFont's report
  that week that `col` orders blocks rather than positioning them, and that
  field's label now says so.
- ✅ **9. An even more compact job listing, with icons.** `detail: line` gives
  one line per posting: title, organization, pay, place, closing date, email.
  Emoji icons appear on both `line` and `compact`, behind the existing
  `theme.icons` switch. The website already had Lucide icons on job cards; there,
  `line` means no description.
- ✅ **8. Image grid `fit`.** crop (same shape, trimmed, the default the gallery
  always drew) / whole (same shape, nothing cut off) / natural (each image keeps
  its own shape) / stretch. On the web it is a gallery class. In email, blank
  stays natural size, because `object-fit` is the one property here that
  Outlook desktop ignores: there, crop and whole fall back to a stretched image.
  It replaced a dead email branch that read `display == "thumb"`, a value the
  glyph never offered.
- ✅ **4. Icons on narratives, and an icon picker.** One vocabulary of 36 names
  (`render/icon_set.hpp`) drawn three ways: Font Awesome in the app, Lucide SVG
  on the website, emoji in email (Gmail strips SVG). The name is the Lucide
  name, and `tests/icon_smoke.cpp` holds every name to the icons that actually
  ship, so an icon cannot look fine in the app and vanish from a page. The
  picker is an inspector editor kind (`"icon":"icon"`), a grid of buttons named
  on hover. Rendering is verified in both domains; the picker itself is 🔨 not
  yet seen in a window.
- ✅ **5. `image_text`, an image and the words that go with it.** An explicit
  block with `image`, `side` (left or right), `icon`, heading, text and alt
  text; it appears in the palette automatically. On the web it is a two-column
  row that stacks on a phone. In email it is a two-cell table using the image's
  published url, or, with none, the text alone plus a render warning that says
  why. Verified in both domains; the canvas preview is 🔨 not yet seen in a
  window.
- 🔨 **1. Shift/ctrl-click multi-select.** It adds or removes. A plain click on a
  member keeps the group so it can be dragged, then narrows to that one on
  release. Delete or Backspace removes the group; the context menu's Width and
  Remove apply to all of it; dragging one member moves the group; a panel above
  the canvas offers Remove all / Full / Half / Third / Clear. Every group action
  is one batch, so it takes one Ctrl+Z. Built and compiled, not yet seen in a
  window.
- 🔨 **2. Flier and featured-event previews that show the tags.** `event_flier`
  and `event_feature` had no canvas case and drew their own glyph name. They
  now draw the flier (the named one, or the image wired to the event in the
  preview's language, which is the page's own resolution order), the event's
  title, date and place, and its tags as chips. Built and compiled, not yet seen
  in a window.
- 🔨 **7. A filter, as an expression.** The grammar needed nothing: block queries
  are Void Core's tag grammar, which has supported AND / OR / NOT, `&&` `||` `!`
  and **parentheses** all along, plus our `date:` predicates. What was missing
  was somewhere to write one. A `</>` button beside every Builder filter opens
  Void Maiz's code editor (the widget the Allomone tab uses, not the Allomone
  language, which is a different grammar) with highlighting, tag completion from
  the data the block is about, a parse check, and a live "matches N of M"
  computed by `query_matches`, the same evaluation the renderers run. Apply is a
  button only: the editor also reports a commit when focus leaves it, which
  would apply the very edit a person was reaching Cancel to discard. Built and
  compiled, not yet seen in a window. *Still ⬜:* the same button on the Data
  tab's and the Calendar's tag filters. `data.cpp` is two lines from its budget,
  so it wants its own small split first.
- ⬜ **6. An image-preview engine.** This is partly true already: `texture_for` is
  a path-keyed texture cache, and before this pass it drew a hero's banner and
  an image grid's thumbnails. The flier blocks simply never asked it. What
  remains is the engine the author means: decoding off the frame thread, a
  bounded cache that evicts, and a live preview of what a query-backed block
  would show. That is its own system, and it is planned as one.

# Two builders, kept distinct

Newsletter = HTML components, vertical, email-safe, single document, no
scripts. Website = JS-interactable components, bands + pages, modern +
themed. Shared vocabulary; they diverge where the media do.
