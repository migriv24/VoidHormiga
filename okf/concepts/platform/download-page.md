---
type: Concept
title: The download page — where the installer lives and how a visitor gets it
description: "A plan, written for the author and for an agent building the site: where to host the .exe, why the website's button should never carry a version number, what Hormiga's `download` and `link` blocks already do, and the four things that make an unsigned Windows download hard for a stranger."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-09-08T00:00:00Z
---

**Status: half record, half plan, and the seam between them is a release.**
[distribution](/concepts/platform/distribution.md) is the built half at the
other end — the installer stages clean, the update client works — and this page
is the moment a person who has never heard of us clicks a button.

What is now built and measured, rather than argued:

- **The page exists**, at <https://clicklafont.com/hormiga-en>, built against
  §6 of this document by an agent who had not written it. The build brief in §6
  is therefore *runnable*, which it was not when it was written.
- **Platform sets** (§5d) — the answer to the author's *"it should detect the
  system (linux, windows, mac), and then provide the correct download"*. A
  `download` or a `link` names a computer; a grid row with two or more of them
  is a set; the visitor's own is moved first and labelled, and **none is ever
  hidden**. Built 2026-09-08; `src/render/download.hpp` carries the reasoning.

What is still a plan is the part that is not ours to write: **there is no
release yet**, so the stable URL in §4 still 404s. §7 is the order.

**Three numbers and one warning on this page were wrong** when it was written
and are corrected below, all measured by the Click LaFont agent on 2026-09-05:
the installer is **7.0 MB, not ~25 MB**, and the apostrophe warning in §6.2 was
over-cautious. The report is
[`okf/reports/click-lafont-download-page-2026-09-05.md`](../../reports/click-lafont-download-page-2026-09-05.md).

**Who this is for.** Two readers at once. Sections 1–5 are the reasoning and are
written for the author, in more detail than an agent needs. **Section 6 is the
build brief** — a transcript an agent can run — and Section 7 is the checklist
before anything goes live. Read 1–5 once; an agent can start at 6 and refer
back.

---

# 1. The whole thing in one picture

```
   GitHub Releases                     GitHub Pages (or Cloudflare Pages)
   ────────────────                    ─────────────────────────────────
   VoidHormiga-windows-x64-setup.exe   the website, rendered by Hormiga
   VoidHormiga-0.1.1-...-setup.exe        from the .miga on your machine
   void-updates.json                      one `link` block → the .exe
   SHA256SUMS.txt                         one `download` block → SHA256SUMS
        │                                       │
        │ read by the running app               │ read by a human with a browser
        ▼                                       ▼
   voidhormiga.exe                         "Download for Windows"
   (update client)
```

**Two hosts, and they hold different kinds of thing.** Releases holds *files
that are the same for everyone and are large*. Pages holds *a website generated
from your database*. The website links to Releases; it does not contain the
installer.

That split is the single most important decision on this page, and §3 is the
argument for it.

# 2. Why hosting is a question at all

You said you did not think downloads would be complicated, and mostly you are
right — an installer is a file and a button is an anchor tag. Four things make
it less trivial than that, and only the last is about Void Core.

**(a) The file is 7 MB and there will be many of them.** (This said 25 MB
until it was built and weighed: NSIS `/SOLID lzma` compresses the 46 MB staged
tree to 7,327,237 bytes, 15.8%. The argument below survives the correction, but
it is a weaker argument than it was and it should be read as one.) A website is a thing
you redeploy constantly; an installer is a thing you publish rarely and keep
forever. Putting the second inside the first means every deploy carries it.

**(b) Nothing is signed.** Windows will actively discourage a stranger from
running it. That is not a hosting problem, but it is the biggest thing standing
between the button and a working install, and §5 is about it.

