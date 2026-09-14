---
type: Concept
title: Territory — the map
description: "The map as a first-class workflow: location-faceted runes on a canvas over a swappable map SOURCE (not necessarily Earth); external conditions, population import, and tiles as Antfarm holidays; layers as tag-views; reactive visuals as Scry projections and interaction-net influence."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-22T00:00:00Z
---

**Territory** is Void Hormiga's fourth workflow — a **map**: people, incidents,
and conditions placed on a canvas you pan, zoom, and edit, laid over a map
**source** that is *not assumed to be Earth*. It absorbs the author's
`../Neighborhood` project (an Electron + Leaflet offline-first spatial app),
but re-founded on the family's architecture — because Neighborhood, built
before Void Core existed, independently reinvented most of it. The mapping is
therefore unusually clean.

Territory is a **form of data management** (its people/incidents are runes
like any other), yet **distinct**: it is canvas-native — spatial exploration,
placement, and reactive visuals — so it earns its own section beside Data,
the Builder, and the Antfarm ([workspace & sections](/concepts/sections/workspace-and-sections.md)).

# Neighborhood → Hormiga: the mapping

Neighborhood's pieces map almost one-to-one onto concepts we already have:

| Neighborhood | Void Hormiga / family concept |
|---|---|
| `Element` (id, type, lat/lon, tags, meta, free-form attrs) | a **rune** with a **glyph** and a **geo facet** — it already IS this |
| `Person` / `Incident` / `ExCondition` classes | glyphs: `person`, `incident`, `condition` |
| Incident `victims`/`aggressors`/`conditions`/`related_incidents` | **edges** (link relations between runes) |
| `DatasetManager` / handlers | the **SQLite Data holiday** + the dispatcher; handlers are gone (the dispatcher is the only door) |
| **`PropertyEngine`** ("reactive derived facts; never mutates, persists, or is a source of truth") | **[Scry](../VoidCore/okf/concepts/scry.md)** — the read-side projection verb; this is the SAME idea, already built upstream |
| `map_element` (data → color/opacity; changes never touch the data) | a **Void Maiz projection** — the view holds no truth; a map node is projected from a rune |
| `map_layers` (filter → a layer; tag/attribute membership; per-group color offsets) | **layers = saved tag queries**; color offsets = Scry selectors; the [tag system](../VoidCore/okf/concepts/tag-system.md) is the view mechanism |
| `LocationAPI` (geocode.maps.co) | a **Geo holiday** in the Antfarm (geocoding; opt-in, online) |
| base tile layer (`addBaseLayer(name, urlTemplate)`, `setBaseLayer`) | the **map-source holiday** (see below) — swappable, not assumed to be Earth |
| `map_gis_api` (overlay weather / external data) | an **external-condition holiday** — import feeds → condition runes |
| population CSV import | an **Import holiday / source node** (the CSV importer we already built, geo-aware) |
| `map_controls` (click → info panel, drag-to-place) | **canvas gestures → dispatcher commands** (the Void Maiz interaction grammar) |

The lesson: Territory is mostly *assembly* of pieces that exist, plus one
genuinely new thing — a **map canvas view** in Void Maiz (see §The canvas).

# The model: location-faceted runes

`person`, `incident`, `condition` glyphs, each carrying a **geo facet**
(`lat`, `lon`, `display_name`). Relations are edges, exactly as Neighborhood's
arrays intended: an incident links to its `victim`/`aggressor`/`witness`
contacts, to the `condition`s active over it, and to `related` incidents.
A `condition` carries **`radius`** and **`intensity`** and links to the
persons/incidents it `affects`. People and organizations from the Data
section can *also* gain a geo facet — Territory is a lens over the same org,
not a separate database.

# The map is a SOURCE, not an assumption ("don't assume Earth")

The load-bearing twist (author, 2026-07-20): **Hormiga does not make the map;
it loads one and overlays data.** The map background is a **holiday** — a
`map-source` interface — so the base can be anything with a way to *give* a
map:

- **Online tiles** — OSM / Google-class tile URLs through a Leaflet-class
  renderer (opt-in, online; managed in the Antfarm like any cloud node).
