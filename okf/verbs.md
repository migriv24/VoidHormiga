---
type: Reference
title: Verb & noun inventory
description: "The seed of Hormiga's CLI vocabulary — the nouns of the domain and the verbs added to the voidscript surface, per Q8. Speculative until validated against the predecessor's route inventory in phase C."
tags: [status:draft, audience:dev, confidence:speculative]
timestamp: 2026-07-16T00:00:00Z
---

Q8's decision (author, 2026-07-16): Hormiga is a large application and gets a
**real library of verbs and nouns** on the voidscript surface. This file is
the living inventory. Ground rules: every verb is dispatcher-routed, logged,
and replayable; anything reaching outside the model is an `effect` (host
compute or a holiday call); the predecessor's ~100-route list is the phase C
checklist that keeps this honest — every old route becomes a verb here, an
effect, or dies with the server.

> **Status: seed, speculative.** Core-inherited names must be trued against
> Void Core's SPEC when phase B wires up; app verbs firm up as their phases
> build them. Naming style follows the core: short, lowercase, noun-first
> where a noun is being managed.

# Inherited from Void Core (not ours to define)

The model verbs come free and are listed only to show the boundary: rune
creation/removal, field `set`/`setjson`, `link`/`unlink`, tagging, `temper`
passes, mantle navigation, `config set`, undo/redo, save/load, `effect <op>`
routing. Hormiga adds *domain vocabulary on top*, never a parallel model
surface.

# The nouns

`contact` · `org` (the external organizations contacts belong to) · `event` ·
`job` · `image` · `resource` · `note` — the data glyphs.
`issue` (a newsletter) · `site` · `page` · `block` · `theme` — the document
side. `holiday` · `farm` (the Antfarm mantle) · `colony` (the org itself,
the `.miga` registry) — the infrastructure side.

# App verbs, by area (each firms up in its phase)

## Data & ingestion (phase C)

- `import <holiday> [args]` — drive an Import holiday (rescue dump, Sheets,
  CSV); the old→new schema mapping lives in the holiday (Q2).
- `ingest <path> [tags…]` — bring an asset in through the Asset holiday:
  content-hash, metadata rune, tag suggestions.
- `axes` / `axis set <ns> <axis>` — the tag-namespace registry that `temper`
  passes and completion read from.

## The builder (phase D)

- `materialize <block>` — bake a query-backed block's current results into
  the issue as content; undoable.
- `translate <mantle|rune> [--missing]` — the bilingual engine's batch pass:
  fill absent language fields via the Translate holiday, marking provenance;
  never clobbers hand-edited fields.
- `effect render <mantle> <domain>` — walk the chain through the renderer
  pack; `effect preview <mantle> <domain>` opens the result in the system
  browser (a viewer, per the boundary).

## Asking the database (built 2026-08-28)

- `effect query '<expression>'` — the runes a block query selects **right now**:
  the same evaluation the renderers and the Builder preview run, so it
  understands `date:past`, `date:today`, `date:future`, `date:recurring` and
  `date:undated`. Read-only; prints each hit's date verdict beside it.

  > **Why this is not `ls --tag`.** `ls --tag` is Void Core's verb over Void
  > Core's grammar, and that grammar has no clock — it cannot answer a `date:`
  > question and never will without an upstream change we are not entitled to
  > make. Since the AGENT-GUIDE tells every caller to check an expression before
  > putting it in a block, a checker that quietly disagrees with the renderer
  > would hand out confident wrong answers, which is the exact failure that
  > guidance exists to prevent. **Use `ls --tag` for a pure tag expression; use
  > `effect query` for anything mentioning a date, and for anything you are
  > about to paste into a block.** See
  > [blocks & domains](/concepts/sections/blocks-and-domains.md).

- `effect read-flier <image-rune>` — propose `kw:` tags for one flier and check
  the date printed on it against the event it is wired to. **Proposes only: it
  dispatches nothing.** There is no OCR in the binary; the recognizer is the
  operator's own, named once in `config set tools.image_text "<cmd with
  {path}>"`, the same decision `deploy_cmd` makes. Absent by default.

## Publish (phase E)

- `effect deploy <site|export> <holiday>` — folder copy, GitHub Pages,
  self-hosted static host (Q10).
- `effect send <issue> <holiday>` — email dispatch through Courier-class
  holidays, with the Q9 image resolver chosen at this seam.
- `update check` / `update apply` — the signed self-updater; visible, logged.

## The colony & the farm (phases C–F)

- `colony create|unlock|lock` — the `.miga` v2 lifecycle: passphrase KDF,
  master-key derivation, the encryption switch offered at creation.
- `colony invite|revoke` — trust topology, keys not passwords (phase F).

## Sync (built 2026-08-27 — [collaboration](/concepts/platform/collaboration.md))

Shipped as effects rather than bare verbs, because each reaches outside the
model: a file, a socket, or a merge that replaces the document.

**Every one of these REPORTS by default and writes only on `apply`.** That is
the deliberate ergonomic: a merge is the one operation here that can lose a
person's work, so seeing what would happen is what you get by typing the obvious
thing.

- `effect sync-version` — this database's Void Palabra cut name. Two devices
  printing the same string need no sync.
- `effect sync-merge <path> [apply]` — merge another state document or `.miga`.
- `effect lan-peers [seconds]` — announce and list; read-only, opens no stream.
- `effect lan-serve [seconds] [apply]` — wait for a peer, exchange, merge.
- `effect lan-sync <host> [port] [apply]` — dial a peer, exchange, merge.

> **Naming note, and a decision against a proposal.** A `db info|sync|prune|
> wipe|export|logout` family was proposed 2026-08-27 and declined: `colony` is
> already this OKF's noun for the org/`.miga` lifecycle (above), and the built
> effects are already `new-database`, `open-database`, `save-database`,
> `backup-database`, `restore-database`. A second vocabulary for the same nouns
> is how a CLI becomes two CLIs. `db export` in particular duplicates
> `save-database as` plus `backup-database` outright.
>
> **The gaps the proposal correctly identified are real and stay open:** there is
> no `colony info` (size, asset count, last sync, known peers) and no
> `colony prune`. They should be built under those names.
- `holiday add|rm|enable|disable <kind> [config]` — Antfarm surgery, secrets
  field-encrypted at write.
- `holiday status [name]` — surface `describe()`; what the node faces show.
- `snapshot` — force the JSON mirror the fallback story rides on.

# Not verbs

Camera, pane fractions, selection — view state, `config` tier, undo-exempt
(the upstream lasagna discipline). And nothing here executes blocks: a
document is a structure, not a program.
