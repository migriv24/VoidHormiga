# Driving Void Hormiga from an agent

**For an AI agent working in some other folder**, told something like *"analyze
these emails and fliers, add them to my outreach database, then make me a
newsletter."* You do not need this repository's source, a compiler, or a window.
You need one binary and this page.

Everything below was executed against the real binary on 2026-08-18. The
transcripts are copied from that run, not imagined.

---

## 1. The one thing to know first

**Hormiga is a database with a dispatcher in front of it.** Every change — a
contact edit, a tag, a newsletter block — is a logged, replayable command. There
is no other way in, which is exactly why an agent can drive it: *your* command
and a person's click produce the identical transcript entry.

The headless binary is the same application with no window attached. It opens
the same document the person's GUI opens, so **your work is there when they next
launch it.**

## 2. Setup

**A database is a folder.** Inside it sits one state document — `<name>.state.json`
— and everything the organization owns hangs off that folder beside it:
`assets/`, `site/`, `exports/`, `backups/` and the rest.

**Always name the document. It is the whole of the setup:**

    voidhormiga-cli --state /path/to/TheirOrg/theirorg.state.json --describe

`--state` sets both the document *and* the folder, so it does not matter where
you run from. The binary is at `<hormiga-repo>/build/bin/voidhormiga-cli.exe`.

**If nobody told you where the database lives, ask.** Do not guess: with no
`--state`, the tool falls back to `demo-org.json` in your current directory,
and if that does not exist it creates an empty one. It now says so —

    note: no database in this folder - creating an EMPTY demo-org.json in
            C:\wherever\you\happened\to\be
          To open an existing one instead, name it:  --state <path-to>.state.json

— but a note you did not read is the same as no note. Working in a second,
empty copy of somebody's database is the most expensive mistake available here;
it once cost two days of edits made against the wrong file.

**To start a new database**, point `--state` at a path that does not exist yet
and build it up. The folder is created for you:

    cd anywhere
    CLI="voidhormiga-cli --state /path/to/NewOrg/neworg.state.json"
    $CLI mantle new neworg          # the org's data mantle
    $CLI use neworg
    $CLI rune new contact ana
    $CLI set ana display_name "Ana Ruiz"

That leaves `neworg.state.json` and its log in `/path/to/NewOrg/`, and every
folder Hormiga later writes — `site/`, `exports/`, `assets/` — lands there too.
There is no separate "create database" verb: a document is the mantles you put
in it.

`--describe` prints a JSON briefing: every verb, every glyph with its real
fields, the mantles that exist, the actions, the Allomone predicates, the
effects with their consequences, and a pointer to the OKF design bundle. **Read
it before writing anything.** It is generated from the running application, so
it cannot go stale the way this page can.

### Where the database's folders are, and how to move them

You do not have to `cd` anywhere. `--state` takes the document and the whole
database moves with it:

    voidhormiga-cli --state /path/to/RivertonNetwork/riverton.state.json <command...>

Everything an organization owns then hangs off **that document's folder**, not
off yours: `assets/`, `tiles/`, `site/`, `exports/`, `backups/`, `documents/`,
`templates/`, `fonts/`. One database, one folder, its own everything. Running
in the wrong directory is how you get a second empty database; `--state` is how
you stop having to think about it.

Any one of those folders can also be moved on its own (2026-09-01):

    voidhormiga-cli config set paths.tiles  "D:/map-cache/lane-county"
    voidhormiga-cli config set paths.assets "../shared-photos"
    voidhormiga-cli config get paths.tiles          # read it back
    voidhormiga-cli config set paths.tiles ""       # unset: back to the default

An absolute path is used as given; a relative one is relative to **the
database**, never to your working directory, so the setting means the same
thing however the tool was launched.

**Unset means `<database folder>/<the name itself>`** — that is the whole rule,
and it is why the defaults need no configuration. For a database at
`/path/to/TheirOrg/theirorg.state.json`:

| key | default when unset | holds | backed up into a `.miga`? |
|---|---|---|---|
| `paths.assets` | `/path/to/TheirOrg/assets` | originals that exist nowhere else | **yes** — the only one |
| `paths.tiles` | `/path/to/TheirOrg/tiles` | map tile cache | no — re-fetchable |
| `paths.site` | `/path/to/TheirOrg/site` | the built website | no — re-rendered |
| `paths.exports` | `/path/to/TheirOrg/exports` | rendered newsletters, map PNGs | no — re-rendered |
| `paths.backups` | `/path/to/TheirOrg/backups` | `.miga` bundles | no |
| `paths.documents` | `/path/to/TheirOrg/documents` | exported document scripts | no |
| `paths.templates` | `/path/to/TheirOrg/templates` | user starter templates | no |
| `paths.fonts` | `/path/to/TheirOrg/fonts` | the org's own webfonts | no |

Folders are created when something first writes to them, so a database folder
that shows only a `.state.json` is normal, not broken.

The column that matters: **`assets/` is irreplaceable and everything else is
derived.** That is why pointing `tiles/` at a scratch disk costs nothing, and
why moving `assets/` somewhere the backup cannot reach costs everything.

**A `.miga` carries every file the model points at**, not just `assets/`
(fixed 2026-09-03). If a `download.file`, a `hero.image`, an `audio.src` or a
`hero.portrait` names a path anywhere under the database folder, `pack` bundles
it and says so:

    [info] pack: bundled papers/annual-report.pdf - a file a block points at
           from outside the assets folder

Before this, only `assets/` travelled, so a resume in `resume/` was silently
absent from every bundle and the block reported "This file is not available"
when the bundle was opened elsewhere. **Read those lines** — they tell you
where an organization's originals actually are, which is often not where
anybody thinks.

Two things it will not bundle, both deliberate: a path **outside** the
database folder (an absolute path has no key the opener could restore it to),
and anything that looks like a credential (`.key`, `.token`, `*secret*`, a
`.state.json`, a `.bkp`). A `.miga` is **not encrypted**, so a deploy token in
one would be a file people hand each other with a live credential inside.

`assets/` is still the right place for originals. This is a safety net for the
paths that end up elsewhere, not a licence to scatter them.

## 3. The shape of the data

| term | what it is |
|---|---|
| **mantle** | a namespace. `demo-org` is the organisation's data; each newsletter or website is its own mantle |
| **rune** | a record — a contact, an event, a newsletter block |
| **glyph** | a rune's type (`contact`, `event`, `organization`, `hero`, `event_grid`, …) |
| **tag** | a label. **Namespaced by convention**: `type:contact`, `lang:es`, `status:active` |
| **link** | a relation between two runes |

**`link` and `relate` are two different verbs writing two different things.**
Both work. Neither is a trap. The confusion is that their inspectors are named
so similarly — `links` and `related` — that it is easy to run the wrong one and
believe the answer.

    link a b --relation 3:1        # a directed EDGE in the mantle's graph
    links a                        # ...and this is what shows you those edges
    relate a b                     # an undirected association between two TAGS

| | writes | reported by | use it for |
|---|---|---|---|
| `link` | a directed edge — `{from, to, relation, weight, directed}` | **`links <rune>`** | the graph: Antfarm wiring, anything with ports |
| `relate` | tag proximity, undirected | `related <tag>` | "these two labels go together" |

**`related` is a TAG verb, not a rune verb** — its own usage line says
`related <tag>`, and it reads tag proximity that `relate` wrote. `links <rune>`
is the verb that reports edges, and it reports them correctly in both
directions.

> ### ⚠ The trap here, and it is ours
>
> `related <rune>` answers `(no neighbors)` for a rune that has edges, because
> it looked for a tag by that name and did not find one. An earlier version of
> this guide recommended `related` as a verification step and never mentioned
> `links` at all — so on 2026-09-02 an agent wrote a correct `link`, ran the
> check this document told them to run, was told the rune had no neighbours,
> and filed it as a defect in `link`. The edge had been there the whole time.
>
> **Verify an edge with `links <rune>`.** The state file's `layout.edges` is the
> same information if you would rather read it directly.
>
> Void Core 0.2.13 (2026-09-02) added the signpost we asked for, so on a current
> build the empty answer tells you which verb you wanted:
>
>     (no tag neighbors; 'click-dns' is a rune with 1 link - try `links click-dns`)
>
> It still reports `ok: true` -- it found what it was asked for, which was zero
> tag neighbours -- so do not branch on `ok` to detect this.

`relate` takes **no flags** and its optional third argument is a *weight*, a
number. Before Void Core 0.2.13 a flag there landed in the weight slot and was
read as `0.0` — so `relate a b --relation friend` wrote the association at
weight zero, "not near at all", the inverse of the intent, and reported success.
It refuses now, which is what you will see:

    relate: weight must be a number, got '--relation'
    (usage: relate <tagA> <tagB> [weight]; `relate` takes no flags)

If the *kind* of association matters, use `link`. If you have a database that
predates 0.2.13 and something ran `relate` with a flag, those pairs are sitting
at weight 0 — `related <tag>` shows the weights.

**A gap that was here until 2026-09-02, in case you are on an older build:**
`link`, `links`, `unlink` and `journal` were all missing from the flat `verbs`
string that `--describe` prints -- so an agent reading the briefing would have
concluded four verbs did not exist, including the one this section tells you to
use. The rest of the briefing is introspected from the running application's own
registries (glyphs, fields, actions, predicates, effects) and was always
trustworthy; that one list was hand-maintained. Void Core 0.2.13 fixed all four
and added a CI test that diffs the list against the router in both directions,
so the next omission fails a build instead of costing somebody an hour.

The `--relation` value on `link` is a **port pair**, `i:j` — output port *i* of
the first rune into input port *j* of the second. `3:1` is what the Antfarm’s own nodes use.

### Three traps that cost real time