- **Offline tile pack** — vendored/region-extracted tiles, fully local.
- **An arbitrary image** — a scanned map, a floor plan, or a **fantasy map**
  (D&D). The map is just a raster with a coordinate transform; "lat/lon"
  generalizes to map coordinates. A dedicated map-*making* tool is out of
  scope — that is a future **Void Maps** the author may build; Territory only
  *consumes* a finished map.

This is why the source is a holiday and not a built-in: the same overlay
machinery works over Portland or over the Sword Coast.

**The source declares what it carries; the treatment calms it (2026-07-22).**
Two author asks sharpened the model. First: *"turn off base map text …
which translates abstractly to finding any displayed text and turning it
off, which could change depending on how a map is loaded in."* For raster
tiles the abstraction resolves cleanly: labels and POI icons are baked into
the pixels — you cannot strip them from a tile, you pick a style that never
drew them. So **a source declares whether it is labeled** (`BaseSource
{key, url, attribution, labeled}` — v1 ships OSM-labeled plus CARTO
light/dark *no-labels* styles, each with its own cache subdir
`tiles/<key>/` and its own attribution, shown live and quoted in every
export). A future vector source could truly toggle its label layer; the
UI contract stays the same — "labels off" picks a label-free rendering,
however the source achieves it. Second: **the base map takes a color
TREATMENT** — the author's brightness/saturation/opacity ask, scoped
honestly for rasters: *brightness* (a black veil = multiply) and *fade*
(a mid-gray veil = desaturate + decontrast toward gray), drawn OVER the
tiles and UNDER the data, so a busy base calms down while markers and
labels stay crisp. The PNG export applies the same treatment CPU-side —
what you see is what exports. True per-pixel hue/contrast/saturation needs
a tile shader the draw-list doesn't offer — recorded as a future upstream
ask, not faked. The base map is **singular** ("for us, google maps" — one
base, many views), so source + treatment are global config
(`ui.basemap`, `ui.basemap_brightness`, `ui.basemap_fade`), managed from a
"Base map" row atop Manage Views; per-VIEW look lives on the view rune
(`layer_opacity`, `layer_brightness` — how that view's ghosted layer
composites under the active one).

# Layers ARE tags; reactive visuals ARE Scry

Neighborhood's two best ideas fold into concepts we already have:

- **Layers = saved tag queries.** "A layer for `active`, a layer for
  `expert`; an element can be in both; layers offset colors for groups but
  never set them absolutely, and stay within the theme." That is the tag
  grammar plus a Scry *selector* (a projection-as-data) that maps a tag set
  to a **color offset**. Switching views = switching the active query; the
  data never changes. One grammar, every surface — the map's filter box is
  the same one the table and the newsletter blocks use.
- **Reactive visuals = Scry projections.** A map node's color/opacity/size is
  *derived*, never stored — Neighborhood's Property Engine constraints are
  Scry's constraints verbatim (derive-don't-mutate, no persistence, no I/O,
  not a source of truth). "This person is inside a high-intensity condition's
  radius → tint them" is a pure projection over (rune, conditions, geometry),
  recomputed on dispatch, feeding the view.

# Interaction-based visuals (the Void Maiz angle)

