---
type: Concept
title: Workspace & sections
description: "The application is three main sections over one core — Data (management), Builder (blocks, Scratch-shaped), Antfarm (infrastructure) — with Analysis and Territory as planned sections. Traditional-widget UI, registered with Void Maiz so the CLI stays complete."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

The author's structural charge (2026-07-16): Hormiga is not one canvas — it is
a full application with **three main sections**, each a different *kind* of
surface over the same core, the same mantles, the same dispatcher. A fourth
and fifth section are planned. One rule binds them all: **every section's
widgets are registered with Void Maiz's widget protocol, so the CLI works
completely** — a click in a form and a typed verb are the same command.

# The three main sections

1. **Data** — where the org's stuff is managed: upload images, edit contacts,
   resources, events. Mostly **traditional desktop UI** (tables, forms,
   detail panes, file pickers), NOT a node canvas — but every widget compiles
   its edits to dispatcher commands (the widget-protocol contract; nothing
   mutates silently). The centerpiece is the **table view** (upstream's
   queued [vh] item; interim host-side tables until it lands): sortable,
   filterable rows, one tag grammar in the filter box, row selection feeding
   the same selection/command model as every canvas. The Connections canvas
   (edges as wires) is a secondary view within Data. **UI/UX phase (begun
   2026-08-03):** the tab's three panes (sidebar | list | detail) are now
   **resizable** (two persisted `maiz::splitter`s, config `view.data_panels`);
   utilities (CSV import, date-tag temper) moved out of the toolbar into a
   detached **Data Tools** window. **Contact list & card views — built
   2026-08-03:** a `list ⇄ cards` toggle over the middle pane; people carry an
   `avatar` (photo or a hash-colored initials placeholder), so every row/card
   shows a face (see [data model](/concepts/foundation/data-model.md)). **Typed inspector
   for people — built 2026-08-03:** contact/org get a curated form (avatar +
   `role` **dropdown** via a registered `enum` editor with "add new", the
   fields that matter, no map-display noise like the "size"/`label_scale` that
   used to leak in), a plain-language **Location** section (map status +
   place/remove, no raw lat/lon box when unmapped), and **connections moved to
   the bottom** for every kind. **Tag-filter builder — built 2026-08-03:**
   `draw_tag_filter`, a reusable chip bar — click tags to add conditions, click
   a chip to negate, an all/any (AND/OR) connector, a "+ filter" type-ahead over
   the full tag vocabulary — compiling to the one grammar (raw box kept as an
   advanced escape hatch); the coming rules engine reuses it. *Still queued for
   the phase:* the same typed + card treatment for events/resources; **resource
   default-images + filetype limits**; then the **rules engine** (Q21).
   Two bigger pieces are deferred by the author: a **data rules engine**
   (highlight-by-tag; possibly a blocks-for-logic engine — → Q21) and **Notes**
   as its own section/engine (→ Q22).
2. **The Builder** — Scratch-shaped, deliberately: a **palette panel you drag
   blocks from** (categorized, searchable), the block canvas where stacks
   snap, an inspector for the property-rich block arguments, and a preview.
   The MIT-Scratch layout is the usability precedent, not just the block
   geometry. See [blocks & domains](/concepts/sections/blocks-and-domains.md).
3. **The Antfarm** — the node graph ([the Antfarm](/concepts/platform/antfarm.md)),
   plus the layer the author flagged as missing: **user-friendliness**. The
   graph alone is a diagram, not a tool. The section needs status-at-a-glance
   (per-node health cards, last-sync, errors surfaced plainly), guided
   add-a-holiday flows (a wizard that compiles to the same `holiday add`
   commands), and plain-language explanations on every node. Power users get
   the graph; everyone else gets the dashboard over it.

