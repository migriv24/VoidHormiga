---
type: Concept
title: What may be public — the repository carries Hormiga, and nothing it was used on
description: "The publication rule for this repository and everything built from it: only the development of Void Hormiga is committed. Nothing from a real database, a client, another project's folder or this machine — not data, not reports about data, not names. Why the rule failed twice as prose, the mechanisms that now hold it (gitignore, lint, hook, staging), and what an agent does instead with a report about somebody's database."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-10-23T00:00:00Z
---

**This repository is public, and so is everything built from it.** The source,
the OKF, `AGENT-GUIDE.md` and `okf/` are copied beside the binary and ship
inside every installer, archive and release. Whatever is committed here is
published to strangers more than once: in git's history, and in every release
built after it.

So the rule is narrow, and it has no exceptions:

> **Only the development of Void Hormiga is committed.** Nothing that came from
> using it: no organization's data, no report about a database, no client's
> name, no person, no place they can be found, nothing from another project's
> folder, and nothing about this machine.

That includes things that look harmless one at a time. A client's name, the
title of a real event, a slug from a real database, a count of somebody's
contacts, a description of who an organization serves, a local path, a
quotation from a person about themselves: each is a piece of somebody else's
life that was handed to Hormiga to keep, and none of it is ours to publish.

# Why this page exists

The rule existed in prose and failed twice.

- **Before publication (2026-09-01)** a partner organization's field reports,
  data and name were removed from the repository and the log, and the log says
  so. `tools/lint_okf_links.py` even kept a comment explaining the empty
  `okf/reports/` it skipped.
- **The next day** that folder came back. Field reports from a second client and
  from a personal portfolio were committed to `okf/reports/`, the index described
  the folder as the place such reports are *kept*, and the reports quoted the
  client's site, its local folder, a phone number and a personal email. The
  OKF ships beside the binary, so they went out inside every release from 0.1.0
  to 0.2.2. Names and descriptions of the clients and of the partner
  organization's members spread from the reports into the log, the questions,
  the concepts, the guide, the source comments and the tests. Found and removed
  2026-10-23.

The lesson is the one this codebase keeps learning elsewhere: **a rule that
lives only in prose is a convention, and a convention is what the next agent
breaks in good faith.** The second failure was not careless. It followed the
index, which said reports were kept, while CLAUDE.md said member data never
enters the repo. Two documents disagreed, and the more specific one won.

# What it covers

| never committed | why it is tempting | what to write instead |
|---|---|---|
| a report written against a real database | it is a measurement, and measurements are evidence | the finding, as a fact about Hormiga, in the log or the code |
| a client's or organization's name, site or domain | it makes the history readable | "a client", "the partner organization", "a field report" |
| real slugs, titles, venues, file names | they are the exact strings a bug printed | `acme-open-house`, `Riverton Community Center`, `banner.webp` |
| counts and shapes of real data (contacts, emails, phones) | they are why a privacy rule exists | "dozens of contacts, most with a personal email" |
| who an organization serves, and what endangers them | it is the reason for the rule | "members who can be harmed by being found" |
| a person's words about themselves, their photo, their documents | the request was vivid | the request, restated as a requirement |
| a phone number or email that is not `555-01xx` or `example.org` | a real one was the failing value | `541-555-0100`, `ana@example.org` |
| a home directory or a machine's paths | it is what the error said | `C:\Users\<name>\…`, or a path relative to the repo |
| a credential, even a revoked one | it was in the transcript | "a token", and revoke it |
| another project's working folder, notes or messages | an agent there left a useful note | a `MESSAGE_FOR_*` file, which is gitignored (CLAUDE.md rule 4) |

**The fictional colony is the way to show real behaviour.** The Cat Dataset and
the Riverton examples exist so that every example in this repository can be
reproduced without anybody's data. When a bug needs a reproduction, rebuild it
there.

# Where a report about a database goes

**Beside that database, never here.** An agent working in an organization's
folder, a client's folder or any other project writes its report there. Its
findings reach Hormiga in one of two ways:

1. **A message**, `MESSAGE_FOR_VOIDHORMIGA_<sender>-<topic>-<date>.md` at this
   repository's root. Messages are gitignored. The receiving session folds the
   *finding* into `okf/log.md` and the code, written as a fact about Hormiga
   with the client unnamed, and deletes the message.
2. **The author**, who relays what they choose to.

`okf/reports/` is gone and ignored. Development evidence about this codebase,
such as the networking audit, lives in `okf/audits/`. An audit measures Hormiga
and its siblings and nothing that runs on them.

# The mechanisms

Each one closes a way the prose rule was broken.

- **`.gitignore`** ignores `okf/reports/`, correspondence, databases and their
  sidecars, `HormigaFiles/`, and `.private-terms`.
- **`tools/lint_public.py`** runs as the `hormiga_public` test, so CI fails on a
  violation. Over every tracked file it checks forbidden paths, email addresses
  outside the example domains, phone numbers outside 555, home directories and
  credential shapes.
- **The private-terms file.** The names that must never appear cannot be
  listed in the repository, because the list is itself the secret. They live in
  `.private-terms` at the root, one per line, gitignored, and the lint reads it
  when it is present (`HORMIGA_PRIVATE_TERMS` names another file). **Add a line
  the day a new client, organization or private project starts using Hormiga.**
- **`tools/hooks/pre-commit`** runs the lint over what is *staged*, terms
  included, and refuses the commit. It also refuses when the commit author
  email names a private term, because every commit publishes it. Turn it on
  once per clone:

      git config core.hooksPath tools/hooks

- **Staging excludes `okf/reports/`.** The build copies `okf/` beside the binary
  and then deletes `bin/okf/reports`, and `void.json` lists the OKF's parts
  instead of the whole folder. A report left on disk by mistake cannot reach an
  installer.

None of this is a reason to relax. The lint knows patterns and the names it is
given. It cannot know that a sentence describes somebody. **Before committing,
read the diff as a stranger would**, and ask of every proper noun, number and
quotation whether it came from Hormiga's development or from someone using it.

# What history still holds

Removing a file does not remove it from git's history or from a release asset
that was built while it existed. When something private is found here, three
separate things are owed, and the author decides each one:

1. **The working tree**: remove or rewrite it, here, in the open.
2. **The history**: a rewrite (`git filter-repo`) and a force-push, which
   changes every commit id and every clone's ancestry.
3. **The releases**: every installer and archive built while it existed carries
   it. They must be rebuilt or deleted.

A credential that was ever committed is revoked, whatever else happens.
