---
type: Guide
title: Antfarm v2 — the command line
description: "The farm grammar, designed before the GUI because every gesture needs its twin first: reading (the connections summary, show, kinds, ports, mantles, eval, status, profiles), changing the graph (add, set, rm, plug, unplug, place, key), acting (check, run, preview, publish, migrate), what each compiles to, how raw link stays an escape hatch the audit catches, and the v2 default colony as a transcript. Most of it runs since 2026-09-28; what does not is listed at the top."
tags: [status:direction, audience:dev, audience:agent, confidence:asserted]
timestamp: 2026-09-28T00:00:00Z
---

**Most of this runs since 2026-09-28** (the prototype, two passes). §6 and §7
are real transcripts. **Still proposal:** `farm key reveal`, `farm migrate`,
`farm profile link` / `accept`. `farm init`, `farm arrange`, `farm chambers`,
`farm showcase` and `farm key set` (standard input) were added while building.
**§5 is the design's colony, not what `farm init` writes today:** the built seed
also adds a `not-private` tag query and a `public` filter feeding every
document, a calendar (`events`), a map (`places`) and this device's profile
(`me`); §6's first lines show it.

**How it is reached.** Void Maiz's headless session has no host-verb hook and
Void Core has no host-registered verbs, so `farm` is a subcommand of the
process, like `update`: `voidhormiga-cli --state <db> farm <verb> …` opens its
own session on the same document, with the same lock and journal, runs one
verb, and closes. Inside the application, the command bar takes `farm …`
directly.

# 1. Principles

- **One noun, `farm`.** It has been reserved for the Antfarm in
  [the verb inventory](/verbs.md) since July. v1's planned
  `holiday add|rm|status` family is dropped in its favour: a second vocabulary
  for the same nouns is how a CLI becomes two CLIs.
- **Names, never indices.** Nodes by name, ports by name
  (`site.publish`), in input and in every printout.
- **Checked at the door.** `farm plug` refuses an ill-typed or over-full wire
  before anything is dispatched (A1, answered).
- **Compiles to core commands.** A `farm` line is routed like the `map` and
  `doc` verbs (`app/verbs.cpp`): checked, then dispatched as `rune new`, `set`,
  `link` and `effect`. The log and replay stay in Void Core's vocabulary.
- **Rehearse by default where it writes.** `farm run` and `farm migrate`
  report what would happen, and write only with `apply`, as the sync effects
  already do.
- **Effects stay effects.** Everything that reaches outside is an `effect` op
  under the same gate: `--allow-effects=<op>`, `--dry-run-effects`, and a
  consequence sentence in `--describe`.
- **`--json` everywhere**, for agents.

# 2. Reading

| verb | answers | GUI twin |
|---|---|---|
| `farm` | the Connections summary: every group, every row, its readiness and primary action | the Connections view |
| `farm ls [--stratum S] [--kind K]` | nodes | the Wiring view |
| `farm show <node>` | its kind, stratum, placement, readiness; each socket by name and type with what it is wired to; fields grouped as *where* / *key* / *options* | the Inspector |
| `farm kinds [--answers Q] [--key K] [--document D]` | the palette, filtered: kinds that answer a question, that a key serves, that a document kind can use | the palette, the wizard |
| `farm ports <kind>` | a kind's sockets: name, type, direction, cardinality | socket hover |
| `farm mantles` | every chamber: runes, bytes, growth over 30 days | the Miga face |
| `farm eval <node>.<port>` | the value of a pure output now: a filter's count and compiled query, a document's rune count | a face's numbers |
| `farm status [<node>]` | readiness, **with no network request**; ill-typed wires found in the graph | readiness dots |
| `farm profiles` | profiles: here or last seen, device, holds, links | the Network group |
| `farm version` | this Antfarm's version (`2`), and whether a v1 graph is still present | the tab's footer |

# 3. Changing the graph