> ## ⚠ Trap zero: the organization's data goes in `demo-org`, whatever the org is called
>
> This one cost a client an hour on 2026-09-02 and every step of it reported
> success. `demo-org` is not a placeholder you are supposed to rename — it is
> the mantle name every block query, `effect query` and the published index
> reads, hardcoded. Naming a fresh data mantle after the organization is the
> obvious first move, and it is the wrong one:
>
>     mantle new theirorg          # ← every command below will succeed
>     rune new image img-cover
>     tag img-cover +type:image +cover
>     effect render-site their-site    # ← "ok", every gallery empty, no asset staged
>
> `validate` said `valid`. Nothing warned. What eventually cracked it was two
> verbs that should agree disagreeing: `ls --tag` found the rune (it projects
> the *active* mantle) and `effect query` found nothing (it projects
> `demo-org`).
>
> **Put the organization's contacts, events, images and resources in
> `demo-org`.** Documents — each newsletter, each website — are their own
> mantles and are named whatever you like. A render now warns when data-shaped
> runes are sitting in a mantle nothing reads, and names the fix
> (`mantle rename theirorg demo-org`), but the warning is a safety net and this
> paragraph is the instruction.
>
> (Whether an organization's namespace should permanently be called `demo-org`
> is `okf/developer_questions.md` Q57. It reads strangely and everyone knows.)

**Tags are namespaced, and queries match them exactly.** The convention is
`type:event`, not `event`. A newsletter block whose query is `type:event` finds
nothing if you tagged the runes `+event`. Tags are cheap — apply both if unsure:

    tag posada-2026 +type:event +community-outreach

> ## ⚠ Use `effect query` to check any expression
>
>     voidhormiga-cli --allow-effects=query effect query '<expr>'
>
> It is right for **every** expression, it is the same evaluation the renderer
> runs, and it changes nothing. `ls --tag` is right only for expressions with no
> `date:` term in them, and **it does not say so when it is wrong** — it returns
> a confident, quietly different answer. `find` is not a query language at all.
>
> If you read nothing else in this section: check with `effect query`.

**`ls --tag` is the query language. `find` is not.** This distinction cost a real
run an hour and produced a worse newsletter, so it is worth being blunt:

| | what it does |
|---|---|
| `ls --tag '<expr>'` | the **filter grammar**: `AND` `OR` `NOT`, parentheses, **exact** tag matching |
| `find <text>` | a substring search over names and tags. It **ignores** `AND`/`OR` — they are just more text |

    voidhormiga-cli ls --tag 'type:event AND month:june'      # 1 event
    voidhormiga-cli find 'type:event AND month:june'          # every event: the
                                                              # expression is ignored

`find` returns `ok` either way, so an expression handed to it produces a
confident wrong answer. **Use `ls --tag` to check what a block query will
select** — it is the same grammar and the same exact matching that the renderer
uses, so what you see is what the newsletter will contain.

