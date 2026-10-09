---
type: Concept
title: Kinds are yours — the next major version
description: "The author's 2026-10-06 direction: an organization names and organizes its data however it wants; contact, organization, event could be product, vendor, invoice, expiration date. So the kinds of thing become DATA: `kind` runes in a `kinds` mantle that travels with the database, each with a name, plural, icon, colour, fields and TRAITS (located, dated, listed). Code stops asking 'is this a contact?' and asks 'can this kind be on a map, on a calendar, in a directory?'. The audit of where the built-in names live, the design, the order of work, and the demos that prove it: the Cat Colony, a cat D&D campaign, a cat grocery mart, a house of cats."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-06T00:00:00Z
---

# What the author asked

> "what we want is to be able to organize and name our data whatever we want.
> so contact, organization, event, etc. could be renamed to: product, vendor,
> invoice, expiration date. i think we should support this. This fundamentally
> will require some digging into the actual naming conventions that hormiga
> currently has, and it likely stretches EVERYWHERE in the code. So this will
> begin the NEXT major version of hormiga. while to the user, this next major
> version won't seem like such a big change, to us internally, it certainly
> will."

> "again, our goal is NOT these demo applications. these demo applications
> will be a result of our new transformation beyond the current systems we
> have in place"

# The audit (2026-10-06)

The built-in kind names, as string literals in `src/`:

| name | references | files |
|---|---|---|
| `image` | 89 | 25 |
| `event` | 55 | 24 |
| `contact` | 48 | 22 |
| `note` | 47 | 17 |
| `organization` | 39 | 22 |
| `incident` | 26 | 14 |
| `product` (2026-10-06) | 12 | 10 |
| `job` | 11 | 9 |
| `resource` | 10 | 8 |
| `type:contact` and the like in queries | 37 | |

Read one by one, every site asks one of four questions, and only one of them
is really about a name:

| the question the code asks | examples | what answers it from now on |
|---|---|---|
| **how does it look?** | label, icon, colour, palette group, the phone's card colour, a marker's default colour | the kind's own definition |
| **what can it do?** | placeable on a map (`map_placeable`), on the calendar (`event`'s date, `job`'s deadline), in a website or newsletter directory (`contact`, `organization`), its card's subtitle | the kind's **traits** and named fields |
| **which kind does this block show?** | event grid, job grid, directory | a kind chosen on the block, among the kinds with the trait it needs |
| **this importer makes a specific kind** | the predecessor's rescue import, iCal, quick add | stays: an importer of contacts makes contacts, which a database may call whatever it likes |

`image`, `note`, `map`, `canvas`, `mapshape`, the Antfarm's glyphs and the
document blocks are the application's own machinery, not an organization's
kinds of thing; they keep their names.

# The design

**A kind is a rune** of glyph `kind`, in its own mantle, **`kinds`**, which
travels with the database like every mantle (sync, `.miga`, backups). It says:

| field | meaning |
|---|---|
| `title`, `plural` | what a person calls one, and many ("Customer", "Customers") |
| `icon`, `color` | how it looks everywhere it is drawn |
| `category` | where it sits in the add palette |
| `fields` | its fields, as JSON: `[{"key":"price","label":"Price","editor":"text"}]` |
| `date_field` | the field that puts it on the calendar (with trait `dated`) |
| `subtitle_field` | what its card shows under its name |
| tags `trait:located`, `trait:dated`, `trait:listed` | what it can do |

**Two uses, one mechanism.**

- **Rename a built-in kind**: a kind rune whose name is a built-in glyph
  (`contact`) overrides how it is called and drawn ("Customer"), and may add
  fields. The stored data does not change: a contact is still glyph `contact`
  underneath, so everything already written about contacts still works.
- **Make a new kind**: a kind rune with a new name (`vendor`, `character`,
  `device`) is registered as a glyph at load, from its fields, exactly as the
  built-ins are registered from C++. It is a first-class kind: searchable,
  taggable, synced, placeable if `located`, on the calendar if `dated`.

**Renaming a FIELD renames its label, not its key.** A key is what the data
stores; changing it means rewriting every rune of the kind, which is a
migration, not a rename. Labels are what a person reads, and they change freely.

**A built-in kind's fields are the database's to name too** (Q109, answered
2026-10-06 with the lean: labels first, extra fields, never keys). A `kind`
rune for a built-in may carry `fields`: an entry for one of the built-in's keys
relabels it, any other key is a field of the database's own. The application
remembers each built-in's descriptor as it registered it and registers it again
with those labels and fields over it; one it no longer says anything about goes
back to the application's own. Only what differs is stored. An application
field is never removed.

**The version** (Q108, answered 2026-10-06 with the lean): this line is
**0.2.0**, the first minor of the 0.x series and the major break inside it;
1.0 stays reserved for the predecessor's retirement (roadmap phase D). The
`.miga` format does not change: a `kinds` mantle is a mantle.

**Traits.**

