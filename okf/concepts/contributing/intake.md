---
type: Concept
title: Intake — how a report reaches the project without carrying anyone's data
description: "voidhormiga-cli report, the issue forms, the intake scan, and private security reports. The design constraint is that the person filing is usually inside a real organization's database when it breaks, so every piece is built to keep that data on their computer."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-23T00:00:00Z
---

Somebody's Hormiga misbehaved. They are probably not a developer, they are
probably in the middle of real work in a real organization's database, and they
are probably in a hurry. Everything on this page assumes that person.

# `voidhormiga-cli report`

    voidhormiga-cli report             a bug: this build, and the link to the form
    voidhormiga-cli report --feature   the same, for the feature form

It prints three facts and a link:

    - Void Hormiga: 0.2.2
    - Platform: windows-x64
    - Networking built in: yes

    https://github.com/<repository>/issues/new?template=bug.yml&version=0.2.2&platform=windows-x64

GitHub fills an issue form's field when a query parameter matches its `id`, so
the link opens the form with the version and platform already in place.

**What it may know is the design, and the list is short.** It knows only what
the *binary* is: its version, platform, and whether networking was compiled in.
It never opens a database, never reads the working folder, a path, the user's
name or an environment variable, and makes no network request. Every value it
prints is a compile-time constant. That makes it safe to paste without reading,
which is how people actually paste. `tools/check_layering.py` enforces it:
`src/report/` may include nothing from the application, so a path or a profile
can never be one function call away from a public tracker.

The repository URL comes from `void.json`'s `repository` field, read by CMake,
so the link follows the project if it moves. It does not depend on who compiled
the binary.

Like `update`, it is a verb of the process and needs no session, because an
installation that will not open its database is when people most want it. It is
tested in `tests/headless_smoke.sh`, which checks that it runs in an empty folder,
creates nothing there, and names no path.

**Not yet:** a "Report a problem" button in the desktop's Settings and on the
phone. When it comes it will call the same `report::facts()` and
`report::issue_url()`. Opening a browser is something the person does, never
the app on its own.

# The forms

`.github/ISSUE_TEMPLATE/` has two forms and no blank issue:

- **Something is wrong** (`bug.yml`) asks for the version, platform, where it
  happened, what happened and the steps. It asks for steps on the Cat Colony, or
  the *shape* of the data ("an event with no date and two fliers") rather than
  the data itself.
- **I need Hormiga to do something** (`feature.yml`) asks for the kind of
  organization and the task, and a role rather than a person.

Each form opens with the privacy warning and a required checkbox. A checkbox
proves nothing, but it is the one moment the person reads the rule before
writing.

# The intake scan

`.github/workflows/intake.yml` runs on every new or edited issue, pull request
and comment. It runs `tools/intake_scan.py`, which uses `lint_public.py`'s
`scan_text()`: emails outside the example domains, phone numbers outside 555,
home directories, credential shapes, and the names from the `PRIVATE_TERMS`
secret.

On a finding it labels the item `privacy-review` and comments **with line
numbers and reasons only, never the matched text.** A bot that quoted a phone
number would publish it a second time.

What it cannot do, said plainly:

- It cannot unpublish. By the time it runs, the text is public. The comment
  asks the person to edit it out and tells them a maintainer will remove the
  old revision from the edit history. That revision stays visible until a
  maintainer deletes it, and a pasted token stays live until it is revoked.
- It cannot see attachments. A screenshot or an uploaded file is not scanned.
  The forms ask for none, and a maintainer who finds one deletes it.
- It knows patterns and names, not meaning. A sentence that describes somebody
  passes.

It runs with `pull_request_target`, so it can label a fork's pull request, and
it stays safe because it checks out only the default branch's `tools/` and reads
the PR text from the event, never the PR's code.

# Security reports

`SECURITY.md` sends them to GitHub's private advisory form. That form reaches
whoever maintains the project at the time, with no email address on anyone's
machine, and the fix and the advisory are published together. What counts is
anything that moves data past a seam Hormiga promises: private fields reaching
an output, consent tags bypassed, one database reaching another, keys leaving
the vault, an update that is not what the feed says.

Turning the form on is a repository setting
(`tools/maintainer/setup-github.sh` does it).

# What the person is asked to leave out

The same list everywhere (the forms, `report`, `CONTRIBUTING.md`,
`SECURITY.md`), so that it reads as one rule:

- an organization's database or `.miga`, and its logs;
- names, emails, phone numbers and addresses;
- screenshots showing real people;
- paths on their computer;
- keys and tokens.

Show it on the Cat Colony.
