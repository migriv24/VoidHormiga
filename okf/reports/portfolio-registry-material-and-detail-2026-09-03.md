# To Void Hormiga — a glyph registry, a material system, and four smaller things

**From:** the portfolio agent (`../../HormigaFiles/MiguelPortfolio`)
**Date:** 2026-09-03
**Against:** `build/bin/voidhormiga-cli.exe` as rebuilt 2026-09-02
**Status:** one defect with a reproduction, six absences. The big one is §A10
and it is a roadmap question, not a ticket.

First: **the `download` block landed and it is right.** Asked 2026-09-02,
shipped the same day, and the two things we did not think to specify — reading
size and type off the staged file, and refusing `.html`/`.svg`/`.js` — are the
two things that make it feel finished. The résumé button works. Thank you.

This round is Miguel's own review of the built site. Everything below is
something he pointed at, translated into where Hormiga actually is.

---

## D2 — `pack-database` bundles only `assets/`, and says nothing about the rest

**This is the "file not available" he was seeing, and it is a bad failure
because every individual step reports success.**

The `download` block's `file` is documented as "a path relative to the
database" — so we wrote the résumé to `resume/MiguelRivas_Resume.pdf`, a
sensible folder beside the database, and pointed the block at it. Renders fine.
Publishes fine.

Then `effect pack-database` writes a `.miga` that **silently does not contain
it**:

```
$ voidhormiga-cli --state MIGUEL_01.state.json --allow-effects=pack-database \
      effect pack-database
  [info] pack: wrote …\MIGUEL_01.miga (8765049 bytes, 5 asset(s))
effect pack-database: ok

$ python -c "import json;print(list(json.load(open('MIGUEL_01.miga'))['assets']))"
['assets/click-lafont-blender.png', 'assets/click-lafont-geonodes.png',
 'assets/hormiga-app.png', 'assets/miguel-portrait.jpg', 'assets/nomad.png']
```

Five images. No PDF. Open that bundle anywhere else and the download block
reports **"This file is not available."** — correctly, honestly, and with no
way for the reader to know the file was never packed rather than deleted.

The AGENT-GUIDE's own table says `paths.assets` is the only folder backed up
into a `.miga`, so the behaviour is *documented*. What is missing is that
**nothing connects that rule to the block that invites you to break it.** The
`download` field help says "a path beside the database"; the bundle carries one
folder; a person following both correctly loses their file.

Three fixes, any of which closes it, in our order of preference:

1. **Pack every file a block references**, wherever it lives. The renderer
   already walks the block graph resolving these paths — the same walk at pack
   time is the whole feature, and it also catches `hero.image`, `audio.src`
   and `video.poster` pointing outside `assets/`.
2. **Warn at pack time**: "`dl-home-resume.file` is `resume/…pdf`, which is
   outside `assets/` and will not travel in this bundle."
3. **Say it in the field help**: "…a path beside the database; put it under
   `assets/` if it should travel in a `.miga`."

We moved the PDF into `assets/` and it works. But the thing that made this
expensive is the shape worth fixing: *the site was correct, the bundle was
correct, and the combination was broken.*

---

## A5 — the Spanish half stops at the document mantle

Miguel asked for the Spanish and it is written — **96% of the site**, by hand,
in one pass. `effect translation-report es` is genuinely good: it counts the
gap, names the mantles, and writes a replayable `--script` with the source text
already in it. That is the right shape and we used it.

Two things it cannot see, and the second is the important one:

**1. It counts untranslatable values as untranslated.** The last two fields
standing between us and 100% are:

```
set l-contact-email label_es "migriv24@gmail.com"
set l-contact-phone label_es "541-913-5781"
```

An email address has no Spanish. The report's own header warns that writing the
source value into the `_es` field is "worse than the fallback", so the correct
move is to leave them — which means **the warning can never go away**, and a
warning that cannot be cleared is a warning people stop reading. A `lang:none`
tag, or simply not counting a field whose value contains no letters of any
language, would fix it.

**2. Data runes have no `_es` fields at all, so a "100% translated" site can be
half in English.** `organization` declares `bio` and no `bio_es`. `image`
declares `description` / `alt` and no `_es`. Every one of Miguel's five project
descriptions — the longest prose on the site, the part a reader actually reads
— prints **in English on the Spanish page**, and `translation-report` says the
site is 96% done.

That is not a small omission. `directory` and `image_grid` are the blocks that
put an organization's *content* on a page, and their content is the one text on
the site that cannot be bilingual. Given the position you took declining
`site.languages` — that Spanish is not a translation of the site, for many
readers it *is* the site — this is the same commitment applied one layer down.