**One exception, added 2026-08-28: `ls --tag` does not know what day it is.**
Block queries may now say `date:future`, `date:past`, `date:today`,
`date:recurring` and `date:undated` (§8, "Say 'the ones that have not happened
yet'"). Those are computed by Hormiga at render time; `ls --tag` is Void Core's
verb over Void Core's grammar and cannot see them, so it will quietly answer a
different question.

    voidhormiga-cli --allow-effects=query effect query 'type:event AND date:future'

Same evaluation the renderer and the Builder preview run, over the same data.
It prints each hit's date verdict beside it, and it changes nothing.

| checking… | use |
|---|---|
| a pure tag expression | `ls --tag '<expr>'` |
| anything mentioning `date:` | `effect query '<expr>'` |
| anything you are about to paste into a block | `effect query '<expr>'` — it is right in both cases |

**A rune name is a slug, not prose.** Rune names are lowercase, hyphenated and
ASCII, because they are command arguments. What gets *printed* comes from a
field:

| glyph | the field that is printed |
|---|---|
| `event` | `title_en` / `title_es` (blank falls back to the humanized name) |
| `contact`, `organization` | `display_name` |

Without them the page prints the slug: `ACME` renders as "Acme", an internal
`-aug` suffix leaks into a headline, a typo in a slug becomes a typo in
published prose, and `jose-garcia` is not how José García writes their name. Set
`display_name` and `title_en` on anything a reader will see.

**A field that is not in the glyph's declared list is stored but never
displayed.** `set` accepts anything and reports success; the value simply never
reaches the UI, the newsletter or the website. So take field names from
`--describe`, never from intuition. `event` has `date`, `venue`, `summary`,
`start_time` — *not* `location`, which looks obvious and silently does nothing.

## 4. Doing work

**Always use `--script`.** Put the commands in a file and run them as one atomic
batch. It either all lands or none of it does, the person gets **one** thing to
review and **one** undo, and — the reason this is not merely a preference —
**quoting only works correctly this way.**

    voidhormiga-cli --script work.txt --atomic --actor "claude"

`--actor` is how the log records that it was you. Always pass it.

### Quoting, and the one that will silently eat your data

Inside a `--script` file, wrap any value containing a space in **single quotes**;
a literal apostrophe is a backslash before it. This is correct, and UTF-8 (n-tilde,
accents, inverted punctuation) round-trips intact:

    set masthead title_en 'Riverton Community Network - August 2026'

**On the command line, that same line silently stores only the first word.** The
shell strips the quotes, the CLI re-joins its arguments with spaces and re-parses
them, and by then the quoting is gone:

    $ voidhormiga-cli set masthead title_en 'Riverton Community - August'
    masthead.title_en = Riverton        <-- no error, no warning, first word only

If you must run a one-off containing a space, **double-quote the single quotes**:

    $ voidhormiga-cli set masthead title_en "'Riverton Community - August'"

This is a real defect in the headless front-end, reported upstream 2026-08-18.
It is also the best argument for `--script`: one path, correct, reviewable,
atomic.

One more, for generated values: if a value might **end in a backslash**, close
the quote before it (`'C:'` then the backslash), otherwise the argument never
closes and the rest of the line is swallowed with no error. (Void Core SPEC 6.1;
four implementations have got this wrong.)

## 5. Let the person review it

This is what makes unattended work acceptable, and it costs you nothing:

    voidhormiga-cli status      # added 3, changed 0, removed 0
    voidhormiga-cli diff        # what exactly changed
    voidhormiga-cli revert      # throw all of it away

Your changes sit as **unsaved changes against the person's baseline** until they
accept them. That is deliberate, and it is why you can work without asking
permission for every edit.

**Do not dispatch `save`.** It snapshots the baseline and erases exactly that
diff, making your work indistinguishable from the person's own. The session
persists the document by itself; you never need the verb.

There is also a journal — `demo-org.json.log` — with every command, who ran it
and when. It outlives the process, so a person can read what happened days later.

## 6. Effects: the one-way doors

Building a newsletter or a website is an **effect**, and effects are **refused by
default**:

    $ voidhormiga-cli effect render en outreach-dec-2026
    refused: `effect` reaches outside the document and this session was not
    granted effects (render: writes a file beside the database; nothing is sent
    to anyone). Re-run with --allow-effects=render if that is intended, or
    --dry-run-effects to rehearse it.

That is not an obstacle to route around. Everything inside the document is
reversible; an effect is where that stops being true. **Ask**, then:

    voidhormiga-cli --allow-effects=render effect render en outreach-dec-2026
    voidhormiga-cli --dry-run-effects   effect render en outreach-dec-2026

- `effect render <lang> <document-mantle>` — the newsletter, as HTML
- `effect render-site [<lang>] <document-mantle>` — the website, into `site/`

Two more that read rather than write, and are worth running before the ones that
do: `effect check-host [<host-node>]` rehearses a publish and
`effect translation-report [<lang>]` says how much of the database exists in
each language, writing the script that closes the gap. Both are under §8
("Publishing, and the history it writes" and the `_en`/`_es` note). They are
effects because they reach outside the document — one talks to a vendor, the
other writes a file — and both change nothing.

**Name the document.** There is no default worth guessing: the GUI's "currently
open newsletter" is view state that does not exist headless, and publishing the
wrong newsletter is not something `revert` can fix.

**On `render-site`, the language is OPTIONAL and you usually want to omit it.**

    effect render-site outreach-dec-2026        ← builds EVERY language
    effect render-site es outreach-dec-2026     ← builds Spanish only

A website is **one folder carrying every language**, and `deploy-site` uploads
the folder. So a site built in one language and published is a site with a stale
half — which is invisible from the page you check afterwards, because that page
is the one in the language you read. That is not hypothetical: it put a stale
Spanish page in front of a real community on 2026-08-20.

Naming a language is for the preview loop, when you are iterating on one page
and want it fast. **Never name one and then publish.** `deploy-site` refuses if
the languages are missing or were not built in one pass, but do not rely on
that — build them all.

## 7. Building a newsletter

A newsletter is **its own mantle** of block runes laid out on a **grid**. Rows
stack top to bottom (`row`, 0 = top); a row is 12 units wide, so a block has a
start column (`col`, 0–11) and a width (`span`, 1–12). Full width is
`col 0, span 12`.

This is the transcript from the 2026-08-18 verification run — it renders:

    mantle new outreach-aug-2026
    rune new hero masthead
    set masthead title_en 'Riverton Community - August'
    set masthead title_es 'Red de Riverton - Agosto'
    set masthead row '0'
    set masthead col '0'
    set masthead span '12'
    rune new section_header h-events
    set h-events title_en 'Upcoming Events'
    set h-events row '1'
    set h-events col '0'
    set h-events span '12'
    rune new event_grid ev
    set ev query 'type:event AND month:august'
    set ev row '2'
    set ev col '0'
    set ev span '12'
    rune new footer pie
    set pie text_en 'Springfield community outreach.'
    set pie row '3'
    set pie col '0'
    set pie span '12'

**No links.** Order is the `row` number, not a chain of edges.

**`mantle new` makes the new mantle active.** After building a newsletter you are
"in" the newsletter, so a following `ls --tag` or `rune new` lands there and not
in the data. Say `use demo-org` before going back to the contacts and events.

Four things that matter:

- **`_en` / `_es` suffixes.** Hormiga is bilingual by design. Fill both where you
  can; the render picks by language, and falls back to the other language rather
  than showing a reader a blank.

  > **There is no way to publish one language, and that is deliberate.** It was
  > asked for on 2026-09-02 and the author declined it: *"instead of opting to
  > NOT have multi language, the ask should've been 'have better and more robust
  > translation tools' … We want MORE features, not less."* Do not propose a
  > `site.languages` setting; propose better tooling.
  >
  > What exists instead: a render now tells you how much of the page it built
  > was actually written in the language asked for, and
  >
  >     voidhormiga-cli --allow-effects=translation-report \
  >         effect translation-report es
  >
  > writes `exports/translate-es.hormiga` — every gap as a
  > `set <rune> <field>_es '<the English text>'` line with the source already in
  > place, grouped under the `use <mantle>` that makes it apply. Edit the
  > right-hand sides, delete the lines you do not want, and replay it:
  >
  >     voidhormiga-cli --script --atomic --actor "<who translated>" \
  >         exports/translate-es.hormiga
  >
  > **Leaving a line unedited writes English into the Spanish field**, which is
  > worse than the fallback it replaces — the fallback is at least honest about
  > which language it is showing. The file says so at the top; delete rather
  > than pass through.
  >
  > If you are translating on the organization's behalf rather than transcribing
  > a translation somebody gave you, **say so in your summary.** Publishing a
  > machine translation of an organization's own words under its name is a thing
  > a person should get to approve.
- **`query` is a tag expression** (`AND`, `OR`, `NOT`, parentheses, exact
  matching), plus the `date:` predicates (§8). The block fills itself from the
  database *at render time*, so the newsletter stays correct as the data changes
  — much better than writing the event list out by hand. **Check it first with
  `effect query '<the same expression>'`** (§3): that is the same evaluation the
  renderer runs, dates included. `ls --tag` is fine too, but only for an
  expression with no `date:` in it.
- **Two blocks side by side** is what the grid buys you: give them the same
  `row` and split the 12 units (`col 0 span 6`, `col 6 span 6`).
- **Rows may be sparse and need not be contiguous**; leave gaps if you want a
  person to slot something in later.

### Every block there is

`rune new <glyph> <name>` in a document mantle. Fields are listed without their
`_es` twins; `--describe` prints the full declaration of each, and is the
authority if this table and the binary ever disagree.

| glyph | what it is | its main fields |
|---|---|---|
| `hero` | the masthead | `title_en`, `subtitle_en`, `image`, `portrait`, `image_filter`, `image_dim` |
| `section_header` | a heading between blocks | `title_en` |
| `narrative` | prose, line breaks kept | `heading_en`, `text_en`, `icon` |
| `image_text` | an image beside words | `image`, `side`, `heading_en`, `text_en`, `alt_en`, `icon` |
| `event_grid` | events chosen by a query | `query`, `detail`, `columns`, `limit`, `sort`, `search` |
| `event_feature` | one event, large, with its image | `event`, `image`, `height`, `cta_en`, `cta_link` |
| `event_flier` | one event and its flier | `event`, `flier`, `display` |
| `job_grid` | job postings chosen by a query | `query`, `detail`, `columns`, `limit`, `sort` |
| `image_grid` | images chosen by a query | `query`, `columns`, `display`, `fit`, `limit` |
| `directory` | people or organizations, clearance-gated | `query`, `kind`, `display`, `limit`, `live` |
| `calendar_embed` | a month of events | `query`, `mode` |
| `map_embed` | a map view | `view` |
| `video` | a pasted video link | `url`, `poster`, `ratio` |
| `audio` | a sound file | `src`, `title_en`, `artist`, `duration`, `cover` |
| `download` | a file a visitor keeps | `file`, `label_en`, `download_style`, `platform` |
| `link` | a link or button | `label_en`, `target`, `link_style`, `platform` |
| `quote` | a pull quote | `text_en`, `author` |
| `stat` | a number with a label | `number`, `label_en` |
| `divider` | a rule between blocks | `divider_style`, `colors`, `bar_height` |
| `footer` | the closing line | `text_en` |

Every block also takes `row`, `col` and `span`, and most take `caption_en`.

### If you meet an older document

Some documents predate the grid and instead chain blocks with
`link a b --relation 1:1` and position them with `setjson <block> pos [x,y]`.
That is the **legacy** layout. Read it, do not extend it — and do not use it for
anything new. `--describe` lists a `migrate` action whose job is precisely to
convert a chain into grid rows, so writing a new chain means creating work that
already has an undo button.

## 8. Making it look like something a person would send

Added 2026-08-19, after an agent shipped a real issue and the person who
received it said, correctly, that it was a wall of text. Everything here is a
declared field — check `--describe`, and note that `hints.labels` on each glyph
carries the accepted values (that is where `band_bg`'s
`none/tint/accent/card/dark/gradient` is written down).

### Links work in email now, and prose is linkified

A `link` block renders in the newsletter as a proper email button:

    rune new link cta
    set cta label_en 'Reserve a seat'
    set cta label_es 'Reserva un lugar'
    set cta target 'https://example.org/rsvp'
    set cta link_style 'button'      # or 'text'
    set cta row '5'

`target` may be a URL, a `mailto:`, a bare `someone@example.org`, or a page
slug. And **you do not need a link block for a URL you wrote into prose** — a
bare `https://…` or email address inside `narrative`, `footer`, a caption or an
event `summary` becomes a link automatically.

> **This was true of the newsletter only until 2026-09-02.** The website
> rendered the same field with the URL as plain text, which meant this
> paragraph was half false and the half it was false about was the half most
> people look at. It is true of both now. If you are reading an older copy of
> this guide beside an older binary, check the output rather than the sentence.

### A `narrative` keeps the line breaks you typed

Also 2026-09-02, and also previously true of the email only. Newlines inside a
`text_en` survive to the page:

    set intro text_en 'Three things this month:

    - The posada is on the 14th
    - The clothing drive needs volunteers
    - Office hours move to Thursdays'

One block, four lines. Before this, eighteen lines in one field rendered as one
line on the website, and the only way to get a break was one block rune per
line — a real site shipped two eighteen-track lists as single hyphen-joined
paragraphs because of it. Runs of spaces still collapse (that is `pre-line`, not
`pre`), so pasted text behaves.

### Say how much of each event to show

The wall of text was mostly one block rendering every match in full:

    set ev detail 'compact'    # default: heading, date/time/venue, ~140 chars
    set ev detail 'title'      # one line each — for a long list
    set ev detail 'full'       # the whole summary — for ONE featured item
    set ev limit '4'           # at most four
    set ev sort 'date'         # default; also 'date-desc' or 'name'
    set ev columns '2'         # cards side by side, 1-3 (2026-09-14)

**`columns` works on `event_grid` and `job_grid`, in both the newsletter and
the website.**

- **Blank** means what each always drew: one card per row in email, and an
  automatic grid on the site.
- **`2` suits `compact` or `title` cards.** `3` only suits short titles: an
  email is 620px wide, so each card gets about 190px.
- **A job grid with `detail line` ignores `columns` in the newsletter**, since
  one line per posting is the point of that form.
- **A phone always stacks** the website's cards.

### Give the newsletter its own look

New 2026-09-15. The newsletter has a theme of its own, separate from the
website's. It has three parts and a preset:

    config set newsletter.theme 'modern'     # classic modern bold editorial night
                                             # ocean sunset forest newsprint minimal
    config set newsletter.palette 'sand'     # classic website clean ink sand night
                                             # ocean sunset forest paper
    config set newsletter.type 'editorial'   # classic | modern | editorial | geometric | impact
    config set newsletter.shape 'soft'       # classic sleek soft sharp bubbly ruled
    config set newsletter.accent '#0071e3'   # blank = the website's accent

- **A preset sets all three parts.** Setting one part yourself overrides only
  that part, so `modern` with `palette sand` is a real combination.
  - A blank part means "from the preset".
  - A blank preset is `classic`, the look every issue had before themes.
- **The presets:**
  - `modern` is clean white, the system sans-serif, large tight headings, soft
    rounded cards, pill buttons and an edge-to-edge banner.
  - `bold` is black and white, heavy uppercase headings, outlined square cards
    and square buttons.
  - `editorial` is warm paper, serif headings over sans-serif text and gently
    rounded cards.
  - `night` is a dark ground with light text and modern type.
- **None of this touches the website.** `theme.*` styles the site and
  `newsletter.*` styles the email. They share the accent colour only until you
  set `newsletter.accent`.
- **It is inline styles and system fonts.** Outlook desktop squares the corners
  and ignores background photos. Everything else falls back to a solid colour
  rather than breaking.

The Style window has a **Newsletter** tab with the same choices.

### Bands, banners and lists reach the newsletter

- **A hero's `image` is drawn in the email.** It was ignored before.
  - It sits above the title.
  - The `sleek` and `sharp` shapes run it edge to edge.
  - `portrait` is a round inset.
  - `image_filter` and `image_dim` still apply only on the website.
- **`band_bg` works in the email.** A band becomes a coloured table cell, and
  the blocks inside it are re-coloured to read on it:
  - `tint` and `card` are soft washes.
  - `accent` and `gradient` switch the text to read on the accent.
  - `dark` uses light text.
  - `band_image` puts a photo behind white text, with a dark fallback where the
    photo cannot load.
  - `band_full 1` runs the band edge to edge.
- **Lists in a `narrative` or `image_text`**, in both the website and the email:
  - a line starting `- `, `* ` or `• ` is a bullet;
  - a line starting `1. ` or `1) ` is a numbered item;
  - text without such lines renders exactly as before.

### Images in a newsletter: online, or clearly marked

An inbox can only show an image that has a link on the internet, which is an
image rune's `url`. How that link is made is the Antfarm's decision (2026-09-15):

- **An image host is an Antfarm node.** Three kinds can answer "put this file
  online and give me the link":
  - `hol_imgbb`: needs an ImgBB key.
  - `hol_object_store`: an S3 or Cloudflare R2 bucket. It needs `bucket`,
    `access_key_id`, the secret, and `public_url`, the address the bucket is
    served at. On Cloudflare that can be an `r2.dev` address or your own domain.
  - `hol_static_host` or `hol_github`: **your own website.** It needs no extra
    account, only `site.base_url`. The file is copied into `site/assets/`, and
    the link works **after the next publish** of the website.
- **Choose one**, or leave it automatic, which uses the first host that can
  answer right now:

      config set hosting.images 'site-host'   # a node name; blank = automatic

- **Put images online:**

      effect host-online <image-rune>          # one image
      effect host-online missing               # every image with a file and no link
      effect host-online missing <host-node>   # through a particular node

  - Each one needs `--allow-effects=host-online`.
  - It writes the link into `url` and the node's name into `hosted_by`.
  - `effect publish <image>` is the old name and still works.
- **Adding an image in the app puts it online automatically** when a host can
  answer. The Antfarm tab's inspector has a "Hosting images online" panel, and
  every image field says "online" or "not online yet", with a Host it online
  button.
- **The preview still draws an image that is not online yet**, outlined in
  dashed red under a PREVIEW notice. **Do not send an issue showing that
  notice.** Run `effect host-online missing` first.

### Pick the featured event

`event_feature` and `event_flier` name one event in `event`:

    set feat event 'fall-dinner'

In the app the Builder shows a searchable list, with upcoming events first.

### Order a list by tags

    set team rank_up 'leader, board'   # listed first, strongest first
    set team rank_down 'volunteer'     # listed last

- **It works on `directory`, `image_grid` and `job_grid`,** in both the website
  and the newsletter.
- **The block's normal order stays underneath** (names, for a directory) and
  breaks ties.
- **Earlier tags outweigh all later ones combined.** A leader on the board comes
  before a leader who is not, and both come before board members.
- **A tag without a namespace matches under any namespace:** `leader` finds
  `role:leader`.

### Say "everything except"

`NOT` works in a block `query` — it is Void Core's grammar all the way down, the
same one `ls --tag` evaluates. So a featured event does not have to appear twice:

    set feat query 'aug26 AND featured'
    set feat detail 'full'
    set upcoming query 'aug26 AND NOT featured'
    set upcoming detail 'compact'

### Give events a real title and a bilingual summary

    set network-meeting title_en 'Riverton Community Network Meeting'
    set network-meeting title_es 'Reunion de la Red'
    set network-meeting summary_en 'Monthly meeting. Parking is limited.'
    set network-meeting summary_es 'Reunion mensual. El estacionamiento es limitado.'

**Set `title_en`.** Without it the display title is the rune *name*, prettified —
which turns `naacp-open-house-aug` into "Naacp Open House Aug". The old
single-valued `summary` still works as a fallback, so nothing already in the
database breaks; but writing both languages into one field means the English
issue prints the Spanish too, which was 18% of a real newsletter.

### Times, colour, and the calendar

`start_time` / `end_time` render on the card (they used to appear only when
`date` was empty, so a dated event showed no time at all). `event.color` gives
each card its own accent bar, so a calendar of four organizations is not four
identical blue blocks.

`calendar_embed` defaults to **`mode agenda`** — a dated list, which for email is
a third of the height of a month grid and legible on a phone. `mode month` draws
the grid if you want a wall calendar; it centres on the month of the first event
rather than on today.

### Fliers that line up

    set fl display 'thumb'     # every image the same height, centred
    set fl columns '2'
    set fl limit '6'

Fliers are portrait scans next to square social posts; at the default they render
at their own aspect ratios and the section looks pasted rather than composed.

### Putting people on a website: `directory`, and the consent it requires

`directory` is the block that lists **contacts or organizations** — the member
carousel, the board page, the partner list.

    rune new directory net-members
    set net-members kind contact          # contact | organization | both
    set net-members display card          # card | list | carousel
    set net-members query 'type:contact AND role:board'
    set net-members caption_en 'Our board.'

**Its query is not sufficient, and cannot be made sufficient.** A rune reaches a
page only if it also carries `clearance:public`. There is no field, flag or
query that turns that off:

| tag | what it releases |
|---|---|
| `clearance:public` | the rune appears at all — name, role, bio, photo, website |
| `clearance:contact` | **additionally** its `email` and `phone` |

These are two independent annotations, not two rungs of a ladder. "List me" and
"print my phone number" are different consents; a contact can carry either
without the other.

This is deliberate and it will look like a broken block the first time you use
it. A real database held 84 contacts, 70 of them with a personal email — a
directory that trusted `type:contact` would have published a phone book, and it
would have done it the first time anyone wrote the obvious query. The render
tells you what it withheld:

    [info] render: directory net-members: 71 rune(s) withheld - only runes
           tagged `clearance:public` are published.

**Do not add the tag yourself to make a page look fuller.** Consent is the
person's, and the tag is the record of it. Ask, then tag what you are told.

### Other website-side things worth knowing

- **`hero` takes `subtitle_en` / `subtitle_es`** — the address or date line
  under the title, kept over the banner image where a separate `narrative`
  cannot go.
- **`event_grid` honours `detail`, `limit` and `sort` on the website too**, and
  reads `title_en`/`summary_en` exactly as the newsletter does. Set them once;
  both outputs agree.
- **`image_grid` filters by language.** A flier in two languages is **two
  `image` runes** with `+lang:en` / `+lang:es`; a picture with no `lang:` tag is
  language-neutral and shows in both. The rule generally: per-language
  *things* get sibling runes and a `lang:` tag, per-language *strings* on one
  thing get the `_en`/`_es` suffix.
- **`image_grid` takes `columns` and it is now read** (2026-09-02 — it was
  declared and ignored before, so column counts were being achieved by trial
  and error with `span`). Blank means the responsive default, which is usually
  what you want; `1`–`6` pins it. It applies to `display grid` only — masonry
  and a carousel lay themselves out, and the render says so rather than
  half-obeying you.
- **`divider_style bar` draws a coloured strip.** `line`, `dots` and `space`
  are still there; `bar` takes `colors` (comma-separated, blank = the theme
  accent) and an optional `bar_height` in pixels:

      set brandbar divider_style bar
      set brandbar colors '#ff5a5f,#3ddc97,#ffd166,#5b8def,#b47cf0'
      set brandbar bar_height 12

  Values that are not a hex colour or a plain CSS colour word are **dropped**,
  and the render tells you which — there is no such thing as a safely escaped
  arbitrary CSS value, so the seam validates rather than quotes. The email
  renderer draws `bar` as its ordinary rule.
- **`page` takes `social_title_en` / `social_title_es`** — the title a share
  card shows, which is not always the title in the nav. A home page reasonably
  called "Home" in a five-item menu shares to a feed as the word "Home", with
  no context. Leave it unset and `og:title` becomes "Home - Your Organization",
  which is better than the bare word; set it and you get exactly what you wrote.
- **The lightbox caption comes from the image's `description`, then `alt`** —
  set one, or a reader gets the slug.
- **`config set site.base_url https://your-domain.org`** before deploying.
  Without it there is no `sitemap.xml` at all: a relative `<loc>` is invalid and
  search engines reject the whole file, so not writing one is the honest
  outcome. `og:url` and `og:image` are also omitted until it is set.
- **`config set org.logo assets/logo.png`** puts the organization's mark in the
  site header and uses it as the favicon. Add `org.logo_dark` if the light one
  disappears on a dark background.
- **`render-site` writes a root `index.html`** that sends a visitor to their
  language, plus `calendar-en.ics` / `calendar-es.ics`. You do not need to
  post-process any of it.

### Publishing, and the history it writes

`effect deploy-site <host-node> [<document>]` puts the built `site/` online
through an Antfarm host node — either a `hol_static_host` (a managed CDN)
or a `hol_github` (a Pages repository). It is refused by default, it is one
of two effects declared `reversible: false`, and **it records itself**:

    [info] deploy: publishing site/ to org-pages (cloudflare-pages)
    [info] deploy: https://example.org

A `deployment` rune lands in the `antfarm` mantle carrying the host, the URL,
the time, the document and `state: live` — and the previous live one is set to
`superseded` in the same batch. That is not bookkeeping for its own sake: it is
how a person answers *which version is up* without asking the vendor, and it is
in the `.miga` if the organization ever leaves that vendor.

**So a publish shows up in `status` and `diff`.** If you publish during
unattended work, say so in your summary — it is the one change a person most
needs to know you made, and unlike everything else you did, `revert` does not
reach it.

`effect rollback-site <host-node> <deployment-id>` asks the host to put a past
version back, and it refuses rather than guessing which version to restore. On
a `hol_static_host` it needs `rollback_cmd` on the node (the sibling of
`deploy_cmd`); on a `hol_github` it needs nothing — restoring is a ref move onto
the commit the `deployment` rune already holds.

#### Wiring a host: the command lives on the node

**Hormiga has no vendor endpoint compiled into it, deliberately.** A REST API
this code cannot execute even once is one it has no business asserting, and a
vendor's URL baked into a binary goes stale silently and needs a release to
correct. The Antfarm exists so a backend is *configuration*:

    rune new hol_static_host org-pages
    set org-pages provider 'cloudflare-pages'
    set org-pages account_id '<your account id>'
    set org-pages project '<your pages project>'
    set org-pages token_file 'cf.key'          # gitignored, beside the database
    set org-pages deploy_cmd 'npx wrangler@3 pages deploy "{site}" --project-name={project}'

Placeholders substituted into `deploy_cmd` / `rollback_cmd`:

| | |
|---|---|
| `{site}` | absolute path of the built `site/` folder |
| `{project}` | the node's `project` |
| `{account_id}` | the node's `account_id` |
| `{token_file}` | absolute path of the token file |
| `{config}` | a curl config file holding `Authorization: Bearer <token>` |
| `{deployment}` | **rollback only** — the host's own id for the version to restore |

**The token never reaches argv.** It is exported as `CLOUDFLARE_API_TOKEN` (and
`HORMIGA_DEPLOY_TOKEN`) for the duration of the call and cleared after, or — for
a curl-based command — written into the `{config}` file, which is deleted when
the call returns. Do not put a secret in `deploy_cmd`; anything on a command
line is readable in a process listing by anything running as that user.

A version note that costs an hour if you miss it: `wrangler@latest` requires
Node ≥ 22. Pin `wrangler@3` on Node 20.

**Zone id goes on `hol_dns`, not on the host** — a zone is a DNS object, and
attaching a custom domain is the registrar's work. Wire
`hol_dns.domain → hol_static_host.domain` with `link` so the host learns the
name through the port rather than duplicating an id that then goes stale on one
side:

    link org-dns org-pages --relation 3:1

#### Check before you publish: `effect check-host`

New 2026-09-02, and it is the first thing to run against new credentials and the
last thing to run before the one-way door:

    voidhormiga-cli --allow-effects=check-host effect check-host org-pages

    [info] host: ok   read theirorg/theirorg.github.io - default branch main
    [info] host: ok   this token can write to it
    [info] host: ok   branch gh-pages - does not exist yet - the first publish creates it
    [info] host: ok   GitHub Pages is not enabled yet - the first publish turns it on

It makes the smallest **real** reads a deploy performs. It deliberately does not
ask a vendor whether a token is "valid": a Cloudflare account-scoped token
answers `Invalid API Token` to `/user/tokens/verify` while working perfectly
against every account endpoint, so a green light from that endpoint predicts
nothing.

**Read all the lines, not the last one.** "Can I publish?" is four questions —
the credential, the repository or project name, whether the branch exists (not
existing is *fine* on a first publish), and whether the host is actually serving
that branch. Four different fixes, and only one of them is the token. A pass on
three and a fail on one is a specific instruction, not a failure.

#### Publishing to GitHub Pages

New 2026-09-02. The other deploy target, and usually the cheaper one to set up
because an organization tends to already have the account.

    rune new hol_github org-pages
    set org-pages repo 'theirorg/theirorg.github.io'
    set org-pages branch 'gh-pages'        # optional; gh-pages is the default
    set org-pages token_file 'gh.token'    # gitignored, beside the database
    set org-pages message 'Publish site'   # optional commit message

    voidhormiga-cli --allow-effects=render-site effect render-site their-site
    voidhormiga-cli --allow-effects=check-host  effect check-host org-pages
    voidhormiga-cli --allow-effects=deploy-site effect deploy-site org-pages their-site

The token is a **fine-grained personal access token** with `Contents: read and
write` on that one repository — add `Pages: read and write` and the first
publish turns Pages on for you — or a classic token with `repo`. It goes in
`token_file` beside the database, or in the vault as `token_key`, and it never
reaches a command line. A relative `token_file` (and `key_file`, `secret_file`,
`imgbb.key`) is looked for in the priority folder, then beside the `.miga` the
working copy was opened from, then in the `--state` folder -- the first two are
read from `<state>.local.json` beside the state document, a machine-local note
the GUI writes (Niche Tools > Where is this database?). When the file is
missing, the error lists every path it tried. `repo` accepts `owner/repo` or a pasted GitHub URL; a
bare name is **refused**, because guessing an owner publishes to somebody else's
repository.

Things worth knowing before you run it:

- **The publish is a mirror, not a patch.** The commit replaces the whole tree,
  so a page you deleted from the model disappears from the site. That is the
  intent; it also means anything a human put on that branch by hand is gone.
  Use a branch nothing else writes to.
- **A `.nojekyll` file is added for you.** Without it GitHub runs Jekyll over
  the branch and silently drops every path starting with an underscore. You do
  not need to create it and you should not.
- **A wired `hol_dns` node becomes a `CNAME` file.** GitHub reads that file, not
  a setting, as the authority on the custom domain — so if the organization has
  one, wire it (`link org-dns org-pages --relation 3:1`) or the publish will
  quietly move the site back to `*.github.io`.
- **Rollback needs no `rollback_cmd`.** The `deployment` rune's `vendor_id` is
  the commit sha, and `effect rollback-site org-pages <sha>` moves the branch
  back onto it without re-uploading anything.
- **Pages takes a minute to build.** The URL the deploy reports may 404 briefly.
  That is GitHub, not a failed publish; the log says so.

Cloudflare Pages and GitHub Pages publish the same `site/` folder through the
same `effect deploy-site`, and you can have both wired at once — in which case
**name the host**, because an unnamed deploy with two targets is refused rather
than guessed at.

**The person can do all of this too**, from the **Publish tab** in the desktop
app — build, preview, a green Publish with a confirmation, and the same history
with `view` and `restore`. That is not a coincidence to be maintained by hand:
the button, the command bar and your `effect` call are three callers of one
verb. If you ever find something you can do that the GUI cannot, that is a bug
worth reporting, not an advantage worth using.

### An organization's own two lines of CSS

New 2026-09-02, and the same pattern as `fonts/` above: drop a **`custom.css`
beside the database** and `render-site` stages it into `site/` and links it
*after* the built stylesheet, so it overrides. Delete the file and the next
render removes it from `site/` too, so undoing is deleting.

    # beside the database, not in the Hormiga repo
    printf '.site-head{backdrop-filter:none}\n' > custom.css

It exists because three two-line cosmetic defects were found on a live public
site on 2026-09-02 and each one was blocked on a Hormiga release — there was
nowhere for the person whose site it is to put two lines of CSS.

**It is a file and never a field, and that is the design.** A `custom_css` field
would be model data: it travels in the `.miga`, it merges between devices, an
import can write it, and it is a way to get authored text into a `<style>` block
on a public page — which is exactly what the `video` block refuses. A file
beside the database is the operator's own machine and their own hand, and a
`<link rel="stylesheet">` cannot execute anything whatever it contains.

So: do not propose a field for this, and if you are writing `custom.css` on
someone's behalf, say so in your summary. A stylesheet an operator forgot they
had is a very good explanation for a page that looks wrong six months later —
which is why the render logs the file when it finds one.

### An organization's own typeface

    config set theme.font_custom 'Mandali'

The `.woff2` files go in a `fonts/` folder **beside the database** (not in the
Hormiga repo), named `<family>-<weight>.woff2` — `mandali-400.woff2`,
`mandali-700.woff2`. `render-site` stages them and writes their `@font-face`
rules; the family goes first in the stack, so a missing file falls back to the
theme's own choice rather than to whatever the browser picks.

### The theme is a set of axes, and every one is a command

The Style tab in the app has no private state — each control writes one of
these, so what you set from a script and what a person sets with a slider are
the same change in the same log.

| command | what it does |
|---|---|
| `config set theme.accent '#2e6b4f'` | the brand colour |
| `config set theme.accent_lite '#a8cbb6'` | lighter partner (blank = derived) |
| `config set theme.accent_dark '#1d4230'` | darker partner (blank = derived) |
| `config set theme.contrast '1'` | readability floor: 0 off · 1 AA · 2 AAA |
| `config set theme.grid_gap '2'` | spacing step, 0 tight … 4 airy |
| `config set theme.grid_even '1'` | equal-height cards across a row |
| `config set theme.banner_filter '1'` | 0 none 1 mute 2 mono 3 warm 4 cool 5 blur |
| `config set theme.banner_dim '45'` | scrim over a banner photo, 0–100 |
| `config set theme.icons '1'` | icons beside dates, places and roles |
| `config set theme.font_custom 'Mandali'` | the org's own typeface (§ above) |

**You do not choose text colours, and should not try to.** The renderer computes
the label colour for every generated background from the WCAG contrast ratio —
a light accent gets dark button text automatically, a dark one gets light. If
you find text you cannot read on a rendered page, that is a bug to report, not
something to patch with a hex.

A hero or a band may override the site-wide banner treatment:

    set masthead image_filter mono      # theme | none | mute | mono | warm | cool | soft
    set masthead image_dim 70           # blank = the site default

### Dates, and what not to hand-format

Set `date` to an **ISO** string (`2026-08-19`) and let the renderer print it.
It becomes `Wed, Aug 19` on the English page and `mie, 19 de ago` on the
Spanish one, with the year shown only when it is not the current one. Writing
"August 19th" into the field instead means the calendar, the `.ics` and every
`upcoming` predicate stop working on that event.

A genuine recurrence (`Last Friday of the Month`) goes in `days` and is printed
verbatim — a recurrence is not an instant, and the renderer does not pretend
otherwise.

### The theme is one place

`config set theme.accent '#2e6b4f'` reaches the newsletter, the website and the
GUI's Style tab — they read the same config, so successive issues look like the
same publication.

### Say "the ones that have not happened yet"

Added 2026-08-28, and it removes the most common reason a Hormiga page goes
stale. A block query can now ask about time:

    set upcoming query 'type:event AND date:future'
    set fliers   query 'flier AND date:future'
    set archive  query 'type:event AND date:past'

| predicate | matches |
|---|---|
| `date:future` | it has not happened yet |
| `date:past` | strictly before today |
| `date:today` | today — **and it also answers `date:future`**, because an event at 6pm has not happened at 9am |
| `date:recurring` | no `date`, only a `days` recurrence — **and it also answers `date:future`**, because a standing meeting is always upcoming |
| `date:undated` | no date of its own and nothing dated wired to it: matches none of the above |

**Two things worth knowing before you use these.**

**A flier gets its date from its event.** An `image` has no date, so it borrows
from any `event` it is wired to — which means `flier AND date:future` works, and
works because somebody ran `link <flier> <event> --relation flyer-of`. **A flier
linked to nothing is `date:undated` and will not appear**, which is deliberate:
better a missing tile than a guess. If a flier you expect is absent, link it.

**A recurring event is never `date:past`.** An event with
`days 'Last Friday of the Month'` and no `date` is upcoming forever, which is
what a standing meeting is. Do not try to archive one with `date:past`; it will
never match.

These are computed fresh on every render, so **nothing goes stale and there is
nothing to re-tag.** If you find yourself maintaining a `site:current` /
`site:archive` pair by hand, delete it and write the predicate instead.

### A data rune's prose is bilingual too

New 2026-09-03. `directory` and `image_grid` publish an organization's *own*
content, and until now that content was the one text on a bilingual site that
could not be bilingual.

| glyph | bilingual fields | legacy field, still read |
|---|---|---|
| `contact` | `bio_en` / `bio_es` | `bio` |
| `organization` | `bio_en` / `bio_es` | `bio` |
| `image` | `alt_en` / `alt_es`, `description_en` / `description_es` | `alt`, `description` |

    set o-nomad bio_en 'A geometry-nodes experiment.'
    set o-nomad bio_es 'Un experimento de nodos de geometria.'

The legacy field is read **last**, so a database written before this still
publishes every word it had — but if you are writing new prose, write both
languages. The order is: the page's language, then the other language, then the
legacy field.

**Fields still waiting for this:** `job.description`, `resource.topic`, and the
civic set. If you need one, say so rather than writing the English into both.

### Do not translate an address

`effect translation-report` and the render's coverage line both skip values that
have no language — an email address, a URL, a phone number, a year. That was a
real problem: counting them made 100% unreachable, so the warning could never be
cleared, and **a warning that cannot be cleared is one people stop reading.**

If you have a value the heuristic does not recognise — a product name, a Latin
motto, a person's name used as a label — tag the rune `+lang:none` and it drops
out of the count entirely. Do **not** write the English into the `_es` field to
silence it: the file that `translation-report` writes says why, and the reason is
that the fallback at least tells a reader honestly which language they are
looking at.

### Publish a file a visitor can keep

New 2026-09-02. The `download` block puts a real file on the site — a flier PDF,
the bylaws, an annual report, a know-your-rights sheet, a printable calendar, a
resume.

    rune new download dl-bylaws
    set dl-bylaws file 'files/bylaws-2026.pdf'
    set dl-bylaws label_en 'Download our bylaws'
    set dl-bylaws label_es 'Descargar nuestros estatutos'
    set dl-bylaws caption_en 'PDF, adopted March 2026'
    set dl-bylaws download_style 'button'    # or 'card'
    set dl-bylaws row '3'

- **`file` takes either form**: a path relative to the database, **or the name
  of a `resource` rune**. The second is what finally makes `resource` mean
  something — it was declared and rendered by nothing until now — so if the
  organization already tracks the document as a rune, point at the rune.
- **The type and size are added for you** (`PDF - 12 KB`), read from the staged
  file. Do not put them in the label.
- **Some extensions are refused, loudly**: `.html`, `.htm`, `.svg`, `.js`,
  `.mjs`, `.wasm` and friends. A file served from the site's own origin can act
  with the site's own authority, and this field is model data an import or a
  merge can set. The render prints an error naming the file; it is not staged
  and no button appears.
- **A file that is not on this machine leaves no button**, and the render says
  which file and why. Check the render output — a missing Download button is
  hard to notice on a page you already know.
- **In the newsletter it becomes a link**, which needs
  `config set site.base_url https://your-domain.org`. Without it the block still
  renders (name, caption, "Available on the website") and the render warns —
  a relative href would open nothing in anybody's inbox.
- **`download_style card`** gives a wider card with the type badge, the caption
  and the size; `button` is the default and is right for a call to action.

There is deliberately **no `download_grid`**. Publishing one named file is a
different act from publishing a query result: naming the file is the consent,
whereas a query over an organization's documents needs the same clearance
conversation `directory` has. If you want several files, place several blocks.

### Offer the right download for the visitor's computer

New 2026-09-08. `download` and `link` both take **`platform`** — `windows-x64`,
`macos`, `macos-arm64`, `linux-x64`, or `any` (the default; leave it off and
nothing changes). Put **two or more of them on the same `row`** and that row
becomes a **platform set**: the visitor's own is moved to the front and marked
*"For your computer"* (*"Para tu computadora"* on the Spanish page).

    rune new link dl-win
    set dl-win label_en 'Download for Windows'
    set dl-win target 'https://github.com/migriv24/VoidHormiga/releases/latest/download/VoidHormiga-windows-x64-setup.exe'
    set dl-win link_style 'button'
    set dl-win platform 'windows-x64'
    set dl-win row '1'
    set dl-win col '0'
    set dl-win span '4'

    rune new link dl-mac
    set dl-mac label_en 'macOS - no build yet'
    set dl-mac target 'roadmap'
    set dl-mac link_style 'text'
    set dl-mac platform 'macos'
    set dl-mac row '1'
    set dl-mac col '4'
    set dl-mac span '4'

    rune new link dl-linux
    set dl-linux label_en 'Linux - no build yet'
    set dl-linux target 'roadmap'
    set dl-linux link_style 'text'
    set dl-linux platform 'linux-x64'
    set dl-linux row '1'
    set dl-linux col '8'
    set dl-linux span '4'

- **Nothing is ever hidden, and there is no way to make it hide anything.** All
  the cards are in the markup, in view, in both languages, and with JavaScript
  off the row is exactly the order you wrote. A page that silently offers a
  visitor nothing is indistinguishable from a broken page, so the block reorders
  and labels; that is all it does.
- **Use `link` for an installer and `download` for a file you host.** A link
  points at another origin (GitHub Releases); `download` stages a file into the
  site. Both carry `platform`. See
  `okf/concepts/platform/download-page.md` §3 for why a release binary does not
  belong in `assets/`.
- **Say on the card when a build does not exist.** The mechanism guarantees a
  Mac visitor sees the macOS card; only your copy can make it honest.
- **Only the operating system is detected, never the processor.** A browser will
  not tell you what architecture it is running on, so `windows-x64` and a future
  `windows-arm64` match the same visitor. The architecture is for you, the
  filename and the release.
- **One block with a `platform` on a row is not a set** — there is nothing to
  choose between — and it renders exactly as it always did.
- **Check the render output.** A `platform` value the renderer does not know is
  reported there and treated as `any`. A typo would otherwise give you a row
  that looks like a platform set and marks nobody's computer.

### Put a recording on the page

New 2026-09-02. The `audio` block plays a file the organization owns — a
recorded meeting, a podcast episode, a Spanish-language radio spot, a track.

    rune new audio sept-meeting
    set sept-meeting src 'assets/2026-09-meeting.mp3'
    set sept-meeting title_en 'September general meeting'
    set sept-meeting title_es 'Reunion general de septiembre'
    set sept-meeting artist 'Recorded by the office'
    set sept-meeting duration '48:12'
    set sept-meeting cover cover-image-rune     # optional, an `image` rune
    set sept-meeting caption_en 'Full audio, unedited.'
    set sept-meeting row '4'

- **`src` is a file, not a URL**, and it must be a file this machine has —
  relative paths resolve against the database folder, and `assets/` is where it
  belongs. `render-site` stages it into `site/assets/` like any image. In the
  desktop app the field's **Browse** ingests the file (content-hashed into
  `assets/`), so a person does not have to move it by hand first.