4. **Territory** — the **map** ([territory](/concepts/sections/territory.md)): people,
   incidents, and conditions on a canvas over a swappable map source (not
   assumed to be Earth). Promoted to a fourth main section (author,
   2026-07-20): maps are a *form* of data management but canvas-native
   (pan/zoom/place/reactive-visuals), so they earn their own workflow. It
   absorbs `../Neighborhood`; its full analysis and concept mapping live in
   the [territory](/concepts/sections/territory.md) concept.

Sections are chrome, not silos: one core, one undo history, one command bar
and log strip visible everywhere. The command bar is global precisely
because the sections are just views.

# Panels & windows (the four-workflow overhaul)

With four real workflows (Data, Builder, Antfarm, Territory), the author
flagged (2026-07-20) that the current fixed tab-and-splitter shell is not
enough: panels should be **movable and flexible — FL-Studio-style, where
everything is a window** you can rearrange, float, and re-dock. This is a
usability necessity, not polish, once four canvas/editor-heavy modes coexist.

**Resolved and shipped (2026-07-20): ImGui docking.** Void Maiz's author
retired the Q11 "no docking framework" lean — hand-rolling a pane manager
would be *more* of a framework than turning on the docking feature of the
ImGui we already vendor (vendor-don't-depend favors using more ImGui, not
less) — and re-vendored `imgui v1.92.1-docking`. Each workflow is now an
ordinary dockable window: **Data / Builder / Antfarm / Map tabbed in a
central node, a Console (log + command bar) docked below**, all
drag/float/tab/re-dockable, layout persisted to `imgui.ini` (the user's
arrangement wins over our seeded default on later launches). A VISIBLE
section window is the active context (it makes its mantle active and draws);
deterministic under the default tabbing. `splitter` still handles the
inside-a-section splits (canvas | inspector).

*Caveat, reported upstream*: Void Maiz's `enable_docking()`/`begin_dockspace()`
helpers shipped **inert** — both guard on `#ifdef ImGuiConfigFlags_DockingEnable`,
an enum not a macro, so always false (enable is a no-op; begin_dockspace never
submits a DockSpace and returns 0 → a null-node segfault if used as documented).
We drive `ImGui::DockSpace` directly host-side (correct `#ifdef IMGUI_HAS_DOCK`,
seed-before-DockSpace) until the one-token upstream fix lands
(`MESSAGE_FOR_VOIDMAIZ.md`, 2026-07-20). **Allmusely** — a future Void Maiz
extension, a "custom Figma" for GUI — remains the eventual general answer;
docking is the interim that makes four workflows livable.

**Also done (2026-07-20)**: bigger text (`FontGlobalScale` ~1.2) and roomier
spacing (frame/item padding, rounding) — the author's "bigger text and better
spacing" ask, a one-call style pass in `apply_theme`.