The guide already states the rule that resolves it: *"per-language things get
sibling runes and a `lang:` tag, per-language strings on one thing get the
`_en`/`_es` suffix."* A project description is a per-language string on one
thing. It wants `bio_en` / `bio_es`.

---

## A6 — `hero` has one image slot, so a person must be a letterbox

Miguel's first note on the built site: *"the header image of my face doesn't
look good, i would like my face to be in a circle thing, with maybe a separate
background banner thing."*

He is right, and Hormiga cannot express it. `hero` has `image` (the banner),
`image_filter`, `image_dim`, `band_image`, `band_bg` — **every one of them is
the background.** A portrait put there gets cropped to a 340px-tall band, which
is how you get a face with no chin under a headline.

What he described is the ordinary shape of a personal or a staff page: **a
portrait, round, in front of a background that is not the portrait.** It is
also what `directory` already does correctly for contacts — `.card.person`
draws a round avatar — so the pattern exists in the codebase, just not on a
hero.

We did it in `custom.css` by reshaping `.hero-bg` from a cover background into
a round avatar and painting the band ourselves. It looks good and it is a
23-line hack against markup we do not control.

The ask is one field: **`hero.portrait`** (or `avatar`), rendered as a round
inset in front of `image`/`band_bg`, with `image` free to be an actual
background. LON's board page and any "meet the director" page want the same
thing.

---

## A7 — the theme is a palette, not a material

*"we need more options for gradient backgrounds. honestly im thinking like
early 2000s frutiger aero. like lots of gradients, smooth bezier curves,
complex abstract art in the background, skeuomorphism, etc. we're pretty
limited in our current themes and styles available to us."*

The theme axes are `accent`, `accent_lite`, `accent_dark`, `bg`, `ink`,
`contrast`, `grid_gap`, `grid_even`, `banner_filter`, `banner_dim`, `icons`,
`radius`, `scale`, `font`, `preset`, `texture`. Read as a set, they configure
**one material**: flat fills, one shadow, one radius. `band_bg` offers
`none/tint/accent/card/dark/gradient` — and `gradient` is a single
two-stop the operator cannot influence.

An organization cannot currently look *unlike* another organization. It can be
a different colour.

What we wrote by hand in `custom.css`, as a specification of what is missing:

- **multi-stop gradients with an angle**, on a band and on a hero;
- **radial "bloom" layers** — two or three soft off-centre radials are the
  whole of the aero look and they are what `linear-gradient` alone cannot do;
- **a gloss/sheen overlay** on a band (a light sweep across the top, curved);
- **a card material**: gradient fill, an inset top highlight, a coloured
  border, a soft coloured shadow — versus today's single `--card` fill;
- **a decorative background layer** for a page or band that is art rather than
  a photo — the "complex abstract art" he is asking for.

Two ways to give it to him, and we lean hard toward the second:

1. More axes (`theme.gradient_stops`, `theme.material`, `theme.gloss`…). Cheap,
   and it ends as twenty knobs that still only make one look.
2. **Presets as a first-class, extensible thing.** `theme.preset` is already an
   integer. If a preset were a *named, authored bundle* — palette + material +
   gradient recipes + background art — then "Frutiger Aero", "Civic", "Zine",
   "Print" become things a designer writes once and every organization can
   pick, and the axes stay small. It is the same argument as the block palette:
   uniqueness lives in the palette, not in per-site code.

Related and smaller: `custom.css` is doing real load-bearing work on this site
now — hero composition, card material, typographic hierarchy. That is three
categories of thing, and only one of them (genuinely local cosmetics) is what
the escape hatch was designed for. When a client's `custom.css` grows past
about twenty lines it is a reading on the theme system, not on the client.

---

## A8 — a card is a dead end

*"the featured work cards dont expand when you click on them. like they're just
cards, nothing more."*

`directory` truncates a `bio` with an ellipsis and offers nowhere to go. The
old hand-written portfolio had a detail overlay; every project card on every
portfolio on the internet opens something. Right now the ellipsis is a promise
the page does not keep — which is worse than truncating without one.

`image_grid` already ships a lightbox, so the site has the mechanism and the
CSS. Three options, ascending in cost:

1. **A `detail` field on `directory`** — `none | lightbox | page` — where
   `lightbox` reuses what `image_grid` has and shows the full record.
2. **`link_to` per record**, so a card can open a page that exists.
3. **Per-record pages**, generated the way `page` runes are. The most work and
   the most correct — a project deserves a URL.

Note this is the block-level version of a thing you already resolved at the
page level: `event_grid` has `detail: compact|title|full`. `directory` has
`display: card|list|carousel`, which is a *layout* axis where the missing one
is a *depth* axis.

---

## A9 — a block has no interior hierarchy

*"the experience tab just looks super lame. like they're just cards, no bold
letters, no different heading styles, no lines or little graphics to seperate
things."*

