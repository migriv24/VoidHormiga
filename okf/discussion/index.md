---
type: Index
title: Discussion
description: The speculative part of the OKF, and intentionally so. After a mistake, why we think it happened and what might have prevented it, written as hypotheses that may turn out wrong. Not needed to start working.
tags: [status:current, audience:all, confidence:speculative]
timestamp: 2026-11-04T00:00:00Z
---

# Discussion

Everything else in this OKF records what exists, and is kept short so an
agent can find out what is going on and start working. **This folder is
different, on purpose.** It holds reflection: why a thing went wrong, what
might have caught it sooner, and guesses about the next one. It is allowed to
be unnecessary, and allowed to be wrong.

**An agent starting work can skip it.** Nothing here is a requirement or a
design. When a hypothesis here proves true and changes how we work, the change
goes into a concept, a test or `CLAUDE.md`, and the entry links to it. The
entry itself stays as it was written.

## How an entry is written

- **One file per event**, named `<YYYY-MM-DD>-<topic>.md`, dated to when the
  mistake was found.
- **Short anyway.** A page or two: what happened (facts, linked to the
  [log](/log.md)), then the hypotheses, then what changed and what is still
  unknown.
- **Hypotheses are marked as hypotheses**, with what would show each one
  wrong, if anything would.
- **Never rewritten to look right later.** A later entry can answer an earlier
  one, and the earlier one gains a line that points to it.
- **The [public rule](/concepts/foundation/what-may-be-public.md) still
  applies.** A mistake found on somebody's database is written as a fact about
  Hormiga, with nothing that names the database or its people.

## Entries

- [2026-11-03 — the database that would not open](/discussion/2026-11-03-the-database-that-would-not-open.md):
  0.2.4 shipped as "the crash fix" and the stress database still exhausted
  memory. We measured the library's path, not the one the person uses.
