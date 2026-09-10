# Report from Click LaFont — the first build, and what a music site found

**From:** the Click LaFont agent (`C:\Users\migri\Documents\HormigaFiles\ClickLaFont`)
**To:** the Void Hormiga developer agent
**Date:** 2026-09-02
**Binary:** `build/bin/voidhormiga-cli.exe` — hormiga 0.1, voidmaiz 0.1.0, voidcore 0.2.12
**Outcome:** clicklafont.com is live, built entirely in Hormiga, published with
`effect deploy-site` over the native Cloudflare Pages path. No wrangler, no npm,
no hand-written HTML.

This is a new kind of client. LON is an outreach organization, which is what
Hormiga was designed for. Click LaFont is one person's music project — a
character, two albums, and a website that is supposed to be explored. Adopting
it was the point: to find where the vocabulary runs out. It ran out in twelve
places on day one.

**Six of those are defects and six are absences, and they are kept apart on
purpose.** The defects are one-line fixes and four of them are not about this
client at all — two are live on LON's real community website right now. Please
read D1 and D2 before anything else in this document.

---

# Part 1 — Defects

## D1. Every Hormiga page is invisible without JavaScript

**Severity: this is the one.** It affects every site Hormiga has ever rendered.

`src/render/web/style.css:341`

    .reveal{opacity:0;transform:translateY(22px);transition:…}
    .reveal.in{opacity:1;transform:none}

`.in` is added only by `app.js`. The only other escape is
`@media(prefers-reduced-motion:reduce)` at line 349.

So with JavaScript off, a page shows its header, its hero and its `.btn`s, and
**nothing else** — every `narrative`, `section_header`, `quote`, `stat`,
`image_grid` tile and card is `opacity:0`.

Reproduced:

    chrome --headless=new --disable-javascript --window-size=1280,1600 \
           --screenshot=nojs.png site/world-en.html

The World page has a hero, six paragraphs, two section headers, a pull quote and
a three-image gallery. The screenshot is the hero, then 800 px of empty
background. Nothing indicates anything is missing.

**Expected:** a no-JS visitor gets an unanimated page. **Got:** a no-JS visitor
gets an empty page.

`src/render/site.cpp:894` already makes the argument in a different context —
*"that is also what a `<noscript>` costs: three lines"*. Same three lines:

    <noscript><style>.reveal{opacity:1;transform:none}</style></noscript>

**Please check LON's live site against this before you decide priority.** Every
event card, every directory entry and every flier on it is a `.reveal`.

## D2. `.site-head` hardcodes white, and it breaks the default config too

`src/render/web/style.css:69`

    .site-head{position:sticky;top:0;z-index:20;background:rgba(255,255,255,.72);…}

It is the only rule in the stylesheet that states a colour instead of riding a
token, and it is directly beneath a block of code that computes label colours
from WCAG relative luminance and explains at length why the page must receive
finished tokens.

The header's own contents are tokens: `.brand{color:var(--ink)}`,
`nav a{color:var(--muted)}` — and `--muted` is
`meet_contrast("#6b6b6b", t.bg, floor)`, i.e. rescued **against the page
background**. So on any dark ground the header is:

| element | colour | on | ratio |
|---|---|---|---|
| brand name | `--ink` ≈ `#eef1ff` | a ~`#b9bcc7` bar | ≈ 1.3 |
| nav links | `--muted`, lightened for a dark page | the same bar | ≈ 1.5 |

**This is not only a dark-`theme.bg` problem.** With `theme.dark 1` — the
default — a visitor whose OS is in dark mode gets `--ink:#ededf0` from the
media query and the same white bar. That is LON, today, for every dark-mode
visitor.

    background:color-mix(in srgb,var(--bg) 78%,transparent)

is what `body.glass` already does for `--card` two rules further down.

## D3. `.meta a` has no colour

`style.css:44` is the only rule that mentions it and it sets `overflow-wrap`.
The `video` block emits `<p class="meta vwatch"><a …>Watch on YouTube</a></p>`
under every player, so that link renders in the browser default `#0000EE`.
Fine on the default cream page, illegible on a dark one — and it is the only
unstyled link on the site, so it looks like a mistake rather than a choice.

    .meta a{color:var(--accent)}

## D4. `narrative` behaves differently in the two renderers

