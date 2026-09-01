---
type: Concept
title: Calendar — dated runes on a time grid
description: "The Calendar workflow: everything carrying a date, on 3-day/week/month grids; styled by the SAME views+rules engine as the map; static PNG + interactive web exports; grounded in RFC 5545 (model) and FullCalendar's feature vocabulary (views); events and incidents as visibly distinct kinds."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-07-22T00:00:00Z
---

The **Calendar** is Void Hormiga's fifth workflow: **anything carrying a
`date` appears on a time grid**. It is not a separate data store — like the
map, it is a *projection surface* over the same runes; a date is to the
calendar what a `geo` is to Territory. The author's framing (2026-07-22):
"structured similarly to how the views are… generate an image and export it
(like for a newsletter) or an interactable version for a website."

# Grounding (the author's ask: "ground it in some existing open source project")

Two references, deliberately split by concern:

- **The model grounds in iCalendar — RFC 5545** (the open standard behind
  .ics files, Google/Nextcloud/every calendar). Our event glyph already maps
  onto its core: `date` ≈ DTSTART (date), `start_time`/`end_time` ≈ the timed
  DTSTART/DTEND pair, `days` ≈ a poor man's RRULE (weekly recurrence),
  all-day = time fields empty. Grounding here buys a future for free:
  **`.ics` export/import is the calendar's cloud-interop holiday** (subscribe
  in Google Calendar; import a partner org's feed) — the same
  local-first/opt-in shape as ImgBB. Full RRULE recurrence is explicitly
  future; v1 renders concrete dates only.
