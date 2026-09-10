---
type: Questions
title: Developer questions
description: Open decisions for the author, each with a lean. The eight founding questions were answered 2026-07-16; three new ones opened by those answers.
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-07-16T00:00:00Z
---

Open decisions, with leans so a non-answer has a sensible default. Answers
fold into concepts and clear from here.

# Open

- **Q67 — libsodium is vendored as a WINDOWS BINARY. Re-vendor the sources?**
  (Opened 2026-09-10, found by the first Linux build.) **Lean: yes, vendor the
  sources, the way SQLite already is.**

  `vendor/libsodium/` holds headers and a prebuilt `lib/libsodium.a` and **no
  sources at all**. That `.a` is a ucrt64 build, so the first Linux link failed
  on `__imp_EnterCriticalSection`, `__imp__errno` and `___chkstk_ms` — Windows
  symbols inside a file this tree calls vendored.

  **Why this is a rule question and not a build question.** Ground rule 5 says
  *vendor, don't depend*, and for two months this looked like compliance. Every
  other vendored piece — SQLite as one amalgamation `.c`, BLAKE3, the
  single-file headers — is **source**, and source is portable by construction.
  A vendored **binary** is a dependency on one toolchain wearing a vendor's
  clothes: it is single-platform, it cannot be audited by reading it, and its
  provenance is whoever built it. The rule was doing one job and appearing to do
  two, which is the same shape as the `_vendors_note` in `void.json` — *"the
  rule decided how third-party code is ACQUIRED and quietly decided it need not
  be WRITTEN DOWN."*

  **The interim, which is a real deviation and is stated rather than silent:**
  off Windows the system libsodium is used (`apt install libsodium-dev`, `brew
  install libsodium`), and the configure step says so out loud and fails with a
  sentence naming the fix if it is absent. That is a package-manager dependency
  on two platforms, which ground rule 5 exists to prevent.

  **Against doing it now:** it is a re-vendor with a version to record and a
  build to wire up, and it was found inside a change whose entire purpose was
  getting an artifact onto two platforms. This project has learned once what
  happens when a refactor rides along with a fix.

  **What settles it:** whether Linux and macOS become supported platforms at
  all. If they stay untested curiosities the interim is honest and cheap; the
  moment either is a platform an organization runs on, a package-manager
  dependency is not acceptable and the sources have to come in.