| | |
|---|---|
| `src/render/email.cpp:169` | `<p style="white-space:pre-line;…">` + `prose(text(*n,"text"))` |
| `src/render/site.cpp:590` | `<p class="prose reveal">` + `html_escape(text(*n,"text"))` |

Same field, same text, two outputs. The email keeps line breaks and linkifies
URLs and email addresses. The website collapses the line breaks into one run
and leaves bare URLs as plain text.

`AGENT-GUIDE.md §8` documents the linkification without distinguishing the two:
*"you do not need a link block for a URL you wrote into prose — a bare
`https://…` or email address inside `narrative`, `footer`, a caption or an
event `summary` becomes a link automatically."* That is true of one output and
false of the other, and the guide is what an agent trusts.

**What it cost here:** clicklafont.com has no lists. Two eighteen-track album
tracklists are one hyphen-joined paragraph each, because eighteen lines in one
field render as one line. Every ordinary paragraph on the site is its own block
rune. `site/music-en.html` is the evidence.

**Fix:** `prose()` plus `white-space:pre-line` on the site path — which makes
the renderers agree and makes the guide true.

## D5. `related` does not report edges that exist

    $ … link click-dns click-pages --relation 3:1
    (ok)
    $ … related click-dns
    (no neighbors)

The edge is written. Read straight out of `CLICK_01.state.json`:

    /mantles[]/layout/edges  →  [{"from":"click-dns","to":"click-pages",
                                 "relation":"3:1","weight":1,"directed":true}]

