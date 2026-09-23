---
type: Concept
title: Holidays, and how the Antfarm uses them
description: "Void Core's word for reaching a system the application does not own, and the four words the Antfarm must keep apart (holiday, node, payload, capability). The effect gate every crossing passes, the effects that cross through Antfarm nodes today, the arrow shapes holidays come in, wrappers as holiday-to-holiday, the 'effect boundary plus a Lens' account with an honest note on how little of it Hormiga yet uses, the snapshot rule, and readiness as the degenerate describe()."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-09-22T00:00:00Z
---

# Void Core's word

> A **holiday** is how Void Core reaches a system it does *not* own. It is
> "away" from the pure core. All real I/O is a holiday: the core does no
> file/network/stdout work itself.
> — [Void Core: holiday](../../../../../VoidCore/okf/concepts/holiday.md)

Void Core sketches the interface as:

```
query(tagExpr) -> [row | rune]     get(ref) -> payload
insert(row)    -> ref              describe() -> { capabilities, kind, status }
```

**What Void Core has built is the seam, not the registry.** Every host
operation goes through one effect handler (`vc_set_effect_handler`), and a
generic `effect <op> [args...]` verb carries any host op through it and returns
the result as data. The *tagged holiday registry*, with many holidays selected
by capability and fallback chains, is marked `planned` upstream. **Hormiga's
Antfarm is that registry built host-side**: the nodes are the registry, and the
capability tables are the selection. That is worth saying plainly, because it
means the redesign is designing something Void Core has named and not built. If
it goes well, it is a candidate to offer upstream the way presence and updates
moved into Void Maiz.

# Four words, kept apart

The old page used "holiday", "node" and "interface" almost interchangeably. The
redesign cannot afford that, because the GUI has to show each of them
differently.

| word | what it is | lives in | example |
|---|---|---|---|
| **holiday** | a *protocol implementation*: code that knows how to talk to one kind of system | C++ (`publish/github.cpp`, `publish/push.cpp`, `platform/storage.cpp`) | "how to commit a folder to GitHub Pages" |
| **node** | *one configured instance* of a holiday: a rune in the `antfarm` mantle | the state document | `site-pages`: repo `example-org/example-org.github.io`, branch `gh-pages` |
| **payload** | the *type* of what flows through a port | the glyph's `hints.ports` | `site` |
| **capability** | a *question with a typed answer*, which several kinds of node can answer | `domain/hosting.hpp`, `domain/collab.hpp` | "put this file online, and give me a link" |

A glyph (`hol_github`) is the declaration that joins them: which holiday, which
fields a node of it needs, which ports it has. One holiday can back several
glyphs, and one glyph can answer several capabilities (`hol_github` publishes a
site *and* hosts an image).

# The effect gate

Every crossing is an `effect`, and an `effect` is refused unless it was
granted. This is the rule the whole Antfarm leans on, and it is already
enforced, which is not true of everything on these pages:

```
$ hormiga effect host-online flier-3
refused: `effect` reaches outside the document and this session was not granted
effects. Re-run with --allow-effects=effect if that is intended, or
--dry-run-effects to rehearse it.
```

- **Granted per operation.** `--allow-effects=check-host` grants that one op,
  not every effect.
- **Rehearsable.** `--dry-run-effects` answers with what *would* happen and
  performs nothing.
- **Each op carries a consequence**, written for the person deciding whether
  to hand an agent the grant, not for a changelog. For example, for
  `deploy-site`: *"PUBLISHES THE WEBSITE. Whatever is in site/ becomes the live
  page everyone sees, immediately, and nothing in this application can take it
  back."* `--describe` lists every op with its consequence.
- **The GUI goes through the same door.** A face button compiles an `effect`
  command like any gesture. The Supabase node's *Import now* is
  `effect import-rescue`.

A redesign should keep this exactly and make it *uniform*. The Publish tab
already does it right: its confirmation **quotes `deploy-site`'s own
consequence** (`publish/panel.cpp`), so the button and the CLI say the same
words. The Antfarm's own faces do not. A face button acts without showing what
it is about to do. Every effect a node can fire should confirm with its
consequence sentence in the same way, from one table.

# The effects that cross through the Antfarm today

| effect | reads which node | does |
|---|---|---|
| `deploy-site` | `hol_static_host` | uploads `site/`, writes a `deployment` rune |
| `rollback-site` | `hol_static_host` (`rollback_cmd`) | puts a past deployment back in front of visitors |
| `check-host` | `hol_github` / `hol_static_host` | the smallest real reads a deploy performs |
| `check-store` | `hol_object_store` | lists one key |
| `push-store` | `hol_object_store` | sends the published index or the encrypted backup |
| `host-online` | any image host (see [capabilities](/concepts/platform/antfarm/capabilities.md)) | uploads an image and writes the link back with one `set` |
| `lan-offers`, `lan-peers` | none (listen / announce) | discovery, read-only |
| `lan-share`, `lan-stay`, `lan-join` | `hol_lan_share`, `hol_membership` | share, stay in sync, join |
| `save` | `hol_sqlite` | mirrors the state document into SQLite |
| `import-rescue` | `hol_supabase` | the rescue import (GUI only) |
| `serve-site` | `hol_localhost` | starts the preview server (GUI only) |