- **Q66 — should `link` carry `caption_en` / `caption_es`?** (Opened
  2026-09-08, Click LaFont's platform-set report §3.) **Lean: yes, and only
  that — two fields, no container.**

  A platform card today is a label and nothing more, and the report shows why
  that is a real limit rather than a cosmetic one. They built §6.2b's row, put
  three explanatory cards in the row beneath it column-aligned under the
  buttons, and produced a page that is correct on the author's machine and wrong
  for every visitor whose computer is not the author's — because **a set
  reorders its own row and nothing else**, so the badge ends up over a card
  describing a different operating system. Nothing warns.

  The reason it could not be fixed by keeping the cards is the part that turns
  this into a question:

  - a card that both **moves with the set** and **explains itself** has to be
    one block, because the unit of movement is one `.wcol`;
  - `link` carries `platform` but no caption;
  - `download` carries `platform` *and* `caption_*`, but its `file` is a local
    path beside the database, so it cannot point at a GitHub release — and §3 of
    [the download page](/concepts/platform/download-page.md) says it should not.

  So the explanation and the thing being explained cannot currently be the same
  block, and any arrangement that puts them in different blocks makes a
  positional promise the set will break.

  **What is asked for is exactly the two fields `download`, `video` and
  `image_grid` already carry**, which is the argument for it: it is existing
  vocabulary reaching one more block, not a new concept. It lets a platform card
  read *"Windows 10 or 11, 64-bit, 7.4 MB"* under its own label and travel with
  it, and it is useful well outside platform sets — every `link_style 'button'`
  on every site has wanted a subtitle at some point.

  **Two things deliberately NOT asked for**, both by the reporter, and both
  worth keeping refused:

  - a `platform_group` container — *"a new concept for a problem two fields
    solve"*;
  - any way to make a *different* row follow a set — *"that is a positional
    coupling between blocks and it would be a worse thing than the bug."*

  **Why it is a question and not just done:** `link` is the most-placed block in
  the vocabulary and it is currently the one element whose entire contract is
  *label plus target*. Growing it is cheap; growing it wrongly means every nav
  link in every site acquires a field that means nothing there. The counter-lean
  worth stating is that the layout warning now in §6.2b already prevents the
  reported bug at zero cost, so this buys a nicer page rather than a correct
  one.

  The interim fix is shipped either way: §6.2b carries the warning, and the
  reporter's own read is that a sentence is the right size of fix, since
  §6.2b's example does not hit the trap.

- **Q63 — the theme configures ONE MATERIAL, and presets are the axis that
  already exists.** (Opened 2026-09-03, portfolio report A7.) **Lean: their
  option 2 — presets as authored bundles — and it is the same item the Builder
  roadmap has carried as 🔨 since July.**

  The author of a portfolio, on his own built site:

  > we need more options for gradient backgrounds. honestly im thinking like
  > early 2000s frutiger aero. like lots of gradients, smooth bezier curves,
  > complex abstract art in the background, skeuomorphism, etc. we're pretty
  > limited in our current themes and styles available to us.

  The diagnosis in the report is the useful part. Read as a set, the sixteen
  theme axes — `accent`, `bg`, `ink`, `contrast`, `radius`, `scale`, `font`,
  `texture`, `preset`, … — configure **one material**: flat fills, one shadow,
  one radius. `band_bg`'s `gradient` is a single two-stop the operator cannot
  influence. So *"an organization cannot currently look unlike another
  organization. It can be a different colour."*

  What their `custom.css` had to add, as a specification of what is missing:
  multi-stop gradients with an angle; radial "bloom" layers (two or three soft
  off-centre radials are the whole of the aero look and `linear-gradient` cannot
  do them); a gloss sweep on a band; a card *material* — gradient fill, inset
  top highlight, coloured border, coloured shadow — versus today's single
  `--card`; and a decorative background layer that is art rather than a photo.

  ## Why this is one bird with two stones

  **[builder-roadmap](/concepts/sections/builder-roadmap.md) already carries
  `🔨 presets — Clean / Soft-neumorphic / Bold-maximal`**, started and never
  finished, and `theme.preset` is already an integer axis. The report's own
  preferred option is that a preset become *"a named, authored bundle — palette
  + material + gradient recipes + background art"*, at which point "Frutiger
  Aero", "Civic", "Zine" and "Print" are things a designer writes once and every
  organization picks.

  That is the same argument the block palette already won:
  `builder-roadmap`'s opening note is that the palette comes from the glyph
  declarations rather than a hand-maintained list, so *uniqueness lives in the
  palette, not in per-site code*. Twenty more scalar axes (their option 1) ends
  as twenty knobs that still only make one look — which is exactly what the
  sixteen we have already did.

  It also touches [Q58/Q59](/developer_questions.md): a preset that carries
  *background art* is an authored asset bundle, which is the same "declared
  thing that ships beside the database" shape the registries need. If presets
  are built as files, the registries get their loader for free, and vice versa.

  ## The reading worth acting on first

  The report's closing observation is the cheapest thing here and the most
  useful: *"When a client's `custom.css` grows past about twenty lines it is a
  reading on the theme system, not on the client."* Three clients now have one.
  Their contents are a specification, and the first move is to read all three
  rather than to design from this list alone.

- **Q60 — a `role` glyph: a work history is not a job posting.** (Opened
  2026-09-02, portfolio report A3.) **Lean: build it, and before `project`.**

  `job` has one date, `deadline`, and `job_grid` renders `Closes <date>`. A held
  role has a **start and an end** and is not an opening anyone can apply to;
  `job_grid`'s whole frame — availability, deadline sort, `date:future` meaning
  still open — is about vacancies.

  **What makes this urgent rather than cosmetic** is where the cost landed. With
  no glyph, six roles became six `narrative` blocks, and the résumé builder now
  parses them back out of block prose with a line-shape convention:

      <role title>
      <org> - <dates>
      (blank)
      - bullet

  In the report's own words, *"a parser over presentation — the exact inversion
  of 'the model is the source of truth', in a repo whose first ground rule is
  that concepts come first. It works because one agent writes and reads it, and
  it breaks the day a person edits a block in the Builder and puts the org on
  line one."*

  That is the strongest argument in either document, and it is an argument this
  project has made to itself before: an `event` has `start_time` and `end_time`
  as fields rather than as a convention inside `summary`, for exactly this
  reason.

  **`role` is not a portfolio glyph**, which is what makes it eligible. A board
  roster, a staff page and a term of office want the same shape — and the civic
  set already ships `term` ("Term of office"), which suggests the concept is
  half-present under another name. The first design question is therefore
  whether `role` and `term` are one glyph; the second is whether `bullets` is a
  repeated field or a newline-separated one (`narrative` now keeps line breaks,
  so the cheap answer works).

- **Q61 — a `project` glyph, or is `organization` close enough?** (Opened
  2026-09-02, portfolio report A2.) **Lean: yes, but after `role`, and the
  interesting part is not the fields.**

  A portfolio project was modelled as an `organization` rune queried by a
  `directory` block. It nearly fits — `display_name`/`abbreviation`/`bio`/
  `avatar`/`url` carry title/subtitle/description/thumbnail/live-link — and it
  fails in four places, of which only the last is structural:

  1. the thumbnail renders as a round portrait (two lines of `custom.css`);
  2. cards centre their text, right for a staff list, wrong for prose (ditto);
  3. a project has two URLs — live and source — and `organization` has one, so
     the repo link went into the prose;
  4. **every GUI label lies.** The author edits a portfolio in a form headed
     *Organization*, with *Abbreviation*, *Logo / photo* and *Kind*.

  The report's sharpest observation is about the consent gate, and it is worth
  quoting because it is a diagnostic technique rather than a complaint:

  > `directory` publishes nothing without `clearance:public` — exactly right for
  > the 84-contact database it was built for, and pure ceremony for a rune
  > describing a Blender project. **That is not an argument against the gate.**
  > It is a clean signal that the block is being borrowed: a privacy control
  > that is meaningless for the data it is guarding means the data is not what
  > the block is for.

  So: a meaningless clearance check is evidence of a missing glyph. Worth
  keeping as a test to apply elsewhere.

  This sits behind `role` on the report's own ordering — *"`role` removes a
  parser that will break; `project` removes labels that are merely wrong"* —
  and behind [Q59](/developer_questions.md), because if organizations can
  register their own types then `project` may be the first thing that proves it
  rather than the next thing we hardcode.

- **Q62 — clearance annotates runes, but a résumé is a third destination.**
  (Opened 2026-09-02, portfolio report A4. Offered as an observation, not a
  request.) **Lean: no change yet; record the shape and wait for the second
  instance.**

  A mailing address belongs on a résumé handed to an employer and on no web page
  ever. The only field guaranteed never to reach a render is `notes`, so it
  lives there — three labelled lines in one free-text field, parsed back out.

  The report is explicit that the privacy answer is right and they would not
  change it: `notes` is enforced at the seam rather than by convention, which is
  the pillar working. What the episode shows is that `clearance:public` /
  `clearance:contact` annotate **runes** while the destinations are binary —
  published or private. A résumé is a third destination. So is a printed
  directory, a grant report, and a roster handed to a partner organization.

  This is the same argument one dimension over from the one
  [security](/concepts/platform/security.md) already makes about clearance being
  *annotations, not a rank*. The honest position today is that we have one
  instance and a hypothesis; a second real destination is what should move it.

- **Q58 — a registry for custom ELEMENTS: can an agent build a page component
  and register it with Hormiga, the way a host registers a widget with Void
  Maiz?** (opened 2026-09-02 by the author, sharpened by them the same day.)
  **Lean: yes. The declaration and the placement are nearly free; the script is
  the whole design problem, and the answer to it is the one `custom.css` just
  took.**

  The author's requirements, which are the specification:

  > 1. it has an appearence in the document builder (doesn't need to be
  >    accurate, just like at least a square i could theoretically position
  >    elsewhere, or even reuse or something)
  > 2. it still has tags, is within the void core framework, etc
  > 3. if it needs to access the database and display something accordingly,
  >    then that should also be allowed as well, but within a framework that's
  >    consistent

  And, on the output side: *"it'd also be nice if the representation it had in
  the document builder was close to what it is in the final website. Also it
  would be nice if it could translate to an email thing too, or at least be
  'skipped over' when generating an email."*

  ## What is already there, and it is most of (1) and (2)

  **A glyph declaration is DATA.** It is a JSON blob naming fields, editors,
  labels, a colour and a face size, and `register_block_glyphs()` is a function
  that reads a pile of them. Everything the author lists under (1) and (2) —
  placeable on the grid, positionable, spannable, taggable, movable between
  pages, editable in the inspector, undoable, in the `.miga`, replayable from a
  transcript — falls out of *being a rune with a declared glyph*, and needs no
  new mechanism at all. The canvas draws from the declaration too, so "at least
  a square I could position" is already what an unrecognised glyph would get.

  So the first rung is small and worth building on its own: **let a glyph
  declaration come from a file beside the database instead of only from a
  `.hpp`.** That alone gives a custom element a name, fields, an inspector, a
  place on the grid, tags, and a square on the canvas.

  ## What is genuinely new, hardest last

  1. **A render per (glyph × domain).** Renderer packs are per (glyph, domain)
     by design ([blocks & domains](/concepts/sections/blocks-and-domains.md)), so a
     registered element needs at least a web rendering. The author is right that
     email should be optional — and **"skipped over" has to be a declared
     property, not an accident.** A newsletter that silently omits a section is
     exactly the class of failure this project watches for; the declaration
     should say `email: skip` or `email: <fallback>` and the renderer should say
     which it did.
  2. **A canvas preview that resembles the output.** The honest options are a
     labelled placeholder, a thumbnail rendered by the live preview server, or
     an embedded browser view — and only the first is cheap. This is the same
     wall item 5 of the 2026-09-02 feedback runs into (canvas text wrap), and
     the two should be answered together.
  3. **The script, which decides everything else.** A registered element that
     ships JavaScript is **arbitrary code on a public page**, and every refusal
     in this codebase points the other way: `video` refuses embed markup, the
     render seam validates colours rather than escaping them, `custom.css` is a
     *file* and not a field precisely so it cannot carry script, `push-store`
     refuses a path. A registry that accepts JS *through the model* walks all of
     that back through one door — and unlike a stylesheet, model data arrives by
     import and by sync, from a device somebody else was using.

     **The shape that survives is `custom.css`'s.** The element's code is a file
     beside the database — a manifest plus a script the operator put there with
     their own hands. What lives in the *model* is the rune: which element,
     which field values, where on the grid. A `.miga` that carries element runes
     whose code the recipient does not have should say so loudly and render a
     placeholder, never silently fetch or execute anything.

  ## (3), which is the requirement with a trap in it

  *"if it needs to access the database and display something accordingly"* —
  and the framework that has to be consistent is one that already exists:
  **a query-backed block does not read the database, it is HANDED a result at
  render time.** `event_grid` declares a `query`; the renderer evaluates it,
  applies the language filter and the clearance seam, and passes the survivors
  in. That is the consistency the author is asking for, and it is also the only
  version that is safe: a component that could query from the browser would be a
  hole in the one rule this project treats as non-negotiable, because the
  clearance seam is enforced at render and cannot be enforced in a visitor's
  browser.

  So: a custom element declares a query like any other block, and receives its
  rows as data baked into the page. It never gets a live connection, and it
  never gets rows that did not clear the seam.

  ## Should this be built first?

  **The test is the one the Civic Record answered for `policy`: does a second,
  unrelated caller want it?** The 2026-09-02 field report's A4 asked for a gated
  block and a little per-visitor state as *built-in* blocks, which is evidence
  that the registry may be the wrong first move — build those two, see what the
  third request looks like, and let the registry be the thing that stops us
  building a fourth. The author's own instinct points the same way: *"of course,
  things officially supported by hormiga are most likely to work well."*

- **Q59 — a registry for custom DATA TYPES: should an organization be able to
  declare its own glyph, or does Hormiga ship every type it will ever have?**
  (opened 2026-09-02 by the author, alongside Q58.) **Lean: yes, and this is the
  more interesting half — but it is a smaller build than Q58 and should probably
  come first.**

  > **UNBLOCKED 2026-09-04.** The thing that actually stood in the way is built:
  > Void Core 0.2.14 put glyph declarations in the state document
  > (`state.glyphs`, `glyph declare`), so a declared type **travels inside the
  > `.miga`** and a rune can no longer arrive somewhere without the descriptor
  > explaining it. Before that, descriptors lived on `VC_Manager` and a bundle
  > carried runes without their meaning, which made "an organization declares
  > its own type" a promise the format could not keep. `tests/spine_smoke.cpp`
  > pins the property — a core that registers nothing reads a declared glyph's
  > fields — and `hormiga::declare_glyph` is the one door.
  >
  > What remains is Hormiga's, and it is the part the author decides: the
  > generic `record_grid` renderer, the declaration UI, and how much of the
  > three-way split from [Q64](/developer_questions.md) to expose (a declared
  > *entity* is the safe one; a declared *act* is where the interesting and
  > dangerous parts live). No longer waiting on anybody.

  The author's argument, and it is a good one:

  > something like "audio file" is a bit specific. Yes, plenty of things might
  > eventually need "audio file" but some may not need it. Something even more
  > specific might be a "3d object". Like do we really think that a 3D object
  > type is as needed as a contact, event, or image? Contacts, events, and
  > images are super universal. But 3D object is a bit more specific. Heck, what
  > if its a unity game showcase, and the data type is webgl Unity games? that's
  > super specific!

  This reframes [Q54](/developer_questions.md) (the audio block, from the Click
  LaFont report) from *"should we add audio?"* to *"is adding a glyph per medium
  the right shape at all?"* — and the answer to the second question changes what
  we do about the first. `release`, in Q54, was already flagged as the first
  crack in a five-glyph model chosen for outreach organizations.

  ## Why this is a smaller build than Q58

  Q58's hard part is *arbitrary code on a public page*. A data type has no such
  problem. A glyph declaration is already data — a JSON blob of fields, editors
  and labels — and everything a custom type needs (create, edit, tag, query,
  link, merge, `.miga`, replay, undo) is Void Core behaviour that does not know
  or care which glyphs exist. **Loading a declaration from a file beside the
  database is the whole first rung**, and it is the same rung Q58 needs.

  The open questions are the ones about *what happens at the edges*:

  - **Rendering.** A custom type with no renderer is a rune that cannot appear
    on a page. Same answer as Q58: declared per domain, with an explicit skip.
    A type that only ever appears in the Data section is a legitimate thing to
    declare and needs no renderer at all.
  - **A database that outlives its declaration.** Open a `.miga` on a machine
    without the type file and the runes are still there — Void Core stores
    content regardless. They must survive untouched, be visible as "a rune of a
    type this install does not know", and above all **not be silently dropped by
    a merge or a save.** This is the failure mode that matters and it is worth
    testing before anything is shipped.
  - **Namespacing.** Two organizations both declaring `release` with different
    fields, meeting in a merge, is the collision to design against now rather
    than later.
  - **The relationship to what ships.** The author: *"eventually we will want to
    support as many different data types and modalities as possible."* So a
    registry is not a substitute for good built-ins — it is what lets a real use
    exist before we know whether it generalises. A custom type that three
    organizations independently declare is the evidence that it should become a
    built-in, which is a better promotion path than guessing.


  **2026-09-03 — the portfolio agent asked the same question independently, and
  narrowed it usefully.** Their A10 is this question arriving from a client
  rather than from the author, which is the strongest evidence a question can
  get. They tabulate four separate gaps they had already filed — a `project`
  glyph, a `role` glyph, `bio_es` on a data rune, `hero.portrait` — and observe
  that all four are one shape:

  > **the glyph set is fixed at compile time, and an organization that is not
  > shaped like LON has to borrow.** Borrowing works … but each borrow costs a
  > lie in the GUI, a workaround in CSS, or a parser over presentation.

  Three things they contribute that this entry did not have:

  1. **A concrete syntax**, which makes the scope visible — `glyph new project`
     with `field title_en title_es text`, `field thumbnail image`, `category
     Content`. A *declared record type*: typed fields, GUI labels, `--describe`
     introspection, defined in the document rather than in C++.
  2. **A rendering answer that does not open [Q58](/developer_questions.md).**
     A generic `record_grid` over a query, plus a `record_detail`, would render
     project cards, board members, publications, courses and equipment
     inventories *without a renderer per type*. That is the observation that
     unblocks this: the hard half of Q58 is that a block needs a renderer in two
     output domains, and a generic grid needs one renderer total.
  3. **The counter-argument, stated by them rather than by us**, and it is the
     one that matters:

     > a registry is how a focused tool becomes a generic database with a worse
     > UI. `directory`'s clearance gate is only trustworthy *because* it knows
     > it is publishing people; a generic `record_grid` over user-declared types
     > cannot make that promise. Whatever the answer is, **the privacy seam must
     > not become configurable** — that pillar is the reason a real
     > organization's data is safe in this thing, and it is worth more than a
     > `project` glyph.

  So the design constraint is now explicit and it is a hard one: **a declared
  glyph may not be publishable through a block that makes a consent promise.**
  `directory` stays for `contact`/`organization` and keeps its gate; a
  `record_grid` over a declared type publishes what the author placed, the way
  `download` does, and naming it is the consent. Any answer that lets a declared
  type flow through `directory` is the wrong answer.

  **2026-09-03 — this question now has a dependency and a better
  decomposition.** Two things moved underneath it the same day:

  1. **It is blocked on Void Core.** Glyph descriptors live on `VC_Manager`, not
     in the state document, so a `.miga` carries runes without their meaning —
     a declared type would not survive being handed to anybody. Asked upstream
     (`MESSAGE_FOR_VOIDCORE_hormiga-rune-kinds-and-the-glyph-split-2026-09-03.md`).
  2. **[Q64](/developer_questions.md) splits it into three cleaner questions.**
     If a descriptor gains a `kind`, then "declare a type" means three different
     things: a declared **attribute** is nearly free (a named dimension), a
     declared **entity** is the case discussed here, and a declared **action** is
     where both the interesting and the dangerous parts live. Answering per kind
     is likely better than answering this as one question.

  Also settled on 2026-09-03, and it strengthens the lean rather than changing
  it: a hand-written renderer per type is a **stopgap for a generative one**, so
  adding `project` with bespoke render cases deepens the stopgap while a generic
  `record_grid` is a rendering *derived from the declaration* — the right shape,
  with a dumb deriver that can be swapped later. See the log.

  **What they are actually asking**, which is a decision only the author can
  make and is the reason this stays open:

  > is a declared data glyph — typed fields, GUI labels, `--describe`, no custom
  > renderer — a thing Void Hormiga wants to have, or is the right answer that
  > the glyph set grows by hand and `project` and `role` simply get added to it?

  Either answer closes [Q60](/developer_questions.md) and
  [Q61](/developer_questions.md) for them; they want to know which, because the
  workarounds they maintain are written differently depending on whether they
  are temporary. **That is a fair thing to be told and it has now been asked
  twice.**

- **Q54 — can Hormiga play a sound? RUNG 1 BUILT 2026-09-02; rungs 2-3 open.**
  (Opened 2026-09-02, from the Click LaFont report A1.) **Lean: yes, and in
  three steps, smallest first.**

  **Rung 1 shipped the same day.** An `audio` block: a file the organization
  owns, staged into `site/assets/`, played by a native `<audio controls
  preload="none">` with no facade — nothing leaves this site, so the standard
  keeps the promise our JavaScript keeps for `video`. Card and Listen button in
  the newsletter. It is placeable from the Builder palette, not just by an
  agent. See `render/audio.hpp`.

  **Rungs 2 and 3 are still open, and rung 2 is now entangled with
  [Q59](/developer_questions.md)** — which is the better question. What follows
  was written before Q59 existed and the middle of it is what Q59 reframes.

  `--describe` lists 69 glyphs and none of them is audio. `video` takes YouTube
  and Vimeo and correctly refuses everything else. So a music artist's website
  cannot play the artist's music: two album covers, links to somebody else's
  player, and forty-four minutes of masters sitting in the folder beside the
  database.

  **This is not a niche of one**, which is the whole reason it is a question
  rather than a client accommodation. An organization with a podcast, a recorded
  meeting, or a Spanish-language radio spot has exactly this absence — and for
  that organization the recorded meeting is often the most accessible thing on
  the site, because it does not require reading.

  1. **An `audio` block.** File, title, duration, optional cover. The same
     posture as `video`: no autoplay, no third-party fetch, a poster card until
     somebody presses play. A native `<audio>` element behind that card needs no
     library and no script we do not already ship. This alone is the difference
     between *cannot* and *can*, and it is the smallest thing on this page.
  2. **A `release` glyph and a `track_list` block.** The report's argument is
     the right one: a release is a first-class thing in this domain the way an
     `event` is in an outreach organization's — title, year, cover, an ordered
     catalogue of tracks, links per service — and the `detail`/`limit`/`sort`
     machinery on `event_grid` carries straight over, as do `title_en` and
     `summary_en`. Today a release is an `image_grid` with `limit 1`
     impersonating a card.

     The open part is whether `release` belongs in the *core* glyph set or in a
     domain pack. Hormiga's five kinds of thing were chosen for outreach
     organizations; a sixth that only a music project uses is the first crack in
     that. The honest test is the one the Civic Record answered for `policy`:
     does a second, unrelated caller want it? A church with a sermon archive and
     a school with a recital series both do, which suggests the general glyph is
     not `release` but *an ordered collection of media with a date* — and that
     naming it `release` would be modelling one client's word.
  3. **A player that survives a page change.** Later, and probably a real
     conversation about whether a static output can host one at all. Deferred
     explicitly rather than silently: it is the one of the three that changes
     what a Hormiga site *is*.

- **Q55 — how does media reach the object store?** (opened 2026-09-02, from the
  Click LaFont report A2.) **Lean: a third `what`, named `assets`, and a
  `base_url` on `hol_object_store`.**

  `effect push-store` takes `index` or `backup` and explicitly not a path. The
  comment at the top of `src/publish/push.cpp` gives the reason and the reason
  is right: an effect that uploads an arbitrary path is a way to send the
  unencrypted database, the vault or a token file to a bucket in one command.
  The report agrees with the reasoning and points at the consequence — there is
  no way to get media into object storage **at all**, and media is exactly what
  should not ride in a static-site deploy. Forty minutes of audio is ~40 MB
  re-hashed and re-uploaded on every publish of a site whose text changes
  weekly.

  The proposed shape keeps the safety argument entirely intact: a third kind,
  `assets`, that pushes `assets/` and nothing else. Still not a path; the caller
  still cannot name a file; and it is the one folder Hormiga already designates
  as irreplaceable originals. The other half is a `base_url` on
  `hol_object_store` so `stage_site_asset` can emit a store URL instead of
  copying the file into `site/`.

  What is genuinely open is the second half, not the first. `stage_site_asset`
  is the choke point every image passes through and it is also where the
  downscaled gallery derivative is made; a version of it that sometimes returns
  a remote URL has to decide what a *thumbnail* of a remote file is, whether a
  site built on a machine that has never seen the bucket is still valid, and
  what happens to `.miga` portability when a page points at a bucket the
  recipient cannot read. **Every cloud host is disposable** is the acceptance
  test, and an asset URL is the first thing on the page that would not survive
  walking away.

- **Q56 — can a rendered page do anything?** (opened 2026-09-02, from the Click
  LaFont report A4.) **Lean: two primitives, and explicitly not the third.**

  What Hormiga renders is static HTML plus a fixed `app.js` — lightbox, filter
  box, scroll-reveal, map, calendar. There is no seam for a page that *does*
  something. The brief that surfaced it was a site that should be "FUN and
  INTERACTIVE" and eventually ARG-shaped, but both asks generalise well past
  that:

  - **A gated block** — content that renders only after a visitor supplies a
    passphrase. Client-side, no server, no account. For an ARG it is a door; for
    an organization it is a members-only page; for a newsletter archive it is an
    unlisted issue. It **must** say plainly in its own documentation that it is
    a doorknob and not access control, because somebody will otherwise put a
    member list behind it — and this project's answer to "who may see this" is
    the clearance seam, which is enforced at render and cannot be enforced in a
    browser.
  - **A little per-visitor state.** "You have found 3 of 7." A visitor who has
    done something being shown something different is the minimum an ARG needs,
    and it is also "you have read this update" everywhere else.

  **Not asked for, and flagged so nobody builds it on the way: a solver
  counter.** A count of who got through is analytics — a different product, a
  privacy question this project has not asked, and one our posture would make
  expensive. The report flagged it before we could.

  One primitive already exists and is doing this work: `page.in_nav 0`. A live
  site has an unlisted page that renders, is in the sitemap, and is linked from
  exactly one text link. That single field establishes the contract the rest
  would rest on — *this site has more in it than the menu admits* — and it cost
  nothing.

- **Q57 — should an organization's data mantle be permanently called
  `demo-org`?** (opened 2026-09-02, from the Click LaFont report D6.)
  **Lean: rename the default, do not make it configurable.**

  `kDataMantle` is `"demo-org"`, hardcoded, and every block query, `effect
  query` and the published index read that mantle and no other. The silent
  failure that follows is fixed — a render now says when data-shaped runes live
  in a mantle nothing reads, and names the one command that fixes it. What is
  left is the smaller, real observation the report makes in a parenthesis: *an
  organization's own data namespace being permanently called `demo-org` reads
  strangely in a `mantles` listing on a real client.*

  Making it configurable is the expensive version and it buys a new failure
  mode: a database whose data mantle is named in config, and config that has
  been lost or copied from another install, renders empty for a reason that is
  harder to diagnose than the one we just fixed. Renaming the default — to
  `org`, say — costs a migration for every existing database and a decision
  about what an unmigrated one does. Both are the author's call; neither is
  urgent now that the failure is loud.

- **Q47 — the advisory engine: how does Hormiga help with a mistake instead of
  accepting it or refusing it?** (opened 2026-09-01, from agent feedback.)
  **Lean: build it, as a third thing beside the dispatcher and Allomone.**

  The complaint, fairly put: `set` accepts any field name and reports success,
  so an agent can type `locaiton` instead of `venue`, run a whole script, see
  `ok` on every line, and discover thirty minutes later that the data is
  invisible. *"That is a data integrity crisis, not a convenience."*

  The obvious fix — reject undeclared fields — is the wrong one, and the reason
  is worth stating because it will be proposed again. **We cannot know that
  `venue` and `location` mean the same thing.** A hard block is too strict: it
  stops a person doing the thing they came to do, and it offers nothing in
  return. But the author's judgement is that a bare warning is *equally* lazy:

  > simply telling the user "that was wrong" and not offering anything is just
  > lazy and it doesn't have the user in mind at all. a user just wants to do
  > things. […] letting everything be a warning, and creating a hard stop, are
  > both equally lazy in design.

  The real answer is a system that *notices*: this database has used `venue`
  ninety times and has never seen `location`; the two are close; ask. That
  needs the graph we already have plus something language-shaped — an NLP layer
  over field names, tag vocabulary and prior usage.

  It is **not Allomone.** Allomone derives properties from rules the operator
  wrote. This observes what the database already looks like and proposes.
  Closest existing relative: `../FaultSack` (a different language, but the
  observation behaviour is the thing to study).

  Sequenced after the first release. It is the largest open design item here.

- **Q48 — numbers, axes and infinity in a graph model.** (opened 2026-09-01, by
  the author.) **Lean: wait for Void Core; do not solve it in Hormiga.**

  Everything in a Void Core document is a rune on a graph, and Hormiga stores
  numbers as strings because it barely has any — a zoom level, a span, a
  weight. That sacrifice is invisible here and is **not** affordable elsewhere:
  Void Unity needs character stats that go up and down, which means real
  numeric semantics, ordering, and something coherent to say about infinity.

  The author's note: numeric values as a concept may shift in Void Core. When
  they do, Hormiga's string-typed numbers become a migration, not a redesign —
  which is the argument for not inventing a private answer first.

- **Q49 — should a bare run refuse when there is no database?** (opened
  2026-09-01, from agent feedback.) **Lean: not yet; the note may be enough.**

  A run with no `--state` used to invent `demo-org.json` in the working
  directory in complete silence. That is the root of the worst bug this project
  has had — two copies of one database, edited for two days. **Fixed 2026-09-01
  to the extent that it now announces itself and names `--state`.**

  What remains open is whether it should refuse outright. The argument for: a
  missing database is the bedrock, and a wrong one poisons everything after it.
  The argument against: a genuine first run has to start somewhere, and every
  "cd somewhere new and begin" flow breaks. If it does refuse, it must offer the
  alternatives in the same breath — an error with no way forward is the lazy
  half of Q47 wearing a different hat.

- **Q50 — three upstream defects in the relation verbs. ANSWERED AND SHIPPED
  2026-09-02 (Void Core 0.2.13).** Kept here rather than moved to Decided
  because of what the closing taught.

  All three are fixed. What is worth remembering is that **our severity call was
  wrong on two of them**, and the lean said *"report and wait; neither blocks
  anything."*

  1. `link` missing from the `--describe` verb list — it was **four** verbs
     (`link`, `links`, `unlink`, `journal`), including the one we had just told
     agents to use instead of `related`. Fixed, and the list is now diffed
     against the router by a CI test in both directions.
  2. `relate --relation <name>` "ignored" — it was **writing weight zero**. The
     weight is positional and there is no flag parsing, so `--relation` reached
     `atof()` and the association was written at 0.0: "not near at all", the
     inverse of the intent, reported as success. A silently inverted fact, not a
     dropped name. It refuses now. We are not exposed — nothing in this
     repository calls `relate`.
  3. `related <rune>` answering `(no neighbors)` for a rune with edges — the
     signpost shipped, worded as we suggested, firing only where the answer was
     already empty and the ref names a rune with edges. `related` still returns
     `ok: true`; nothing branching on `ok` moves.

  **The lesson to carry:** a defect that does not block us can still be writing
  wrong data, and "neither blocks anything" is a statement about our roadmap
  rather than about the defect. Reporting it as *"not a defect, a message that
  is correct and reads as false"* is what made it actionable — Void Core said so
  explicitly — but the severity should have been theirs to set, not ours.

- **Q51 — does bilingual output need its own translation engine?** (opened
  2026-09-01, by the author.) **Lean: yes, eventually, and it is not Palabra.**

  Void Palabra is the *system layer* — history, versions, merge, persistence,
  sync. The name misleads (it has misled this repository's own README), but it
  has nothing to do with language. Hormiga's bilingual requirement is real and
  currently manual: parallel `_en`/`_es` fields, authored by hand. Q5 already
  said bilingual "must be an ENGINE."

  **2026-09-02 — the first half is built, and the author sharpened the
  question.** The Click LaFont report asked for a way to publish ONE language;
  the author declined it in the terms that make this question's answer
  non-negotiable:

  > instead of opting to NOT have multi language, the ask should've been "have
  > better and more robust translation tools" … We want MORE features, not less.

  What shipped: a render reports how much of the page it just built was actually
  written in the language asked for (the fallback was silent, which is how a
  site can be 0% translated and report `ok`), and `effect translation-report
  [lang]` writes a **replayable script** — every gap as a `set <rune>
  <field>_es '<the English text>'` line with the source already in place, under
  the `use <mantle>` that makes it apply. Editing it and replaying it with
  `--script --atomic --actor` makes translating a logged, attributed batch like
  every other change. See `src/app/translate.cpp`.

  What is still open is the ENGINE half, and it is now a narrower question:
  given a file whose right-hand sides need filling, what fills them? A model
  holiday (the reserved `model` node) is the obvious answer and brings its own
  question — a machine translation of an outreach organization's own words,
  published under its name, is a thing a person should approve rather than a
  thing a pipeline does. The script shape was chosen partly for that: it puts a
  human between the proposal and the dispatcher by construction.

- **Q52 — `deploy_cmd` is readable in the state document.** (opened 2026-09-01,
  from agent feedback.) **Lean: accept, and say so.**

  Tokens go to the vault and never reach `argv`. But the *command* is a field:
  anyone reading a stolen `.state.json` learns which provider and CLI version an
  organization uses. That is reconnaissance, not a credential. Worth writing
  down as accepted rather than leaving it to be rediscovered as a finding.

- **Q53 — is "distrustful of its operator" a stance we keep?** (opened
  2026-09-01, from agent feedback.) **Lean: keep it, and say it out loud.**

  The feedback: the tool is *"defensive, noisy, and distrustful of its own
  operators,"* and a reader would verify every `set` with a `get`. Offered as a
  design smell; the author's answer is that it is the design:

  > we cannot assume the intelligence of an ai agent. […] i am trying to design
  > this for GPT 3 and the latest Fable 5. […] the user might not know
  > themselves.

  An operator may be a state-of-the-art model, a weak self-hosted one, or a
  volunteer, and the database holds real people's information. The cost is
  tokens spent on verification; the trade is deliberate. It belongs in a
  concept page so a future contributor does not "fix" it.

- **Q43 — does a headless Hormiga run in the cloud, holding the `.miga`?**
  (opened 2026-08-21.) **Lean: yes eventually, no for H1–H3, and it is the gate
  on posts.** Full argument in
  [web platform](/concepts/platform/web-platform.md) §7.4.

  The tension is structural, not incidental. Ground rule 3 says the dispatcher
  is the only door; that dispatcher runs on a volunteer's desktop when they
  open the app; and a live website with member posts needs writes to appear
  without a person at a keyboard.

  The good news is that the architecture already permits the answer:
  `voidhormiga-cli` is a real headless front-end over the *same* dispatcher,
  with the same glyphs, the same effect gate and the same journal. A Hormiga
  draining the inbox on a schedule in the cloud is **not a second door — it is
  the same door running somewhere else.** Nothing new has to be built to make it
  possible.

  **What has to be decided is custody.** It would put the organization's whole
  database — 146 contacts, ~70 emails, ~66 phone numbers, and the vault's
  secrets — on a server, where today it is one file on one machine and the file
  is the authority. For an org serving communities where a leaked contact list
  has consequences that are not financial, that is a real change of posture and
  it is the author's to make, not the developer's.

  **Sub-questions, in the order they bite:**
  1. If a cloud Hormiga holds the `.miga`, does it hold the **whole** one, or a
     publish-scoped subset with the internal fields never present? The second is
     much safer and is probably possible, since §7.2's seam already computes
     exactly that subset — but a subset cannot dispatch a transcript that edits
     a rune it cannot see.
  2. Does the **vault** travel with it? Secrets ride the `.miga` by design
     (that is what `token_key` is for), so a cloud copy carries the org's deploy
     tokens unless it is stripped.
  3. Is there an intermediate: the cloud holds **only the inbox**, and a
     scheduled *desktop* Hormiga drains it — accepting that posts appear
     whenever the volunteer's machine is on? That keeps custody unchanged and
     costs latency, and may be the right first answer.

  **Not blocking H1, H2 or H3**, which is the reason the phasing is shaped as it
  is: the read-only projection, sign-in, and a human-drained inbox all work with
  custody exactly as it is today.

- **Q44 — all-AWS, or AWS for accounts and Cloudflare for the site?**
  (opened 2026-08-21.) **Lean: keep Cloudflare Pages for the site, add AWS for
  the parts Cloudflare does not do well.**

  The author already pays Cloudflare, the site deploys there today, the native
  Direct Upload path works, and `render-site` produces a folder that any CDN can
  serve — so moving it to S3+CloudFront buys **consolidation and nothing else**,
  at the cost of re-solving a problem that is currently solved.

  The counter-argument is real and is about the volunteer, not the developer:
  two vendors means two dashboards, two bills and two places to look when
  something breaks. If the org ever has to debug this without an agent present,
  one vendor is worth paying for.

  The Antfarm makes this genuinely reversible — `hol_static_host` is a node, and
  switching is a re-deploy — so this is the cheapest decision on the page and
  should not be agonized over. Deciding it *late*, once H1 has shown how much
  AWS surface is actually involved, is legitimate.

- **Q42 — does the map become Void GIS, and when?** (opened 2026-08-21.)
  **Lean: not yet, and not for the reason it was proposed — but decide the
  destination now, because it changes how the next six months of map work is
  written.** Full argument in
  [application boundaries](/concepts/foundation/application-boundaries.md).

  The short form, because the measurement reorders the question:

  **The map is four layers, and only one of them is what the author is unhappy
  about.** The complaint is *"it is easier to drop a couple of markers in Google
  Maps and take a screenshot than to use my own database"* — that is markers,
  gestures and affordances, i.e. the **view**. Void Maiz ruled (2026-07-18) that
  custom views are **host-built**, so the view stays in Hormiga under every
  possible separation. **Separating the map cannot fix the reason to separate
  the map.** That is the single most important sentence here.

  **What IS separable is the engine**, and it is separable cheaply because the
  map is already generic — measured: `ui/map.cpp` draws anything carrying a
  `geo` field, has its own glyphs (`map`, `mapshape`, `refpoint`), special-cases
  no Hormiga domain glyph, and couples only to host plumbing. So there is **no
  urgency premium**: the seam is as cheap to cut later as now.

  **The one thing worth doing immediately** is a discipline, not a refactor:
  write new map work against the seam (scene in, commands out) rather than
  against `HormigaApp`, which carries 18 `map_*` members today. Every control
  added against the struct is a line item in a future extraction; against the
  seam it is not.

  **Three sub-questions for the author:**
  1. **Is there a second user?** Void GIS earns its own repo when something
     other than Hormiga needs it. The **map builder for non-Earth maps** is the
     most plausible candidate and may genuinely be one — it is a different
     product with different users, not a feature of a newsletter tool. If that
     is a real intention rather than a someday, it changes the answer.
  2. **Is a second application acceptable to the volunteer?** Row 3 of the
     boundaries table is paid by the least technical person in the chain — a
     second window, a second install, a second thing that can be out of date.
     Hormiga's whole premise is that one person at one keyboard can run an
     organization's data.
  3. **Do we spend the next map sitting on UX or on structure?** The lean says
     UX, in place, built against the seam. Structure-first would mean a better
     factored map that is still worse than Google Maps at dropping a pin.

  **ANSWERED 2026-08-21.** The author chose **structure first**, and confirmed
  the **non-Earth map builder is a real intention**. The second answer is the
  one that moved things: a real second client makes the engine worth *designing*
  for rather than merely extracting.

  Built the same sitting: **`src/gis/`** — `geo.hpp`, `projection.hpp`,
  `source.hpp` — depending on nothing but the standard library, with
  `check_layering.py` enforcing `ALLOWED['gis'] = {'gis'}` so it stays liftable.
  `BaseSource`'s six hidden assumptions (web-mercator, z/x/y, 256px tiles,
  zoom 0..19, longitude wrap, haversine metres) are now fields on
  `gis::MapSource`, defaulted to reproduce Earth exactly and overridable for an
  authored world. Golden render byte-identical; `tests/gis_smoke.cpp` (44
  assertions, ctest #31) drives the `Flat`/non-wrapping paths that no caller in
  the tree reaches yet, because the golden render cannot see them.

  **Still open, and now the interesting part:** whether Void GIS becomes a repo.
  The folder is the hypothesis. It lifts out with its tests if the map builder
  wants it; nothing is spent if it does not.

  **Not bundled with this:** the Antfarm. The author raised it in the same
  breath and it is a different shape — a configuration surface for a Void Core
  concept, whose natural gravity is toward Palabra at their Phase 4, not toward
  a sibling application. Pairing the two decisions would get both wrong.

- **Q40 — do we adopt Void Palabra's `archive` for document history?**
  (opened 2026-08-20, from the field agent's publish report.) **Lean: yes, but as
  its own piece of work, not folded into publishing.**

  The publish work of 2026-08-20 answered *"which version of the site is live,
  and put the old one back"* with `deployment` runes. It deliberately did not
  answer the other question the operator asked — *"what did this say in June,
  and can I undo a bad edit"* — because they are two different histories:

  | | of what | answers |
  |---|---|---|
  | `deployment` runes (built) | the **publishes** | what is live, what was live, restore that |
  | Palabra `archive` (this question) | the **document** | what did it say then, undo an edit |

  The case for `archive` is strong and is not mine: it is `status:current` with
  `resource: src/archive.cpp`, it compiles and passes its suites, and its own
  concept page names **Hormiga's save system as its forcing client** — it was
  written for this. The numbers fit our shape exactly: `org_01.miga` is 87.6 MB
  of which the state document is **388 KB** and ~99% is base64 assets that do
  not change between saves. Palabra measures 100 saves plus a 400 KB asset at
  **560 KB** against ~54 MB of `.miga`-per-version. Content addressing is the
  right structure for a 99%-unchanged shape; a bundle per version is the wrong
  one. And it is **a linear list on purpose** — `archive.md` argues that on one
  device with one person at one keyboard the history genuinely *is* a sequence —
  so adopting it does not mean adopting a distributed-systems problem.

  **Why it is a separate decision rather than a detail.** It changes how the
  `.miga` is written and adds a vendored dependency to the save path, which is
  the one path where a bug loses somebody's data. It is not a thing to smuggle
  in behind a green button in the same session.

  **MEASURED 2026-08-21, and the lean is unchanged but now has a precondition.**
  Palabra was built and its `Archive` run against a real Hormiga state document
  (`MESSAGE_FOR_VOIDPALABRA_hormiga-archive-readiness-2026-08-21.md`). Its 7
  suites pass; it links alongside Void Maiz with no cJSON collision; the numbers
  hold at our shape — **a 456 KB document, twenty edits, every version kept:
  464 KB, against 9.6 MB for a `.miga` per version.** Per-edit cost is
  rune-sized (~1.1–1.7 KB), a 400 KB asset edited by one byte costs +64 KB, and
  `import_miga` already reads our v3 bundle including assets.

  **One thing blocks adoption, and it is not a nitpick.** `Archive::save` stores
  the `mantles` slice and discards the rest of the document. A save/load round
  trip returns 1 of our 8 top-level keys: `config` (**`site.base_url`**,
  `theme.accent`, the actor, view state), `scripts` (the org's Allomone rules),
  `domains`, `bindings` and `active` are all lost. Worse, because a version is
  named from `mantles` alone and the dedup guard compares that name, a
  **config-only change produces no save at all** — a volunteer changes the
  site's address, presses Save, gets no error, and the change is gone.

  Palabra's exclusion is well argued *for sync* (a domain carries real deploy
  commands; syncing one would run one device's deploy on another) and the ask
  back is to separate the two jobs: name by `mantles`, store the whole document.
  Until that is settled this stays unadopted — **not** because the rest is
  unproven, but because the save path is the one place a silent loss is
  unacceptable.

  **RESOLVED 2026-08-21, same day.** Palabra fixed it, taking the split we
  proposed — name from `mantles`, store the whole document, dedup on stored
  content. Re-verified independently: all eight top-level keys survive, a
  config-only change saves and loads back, storage numbers unchanged. Their fix
  surfaced a second bug (`load_latest()` resolved by version name, so once two
  saves could share a name it returned the *older* config — the same data loss
  one layer down); also fixed, and both are pinned by their regression tests.
  The rule is now normative in their SPEC §7 rather than living in one
  implementation's habits.

  So: **the answer to "is Palabra ready?" is now simply yes.** What remains is
  the author's decision about when, not a readiness question.

  **Two things settled while asking, both of which reduce our work:**
  - **Encryption is Palabra's, not ours.** Their container will carry it at
    Phase 4; keys stay ours (we supply key material, they encrypt). So we do
    **not** build whole-bundle encryption, and we **keep the vault** — it is
    doing the right job. Folded into [miga-format](/concepts/platform/miga-format.md).
  - **Unicode: normalize at the boundary**, now normative in their SPEC §6.1 at
    our proposal — they have a hash function, we have the keyboard. Measured
    here: the live database is **19,858 strings, zero decomposed**, and Hormiga
    passes text through byte-transparently (it neither corrupts nor normalizes).
    `tools/lint_nfc.py` plus two smoke assertions keep that true. Full
    input-boundary normalization in C++ is **not built** — see Q41.

  **Palabra's own advice on sequencing, which we should take:** adopt
  `save`/`load` first and leave the multi-user path alone until their Phase 4
  exists. Their sync story still has one large unmeasured claim in it (that the
  protocol survives arbitrary *message* order, as distinct from arbitrary *merge*
  order, which is what their join suite actually proves). That is an unusually
  honest thing for a library author to volunteer and it should be believed.

  **ANSWERED by the author 2026-08-27: yes — "remember to utilize palabra".**
  Adopted the same day, and the adoption is narrower than "we now use Palabra
  for everything", which is worth writing down precisely:

  - **The MERGE is theirs and we call it.** `enrich` / `join` / `conflicts` /
    `flatten` / `version_name`, through one thin adapter (`src/sync/merge.*`)
    that contains no merge logic of its own. This is the piece that could be
    silently wrong and it is the piece we are not writing.
  - **The TRANSPORT is ours for now**, because `reconciliation` and
    `peer-and-tier` are still `status:planned` on their side and the author
    needs multi-device now. Built to the same discipline `src/platform/miga.*`
    is built to — a swappable implementation, not an interface other code binds
    to — so their Phase 4 replaces it without touching anything else.
  - **`Archive` as the document-history store is still NOT adopted**, and that
    is the part this answer deliberately leaves alone. It changes the save
    path, which is the one place a silent loss is unacceptable, and it is
    independent of merge. Their sequencing advice above still stands for it.

  Full shape in [collaboration](/concepts/platform/collaboration.md) §2–3.

- **Q41 — do we normalize text to NFC on input, and with what?** (opened
  2026-08-21.) **Lean: yes, but not until it can bite.**

  Palabra requires NFC from callers (their SPEC §6.1). We measured that our
  corpus already satisfies it — 19,858 strings, zero decomposed — and that
  Hormiga is byte-transparent: type decomposed text and decomposed text is what
  lands. So today the precondition holds by luck of who has been typing, and
  `tools/lint_nfc.py` is what notices if that changes.

  Actually normalizing needs a decision the linter does not: **where**, and
  **with what**. The dispatcher is the one door everything passes through, which
  makes it both the obvious place and the risky one; the alternatives are the
  CSV import, the GUI field editors and the command bar, which is three places
  instead of one. And the *with what* is a real dependency question — full NFC
  wants the Unicode composition tables (~30–60 KB vendored), or Win32's
  `NormalizeString`, which is correct here and is a platform call in an app that
  keeps its platform seams thin.

  **Not urgent**, and that is measured rather than assumed: it cannot bite until
  either a second device syncs (Palabra Phase 4) or a volunteer types on a
  platform that emits NFD. The linter catches the second the next time anyone
  looks. It is recorded so that the day either happens, the answer is not
  invented in a hurry.

  **Two sub-questions for the author, since the vocabulary is the part that
  leaks:**
  1. `archive.md` is explicit that a version-control vocabulary must not reach a
     newsletter tool's UI. Hormiga would say **save** and **load**, never "cut"
     or "commit". Confirm?
  2. The field agent has run `git init` in their org folder as a stopgap and says
     it should be **deleted** when `archive` lands rather than kept — because
     git versions *files* while Hormiga's unit of change is a dispatched
     command, and two histories of the same thing that disagree is worse than
     one. That reasoning looks right; it is the author's call whether an org
     folder may carry a git repo at all.

- **Q39 — which vendors, and who owns the account?** (opened 2026-08-19.) The
  design is vendor-neutral by construction — five Antfarm nodes, config not code
  — so this is a purchasing decision, not an architectural one. **Lean:
  Cloudflare Registrar for the domain (at cost, no renewal markup), Cloudflare
  Pages for the static site, Supabase for accounts and storage** (Postgres, so
  `pg_dump` is the exit; Google sign-in built in; Row Level Security is how "a
  visitor may edit their own profile and nothing else" is enforced at the
  database rather than in readable JavaScript). The question the lean does not
  answer: **whose credit card and whose email own those accounts.** For an
  outreach network that outlives its volunteers, an account in one person's name
  is a single point of failure that no amount of disposability fixes.

- **Q40 — the inbox surface: its own section, or part of Data?** (opened
  2026-08-19.) Visitor submissions need somewhere a person reviews them. **Lean:
  a section of the Data tab, not a new tab** — a submission is a proposed change
  to the org's records and belongs beside them, and a fifth top-level tab for
  something that is empty most weeks is a tab people stop opening. What it needs
  that Data does not have: the **derived** "what does this transcript actually
  do" view (§4 of the concept refuses to store a `kind`), and Approve/Reject as
  one batch. Deliberately unbuilt until a real submission exists to look at —
  the headless path works today with no new code.

- **Q41 — how does a block render against an identity?** (opened 2026-08-19.)
  A profile page shows *the signed-in visitor's own* contact; a members area
  shows different content to different people. Every block today renders against
  a **query**, which is a fact about the data. This is a fact about the
  *reader*. **Lean: it is `scry` context, not a new block kind** — Void Core's
  `Context` is `{locale, audience, date, role}` and `audience`/`role` are
  exactly this, already first-class and already the thing the render seam
  passes. The alternative (an `identity` field on a block) would put the reader
  inside the document, which is the one place a reader does not belong. Needs a
  real signed-in visitor to design against.

- **Q42 — what happens to a submission after it is decided?** (opened
  2026-08-19.) An approved submission's transcript has been dispatched; the rune
  is now a record of a decision. **Lean: keep it, forever, with `state` and
  `decided`.** It is the only evidence of *who asked for what and who agreed* —
  the command log is session-scoped and cannot answer it later. The cost is a
  mantle that grows monotonically, which is the cheap side of this trade. Open:
  whether a rejected submission's transcript is kept (it contains what a person
  wanted to write about themselves) or blanked to a note.

- **Q29 — what do we call the votable item? — ANSWERED by the author, and my
  first recommendation was wrong.** (opened + resolved 2026-08-12.) I argued for
  `measure` on the grounds that a policy is the *outcome* and the tracked thing
  is the item in flight. The author's counter is better: **a policy is defined by
  its delta** — it is proposed, changed, re-proposed, denied, and comes back
  different; it splits, it merges, its authors drop it and pick it up, its own
  name changes. Once *versions* are first-class, the thing voted on is simply **a
  revision**, and a separate `measure` glyph stops earning its place. A policy
  nobody votes on (a platform's terms of service) just has no vote edges.
  **Settled: `policy` + `revision` + `provision`, votes as edges onto a
  revision.** Reading the real Springfield document confirmed it — see
  [the civic record](/concepts/projects/civic-record.md), which also records the two
  things the document taught that neither of us predicted (a policy is a *tree*
  of numbered provisions, and **suspension** is a change that alters no text).

- **Q30 — does the dataset generator become its own project?** (opened
  2026-08-12). **Lean: yes, and only this one.** The four view sections (Data,
  Maps, Calendar, Builder) stay one application because they are one model seen
  four ways, and splitting them would mean inventing a protocol to recreate what
  a shared `Core` gives free — buying concurrent log writes and cross-process
  cache invalidation in exchange for a separation the tabs already provide. The
  generator is different in kind: it does not *view* the model, it manufactures
  it from video and PDFs, and it needs a Python ML stack that **ground rule 5
  forbids vendoring**. It joins as a `records` source holiday and shares the
  *verb vocabulary*, not a library — so it links nothing and could be rewritten
  in any language later. **Named by the author 2026-08-12: Void Reyna** — the
  queen, who does not forage or build but *founds the colony*. **Founded the same
  day** at `../VoidReyna` with its own OKF; the Hormiga-side view of it stays at
  [Void Reyna](/concepts/projects/void-reyna.md).

  **Q30b — which stack does it build on? ANSWERED: Void Core, not Void Maiz.**
  Void Core ships a real Python package (`voidcore` 0.2.6, C core as a DLL,
  **no required deps — "holidays pull their own deps when used"**), which is
  exactly Reyna's dependency model. Void Maiz is the C++ *view* library and
  Reyna is batch and headless. The one place a UI looked unavoidable dissolved:
  **the human confirmation step must be a dispatcher command**, so naming a
  voice cluster is logged and replayable — which means it happens in *Hormiga*,
  not Reyna. Reyna proposes, Hormiga confirms; the same propose-never-write shape
  as Allomone's Weaver. A Python Void Maiz may be worth building one day, but it
  should be justified by its own clients, because this one dissolved.

- **Q30a — split `app.cpp` by section? — ANSWERED by the author 2026-08-17:
  yes, considering the modularity carefully.** Done. `app.cpp` is now the
  **shell** — lifecycle, host seams, projection, persistence, the frame, 2,627
  lines — with six section units beside it (`section_data`, `section_web`,
  `section_allomone`, `section_builder`, `section_map`, `section_calendar`) and
  `app_shared.cpp` for the helpers they share.

  **The boundary is deliberately not a new abstraction.** Every section is still
  a set of `HormigaApp::` methods declared in `app.hpp`, one struct and one
  dispatcher. Inventing a "section interface" to justify the split would have
  added a seam nothing needed and made the one-sync rule harder to see, not
  easier. What moved is text; what did not move is the design.

  **Two things the split improved beyond tidiness.** Territory's mercator maths
  and the map itself had been separated by the entire calendar, for no reason
  but the order they were written in; they are one file now, and `static` says
  which helpers are private to them. And the shared header forced the honest
  rule — *more than one unit needs it, and it does not know about HormigaApp* —
  which pushed `seed.hpp` and `rescue_import.hpp` (and with them the whole of
  nlohmann/json) out of five units that never used them.

  **What it cost, and a correction.** The link died with no diagnostic
  whatsoever, and was diagnosed as PE's sixteen-bit section count (measured:
  44,341 sections before the split, 81,867 after, ceiling 65,535) and fixed with
  `-Og`. **That was wrong** — retested 2026-08-18, the tree links fine at `-O0`.
  The real cause was one undefined symbol whose body was lost during the move to
  `app_shared.cpp`, fixed in the same sitting. `-Og` has been removed.

  The lesson that survives is the tooling one: **the gcc driver swallows `ld`'s
  stderr here**, so every link failure looks identical and says nothing. Run
  `ld.exe` directly to see the real errors. See `src/app_internal.hpp`.

- **Q31 — valid time vs transaction time — ANSWERED 2026-08-17. The author left
  the call to me, and the answer is that there are THREE axes, not two.**

  1. **Transaction time** — when *we* learned it. The command log and Reyna's
     archive record, and never on a rune. Already true; kept true.
  2. **Valid time** — `from`/`until`. When the assertion is in force.
  3. **Decision time** — `adopted`. When the body voted.

  `from` had been doing jobs 2 and 3 at once. That is invisible while adoption is
  prospective and immediate — the only case the current corpus contains — and it
  breaks on a **retroactive** amendment, which is entirely ordinary in civic
  data: an ordinance adopted in March, effective back to January. Two such
  assertions in force on the same day then carry equal strength and merge to ⊤,
  reporting to a reader a conflict the record does not have, as though the law
  were unclear.

  So `resolve_at` now **filters** on valid time and **ranks** on decision time,
  falling back to `from` when `adopted` is absent — which for an immediate
  adoption is not an approximation but the same number, so everything already
  stored keeps its exact meaning. `adopted` is a *field* rather than a second
  interval because it is a fact the source states about the act; where an
  assertion sits in time is `from`/`until`. Reyna emits it; `civic_smoke` §4b
  tests the retroactive pair, and would fail under the old ranking.

- **Q32 — a `geometry` pivot before any polygon importer — ANSWERED 2026-08-17
  by building it, and it turned out to be larger than geometry.** The author's
  instruction was to work something out generally and not to fear going
  overboard if the foundation improves. Taken literally, and it paid.

  Reyna's [pivot](../../VoidReyna/okf/concepts/pivot.md) is **four coordinate
  systems, not one**: `Trail` (containment), `Span` (time), `Site` (space,
  including volumes and tiling cells), `Key` (named dimensions). They came out of
  writing twenty-two hypothetical harvests
  ([extraction-cases](../../VoidReyna/okf/concepts/extraction-cases.md)) and
  finding that six of them break a single-shape design.

  **The part that makes it ours rather than arbitrary: the four were read off
  Allomone.** Hormiga already ships 22 domain predicates and they partition
  exactly that way — `under` is Trail, `before`/`after`/`overdue`/`upcoming`/
  `undated` are Span, `near`/`near-rune`/`located` are Site,
  `field`/`field-has`/`role` are Key. The app has been asking questions in four
  coordinate systems since long before the pivot existed.

  Three consequences worth carrying back here. **A Site need not be resolved** —
  an address is how the most common spatial datum actually arrives, so geocoding
  is a holiday and ungeocoded is a state, not an error. **`place` is the view
  slice** (SPEC §3.2/§6) and a geographic locus must never emit it; Reyna's type
  is named `Site` to stay clear of the collision. And **loci are a set** — a
  council district redrawn every decade is a region *and* an interval, which is
  the case that kills any design making them exclusive.

  Still deferred: the GeoJSON importer itself. But it is now one lens into a
  pivot that exists and is tested, rather than a direct adapter in a costume.


- **Q33 — attribution of unlabeled speech** (opened 2026-08-12; **the easy way
  out is closed**). Springfield publishes **no transcripts**, and video is
  YouTube-only from 2020-09-21, so the statement corpus has to come from
  auto-captions — timed, but with **no speaker labels**. So inference is the only
  path to "who said this," on video of named real people, published.

  **Lean, and it is the author's own framing: models distinguish, humans
  identify.** Diarization is very good at *"these forty segments are one voice"*
  and has no idea whose. So cluster the voices, let a person name each cluster
  **once**, and one click attributes hundreds of segments — an afternoon instead
  of a month, with accountability intact. Every attribution carries a **method**
  (`labeled`/`diarized`/`inferred`/`human`) and a **confidence**, and **only
  `labeled` and `human` may reach the website** — the `web-hide` export seam that
  already exists and is tested.

  **The good news that makes this safe to defer entirely:** minutes record who
  moved, seconded and voted, **by name, as a matter of law**. So the
  highest-value, legally-meaningful half of the dataset needs no ML at all. Votes
  first, speech later.

- **Q34 — what exactly is a harvest record? — LARGELY ANSWERED by Void Core**
  (opened 2026-08-12; researched the same day). The archive is the load-bearing
  part: fetching is easy, *proving in two years what a page said on a given day*
  is not. I was about to specify a bespoke record. **`Scry` already ships it:**
  `provenance(data)` is *"a stable, order-independent snapshot id"* — pure, no
  clock, content-derived — and `materialize(..., stamp=<field>)` writes it, so
  *"an archive carries proof of what snapshot it captured and a reader can tell
  if the live data still matches."*

  So a harvest record is **a holiday that snapshots + `materialize` with a
  stamp**, plus the transport facts a snapshot id cannot carry (URL, UTC
  timestamp, HTTP status/headers, raw bytes). And the inherited rule matters as
  much as the mechanism: **holiday-backed data is resolved from a snapshot,
  never folded into authoritative state at edit time** — for a civic record that
  is a provenance guarantee, not just hygiene. Change detection is then a
  comparison, and **a stamp that moved on a document supposed to be final is
  itself a finding.**

- **Q37 — where do the models run, and is that settled?** (opened 2026-08-12).
  **Lean: settled, and forced rather than chosen.** Void Core's transform-layer
  invariant 4 — *"any nondeterminism is injected by the action, never generated
  inside"* — means a diarization or ASR model cannot live in the pure interior.
  The resolution is not to give up reproducibility: **run the model at the
  holiday boundary, record its output as data, and make everything downstream a
  pure function of that record.** The model's output becomes an *input*. That
  buys re-runnability without the model, model-version-as-part-of-the-record
  (so "re-transcribed in 2028 with a better model" is a new assertion rather
  than a silent overwrite — the same shape as amending a policy), swappable
  models, and a downstream that tests with no GPU and no network. It is also why
  the LLM ban is technical rather than squeamish: an LLM's output is
  irreproducible *and* uncitable, failing both the invariant and the archive's
  reason for existing.

- **Q38 — "the Queen" vs "Void Reyna" — ANSWERED (author, 2026-08-12): rename
  the interface.** It is now **`model`**, the plain payload word, matching
  `records` / `assets` / `site`. Two reasons from the author: LLM integration is
  likely to become a **Void Maiz** concern with its own name (the way Allomone
  did), so Hormiga's side should be a descriptive port and not a brand; and
  naming a placeholder is what caused the collision in the first place.
  Antfarm and roadmap updated.

- **Q35 — the Wayback Machine as a second witness?** (opened 2026-08-12). Its
  CDX API lists every capture of a URL with a **content digest**, so a page's
  change history is readable without downloading captures; Save Page Now can
  push a URL into a third-party archive. **Lean: use both.** A claim that rests
  on somebody else's copy as well as ours is worth far more than ours alone —
  our own archive is exactly the evidence an accused party would dispute.

- **Q36 — does Hormiga learn about provenance, or does Reyna keep it?** (opened
  2026-08-12). Either every imported rune carries a citation (source URL +
  capture timestamp + hash) as fields, or Void Reyna keeps the evidence locker
  and Hormiga stores only a reference into it. **Lean: a citation on the rune.**
  A public civic site's whole credibility is "here is where this came from, go
  check," and a citation that lives in another application's database is a
  citation the website cannot render. The raw bytes stay in Reyna; the *pointer*
  travels with the data.


- **Q21 — Allomone, the rules engine** (opened 2026-08-03; **named + fully
  specified 2026-08-03** → the [Allomone](/concepts/allomone/index.md) concept
  folder; plain-language explainers in
  [developer_explanations](/developer_explanations.md)). A visual, declarative,
  rule-based language over the rune graph (production / graph-rewriting system,
  *not* Scratch's actor model; stigmergic, not event-driven), edited as typed
  blocks with a dual **Allomone Script** text surface that lowers to Void Script.
  The sub-decisions, **now answered by the author (2026-08-03)**:
  - **q21a — unify or parallel? ANSWERED: parallel then replace.** Allomone will
    replace the map/calendar rules engines (it reaches all tabs); **freeze** them
    now (do not delete, do not extend), migrate onto Allomone at parity
    ([roadmap](/concepts/allomone/roadmap.md) phase F).
  - **q21b — derive vs materialize. ANSWERED: DERIVE-ONLY for now.** Allomone
    may **not** create tags or change the database at all in its current/near
    scope — it only *styles*. Materialization (writing data) is **deferred until
    the engine is mature** (sandboxing, dry-run, error prevention, testing),
    because mixing writes with enable/disable + loops/clock is a soundness
    minefield ([domains](/concepts/allomone/domains.md)).
  - **q21c — reactive/clocked mode. ANSWERED + refined.** Delta-reactivity is
    **always on**, and **tracked separately** from the clock — deltas are
    **stored on the graph itself** (each rune/mantle keeps an append-only delta
    log), not on a side event queue. The **clock** (loops/`wait`/animation) is
    **one** background app-wide timer, **off by default** (`ui.allomone.clock`),
    made as efficient as possible ([reactivity](/concepts/allomone/reactivity.md)).
  - **q21d — layering & specificity. ANSWERED: overlap is a FEATURE, no
    warnings.** Broad defaults + specific exceptions ("all dogs blue; small
    european dogs light blue") resolve by **specificity**, measured as **logical
    depth** — subsumption (entailment) first, then a score counting condition
    literals **and** conditional-nesting depth ("length of logic"); **recency**
    (per-*script*, using the rule rune's existing id — no per-line IDs) as the
    final tiebreak ([conflicts](/concepts/allomone/conflicts.md); explainer in
    [developer_explanations](/developer_explanations.md)).
  - **q21e — home. ANSWERED: Allomone gets its OWN tab/section** (too large for a
    Data Tools entry). Rules default to **shared** (model tier); **local views**
    (config tier, per-machine) are an explicit scope.
  - **q21f — round-tripping. ANSWERED: no Void Core message needed.** Hormiga owns
    the Allomone AST, so blocks ⇆ Allomone-Script round-trips entirely host-side.
    A Core parse API would only help importing *foreign* raw Void Script into
    blocks — not needed to ship; draft the small ask only if that nicety is ever
    wanted ([language](/concepts/allomone/language.md) §"round-tripping").
  - *Remaining small opens:* the name **Allomone Script** (vs the earlier working
    "Hormiga Script") is the author's to confirm; the block-canvas UX and the
    Allomone-Script concrete syntax are settled during phase A/D.
- **Q22 — the Notes engine** (opened 2026-08-03; **bare-bones tab BUILT
  2026-08-03**). Notes now have their **own dockable tab** (`draw_notes_body`:
  a note list + a plain text editor; notes removed from the Data tab) — the
  minimal "somewhere to keep notes" the author asked for. The **engine** is
  still deferred: **seamless Obsidian / plain-markdown interop** (read/write
  `.md`, a vault-shaped store), **"note → newsletter"** (a note becomes Builder
  document blocks, reusing the `doc` verbs), and **Allomone acting on note
  text** — which implies **textual-embedding models** built into the app for
  semantic search / auto-linking / tag suggestion over prose. *Lean:* keep the
  tab minimal; treat the markdown vault, the newsletter bridge, and the
  embedding-powered Allomone-on-text mechanics as separate later builds. Not the
  current focus.
- **Q23 — Allomone's "class"/object model** (opened 2026-08-05, by the author:
  "can we make a class? idk if we wanna go into OOP given the graph structure").
  The tension is real: prior claims make Allomone **declarative, set-at-a-time,
  stigmergic** — runes **never message-pass**, they coordinate through the shared
  environment ([paradigm](/concepts/allomone/paradigm.md)). Classical
  message-passing OOP (objects sending each other messages, encapsulated mutable
  state, inheritance chains) is therefore **off-brand by construction**. **Lean:
  do NOT adopt message-passing OOP; adopt the model Hormiga already has —
  ARCHETYPES + GENERIC DISPATCH** (the ECS / R-S3-S4 shape):
  - A **glyph is a class** (schema + instances): `contact`, `event`, `day` are
    archetypes; a rune is an instance; `thing is (contact)` is `isinstance`.
  - A **tag is a trait/mixin** (structural, many per rune) — composition, not an
    inheritance tree; closer to Rust traits / Go interfaces / ECS components.
  - A **script is a "method-less" system**: behavior lives in scripts that
    **query** runes and derive, not in methods attached to objects.
  - "Defining a class" in-language = declaring a **named archetype** (a predicate
    view: "a `Partner` is any org with `type:partner` and a non-empty email"),
    and a "method" = a **generic verb that dispatches on archetype/tags** —
    exactly **R's S3/S4 generic-function dispatch**, which is *not* message
    passing and *does* fit our substrate. This deepens the model instead of
    contradicting it. Captured as a direction in
    [language](/concepts/allomone/language.md); build after **thing-as-a-value**
    (below) lands, since dispatch needs to pass runes to verbs.
- **Q24 — imports / modules across scripts** (opened 2026-08-05, by the author:
  "import functions and classes from one script to another"). Scripts are already
  **runes** in the `allomone` mantle and already **carry tags** (built 2026-08-05).
  **Lean: a script IS a module; importing is a LINK between script runes** — which
  unifies with the links/connections primitive ([inputs](/concepts/allomone/inputs.md)
  §3): `import "helpers"` binds another script's **exported** functions/archetypes
  into scope and records a dependency edge; load order is the edge DAG; **cycles
  are rejected** (the same stratification rule as
  [conflicts](/concepts/allomone/conflicts.md)). Namespacing follows the R
  `pkg::fn` idea already noted for the data-verb "libraries"
  ([language](/concepts/allomone/language.md)). Not built; depends on functions
  (done) + a visibility/export marker.
- **Q25 — the Antfarm redesign** (opened + **BUILT 2026-08-05**). The typed
  dataflow model below shipped: `core` now exposes **records** + **assets**
  payload ports; record agents (SQLite/CSV/Supabase/Sheets) plug into records,
  asset agents (local files / ImgBB) into assets, and the **HTML publisher**
  consumes records+assets to emit a **site** that a **server** (localhost) or
  **deployer** (GitHub Pages) carries — a visible **pipeline**. Locality is
  badged; the default is local-only. `seed.hpp` (`register_antfarm_glyphs` +
  `seed_antfarm_transcript`), palette regrouped by payload, concept rewritten
  ([antfarm.md](/concepts/platform/antfarm.md)). *Remaining polish (not blocking):* faces
  for the new `hol_github` deployer; store-vs-source could later split into
  distinct core ports; the `.miga` publish/deploy actions wire to the new nodes.
  Original critiques, all now addressed: (a) **Supabase and
  Google Sheets share one `import` socket** though they're fundamentally
  different agents; (b) **`out-html` and `out-imgbb` share one `output` socket**
  though a *site generator* and an *image host* are unrelated; (c) the **`core`
  node is a featureless hub** (4 generic sockets: data/assets/output/import);
  (d) the model **doesn't distinguish local from cloud**, yet the shipped Cat
  Colony is purely local (**done meanwhile: the default Antfarm is now
  LOCAL-ONLY** — sqlite/assets-fs/out-html/localhost/csv, no cloud seeded;
  `seed.hpp`). The root problem: **the ports are typed by a coarse
  input/output bucket, not by WHAT FLOWS.** *Lean — a typed dataflow graph:*
  - Type ports by **(payload × direction)**, not "in/out": **records**
    (structured rows), **assets** (blobs), **document** (a rendered site) — so
    a site-publisher (`records+assets → site`) is a different port shape than an
    asset-host (`blob → hosted URL`) than a record-store (SQLite/Supabase). The
    coarse `output`/`import` sockets dissolve.
  - Make publish a **pipeline**, not one socket: `core (records+assets) →
    publisher (out-html builds the site) → server/deployer (localhost serves /
    GitHub Pages deploys)`. `out-html → out-localhost` is a *chain*, not two
    things lumped on "output" — the graph should show that.
  - **Locality is a first-class attribute** (local vs remote), badged per
    holiday; **local-first** means local agents are the default and cloud ones
    are added deliberately. For a local app the Antfarm is mostly trivial/quiet
    — it earns its node-graph only when external services are wired.
  - **`core` exposes payload-typed ports** reflecting the data model, not four
    generic buckets. This is the interaction-net **I/O boundary** made precise
    ([concepts/antfarm.md](/concepts/platform/antfarm.md);
    [allomone/inputs.md](/concepts/allomone/inputs.md) §6). *Big build — needs
    the author's go-ahead on direction + scope before rebuilding the glyph/port
    model.*
- **Q26 — when to add a glyph (datatype) vs a tag** (opened 2026-08-05, author:
  "careful giving all runes a 'role' — events are runes too; maybe we need more
  datatypes?"). The observation is exactly right: **fields are GLYPH-SCOPED.**
  `role` is a `contact` field; an `event` has no `role`, so `event.role` reads
  `""` (graceful, but a script should **guard by glyph** — `rune is contact` —
  before reaching for a field). The design principle, restated
  ([data-model](/concepts/foundation/data-model.md), and the archetype framing in **Q23**):
  - **A glyph is a "class": a distinct FIELD SCHEMA + identity.** Add one when a
    thing has its own shape of data (a `resource` has `path`/`topic`; an `event`
    has `date`/`presenters`) — that's why the five kinds exist.
  - **A tag is a cross-cutting TRAIT/mixin** — used when the attribute is a label
    a rune of *any* glyph might carry (`orange`, `status:active`, `cat`).
  - **Lean: don't multiply glyphs for traits.** Cats-as-`contact` was right — a
    cat is contact-*shaped* (name/photo/bio/located), so it needs no new schema;
    "cat" is a tag. Reach for a new glyph only when the **field schema genuinely
    differs**. So: probably **no** new datatype for the cat colony; the general
    rule governs future cases. (Allomone already respects this — each rune only
    carries its glyph's fields; absent-field reads are `""`.)
- **Q27 — project folders / .miga internal organization** (opened 2026-08-05,
  author musing: "we might need project folders for data management — we have so
  many kinds of files to keep track of in the `.miga`"). Real: a database now
  holds data runes, Allomone scripts, Antfarm config, Builder documents, assets,
  and more. Two layers to the question:
  - **Inside the model**, Hormiga *already* has the organizing primitive:
    **mantles** are namespaces (`demo-org`, `allomone`, `antfarm`, `issue-demo`),
    and glyphs type the runes within. So "folders for the data" largely exist —
    the question is whether the UI should *surface* them as a browsable
    project-tree (a left-rail of mantles → glyphs → runes), which would help as
    the kinds multiply. *Lean: a mantle/kind tree view is a good UI addition; the
    model needs nothing new.*
  - **On disk**, the `.miga` v3 bundle is one packaged file
    ([miga-format](/concepts/platform/miga-format.md)); its *internal* layout could grow a
    clearer folder structure (state/, assets/, per-mantle sections) as content
    diversifies. *Lean: keep the single portable `.miga` as the unit; formalize
    its internal folders if/when packing/unpacking needs it — not user-facing
    "project folders" on the filesystem, which would fight the portable-bundle
    and E2EE-vault design.* Not built; revisit when the Builder/asset volume grows.
- **Q28 — Allomone surface syntax: `.` vs `:`, `has`, and the model it reflects**
  (opened 2026-08-05, author rethinking the language). First, the **model, stated
  precisely** (the author asked to be corrected — "is everything a tag?"). A
  **rune** is FOUR distinct things, not one:
  - its **glyph** — its *type* (`contact`), set at creation. Not a tag. (Hormiga
    *also* redundantly tags `type:contact` — a real overlap that fuels the "is it
    a tag?" confusion. `color:`/`icon:` tags similarly shadow map-marker style.)
  - its **tags** — axis-labelled **set membership** (`orange`, `type:contact`,
    `month:july`). A rune has a *set* of them. Set-algebra, not values.
  - its **fields** — **typed values declared per glyph** (`role`, `email`,
    `date`, `bio`). A rune has one *value* per field (or empty). Not tags.
  - its **links** — directed **edges** to other runes.
  - and `rune:color(...)` is **none of the above** — a **derived effect**
    (derive-only styling *output*), never stored, never read back
    ([execution](/concepts/allomone/execution.md)). So it isn't a getter/setter
    on a "color property." So: **not** "everything is a tag" — tags/fields/glyph/
    links/effects are five distinct things, and a good syntax should *reflect*
    that, not flatten it.
  - **The syntax question.** The author leans toward `:` for "additional
    property" (`rune:color`, `rune:tag("cat")`), possibly dropping `has`, and asks
    whether the language should be wordier or more symbol-heavy. *Lean — map the
    operator to the model:* **`.` reads DATA (nouns)** — `rune.date`, `rune.email`
    (fields), `rune.tags`/`rune.glyph`/`rune.links` (structure); **`:` invokes
    ACTIONS/EFFECTS (verbs)** — `rune:color(hex)` today, gated `rune:addTag(...)`
    later. This mirrors **Lua** (`.` data, `:` method) and gives IntelliSense a
    clean rule: **typing `.` suggests fields/tags; typing `:` suggests the effect
    palette** (and `:color` pops the wheel — exactly the author's wish). Keep
    **`has`/`is`** as *readable* set/type tests (Allomone's audience is outreach
    staff, not programmers — favour English where it aids comprehension), but
    make the intellisense complete both. *Open for the author:* whether to also
    allow **typed subjects** (`contact`/`event` as narrowing aliases for `rune`,
    like a typed `var`) so `contact has …` suggests only contact-relevant tags via
    the [tag-recommender](/concepts/allomone/tag-recommender.md) — a good
    IntelliSense refinement, additive, engine-neutral. And the **wordy-vs-terse**
    dial is genuinely the author's stylistic call; this lean keeps it readable.
    (Not built — a naming/UX decision, not an engine change; the interpreter
    already separates tags/fields/glyph/links/effects.)
- **Q12 — the widget toolkit** (opened 2026-07-16, upstream-gated). The
  sections need traditional desktop UI — tables, forms, tabs, palettes,
  wizards — every widget CLI-complete via Void Maiz's widget protocol. The
  protocol exists as upstream concept only (their Phase 4); Qt-wrapping is
  contemplated there but nothing is built. Asked in `MESSAGE_FOR_VOIDMAIZ.md`
  (2026-07-16). **Lean: an ImGui-composed widget kit speaking the protocol,
  not Qt embedding** — Hormiga wants the traditional *feel*, and real Qt
  fights vendor-don't-depend, the single render loop, and the Android path.
  Interim: host-side ImGui tables/forms (already sanctioned upstream),
  structured so the kit slides in underneath.
- **Q13 — Analysis section scope. ANSWERED by reframe (author, 2026-07-22):**
  "we're already kinda doing it — a calendar and map are all forms of
  visualization of the data… we will naturally create more visualization
  tools as this software progresses." No dedicated Analysis build:
  visualization surfaces ACCRETE (map, calendar, physics graph, next ones),
  each riding the same rules/layers machinery. Charts arrive when a chart is
  the natural next surface, not as a section project.
- **Q14 — Territory's holidays.** The map section's physics — now fully
  conceptualized ([territory](/concepts/sections/territory.md), 2026-07-20): (a) **map
  source** — the base is a *holiday*, and the twist is **don't assume Earth**:
  online tiles vs offline tile pack vs an arbitrary image (a fantasy map);
  Hormiga loads a map, it doesn't make one (map-making = a future Void Maps);
  (b) **geocoding** — a Geo holiday (opt-in, online) replacing Neighborhood's
  geocode.maps.co; (c) **export** — a web-domain map page and a static
  map-image block for newsletters. **Leans adopted; the concept doc is the
  answer.** Remaining sub-decision → Q15.
- **Q15 — the map canvas & 3D.** Territory's mode-2 needs a Void Maiz **map
  canvas view** (geographic camera) — asked upstream 2026-07-20. Open: is the
  live map a 2D Void Maiz view, or does a 3D map belong in **Void Maiz XR**?
  **Lean: 2D canvas view first (mode 1 static image needs no widget at all);
  3D is a "just in case" horizon, deferred to XR.** Resolve when T2 starts.
- **Q19 — read vs write nodes; multi-database open/create.** The author's
  2026-07-22 direction: `.miga` = the Antfarm (topology + creds), data lives
  behind nodes; "create new database" = a new `.miga` + fresh local nodes +
  a demo database for fresh installs; shared nodes across differently-
  arranged topologies are the collaboration path. Open: the read/write
  distinction's shape (per-edge flag? per-node capability? tied to roles?),
  and the multi-database boot flow (open/create/recent list). **Lean:
  per-edge read/write capability declared in the topology, enforced at the
  holiday seam; boot flow = open-or-create with the demo as a starter.**
  **Extended (2026-07-23) — DOCUMENTS AS PROJECTS.** The author: "lots of mini
  projects within Hormiga; the builder [and calendar] need to save & load
  documents like different projects; things need to be SAVED as a file on
  disk and LOADED; storage = wherever the database saves (Antfarm-decided)."
  This is the same multi-store question one level in: not just many databases,
  but many DOCUMENTS (newsletters/websites/calendars) within a store. Today =
  one issue-demo mantle. **Lean: a document is a named mantle (or a saved
  element-transcript+theme bundle — a user template already is one); the
  Builder/Calendar/Map gain a create/open/rename/switch document picker; the
  store is the Data holiday.** Design with the multi-database flow in one
  session (they are the same shape at different scopes).
  **RESOLVED (2026-07-23) → [.miga v3](/concepts/platform/miga-format.md).** The
  dedicated session happened. A `.miga` file is the WHOLE database (all
  mantles: data + every document + maps + calendars + the Antfarm topology) +
  assets + encrypted secrets — you switch `.miga` files like projects.
  Architecture is git-shaped: the working copy (`base_dir` `.db`+`assets`) is
  the working tree; the `.miga` is the bundle; Save packs, Open unpacks.
  Re-derivable data (tiles/site/exports) rebuilds from protocols (the Antfarm
  graph IS the protocol layer); irreplaceable data (state/assets/secrets) is
  bundled. Online/offline handled at load (offline always opens; cloud nodes
  degrade). Read/write-node distinction folds into the sharing model (still
  future). Documents-as-projects (built 2026-07-23) is the same shape one
  scope in. Sharing (LAN/P2P) documented, empty window, built later.
- **Q20 — the Builder pivot: components + live preview** (author direction,
  2026-07-22 — "not blocks, but actual components… a live preview… like
  website builders today; newsletter mode and website mode; we may be
  ditching the blocks soon"). The MODEL survives (content units, query-backed
  sources, bilingual fields, per-domain render packs, logged edits); the
  EDITING SURFACE becomes direct manipulation of the rendered thing. The
  open question is the preview's engine in a native ImGui app: (a) an
  ImGui approximation of the output (fast; but two renderers drift apart),
  (b) a vendored HTML renderer (litehtml-class: faithful, heavy, a real
  dependency), (c) a **hot-reload browser preview** — every edit re-renders
  site/ (or the email preview) and the browser beside the app auto-
  refreshes; the web page previews AS a web page, the native canvas stays
  the arranger. **Lean: (c) first — it is honest WYSIWYG at near-zero cost
  and forces no renderer duplication; graduate to (a) for in-canvas
  arrangement feel once component drag/resize gestures need pixel-true
  feedback. Blocks are not deleted until the component surface covers their
  jobs** (the map/calendar widgets, grids, and render packs carry over
  regardless).
  Sandbox/simulate modes: recorded, deliberately not designed yet.
  **Author (2026-07-22): direction sharpened.** The Builder 'builds'
  MULTIPLE things — newsletter builder and website builder now,
  presentations/PDFs as an open future — with a **very distinct
  difference**: the newsletter builder focuses on **HTML components**, the
  website builder on **JavaScript interactable components**. Live preview +
  drag-and-drop wanted; "we likely need to think through this a lot more" —
  plan drafted for author review (okf/concepts/sections/builder.md, 2026-07-22).
  Q12 note, same pass: more widgets/GUI polish deferred — "functionality
  and features" first. Q19: crucial (Antfarm is foundational) but wants a
  DEDICATED design session — deliberately not folded here.
- **Q18 — canvas actions as first-class, agent-legible commands** (asked
  upstream 2026-07-21). A map canvas has pseudo-GUI *inside* it (place/move/
  region/radius-select/measure). The WRITE side is already first-class (custom
  view emits `set geo`/`rune new`, logged + agent-reproducible). Open: the
  generalizable engine for (a) a host-registered, introspectable action/tool
  vocabulary on custom views (Void Maiz), (b) spatial/custom query predicates
  for agents — `ls --near`/`--in-region` (likely Void Core, relayed), (c) the
  one-definition-two-front-ends prize (a gesture AND a CLI verb → the same
  logged command). **Lean: we supply needs+context (the map), upstream owns
  the generalization; the one-definition property matters more than where the
  registry lives.** Blocked on the upstream reply; not on our critical path
  (map mode-1 needs none of it).
  **Author (2026-07-22): elevated — this is the Builder pivot's spine.**
  "The builder itself is another visualization of the data… the map and
  calendar are different ways of visualizing; the builder is a way to
  EXPORT this. We can't really have layers — we have GRIDS, and we place
  things on a grid. The newsletter will have grids within grids… newsletters
  are a lot more constrained; websites have a bit more freedom." Canvas
  actions + agent-legible commands are the required foundation: every
  builder gesture must be a `doc` verb the way every map gesture is a `map`
  verb. Folded into the Q20 plan.
- **Q17 *(direction set by the author, 2026-07-22 — see [antfarm](/concepts/platform/antfarm.md)
  "the door, not the warehouse")* — the `.miga` v2 container: how much does it hold?** The credential
  vault shipped (2026-07-20) as a passphrase-locked secrets file. The author
  wants `.miga` to be the full **org bundle** (topology, roles, templates,
  local tags, notes, media refs — potentially large with images/video).
  Open: keep `.miga` a *small encrypted secrets file* + a *separate* encrypted
  bundle/manifest referencing a content-addressed blob store (the lean, from
  the earlier .miga-architecture discussion — media as hash refs, not inline),
  or one growing container? **Lean: small secrets vault (built) + an encrypted
  manifest bundle that references `assets/`-style content-hashed blobs; never
  inline large media in one monolithic encrypted file.**
  **RESOLVED (2026-07-23) → [.miga v3](/concepts/platform/miga-format.md).** The author
  chose the OPPOSITE of the old lean, deliberately: `.miga` v3 IS the whole
  database in one bundle — state + assets INLINE (base64) + encrypted secrets —
  "large files now are fine; I want something that stores everything." The old
  "hash-refs, never inline" lean was optimizing a constraint the author
  waived. What's kept from it: the content-addressed dedup idea returns as the
  COMMIT-GRAPH future (unchanged blobs dedupe by hash once Save becomes a
  commit). v2 (secrets-only) is now LEGACY. The re-derivable/irreplaceable
  split (tiles re-fetch; assets bundle) replaces "everything or nothing."
- **Q16 — movable panels — CLOSED (author, 2026-07-22): "they work just
  fine for me right now."** ImGui docking shipped 2026-07-20 (all panels
  dock/float/close/restore via the Windows menu). Allmusely remains the
  eventual general answer; nothing further needed here.
- **Q9 — email image delivery.** The `email` domain's hard physics: a
  newsletter's images must be loadable by the recipient's mail client, which
  means a public URL or an embedded attachment — and v1 ships entirely local
  (Q4). Options: (a) **CID-embedded attachments** stamped at send/export time
  (works fully offline, bloats message size), (b) a **self-hosted static
  asset host** on the org's own domain (the Q10 infrastructure), (c) a
  third-party image host as an explicitly opt-in holiday (never default,
  never subscription-assumed). **Lean: make image `src` resolution a holiday
  call at the render seam** — an asset-URL resolver the renderer asks, so
  export-with-CID, self-hosted URLs, and third-party URLs are three resolvers
  behind one seam and the block model never knows the difference. v1 exports
  can inline/CID; the self-hosted host is the destination.
  **Partially resolved (2026-07-20):** the email render now resolves image
  `src` to the published ImgBB **`url`** (table layout too — Gmail-safe),
  while the web render uses the local `path` (self-hosted assets). Two
  resolvers behind the seam, live. CID-embedding and self-hosted-URL resolvers
  remain for the Courier/self-host paths.
  **Author (2026-07-22): destination set** — self-hosted on a purchased
  domain when things leave the device; until then the discipline is
  **render-as-if-real**: "EVERYTHING should render and be exported as if it
  were real (it just so happens to be using a local database as the hosting
  service)." The resolver seam is exactly the slot the future self-host
  fills; nothing about message assembly is decided now.
- **Q10 — the self-hosted infrastructure scope.** The author's direction:
  no subscription services; Hormiga should provide the tools to run your own
  — the Antfarm as the org's own Backend-as-a-Service, hosting files and
  sites on a domain the org purchases (domains stay external purchases). The
  same infrastructure serves website deployment AND newsletter assets. Open:
  what is the first online rung — a static-host deploy holiday (push to a
  VPS/object store the org controls), or Hormiga itself running a serving
  process? **Lean: push-to-something-org-owned first** (deploy holiday to a
  self-hosted static host; rsync/SFTP/S3-compatible-self-hosted class), and
  "Hormiga as the server" stays a recorded horizon — it reopens the
  no-server-in-the-runtime boundary, so it must be argued for on evidence,
  as its own hosted component if ever.
  **Author (2026-07-22): confirmed as PLANNED** — local focus now; "the
  self-hosting domain infrastructure will come soon. we should plan it."
  The lean stands; it enters the roadmap when the local builders settle.
  **First rung BUILT (2026-07-23): the LOCAL WEB HOST.** The author: websites
  are "getting complex enough that we need localhost versions, not just an
  html file… simulate what it might be like with a real domain… adding extra
  nodes to the antfarm." Done: a **`hol_localhost` Antfarm node** (Output,
  wired to core) serves the built `site/` clean on `127.0.0.1:8780` — a
  stand-in for a real domain, distinct from the dev preview (host = no reload
  injection, deploy-faithful, site/ as root; preview = artifact-whitelisted,
  reload-injected). `effect serve-site` / the node's "Serve site locally"
  button start it. **Newsletters stay static HTML** (their nature); only the
  website is hosted. The real remote-domain deploy node is the next rung
  (rsync/SFTP/object-store to org-owned space) — the local host proves the
  node shape it will fill.
- **Q11 — OS keychain for convenience unlock** (carried from Q6, unanswered).
  The passphrase is mandatory; is the OS keychain offered to soften daily
  unlocks? **Lean: decide when the unlock flow is built; not assumed.**


# Philosophical

*Opened as a section 2026-08-27, at the author's request.* These are not
blocked builds. They are questions about **what this thing is**, and they earn
a place here because the OKF has twice discovered that an unexamined answer to
one of them was quietly steering a decision — the Q20 Builder pivot and the Q42
map-separation question were both really this kind of question wearing a
feature's clothes.

The discipline that makes them useful rather than decorative: **a philosophical
question stays open until a build forces it**, and when it is forced, the answer
lands in a concept and the question moves to *Decided* with the build that
settled it named. They are not a place to think out loud forever.

- **Q45 — what is Void Hormiga?** (opened 2026-08-27, by the author.)

  > is it the builder? or is it the antfarm? its definitely not the map (we've
  > even thought of separating that). is it the database representation? idk.

  The question arrived from a real observation: **the `.miga` holds the keys to
  the entire Antfarm and its configuration.** All someone needs is that file.
  So whatever Hormiga is, the file seems to be more of it than any window is.

  **Lean: Hormiga is the `.miga` — a database that carries its own backends —
  and every section is a projection of it.** The argument, in four steps:

  1. **The sections are already interchangeable and the file is not.** Data,
     Builder, Calendar and Territory talk only through the dispatcher and the
     mantles. Any of them could be deleted and the database would still be the
     organization's database; delete the `.miga` and there is nothing to
     project. [Application boundaries](/concepts/foundation/application-boundaries.md)
     and Q42 already established that the map is separable — that separability
     is evidence for this answer rather than a threat to it.
  2. **The Antfarm is the part of the file that makes it self-describing.** The
     topology *is* the protocol layer
     ([.miga format](/concepts/platform/miga-format.md)): a local-SQL node says
     the data lives in a local db, a tile-source node says where tiles come
     from, an object-store node says where the backup goes. A database that
     carries the recipes for rebuilding everything derivable about it is a
     genuinely different kind of object from a database plus a config file, and
     it is the unusual thing here.
  3. **So the Antfarm is not a rival answer; it is a property of the file.**
     "Is it the builder or the antfarm" has the shape of a false choice: the
     Builder is a projection, the Antfarm is part of the thing being projected.
  4. **And this is why sync is the load-bearing feature it turned out to be.**
     If Hormiga is a file, then "work on it from multiple devices" is not a
     feature request — it is the question of whether the file can exist in two
     places and still be one thing, which is the only question that could
     falsify the whole framing. See
     [collaboration](/concepts/platform/collaboration.md).

  **What would change if the author answers differently.** If Hormiga is *the
  Builder* — a publishing tool that happens to keep a database — then the data
  spine is infrastructure, the Civic Record is a different product, and the map
  should have been separated already. If Hormiga is *the Antfarm* — an
  integration surface — then the sections are demos of it and the real roadmap
  is more holidays. Both are coherent; they are just different projects, and
  it is worth knowing which one is being built before the next big commitment
  rather than after.

  **The thing this lean does NOT license.** "It is the file" must not become an
  argument for putting more into the file. The bundle already refuses to store
  re-derivable data, and the discipline of `.miga` being the *irreplaceable*
  set is what keeps it portable. An answer to a philosophical question is not a
  budget.

- **Q46 — do the three kinds of people get invented names?** (opened
  2026-08-27, from the user-profiles proposal.) The proposal named them
  **Barriga** (a person using the desktop app), **Ramon** (a person using the
  website) and **Chavo** (a person in the database), on the reasoning that
  semantically empty names *force* a developer to read the documentation.

  **Lean: no — keep admin / identity / contact, and the author overrules this
  if the names are wanted.** Four reasons, and the first is the one that
  matters:

  1. **The system already names all three, and the existing names carry a
     distinction the new ones lose.** `src/domain/seed.hpp` is emphatic that an
     `actor` is *"the identity the sign-in provider vouched for, NOT a person"*
     — and [identity](/concepts/platform/identity.md) extends the same rule to
     the admin profile, which is a **credential on a device**, not a human. A
     naming scheme whose whole premise is "these are three kinds of *person*"
     re-collapses exactly the distinction that lets Ada have a laptop and a
     phone without a device table.
  2. **It re-opens a refusal.** [web platform](/concepts/platform/web-platform.md)
     §4 says an admin "is not a row in a table with a boolean. It is a contact
     whose runes carry a tag." Three classes with "clear object types in code"
     is the roles column returning under a new name.
  3. **"Forcing a developer to read the docs" is an anti-goal in this repo.**
     The OKF is written to be read cold, by agents, in a terminal, beside a
     binary. And the vocabulary budget is already spent: rune, mantle, glyph,
     holiday, temper, colony, Antfarm, Allomone, Scry. Three more opaque terms
     is a real cost against a benefit that is a slogan.
  4. **They are not semantically empty in Spanish**, which is half of this
     application's audience: *barriga* is a belly, *ramón* is a common given
     name, and *chavo* is everyday Mexican slang for a kid or a guy. Labelling
     every person record in a Spanish-speaking organization's database a "Chavo"
     is likelier to read as flippant than as neutral.

  **What is worth keeping from the proposal's instinct**, because it is a real
  one: the *three-way distinction* is correct and was under-documented. That is
  now [identity](/concepts/platform/identity.md), written with the existing
  words. If the author wants the invented names anyway, the concept page is
  where they would be applied, and it is a rename rather than a redesign — the
  structure does not depend on the words.

# Decided

- **Q64 — three kinds of rune** and **Q65 — may an edge weight BE a value?**
  ANSWERED 2026-09-03/04, upstream, and cleared from Open. Void Core 0.2.14
  built both. Read
  [the log](/log.md) for the exchange; the durable statements are:

  **Q64 → `entity` / `act` / `measure`**, a `"kind"` on the glyph descriptor,
  default `entity`, nothing migrates. The naming is ours and the reasoning that
  carried it was evidential rather than aesthetic: the author's first proposal
  (γ/δ/ε, after the interaction combinators) was withdrawn because
  `../VoidMaiz/include/voidmaiz/reduce.hpp` already uses those letters in
  Lafont's own sense *about glyphs*, and ε is the arity-**zero** eraser — close
  to the opposite of "a concept carrying a value". Two sibling projects using
  the same three letters for different things, both about glyphs, in one stack
  would have been a confusion created on purpose. "Verb" was unavailable to the
  dispatcher.

  Core varied us in one place worth remembering: kinds are **not** a reserved
  `kind:<k>` tag the way `glyph:<name>` is, because `kind:` is already an
  ordinary application namespace and reserving it would have silently changed
  what every existing `kind:vegetable` tag matches. The queries are
  `ls --kind <k>` and `glyphs --kind <k>`.

  Hormiga's adoption: five glyphs carry `"kind":"act"` — `statement`,
  `revision`, `submission`, `deployment`, `incident` — each recording a
  happening whose subject is another rune, each already reified for the arity
  reason. `event` deliberately is not one, because an event here is the thing a
  person attends and its fields are read by renderers rather than filled as the
  roles of a verb; marking it would be reading the English word rather than the
  model.

  **Q65 → yes in Void Core, and NO for Hormiga's own data**, for a mathematical
  reason rather than a conservative one. An edge whose `to` endpoint is a rune
  of a `measure`-kind glyph is an **attribute assertion** and the weight *is*
  the value; the `values` verb reports them. But **a weight is a magnitude, and
  a date has no magnitude — it has a position.** Dates and coordinates are
  points in an affine space (subtract two for a duration or a displacement, add
  one of those to a point; never add or scale two points), while health and
  speed are vectors. *"Half of September 3rd"* is meaningless for exactly that
  reason while *"a quarter past twelve"* is fine. Most of Hormiga's numbers are
  points, so most of Hormiga keeps fields, and `measure` is unused here.

  The criterion for anyone choosing, which is the transferable part:
  **if a number is read by RULES that produce new structure it belongs on an
  edge where the rules can see it; if it is read only by renderers it belongs in
  a field.**

  Core varied us on where the unit lives — **on the RUNE, not the glyph**, via a
  new `measure` verb writing a `quantity` object beside `content`. We had
  written that the attribute rune supplies the unit and then asked for it on the
  glyph anyway; `health`, `speed` and `strength` share one schema and differ
  only in what they measure, so a glyph could not have carried it. They also
  declined a numeric-array weight, out of two rules that were already true:
  `concepts/links.md` says anything beyond relation/direction/weight must be
  reified as a rune, and a position is a point, so it was never a weight.

  Core wrote the **quantity** page we offered to draft
  (`../VoidCore/okf/concepts/quantity.md`).

## By the author, 2026-07-16 (the sections restructure — second batch)

- **The application is three main sections** — Data (management, traditional
  UI), Builder (Scratch-shaped: palette panel + block canvas), Antfarm (node
  graph + a user-friendliness layer the graph alone lacks) — over one core,
  one dispatcher, one global command bar. The bar is the FULL application:
  old Hormiga's whole surface, rebuilt better, not a demo canvas.
  See [workspace & sections](/concepts/sections/workspace-and-sections.md).
- **Analysis tools are wanted** (a mini-Tableau; scope → Q13).
- **Neighborhood is absorbed as Territory** — the map-making application
  (`../Neighborhood`) becomes a planned section on the spine: runes with
  location facets, the map as a view, geocoding/tiles/export as holidays
  (physics → Q14). The Geo interface reserved in the Antfarm now has its
  client.
- **Every section's widgets register with Void Maiz** so the CLI stays
  complete — the widget-toolkit question went upstream (→ Q12).

## By the author, 2026-07-16 (the founding eight, answered)

- **Q1 — name & identity: "Void Hormiga" is the full official name; the UI
  drops the "Void."** Like Adobe Photoshop — the family prefix is formal,
  "Hormiga" is what users see and say. Repo and stack-facing artifacts keep
  `VoidHormiga`/`voidhormiga`.
- **Q2 — schemas: redesign.** New shapes that still cover the original's
  purpose but fit the greater Void Core design (bio/internal-notes born
  separate, presenters 1–N as edges, axis-typed tags native). The import
  holiday owns the old→new mapping.
- **Q3 — PDF previews: ship without.** The feature never fully worked in the
  predecessor anyway; resources render as typed cards. Revisit on demand.
- **Q4 — Sheets and the cloud posture: v1 ships entirely local.** A 2-way
  Sheets holiday exists eventually as *some aspects* — never integral to the
  application. The deeper directive recorded here and in
  [the Antfarm](/concepts/platform/antfarm.md): most of Hormiga's features must work
  with NO internet connection; online holidays (image host, Sheets, email
  dispatch) are the exception, not the spine — and where they're needed, the
  strong preference is **self-hosted over third-party subscription services**
  (→ new Q9, Q10).
- **Q5 — bilingual: parallel EN/ES fields, and it must be an ENGINE.**
  Translation as content you can fix, not a render side-effect — but the
  behavior ships configured: the user does not assemble nodes to get
  bilingual newsletters. Generalized into the Antfarm principle:
  **defaults, not assembly** — the Antfarm is where behavior is *seen and
  reconfigured*, not where it must be *built* ([the Antfarm](/concepts/platform/antfarm.md)).
- **Q6 — encryption: confirmed as a headline feature.** E2EE, password-
  protected files, at-rest sealing — a big feature, not a checkbox
  ([security](/concepts/platform/security.md) stands as written; keychain → Q11).
- **Q7 — core linkage: follow Void Maiz's per-target pattern.** No third
  pattern invented.
- **Q8 — the verb surface: yes, a whole new vocabulary.** Hormiga is a large
  application and gets a real library of verbs and nouns added to the
  voidscript surface — seeded now in [the verb inventory](/verbs.md),
  validated against the predecessor's route inventory during phase C.

## At founding, by the author, 2026-07-15 (recorded in `DESIGN.md`)

- **A new native C++20 application, the whole shell on Void Maiz** — reversing
  the predecessor's convergence plan (no-new-app, keep-Python) and the old
  web/protocol lean, because the premise changed: Void Maiz exists, and the
  Python-specific surface audited small. Upstream trued up same day (their
  Q10 supersession note; we are client #2).
- **SQLite as the default Data holiday** (not MeshDB) — the native-host trade
  flips the old call; MeshDB stays the mesh growth path behind the same
  interface ([Antfarm](/concepts/platform/antfarm.md)).
- **No cloud BaaS, no hosted image service** — removed by design; cloud is
  opt-in holidays only.
- **E2EE as a pillar**; libsodium as the one crypto dependency; the old
  hardcoded-secret design is burned and stays burned
  ([security](/concepts/platform/security.md)).
- **Workspace shell built on upstream primitives** (splitter + touch metrics +
  the blessed InteractionCombinators pattern); no docking framework is coming
  — per Void Maiz's Q11 ruling (proposed, awaiting their author's
  confirmation; we build against the primitives either way).
- **Kept from the predecessor's plan because it was right**: the model mapping
  (block/event/contact = rune, newsletter = mantle, Antfarm node = holiday,
  `.miga` v2 = holiday registry), the undo boundary (owned mantle state
  undoable; holiday writes logged, not undoable), the compute boundary, tag
  hygiene as temper, and the public-repo posture.