| verb | compiles to |
|---|---|
| `farm add <kind> <name> [field=value …]` | `rune new <kind> <name>` in the `farm` mantle, then one `set` per field, as one batch |
| `farm set <node> field=value …` | `set`, one batch |
| `farm rm <node>` | the node and every wire touching it, one batch, one undo |
| `farm plug <a>.<out> <b>.<in>` | after the check, `link <a> <b> --relation <out>:<in>` |
| `farm unplug <a>.<out> <b>.<in>` | `unlink` |
| `farm place <node> on <profile>\|each\|any` | `set <node> placement …` |
| `farm key add <provider> <name>` | the rune, as `farm add`; the value is read from a prompt or stdin and sealed into this device's vault, never an argument |
| `farm key set <key>` | a new value, same path; shared to members on the next sealed session |
| `farm key <key>` | the rune: provider, serves, added by and where, expiry, last four, which members hold the current value |
| `farm profile link <profile>` / `farm profile accept <profile>` | a `same-person` relation proposed by one side, completed by the other |

**What the check refuses, and how it says so:**

```
$ hormiga farm plug data-grant.kept pages.in
refused: pages.in takes a rendition, and data-grant.kept is a mantle.
         A mantle becomes a rendition through a document, for example:
         farm plug data-grant.kept site.data
$ hormiga farm plug cloudflare-main.key pages.key
refused: pages.key accepts one key and already has github-main.
         farm unplug github-main.key pages.key   first, to replace it.
$ hormiga farm plug home.river this-db.rests-in     # home has no live reservoir
refused: a database that is being written must rest on this device.
         To keep a copy in R2, store a backup there.
```

**Raw `link` still works**, because it is a Void Core verb and Hormiga does not
patch Void Core. The door checks. **The audit catches what went around it:**
`farm status` reports any wire in the `farm` mantle whose ports or types do not
match, as *failing*, with the command that made it. The ask upstream stands:
whether port types should become part of Void Core's glyph contract, so that the
door is the engine's own.

# 4. Acting

| verb | effect op | consequence (the sentence the gate shows) |
|---|---|---|
| `farm check <node>` | `check` | the smallest real read the node's kind allows. *"Asks <vendor> whether this key/bucket/host answers. Sends the key, nothing else."* |
| `farm run <node> [apply]` | per kind (`import-csv`, `store`, …) | a source or store, run. Rehearses unless `apply` |
| `farm preview <document>` | `preview` | renders and serves or sends to its preview domain |
| `farm publish <document> [--to <domain>]` | `deploy-site`, and later `send` | as today, for sites; for a message, *what leaves* is named |
| `farm key reveal <key>` | `reveal-key` | *"Shows the key in full on this screen. It is logged that you did."* |
| `farm migrate [apply]` | `migrate-farm` | see [migration](/concepts/platform/antfarm/v2/migration.md) |

# 5. The v2 default colony

What `seed_farm_transcript()` would write for a new database, as the commands
it compiles from. It is entirely local, like v1's.

```
mantle new farm
farm add miga this-db
farm add folder here path=assets
farm place here on each
farm add river home
farm plug here.river home.reservoirs
farm plug home.river this-db.rests-in
farm add separate chambers
farm plug this-db.all chambers.mantle
farm add tunnel-data-assets pictures
farm plug chambers.data pictures.data
farm plug chambers.assets pictures.assets
farm add website site
farm plug chambers.data site.data
farm add local-domain preview-here port=8780
farm place preview-here on each
farm plug site.preview preview-here.in
```

and what reading it back would print:

```
$ hormiga farm
WHERE THE DATABASE RESTS
  ● home           ready     here (this device) · 38 MB
WHERE FILES ARE KEPT
  ● home           ready     1 reservoir · 412 files
DOCUMENTS AND WHERE THEY GO
  ● site           ready     preview → preview-here (localhost:8780) · never published
                             publish → nowhere yet            [Add a domain]
KEYS
  (none)                                                      [Add a key]
THE NETWORK
  ● you (desktop)  here
PLANNED
  ○ Mail domain   ○ Sheets import   ○ Sign-in
```

# 6. A real transcript (2026-09-28)

Run against `voidhormiga-cli` from the working tree, in a scratch folder holding
the Cat Dataset (`catgen`), so no organization's data was involved. Outputs are
verbatim; `hormiga` stands for `voidhormiga-cli --state cats.json`.