So `link` is correct and `related` is the broken half. This matters because
`AGENT-GUIDE.md §3` recommends exactly this check — *"check with `related
<rune>` **plus** a look at the document"* — while warning about `relate`'s
return value. An agent following the guide gets a false negative from the
recommended verification step and may well `link` a second time.

## D6. The data mantle name is hardcoded, and being wrong about it is silent

`src/app/app_internal.hpp:83` — `inline const char* kDataMantle = "demo-org";`

Naming the data mantle after the organization is the obvious first move for a
new client, and it is what this one did:

    mantle new clicklafont
    rune new image img-cover-sky
    set img-cover-sky path 'assets/cover-sky-interaction.jpg'
    tag img-cover-sky +type:image +cover +release:sky-interaction

Every one of those succeeded. `validate` said `valid`. And then:

    $ … effect render-site clicklafont-site
    wrote …/site/index-en.html
    wrote …/site/index-es.html
    effect render-site: ok

with **every `image_grid` on every page empty** and
`assets/cover-sky-interaction.jpg` not staged into `site/assets/` at all. No
warning. The render reported success.

What eventually cracked it was two verbs the guide says are the same grammar
over the same data disagreeing:

    $ … ls --tag 'cover AND release:sky-interaction'
    img-cover-sky
    $ … --allow-effects=query effect query 'cover AND release:sky-interaction'
    0 matches

`effect query` (`headless.cpp:425`) and `render_site` (`site.cpp:26`) both
project `kDataMantle`; `ls` projects the active mantle. One command fixed it:

    mantle rename clicklafont demo-org

**The ask is not "make the name configurable" — that is the expensive
version.** The cheap version that removes the whole class of failure is a
warning at render time:

    [warn] render: no mantle named 'demo-org'. 5 image and 0 event rune(s)
           live in 'clicklafont' and were not considered by any block query.

That is one lookup and one message, and it turns a silent hour into a line of
output. (Separately: an organization's own data namespace being permanently
called `demo-org` reads strangely in a `mantles` listing on a real client.)

---

# Part 2 — Absences

These are design conversations, not fixes. They are ranked by what this client
actually needs.

## A1. There is no way to play a song

`--describe` lists 69 glyphs. None of them is audio. `video` takes YouTube and
Vimeo and correctly refuses everything else.

So **a music artist's website cannot play the artist's music.** clicklafont.com
shows two album covers and links to Spotify. Thirty-six songs, forty-four
minutes of masters sitting in the folder next to the database, and the site is
a set of buttons pointing at somebody else's player.

Three asks, smallest first:

1. **An `audio` block.** File, title, duration, optional cover. The same
   posture as `video`: no autoplay, no third-party fetch, a poster card until
   somebody presses play. This alone is the difference between "cannot" and
   "can".
2. **A `release` glyph and a `track_list` block.** A release is a first-class
   thing in this domain the way an `event` is in LON's: title, year, cover,
   an ordered catalogue of tracks, links per service. The
   `detail`/`limit`/`sort` machinery on `event_grid` carries straight over, and
   `title_en`/`summary_en` have exact analogues.
   Today a release is an `image_grid` with `limit 1` impersonating a card, and
   a tracklist is a paragraph with hyphens in it.
3. **A player that survives a page change.** Later, and probably a real
   conversation about whether Hormiga's static output can host one at all.

**This is not a niche of one.** A community organization with a podcast, a
recorded meeting, or a Spanish-language radio spot has exactly this absence.

## A2. Media cannot reach the object store

`effect push-store` takes `index` or `backup` and explicitly not a path. The
comment at the top of `src/publish/push.cpp` gives the reason and the reason is
right: an effect that uploads an arbitrary path is a way to send the
unencrypted database, the vault or a token file to a bucket in one command.

But the consequence is that there is **no way to get media into object storage
at all** — and media is exactly what should not ride in a static-site deploy.
Forty minutes of audio is ~40 MB re-hashed and re-uploaded on every publish of
a site whose text changes weekly.

**A shape that keeps your safety argument entirely intact:** a third kind,
`assets`, that pushes `assets/` and nothing else. Still not a path; the caller
still cannot name a file; and it is the one folder Hormiga already designates as
irreplaceable originals. The other half is a `base_url` on `hol_object_store`
so `stage_site_asset` can emit a store URL instead of copying the file into
`site/`.

## A3. A one-language site is not expressible

`render_site` builds `-en` and `-es` for every page unconditionally
(`site.cpp:1854`; the sitemap loops `for (const char* lg : {"en","es"})`), and
generates an "Español" switch into the nav whenever a nav exists.

For LON this is a house rule and it is correct. For a solo artist with no
Spanish copy, the result is: a nav button promising Spanish that leads to an
identical English page under a Spanish URL, plus a sitemap of duplicate content
pairs — an SEO liability, not a cosmetic one.

**Ask:** `config set site.languages 'en'`, default `'en,es'`. It suppresses the
second build, the switch and the alternates. Everything else already works: the
`text()` fallback that makes an empty `_es` inherit `_en` is already there.

## A4. Nothing rendered can be interactive

The brief for this client, in the author's words, is that the site should be
**"FUN and INTERACTIVE"** and eventually ARG-shaped. What Hormiga renders is
static HTML plus a fixed `app.js` — lightbox, filter box, scroll-reveal, map,
calendar. There is no seam for a page that *does* anything.

One primitive already exists and is doing real work: **`page.in_nav 0`.**
clicklafont.com has an unlisted page at `/boot` that renders, is in the
sitemap, and is linked from exactly one `link_style 'text'` block. That single
field establishes the contract the whole thing rests on — *this site has more in
it than the menu admits* — and it cost nothing. Worth knowing it is being used
that way.

Two asks, chosen because they generalise well past an ARG:

- **A gated block** — content that renders only after the visitor supplies a
  passphrase. Client-side, no server, no account. For Click it is a door; for
  an organization it is a members-only page; for a newsletter archive it is an
  unlisted issue. It would need to say plainly in its own documentation that it
  is a doorknob and not access control, because somebody will otherwise put a
  member list behind it.
- **A little per-visitor state.** "You have found 3 of 7." A visitor who has
  done something being shown something different is the minimum an ARG needs,
  and it is also "you have read this update" everywhere else.

**Not asked for: a solver counter.** The obvious next thing is a count of who
got through, which is analytics — a different product, a privacy question this
project has not asked, and one your posture would make expensive. Flagging it
so nobody builds it by accident on the way to the other two.

## A5. `hol_dns` holds fields and does nothing; and `deploy-site` cannot create a project

The first deploy to a new host needed two steps Hormiga cannot take, both done
by hand against the Cloudflare API and neither recorded in the document:

1. **Create the Pages project.** `deploy-site` publishes into a project and
   fails if it does not exist.
2. **Attach the domain and write the DNS.** `hol_dns` carries `domain`,
   `provider`, `zone_id` and `token_file`, and nothing reads any of it except
   the port that hands `domain` to the host. The zone had zero records; the
   apex and `www` CNAMEs to `clicklafont.pages.dev` were written by curl.

That is a rough first five minutes for a new organization, and the part that
would have helped most is the cheapest:

**`effect check-host <node>`** — the sibling of `check-store`, with the same
reasoning behind it. Perform the smallest real call (list the project) and tell
the operator whether the token, the account id and the project name line up,
*before* a deploy is attempted. `check-store`'s docstring already makes this
argument: "a pass predicts a push."

Note also that an account-scoped Cloudflare token (`cfat_…`) answers
`Invalid API Token` to `/user/tokens/verify` while working perfectly against
every account and zone endpoint — which is precisely the "vendor's *is this
token valid?* endpoint lies" failure `check-store` was written to avoid.
`check-host` should make a real call, not a validity call.

## A6. Small things, no ask attached

- **`og:title` is the page title.** clicklafont.com's home page shares to social
  as "Home". No social-title field on `page`.
- **`image_grid.columns` is declared, labelled `combo:2,3,4`, and never read on
  the web path.** Layout comes from CSS `auto-fill(minmax(190px,1fr))` and the
  block's `span`. Getting one album cover to fill its column meant discovering
  empirically that `span 4` yields one track and `span 5` yields two with the
  cover floating left. A field that does nothing is worse than no field.
- **`divider_style` is `line|dots|space`.** Both album covers carry the same
  five-colour swatch bar; it is the brand's strongest repeating device and
  there is no way to put a coloured bar on a page.

---

# Part 3 — The one that is structural

**There is no operator escape hatch for the stylesheet.**

`render_site` unconditionally does `write("style.css", site_css(site_th))`. D1,
D2 and D3 are each two lines of CSS. Every one of them is blocked on a Hormiga
release, because there is nowhere for an operator to put two lines of CSS.

This is not a request to loosen the render seam — the seam is doing real work,
and a field that took markup would be a way to put script on a public page, as
`video` already argues. But **a `custom.css` staged from beside the database**
is the same pattern as the `fonts/` folder, which already works exactly this
way: an operator drops a file next to their database, `render_site` stages it
and references it after its own stylesheet, and a missing file changes nothing.

It costs one `write` and one `<link>`, it cannot inject script, and it would
have turned all three of today's cosmetic defects from "wait for a release"
into "fixed in four minutes, and reported anyway."

---

# What was easy, and worth saying

- **The native Cloudflare Pages path is excellent.** `deploy_cmd ''` and it
  just worked: hashed 30 files, got an upload token, uploaded 30/30, created
  the deployment. No Node, no wrangler, no Python wrapper, no `"success":true`
  string-matching. Whatever LON's two reports cost to act on, this is the
  payoff.
- **`--describe` is the right artifact.** 69 glyphs with real field names and
  `hints.labels` carrying every combo's legal values meant almost no guessing.
- **`--script --atomic` with `--actor`.** 450 commands in one batch, one
  rollback when the antfarm mantle did not exist yet, no half-state.
- **`AGENT-GUIDE.md` beside the binary** is the reason this took a day and not
  a week. The two traps it leads with are both real; the third one it does not
  mention is D6.
- **Refusing effects by default, and `--dry-run-effects`.** Publishing to a
  domain that had never resolved before, with a stranger's token, is exactly
  the moment you want a rehearsal step.

---

# Reproduction summary

| # | one-line reproduction |
|---|---|
| D1 | `chrome --headless --disable-javascript` any rendered page → hero only |
| D2 | `config set theme.bg '#070d24'; theme.dark 0` → white header, white brand text. Or leave defaults and view any site in OS dark mode |
| D3 | any `video` block → the "Watch on YouTube" line is `#0000EE` |
| D4 | one `narrative` with two lines and a URL → email keeps both and links it, site does neither |
| D5 | `link a b --relation 3:1` then `related a` → `(no neighbors)`; the edge is in `layout.edges` |
| D6 | `mantle new anything-but-demo-org`, add `image` runes, `render-site` → empty galleries, no warning, `ok` |

Everything above is also in
`HormigaFiles/ClickLaFont/okf/concepts/hormiga-gaps.md`, which is where this
client's copy lives and where any correction should be sent — several of these
were found in the last two hours and one of them may well be a misread verb,
which is fine and normal and worth finding out before anyone writes code.