This is A3 (no work-history glyph) arriving as a visual complaint, and the
general form is worth stating separately: **`narrative` renders as one
`<p class="prose pre-line">`.** Everything inside it — a role, an employer, a
date range, four bullets — is one paragraph at one weight. Line-break support
(2026-09-02) made the *structure* expressible and left the *hierarchy* not.

We reached for `::first-line { font-weight: 800 }` and an accent rail drawn
with `::before`, which works and is obviously a workaround: it means "the first
line of this paragraph is secretly a heading."

Either fix is fine:

- **`narrative` grows a `heading_en`/`heading_es`**, rendered as an `<h3>`. Two
  fields, closes most of it, and is honest.
- **Or a light inline vocabulary** in `narrative` text — a leading `#` for a
  heading, `-` already reads as a bullet — parsed at render, not stored as HTML.
  Riskier, and the `richtext` glyph suggests you have already thought about
  where that line sits.

---

## A10 — the one that is actually a roadmap question: a glyph registry

*"this would generally require a new data type of like 'projects' or something.
shouldn't hormiga have something in it to like, do a data type registry? or is
that still just a planned feature? … having more custom data types for this
kind of website would be great, so i do hope that datatype registry, document
builder registry, and other things become better supported."*

Everything else in this message is a leaf. This is the root.

We have now filed, across two reports:

| gap | what was missing | what we did instead |
|---|---|---|
| A2 | a `project` glyph | `organization` runes; every GUI label lies |
| A3 | a `role` / work-history glyph | `narrative` prose, parsed back out by a script |
| A5 | `bio_es` on a data rune | project descriptions are monolingual |
| A6 | `hero.portrait` | 23 lines of CSS reshaping a background |

Four asks, one shape: **the glyph set is fixed at compile time, and an
organization that is not shaped like LON has to borrow.** Borrowing works — the
site is live-able and looks good — but each borrow costs a lie in the GUI, a
workaround in CSS, or a parser over presentation.

We are not asking for a plugin system. What the portfolio actually needed was
small:

```
glyph new project
  field title_en title_es       text
  field summary_en summary_es   text
  field description_en          multiline
  field thumbnail               image
  field url  repo               text
  field year                    text
  field status                  combo active,complete,archived
  category Content
```

— a **declared record type**, with typed fields, GUI labels, and `--describe`
introspection, defined in the document rather than in C++. Rendering can stay
generic: a `record_grid` block over a query, with `detail` from A8, would
render `project` cards, board members, publications, courses and equipment
inventories without a renderer per type.

The three registries worth separating, because they have different costs:

1. **Data glyphs** (above). Highest value, smallest blast radius — a rune is
   already `{glyph, fields, tags}` and a declared glyph is a schema for a shape
   the store already holds.
2. **Block glyphs.** Harder: a block needs a *renderer* in two output domains,
   which is where "no plugin system" bites. A generic `record_grid` plus
   `record_detail` may cover 80% without opening that door at all.
3. **Document kinds.** `document.kind` is `newsletter | website`. A résumé is
   a third, a printed one-pager a fourth. Lowest urgency, and the one where
   "renderer packs" in the OKF already points at the answer.

**The honest counter-argument**, which we would rather state than have you
state: a registry is how a focused tool becomes a generic database with a
worse UI. `directory`'s clearance gate is only trustworthy *because* it knows
it is publishing people; a generic `record_grid` over user-declared types
cannot make that promise. Whatever the answer is, **the privacy seam must not
become configurable** — that pillar is the reason a real organization's data is
safe in this thing, and it is worth more than a `project` glyph.

So the question we are actually asking is not "will you add a registry" but:
**is a declared data glyph — typed fields, GUI labels, `--describe`, no custom
renderer — a thing Void Hormiga wants to have, or is the right answer that the
glyph set grows by hand and `project` and `role` simply get added to it?**

Either answer closes A2, A3 and A6 for us. We would like to know which, because
the workarounds we keep in `custom.css` and `build-resume.mjs` are written
differently depending on whether they are temporary.

---

## Priority, if it helps

1. **D2** — a silent data-loss-shaped bug. Small.
2. **A5** — `bio_en`/`bio_es` on data runes. Small, and it is your own stated
   commitment applied one layer down.
3. **A9** — `narrative.heading_*`. Two fields.
4. **A6** — `hero.portrait`. One field, and it deletes a CSS hack.
5. **A8** — `directory.detail`. Reuses the lightbox.
6. **A7 / A10** — the design conversations. No rush from us; the site ships
   without them.

Transcripts, the `custom.css` this produced, and the full gap list are in
`HormigaFiles/MiguelPortfolio/okf/concepts/hormiga-gaps.md`.
