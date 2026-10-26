---
type: Index
title: Contributing — how somebody who is not the author changes Hormiga
description: "The contribution pipeline as its own thing: the stages a report, a request or a change passes through, what each one checks, who may do what, and the rule underneath all of it — roles are accounts, never machines. Opened 2026-10-23."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-23T00:00:00Z
---

**Opened 2026-10-23, at the author's direction:** contributions had been
thought about only in passing, and should be a pipeline of their own. The
second half of the direction shapes everything below: *do not assume this
device will always be the admin.*

Until now, every way into this project ran through one person at one
computer. Issues were read there. Messages between Void projects were files
on that disk (CLAUDE.md rule 4). The privacy names list was a file there,
and so were the release keys. That works for one person and one machine, and
it stops working the day there is a second person, a new laptop, or a
maintainer who is not the author. [Void Mago's own open list](../../../../VoidMago/okf/index.md)
already says this about the sibling layout: it "works for one developer and
does not survive a second one."

# The rule underneath

> **A role is an account, not a device.** Anything a maintainer does, they
> can do from any computer signed in to a maintainer's GitHub account. Any
> state the pipeline needs lives in the repository, in its settings, or in its
> secrets, never only on one disk.

| what | where it lives | not |
|---|---|---|
| who the maintainers are | `.github/CODEOWNERS` and the repository's roles | whoever is logged in here |
| the private-terms list | the `PRIVATE_TERMS` repository secret, plus each maintainer's local copy | only `.private-terms` on one machine |
| security reports | GitHub private advisories | one person's inbox |
| triage state | issue labels | a notes file |
| what a release contains | the tag and CI | a local `build/` folder |

Where the project does not meet this yet, [maintainers](/concepts/contributing/maintainers.md)
says so (release signing and the Windows installer are still built on one
machine).

# The stages

```
  someone            the tracker              a maintainer           main
  ───────            ───────────              ────────────           ────
  report ──────────► issue form ──► intake ──► triage ──► change ──► review ──► merge ──► release
  (voidhormiga-cli   (privacy        scan        labels     (theirs   CI + the            notes credit
   report)            checkbox)      labels                  or ours)  names check          the issue
                                     privacy-review
```

1. **Report.** A person who uses Hormiga runs `voidhormiga-cli report`. It prints
   what the build is and a link that fills in the form. It knows nothing about
   their database or their machine, by construction. See
   [intake](/concepts/contributing/intake.md).
2. **Issue form.** There are no blank issues: every issue goes through a form
   that says, before anything else, what must stay out. Security problems go to
   a private advisory instead.
3. **Intake scan.** `.github/workflows/intake.yml` checks every new issue,
   pull request and comment with the same checks as the repository lint, plus
   the names. On a finding it labels the item `privacy-review` and comments with
   line numbers only.
4. **Triage.** A maintainer labels it: `bug` / `feature`, `needs-repro`,
   `upstream` (the change belongs to a sibling), `design` (a decision is owed
   first, and it goes to [the questions](/developer_questions.md)), or
   `declined` with a reason. The goal of triage is a reproduction on the Cat
   Colony, because that is the only kind of reproduction that can live here.
5. **Change.** Anyone may open a pull request; `CONTRIBUTING.md` is its brief and
   the pull request form is its checklist. The ground rules are CLAUDE.md's,
   written for people.
6. **Review.** CI builds on three platforms and runs the tests and the privacy
   lint. A fork's pull request cannot see secrets, so a maintainer runs
   `tools/maintainer/review-pr.sh <n>`, which checks the PR's tree for the names
   with the maintainer's own lint, without running the PR's code.
7. **Merge and release.** Only maintainers merge and release. The push to
   `main` runs the privacy check again with the names, which catches what a
   merge brought in from any machine.

# What a contribution may not carry

Everything in [what may be public](/concepts/foundation/what-may-be-public.md),
and contributions make it more urgent rather than less. The people most
likely to report a Hormiga bug are working inside a real organization's
database at that moment, so the obvious thing to paste is that organization's
data. Every stage above has a privacy check for that reason. A contribution
that needs real data to show a problem gets the problem rebuilt on the Cat
Colony, and the real data stays on the person's computer.

# The pages

- [Intake](/concepts/contributing/intake.md): the report verb, the forms, the
  scan, security reports, and what a person is asked to leave out.
- [Maintainers](/concepts/contributing/maintainers.md): what a maintainer needs,
  on any machine; triage, review and merge; the setup script; what is still
  tied to one device.
- Void projects talking to each other: decided 2026-10-24, an issue on the
  recipient's repository labelled `family-message` is the message, and the local
  `MESSAGE_FOR_*` file is its draft. `mago notify --issue` is the next Mago step.
- `main` is protected (decided 2026-10-24): pull requests and the privacy check,
  admins may still push in an emergency. It is turned on with
  `setup-github.sh --protect` once the workflows are pushed.

# What is built and what is not (2026-10-23)

**Built, in the working tree:**

- `voidhormiga-cli report`;
- `CONTRIBUTING.md` and `SECURITY.md`;
- the issue forms, the PR form and `CODEOWNERS`;
- the `intake` and `privacy` workflows;
- `tools/intake_scan.py` and the two maintainer scripts.

**Not done:** nothing is pushed, and none of the repository settings are
applied. Pushing workflow files needs a token with `workflow` scope, and the
settings need `tools/maintainer/setup-github.sh` run by a maintainer.
The desktop and phone have no "Report a problem" button yet (only the CLI
verb). The Mago half is a message, not code.
