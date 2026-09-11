---
type: Index
title: Sections — the application's rooms
description: "The four main sections over one core, and the documents they author: workspace shape, Builder, blocks and domains, Calendar, Territory."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-08-27T00:00:00Z
---

**One core, several rooms.** A section is a view over the same state document,
and the rule that keeps them honest is that sections never call each other —
they talk only through the dispatcher and the mantles
([workspace & sections](/concepts/sections/workspace-and-sections.md)). That
discipline is what makes the possible split into several applications cheap
rather than a rewrite ([application boundaries](/concepts/foundation/application-boundaries.md)).

- [Workspace & sections](/concepts/sections/workspace-and-sections.md) — the
  application's shape: Data / Builder / Antfarm / Territory as four sections
  over one core; the panels and windows overhaul; the widget-toolkit story.
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
  3-day / week / month views styled by the map's rules engine; static and web
  exports; grounded in RFC 5545 (model) and FullCalendar (view vocabulary).
- [Calendar roadmap](/concepts/sections/calendar-roadmap.md) — the do-list: the
  **X-track** (the exchange — the iCalendar lens, conformance, time, import,
  subscriptions, two-way) added by the 2026-09-10 reframe of the calendar as a
  *hub* rather than a destination, plus the continuing C-track (the room).
- [Territory](/concepts/sections/territory.md) — the map: location-faceted
  runes on a canvas over a swappable map source (not assumed to be Earth); the
  Neighborhood analysis and its mapping to runes / holidays / tags / Scry;
  reactive visuals as interaction-net-flavoured projections.

# The one section documented elsewhere

**The Antfarm** is a section too, but its page lives in
[platform](/concepts/platform/antfarm.md) rather than here, because it is a
section *of* the machine underneath: it is where backends are configured, and
it is the protocol layer the `.miga` bundle stores. The Antfarm's
user-interface story is here in
[workspace & sections](/concepts/sections/workspace-and-sections.md); what it
*is* is there.