- **A file that is not there gets a sentence, not a dead player**, and the
  render names it: *"audio file 'assets/x.mp3' is not on this machine."* Check
  the render output rather than the page.
- **`duration` is text you write**, exactly as you want it shown. Nothing reads
  the file's length — decoding an arbitrary media container to print `3:42` is
  a codec dependency for a cosmetic string.
- **There is no facade and that is deliberate.** `video` uses a click-to-load
  facade because a YouTube iframe reports every visitor to Google before they
  press play; nothing here leaves the organization's own site, so the page
  ships a plain `<audio controls preload="none">` — no bytes fetched until
  somebody presses play, and it works with JavaScript off.
- **In the newsletter it becomes a card and a Listen button**, never a player:
  Gmail and Outlook both strip `<audio>`. The button needs an absolute address,
  so **`config set site.base_url https://your-domain.org` is what makes it
  work** — without it the card renders and says the recording is on the
  website, and the render warns.

**What it is not, yet:** an `audio` *data* rune. There is no audio equivalent of
an `image` you can tag, query and link to the event it was recorded at — that is
`okf/developer_questions.md` Q59, which asks whether a glyph per medium is the
right shape at all or whether organizations should be able to register their own
types. Do not add one on the assumption it is coming; ask.

### Put a video on the page

    rune new video youth-hub
    set youth-hub url 'https://youtu.be/dQw4w9WgXcQ'
    set youth-hub caption_en 'Youth HUB'
    set youth-hub caption_es 'Youth HUB'
    set youth-hub poster 'assets/youth-hub-still.jpg'   # optional
    set youth-hub ratio '16:9'                          # or 4:3, 1:1, 9:16
    set youth-hub row '6'