**(c) A static site cannot compute.** [A static
site](https://en.wikipedia.org/wiki/Static_web_page) has no server that runs
code per visitor, so the page cannot ask *"what is the latest version?"* at the
moment somebody loads it. Whatever the page says was decided when you last
deployed.

**(d) And this is the Void Core part.** *Your website is generated from your
database.* The download button is not HTML somebody typed — it is a **rune with
fields**, living in a mantle, edited through the dispatcher, rendered by
`render-site`. See [blocks &
domains](/concepts/sections/blocks-and-domains.md). That is mostly a gift: the
button is versioned, replayable, and an agent can change it with a logged
command instead of editing markup.

But it has one sharp edge, which is (c) and (d) together:

> **If the version number is written on the page, it is data — and data goes
> stale.** Publish 0.1.2 and the page still says 0.1.1 until somebody
> remembers to open Hormiga, edit a rune, and redeploy. Now the download page,
> the update feed and the manifest are three places one number lives.

§4 is the fix, and it is a good one: **do not put the version on the page at
all.**

# 3. Where the installer lives

## The recommendation: GitHub Releases

`https://github.com/migriv24/VoidHormiga/releases`

**Why.** It is built for exactly this and costs nothing:

- **No repository bloat.** Release assets are stored outside the git object
  database. A file in your Pages branch is in [git's
  history](https://en.wikipedia.org/wiki/Git) *permanently* — twenty releases of
  a 7 MB installer is 140 MB of history you can never delete without rewriting
  the repo, against GitHub's 1 GB soft repository limit, and it only ever
  grows. A release asset can be
  deleted and the repo does not remember it.
- **It is already a [CDN](https://en.wikipedia.org/wiki/Content_delivery_network).**
  Release downloads are served from GitHub's edge, not from your Pages
  allowance.
- **It serves `Content-Disposition: attachment`**, so the browser *saves* the
  file rather than trying to display it — which matters, and §4 explains why.
- **Download counts, free.** The API reports how many people took each asset.
  That is the only real usage number you will have, and you get it without
  measuring anybody.
- **The app already points there.** `default_feed_url()` in
  [`src/update/update.hpp`](../../../src/update/update.hpp) is
  `.../releases/latest/download/void-updates.json`. The website linking to the
  same place means the site and the running application agree about where
  software comes from, which is one fact rather than two.

**What actually gets attached to each release:**

| file | why |
|---|---|
| `VoidHormiga-0.1.1-windows-x64-setup.exe` | the versioned installer — the permanent, citable artifact |
| `VoidHormiga-windows-x64-setup.exe` | **a byte-identical copy with the version stripped out.** This is the trick §4 turns on |
| `void-updates.json` | what the running app reads. Produced by `mago feed`. Since mago 0.1.6 it also *states* the stable name as `stable_file`, and `mago feed --artifacts` checks that the copy exists and is a copy |
| `SHA256SUMS.txt` | so a person can verify by hand |

## The alternatives, and why not

- **Put the `.exe` in the site's `assets/` with a `download` block.** It works
  today — §6 shows how — and it is right for *small* files (a PDF, the
  checksums). For the installer it is the repository-bloat problem above, and
  on the GitHub deploy path there is a second cost: `hol_github` uploads each
  file [base64-encoded](https://en.wikipedia.org/wiki/Base64) in a JSON body,
  so a 7 MB installer becomes a ~9.4 MB request on every single deploy of the
  whole site — every deploy, not every release.
- **Cloudflare R2 / S3.** Fine, and Hormiga already speaks
  [S3](https://en.wikipedia.org/wiki/Amazon_S3) natively (see [the web
  platform](/concepts/platform/web-platform.md)). Skip it until there is a
  reason: it is an account, a bill and a credential, and Releases is free and
  already in the loop.
- **Your own server.** [The web platform](/concepts/platform/web-platform.md)
  records why not, in your words: *"I kinda don't wanna self host, because then
  I'll need a machine that's constantly on."*

**The invariant that makes all of this safe** is the same one that page
carries: **every cloud host is disposable.** Nothing above is a place we edit —
it is a place we push to, and everything pushed is regenerable from the `.miga`
on your machine plus a build.

# 4. The website's button must not know the version

This is the design decision worth actually understanding, because it makes an
entire category of maintenance disappear.

GitHub gives you a URL that always resolves to the newest release's asset **of
that exact name**:

```
https://github.com/migriv24/VoidHormiga/releases/latest/download/<asset-name>
```

If the asset is named `VoidHormiga-0.1.1-windows-x64-setup.exe`, that URL is
useless a month later — the name has the version in it. So **attach a second
copy of the same bytes under a stable name**, `VoidHormiga-windows-x64-setup.exe`,
and the link becomes permanent:

```
https://github.com/migriv24/VoidHormiga/releases/latest/download/VoidHormiga-windows-x64-setup.exe
```

**Consequences, all good:**

- The page never carries a version, so it can never be *wrong* about one. The
  staleness problem from §2(d) does not get solved; it stops existing.
- **You never redeploy the website to ship an update.** Publish a release; the
  button that was already there now hands out the new file.
- The button says *"Download for Windows"*, full stop. Which is also better
  copy — nobody outside this repository knows what 0.1.1 means.
- The versioned asset stays attached too, so a specific version is still
  citable and downloadable forever. Both facts are true at once.

**Since mago 0.1.6 the stable name is stated rather than remembered.** The feed
carries it as `stable_file` beside the versioned `file`, from one rule that
differs in one segment, so the two names cannot drift — and `mago feed
--artifacts` hashes the copy and complains if it is missing *or* if it is
present and hashes differently. That second case is the one worth having: a
stale copy from the previous release is present, plausible, and hands every new
visitor the old application while every check that only looks for the name
passes. The name is pinned on our side too, in `tests/update_smoke.cpp` §10,
because this repository is the only place that sees both halves — the feed the
application reads and the button a person types.

**One HTML detail worth knowing**, because it looks like a bug when you meet it.
The `download` attribute on an anchor — the one that forces *save* instead of
*navigate* — is **ignored for cross-origin URLs** by every browser, a
[same-origin](https://en.wikipedia.org/wiki/Same-origin_policy) rule. So a link
from your site to GitHub cannot force a save on its own. It does not matter
here: GitHub sends
[`Content-Disposition: attachment`](https://en.wikipedia.org/wiki/MIME#Content-Disposition),
which makes the browser save it anyway. The result is what you want; the reason
is the header, not the attribute. If you ever move hosting, check that the new
host sends that header, or your download button becomes a "here are 7 MB of
binary rendered as text" button.

## The open choice: should the page state the version at all?

Three options, and a lean.

1. **Say nothing.** *"Download for Windows"* and a link to the releases page for
   people who want detail. **Lean: this, to start.** Zero maintenance, nothing
   can be stale, and the update client is what tells existing users about new
   versions — which is its job.
2. **Say it, and accept the redeploy.** One `set` command and a `deploy-site`
   per release. Honest, cheap, and it *will* be forgotten once.
3. **Fetch it at load time with a little JavaScript** — read
   `void-updates.json`, write "Latest: 0.1.2" into the page. Hormiga sites
   already ship an `app.js`, so this is not foreign. But there is no block for
   it today, so **do not improvise one**: an author-controlled `<script>` is
   exactly what `download_refusal` and the `custom.css`-is-a-file decision exist
   to prevent (see §6.3). If you want it, it should be a proposed block with a
   design conversation attached, the way `download` itself arrived.

# 5. The four things that will actually lose you a download

None of these are code. All of them are the difference between a button and an
install.

## (a) SmartScreen, and being honest about it

The installer is **unsigned**. `void.json` says `"signed": "ed25519"` and nothing
implements it yet — [distribution](/concepts/platform/distribution.md) §5 says
so plainly. So on a stranger's machine, Windows shows:

> **Windows protected your PC** — Microsoft Defender SmartScreen prevented an
> unrecognised app from starting.

with a **Run anyway** hidden behind *More info*.
[SmartScreen](https://en.wikipedia.org/wiki/Microsoft_SmartScreen) is
reputation-based, so this is not a verdict about the file — it is a statement
that not enough people have run it yet. [Code
signing](https://en.wikipedia.org/wiki/Code_signing) with an OV certificate
(~$200–400/year) softens it; an EV certificate removes it and costs more.

**Until then, put it on the page.** A short, calm paragraph under the button:
what they will see, that it is expected, how to get past it, and *why* — that
the app is new and not yet signed. Two reasons this is right and not just
polite:

- **A warning you did not warn them about is the one that stops them.** A
  warning you predicted is one they trust you through.
- It is the same rule the update prompt already follows. It says, in these
  words: *"this release is not signed: the checksum proves the download arrived
  intact, not who built it."* The website should not claim more than the
  application does.

## (b) The checksum, and what it is actually for

Publish `SHA256SUMS.txt` and a one-line "verify this download" note.

Be precise about what it proves, because it is easy to overstate.
[SHA-256](https://en.wikipedia.org/wiki/SHA-2) proves the bytes that arrived are
the bytes that left. It proves nothing about who made them — if somebody
replaced both the installer and the checksum file, they match perfectly. That
is the difference between a checksum and a
[signature](https://en.wikipedia.org/wiki/Digital_signature), and it is why the
`signature` field in the feed is `null` rather than absent.

It is still worth publishing: it catches a corrupted download, and it is what a
cautious person will look for.

## (c) Say what it is and what it needs

Above the fold, in plain words: **Windows 10 or 11, 64-bit. About 7 MB. Installs
for you only — no administrator password.** That last one is a genuine selling
point and people do not expect it: [the installer is
per-user](/concepts/platform/distribution.md#4-side-by-side-is-what-makes-an-update-reversible),
so it needs no elevation and touches nothing outside the profile.

Also say **there is no macOS or Linux build**, rather than leaving a Mac visitor
to click and find out. `void.json` declares `"platforms": ["windows-x64"]`; the
page should not imply otherwise.

## (d) Detect their OS. Never hide anything. — PLATFORM SETS, built 2026-09-08

This section said *"do not detect their OS"* until the author asked for exactly
that: *"it should detect the system (linux, windows, mac), and then provide the
correct download for hormiga."* Both halves of that sentence are now true, and
reconciling them is the design.

**What was right in the old rule, and is kept:** a Mac visitor must still see
every button. Guessing an operating system from a [user agent
string](https://en.wikipedia.org/wiki/User_agent) is unreliable, and a page that
silently offers nothing is indistinguishable from a broken page. **What was
wrong is that this was written as a rule** — a sentence an author has to
remember — when it could be a property of the mechanism.

**A platform set is a grid row holding two or more blocks that name a computer.**

- **`download` and `link` each gained a `platform` field**: `windows-x64`,
  `macos`, `macos-arm64`, `linux-x64`, or `any` (the default, so nothing changed
  for the résumé and the flier PDF `download` was built for). It is the
  vocabulary `void.json` and `mago plan <app> --platform <name>` already speak.
  `link` carries it as well as `download` because **the installer button is a
  `link`** — §3 puts the `.exe` on Releases, so the thing that offers it is a
  navigation, not a staged file, and a field that reached only `download` could
  not have served this page.
- **The renderer decides what a set is, at build time**, and says so in the
  markup: `<div class="wrow platform-set" data-yours="For your computer">`, with
  `data-platform` on each candidate. The badge text is in the markup rather than
  in the script because one `app.js` serves the English and the Spanish page.
- **`app.js` does two things and owns no others**: it moves the matching card to
  the front of its row and labels it. There is no branch in it that hides
  anything, so §5(d)'s old rule is satisfied *by construction* rather than by an
  author remembering it — which is the difference between a rule and a design.
- **Only the family is matched, never the architecture.** A browser will not
  tell you what architecture it is running on — `navigator.platform` still
  reports `Win32` on a 64-bit machine. The arch is carried for the author, the
  filename and the release.
- **When the guess is unsafe, nothing is said.** A phone, a tablet and a
  Chromebook run none of these installers, and Android and Chrome OS both
  report themselves as Linux; matching either would badge the Linux card *"for
  your computer"* on a device that cannot use it. Unsure means silent, and
  silent means the page as rendered.
- **With scripting off, none of it runs** and the author's order stands, which
  is the same guarantee the `<noscript>` fix bought every other page.

**Why it is renderer-owned and not an author `<script>`.** It sits at the same
trust level as the lightbox and the video click-to-load that already ship in
`app.js`. An OS-detecting `<script>` in a rune field is the shape
`download_refusal` and the `custom.css`-is-a-file decision exist to refuse:
model data arrives by import and by merge, from a device somebody else was
using, so a field that becomes code on the site's own origin is a hole. The
Click LaFont agent asked for this version rather than the loosening, and was
right to.

**And it still cannot invent a build.** There is one, `windows-x64`. Detection
today can only tell a Mac visitor there is nothing for them — which is a
sentence, and the macOS card is where that sentence goes. Do not write a set
that offers a platform no release exists for without saying so on the card.

See `src/render/download.hpp` for the vocabulary and `src/render/web/app.js` for
the twenty lines of behaviour.

# 6. The build brief — for an agent

Everything below runs against a real Hormiga database. It uses only what exists
today; nothing here needs a new block or a code change.

**Read first:** [AGENT-GUIDE.md](../../../AGENT-GUIDE.md) §7 (how a page is a
mantle of block runes on a 12-unit grid) and §9c (the `update` verb). The
commands are dispatcher commands — see [founding commitment
1](/index.md): the GUI, the command bar and you are three callers of the same
verbs.

## 6.1 The two blocks you need, and which to use where

**`link`** — a label and a `target`, where target is a page slug **or an
external URL**. `link_style` is `button` or `text`. **This is the download
button.** It is a navigation to GitHub, which is what §3 and §4 established.

**`download`** — takes `file`, which is a path beside the database *or the name
of a `resource` rune*. It **stages the file into the site**, emits
`<a … download>`, and prints the type and size for you (`PDF · 12 KB`) because
that is the one thing a visitor wants before tapping on a phone.
`download_style` is `button` or `card`. Use it for **any PDF or guide that
belongs to the site and does not change when a release does** — a flier, the
bylaws, an annual report.

Do not use `download` for the installer. It works, and §3 says why not to.

**Nor for `SHA256SUMS.txt`, which this section told you to do until 2026-09-08.**
The Click LaFont agent declined that instruction while building the live page and
was right to: a checksums file staged into the site is *version-specific*, so it
goes stale on every release and has to be re-staged and redeployed — which is
precisely the maintenance §4 built the version-free URL to abolish. Following
§6.2 there would have undone §4. Link
`releases/latest/download/SHA256SUMS.txt` instead: same file, never stale, one
less thing to redeploy.

**The general rule that falls out of it:** `download` is for a file the *site*
owns; a `link` is for a file the *release* owns. Anything that changes when a
version changes belongs to the release.

**Both take `platform`** — `windows-x64`, `macos`, `macos-arm64`, `linux-x64`,
or `any` (the default). Put two or more of them on **the same `row`** and the
renderer makes that row a platform set: §5(d) is the whole story, and §6.2b is
the transcript. A value this renderer does not know is reported at render time
and treated as `any`, so check the render log rather than the page — a typo
would otherwise give you a row that *looks* like a set and marks nobody.

## 6.2 A transcript that builds the page

Adjust names and copy; the shape is the point. Bilingual `_en`/`_es` fields are
not optional — [there is no way to publish one
language](/log.md), by the author's decision, and a missing `_es` falls back to
the English rather than showing a blank.

**One trap before you rewrite the copy.** Every value below is a single-quoted
dispatcher argument (SPEC §6.1), and the sentences were written without
apostrophes on purpose. Rewrite one to say *"it doesnt ask for a password"* with
a bare apostrophe and it ends the argument early — a failure this stack has now
made in five separate implementations, including two of its own.

**Corrected 2026-09-05:** this used to say *do not hand-escape it*, which read as
though escaping were unavailable. It is not. `'` inside single quotes works
exactly as `--describe`'s house rule #3 documents, and the Click LaFont agent
round-tripped ten apostrophes through it on the live page. Escape it, or emit
the command through the encoder (`maiz::arg`, or Void Core's `vc_arg_quote`), or
type it into the Builder's inspector, which quotes for you. What is true is the
narrower thing: an *unescaped* apostrophe ends the argument.

```
mantle new site-download
rune new page p-download
set p-download slug 'download'
set p-download title_en 'Download Hormiga'
set p-download title_es 'Descargar Hormiga'
set p-download order '2'
set p-download in_nav '1'
set p-download meta_desc 'Hormiga for Windows: people, events and images as tagged records, composed into newsletters and websites.'

rune new hero h-dl
set h-dl page 'download'
set h-dl title_en 'Hormiga for Windows'
set h-dl title_es 'Hormiga para Windows'
set h-dl subtitle_en 'Free, local-first, and it works with no internet connection.'
set h-dl row '0'
set h-dl col '0'
set h-dl span '12'

rune new link dl-button
set dl-button page 'download'
set dl-button label_en 'Download for Windows'
set dl-button label_es 'Descargar para Windows'
set dl-button target 'https://github.com/migriv24/VoidHormiga/releases/latest/download/VoidHormiga-windows-x64-setup.exe'
set dl-button link_style 'button'
set dl-button platform 'windows-x64'
set dl-button row '1'
set dl-button col '0'
set dl-button span '12'

rune new narrative dl-facts
set dl-facts page 'download'
set dl-facts heading_en 'What you need'
set dl-facts heading_es 'Requisitos'
set dl-facts text_en 'Windows 10 or 11, 64-bit. About 7 MB. It installs for you only, so it will not ask for an administrator password.'
set dl-facts row '2'
set dl-facts col '0'
set dl-facts span '12'

rune new narrative dl-smartscreen
set dl-smartscreen page 'download'
set dl-smartscreen heading_en 'A warning you should expect'
set dl-smartscreen heading_es 'Una advertencia esperada'
set dl-smartscreen text_en 'Windows will probably say "Windows protected your PC" the first time you run it. That is expected: Hormiga is new and not yet code-signed, so Windows has not seen enough copies of it to recognise it. Click More info, then Run anyway. If you would rather check the file first, the SHA-256 checksums are below.'
set dl-smartscreen row '3'
set dl-smartscreen col '0'
set dl-smartscreen span '12'

rune new link dl-sums
set dl-sums page 'download'
set dl-sums label_en 'SHA-256 checksums'
set dl-sums label_es 'Sumas de verificacion SHA-256'
set dl-sums target 'https://github.com/migriv24/VoidHormiga/releases/latest/download/SHA256SUMS.txt'
set dl-sums link_style 'text'
set dl-sums row '4'
set dl-sums col '0'
set dl-sums span '6'

rune new link dl-releases
set dl-releases page 'download'
set dl-releases label_en 'All versions and release notes'
set dl-releases target 'https://github.com/migriv24/VoidHormiga/releases'
set dl-releases link_style 'text'
set dl-releases row '4'
set dl-releases col '6'
set dl-releases span '6'
```

**`mantle new` makes the new mantle active**, so say `use demo-org` (or whatever
the data mantle is called) before touching contacts or events again.

## 6.2b The three-platform row

Replace the single `dl-button` above with this when you want §5(d)'s platform
set. The whole mechanism is *three blocks sharing a `row`*; there is no other
switch to find.

> **A set reorders its own row, and nothing else. Do not place column-aligned
> content beneath one** — a second row of detail cards lined up under the
> buttons, say. It will not follow, the badge will end up over a card describing
> a different operating system, and **the mismatch is invisible on the author's
> own machine**, because the author's own platform is usually the one already in
> front. Nothing warns; the render log is clean.
>
> Measured 2026-09-08 by the Click LaFont agent, who built exactly that and
> caught it only by spoofing six platforms. If a card needs explaining, the
> explanation has to be *in* the card or in a full-width block that promises no
> position — the unit of movement is one `.wcol`.

The macOS and Linux cards are the part that makes it honest. They are not
placeholders: they say what is absent and why, so a Mac visitor learns the truth
from the page rather than from a click that does nothing. **Never write a card
for a platform with no release without a sentence like these on it.**

```
rune new link dl-win
set dl-win page 'download'
set dl-win label_en 'Download for Windows'
set dl-win label_es 'Descargar para Windows'
set dl-win target 'https://github.com/migriv24/VoidHormiga/releases/latest/download/VoidHormiga-windows-x64-setup.exe'
set dl-win link_style 'button'
set dl-win platform 'windows-x64'
set dl-win row '1'
set dl-win col '0'
set dl-win span '4'

rune new link dl-mac
set dl-mac page 'download'
set dl-mac label_en 'macOS - no build yet'
set dl-mac label_es 'macOS - todavia no'
set dl-mac target 'roadmap'
set dl-mac link_style 'text'
set dl-mac platform 'macos'
set dl-mac row '1'
set dl-mac col '4'
set dl-mac span '4'

rune new link dl-linux
set dl-linux page 'download'
set dl-linux label_en 'Linux - no build yet'
set dl-linux label_es 'Linux - todavia no'
set dl-linux target 'roadmap'
set dl-linux link_style 'text'
set dl-linux platform 'linux-x64'
set dl-linux row '1'
set dl-linux col '8'
set dl-linux span '4'
```

What that renders, and what it never renders:

- `<div class="wrow platform-set" data-yours="For your computer">` (and `Para tu
  computadora` on the Spanish page), with `data-platform` on each of the three.
- On a Windows visitor's screen, the Windows card first, wearing the badge.
- On a Mac visitor's screen, the macOS card first, wearing the badge — and its
  own sentence saying there is no build, which is why that sentence is not
  optional.
- On a phone, a Chromebook, or a browser that will not say: all three, in the
  order you wrote them. Nothing is hidden in any of these cases; there is no
  code path that hides one.

Check `effect render-site`'s output, not the page: a `platform` value the
renderer does not know is reported there and treated as `any`.

## 6.3 Rendering and publishing it

Both effects take arguments; a bare `effect render-site` is a usage error.

```
voidhormiga-cli effect render-site site-download
voidhormiga-cli effect deploy-site <host-node> site-download
```

`<host-node>` is the name of an Antfarm host rune — a `hol_github` for a Pages
repository, or a `hol_static_host` for a managed CDN. `voidhormiga-cli effect
check-host <host-node>` tells you whether its credentials line up **before** a
deploy is attempted, which is the cheaper order.

Two things about `deploy-site` an agent should know rather than discover:

- **It is refused by default and is one of two effects declared
  `reversible: false`.** Do not publish during unattended work without saying so
  in your summary — `revert` does not reach it.
- **It records itself.** A `deployment` rune lands in the `antfarm` mantle with
  the host, the URL, the time and `state: live`, and the previous live one
  becomes `superseded` in the same batch. That is how a person answers *which
  version is up* without asking the vendor, and it travels in the `.miga` if the
  organization ever leaves that vendor. It is also why `deployment` is one of
  the five glyphs marked `kind: act` — see [the data
  model](/concepts/foundation/data-model.md).

## 6.4 Four things you must not do

- **Do not point `download.file` at an `.html`, `.svg`, `.js` or `.wasm`.** The
  renderer refuses, with a sentence, and the reason is in
  [`src/render/download.hpp`](../../../src/render/download.hpp): a file served
  from the site's own origin can act with the site's own authority, and `file`
  is *model data an import or a merge can set*. Do not work around the refusal.
- **Do not put the version number in the button's label or URL** unless the
  author has chosen option 2 in §4. The stable-name link is the whole design.
- **Do not add a `<script>` or inline HTML anywhere.** A static site rendered
  from model data is not a place to smuggle code; if the page needs behaviour,
  that is a block proposal, not an edit. **Platform sets are what that looks
  like when it goes right** — the ask arrived, it was refused as an author
  script, and it came back as a renderer-owned field with a rule built into it.
  Ask for the next one the same way.
- **Do not write a platform card for a build that does not exist without saying
  so on the card.** The mechanism guarantees the card is visible; it cannot
  guarantee it is honest. §6.2b.
- **Do not put anything from the admin database on this page.** Nothing here
  should name a person. See [data
  planes](/concepts/platform/data-planes.md) — the publication plane only ever
  carries what cleared the render seam, and a download page has no reason to go
  near it.

# 7. The order to do it in

**Where this stands on 2026-09-08, second revision.** Steps 1 and 2 are done and
step 2 found a bug, which **Void Mago fixed the same day** (mago 0.1.6) —
verified here against this repository's own manifest, not a fixture: `mago
wizard voidhormiga --platform windows-x64` now emits `SetOutPath
"$INSTDIR\voidhormiga-0.1.0\vendor\fonts"`, and none of the 23 quoted path
arguments in the generated script contains a forward slash. The page in step 6
is built and live ahead of its turn at <https://clicklafont.com/hormiga-en> with
a sentence where the button goes.

**Nothing in code is blocking any more.** What is left is step 3, which needs a
second computer, and step 4, which needs a person to create a release.

1. ~~**Freeze the stack.**~~ Void Allomone and Void Palabra are still moving; the
   author's instruction is to have this ready rather than launched.
2. `mago stage voidhormiga --platform windows-x64`, then `mago wizard` and
   `makensis` on the emitted `.nsi`. Produces the setup `.exe`.

   **This step earned its place on 2026-09-05, and the record is worth keeping
   now that it is fixed:** Mago's generated `.nsi` wrote a `SetOutPath` with the
   forward slash from `vendor/fonts/` still in it, NSIS read it as an ordinary
   character in a directory *name*, and the fonts installed into `vendorfonts`
   — so an installed Hormiga had no icons in the desktop UI and warned `no
   webfonts found beside the binary` on every `render-site`. The staging tree
   was correct the whole time, because `fs::path` forgives a forward slash and
   NSIS does not; nothing that inspected the tree could see it. Diagnosed by the
   Click LaFont agent by rebuilding with backslashes. Fixed at a chokepoint —
   one `nsis_path()` every emitted path goes through — with a test asserting the
   general form over the whole script rather than the one line that had it.
3. **Install it on a second computer and open it.** This is
   [distribution](/concepts/platform/distribution.md)'s exit test and it is not
   optional — it is the same second machine [phase F](/roadmap.md) has been
   waiting on. Everything after this step assumes it passed. **Check the fonts
   land in `vendor\fonts` and the icons are there**; Mago asked for that
   sentence back, and a diff against a file somebody else compiled is one step
   removed from the thing itself.
4. Create the GitHub release. Attach: the versioned `.exe`, the **stable-named
   copy**, `void-updates.json` and `SHA256SUMS.txt`.

   **`--artifacts` is not optional**, and it now buys two things rather than
   one:

   ```
   mago feed voidhormiga \
     --base-url https://github.com/migriv24/VoidHormiga/releases/latest/download \
     --artifacts <the directory holding both .exe files> \
     --output void-updates.json
   ```

   Without it the feed carries no `sha256`, and the update client **refuses to
   download a release with no usable digest** — so updates fail closed on our
   side and Mago's warning is the only thing that explains why.

   With it, Mago also checks the stable-named copy: it says so if the file is
   absent, and — the one that will actually earn its keep — it says so if the
   file is *present but hashes differently*, which is what a copy left over from
   the previous release looks like. A stale one is plausible, passes every
   check that only looks for the name, and hands every new visitor the old
   application while everything reports success.
5. **Verify the feed before the website.** From a checkout:
   `voidhormiga-cli update --check` should now say *up to date* instead of
   *HTTP 404*. If it does not, the website will be advertising something the
   application cannot see.
6. Build the page (§6, and §6.2b for the platform set) and `deploy-site`. Done
   ahead of turn, on clicklafont.com; flipping the button live is two `set`
   commands and a redeploy once step 4 exists.
7. **Download it from the live URL, on a machine that has never had it**, in a
   browser you are not signed into GitHub with. Click your own button. This is
   the only step that tests the thing a stranger will actually do.
8. **Then do it again with 0.1.1**, because none of the above tests an *update*.
   One release proves the feed parses and says *up to date*; the exit test is
   *"a stranger downloads Void Hormiga from a page Void Hormiga deployed, and it
   updates itself"*, and the second half needs something to move to. Bump the
   version, fill in `release.previous` (absent today, correctly — there is
   nothing to move from yet), re-stage, cut a second release, and run
   `voidhormiga-cli update --install` on the second computer. `side_by_side` is
   true, so 0.1.1 lands beside 0.1.0 and the test is reversible.

# 8. Further reading

In this OKF:

- [Distribution](/concepts/platform/distribution.md) — the built half: the
  installer, the update client, side-by-side installs, and a plain list of what
  is not proven.
- [The web platform](/concepts/platform/web-platform.md) — domain, hosting, and
  *every cloud host is disposable*, which is the rule this page inherits.
- [Blocks & domains](/concepts/sections/blocks-and-domains.md) — why a button is
  a rune.
- [Data planes](/concepts/platform/data-planes.md) — why nothing from the admin
  database belongs on a public page.
- [Security](/concepts/platform/security.md) — signed releases, and the seam
  the renderer enforces.

Background, none of it required:

- [Code signing](https://en.wikipedia.org/wiki/Code_signing) and
  [Microsoft SmartScreen](https://en.wikipedia.org/wiki/Microsoft_SmartScreen) —
  §5(a), the biggest obstacle between the button and a running app.
- [SHA-2](https://en.wikipedia.org/wiki/SHA-2) and
  [digital signatures](https://en.wikipedia.org/wiki/Digital_signature) — what a
  checksum does and does not prove.
- [Supply chain attack](https://en.wikipedia.org/wiki/Supply_chain_attack) and
  [trust on first use](https://en.wikipedia.org/wiki/Trust_on_first_use) — the
  shape of the problem signing solves, and what we are relying on instead.
- [Static web page](https://en.wikipedia.org/wiki/Static_web_page) and
  [CDN](https://en.wikipedia.org/wiki/Content_delivery_network) — §2(c) and §3.
- [Same-origin policy](https://en.wikipedia.org/wiki/Same-origin_policy) and
  [Content-Disposition](https://en.wikipedia.org/wiki/MIME#Content-Disposition)
  — why the `download` attribute behaves the way §4 describes.
- [Software versioning](https://en.wikipedia.org/wiki/Software_versioning) —
  what `0.1.1` is claiming.
- [NSIS](https://en.wikipedia.org/wiki/Nullsoft_Scriptable_Install_System) — the
  installer engine Void Mago emits a script for.
- [Portable Executable](https://en.wikipedia.org/wiki/Portable_Executable) and
  [DLL Hell](https://en.wikipedia.org/wiki/DLL_Hell) — the format whose import
  table gave up `libwinpthread-1.dll`, and the problem side-by-side installs
  avoid.
