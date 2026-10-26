---
type: Concept
title: Maintainers — the roles, what each needs on any machine, and what is still tied to one
description: "Who may triage, review, merge and release; the setup a maintainer needs on a new computer; the commands for each stage; and an honest list of what still lives on one device (the names list's master copy, the Windows installer build, the Android signing key) with the way out of each."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-10-23T00:00:00Z
---

# The roles

| role | may | how it is granted |
|---|---|---|
| **contributor** | open issues and pull requests | nothing: anyone with a GitHub account |
| **triager** | label, close, ask for a reproduction | the repository's Triage role |
| **maintainer** | review, merge, run the setup script, cut releases | the Maintain or Admin role, and a line in `.github/CODEOWNERS` |

All of these are **GitHub accounts**. None is a machine, and none is "the
computer `gh` is signed in on". Today there is one maintainer account. The
pipeline is written so that a second one, or the same one on a new laptop,
needs nothing from the first machine.

# A maintainer on a new computer

1. `gh auth login` as a maintainer account. Pushing a change under
   `.github/workflows/` needs the `workflow` scope (`gh auth refresh -s workflow`);
   everything else needs `repo`.
2. Lay out the family as siblings: `mago bootstrap <this repository's URL> --full`
   (decided 2026-10-24: bootstrap is Void Mago's, and is built). Until Mago is
   public, the clone lines in `CONTRIBUTING.md` do the same by hand.
3. `git config core.hooksPath tools/hooks` (the privacy hook).
4. `git config user.email <id>+<login>@users.noreply.github.com`. Every commit
   publishes its author email.
5. Get the **private-terms list** into `.private-terms` at the repo root. Where
   the master copy lives is [Q126](/developer_questions.md). A repository secret
   cannot be read back, so the secret cannot be the master.

Nothing else. In particular, no file from the previous machine is needed to
triage, review or merge.

# The stages, as commands

**Set up the repository** (once, and again after any change to it):

    tools/maintainer/setup-github.sh            # labels, private advisories
    tools/maintainer/setup-github.sh --terms    # also upload .private-terms as PRIVATE_TERMS
    tools/maintainer/setup-github.sh --protect  # also require PRs and the privacy check on main

It acts on whatever repository `origin` points at, so it follows the project if
it moves to an organization.

**Triage**:

    gh issue list --label needs-triage
    gh issue list --label privacy-review      # first: these may need a revision deleted

A `privacy-review` item comes first. Edit out what the scan found, delete the old
revision from the edit history (the issue's "edited" menu), remove any
attachment that shows real data, and remove the label. Then the ordinary work:
reproduce on the Cat Colony and label it.

- `needs-repro`: it cannot be shown on the Cat Colony yet.
- `upstream`: the fix belongs to a sibling. Write the sibling message (CLAUDE.md
  rule 4) and link it.
- `design`: it needs a decision. Open the question in `okf/developer_questions.md`
  with a lean.
- `declined`: say why in the thread, in terms of the design.

**Review**:

    tools/maintainer/review-pr.sh <n>

It puts the PR's head in a temporary worktree, shows what changed, and runs
*your* checkout's privacy lint over *their* tree with the names. It runs nothing
from the PR, and a PR that edits the lint cannot weaken the check that reviews
it. CI has already built it on three platforms and run the tests.

Then read the diff the way [what may be public](/concepts/foundation/what-may-be-public.md)
says, as a stranger would. And read it against CLAUDE.md's ground rules:
the dispatcher is the only door, privacy at the seams, no sibling patched, no
dependency unvendored, the OKF updated.

**Merge**: squash, with a message that says what changed and credits the
contributor. The push to `main` re-runs the privacy check with the names.

**Release**: see [distribution](/concepts/platform/distribution.md). The
release notes credit the issues and pull requests in it.

# What is still tied to one device

Said plainly, because the author asked for the opposite and these are where it is
not true yet.

| what | where it is today | the way out |
|---|---|---|
| the private-terms list's master copy | one machine's `.private-terms` | a private place every maintainer can read ([Q126](/developer_questions.md)) |
| the Windows installer | built by `mago` and `makensis` on one machine, uploaded by hand | **decided 2026-10-24:** built in `release.yml` like Linux and macOS, once Mago is public so the runner can `mago bootstrap` |
| the Android signing key | `android/hormiga-release.keystore` on one machine, backed up by the author | **decided 2026-10-24:** an encrypted repository secret used by a release job, with the offline backup kept |
| messages between Void projects | `MESSAGE_FOR_*` files on one disk | **decided 2026-10-24:** an issue on the recipient's repository, labelled `family-message`, is the message, and the local file is its draft. A message that would quote unreleased or private work stays a local file |
| Void Hammer | a folder with no repository | a public remote; until then a contributor builds without networking, and CI says so |
| the sibling layout | cloned by hand | **built 2026-10-24:** `mago bootstrap` |

The first three are why a maintainer on a new machine can triage, review and
merge but cannot yet *release*.

# Handing over

If the author stops maintaining, or adds a maintainer:

1. Give the account the Maintain role and add it to `CODEOWNERS`.
2. Share the private-terms master (Q126) and the Android key backup, both
   outside this repository.
3. The new maintainer runs the setup on their own computer (above), then
   `tools/maintainer/setup-github.sh` once to confirm the settings.