**Paste the address, not an embed code.** The share dialog's `<iframe>…` snippet
will not work and is not meant to — a field that took markup would be a way to
put script on a public page. Everything a person actually copies does work:
`youtube.com/watch?v=…`, `youtu.be/…`, `/shorts/`, `/embed/`, `/live/`,
`vimeo.com/…`, `player.vimeo.com/video/…`, and a bare video id.

YouTube and Vimeo only. Anything else renders an empty-state on the page and a
`[warn]` in the render log rather than a broken frame, so check the log.

**The page does not load the video until a visitor clicks it.** It ships your
poster (or a plain themed card) and swaps in the player on click, so nobody is
reported to the video host for merely reading the page. `poster` is one of the
organization's own images — nothing is ever fetched from the provider, thumbnail
included. In the newsletter the block becomes the poster plus a "Watch on
YouTube" button, because no mail client plays video.

### Read a flier, and check its date

    config set tools.image_text 'tesseract {path} - -l eng+spa'
    voidhormiga-cli --allow-effects=read-flier effect read-flier flier-relaunch

**There is no OCR in Hormiga.** This effect runs the recognizer *you* named and
turns its output into proposed commands — `{path}` is replaced with the image
file. Without that config key it does nothing and says so.

**It proposes; it never dispatches.** You get `tag <rune> +kw:<word>` lines to
review and run yourself, and — the part that matters — a check of the date
printed on the sheet against the date stored on the event it is linked to:

      [note] MISMATCH: the flier prints 2026-09-16 (read as "September 16th")
             but relaunch-sep stores 2026-08-19. One of them is wrong …

