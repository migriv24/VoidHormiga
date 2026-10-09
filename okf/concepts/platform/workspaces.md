---
type: Concept
title: Workspaces — one application, set up for different kinds of work
description: "The author's 2026-10-06 direction: Hormiga has been tuned for outreach organizations, and should also be set up for grocery stores, smart homes, volunteer organizations, D&D campaigns and more. A workspace is a starting point chosen when a database is made: its kinds of things, its canvases, its categories, its default screens and words. Not a mode and not a fork: every workspace is the same data model, the same dispatcher, the same sync. Which are built, what each needs, and the line no workspace may cross."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-06T00:00:00Z
---

# What the author asked

> "In general I would like to consider different 'workspaces' or set ups for void
> hormiga. Right now it's mostly been optimized for normal organizations.
> However we should also build for things like: grocery stores, smart homes
> (just keeping a layout of the home), volunteer organizations, DND campaigns,
> etc. We have a very versatile tool, and we should build accordingly."

# What a workspace is

**A starting point, chosen when a database is made.** Concretely, a workspace
is:

- a **seed transcript**: dispatcher commands, like the Cat Colony's, that build
  the starting canvases, regions, categories and example things. Replayable and
  inspectable, never a hidden template file;
- the **kinds of things** it puts first (a grocery store's products, a home's
  devices and rooms, a campaign's characters and places);
- its **canvases** ([canvases](/concepts/sections/gis/canvases.md)): Earth for an
  outreach territory, a plan for a store or a home, an authored world for a
  campaign;
- its **categories**: the tag namespaces its regions give (`aisle:`, `room:`,
  `zone:`, `region:`);
- its **default screens**: which sections open first, and on a phone which four
  are on the bar;
- the database records which it started as (`config workspace.kind`), so the
  application can phrase things for it.

**What a workspace is not.** Not a mode that hides the rest of the application
(a grocery store can still send a newsletter), not a fork of the data model (a
product is a rune, synced, searched and exported like any other), and never a
reason for a feature to exist in one place only. The privacy seams, the
dispatcher, sync and the Antfarm are the same in every workspace.

# The workspaces

| workspace | canvases | kinds put first | categories | status |
|---|---|---|---|---|
| **Organization** (outreach) | Earth (territory) | contacts, organizations, events, incidents | tags as today | built: what *New database* has always made |
| **Grocery store** | the sales floor, in metres | products, vendors, customers, sales, purchase orders | `aisle:`, `dept:`, `vendor:`, `stock:` | **built**: **Whisker Mart**, a cat grocery; the first shape of a point of sale |
| **Smart home** | the ground floor, in metres | devices, residents, supplies (with expiration dates), chores | `room:`, `protocol:` | **built**: **the House of Cats** |
| **Volunteer organization** | Earth, and a plan of the venue | volunteers (contacts), shifts (events), stations | `station:`, `shift:` | designed: the outreach workspace plus a venue plan; nearest to what exists |
| **D&D campaign** | the Whiskerwood, in feet | characters (5e race, class, abilities), creatures, places, quests, sessions, items | `region:` | **built**: **the Whiskerwood**, a party of cats of 5e races. Private notes per player wait for roles |

**Since the kinds release (2026-10-06) every workspace's kinds are data**: each
demo defines its own kinds as `kind` runes in its `kinds` mantle
([kinds](/concepts/foundation/kinds.md)); nothing about a store, a home or a
campaign is in the application's code. The Cat Colony, the organization's demo,
is unchanged.

**Choosing one.** *New database* asks which workspace, with the organization
first (what it made before), each with one line on what it sets up. A workspace
not yet built is listed and marked so, not hidden: the author wants the breadth
seen.

# Boundaries

- **One data model.** A workspace adds runes, canvases and tags through the
  dispatcher; it never adds a storage path or an export path of its own
  (CLAUDE.md rules 3 and 6).
- **A workspace is where you start, not where you stay.** Anything a workspace
  sets up can be changed or added to afterwards, in any database.
- **Demo data is synthetic and public**, as the Cat Colony is: no real store,
  no real home, no real people.
