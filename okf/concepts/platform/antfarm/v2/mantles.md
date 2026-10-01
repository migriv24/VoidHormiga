---
type: Concept
title: Antfarm v2 — chambers, selection, and tunnels
description: "A .miga is a set of chambers, each a mantle: Data, Assets, Network, Documents, plus the Antfarm and the rules. The Miga node emits all of them as one mantle, and operator nodes narrow it the way Geometry Nodes narrow geometry: separate chambers, filter by a query (tag, glyph, kind, field, date, place, link, change), join, count. Mantle size and growth, read from the state and the log. Tunnels: the pure mappings between chambers (Data→Assets above all), declared by glyphs, checked on a face."
tags: [status:direction, audience:all, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Built 2026-09-28** (`domain/chambers.hpp`), with two decisions this page did
not make, recorded in the [v2 index](/concepts/platform/antfarm/v2/index.md)
§"The chambers, as built": **references in Data stay paths**, because every path
already carries its file's sha256, and **the Documents chamber holds a
document's identity and deployment**, while its content stays where its tab
edits it. The query and operator nodes below are built; `Kind`, `Near`,
`Linked to` and `Changed` query nodes are not yet.

# 1. The `.miga` is a set of chambers

The author, 2026-09-28:

> a mantle is sort of like a file system itself, but in graph form. so the miga
> database is a collection of mantles.

Void Core says the same thing in its own agent guide: *mantle ≈ directory, rune
≈ file, tag expression ≈ glob.* So this is not a metaphor laid over the engine.
It is the engine's own shape, made visible.

| chamber | what it holds | today (v1) | v2 change |
|---|---|---|---|
| **Data** | the organization's runes: contacts, events, orgs, notes, images' meaning | the data mantle (`demo-org` in a fresh database) | none. It is renamed on screen only |
| **Assets** | one `asset` rune per file: its sha256, size, media type, original name, public link if hosted | **does not exist**: files are `path`/`url` fields on `image` runes in the data mantle, plus `assets/` on disk | **new**. See §4 |
| **Network** | one `profile` rune per profile that has joined, and the devices they run on | `members.json` (its own small document) plus `peer` runes | **new as a mantle**. `hol_membership.store` already names `mantle` as an option, reserved for this since 2026-09-16 |
| **Documents** | one `document` rune per newsletter, website, calendar or map, each entering its own content mantle | one mantle per Builder document; `calview` runes; map view runes | **new as a catalog**. See [documents](/concepts/platform/antfarm/v2/documents.md) |
| Antfarm | the v2 graph itself | the `antfarm` mantle | a new mantle, `farm` ([migration](/concepts/platform/antfarm/v2/migration.md)) |
| Rules | Allomone sources | the `allomone` mantle | none |

The Antfarm and the rules are chambers in the storage sense. They do not
appear as outputs of the Miga node, because a document never needs to read the
Antfarm. That avoids a graph that contains itself.

## Why Assets is its own chamber

The first conversation argued for keeping files *content-addressed* and
against a "path" type. The author wanted assets to be a mantle. **Both hold,
because they answer different questions:**

- **The Assets chamber knows *what*.** An `asset` rune is the file's identity:
  its hash, size and type. It is data, so it syncs, it can be tagged, and a
  mantle of assets has a size.
- **Rivers know *where*.** Which folder, bucket or peer holds the bytes is a
  river's business ([rivers](/concepts/platform/antfarm/v2/rivers.md)), and it
  can change without touching the rune.

The content address is what joins the two. Because the rune names the bytes by
their hash, any river holding the same hash holds the same file. That keeps
"every cloud host is disposable" true.

**What moves out of Data:** an `image` rune keeps what it *means* (alt text in
two languages, a description, tags, the events it belongs to) and gains an
`asset` field. The bytes' identity moves to the Assets chamber. A contact's
`avatar` points at an asset. A flier and an avatar can share one file.

# 2. Selecting: the index feature

The author:

> If we just are outputting the database mantle, then thats like EVERYTHING.
> However, we can "index" or filter mantles.

In Geometry Nodes, the `Group Input` hands you all of the geometry, `Separate
Components` splits it into mesh, curves and points, and a *selection field*
(a diamond socket) says which elements a node acts on. v2 copies that
structure.

## The nodes

| node | in | out | what it does |
|---|---|---|---|
| **Miga** | `rests in` (River), `backups` (River, many) | `all` (Mantle) | "this database". Its face lists every chamber with runes, bytes and growth |
| **Separate chambers** | `mantle` | `data`, `assets`, `network`, `documents` | Geometry Nodes' `Separate Components` |
| **Filter** | `mantle`, `where` (Query) | `kept`, `rest` | keeps the runes the query selects, and hands the others on (`Separate Geometry`) |
| **Join** | `mantles` (many) | `mantle` | union (`Join Geometry`) |
| **Count** | `mantle` | `runes` (Value) | how many |
| **Measure** | `mantle` | `bytes`, `runes`, `growth` (Value) | size, and change over a window |

## The query nodes (◆)

A query is a rule, not a set. It becomes a set only when a filter applies it
to a mantle, which is why it travels on a diamond.

| node | selects | example | compiles to |
|---|---|---|---|
| **Tag** | a tag expression, the full Void Core grammar, colon tags included | `volunteer AND NOT private` | a Void Core tag expression |
| **Glyph** | runes of one or more glyphs | `contact`, `event` | `glyph` |
| **Kind** | entity, act or measure | `act` | `ls --kind` |
| **Field** | a field compared to a value, or set / not set | `avatar is set`, `role = volunteer` | a Scry projection |
| **Date window** | dated runes in a window | next 30 days, this month | Hormiga's date query |
| **Near** | runes with a place inside an area | within the map document's `district-3` view | a Territory predicate |
| **Linked to** | runes related to a given rune | everyone linked to `spring-fair` | Void Core links |
| **Changed** | runes changed since a time, or by a profile | changed by `ana` this week | the dispatcher log, through the Network chamber |
| **And**, **Or**, **Not** | combinations | | |

**The author asked whether selection could be "by a mantle name like the
contacts mantle, or by something else".** Today contacts are a *glyph* inside
the Data mantle, not a mantle of their own. So "the contacts" is `Glyph:
contact`, and the author's instinct works unchanged. Mantle names still
select at the chamber level: a document's content mantle is picked by name
from the Documents chamber.

**Every query shows what it compiled to.** The face of a `Tag` node reads
`volunteer AND NOT private`, and `farm eval` prints it. An agent can read the
filter without reading the graph.

**Queries are not logic.** They are declarative selection, like a glob. That
keeps the [three DSLs](/concepts/foundation/dsls.md) apart: no variables, no
loops. When a selection genuinely needs logic ("contacts whose last event was
more than a year ago"), the answer is an **Allomone** query node naming a
derive-only predicate, planned with Allomone's I/O phase, not a growing query
language here.

# 3. Size and liveliness

The author:

> mantles will also have file sizes, based on how many runes they have and the
> size of each rune.

and, from the first conversation, a mantle node's face could show *"a mini
preview graph"*, because in a large group *"a mantle node might be constantly
changing its size"*.

Every Mantle output can answer, and every face that carries one shows:

- **runes**: the count;
- **bytes**: the serialized size of those runes in the state document, plus,
  for the Assets chamber, the sum of the files' sizes;
- **growth**: a sparkline of runes added and removed per day. **This costs no
  new storage.** Every `rune new` and every delete is already a logged
  dispatcher command, so the history is derived from the log;
- **activity**: which profiles changed it recently, from the same log's
  attribution, shown as their presence colours.

These are cached projections, recomputed on dispatch and only for faces on
screen. A 10,000-rune mantle is not re-measured every frame. `farm eval
<node>.<port>` prints the same numbers.

# 4. Tunnels: how chambers speak to each other

The author:

> a big important HOLIDAY is the mapping between the database mantle and the
> assets mantle. because a lot of contacts will have a profile picture, and the
> picture should point to an asset in the assets mantle.

These mappings are **tunnels**, not holidays, because both ends are inside the
`.miga` and nothing is reached ([index](/concepts/platform/antfarm/v2/index.md)
§"The four words, revised"). A tunnel is pure. Its whole job is to state a
reference and check it.

**Tunnels are declared by glyphs, not drawn by hand.** A glyph already declares
`"editors":{"avatar":"image"}`. In v2 that declaration *is* the tunnel: the
field holds `asset:<sha256>`, and the Data→Assets tunnel is the set of every
such field. The seed places one tunnel node per chamber pair so its face can
be seen. Placing another is only needed for a custom glyph.

| tunnel | the reference | its face shows | its actions |
|---|---|---|---|
| **Data → Assets** | image-typed fields (`avatar`, `path`, a flier's picture) hold an asset's hash | referenced · **missing** (the field names a hash with no asset rune) · **unheld** (an asset rune whose bytes no reachable river has) · **orphans** (assets nothing references) | `check`; adopt orphans into an image rune; fetch unheld |
| **Documents → Data** | a block that shows a rune by name or query | references · **broken** (the rune was deleted) | `check`; open the block |
| **Network → Data** | a profile linked to the contact who *is* that person; every command's author | linked · unlinked profiles | link a profile to a contact ([identity](/concepts/platform/identity.md): never `contacts.is_admin`) |
| **Network → Assets** | who added a file | | |

**The round-trip law is where Void Reyna's mathematics pays off first.** Data→Assets
is a lens with a law that can be tested: resolving a field to an asset and
writing the asset's address back gives the same field. The same test catches
a v1-to-v2 migration that dropped a picture.

# 5. What a chamber write looks like

Nothing flows *into* a chamber by being wired
([types](/concepts/platform/antfarm/v2/types.md) §3). A source wired into the
Data chamber's `import` input is a statement of where imports come from. The
write happens when someone runs it:

```
farm run import-csv members-sheet      # rehearses: 212 new contacts, 3 updated, 0 removed
farm run import-csv members-sheet apply
```

and it lands as **one batch** of `rune new` and `set` commands, one undo frame,
attributed to the profile that ran it. That is what CSV import does today.