That is the failure that goes wrong silently, so read the `[note]` lines even if
you throw the keyword proposals away.

### Check before you send

The render reports what it made:

    $ voidhormiga-cli --allow-effects=render effect render en aug2026
      [info] render: 102 words, about 1 min read
      [warn] render: 2 image(s) have no public URL - they will render as
             'unpublished' boxes in email
    wrote .../exports/preview-en.html

**Read those lines.** The word count is how you find out it is too long before a
person does, and the unpublished-image warning is the difference between a
newsletter with pictures and one full of dashed grey boxes.

`render-site` has three more lines added 2026-08-28, and each one names a
failure that used to be invisible:

    [warn] render: flier-openhouse is on a page, and the event it is linked to
           was 9 day(s) ago. Either the flier belongs in an archive section, or
           its block wants `date:future` in the query …
    [warn] render: 3 image rune(s) matched an image_grid query but have no file
           on this machine, so they reach no page in any language: …
    [warn] render: a `video` block holds <…>, which is not a YouTube or Vimeo
           address this build recognises …

The middle one is the one to read most carefully. **The website is self-hosted**:
a rune whose `path` is empty, or points at a file that is not on this machine,
cannot be published in any language — and from the outside that looks exactly
like the language filter hiding it. If a tile you expect is missing, check this
warning before you conclude anything about `lang:` tags.