**Closable windows + the Windows menu (2026-07-22)**: every panel — the four
sections, Calendar, Settings, Console — carries an ImGui **'x' close button**
(FL-Studio expectation: windows come and go), and a **Windows** menu in the
menu bar lists them all with checkmarks to reopen anything closed. Closing a
section window just hides it (state is untouched — the model doesn't know
what's on screen); the Windows menu is the one place a lost panel is always
recoverable from.

# Publish — the operator's tab (2026-08-20)

**A dockable window, not a fifth main section**, and the distinction is the
author's:

> *we are not making hormiga purely a website builder, websites happen to be 1
> of the things hormiga can do. but i guess, temporarily, we'll need like a sort
> of website publishing and managment tab (which later might take on a bigger
> role with hosting)*

The four sections are what this application **is** — data, builder, antfarm,
map. Publishing is something you **do** to one of them, occasionally. So it is a
tab, and promoting it later is one line.

**It is written against `hol_static_host` NODES, not against "the website".**
That is what keeps the tab honest to the sentence above: a second kind of
publish target — a newsletter send, a mirror, a hosted calendar feed — arrives
as another holon and another row, not as a rewrite. The panel lists what the
Antfarm holds.

**The Antfarm is where a backend is WIRED; Publish is where one is OPERATED.**
Those are different jobs, usually done by different people and always at
different times, which is why a node-graph canvas is the wrong surface for a
green button and the right surface for a typed port.

## What it shows, and the order it shows it in

Four questions, visible together, because the fourth is irreversible:

    what am I publishing · where does it go · what does it look like · go

- **Build** → `effect render-site` (both languages; the house rule is both).
- **Preview** → the built file, on this machine. *Nobody else sees it.*
- **PUBLISH** → `effect deploy-site`. A wide green button, set apart, and the
  only control in the application that opens a confirmation.
- **History** → the `deployment` runes, newest first, with `[view]` before
  `[restore]`.

**Preview and publish are different words on purpose** (author): preview says
*where it goes* (a file), publish says *who sees it* (everyone, now). The
confirmation quotes the effect's own `consequence` string **verbatim** — the
sentence written for the person deciding whether to hand an agent
`--allow-effects` is the right sentence here for the same reason, and a GUI path
must never give the person at the keyboard less warning than the agent gets.

`[view]` before `[restore]` is the detail that matters: every managed host keeps
each deployment at its own permanent URL, so *looking at* the old version before
deciding is one click. That is what turns a rollback from a frightening button
into an ordinary one.

# Planned sections

- **Notes** — **bare-bones tab BUILT (2026-08-03)**, its own dockable window
  (`draw_notes_body`): a list of `note` runes + a plain multiline text editor,
  every edit a `setjson` command. Notes carry **tags** (chips + a type-ahead to
  add/remove) and a **filter search** (the same `draw_tag_filter` chip bar + name
  search as the Data tab). Notes **left the Data tab** (removed from the Data
  palette and list) to live here. Deliberately minimal for now — the real **Notes
  engine** (markdown / Obsidian interop, "note → newsletter", and **Allomone
  acting on note text** via textual-embedding models for search/linking/
  semantics) is a **far-future** build (→ Q22). The text editor itself will
  eventually be the **shared document-writer engine** it splits with Allomone
  Script (one editor core, per-surface language services — see
  [Allomone Script](/concepts/allomone/language.md)); today it's a plain box.
- **Allomone (the rules engine)** — a **new main section** (author, 2026-08-03),
  its own concept folder: [Allomone](/concepts/allomone/index.md). A visual,
  declarative rule language over the rune graph, edited as typed blocks; large
  enough to warrant its own tab (block palette | canvas | inspector | live
  "what this styles now" preview). It **subsumes the map/calendar rules engine**
  — those are now **frozen** (maintained, not extended) and migrate onto Allomone
  at parity. Built after the tag-filter widget; phased plan in the
  [roadmap](/concepts/allomone/roadmap.md). (→ Q21.)
- **Calendar** — **v1 BUILT (2026-07-22)**, its own concept:
  [calendar](/concepts/sections/calendar.md). Dated runes on Month / Week / 3-day
  grids (anchor + stride navigation, today highlighted); **events and
  incidents visibly distinct** (warning mark + red accent, always); styled
  by the selected map view's rules through the one shared parser — the
  cross-view styling engine made real; entries select into the one shared
  inspector. **Static PNG export shipped** (the newsletter's month grid,
  CPU-composed like the map export; `effect export-calendar`). Model grounds
  in RFC 5545, view vocabulary in FullCalendar; year view = a
  GitHub-style density heat map (directed). Still ahead: the tag FILTER on
  exports (privacy-blocking for anything published), recurrence (`days` →
  concrete occurrences), the dynamic web export, temporal-linkage badges.
- **The physics graph view** (directed, 2026-07-22) — the Data section's
  connections view becomes a Gephi-class force-directed visualization (NOT
  the node editor): community/neighborhood detection, importance-sized
  nodes, an analysis progress bar. Host-buildable on the custom-view seam;
  analytics are host compute.
- **Settings** — *built 2026-07-22*: a dockable tab; every knob is
  config-tier (`ui.*`) so preferences are logged and ride the org. Home of
  the **hidden-connections** switches (derived relationships, off by
  default — spatial proximity first). A **Visual effects** group (UI/UX phase,
  2026-08-03) holds the **granular, opt-out** polish toggles: **Drop shadows**
  (`ui.fx.shadows`), **Accent highlights** (`ui.fx.highlights`, re-themes
  live), **Animations** (`ui.animations`), **Translucency/blur**
  (`ui.fx.blur`), plus **Performance mode (all off)** / **All on**. The
  animation-vs-snap split (and now the whole effects set) is a real
  accessibility + compute choice, not polish — every animated/decorated
  affordance must have an instant, correct fallback when its flag is off (the
  map's drag-grow degrades to a plain cursor-follow; the panels lose their
  shadow but not their layout). The old **Advanced** group keeps **Show legacy
  tools**.
- **Analysis (a mini-Tableau)** — internal analysis tools over the org's own
  data: charts and pivots built from **tag-grammar queries** (`@month:june
  AND type:event`, grouped by axis), host-computed, rendered as a view. The
  same query language the blocks and tables use — an `event_grid` block and
  a bar chart are the same query with different projections. May live as a
  section or inside Data; decided when built (→ Q13).

# The widget story (the Qt question, honestly)

The author's ask: traditional Qt-class widgets, registered with Void Maiz.
What upstream actually provides today: ImGui-composed node widgets, faces,
splitters, the command bar — and a **widget protocol** (registration buys
CLI representation, tag awareness, undo participation, responsiveness
hooks) that explicitly contemplates *wrapped Qt widgets* — but the protocol
is upstream's **Phase 4 design work, not yet built**, and Void Maiz
deliberately ships no general-purpose widgets or application chrome.

Our position (lean, pending the author and upstream): **what Hormiga needs
is the traditional-desktop *feel* — tables, forms, tabs, palettes, wizards —
not Qt the dependency.** Embedding real Qt would fight the family's rules
(vendor-don't-depend; one render loop; the Android path) for little gain
over a proper ImGui-composed widget kit speaking the protocol. But the
protocol itself, the table view, and a form/desktop-chrome kit are genuine
upstream needs — asked in `MESSAGE_FOR_VOIDMAIZ.md` (2026-07-16), with
Hormiga offered as the forcing client, and the Qt-adapter question posed
straight so upstream's author can rule. Until the reply: interim host-side
ImGui tables and forms (already sanctioned upstream), structured so the
widget kit can slide in under them.

# The file layout — folders (2026-08-20)

`src/` was 33 flat files, three of them over 2,700 lines. The author's
complaint, and the reason it is a design problem rather than an aesthetic one:

> *we need to think of modularity not just in the modularity of a singular
> script, but the modularity of engines … i really can't stand that you have to
> sift through thousands of lines of code.*

```
src/
  main/      the two front-ends. desktop.cpp and headless.cpp, side by side
  app/       the shell: lifecycle, frame, dock, seams, persistence, verb routing
  domain/    glyphs, temper passes, importers — Scene in, commands out
  render/    the Output domain: email, site, theme, assets, text, map PNG
    web/     style.css and app.js, REAL FILES embedded at build time
  publish/   deploy, rollback, the deployment history
  ui/        the GUI sections + style, settings, shared widgets
  platform/  storage, vault, .miga, the preview server
```

**No event bus, and the reason is the architecture we already have.** The author
raised one. Hormiga is already hexagonal — Void Core is the domain, the effect
seam and the platform seams (`on_shell_capture`, `on_open`, `on_pick_file`) are
the ports, the GUI and the CLI are adapters, and **the dispatcher is the message
bus**: every change is a logged, replayable command. A second bus would be a
second door, against founding commitment 3. What was wrong was the file layout,
a 354-member god object, and duplicated primitives — not the paradigm.

## Two placements that are load-bearing

**`main/` holds both entry points.** Found by `tools/check_layering.py`, which
objected that `platform/` may not depend on `app/` and was right: a `main()` is
an adapter, not machinery. Putting the two together makes founding commitment 1
**visible in the tree** — there are exactly two doors, they are peers, and
neither is the real one.

**`render/` contains no ImGui, and that is now enforced.** It was true by care
since the Output domain was written, and it is why adopting headless in August
was cheap. A property that valuable should not depend on everyone remembering
it, so `check_layering.py` fails the build on a `#include "imgui.h"` under
`render/` or `domain/`.

## The rule the folders exist to serve

**A shared primitive hidden by file-level `static` is invisible until something
moves.** Splitting the map PNG export out of the map tab revealed that four
mercator functions were shared by proximity rather than by dependency; splitting
the two renderers apart revealed the same of `human_date`, `clip` and
`display_name`. In a flat file each would eventually have become a second copy —
which is exactly what happened to `field_value`, and what put a literal `\n` on
a live public page (see [the log](/log.md), 2026-08-20).

## The tools that keep it

`tools/` is not tidiness: **four of its seven exist because a bug shipped that
they would have caught**, and all four run in `ctest`, because a linter nobody
runs is a comment.

- `lint_glyph_fields.py` — a declared field is a promise, and every domain that
  renders the glyph owes it. Found 15 real gaps on its first run.
- `lint_i18n.py` — visitor-facing English that is not language-selected.
- `check_layering.py` — the boundaries above.
- `find_long.py` — a ratchet; the budgets only ever go down.
- `golden_render.sh` — a refactor that changes a byte of output is not a
  refactor. It caught two real drifts during the restructure.
- `embed_asset.py` — how 816 lines of CSS and JavaScript stopped being C++
  string literals without the binary becoming any less self-contained.
- `split_tu.py` — the mechanical half of a file split, so moving 400 lines
  cannot drop a brace.

## Deferred, deliberately

`HormigaApp` is still one struct with 354 members. Splitting files does not
split the struct, and the author chose the lower-risk depth first. The right
time to revisit is once the folders have made the coupling visible — which is
what they are for.

# The file layout (since 2026-08-17)

The sections are now visible in the file listing, which they were not for the
first month:

    src/app.cpp            the SHELL — lifecycle, host seams, projection,
                           persistence, the console, the frame
    src/section_data.cpp   sidebar → list → detail, the person form, Notes
    src/section_web.cpp    the newsletter preview and the static site
    src/section_allomone.cpp  the rules engine's host side (+ the frozen dialect)
    src/section_builder.cpp   palette | block canvas | inspector | preview
    src/section_map.cpp    Territory: mercator, tiles, position channels
    src/section_calendar.cpp  dated runes on a time grid
    src/app_shared.cpp     the helpers more than one unit needs
    src/app_internal.hpp   the shared include set, structs and declarations

**This is a file boundary, not an architectural one, and the distinction is the
point.** Every section is still a set of `HormigaApp::` methods declared in
`app.hpp`, sharing one struct, one dispatcher, one undo history and one
projection. Inventing a "section interface" to justify the split would have
bought a seam nothing needed and made the one-sync rule *harder* to see — the
same argument [Q30](/developer_questions.md) makes for why the sections are not
separate applications, one level down.

The test for what belongs in `app_internal.hpp`: **more than one unit needs it,
and it does not know about `HormigaApp`.** Anything a single section uses stays
`static` in that section's file, where a reader can tell at a glance that nothing
else depends on it. That rule is enforced by something sturdier than diligence —
see the section-count note in `src/app_internal.hpp` for what happens when it is
not followed.

# Precedents

Old Hormiga's tab surface (contacts/events/images/resources/connections +
builder + server tab) is the checklist the three sections must cover —
through `DESIGN.md`, per the founding rule. `../Neighborhood` is the
behavioral precedent for Territory. MIT Scratch is the usability precedent
for the Builder's chrome.
