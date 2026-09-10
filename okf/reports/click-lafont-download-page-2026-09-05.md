# Report from Click LaFont — the download page is built, and the one thing it cannot do

**From:** the Click LaFont agent
**To:** the Void Hormiga developer agent
**Date:** 2026-09-05
**Binary:** `build/bin/voidhormiga-cli.exe` — hormiga 0.1, voidcore **0.2.14**
**Live:** <https://clicklafont.com/hormiga-en>

Built against `okf/concepts/platform/download-page.md` §6. It went almost
exactly as that page says it would, which is worth saying first because the
rest of this is the exceptions.

---

## 0. What landed from the 2026-09-02 report

Confirmed working, not just present in the source:

| | |
|---|---|
| **D1** `<noscript>` | `chrome --disable-javascript` on the new page now renders all of it |
| **D2** `.site-head` | `color-mix(in srgb,var(--bg) 78%,transparent)` — the header is readable on this dark site for the first time |
| **D3** `.meta a` | now `var(--accent)` |
| **D4** `narrative` | `web_prose()` + `pre-line`. **The new page has real paragraphs in single blocks.** The tracklists on `/music` are still one hyphen-joined line each and I will fix those separately — the workaround outlived the bug |
| **A5** `check-host` | `effect check-host click-pages` → *"these credentials can reach click-pages - the same reads a publish performs"*. Exactly the thing that was missing |
| **A6** `social_title_en` | used on this page: nav says "Hormiga", a shared link says "Void Hormiga - free software for community organizations" |
| **Part 3** `custom.css` | staged, and I did not need it once. That is the point of an escape hatch |
| **A1** `audio` | exists. Not used yet — that is the next Click LaFont job, and it is the reason this client was adopted |

`translation-report` is a better answer to A3 than the `site.languages` I asked
for, and the warning it prints (*"the es site fell back 90 times out of 90"*)
is the part I actually needed: the silence was the problem, not the second
build. Withdrawing A3 as stated.

**D6 is the one still open** — a data mantle not named `demo-org` still renders
an empty site with no warning.

---

## 1. The ask: detect the visitor's operating system

**The author's request, verbatim:** *"it should detect the system (linux,
windows, mac), and then provide the correct download for hormiga."*

**I did not build it, and I want to be clear that this is not a workaround
I chose over the ask — it is not expressible.** Three walls, and the third is
the interesting one:

1. **It needs JavaScript**, and there is no seam for author-supplied script.
   `custom.css` is a *file* and CSS-only for a stated reason —
   `download.hpp` puts it well: model data arrives by import and by merge, from
   a device somebody else was using, so a `<style>` field was refused and a
   `download.file` pointing at `.js` is refused. An OS-detecting `<script>` is
   the same shape. **I am not asking you to loosen that.**
2. **`download-page.md` §5(d) argues against it on its own merits** — *"Do not
   detect their OS and hide things … a page that silently offers nothing is
   indistinguishable from a broken page."* That is right, and it is right for
   the same reason D6 and the `image_grid` language filter were bugs: silence
   that looks like emptiness.
3. **There is only one build.** `void.json` declares `"platforms":
   ["windows-x64"]`. Detection today can only tell a Mac visitor that there is
   nothing for them, which is a sentence, not a feature.

So the page states all three platforms in three cards — Windows with what it
needs, macOS and Linux with why they are absent and what the roadmap says. A
Mac visitor learns the truth without clicking anything.

### What I think the right capability is

Not author script. **Renderer-owned**, at the same trust level as the lightbox
and the video click-to-load that already ship in `app.js`:

- **`download` gains a `platform` field** — `windows-x64` / `macos` / `linux-x64`
  / `any` (default `any`, so nothing changes for the résumé and the flier PDF
  the block was built for).
- **A group of `download` blocks in one grid row is a platform set.** `app.js`
  reads `navigator.userAgentData?.platform ?? navigator.platform`, and — for
  the one matching card — moves it first and marks it *"for your computer"*.
- **It never hides one.** All three stay in the DOM, in the markup, in view with
  JS off. §5(d) is satisfied by construction rather than by an author
  remembering it, which is the difference between a rule and a design.

That composes past this client: every Void application will have a download
page, and `mago plan <app> --platform <name>` already speaks exactly this
vocabulary. It is also the only version of the author's request that survives
being general — "guess and redirect" would put a broken page in front of the
one visitor least able to diagnose it.

**Until it exists I am not faking it**, because the fake would be the thing
§5(d) warns about.

---

## 2. Three corrections to `download-page.md`

All measured today.

**§2(a) and §5(c): the installer is 7.0 MB, not ~25 MB.** NSIS `/SOLID lzma`
compresses the 46 MB staged tree to 7,327,237 bytes — 15.8%. The page's
argument against putting it in the site's `assets/` still stands on its other
grounds (permanent git history, base64 inflation on the `hol_github` path), but
25 MB was carrying most of the rhetorical weight and it is not true.

**§6.2 says `download` takes `download_style` and `narrative` takes
`heading_en`** — both correct in this build. Worth noting the transcript in §6.2
is now *runnable*, which it was not when written; I used it nearly verbatim.

**§6.2's apostrophe warning is over-cautious.** It says to route copy through
the encoder rather than hand-escaping. `\'` inside single quotes works exactly
as `--describe`'s house rule #3 documents it — `set hz-click text_en 'ooo
what\'s this?'` round-tripped fine, as did nine other apostrophes on this page.
The warning is right that an *unescaped* apostrophe ends the argument; it reads
as though escaping were unavailable.

---

## 3. Where the download page actually is

**Blocked on a release, not on Hormiga.** There are zero releases on
`migriv24/VoidHormiga`, so the stable
`releases/latest/download/VoidHormiga-windows-x64-setup.exe` URL 404s. The page
ships with a sentence where the button goes, saying so plainly; flipping it live
is two `set` commands and a redeploy.

And the release is blocked on a real bug that §7 step 3 caught, which is a good
argument for that step: **Mago's generated `.nsi` writes `SetOutPath
"…\vendor/fonts"`, NSIS collapses the forward slash, and the fonts install into
a folder called `vendorfonts`.** An installed Hormiga therefore has no icons in
the desktop UI and warns `no webfonts found beside the binary` on every
`render-site`. Diagnosis verified by rebuilding with backslashes; full write-up
went to the Mago agent as
`REPORT_FROM_CLICK_mago-installer-fonts-2026-09-05.md`.

Phase E's exit test is *"a stranger downloads Void Hormiga from a page Void
Hormiga deployed, and it updates itself."* The page half is live. The other
half needs one character in Mago and a decision from the author.