## 9. Publishing to AWS (or any S3-compatible store)

New on 2026-08-25. Read this before wiring an organization's hosting; the order
of the four effects below is the order to run them in, and the first one is a
check rather than a change.

**What this is for.** The website used to need a full render and a full site
deploy whenever one contact changed. It does not any more: the published
directory is a small artifact you can push on its own.

### The two things that may be pushed, and why it is not a path

`effect push-store` takes `index` or `backup`. It does **not** take a file path,
and that is deliberate — an effect that uploads an arbitrary path is a way to
send the unencrypted database, the vault or a token file to a bucket with one
command. If you need something else pushed, that is an edit to
`src/publish/push.cpp` by someone who has read the comment at the top of it.

| what | source | what the store can read |
|---|---|---|
| `index` | `site/index/*` | exactly what cleared the clearance seam |
| `backup` | `<document>.bkp` | **nothing** — it is encrypted before it leaves |

### Wiring it

One Antfarm node. `secret_key` names a secret in the vault; `secret_file` is the
alternative for an operator who would rather keep it out of the database.
**Never put a secret in a field** — a field is in the state document, and the
state document travels.

    use antfarm
    rune new hol_object_store org-store
    set org-store bucket 'my-org-index'
    set org-store region 'us-west-2'
    set org-store access_key_id 'AKIA...'          # not a secret
    set org-store secret_key 'aws.org-store'       # names a VAULT entry
    set org-store prefix 'v1'                      # optional

`endpoint` is blank for real AWS. Set it for an S3-compatible store —
Cloudflare R2, MinIO — and everything else is identical, because the signing is
the same. That is what keeps this from being an AWS dependency.

### The order to run things

    effect check-store                 # 1. can these credentials reach it?
    effect publish-index               # 2. build the cleared projection
    effect push-store index            # 3. send it (a few KB)

    effect backup-database             # and, independently:
    effect push-store backup           # HORMIGA_BACKUP_PASSPHRASE must be set

**Start with `check-store`.** It performs the smallest *real* operation against
the actual bucket rather than validating a credential in the abstract, so a pass
predicts a push. This rule exists because an operator once lost an afternoon to
a vendor's "is this token valid?" endpoint cheerfully answering yes for a token
that failed every useful call.

### Reading a failure

Every AWS error reaches the log **verbatim and unparsed**, and the code in it is
the whole diagnosis. The four you will actually see:

| code | what it means | what to change |
|---|---|---|
| `InvalidAccessKeyId` | the key does not exist | `access_key_id` |
| `SignatureDoesNotMatch` | the key exists, the secret is wrong | the vault entry |
| `AccessDenied` | both are right, the IAM policy is not | the bucket policy |
| `NoSuchBucket` | the bucket or region is wrong | `bucket` / `region` |

Note that AWS checks the key **before** the bucket, so a bad key and a
non-existent bucket both report `InvalidAccessKeyId`. That is AWS declining to
tell an unauthenticated caller which buckets exist, and it is correct of them.

If you ever see `SignatureDoesNotMatch`, AWS's response includes **the canonical
request it computed**. `hormiga::aws::Signed` exposes ours. Diff the two; the
first line that differs is the bug. That is a much better position than most
signing failures leave you in, and it is why the raw body is never summarised.

### What is proven and what is not

Signing is tested offline against FIPS 180-4 and RFC 4231 vectors, and the
canonical request is built field for field against the specification — so the
cryptography and the structure are not in doubt. **Whether AWS agrees has only
been checked as far as `InvalidAccessKeyId`**, which proves the request parses
and the credential was looked up, and does *not* prove the signature verifies,
because AWS rejects at key lookup first.

So: **the first real push with a real key is a measurement nobody has taken.**
If you are the agent taking it, report what happened — the vendor's exact words
if it fails — the same way the first Cloudflare deploy was reported. That report
found a one-word bug in a minute that would otherwise have cost an afternoon.

### The backup passphrase

`backup-database` reads `HORMIGA_BACKUP_PASSPHRASE` from the environment and
refuses without it. It is deliberately not an argument: a command line is
readable in a process listing by anything running as the same user.

**There is no recovery for a forgotten passphrase**, and there must not be — a
backup the developer could open is a backup a vendor could be compelled to
produce in readable form. If the backup is an organization's only remaining
copy, the passphrase is as important as the file.

To restore, use the flag rather than the effect:

    voidhormiga-cli --restore-backup mydb.json.bkp

An effect runs inside a session and a session needs a loadable document, so the
effect cannot help when the database is the thing that is broken. The flag runs
before any session exists, which is the only way a recovery tool is useful.

## 9b. Working from more than one device

New 2026-08-27. Full design in `okf/concepts/platform/collaboration.md`.

**The rule that makes all of this safe: these verbs REPORT by default and write
only when you add the word `apply`.** Run one, read the report, then run it
again with `apply` if the report says what you expected. A merge is the one
operation in this application that can lose somebody's work.

### What version am I?

    effect sync-version

Prints a cut name like `v:3ec5dc2f…`. Two devices that print the same string
hold the same data and have nothing to sync. This is the cheapest possible
answer to "am I up to date" and it needs no network.

### Merge another copy of the database in

    effect sync-merge ../other-folder/org.json          # REPORT ONLY
    effect sync-merge ../other-folder/org.json apply    # do it

Takes a state document or a `.miga` bundle. The report tells you three things
and you should read all three:

- **conflicts** — both people changed the same field to different values. Both
  values are shown with an address. Nothing is chosen for you silently; settle
  each one afterwards with an ordinary `set`.
- **runes arriving from the peer** — either they added them, **or you deleted
  them and this merge brings them back.** See the warning below.
- **runes only here** — they will reach the peer.

### On another machine, same Wi-Fi

    effect lan-peers 8                  # who is out there (read-only)
    effect lan-serve 60 apply           # wait for someone to connect
    effect lan-sync 10.0.0.42 apply     # connect to someone

Both sides print a six-character code. **Compare it with the other screen before
you trust the connection.** If the two codes differ, someone is between you —
stop. The code is not a password and you do not type it anywhere; it is checked
by two people looking at two screens. After the first successful pairing the
device is remembered and you will not be asked again unless its key changes.

### The one limitation you must know about

**Deletions do not propagate yet.** If you delete a contact and then merge with a
device that still has it, the contact comes back. The report names every rune
that arrives and warns you, so the fix is: read the arrivals, and re-delete
anything you meant to be gone. Then sync again so the other side agrees.

This is not a bug to work around quietly — it is the honest state of the merge
today (`collaboration.md` §2 explains why), and the report exists so it is
visible rather than silent.

### What does NOT travel

Your Antfarm wiring, your domains, and your `config` — including
`site.base_url` and any credentials. Sync moves the organization's data and
structurally cannot move a device's backends. Each machine keeps its own.

Assets (images) do not travel yet either. Only the state document does.

### Files this creates

`<database>.peerkey` appears beside the database: this device's identity. **It
contains a secret key.** Never commit it, never copy it to another machine, and
never put it in a `.miga` you hand to someone. It is gitignored.

## 9c. Is there a newer Hormiga?

New 2026-09-04. Full design in `okf/concepts/platform/distribution.md`.

**`update` is not an effect and does not run inside a session.** It touches no
document, takes no lock, and works in a folder with nothing in it — which is
deliberate, and the same reason `--restore-backup` is a flag: one perfectly
ordinary reason to want a newer Hormiga is that this one will not open the
database, and a recovery tool that requires a working system is not a recovery
tool. So it is spelled without `effect`:

    voidhormiga-cli update            what is installed, and the setting
    voidhormiga-cli update --check    ask the feed (a NETWORK REQUEST)
    voidhormiga-cli update --install  check, download, verify, launch

A bare `update` makes no network request. It prints the running version, the
platform, whether this machine has been asked about update checks, the feed URL,
and where that answer is stored.

### If you are an agent, read this part

**`--check` and `--install` are decisions belonging to the person who owns this
machine, not to you.** They are here so a person can run them in a terminal, and
so you can *answer a question about them* — "am I on the latest?" is a fine
thing to be asked. Two rules:

- **Do not run `--check` unless you were asked to check.** It is a network
  request the machine's owner did not make. If you are reporting on the state of
  an installation, a bare `update` tells you everything except what the feed
  says, and it costs nothing.
- **Never run `--install`, or change the preference (`--startup`, `--never`,
  `--feed`), unless the person told you to.** Installing software on somebody's
  computer is not a step in a newsletter task.

