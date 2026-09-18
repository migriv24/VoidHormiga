---
type: Concept
title: Distribution — the installer and the update client
description: How Void Hormiga gets onto a machine that is not the developer's, and how the person on it learns a newer version exists without ever being updated behind their back.
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-09-04T00:00:00Z
---

**Opened 2026-09-04**, when Void Mago staged this repository for the first time
and found two things that would each have broken the first install on somebody
else's machine. This page is what that seam is, what is ours, and the two rules
the update client exists to keep.

Void Mago is `../VoidMago` and has its own OKF; this is Hormiga's view of the
seam, not the source of truth for Mago's half.

# 1. The seam, in one table

Mago is a **build-time** tool. It never runs on a user's machine, and an updater
that needed Mago installed would be one more bootstrap the family cannot afford
— the same argument that keeps Mago itself off Void Core.

| | Void Mago | Void Hormiga |
|---|---|---|
| compiles `void.json` into an installer | ✅ | — |
| writes `mago-receipt.json` at install time | ✅ | reads it |
| `mago feed` → `void-updates.json` | ✅ | one HTTP GET, **only when asked** |
| comparing versions | — | ✅ its own against `latest` |
| **asking the person** | — | ✅ **and never skippably** |
| supplies `sha256` | ✅ | ✅ checks it |
| running the installer | — | ✅ launches it |

Ours is `src/update/` (the decisions, view-free), `src/update/cli.cpp`
(`voidhormiga-cli update`) and `src/ui/updates.cpp` (the prompt).

# 2. There is no Void Hub, and the hub is a file

The idea was raised and declined, and the reasoning survives the decision. A hub
is an application you must install *before* the application you actually wanted.
What it would give a **user** is either unnecessary or is not an application:

- a shared runtime cache — unnecessary. Side-by-side installs each carry their
  own 249 KB `libvoidcore.dll`, and shared runtimes are what DLL hell is made
  of;
- a single place that knows which versions exist — that is **a list, and a list
  is a file**.

So the hub is `void-updates.json`: one static object beside the installers, read
by each application about itself. A browsable index, if one is ever wanted, is a
web page generated from that same file — not a program anybody installs.

# 3. The two rules, and which one is easy to lose

`void.json` commits us:

> no silent updates: the user is told an update exists and chooses it.

That has two halves and only the second is obvious.

**Never install without being told to.** `update::download()` fetches and
verifies; it runs nothing. Launching the installer is a separate call behind a
separate button.

**Never check without being asked to** — the half that is easy to lose. *A check
is a network request a person did not make.* So the preference starts at
`unasked`, and a fresh install's first update-related event is a **question
about the check**, before any request exists. "No, don't ask again" is real: it
is persisted and it is never re-asked.

Two consequences that fall out of taking that seriously:

- **The preference is not `config set`.** Every other setting in the
  application is config-tier and rides the saved org — so "don't ask me about
  updates" would have been an answer attached to whichever database happened to
  be open, and it would have travelled to another device on the next merge,
  answering a question that device was never asked. It lives in
  `%LOCALAPPDATA%/VoidHormiga/updates.json`, which is the **suite** folder: the
  parent of each side-by-side version folder, so the answer survives the update
  it was given for.
- **A failed startup check is not a modal.** It goes to the log. Putting a
  dialog in front of an organization because their café Wi-Fi is captive is how
  you train people to dismiss the one that matters. A check somebody *asked*
  for reports its failure, in the remote's own words.

# 4. Side-by-side is what makes an update reversible

Each version installs into its own folder under `%LOCALAPPDATA%\VoidHormiga\`:

```
%LOCALAPPDATA%\VoidHormiga\
  mago-receipt.json      what landed and why
  updates.json           the answer to "should I check?", ours
  uninstall.exe
  voidhormiga-0.1.0\     voidhormiga.exe, voidhormiga-cli.exe,
                         libvoidcore.dll, AGENT-GUIDE.md, okf\, vendor\fonts\
  voidhormiga-0.1.1\     the same again
