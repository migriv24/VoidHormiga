---
type: Concept
title: Allomone — execution & logging (derive vs mutate)
description: "Running a derive-only script is a PROJECTION — a pure function over the current state that changes nothing, so it is NOT logged and needs no command. Only MUTATION goes through the dispatcher and is logged/replayable. The command log records deltas to the model; derivation produces no delta."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-05T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). The author's question
(2026-08-05): do Allomone scripts get **logged** when they run? The answer is a
principle worth naming.

# The principle: a log entry marks a CHANGE, not an evaluation

> "A log or CLI command is evoked when there's a real change that happens to the
> dataset." — the author.

Exactly. Void Core's command log is the record of **deltas to the model** —
every mutation is a logged, replayable dispatcher command (ground rule 3). But
**reading and deriving change nothing**, so they produce **no delta** and
therefore **no log entry**. Running a derive-only Allomone script is not an
event to record; it is a **recomputation of a view**.

# Two kinds of Allomone activity

| | **Derivation** (today) | **Mutation** (gated, future) |
|---|---|---|
| what it does | reads runes → produces **annotations** (a color per rune) | **writes** the model (add a tag, set a field, add a link) |
| in Void Core terms | a **projection** — a pure function over current state | a **dispatch** — a logged command |
| logged? | **No.** Nothing changed. | **Yes.** Every write is a command in the log. |
| reproducible by | **re-running** it (pure over data + enabled scripts) | **replaying** the log |
| cost of "undo" | nothing to undo — disable the script, the styling vanishes | ordinary undo of the command(s) |

This is the standard **query/command split** (CQRS; reads vs writes; pure vs
effectful), stated in Hormiga's vocabulary: **derivation is a projection,
mutation is a dispatch.** The line between them is exactly the **derive /
materialize gate** ([domains](/concepts/allomone/domains.md)) — Allomone is
**derive-only** until mutation is unlocked, so *today every script is a
projection and nothing a script does is ever logged.*

# What IS logged around Allomone

The *scripts themselves* are runes in the `allomone` mantle, so **editing** them
is a change and **is** logged:

- **creating / editing / deleting a script** — a change to the script rune's
  `body` → logged dispatcher commands (the transcript that builds the script).
- **enabling / disabling / reordering** a script — a change to its `enabled`
  field or order → logged.
- **running / evaluating** a script — **not logged.** It is `refresh_allo_rules`
  recomputing the `allo_colors` cache from `(data × enabled scripts)`. Change the
  data or the scripts and it recomputes; the recomputation is a view, not a
  delta.

So the log stays a faithful record of *what the org changed*, never cluttered by
*how the app chose to paint it*. Styling is a function of the logged state, not a
part of it.

# Why this is sound

- **Purity → no need to log.** A derivation is referentially transparent over
  `(current runes, enabled scripts)`. Anyone with that state reproduces the exact
  same annotations by re-running — so recording the run would be redundant, and
  worse, would bloat the log with non-events.
- **Termination.** Because a derivation is recomputed live (on every relevant
  change, even per keystroke in the editor), it **must terminate** — which is why
  the interpreter runs under a fuel + call-depth budget
  ([language](/concepts/allomone/language.md)).
- **Determinism.** Same data + same enabled scripts ⇒ same styling, always.

# When mutation lands (the future)

The moment a script is allowed to **write**, that write is a **dispatcher
command** like any other — logged, undoable, replayable, synced. Mutation does
not get a special path; it rejoins the one door (ground rule 3). The design
keeps the two cleanly separated: a script's *reads/derivations* are free and
silent; its *writes* are transactions. A script that both derives and mutates is
simply a projection with, at its end, one or more logged commands — and until the
gate opens, that second half is inert.
