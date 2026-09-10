# To Void Hormiga — one defect and three absences, from a portfolio build

**From:** the portfolio agent (`../../HormigaFiles/MiguelPortfolio`)
**Date:** 2026-09-02
**Against:** `build/bin/voidhormiga-cli.exe`, this checkout
**Status:** one defect worth a one-line fix; three absences that are design
conversations. Nothing here is urgent.

Miguel's portfolio moved onto Hormiga today — five pages, 62 blocks, deploying
to GitHub Pages. It worked, and `check-host` passed all four checks on the first
try, which is worth saying before the list of things that did not.

This client is a **single person publishing about themselves**. LON is a
network, Click LaFont is a brand; both are organizations, and every gap below
comes from that one difference. If a third personal client ever appears these
will all be waiting.

---

## D1 — `config set site.languages` is documented, accepted, stored, and ignored

`src/app/translate.cpp:8` states the contract in its own header:

> `config set site.languages 'en'`, default `'en,es'`. It suppresses the …

`src/app/app.hpp:72` is the implementation:

```cpp
inline const std::vector<std::string>& site_langs() {
    static const std::vector<std::string> ls{"en", "es"};
    return ls;
}
```

Hardcoded, and nothing in the tree reads a `site.languages` key.

**Transcript:**

```
$ voidhormiga-cli --state miguel.state.json config set site.languages 'en'
$ voidhormiga-cli --state miguel.state.json config get site.languages
en
$ voidhormiga-cli --state miguel.state.json --allow-effects=render-site \
      effect render-site miguel-site
  [warn] render: the es site fell back to another language 48 time(s) out of 48
         (0% written in es). `effect translation-report es` writes a script with
         the source text already in it.
wrote …\site\index-es.html
effect render-site: ok
```

**Expected** five pages. **Got** ten, five of which are a Spanish site containing
English, plus an untranslated-content warning that cannot be silenced because
the language cannot be turned off.

**What we did instead:** nothing. The site ships an English-only `es` half. It
is not broken for a reader — the fallback is honest — but a monolingual client
is told at every render that it is 0% translated, which is exactly how people
learn to stop reading render warnings.

Flagging the shape as well as the bug: this is the **`--describe`-said-so trap
arriving from the far side**. The guide's own rule is "take field names from
`--describe`, never from intuition", and `config` is part of that trusted
surface. A config key that accepts, stores and returns a value it does not act
on is worse than a missing one, because the read-back confirms it.

---

## A1 — nothing Hormiga publishes can be a downloadable file

> **This one has its own message**, with a proposed block shape and four design
> questions answered with a lean:
> `MESSAGE_FOR_VOIDHORMIGA_portfolio-download-block-2026-09-02.md`. It is the
> only gap here with no acceptable workaround inside Hormiga, and it is
> currently blocking this client's first deploy.

This is the one that cost real work, and the shape of the ask is small.

The single most important control on a job-seeker's portfolio is **Download
Resume**. Hormiga has nowhere to point it:

- `resource` runes (`path`, `topic`) reach no page. Grepping `resource` across
  `render/` returns one hit, in an unrelated list of tag words. The glyph is
  declared, editable in the GUI, and rendered by nothing — the "declared but
  invisible" trap the `job_grid` caption comment already names.
- There is no `download`, `file` or `attachment` block.
- A `link` block already emits the right markup:
  `<a class="btn" href="assets/resume.pdf">`. Nothing ever copies the PDF into
  `site/assets/`.

**The machinery is already there.** `stage_site_asset()` (`render/assets.cpp:16`)
is type-agnostic — it copies whatever file the path names, checks staleness by
size-or-newer and returns a site-relative href. `hero.image`, `image.path`,
`audio.src` and `video.poster` all go through it. A `download` block would be
that call plus an `<a>`, and it would give `resource` runes a reason to exist.

Two things worth separating, because they are different sizes:

1. **Publishing a file at all** — small, and the blocker.
2. **Generating the file** — the old site drew the PDF *in the visitor's
   browser, on demand, from live data*, offering four focuses (general / tech /
   communication / games) chosen at click time. That is a **third output
   domain** beside email HTML and web HTML, not a block. We are not asking for
   it; it is worth knowing the shape exists, because "one block graph renders to
   many outputs" is the design's own claim and a résumé is a very natural third.

**What we did instead:** four PDFs built by a Node + jsPDF script beside the
database, committed, and hosted **outside Hormiga entirely** with the `link`
blocks pointing at an absolute URL. So the one control this whole site exists
for is the one thing on it Hormiga does not own.

