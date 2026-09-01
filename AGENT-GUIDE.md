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

    voidhormiga-cli --describe

Run it **in the folder that holds the database** — the one containing
`demo-org.json` / `demo-org.db`. That working directory is how the tool finds
everything.

The binary is at `<hormiga-repo>/build/bin/voidhormiga-cli.exe`. If nobody has
told you where the database lives, **ask.** Writing into the wrong folder
silently creates a second, empty database, and that is a tedious mess to unpick.

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
thing however the tool was launched. Unset means the plain default — the name
under the database folder.

Worth knowing which is which before you move anything: **`assets/` holds
originals that exist nowhere else** and is the only one bundled into a `.miga`
backup. **`tiles/` and `site/` and `exports/` are derived** — a re-fetchable map
cache and re-rendered output — which is why they are not backed up and why
pointing `tiles/` at a scratch disk costs you nothing.

## 3. The shape of the data

| term | what it is |
|---|---|
| **mantle** | a namespace. `demo-org` is the organisation's data; each newsletter or website is its own mantle |
| **rune** | a record — a contact, an event, a newsletter block |
| **glyph** | a rune's type (`contact`, `event`, `organization`, `hero`, `event_grid`, …) |
| **tag** | a label. **Namespaced by convention**: `type:contact`, `lang:es`, `status:active` |
| **link** | a relation between two runes |

**The verb that writes an edge is `link`, not `relate`.**

    link org-dns org-pages --relation 3:1     # writes the edge
    relate a b --relation flyer-of            # reports success, writes NOTHING

`relate` is a tag verb from Void Core and it is reported upstream; until it is
fixed it will tell you it worked. `link` is missing from `--describe`'s `verbs`
string, which is the other half of the same problem — the discoverable verb is
broken and the working verb is undiscoverable. Use `link`, and check with
`related <rune>` **plus** a look at the document, not the return value.

The `--relation` value is a **port pair**, `i:j` — output port *i* of the first
rune into input port *j* of the second. `3:1` is what the Antfarm's own nodes
use; a plain name like `flyer-of` also works for a data relation.

### Two traps that cost real time

**Tags are namespaced, and queries match them exactly.** The convention is
`type:event`, not `event`. A newsletter block whose query is `type:event` finds
nothing if you tagged the runes `+event`. Tags are cheap — apply both if unsure:

    tag posada-2026 +type:event +community-outreach

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
  can; the render picks by language.
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

### Say how much of each event to show

The wall of text was mostly one block rendering every match in full:

    set ev detail 'compact'    # default: heading, date/time/venue, ~140 chars
    set ev detail 'title'      # one line each — for a long list
    set ev detail 'full'       # the whole summary — for ONE featured item
    set ev limit '4'           # at most four
    set ev sort 'date'         # default; also 'date-desc' or 'name'

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
through an Antfarm `hol_static_host` node. It is refused by default, it is one
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
version back. It needs `rollback_cmd` on the node (the sibling of `deploy_cmd`)
and it refuses rather than guessing which version to restore.

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

**The person can do all of this too**, from the **Publish tab** in the desktop
app — build, preview, a green Publish with a confirmation, and the same history
with `view` and `restore`. That is not a coincidence to be maintained by hand:
the button, the command bar and your `effect` call are three callers of one
verb. If you ever find something you can do that the GUI cannot, that is a bug
worth reporting, not an advantage worth using.

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
