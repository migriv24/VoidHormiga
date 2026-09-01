---
type: Concept
title: The Cat Colony — Hormiga's shipped default dataset
description: "The public, fictional cat colony that ships as Hormiga's default demo: 50 tag-rich cats (photos, US locations, birthdays), colony events, a shelter org, and a sample newsletter — born from a replayable transcript (seed_cat_transcript) on fresh boot. Replaces a partner organization.s private data org as both the shipped demo and the Allomone testing ground."
tags: [status:built, audience:dev, confidence:asserted]
timestamp: 2026-08-05T00:00:00Z
---

# Why it exists

Two needs, one dataset. (1) A **shipped default** so anyone who opens Hormiga has
something real to play with — the app should not boot empty. (2) A **testing
ground for Allomone** we're free to invent and reshape, unlike the author's
private partner organization. A colony of fictional cats serves both,
and it's fun — the author's call (2026-08-05): "I don't care much for
professionalism in that regard just yet." Marked **temporary**, but real enough
to demo.

# The real-data move (2026-08-05)

The real partner data is **out of the project folder entirely** — `demo-org.db/.json`
(the 280-thing import), `databases/<org>_*.miga` (the 60 MB bundles), the partner's
`assets/` (77 flyers/photos), and the built `site/`/`exports/` were **moved** to
`Documents/HormigaFiles/<org>/` (moved, not deleted — reversible; the author also
has it backed up). The project no longer contains any real org data. The small
fictional `seed_transcript()` / `seed_issue_transcript()` in `seed.hpp` remain
only as **replay/spine test fixtures** (header-blessed as fictional); the running
app no longer uses them.

# How it ships: born from a transcript

Like every org in Hormiga, the colony is a **replayable transcript** (ground rule
3), not a committed data file. `hormiga::seed_cat_transcript()` (in `seed.hpp`)
emits the dispatcher commands; on a **fresh boot** (no `demo-org.json`), the app
replays it into the `demo-org` data mantle. So the dataset lives in **source**,
ships automatically, and the working `demo-org.json`/`.db` stay gitignored
(runtime, user-mutable). Deterministic (seeded RNG) → reproducible.

`tools/catgen.cpp` is now a thin **exporter**: it replays the *same* transcript
headless and writes `export_state()` to a JSON snapshot (for a portable copy or a
future `.miga` pack). One source of truth, no divergence.

# What's in it (106 runes)

- **50 cats, as `contact` runes** (author, 2026-08-05: cats are **contacts**, not
  a bespoke `cat` glyph — the point is to exercise the *contact* datatype and
  contact-to-contact relationships). Each has an **avatar** (photo), a **role**
  (e.g. "grumpy orange cat"), a **bio**, `geo` (real lat/lon → the **map**), and
  **heavily overlapping tags**: `cat` (the bare species tag, for the set-theory
  example), sex, a bare coat color, `breed:…`, `state:XX`, a personality, one–two
  affinities, `located`. Cat attributes ride as **tags** because `contact`
  doesn't declare coat/breed/etc. as fields (and projection drops undeclared
  fields).
- **~55 cat-to-cat relationships** — `friend-of`, `sibling-of`, `rival-of`,
  `grooms`, `plays-with`, `mentor-of` — so the connections/relations UI has real,
  varied edges to test (the whole reason to use `contact`).
- **50 birthday `event`s** — dated this year (on the **calendar**), `link`ed back
  to their cat (`celebrates`).
- **5 colony events** — Annual Cat Show, Spring Adoption Fair, Vaccination
  Clinic, Caturday Meetup, World Napping Championship — dated, located, with a
  few cats `link`ed as `attends`.
- **1 shelter org** — `whisker-haven` (on the map), with ~12 cats `member-of` it.
- **A sample newsletter** — `seed_cat_issue_transcript()` builds "The Cat Colony
  Gazette" in the Builder (hero → narrative → **event_grid** `type:event` →
  **event_grid** `type:event AND birthday` → footer). Query-backed and live.
- **A library of 9 commented Allomone scripts** — `seed_cat_scripts_transcript()`
  seeds `01-hello-allomone` … `09-color-by-glyph` into the `allomone` mantle,
  **disabled**, each teaching one concept (branching, counting, lists/matching,
  functions, recursion, set theory, quantifiers, glyph dispatch). Enable one to
  see it style the cards; they match the user [guide](/concepts/allomone/guide.md).

## The set-theory seam (the author's worked example, baked in)

A canonical set of orange-named cats — garfield, marmalade, pumpkin, biscuit,
tigger, milo — plus any random orange males, are forced **orange + male** and
carry a **second-order** tag set **`{lasagna, mondays, garfield-type}`**. So the
two-hop query "`{orange, cat, male}` → collect their OTHER tags → act on those"
returns a real **group** — the motivating data for the coming set-comprehension +
quantifier increment.

# Photos

Kaggle needs auth + a ~4 GB pull, so photos come from **cataas.com** ("Cat as a
Service", free/no-auth): 50 distinct 400×400 JPEGs committed to **`demo-assets/`**
(the one gitignore exception — shipped on purpose). Each cat's `avatar` is
`demo-assets/cat-NN.jpg`, resolved against `base_dir` at runtime; a missing file
falls back to the letter avatar. Events keep letter-avatars (they aren't cats).
Re-fetch into that folder if ever cleared:
`for i in $(seq 1 50); do curl -sL -o "cat-$(printf %02d $i).jpg"
"https://cataas.com/cat?width=400&height=400&_=$RANDOM$i"; done`.

# Surfacing in the app

The cats are ordinary **contacts** — they appear under **Contact (50)** in the
Data kind-rail, with the normal contact detail form (photo/role/bio/tags/
location/connections), and flow into the calendar, map, Builder, and Allomone
like any contact. There is **no `cat` glyph** — a bespoke datatype for a test
fixture defeats the purpose (testing the real contact datatype).