Two ops are answered by the GUI host and not by the CLI. That breaks rule 8
in the [index](/concepts/platform/antfarm/index.md) ("every GUI gesture has a
CLI twin"), and the [CLI examples](/concepts/platform/antfarm/cli-examples.md)
list it.

# The shapes holidays come in

Read as arrows between payloads (the category reading from
[Void Reyna](/concepts/projects/void-reyna.md): payload types are the objects,
holidays are the arrows, wiring is composition), the Antfarm's node kinds fall
into a handful of shapes. This is the vocabulary a "what do you want to do?"
wizard should use:

| shape | arrow | examples |
|---|---|---|
| **store** | `records ⇄ world` (both ways, keeps the truth) | SQLite, Supabase |
| **source** | `world → records` (import) | CSV, Sheets, `.ics`, the rescue dump, visitor submissions |
| **file store** | `assets ⇄ world` | local files, an object store |
| **host** | `asset → url` (one way, returns an address) | ImgBB, an object store's `public_url`, the website itself |
| **transform** | `records × assets → site` (pure, no world) | the HTML publisher |
| **server / deployer** | `site × domain → world`, returns a `deployment` | localhost, GitHub Pages, a static host |
| **provider** | `→ domain`, `→ identity` | DNS, sign-in |
| **peer** | `records ⇄ records` across devices | LAN share, a future relay |

**The transform is not a holiday at all**, strictly. `hol_html` touches no world.
It is pure rendering, and the render seam's privacy rule lives in it. It sits
in the Antfarm because it is the head of the publish pipeline and people need
to see that chain. That is a good reason, and the node should say it is local
and pure, so nobody reads it as a service.

# Wrappers: holiday → holiday

The founding design named three utility wrappers: **Fallback** first (the
offline story's second half), then **Cache** and **Logger**, as "resilience as
a graph property". Their type is `holiday → holiday`, which makes them an
algebra of holidays the design has had since July without calling it one.

**Two of them already exist, unnamed:**

- **Fallback**: the snapshot fallback. When the SQLite store is unavailable,
  the application runs read-only on the JSON snapshot, and the SQLite face
  says so.
- **Cache**: the mirror. `assets/mirror.json` plus content-hashed bytes is a
  cache in front of every remote-URL host, rebuildable by re-running.

**Logger** is the dispatcher's log, which already records every effect. The
redesign question is whether wrappers become *visible nodes* (a Fallback node
between core and the store), or stay properties of a node's face. The lean,
recorded in the [workbook](/concepts/platform/antfarm/redesign.md), is faces:
a wrapper that is always on should not cost a node.

# A holiday is an effect boundary plus a Lens

From [Void Reyna](/concepts/projects/void-reyna.md) (2026-08-12), where the
author asked for holidays to be taken seriously mathematically:

> **holiday = an effectful boundary crossing + a pure `Lens` (forward, backward,
> round-trip law).**

The effect is the thin impure part: fetch bytes, send bytes, record what
happened. The Lens is the mapping between the outside shape and runes, with the
law `backward(forward(x)) == x` making "this adapter loses nothing" a test
rather than a hope. Lenses compose, which is what makes the **pivot rule** a
theorem:

> **Never write a direct A→B holiday when A→pivot→B exists. If no pivot exists,
> the first job is to name one.**

**How much of this Hormiga uses: very little, honestly.** No Hormiga holiday is
written as a Void Core `Lens` today. The CSV import, the rescue import, the
`.ics` import and export, the site renderer and the Sheets plan are each a
hand-written mapping, and none of them checks a round trip. The two places the
idea shows clearly are:

- **the calendar**, where one `VEVENT ⟷ rune` lens is the stated design and
  every calendar system is a transport onto it
  ([calendar roadmap](/concepts/sections/calendar-roadmap.md), the X-track);
- **assets**, where the content hash *is* the pivot: every file host maps to
  `sha256 → bytes`, so any host can be swapped for any other.

The [mappings](/concepts/platform/antfarm/mappings.md) page names the pivot
each payload should have. A redesign that wants the mathematics to pay should
start where Void Reyna said it pays first: **one mapping instead of three**
(read, write and persistence written separately is how Portfolio Manager
shipped a lossy-tag bug).

# The snapshot rule

> **Holiday-backed data is resolved from a snapshot, never folded into
> authoritative state at edit time.**

That is why the mirror has two tiers. The cache is not model truth. The one
`set path` that records "we now own a local copy" is. It is also why a publish
writes one `set url` and a `deployment` rune, instead of the vendor's answer
leaking into fields piece by piece. Any new holiday follows the same shape: the
world's answer becomes a small number of commands, logged and undoable.

# describe(), and readiness

Void Core's `describe() -> { capabilities, kind, status }` is what a node's
**face** is: the SQLite face shows rune counts and last save, the ImGBB face
shows whether a key is present, the Supabase face shows whether the rescue
dump exists.

The minimal, typed form of it arrived with the first capability:
`host_problem(node)` returns an empty string or **the one thing the node is
missing**. That is readiness as data rather than as face text. Today each face
computes its status its own way, in `app.cpp`, which is why the CLI cannot ask
"is this node ready?" (there is no verb, only the `check-*` effects that go to
the network). A `describe()` contract per holiday, readable from the CLI, the
face and a phone alike, is the smallest change that would make the Antfarm
legible without redrawing it.

# Holidays throughout the application, and where there are none

Holidays are not only backend configuration. Translation is a holiday call from
the Builder. Deploy is one from the Publish tab. A `model` holiday is what an
assistant would invoke. Import wizards drive sources. "Host it online" is one
from the image editor.

And **styling the application needs no holiday**. The application is the
interior render target. Allomone colouring a contact card crosses nothing. The
line from [the three DSLs](/concepts/foundation/dsls.md) is the one to keep:
*interior is default, and only reaching an external output crosses the
Antfarm.*
