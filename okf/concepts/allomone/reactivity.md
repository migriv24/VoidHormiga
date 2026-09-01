---
type: Concept
title: Allomone — control, time, and reactivity
description: "Two computational models under one engine: a quiescent least-fixpoint by default (no clock), delta-reactivity always on (react to tag changes), and an opt-in application clock that enables loops and animation at real compute cost (the Settings toggle, q21c)."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md). `if`/`if-else` are core and
cheap ([blocks](/concepts/allomone/blocks.md)). **Loops and time are a different
computational model**, and the author's ambivalence is exactly right — so
Allomone runs in **two clearly separated modes**, and the expensive one is
opt-in.

> **Status (2026-08-05).** Mode 1 (quiescent) is what runs today: scripts
> re-evaluate on change via `refresh_allo_rules`, derive-only. Mode 2 (the
> **clock**) is **designed here but NOT built** — no `AllomoneClock`, no
> temporal blocks yet. The one piece that landed early is the **termination
> guard** the clock depends on: the interpreter now runs under a **fuel budget +
> call-depth cap** (built with functions/recursion, 2026-08-05 —
> [language](/concepts/allomone/language.md)), so unbounded work is *already*
> caught. That is the safety precondition for a per-tick clock; the clock itself
> waits on the language maturing (roadmap increment 8).

# Mode 1 — quiescent (default, always available)

The engine evaluates the rule set to a **least fixpoint** — keep firing until
nothing new is derived — and then **idles**. No clock, no per-frame work; it
recomputes only when something changes. This is the correct model for "rules,"
and it is what runs everywhere by default.

**But it is still reactive — to *changes*, not to a clock.** Every mutation is a
**dispatcher command**, so the engine re-evaluates **incrementally** on the
command stream (Rete + differential dataflow: re-test only the rules a change can
affect). This powers the **delta triggers** — `on add <tag>`, `on remove`,
`on tag change`, with `previous tags` vs new. **Delta reactivity stays ON even
when the clock is off** (author, 2026-08-03): deltas and the clock are **tracked
by completely different mechanisms**, so one has nothing to do with the other.

**How deltas are tracked — as graph data (author's idea, adopted).** A delta is
not a fleeting event on a queue that's gone once handled; it is **appended to a
history the graph itself carries**. Each rune (and each mantle) keeps an ordered
**delta log** of its own tag/field changes — a small append-only stack on the
node. This is elegant for several reasons:
- It keeps the reactive substrate **inside the graph** (no side-channel event
  bus to fall out of sync), consistent with the stigmergic model
  ([paradigm](/concepts/allomone/paradigm.md) §4) — the environment *remembers*.
- `previous tags` vs `new tags` is then just **reading the last delta** on the
  rune; time-travel comparisons ("tags as of yesterday") become possible later.
- It composes with the dispatcher: since every change is already a logged
  command, the per-rune delta log is a **projection** of that log filtered to the
  rune — cheap to maintain, and re-derivable if ever lost.
Deltas are therefore **always on and cheap** — they are written when a change
happens (never polled), and reading them is a local lookup on the rune.

# Mode 2 — clocked (opt-in, for loops & animation)

`repeat`, `forever`, and `wait` need something the quiescent engine does not
have: a **tick**. They turn Allomone into a **synchronous reactive / FRP** system
(the Esterel / Lustre / functional-reactive family), where a **global clock**
drives re-evaluation each tick, so a rule can *change over time* — the pretty
case the author wants: **a card cycling through colors drawn from its tags**.

**How it works (the mechanism the author asked for):**

- A **single application clock** (`AllomoneClock`) ticks at a fixed cadence
  (say ~15–30 Hz, independent of the render frame rate). It is the **one** timer;
  no rule spawns its own.
- On each tick the clock advances a counter and re-evaluates only the rules that
  contain a **temporal block** (`wait`, `repeat`, `forever`) or read the clock
  (`timer`, `tick`). Purely declarative rules are untouched — they are still
  fixpoint-evaluated on change, not on tick.
- A `forever`/`repeat` body is not an actual busy loop; it is compiled to a
  **state machine advanced one step per tick** (the standard FRP treatment). So
  "forever cycle colors" = "on each tick, pick the next color" — bounded work per
  tick, never a spin.
- Because the clock is a real cost (a standing per-tick pass across Hormiga), it
  is a **Settings toggle**: `ui.allomone.clock` (**off by default**). With it
  off, the clock never runs, temporal blocks are **inert** (and the editor says
  so with a hint), and everything else — including delta reactivity — works
  normally.

So the split is principled, not a hack: **timeless least-fixpoint** (Datalog-
style, terminating, cheap) vs **clocked reactivity** (FRP-style, animated,
costed). Turning the clock off does not cripple the engine; it removes only the
animation family, which is the honest trade the author described.

# Summary table

| capability | needs | default | notes |
|---|---|---|---|
| `if` / `if-else`, conditions, actions | nothing | on | terminating fixpoint |
| delta triggers (`on tag change`) | dispatcher stream | **on** | incremental, cheap, always on |
| spectral conditions (centrality) | budgeted recompute | on | like the physics view's budget |
| `repeat` / `forever` / `wait` (animation) | the app clock | **off** | Settings `ui.allomone.clock`; inert when off |

# Guarantees

- With the clock **off**, Allomone always **terminates** (fixpoint) and adds no
  per-frame cost beyond re-evaluating on actual changes.
- With the clock **on**, per-tick work is **bounded** (state-machine step, not a
  loop), and the whole subsystem is one timer that can be switched off instantly.
- Delta reactivity is **orthogonal** to the clock: it is about *changes to the
  graph*, the clock is about *the passage of time*. The author wanted the former
  always on and the latter optional — this design delivers exactly that.