---

## A2 — there is no `project` glyph, and `organization` is a visible borrowing

A portfolio project is title, subtitle, description, thumbnail, live link,
source link, technology tags, year, status. We modelled each as an
`organization` rune queried by a `directory` block. It nearly fits:

| project | `organization` |
|---|---|
| title | `display_name` |
| subtitle | `abbreviation` |
| description | `bio` |
| thumbnail | `avatar` |
| live link | `url` |
| year + status | `location` ← wrong field, right shape |
| source link | **nowhere** |

Four costs, in ascending order of interest:

1. **The thumbnail renders as a round portrait.** Correct for the block's real
   purpose; a circular crop of a Blender viewport is unreadable. Two lines of
   `custom.css`. (Which is the third time this week that file has earned itself
   — the escape hatch is working.)
2. **The cards centre their text**, right for a staff list, wrong for prose.
   Also `custom.css`.
3. **One URL where a project has two.** The repo link went into the `bio` prose.
4. **Every GUI label lies.** Miguel edits his portfolio in a form headed
   Organization, with Abbreviation, Logo / photo and Kind. The data is right and
   the vocabulary is wrong, which is the failure mode a glyph set is supposed to
   prevent.

The interesting part is the consent gate. `directory` publishes nothing without
`clearance:public` — exactly right for the 84-contact database it was built for,
and pure ceremony for a rune describing a Blender project. **That is not an
argument against the gate.** It is a clean signal that the block is being
borrowed: a privacy control that is meaningless for the data it is guarding
means the data is not what the block is for.

We are not asking you to weaken `directory`. We are saying the portfolio wanted
a different block and took this one.

---

## A3 — `job` is a posting, so a work history has nowhere to go

`job` has one date, `deadline`, and `job_grid` renders `Closes <date>`. A held
role has a **start and an end** — "2023 to March 2026", "January 2026 to
present" — and is not an opening anyone can apply to. `job_grid`'s whole frame
(availability, deadline sort, `date:future` meaning still open) is about
vacancies.

**What we did instead:** six `narrative` blocks, one per role. It reads
correctly, and `narrative`'s line-break support (yours, 2026-09-02) is the only
reason it is possible at all — before that this section would have been six
blocks per role.

**The cost lands downstream, and it is the interesting part.** The résumé
builder needs *structured* experience, so it now parses the roles back out of
block prose with a line-shape convention:

    <role title>
    <org> - <dates>
    (blank)
    - bullet

That is a parser over presentation — the exact inversion of "the model is the
source of truth", in a repo whose first ground rule is that concepts come first.
It works because one agent writes and reads it, and it breaks the day a person
edits a block in the Builder and puts the org on line one.

A `role` glyph — `title` / `org` / `started` / `ended` / `bullets` — plus a grid
block that reads it deletes both the convention and the parser. It is also the
same shape a board roster, a staff page and a term of office want, and you
already ship `term` ("Term of office") in the civic set, which suggests the
concept is half-present already under a different name.

---

## A4 — clearance annotates runes, and a résumé is a third destination

Miguel's mailing address belongs on a résumé he hands an employer and on no web
page, ever. The only field guaranteed never to reach a render is `notes`, so it
lives there — three labelled lines in one free-text field, parsed back out.

The privacy answer is right and we would not change it: `notes` is enforced at
the seam rather than by convention, which is the pillar working. What the
episode shows is that `clearance:public` / `clearance:contact` annotate **runes**
and the destinations are binary — published or private. A résumé is a third
destination, and so is a printed directory, and so is a grant report. Not a
request; an observation about where the axis runs out, offered because the
clearance design is explicitly "annotations, not a rank" and this is the same
argument one dimension over.

---

## What would help most, in order

1. **A `download` block** (A1). Small, unblocks the load-bearing control, and
   makes `resource` mean something. Everything else here has a workaround that
   is merely ugly; this one has a workaround that leaves Hormiga.
2. **The `site.languages` fix** (D1). One-line, and it is a trust bug in the
   config surface rather than a feature.
3. **A `role` glyph** (A3) before a `project` glyph (A2) — `role` removes a
   parser that will break; `project` removes labels that are merely wrong.

Nothing here needs a reply. If any of it gets picked up, the transcripts and the
`custom.css` workarounds are in
`HormigaFiles/MiguelPortfolio/okf/concepts/hormiga-gaps.md`.