The author's flagship for Territory: **spatial interaction changing the
visuals** — weather affecting how people render, an incident tinting other
incidents within a radius. This is where Void Maiz's interaction-net posture
earns its keep (upstream Q9: "utilize the interaction-net structure wherever
possible"). Two readings, both valid and both Scry-backed:

- **Radius influence as derived style**: `map_computations`-style proximity
  (a condition's `radius` contains a person) yields a derived tint — a Scry
  selector keyed on a spatial predicate. Colors *offset*, never overwrite
  (Neighborhood's rule), staying within the theme.
- **Influence as edges**: when a condition's radius covers a rune, the
  overlap can be *materialized* as an `affects` edge (Scry → the one explicit,
  undoable bake), making the influence a first-class, queryable relation the
  interaction net can reduce over — not just a transient color.

The line to hold: **influence is derived by default, materialized only on
request** (Scry's discipline) — so the map stays live as conditions move,
and freezing a snapshot is a deliberate, undoable act.

# Where this might one day live (2026-08-21)

The author asked whether the map should become **Void GIS**, its own Void Maiz
application that Hormiga integrates — because the map needs to grow a great deal
(better markers, drawing tools, filtering and display, a UI/UX overhaul, and
eventually a map *builder* for non-Earth maps).

The answer is in [application boundaries](/concepts/foundation/application-boundaries.md)
and it is **not yet, and not for that reason**: the map is four layers, and the
complaint that prompted the question ("easier to drop pins in Google Maps and
screenshot it") lives in the **view** — which Void Maiz's custom-view ruling
keeps in the host under every possible separation. What *is* separable is the
**engine** (projections, tiles, geometry, formats), and it is separable cheaply
because this map is already generic: it draws anything carrying a `geo` field
and special-cases no Hormiga glyph.

The practical consequence for everything below: **write new map work against the
seam, not against `HormigaApp`.** Tracked as Q42.

# The canvas: the one genuinely new build

Everything above is assembly; the map **canvas view** is new. It is a
host-built Void Maiz view on the blessed seam (their custom-view ruling,
2026-07-18: `project_scene` in, compiled commands out; the view keeps its own
viewport as config-tier `view.*` state, undo-exempt). Concretely it needs:

- a **map canvas widget** — pan/zoom over the base-source raster/tiles,
  markers/shapes drawn from projected runes, click → inspector, drag-to-place
  → a `set geo` command, radius circles for conditions;
- **camera/viewport as substrate-shaped view state** (lat/lon/zoom), the same
  pattern the node canvas uses;
- (later, "just in case") a **3D-capable** variant — which would want a real
  graphics path and modular render scripts, and is the natural boundary with
  **Void Maiz XR**. Flagged, not planned.

This is an **upstream ask** — drafted for the author to relay
(`MESSAGE_FOR_VOIDMAIZ.md`): a map/spatial canvas view is the first Void Maiz
view whose coordinate space is *geographic*, not node-layout. Until it lands,
**mode 1** (below) needs no new widget.

# Canvas actions as first-class commands (the CLI/agent's map)

The author's realization (2026-07-21): a map canvas has **pseudo-GUI inside
the canvas** — tools you pick and actions you take *within* the view (place a
marker, drag it, draw a region, select everything in a radius, measure). These
are unlike the form/table GUI we've built, and they must be first-class in the
log/CLI so agents can drive and read the map too. Sorting out what's already
handled from what's genuinely new:

**Already handled — the WRITE side.** A custom view on the blessed seam emits
ordinary Core commands. A marker drag is `set <rune> geo "lat,lon"`; a place
is `rune new <glyph>` + `set geo`; a region is a rune with a shape facet. All
logged, undoable, replayable, and **reproducible by an agent through the same
verbs** — total observability, inherited. The map's actions are *already*
first-class in the transcript. Positions were always model content
(node canvas proved it); geo is the same.

**Genuinely new — three gaps**, all about making canvas actions NAMED and
LEGIBLE rather than anonymous gesture code:

1. **A host-registered action/tool vocabulary** (Void Maiz's to generalize).
   Today a custom view hand-rolls all gesture handling and emits raw commands.
   A map has *several* tools (place / move / region / radius-select / measure)
   and wants them declared — name + params + gesture binding + compile-to-
   commands — so they are introspectable ("what can I do on this canvas?"),
   not buried in host code. This generalizes the built-in gesture compilers
   (`compile_move`, `compile_add`, `find_snap`) to *host-registered* ones.
2. **Agent legibility — the READ side.** Agents drive the MODEL via the CLI,
   not the view. To "list locations near X" or "what's in this region," the
   tag grammar isn't enough (it's tag-based, not spatial/numeric). This wants
   a **custom query predicate** (spatial being ours) the CLI/Scry can use —
   which is likely **Void Core's** to own, relayed through Void Maiz.
3. **One definition, two front-ends.** The prize: define an action ONCE and
   get both a canvas gesture (Void Maiz) AND a CLI/voidscript verb (Void Core)
   that compile to the same logged command — so `map place contact @here` from
   an agent and a click-to-place from a volunteer are the same transcript
   entry. The bridge spans Maiz (gesture) and Core (verb + query); the ask
   asks Void Maiz to generalize the view-side and coordinate the Core-side.

The generalization is Void Maiz's to make — we bring **our needs + context**
(a map), not a prescribed API; future canvas apps (timelines, diagram editors)
are the reason the shape must not say "map." Drafted in
`MESSAGE_FOR_VOIDMAIZ.md` (2026-07-21).

# Two modes (the author's framing)

1. **Map-image builder (independent, buildable now).** Territory renders a
   **static map image** — base + placed markers — as an asset the newsletter
   and website blocks can embed (a `map` block, query-backed like the image
   grid). Needs only host-side rasterization (stb-class blitting over a base
   image), no live canvas. Feeds the existing output pipeline immediately.
2. **Live interactive map — v1 SHIPPED (2026-07-22), host-side.** A native
   slippy map on the blessed seam, no new upstream widget needed after all:
   web-mercator math + a background tile fetcher (curl → `tiles/` disk cache,
   temp-then-rename so a half-written tile can never poison the texture
   cache) + OSM raster tiles drawn via the ImGui draw list; **markers from
   geo-faceted runes**, color-coded by glyph (contact/organization/event/
   incident), click-select into the shared inspector, **marker-drag → the
   `move` action**, **right-click → place NEW contact/org/event/incident or
   an EXISTING unlocated rune** (both through the same ActionRegistry the
   `map` verbs use — one definition, two front-ends, live), wheel-zoom about
   the cursor, viewport persisted via `compile_camera(cam, "view.map.camera")`.
   **Maps are runes** (`map` glyph: source/center/zoom — one database, many
   maps; "New Earth map" mints one with `source=osm`); a non-Earth map is the
   same glyph with an image source (future). The `incident` glyph landed too
   (date/time/severity/geo; `type:incident` keeps it apart from planned
   events — the calendar view that will host both is future work). Original
   framing (kept for the record): explore, pan, place, filter by layer,
   watch reactive visuals — layers-as-tags and reactive tints are the next
   rungs. Depends on
   the Void Maiz map canvas, and — for the *web* domain — the JS/web renderer
   we just built (a live Leaflet-class map on the deployed site).

Mode 1 gives value before the widget exists; mode 2 is the first-class
section the author wants.

# Position channels: per-view locations and locking (the model, 2026-07-22)

The author's requirement pair: the same entity may sit at *different*
positions in different views (#9), and views can be **locked** so a move in
one moves all (#10) — with "maybe a view is like a tag?" left to us to
formalize. The model that makes both trivial:

- **A channel is a named position set.** `pos(rune, channel)` is stored in
  the rune's `geo` field for the distinguished channel **main**, and in
  `geo_<channel>` for any other.
- **A view carries a channel reference** (its `channel` field). The lookup
  rule is one line: `pos(rune, view) = geo_<channel(view)> ?? geo` — a
  channel's missing entries **fall back to main**.
- **Locking IS channel-sharing.** Views on the same channel read and write
  the same field, so "locked together" needs no synchronization machinery —
  it is identity, not mirroring. Locking = an equivalence relation over
  views, implemented by channel equality; the lock groups are the
  equivalence classes.
- **Unlocking is copy-on-write.** A view given a fresh channel stores
  *nothing* until something is actually moved in it; unmoved entities keep
  reading main through the fallback. Divergence is stored per-entity,
  lazily — fork cost is zero.
- **Re-locking is cheap and non-destructive**: set the channel back to main;
  the view's own positions stay stored, dormant, and revive if it unlocks
  again.

Answer to the "view as tag" musing: views don't need to be tags — the view
carries a channel, and channel identity replaces tag-equality bookkeeping.
Tags stay semantic; positions stay facets; the `located` tag means "has any
position." Writes go through the same `place`/`move` actions (they gained an
optional `field` param, so the CLI and canvas remain one definition), and
every divergence is an ordinary logged `set`.

**Implementation seam (found 2026-07-22, the hard way — twice):** Void
Maiz's projection fills only fields the glyph *declares*
(`fill_fields` iterates the descriptor's `fields` list), so a copy-on-write
`geo_<channel>` stored on a rune is invisible to the scene until the
placeable glyphs declare it. `reproject()` — the one seam every model change
passes through — scans the map runes for unlocked channels and, when the set
changes, **re-registers the placeable glyphs with the channel fields
appended** (`register_glyphs(core, channel_geo_fields)`;
`register_glyph` is documented upsert), then re-projects. Unlocking a view
makes its positions visible the same frame; no upstream patch needed. Until
this landed, per-view positions silently fell back to main — the lesson:
*a fallback that hides a broken primary needs a test that forks the primary.*

The seam bit AGAIN the same day: the first map-config fields
(`label_scale`, `show_labels`) were written to the view rune but not added
to the map glyph's declaration — `set` stored them, projection dropped them,
and the sliders read defaults forever ("label size doesn't work"). Elevated
to a rule: **every new view-rune field lands in the map glyph's `fields`
list in the same edit that introduces it** (`seed.hpp` — the declaration and
its accessor are one change, never two). The write path succeeding while the
read path silently defaults is exactly why this class of bug reaches the
author before it reaches a test.

# Views, rules, and exports (author directives, 2026-07-22)

- **Style RULES live on the map**: `[{tag filter → icon, color}]` — evaluated
  with the one grammar; explicit `icon:`/`color:` tags beat rules beat glyph
  defaults. Default rules ship with every new map (defaults, not assembly).
  Rules apply **live**: the marker-styling loop reads the parsed rules every
  frame, so there is no "apply" step — adding or editing a rule restyles the
  map the same frame. The panel lists each rule with its **live match count**
  `(n)` and a conflict flag: because the FIRST matching rule wins, a later
  rule that would also match an already-styled entity is **shadowed** (⚠ +
  a tooltip naming the winner). Rule **order is priority** — right-click a
  rule to Edit / Duplicate / Move up / Move down / Delete, or `map rules` in
  the Console for the full audit. *Built (2026-07-22).*

  **The single-quote discipline (found the hard way, 2026-07-22).** Rules
  round-trip as JSON on the view rune's `rules` field via `setjson`. But the
  dispatcher's arg tokenizer (`../VoidCore/core/src/dispatch/args.c`) STRIPS
  bare quote characters — it treats `"` as a shell-style quote and consumes
  it — so raw JSON reaches `setjson` with its quotes gone, cJSON rejects the
  now-unquoted text, and it lands as a broken *string*. The map's rules then
  silently vanish (the bug surfaced when a rule named with a *space* refused
  to apply: the space wasn't the problem, the stripped quotes were). The fix
  is a host discipline, not an upstream change: **any structured-JSON
  dispatcher argument is single-quote-wrapped** (`app.cpp` `json_arg`) —
  inside `'…'` the tokenizer keeps every inner `"` and space literal, and a
  literal `'` escapes as `\'`. Pinned by `spine_smoke` §7, which proves the
  bare form corrupts and the wrapped form survives through the real
  dispatcher. Lesson: *a value that crosses the command line is shell-quoted,
  even when the "shell" is our own dispatcher; a silent string-fallback on
  bad JSON hides the corruption until a downstream parse fails.*
- **Views are LAYERS** (author, 2026-07-22): like an art program, multiple
  views stack — each has a `visible` toggle (the eye checkbox in Manage
  views), and every *visible* non-active view composites on the canvas
  **ghosted** (its own channel's positions, its own rules' colors, ~43%
  alpha, no interaction). The ACTIVE view is the edit layer — the only one
  that receives clicks, drags, and placement. A view = camera + rules +
  channel + visibility. *Built.*
- **The rules engine is cross-view** (author, 2026-07-22): views + rules are
  not a map feature but a *styling engine* — the Calendar tab will style its
  entries with the same `[{tags → icon, color}]` rules, and the calendar
  gets the same export pair (**static** image and **dynamic** interactive)
  as the map. Recorded as design; the calendar build stays its own project.
- **A saved VIEW = camera + rules + channel + visibility (+ display config).**
  Many views per map, managed in the map side panel. **Export static: built**
  — Manage views "PNG" (or `effect export-map [view]`) composes cached tiles +
  rule-styled markers CPU-side (1200×800) into `exports/<view>-<stamp>.png`,
  credits "(c) OpenStreetMap contributors" in the toast, never touches the
  render seam's guarded fields (markers only — no notes-class data can leak
  into a pixel export). Marker **labels are rendered into the PNG** by
  sampling each glyph's coverage from ImGui's live font atlas
  (`ImFontBaked` + `TexRef._TexData` in the 1.92 font system — the legacy
  `GetTexDataAsAlpha8` asserts against the dynamic atlas) and alpha-blending
  a white-outlined dark label; the export is now legible, not just dots.
  `exports/` is gitignored: an export of real org positions is member data.
  *Still directed:* the Builder `map` block referencing a view, and **export
  dynamic** (a Leaflet-class component the deployed site carries).
- **Map CONFIG — "internally rules", surfaced like settings** (author,
  2026-07-22). The author's framing: some map knobs "are actually rules but
  modifiable like settings", kept as a *config* distinct from the app's
  Settings menu. Realized as ordinary fields on the **view rune**
  (`show_labels`, `label_scale`) — logged, undoable, ride the org like any
  content, and read by BOTH the live canvas and the PNG export, so what you
  configure is what exports. First knobs: show/hide labels and label size;
  the section is built to grow (marker size, label color, tile opacity are
  the obvious next fields). This is the seam by which "make the export show
  what I want" becomes per-view state rather than a global toggle.
- **The map and its list act alike** (author #1, 2026-07-22). The "On this
  map" side-panel list and the map markers share ONE right-click menu
  (`marker_menu_items`: select, center, icon/color submenus, remove-from-map,
  delete) — the list is a keyboard-reachable twin of the canvas, not a
  read-only legend. **Box / shift select** (author #2): shift-drag rubber-
  bands a selection over the canvas, shift-click toggles one marker, and a
  multi-selection swaps the inspector for a batch panel (tag-all, color-all,
  icon-all, remove-all, delete-all) — every batch still one command per
  entity through `pending_cmds`.
- **Hidden connections**: *derived* relationships (computed per frame, never
  stored) revealed by Settings toggles — the derive-don't-materialize
  discipline, live. **Spatial** proximity (distance-weighted haversine,
  weak springs in the physics view + canvas lines) *and* **temporal**
  proximity (2026-07-22: dated runes within `ui.time_proximity_d` days
  attract, weight falling linearly with day distance — mainly a physics-view
  input, where date clusters become visible neighborhoods). A future
  `materialize` turns a chosen proximity into a real edge. *v1 built.*

# The flagship use case (kept from Neighborhood/FUTURES)

**Location-personalized newsletters.** With a subscriber's location known,
the render context (`Scry`'s `Context = {locale, audience, date, role}`,
extended with place) orders events by distance and injects nearby conditions
("a road closure near your venue"). This ties Territory to **the Courier**
(email dispatch) — the two together turn generic mass-blast into genuinely
useful mail. Recorded as the north star, not the first step.

# Boundaries

- **Not a map maker.** Territory consumes a finished map source; authoring
  maps is a future Void Maps, not this.
- **Not a GIS suite.** Proximity/clustering helpers, yes; spatial analysis as
  a product, no.
- **Execution stays out.** Reactive visuals are *projections* (Scry), not a
  simulation engine running in the model; any baked influence is an explicit
  `materialize`.
- **Privacy is sharper here.** Location is `Neighborhood-grade PII`
  ([security](/concepts/platform/security.md) §3) — the "may not leave the device"
  render rule applies; a public web map must never leak precise personal
  coordinates.

# Phasing (post data-spine; evidence-ordered)

- **T0 — geo facet + import.** `lat`/`lon` on runes; the CSV/rescue importers
  gain geo columns; the Geo holiday (geocoding, opt-in) lands in the Antfarm.
  **T0 STARTED (2026-07-21, riding the canvas-actions reply):** the `geo`
  facet lives on contact/organization/event ("lat,lon", not assumed Earth);
  **`place`/`move` are ActionDescriptors** (`src/domain/map_actions.hpp`) on the
  shipped `maiz::ActionRegistry` — the command bar's `map place|move …` verb
  front-end calls the same compile the canvas gestures will (one definition,
  dispatched as ONE `batch` per Core's ruling), `map actions` prints the
  agent-readable manifest, and **`effect query near "lat,lon" <m>`** answers
  "what's around this point?" host-side (haversine; Core's blessed interim
  until composable where-predicates). Test-pinned in `spine_smoke` §5–6.
- **T1 — mode 1, the map-image block.** Static rendered map into newsletter/
  site; proves the source-holiday + overlay model with zero new widgets.
- **T2 — the map canvas view** (upstream ask) → the live Territory section:
  pan/zoom/place, layers-as-tags, the inspector shared with Data.
- **T3 — conditions + reactive visuals.** Condition glyph with radius; the
  external-condition holiday (weather/feeds → conditions); Scry-derived tints
  and radius influence; optional `materialize` of `affects` edges.
- **T4 — personalized-by-location newsletters** (with the Courier).
- **Continuous / research**: the 3D-capable canvas (Void Maiz XR boundary);
  a native Void Maps source.

# Built: marker SHAPES + export WATERMARK (author 2026-07-24)

- **Marker shapes.** A marker is no longer only a circle: `shape:` (a tag or a
  style RULE) picks **circle | pin (traditional teardrop, tip at the exact
  point) | square | diamond**. Drawn identically on the canvas, the PNG export
  (CPU raster), and the web widget (canvas JS) — one shape vocabulary, three
  surfaces. Set via the marker context menu's "Shape" submenu or the Rule
  Editor's shape dropdown. The pin's icon rides in its bulb.
- **Export watermark.** Settings/Style → Branding (the org's `org.name` +
  `org.logo`, config-tier, riding the database). Every map PNG export stamps a
  bottom-right watermark (logo thumbnail + company name on a translucent
  plate), read from config so it always reflects the saved database. Automatic
  branding for newsletters/sites.

# Built: drawable map SHAPES (author 2026-07-24) — #3 of the annotation batch

- **`mapshape` runes** — a drawn **rectangle or ellipse** over the map, stored
  as a rune (`kind`, `geo1`/`geo2` corners, `label`, `bestows`), taggable and
  **colored by the same rules/tags as markers** (a shape colors by its
  `color:` tag or a matching rule). Rendered identically on the canvas, the
  PNG export (CPU raster), and the web widget — under the markers, ~20% fill +
  a solid outline.
- **The draw tool** — a map-toolbar "Draw shape" (rect/ellipse); arm it, then
  click-drag on the map to place the shape (geo1/geo2 = the drag corners); Esc
  cancels. Click a shape to select; right-click for its menu (color, delete,
  edit).
- **The spatial tag, materialized** — a shape's `bestows` field names a tag;
  "Apply tag to entities inside now" walks the entities, and every one inside
  the shape's bounding box gets tagged (`+<bestows>`). This is the
  derive-then-materialize discipline: containment computed on demand, the tag
  written by an explicit action — so rules can then target those entities.
  *(Still ⬜: live "derived" containment highlighting before materialize;
  polygons beyond rect/ellipse; point-in-ellipse for bestowal, currently the
  bbox.)*

Verified: a blue rectangle + a red ellipse render in a PNG export, colored by
their tags, markers on top. *(Author 2026-07-24: the same geometry-creation
engine is meant to be reused for the Builder's shapes element — one engine,
two surfaces; noted for that build.)*

# Built: reference points (#4, 2026-07-31)

1b. **REFERENCE POINTS (editor-only gizmos, author 2026-07-24 → built
   2026-07-31).** A reference point marks a location but is **invisible in
   every export/webview** — Hormiga-only, like a gizmo. Its purpose: multiple
   markers can be **children of one reference point** — all sharing the
   reference's coordinates, but on the generated map **fanned out / spaced**
   around it instead of overlapping (a parent that de-clutters co-located
   pins). A clean answer to "10 orgs at the same building."

   **How it's built.** A `refpoint` glyph (fields `geo`, `label`, category
   Territory) is the gizmo rune; entities carry a `ref` field (declared on
   contact/organization/event/incident glyphs) naming their parent refpoint.
   The layout is `ref_fans(scene, channel)` — for each refpoint it groups its
   children and rings them **6 per ring** at `radius = 22 + ring*22` px
   (spiralling outward for more than six), returning each child's display
   position as *the refpoint's geo + a screen-pixel offset* so the fan stays
   spread at any zoom. All three render surfaces honor it: the **canvas**
   (`located_nodes` uses the fans; the gizmo draws separately as a hollow ring
   + crosshair + connector lines to its children, `◇`-prefixed label), the
   **PNG export** (`ex_fans`, `if (node.glyph=="refpoint") continue;` — gizmos
   never export), and the **web pack** (`web_fans` emits `dx`/`dy` in the
   marker JSON; the canvas JS adds `(m.dx||0)`/`(m.dy||0)`). The refpoint rune
   itself is **skipped on every export/web surface** — it draws only in-app.

   **Interaction.** Right-click empty map → *New reference point here* (mints a
   `refpoint` at the cursor geo). Right-click a marker → *Attach to reference
   point* (submenu of refpoints, sets `ref`) / *Detach* (clears `ref`).
   Right-click a gizmo → *Edit (inspector)* / *Delete (detaches children)* —
   delete clears every child's `ref` in the same commit before `rm`, so no
   marker is left pointing at a dead parent. All edits are dispatcher commands
   (`compile_commit` batches), replayable like everything else.

   **Move & offset.** The gizmo is **draggable** — press-drag it and the whole
   fan follows, because children are positioned relative to it (`set <ref>
   geo`). Dragging a **child** is a *re-arrangement, not a relocation*: it
   writes a per-child **`ref_off`** ("dx,dy" screen pixels, a hidden field on
   the geo-bearing glyphs) that overrides the auto-ring for that one child,
   never a new absolute `geo`. So a child of a refpoint has no standalone
   position — it is always *refpoint anchor + offset*, and moving the refpoint
   carries the hand-placed offset with it. `ref_fans` reads `ref_off` when
   present and falls back to the spiralling auto-ring otherwise. While dragging,
   the gizmo tracks the cursor and ghost dots preview where the children land;
   both snap on release.

   **Verified 2026-07-31 (headless):** a refpoint with three attached contacts
   exported to PNG — the three markers fan around the refpoint's location while
   unrelated markers stay put, and **no gizmo appears in the export**.
2. **Image markers (Snapchat-Map-style).** Entities with associated images
   (contact photos, org logos — the `image` runes + `image_url` legacy
   fields already in the model) can render ON the map as small round photo
   thumbnails instead of dots — **toggleable** (a map-config knob like
   labels). The texture cache already loads/uploads images; the work is
   marker rendering (circular crop, border, size scaling with zoom) and the
   privacy check: photos on an EXPORTED map are member data — the export
   seam must honor the same internal-notes-class guard before a face ever
   lands in a PNG.
3. **Geo holidays in the Antfarm.** Two nodes: (a) a **geocoding** holiday —
   an address-to-coordinates API (Nominatim-class) so entities can be placed
   by street address instead of hand-dropped pins ("actually use addresses");
   (b) a **weather/conditions** holiday — an external feed rendered as an
   **overlay view layer** (the T3 conditions phase, sharpened: the overlay
   rides the layers model — a weather layer is a view whose content comes
   from a holiday instead of a channel). Both are opt-in cloud nodes managed
   in the Antfarm like ImgBB/Supabase — keys in the vault, never in state.