| trait | means | used by |
|---|---|---|
| `located` | it carries a position (`geo`, and a canvas's channel) | the map, every canvas |
| `dated` | `date_field` puts it on the calendar | the calendar, date queries |
| `listed` | it may appear in a directory on a website or newsletter | the directory blocks |

More traits come with the features that need them (a `priced` kind for a point
of sale, a `party` kind for who did something). A trait is added when a second
place in the code would otherwise ask for a name.

**One registry, read everywhere.** `domain/kinds.hpp` holds the built-in kinds
(the current ones, with their traits) and merges the database's `kinds` mantle
over them at every projection. Every front-end, renderer and the CLI asks it.

# Two measured constraints, and what they decided

1. **Void Core refuses a rune of a glyph it has not been told about.** So a
   kind is registered before a rune of it is made: the Kinds window commits the
   kind, the next projection registers it, and only then can anything be made of
   it. A demo is two phases (kinds, registration, data). The CLI registers a
   database's kinds when it loads the document; a single script that makes a
   kind and then a rune of it needs two runs, because `run_cli` has no
   per-command hook.
2. **A glyph the document DECLARES (`glyph declare`) shadows the same glyph the
   app REGISTERS.** Declaring kinds into the document would have made them
   travel by themselves, but the position channels the app adds for canvases
   would then be dropped. So kinds are **registered from their `kind` runes**,
   as layer channels always were, and it is the runes that travel (they are in
   the `kinds` mantle). `spine_smoke` pins the shadowing, so a change in Void
   Core shows up as a failing check and this choice can be revisited.

# A database's own kinds at the public seams (CLAUDE.md rule 6)

Measured 2026-10-07, by reading every output that walks the whole database.
Several published by SUBTRACTION (everything except contacts, notes, images):
safe while the only kinds were the application's, and a leak the day a
database made a kind of its own. A home's residents and devices, a store's
customers and sales, would have reached a website's map and calendar. So:

**A database's own kind leaves only with consent, and only its name, date and
one safe line.** Every public output asks `domain/kinds.hpp`:

| output | the rule |
|---|---|
| directory block (website, newsletter, Builder preview) | `in_directory`: kinds with the trait `listed`; then `clearance:public`, and `clearance:contact` for email and phone, as always |
| event grid (website, newsletter) | `in_event_grid`: a chosen dated kind, only with `clearance:public` |
| the website's map widget | `on_public_map`: the built-ins as before; an own kind only with `clearance:public`; a layer on a drawn canvas puts nothing on an Earth map, and only Earth's regions are drawn |
| the website's calendar and its `.ics` | `withheld`: an own kind only with `clearance:public`, by its own date, with no times, no place, no description and no position |

**The line under a name** (`directory_line`) is a kind's subtitle field, and
never `notes`, `email` or `phone` whatever a kind's maker chose. **The name**
(`published::listed_name`) is an own kind's `title`, never its handle.

**Every core that projects a database applies its kinds, LAST** (after every
application glyph, so the clash guard sees them all): the app's `reproject`,
its `on_register_glyphs` (the share plan, translation and sync probes), the
CLI's session, its effects' cores and `render_from_state`, and the Antfarm v2
throwaway. Found by rendering Whisker Mart from the CLI: the vendor was listed
and its fields were empty, because the render's core had never been told its
kind.

**A kind may never take an application glyph's name.** A kind called
"Directory" or "Note" would have registered over the document block or the
notes. The Kinds window refuses such a name; `apply` refuses to register one
that arrives any other way, drops it from the registry and reports it
(`Registry::clashes`), and the window shows it.

**Letting a kind go.** A database's own kind is deleted only when nothing is of
that kind; a renamed built-in can go back to the application's own (nothing
stored changes).

# The order of work

1. **The registry and the `kinds` mantle** (built 2026-10-06), the glyphs
   registered from data, and the questions of *looks* answered by it: the Data
   list (plural names and icons), the add palette, glyph icons, map markers,
   the phone's cards (colour, icon, subtitle), the calendar's entries.
2. **Traits** replace names where the code asks *what can it do*: what can be
   placed (the phone's map, *Add here*, the desktop's right-click: built), the
   calendar for a database's own dated kinds (built; the built-ins keep their
   own rules unchanged), **the directory blocks (built)**: the website, the
   newsletter and the Builder's preview ask one rule (`kinds::in_directory`)
   which kinds a directory lists (every `listed` kind; the block's kind picker
   offers them by name), and one rule (`kinds::directory_line`) for the line
   under a name, which never releases `notes`, `email` or `phone` whatever a
   kind's subtitle says. The two consent gates are unchanged; for a database of
   contacts and organizations the output is byte for byte what it was (the
   golden render).
3. **A Kinds window** where a person renames a kind, changes its icon and
   colour, adds and relabels fields and toggles traits (built 2026-10-06 on the
   desktop; the phone reads it). A built-in's fields are listed too: relabel
   them, add to them, never remove or rekey them.
4. **Blocks choose a kind** instead of naming one: the directory (built); the
   event grid choosing among `dated` kinds and a block's fields rendered by
   their labels (next).
5. **Field keys renamed** by migration, if anyone ever needs it.

# The demos that prove it

Not the goal: the proof. Each is a workspace
([workspaces](/concepts/platform/workspaces.md)) whose kinds are **defined in
its own `kinds` mantle, as data**, not in C++:

| workspace | demo | kinds it defines |
|---|---|---|
| Organization | **the Cat Colony** (unchanged) | none: the built-ins |
| D&D campaign | **the Whiskerwood campaign**: a party of cat adventurers of 5e races on an authored map in feet | character, creature, location, quest, session, item |
| Grocery store | **Whisker Mart**: a small cat grocery, its floor, products, vendors (Big Fish, M.E.O.W. Distribution), customers and sales: the beginning of a point of sale | product, vendor, customer, sale |
| Smart home | **the House of Cats**: a home's floor plan, its smart devices and its inventory, with expiration dates on the calendar | device, item, task |

# Boundaries

- **One data model.** A kind made in a database is a glyph like any other: the
  same dispatcher, sync, search, privacy seams and exports.
- **Stored data never changes when a kind is renamed.** Only what people read.
- **The application's machinery keeps its names** (images, notes, maps,
  canvases, documents, the Antfarm).
- **No feature exists for one name.** Where code needs a kind to do something,
  it asks for a trait.