```
$ hormiga farm
no v2 Antfarm in this database yet - `farm init` creates one

$ hormiga farm init
created the v2 Antfarm: `farm` shows it, `farm arrange` lays it out

$ hormiga farm
WHERE THE DATABASE RESTS
  [ready]    this-db          rests in home
WHERE FILES ARE KEPT
  [ready]    home             1 reservoir
DOCUMENTS AND WHERE THEY GO
  [ready]    site             preview -> preview-here · publish -> nowhere yet
  [unset]    events           no document named
  [ready]    places           preview -> preview-here · publish -> nowhere yet
DOMAINS
  [ready]    preview-here     http://localhost:8780
THE NETWORK
  [ready]    me               migriv24 · here (this device)

$ hormiga farm eval public.kept
public.kept  (mantle)
  107 runes, 37.2 KB, 3 strand(s)
    data/contact  50
    data/event  55
    data/map  1
    data/organization  1
  selected by: NOT private

$ hormiga farm plug public.kept preview-here.renditions
refused: preview-here.renditions takes a rendition, and public.kept is a mantle. A mantle becomes a rendition through a document.

$ hormiga farm add web-domain pages host=github target=example-org/example-org.github.io
added pages (Web domain)

$ hormiga farm plug site.publish pages.renditions

$ hormiga farm key add github gh-main
added the key gh-main. Its value is set with `farm key set gh-main` (read from standard input, never an argument).

$ hormiga farm plug gh-main.key pages.key

$ hormiga farm status
  …
  [needs]    site             pages needs: its key: this database has no vault yet
  [needs]    pages            its key: this database has no vault yet
  [needs]    gh-main          this database has no vault yet
```

Readiness travels along the wires: the website needs its web domain, which
needs its key, and the sentence says so all the way up.
`echo <token> | hormiga farm key set gh-main` then seals the value into this
device's vault, and all three turn *ready*.

# 7. The chambers, the showcase and the effects (2026-09-28, second pass)

Run against a fresh Cat Dataset (`catgen`) in a scratch folder. `hormiga` is
`voidhormiga-cli --state cats.json`. Outputs are verbatim, shortened only where
marked.

```
$ hormiga farm showcase
the Cat Colony showcase: website, birthdays calendar, map, newsletter, a photo river, keys and a web domain
`farm` shows it; `farm run new-cats` imports the new arrivals; `farm run mirror-photos` copies the photos to the USB stick folder

$ hormiga farm mantles
  data            108 runes  37.4 KB
  assets           50 runes  4.3 MB
  network           1 runes  249 B
  documents         4 runes  646 B

$ hormiga farm run new-cats
refused: `effect` reaches outside the document and this session was not granted effects (farm-run: an import
adds runes (one undoable batch); a store COPIES FILES into another folder or PUTS THEM ONLINE through a bucket
or image host, where anyone with the link can see them). Re-run with --allow-effects=farm-run if that is
intended, or --dry-run-effects to rehearse it.

$ hormiga --allow-effects=farm-run farm run new-cats
would import 6 contact(s) from imports/incoming-cats.csv
(rehearsal - add `apply` to import, as one batch and one undo)

$ hormiga --allow-effects=farm-run farm run new-cats apply
imported 6 contact(s) from imports/incoming-cats.csv

$ hormiga --allow-effects=farm-run farm run mirror-photos apply
50 file(s) named; 100 copied, 0 put online, 0 already there, 0 not on this device

$ hormiga --allow-effects=farm-preview farm preview colony-site
built into …\cats\site:
  colony-site -> / (cat-colony-site)
  cat-news -> /news/
  birthdays -> /birthdays/calendar.ics
  where-they-nap -> /map/places.geojson (50 places)

$ hormiga --allow-effects=farm-publish farm publish colony-site
cats-on-pages is not ready: its key: this database has no vault yet

$ hormiga --allow-effects=farm-check farm check usb-stick
ok: …\cats\mirror/usb-stick is writable, 33.0 GB free
```

The store copied 100 files: the demo photos live beside the executable, so the
home folder got them (local first) and so did the USB-stick folder. The
publish refuses before it builds anything, with the reason the key node itself
gives.
