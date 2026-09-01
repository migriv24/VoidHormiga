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
  lives isolated beside `issue-demo`. *Still ⬜:* **delete + rename**
  (blocked on Core `mantle rm`/`rename` — MESSAGE_FOR_VOIDCORE 2026-07-23,
  interim is clear-elements); **export/import a document as a standalone
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

# Two builders, kept distinct

Newsletter = HTML components, vertical, email-safe, single document, no
scripts. Website = JS-interactable components, bands + pages, modern +
themed. Shared vocabulary; they diverge where the media do.