```

Per-user (`$LOCALAPPDATA`, no elevation). **The running copy is never touched**,
which is the property that makes "try the update" a decision somebody can take
back. Nothing in this repository ever deletes a version folder.

Repointing the Start Menu shortcut and removing an old version are still open
(Mago's, or Velopack's, later).

## 4a. Off Windows the same rule is kept by the client itself (0.1.4)

Linux and macOS have no installer. What the release carries is the archive the
release runner packs, `VoidHormiga-<version>-<platform>.tar.gz`, holding one
folder of the same name. Mago's feed names that archive (Mago 0.1.8) and hashes
it, **provided the archive is in `--artifacts` when the feed is written**. The
client checks the sha256 as it does on Windows. Then, rather than handing the
file to the OS, it unpacks it **beside** the running install and starts the new
binary with `--state` pointing at the open database:

```
~/apps/
  VoidHormiga-0.1.3-linux-x64/     the running copy, never touched
  VoidHormiga-0.1.4-linux-x64/     unpacked next to it
```

It refuses to unpack over the running folder. If the parent folder is not
writable, it says so and leaves the verified archive where it was downloaded.
`update/update.hpp` `unpack_beside`, tested in `update_smoke` §11 on the Linux
and macOS runners.

# 5. What is NOT proven yet

Stated plainly, because a distribution story that overstates itself is worse
than none:

- **Nothing is signed.** `void.json` says `"signed": "ed25519"` and nothing
  implements it. The feed carries `"signature": null` — present so the client
  can code against the field, null so its absence is not mistaken for a
  decision. Until it exists, `sha256` proves the bytes arrived intact and proves
  **nothing about who made them**, and SmartScreen will warn on every download.
  The update prompt says so, in those words.
- **No installer has been compiled or run.** `mago stage` produces a complete
  package with no unresolved imports and no missing files, verified 2026-09-04;
  `makensis` has not been run and no second machine has installed anything. The
  exit test for this page is the same shape as
  [collaboration](/concepts/platform/collaboration.md)'s: **a second computer**,
  not more code.
- **The feed is not published.** `voidhormiga-cli update --check` reaches
  GitHub and gets a 404, which is the correct answer while no release exists.
  That is a real end-to-end exercise of the transport and the status handling,
  and it is not an exercise of the parser — that is `tests/update_smoke.cpp`,
  against a feed in the shape Mago writes.

# 6. The two blockers, kept because they are a pattern

Mago's first staging run found both, and both were fixes here.

**`libwinpthread-1.dll`.** Both executables imported it. It lives in the ucrt64
toolchain directory and **on no other machine in the world**. No manifest
mentioned it because no manifest could — it is a fact about how the binary was
linked, not about what the project declares. `-static` under MinGW; verified by
reading the PE import directory afterwards, which now names only Windows system
DLLs and `libvoidcore.dll`.

**`fonts/` was promised and did not exist.** The manifest declared it;
`ships_beside_binary` resolves against the *checkout*; there is no `fonts/` in
this repository and there never was. Fixing it found a **third** blocker nobody
had reported: `desktop.cpp` loaded Lato, JetBrains Mono and Font Awesome from
`current_path()/vendor/fonts`, so an installed copy — which has no `vendor/` and
is not launched from a checkout — would have fallen through every
`if (exists)` to `AddFontDefault()` and opened with a bitmap face and **no
icons at all**, every section tab and map marker being a Font Awesome codepoint.

That is the same failure as August's empty `site/fonts/`, in the other
front-end, and the pattern is worth naming:

> **A path that resolves on the developer's machine because two folders happen
> to be the same folder is not resolved; it is unfalsified.** `ship_dir` and
> `base_dir` are equal in a build tree and different everywhere else.

Both are now staged under `vendor/fonts/` — the name they actually have — so
the build tree and the install tree have one shape. See
[miga-format](/concepts/platform/miga-format.md) for the other side of that
line: what ships beside the binary versus what belongs to the organization.

# 6b. The other end: how a stranger gets it

Everything above is about the machine an installer lands on. The step before it
— a person who has never heard of Hormiga, on a web page, deciding to click — is
[the download page](/concepts/platform/download-page.md), which is a **plan**
rather than a record. Its two load-bearing decisions:

- **The installer lives in GitHub Releases, not in the website.** Release assets
  sit outside the git object database, are served from a CDN, and can be
  deleted; a 25 MB binary in a Pages branch is in that repository's history
  forever, once per release.
- **The website's button does not carry a version.** A second, stable-named copy
  of each release's installer makes
  `releases/latest/download/VoidHormiga-windows-x64-setup.exe` permanently
  correct, so publishing a new version never requires redeploying the site — and
  the page can never be stale about a number, because it never states one.

# 7. Related

- [Security](/concepts/platform/security.md) — signed releases, and why an
  unsigned one says so.
- [Collaboration](/concepts/platform/collaboration.md) — the other page whose
  exit test is a second machine.
- [Roadmap](/roadmap.md) — where this sits.
