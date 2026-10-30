---
type: Concept
title: The store and the working set — a database larger than the device's memory
description: "Decided by the author 2026-10-27: Hormiga must not hold the whole database in memory. The database lives on disk in SQLite (and a .miga v4 file packs that SQLite file); Void Core holds a working set sized to the device, loaded by cluster; edits stay dispatcher commands and are written back from Core's own answers; views read the store. The target is a 2 GB device opening the 5×10^5-rune stress database. What it replaces, how each part works, the order it is built in, and the test each stage must pass."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-10-27T00:00:00Z
---

# The assumption it replaces

Until 0.2.3 a running Hormiga held the whole database as one Void Core state
document, because `vc_create(state_json)` is Core's only way in. On top of that
it held the projected scene, the exported text, an undo frame per edit, and the
copy a `save` keeps for dirty tracking. Each is the size of the database. The
5×10^5-rune stress database (296 MB on disk) peaked at 22 GB, and the window
drew nothing for minutes after the progress bar
([scale](/concepts/platform/scale.md)).

[Data model](/concepts/foundation/data-model.md) planned for 10^2 to 10^3 runes,
where none of that mattered. The author, 2026-10-27: *"we don't have to load the
whole database into memory?? What if the database was like 20gigs in file
size?? ... even this stress test should run on very weak hardware. It's just
that, the better the hardware, the faster load times and larger chunks can be
loaded."*

# The decision (the author, 2026-10-27)

- **Straight to the redesign**: no 0.2.x patch of the whole-document open.
- **A `.miga` v4 file packs the SQLite store**, with the pictures. A v3 file
  (one JSON document) still opens, by streaming it into a store.
- **The smallest promised machine has 2 GB of RAM.** The stress database opens
  there and stays responsive.

# How it works

## The store (on disk, the database itself)

One SQLite file per database, the working copy's `.db`:
- **One row per rune**: its mantle, its order in the mantle, its name, glyph and
  id, and its whole JSON as Core writes it, so nothing about a rune is lost
  that the store does not model.
- **One row per edge**: mantle, order, from, to, relation, and its whole JSON.
- **One row per tag**, indexed. The tag grammar compiles to SQL over it.
- **One row per mantle** for what a mantle has besides runes and edges (its id,
  domain, tags, rules, other layout keys); **one row per top-level key** of the
  state for the rest (config, glyphs, active, scripts, domains, bindings).
- **The pictures**, as blobs, so the store is the whole database (`.miga` v4).

The store can always write the state document back out, whole or in part, and
reading it in and out again gives the same document (the round-trip test). Core's
`_baseline` is not kept: the store is the baseline.

The page cache is bounded, so reading a 20 GB store costs the same memory as
reading a small one.

## Opening a file

- **A v4 `.miga`** is copied to the working copy as it is: one sequential copy,
  constant memory, then the store opens.
- **A v3 `.miga`** (or a bare state document) is read by a streaming reader. It
  holds one rune's text at a time, never the document, and inserts rows in one
  transaction. Memory does not grow with the file.

## The working set (in memory, what Core holds)

Core is given a state document the store writes for it:
- the top-level keys;
- every SMALL mantle whole (documents, the Antfarm, kinds, rules, settings);
- the runes of the large mantles that are LOADED, and the edges whose both ends
  are loaded.

The unit of loading is the **cluster**: the `c1:`/`c2:` tags of the Leiden
hierarchy ([clusters](/concepts/platform/clusters.md)). A rune with no cluster
belongs to its mantle's remainder, which loads in pages. Opening a record loads
its cluster. Loading or dropping clusters rebuilds Core from the store
(`replace_state`), which is why a change of working set is a reload and never an
edit.

**The budget** is a share of the machine's physical memory: an eighth, between
128 MB and 4 GB. Everything the working set costs counts against it: Core's
tree, the undo frames, the projected scene and the exported text. Better
hardware gets a larger budget, so more clusters stay loaded and fewer loads
happen. The undo depth is part of the same budget.

## Edits (the dispatcher is still the only door)

Every change is still a dispatcher command on Core (CLAUDE.md rule 3). After a
command lands, the store is brought into step with what Core now says:
- **The common edits** (`set`, `setjson`, `facet`, `tag`, `measure`, `rune new`,
  `link`, `unlink`) are written back for the runes they name. Core's `cat <name>`
  answers with the rune's JSON, and `links <name>` with its edges.
- **A deletion or a rename** also reaches the edges and runes that are NOT
  loaded. Core would have changed those itself had it held them; the store does
  it in SQL.
- **Everything else** (`undo`, `redo`, `revert`, mantle and glyph verbs, a
  `batch` the classifier does not read) brings the whole working set back into
  step from one export. It is always correct, and bounded by the budget.

## Views (read the store, not Core)

Anything that shows or counts the whole database asks the store:
- the Data tab, by page;
- its counts and search;
- the calendar, by month;
- the map, by viewport;
- publishing, by query.

What is loaded is only what can be edited without a wait.

## Progress

Every phase of an open is counted in units known before it starts (bytes, then
runes). This device's measured cost per unit, remembered from its last opens,
predicts the rest. That is the work-unit model of the database progress
indicators (Luo, Naughton, Ellmann & Watzke 2004; Chaudhuri, Narasayya &
Ramamurthy 2004). Nothing happens after the bar reaches the end.

# The order it is built in, and each stage's test

1. **The store**: schema, the streaming v3 reader, and export of the state
   document. *Test:* round trip equal to the input. Importing 4× the runes must
   not grow peak memory more than a constant.
2. **`.miga` v4**: pack and open. *Test:* round trip, assets included; v3 still
   opens.
3. **The working set and write-back.** *Test:* a model-based test that runs the
   same random commands against a working set and against a Core holding
   everything. After every command the two stores must export the same document,
   including edges to unloaded runes.
4. **The low-memory harness**: the CLI and the app run under an OS memory cap
   (a Windows job object, `ulimit` elsewhere). *Test:* the stress database opens
   and takes edits under a 2 GB cap.
5. **The app's open path and the Data tab** on the store, with the progress
   model.
6. **The calendar, the map, search and publishing** on the store.
7. **LAN sync** of a store-backed database. It merges documents today, and will
   merge the store's rows.

# What is not changed

- Void Core. Everything above uses its existing ABI: `vc_create`, `cat`,
  `links`, export, `replace_state`, the journal.
- The privacy seam. Publishing still passes internal fields through the render
  seam, now from store queries.
- Small databases. They fit the budget whole, and their working set IS the
  database.

# Related

[scale](/concepts/platform/scale.md) ·
[clusters](/concepts/platform/clusters.md) ·
[miga format](/concepts/platform/miga-format.md) ·
[data model](/concepts/foundation/data-model.md)