- **The views ground in FullCalendar** (MIT, the de-facto open-source
  calendar UI): its vocabulary — dayGrid **month**, timeGrid **week**/**day**,
  **list** — names our granularities: **3-day / week / month** now, agenda
  list later. FullCalendar is also the candidate engine for the **dynamic web
  export** (vendored, per vendor-don't-depend — though the site pipeline's
  hand-rolled JS may cover a month grid without it; decide when the dynamic
  export is built, not before).
- **The year view** (author: "idk how we can do that") grounds in the
  GitHub-contributions heat map: a year is 366 cells colored by **density of
  dated runes**, clickable into month view. Cheap to render, genuinely
  informative for an outreach org's rhythm. Directed future, not v1.

# The model

- **A dated rune is a calendar entry.** `date` (YYYY-MM-DD) is the hinge;
  `start_time`/`end_time` order a day's entries. Events, incidents — and any
  future dated kind — appear with no registration step (projection, not
  subscription).
- **Events and incidents are visibly distinct kinds** (author directive,
  standing): planned events vs. dated occurrences (road closures, ICE
  activity) never blur. Incidents carry a warning mark and a red accent
  regardless of rules; rules refine, kinds distinguish.
- **The styling engine is the map's** (the cross-view directive made real):
  a calendar entry's icon/color resolve exactly like a marker's — first
  matching RULE of the selected view styles it, explicit `icon:`/`color:`
  tags win, glyph defaults ground it. One `parse_view_rules_of`, two
  surfaces. Shared icons/colors between map and calendar are therefore not a
  feature — they are the *absence* of a second styling system.
- **Temporal linkage** — the derived time-proximity relationship (Settings
  toggle, physics view) reads naturally here later: entries within the
  window can badge or cluster ("this incident brackets your event"). Derived,
  never stored; v1 notes it, doesn't render it.
- **Linking ("not independent")** — relations between dated runes (an event
  `responds-to` an incident) are ordinary edges; the calendar can badge
  linked entries and jump along links. Directed future.

# Views and navigation

An anchor date + a granularity: **Month** (7×up-to-6 grid, entries as chips,
other-month days dimmed), **Week** (7 day-columns, fuller entries with
times), **3-day** (the same, denser — the "what's imminent" view). Prev /
Today / Next move by the granularity's stride. Selection is the app's one
selection: clicking an entry selects the rune (inspector shows it; the same
entity selected on the map is selected here — one `ed`).

# Exports (the author's pair, same as the map)

- **Static PNG** — the newsletter's calendar: the month grid composed
  CPU-side (same compositor discipline as the map export: white grid, day
  numbers, rule-colored entry chips, atlas-blitted text — WYSIWYG with the
  live view) into `exports/calendar-<stamp>.png`. `exports/` stays
  gitignored.
- **Dynamic web** — an interactive calendar page in the site pipeline
  (FullCalendar-vendored or hand-rolled JS; decided when built). Rides the
  same deploy as the website blocks.
- **Privacy at the export seam (sharper here than anywhere):** incidents can
  be *sensitive* (ICE activity is the canonical example). A public calendar
  export must be **filterable** — the view's tag filter decides what leaves
  the machine, and internal-notes-class fields never render into any export.
  v1 exports what the grid shows; the filter field is the immediate next
  step and blocks any *published* calendar until it lands.

# The toolset (the feature inventory, 2026-07-22)

The author's direction: list EVERYTHING calendar tools should do, then build
one by one (or flag what wants Void Maiz). The inventory, grounded in the
FullCalendar / Google-Calendar-class feature space, phased like Territory's
T-track. **Upstream column: everything here is host-buildable on ImGui
primitives (the map canvas already proved the input model); the only
*candidate* asks are marked ◇.**

**C1 — creation & direct manipulation (the calendar becomes a TOOL)**
- C1a. Right-click an ENTRY → context menu (open, icon/color, unschedule,
  delete — the map's marker-menu parity). *Building now.*
- C1b. Right-click a DAY → "New event here" / "New incident here" (date
  pre-filled; month cells create all-day). *Building now.*
- C1c. **Time-grid week/3-day** (the author's meaning for these views): hour
  rows, timed entries as positioned blocks, a now-line — and **drag on empty
  time → create an event with real start/end times** (snapped to :30).
  *Building now.*
- C1d. **BUILT (2026-07-23):** reschedule by drag — (a) month: ImGui
  drag-drop an entry onto another day → `set date`; (b) time grid: drag a
  block to a new day+time, duration preserved → `set date`+`start`+`end`
  (one batch). A grab with no movement selects; incidents (a point in time)
  move but don't resize.
- C1e. **BUILT (2026-07-23):** drag a block's bottom edge (a grip appears on
  hover) = resize `end_time` (min 30 min), same manual-drag discipline as
  create/move.
- C1f. Quick-add: type-in-cell (name, Enter → dated rune).

**C2 — views & navigation (beyond the three grids)**
- C2a. **BUILT (2026-07-23):** Agenda/list view — chronological, today
  forward (180 days), grouped by date, honoring the filter; the newsletter's
  list twin.
- C2b. Year = density heat map (GitHub-contributions style), click into month.
- C2c. Jump-to-date; keyboard strides (arrows/PgUp/PgDn).

**C3 — the model catches up**
- C3a. Recurrence: `days` (weekly RRULE-lite) renders as concrete occurrences.
- C3b. Multi-day events (a `days`-long span renders across cells).
- C3c. All-day vs timed: empty time = all-day band (the grid does this now).

**C4 — meaning & cross-view**
- C4a. **BUILT (2026-07-23): tag FILTER + kind filter** — a toolbar row: a
  KIND combo (All / Events only / Incidents only) and a tag-query box fed by
  the reusable `tag_picker` (no hand-typed AND/OR — the author's UX ask). The
  filter drives `cal_entries_on`, so every view (month/week/3-day/agenda)
  and the PNG export honor it. *Privacy-blocking for anything published*
  (incidents can be ICE activity — "Events only" hides them by choice). The
  *published* export still needs this filter baked into a saved calendar
  view (C4d) before a public calendar ships.
- C4b. Temporal-linkage badges (the derived proximity, surfaced here).
- C4c. Relation links: an event `responds-to` an incident → badge + jump.
- C4d. **BUILT (2026-07-23): calendar VIEWS as `calview` runes** — a saved,
  named calendar carrying its filter (kind + tag query) + default
  granularity (`mode`) + a `rules` field (reserved; styling still shares the
  map's rules for now). The toolbar has an "(ad-hoc) / saved views" picker,
  "Save as view", "Update", "Delete". Selecting a view loads its settings;
  the in-app filter it drives flows into the **PNG export**, so exporting
  while "Public Events" is active bakes the privacy choice into the file —
  the publishable path the C4a filter needed. `calview` lives in the data
  mantle (where the calendar reads), like map views. *Still ⬜:* per-view
  rules editor, and the web `calendar_embed` referencing a calview by name
  (as `map_embed` references a map view) for the dynamic export.

**C5 — exports & interop**
- C5a. Static PNG: month shipped; week/3-day exports.
- C5b. **`.ics` export — BUILT (2026-07-22)**: the `calendar_embed` block
  writes `site/calendar.ics` (RFC 5545 VEVENTs, timed and all-day) beside
  the page; per-entry **"add to Google Calendar"** template links in the
  widget. Import works everywhere today; live *subscription* needs the site
  deployed at a stable URL (it rides the deploy holiday when that ships).
- C5c. `.ics` import (partner org feeds) — an Antfarm holiday, opt-in.
- C5d. The dynamic web export — **BUILT (2026-07-22), hand-rolled** (the
  vendor-question resolved: ~130 lines of vanilla JS beat vendoring
  FullCalendar for three read-only views): the `calendar_embed` block
  renders an interactive month / week / 3-day widget in the site, read-only
  by construction. Blocks-and-domains.md carries the embed-block contract.

**◇ Upstream candidates (only if/when)**
- ◇ A reusable calendar widget in the widget protocol — only if a second
  family app wants calendars; until then this is host UI by design.
- ◇ Cross-window drag-and-drop (drag a contact FROM Data ONTO a day) — needs
  library-level drag conventions; worth an ask only when C1d feels cramped
  inside one window.

# Upstream (Void Maiz)

**No ask needed for v1.** ImGui tables/children render the grids; the rules
engine, selection, and inspector are already shared. A *possible* future ask
— only if other family apps want calendars — is a calendar widget in the
widget protocol; until then the grid is host UI, and Allmusely remains the
eventual general answer to chrome.

# Boundaries

- **Not a scheduler.** No invitations, no attendees, no free/busy — an
  outreach org's calendar shows and exports; it does not negotiate meetings.
- **Not a second query language.** "What's on the calendar" is the tag
  grammar + a date; the same expression the blocks and tables use.
- **Recurrence is modeled, not simulated.** `days` renders as its concrete
  occurrences when recurrence lands; no phantom entries before then.