The desktop application asks a person directly and never checks until they have
said yes. The two surfaces print the same words about the same release, so
whatever you read here is what they will see.

### What "up to date" and its failures look like

    up to date (the feed's latest is 0.1.0).

    update check failed: the update feed answered HTTP 404

The second is the correct answer while no release has been published. **A failed
check is not a reason to retry in a loop** — report the sentence and move on.
The remote's own words are routinely the entire diagnosis, which is why they are
printed rather than flattened into "something went wrong".

### What it will never do

It will not update anything silently, it will not delete the version you are
running (each version installs into its own folder), and it will not run an
installer whose SHA-256 does not match the feed — a mismatched download is
deleted rather than left on disk. Nothing is signed yet, and the prompt says so.

## 10. Building it (if you are working on Hormiga itself)

Two failures cost this project a day each and both look like something they are
not.

**`collect2.exe: error: ld returned N` with no other output.** The gcc driver on
MinGW swallows ld's stderr entirely, so every link failure looks identical
whatever caused it. Two things to try, in this order:

1. **Close the app.** A running `voidhormiga.exe` locks the file the linker is
   trying to write. This is the single most common cause and it has been
   misdiagnosed twice, once as a PE section-count ceiling with convincing
   measurements. If the same link succeeds when you run it by hand, this is it.
2. **Run `ld.exe` directly** to see the real errors — take the `collect2` line
   out of `c++ -v` output, strip the `-plugin` arguments, keep the whole library
   tail (dropping `-lgdi32` and friends invents undefined symbols that are not
   your bug), and run it. Undefined symbols appear immediately.

A failure that reproduces *without* your change is not your change. Check that
before theorising about anything.

**Also locked by a running app: `libvoidcore.dll`.** The link can succeed and
the build still fail on `Error copying file (if different) …
libvoidcore.dll`, which is the staging step, not your code. Same fix: close the
app. Until you do, `build/bin/` keeps whatever version of Void Core it already
had, so a fresh upstream fix will appear not to have landed.

### The test suite, and the one failure that is not yours

    ctest --test-dir build --output-on-failure

**`reduce_conformance` fails at 17/25 and has for as long as anyone has looked.**
It is `maiz_reduce_conformance`, declared in `../VoidMaiz/CMakeLists.txt` and
pulled into our build by `add_subdirectory` — **Void Maiz's test of Void Maiz's
reducer, not ours**, and CLAUDE.md rule 4 says we do not patch upstream. Do not
spend an afternoon on it and do not "fix" it.

Everything else should be green. If one of these goes red, it is worth knowing
what it is actually telling you:

| test | what a failure means |
|---|---|
| `hormiga_layering` | a `domain/` or `render/` file included something from `app/`. The fix is usually that the file is in the wrong folder |
| `hormiga_file_length` | a file crossed its budget in `tools/find_long.py`. **Look for something to move out before raising the number** — twice now the ratchet was pointing at a real seam (`ui/documents.cpp`, `render/audio.hpp`) rather than at warranted growth |
| `hormiga_golden_render` | rendered output changed. If that was the point, `tools/golden_render.sh <abs-path-to-cli> capture`; if it was not, you changed something you did not mean to |
| `hormiga_headless_smoke` | a behaviour assertion. Each check names the report item or the date it came from, so the failing line says why it exists |
| `hormiga_replay_smoke` | phase B's exit test: a GUI transcript no longer replays to an identical state |

Pass the CLI to the two shell tests as an **absolute** path — they `cd` into a
temp directory and a relative one stops resolving.

### Building on Linux or macOS

New 2026-09-08, and **read the last paragraph before you conclude anything from
what you see.**

There is no Linux or macOS release: `void.json`'s `platforms` is a *shipping
record* and lists `windows-x64` alone, and Void Mago's `wizard` emits NSIS and
nothing else. So on these two the only way in is a source build, and this is it.

The family lives as **siblings on disk** — our CMake defaults are `../VoidMaiz`
and `../VoidPalabra`, and Void Maiz in turn finds `../VoidCore/core`. Reproduce
that layout, not the usual one-repo-one-folder shape:

    mkdir Projects && cd Projects
    git clone https://github.com/migriv24/void-core.git    VoidCore
    git clone https://github.com/migriv24/VoidMaiz.git     VoidMaiz
    git clone https://github.com/migriv24/VoidAllomone.git VoidAllomone
    git clone https://github.com/migriv24/VoidPalabra.git  VoidPalabra
    git clone https://github.com/migriv24/VoidHormiga.git  VoidHormiga

**On Linux, install the window-system headers first.** GLFW 3.4 builds both an
X11 and a Wayland backend by default, so configure wants both sets even though
you will only ever use one. This is the step a first-time Linux builder is most
likely to be missing, and the error it produces reads like a broken checkout
rather than a missing package:

    sudo apt-get install -y xorg-dev libwayland-dev libxkbcommon-dev wayland-protocols

**Build Void Core first.** It is a shared library — `libvoidcore.so` on Linux,
`libvoidcore.dylib` on macOS — and it has to exist before Hormiga configures:

    cmake -S VoidCore/core -B VoidCore/core/build -DCMAKE_BUILD_TYPE=Release
    cmake --build VoidCore/core/build

    cmake -S VoidHormiga -B VoidHormiga/build -DCMAKE_BUILD_TYPE=Release
    cmake --build VoidHormiga/build

The build stages Core's shared library beside both binaries and gives them an
`$ORIGIN` (Linux) / `@loader_path` (macOS) rpath, so they find it beside
themselves and stay movable. If that staging is skipped you get the failure
`libvoidcore.dll` produces on Windows and it is just as opaque: a green build and
a program that does nothing, with the loader saying at most `error while loading
shared libraries`. CMake warns at configure time if it could not find the
library — that warning is the thing to read.

`curl` must be on `PATH`. Every network call — publishing, the update check —
goes through it rather than a linked HTTP client, so a machine without it builds
fine and fails at the first deploy.

Then check the two things worth checking, in this order:

    ./VoidHormiga/build/bin/voidhormiga-cli update
    # → Void Hormiga 0.1.0  (linux-x64)     ← the platform tag, NOT "unknown"

    ctest --test-dir VoidHormiga/build --output-on-failure -E reduce_conformance

    ./VoidHormiga/build/bin/voidhormiga
    # → a window, with icons in the tabs

**What is proven and what is not.** `.github/workflows/ci.yml` builds all of
this — library, CLI, GUI and tests — on `windows-latest`, `macos-latest` and
`ubuntu-latest`, so a compile failure here is news. **A window is not proven.**
No runner has a display, so nothing has ever confirmed that this application
draws a pixel outside Windows, and the failures that survive a green build are
exactly the ones to expect: a window that opens blank, wrong scaling on a HiDPI
screen, missing icons if `vendor/fonts/` did not travel, a file dialog that does
nothing because the native one is Windows-only.

**If you are the first person to run it: that is the test.** Say what you saw,
including "it worked", and say which distribution and desktop session. A
platform moves into `void.json`'s `platforms` array when somebody has looked at
a screen, and not before — which is what lets the download page tell a Mac
visitor the truth. Until then a green build on this machine means the code
compiles, and that is a smaller claim than it looks.

## 11. House rules

1. **Never invent data.** If a flier does not give a time, leave `start_time`
   empty. An empty field is fixable; an invented one is a falsehood the
   newsletter will publish under someone's name.
2. **Never dispatch `save`** (§5).
3. **Never run an effect unasked** (§6). `deploy-site` and `rollback-site`
   change what the public sees and `revert` does not reach either. Publishing is
   the operator's decision every time, even when they asked you to update the
   site — build it, tell them it is ready, and let them press it.
4. **A person outranks you.** If someone opens the GUI while you are working,
   your session loses the floor: the next command fails and your unsaved work is
   dropped. That is correct behaviour — say so plainly and offer to redo it.
5. **`notes` fields are private.** Hormiga treats internal-notes-class fields as
   unpublishable and blocks them from reaching the website. Do not copy their
   contents into a `summary`, a `bio`, or a newsletter block; that routes around
   a privacy rule the application is enforcing on the person's behalf.
6. **Never add a `clearance:` tag to make a page look fuller.** It is the record
   of a person's consent to be published, not a display switch. If a directory
   is empty, say so and ask who has agreed to be listed.
7. **Read the design when unsure.** The OKF bundle is the real documentation:
   `python -m okf --bundle okf ls`, then
   `python -m okf --bundle okf get --head <id>`.

---

# The prompt

Everything an agent needs ships **beside the binary** — this guide and the OKF
bundle are copied next to `voidhormiga-cli` at build time — so a caller needs no
checkout, no source and no compiler. Two paths and a task:

> You have **Void Hormiga**, a contact and outreach database for community
> organizations. You drive it from the command line.
>
> - Binary: `C:/.../voidhormiga-cli.exe`
> - Run every command from: `C:/path/to/my-database-folder`
>
> First run `voidhormiga-cli --describe` and read the JSON — it lists every verb,
> every record type with its real field names, what already exists, and the house
> rules. Then read `AGENT-GUIDE.md`, which sits next to the binary.
>
> Two things that will bite you: field names must come from `--describe` (setting
> an undeclared field succeeds and is silently invisible), and tags are
> namespaced — `type:event`, not `event`.
>
> Do the work as one atomic batch
> (`voidhormiga-cli --script work.txt --atomic --actor "claude"`), then show me
> `voidhormiga-cli status`. I can discard everything with `revert`, so prefer
> doing the work over asking permission for each edit.
>
> Never run `save`. Never run an `effect` — building a newsletter or a website —
> without asking me first. Never invent a fact: if the source material does not
> say something, leave it empty and tell me what was missing.
>
> Your task: <TASK>

**The shortest version that still works**, if you would rather not paste all of
that: the two paths, plus *"read `--describe` and the `AGENT-GUIDE.md` next to
the binary before you start."* Everything else is in those two places — the
paragraphs above exist because saying it twice makes it stick.
