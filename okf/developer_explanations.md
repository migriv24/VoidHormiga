---
type: Explanations
title: Developer explanations
description: A living, plain-language tutor doc — the author asks, this file explains. Concepts from the dev questions and concepts, unpacked without assuming the jargon. Grows and gets edited over time.
tags: [status:current, audience:author, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

This file is a **changing tutor doc**: when a concept or one of my questions
needs unpacking, the explanation lands here in plain language (the formal
version lives in the concepts). Newest topics on top. If something here becomes
settled and obvious, it can be trimmed.

---

## Specificity as "logical depth", and the recency/ID worry (q21d)

**Your instinct:** the rule that took a *longer path of logic* to reach its
conclusion should win. And overlapping rules are a **feature** — "all dogs blue"
plus "small european dogs light blue" — not a bug to warn about. Both correct;
here's the precise machinery.

**Two rules that overlap — who wins?** The **more specific** one, *for the subset
it covers*. "Small european dogs light blue" wins for small european dogs; "all
dogs blue" still governs every other dog. This is a real, old idea in logic:
**defeasible reasoning with specificity** — the "birds fly, but penguins don't"
pattern. Overlap is where the interesting behavior lives, so Allomone leans into
it and resolves quietly (no warnings).

**How we measure "more specific" — two layers:**

1. **Subsumption (the clean case).** If rule A's condition *logically implies*
   B's — every rune matching A also matches B — then A is unambiguously more
   specific (its matched set is a subset). "dog AND small AND european" implies
   "dog", done. No scoring needed; it's a real logical order.
2. **Logical-depth score (when neither implies the other).** "Small dogs" vs
   "european dogs" overlap but neither is a subset — so we need a number. Your
   "length of logic": count the **condition literals** *and* add weight for
   **how deeply the conditionals nest** (an `if` inside an `if` is deeper
   reasoning than two flat `and`s). Formally it's the **size/depth of the rule's
   proof tree**, which we just read off the block AST. More premises + deeper
   nesting = wins.

**The recency/ID worry — you're safe.** You asked: does recency mean "secretly
every line gets a unique ID?" **No.** Recency is decided **per rule (per
script)**, and a script is a **stored object** — a rune in the Allomone mantle —
which *already* has an id and a place in the command history. So "which rule is
newer" is answered by data we already keep, at the whole-rule level. The
individual *lines/blocks* inside a rule need no IDs, and nothing about this leaks
into the Allomone Script text — the id lives on the saved rule, not in the
syntax. (Recency only matters as a last resort anyway: only when two rules are
*equally* specific and still both apply.)

---

## Deltas stored on the graph (q21c)

**Your idea:** instead of deltas being fleeting events on some queue, have the
**runes and mantles store their own deltas**, keeping it all in the graph. I took
that — it's the better design.

A **delta** is "what changed" — e.g. "`vip` tag added to *maria* at time T". The
naive way is a temporary event that fires once and is forgotten. Your way: each
rune keeps a little **append-only list of its own changes** (and each mantle a
roll-up). Why it's nicer:

- **The environment remembers.** That fits the whole ant/stigmergy model — agents
  read the shared environment, and now the environment carries its own history.
  No separate event bus to drift out of sync with the graph.
- **"Compare previous vs new tags" becomes a local lookup** — just read the
  rune's last delta, rather than reconstructing it from a global log.
- **It's cheap and recoverable.** Every change is already a logged command, so a
  rune's delta list is just *that log, filtered to this rune* — inexpensive to
  keep, and rebuildable if ever lost.

This is separate from the **clock**: deltas are written *when you change
something* (free when nothing changes); the clock ticks *continuously* whether or
not anything changed (which is why it's the part we make optional). Two different
mechanisms, which is exactly why one can be on while the other is off.

---

## Derive vs materialize (q21b)

**The question:** what does "derive vs materialize" mean for Allomone rules?

**The short version:** it is *where a rule's result lives* — computed fresh each
time you look (**derive**), or written into the database as real data
(**materialize**).

**An analogy.** Think of a spreadsheet.

- A **formula cell** (`=A1+B1`) is **derived**. The value shows on screen, but the
  cell doesn't *store* a number — it recomputes from A1 and B1 every time. Change
  the inputs, it updates. Delete the formula, the value just disappears; you
  haven't lost any "real" data.
- **Paste-as-values** is **materialize**. You take that computed number and write
  it into the cell as an actual, stored `7`. Now it persists, other formulas can
  point at it, and it survives even if A1/B1 change — but it is now *real data you
  have to maintain*, and undo is how you take it back.

**In Allomone:**

- **Derive (the default).** "Contacts tagged `democrat` are blue." The blue is
  **not stored** on the contact. Each time a card is drawn, the engine checks the
  tag and colors it. Turn the rule off → the blue vanishes, and the contact rune
  is exactly as it was. Nothing was ever written; nothing to undo. This keeps
  *styling* from being confused with *data* — the database stays clean, and a
  visitor's view can differ from yours without editing the shared truth.
- **Materialize (on request).** "When `dog` is added and the logo is empty, **set**
  the logo to the dog image." Here you *want* a real change — the logo field
  should actually contain that value from now on, be exported, be seen by
  everyone, be queryable by other rules. So the rule **writes** it: it emits the
  same `set logo …` command you'd type by hand. It persists, it syncs, and undo
  can reverse it.

**Why default to derive?** Because most rules are about *appearance*, and
appearance shouldn't silently rewrite your data. You only "paste as values"
(materialize) when the derived thing should *become* a fact. This is the same
instinct behind the map's existing behavior: coloring a marker by a tag doesn't
write a color onto the rune; it derives it at draw time. (The academic names, if
you ever want them: derived facts are Datalog's **IDB**, stored facts are the
**EDB**.)

**Your decision (2026-08-03): derive-ONLY for now — no materialize at all yet.**
You're right to be cautious. While Allomone is young, letting a rule *write* data
is dangerous, especially combined with turning scripts on/off and the optional
clock: disable a script — does its created data vanish or linger? Re-run it in a
loop — does it duplicate? Those are real soundness traps. So for now Allomone can
only ever change how things **look**, never the underlying truth — the worst a
bad rule can do is pick the wrong color, which is completely safe and instantly
reversible (just turn the rule off). Materialization comes back only once the
engine is mature: sandboxed, with dry-run/preview and error prevention. Until
then, the database is untouchable by rules.

---

## Conflict resolution (q21d)

**The question:** how do we decide which rule wins when several apply?

**The setup.** Once you have lots of rules, two of them will eventually try to set
the *same thing* on the *same* rune to *different* values. "Democrats are blue"
and "Donors are gold" both fire on someone who is a blue **and** a donor. The card
can only be one color. Who wins? That decision procedure is **conflict
resolution**, and it's a well-worn part of rule engines, so we don't have to
invent it.

**First, most "conflicts" aren't.** If rule A sets a *color* and rule B sets an
*icon*, there's no conflict — both just apply. A real conflict is narrow: **two
rules writing the same property of the same rune to different values.** Only those
need a referee.

**The referee, in order:**

1. **Priority (you set a number).** Every rule has a priority; higher wins. This
   is your steering wheel — "the VIP rule beats the default rule because I said
   so." (Today's map rules do a rough version of this: whichever rule is *first
   in the list* wins.)
2. **Specificity (the more specific rule wins).** If two rules tie on priority,
   the one with the **narrower** condition wins — a rule for "contact AND vip"
   beats a rule for just "contact", because it's clearly the more special case.
   This is the "exceptions override defaults" instinct, done automatically.
3. **Recency (newest wins).** If they *still* tie, the rule you edited most
   recently wins — your latest intent tends to take effect.

If everything ties, we break it **deterministically** (a stable order by id), so
the screen never flickers between two colors randomly.

**Two more pieces that make it usable:**

- **Provenance ("who did this?").** Each derived value remembers *which rule*
  produced it, so you can hover a blue card and see "colored by rule *Democrats*
  (priority 10)." Without this, a big rule set is a black box.
- **Shadow warnings.** We tell you when a rule can *never* win because another
  always beats it ("this rule is fully shadowed"). The map rules already do a
  simple version of this; Allomone generalizes it.

**One subtlety beyond taste: order can be *required*, not just preferred.** If
rule B reads a tag that rule A *creates*, then A must run before B or B sees
nothing. The engine works this out by looking at which rules depend on which
(this is called **stratification** — "run things in layers, derived-first"), and
it complains if the rules form an impossible loop (A needs B, B needs A).

**What we build first:** priority + shadow warnings + a stable tiebreak. The
fancier stuff (full specificity/recency/provenance UI, stratification with
cycle-detection) is real work and gets its own pass later.

---

## Reactivity: the clock, and why loops are optional (q21c)

**The question:** how do loops/animation work, and how do we keep the "react to
tag changes" part without paying for a clock?

**Two different meanings of "reactive," kept separate:**

1. **React to *changes*** — "when a tag is added, do X." This is **cheap and
   always on.** Every edit in Hormiga is a command going through one pipe; Allomone
   just watches that pipe and re-checks the few rules a given change could affect.
   No timer needed — it only does work when *you* change something. Your "on tag
   change, compare old vs new tags" lives here, and it stays on always.

2. **React to *time*** — "every second, cycle the card to the next color." This is
   the only part that needs a **clock**: a steady tick, whether or not anything
   changed, so a rule can animate. That's a standing cost across the whole app.

**So we make the clock a switch.** There is **one** application clock (not one per
rule). It's **off by default**. Turn it on (`ui.allomone.clock` in Settings) and
loop/`wait` blocks come alive; leave it off and those blocks simply do nothing
(the editor tells you so), while everything else — including "react to changes" —
works normally. Under the hood a `forever` isn't a real spinning loop; it's "on
each tick, take one step," so it can't hang the app, and switching the clock off
stops all of it instantly. Your lean was right; this is just the *how*.

---

## Round-tripping blocks ⇄ text, and do we need Void Core? (q21f)

**The question:** you want blocks that convert to a text script and back — do we
have to ask Void Core for anything, or can Hormiga do it alone?

**Hormiga can do it alone — for Allomone's own scripts.** The trick is that
**neither the blocks nor the text is the "real" thing** — both are just two ways of
*drawing* the same underlying structure (an AST — a tree that says "rule: when
[condition] do [action]"). Blocks draw that tree as puzzle pieces; **Allomone
Script** draws the same tree as text. Converting one to the other is just
"re-draw the tree the other way" — no risky parsing of someone else's language,
because Hormiga owns the tree, owns the block editor, and owns the Allomone Script
grammar and its parser. So: **no message to Void Core needed.** You were right.

**The one place Void Core could ever help** is narrow and optional: if you wanted
to paste in a **raw, hand-written Void Script line that did *not* come from
Allomone** and have it turn into blocks, *that* would mean parsing **Void Core's**
grammar — which Core owns, not us. We don't need that to ship the feature (Allomone
round-trips its own scripts perfectly). If we ever want that import nicety, the ask
to Void Core is tiny ("give us a way to read your grammar as a tree"), and that's
the moment to write the message — not now.

**Why bother with two surfaces at all?** Because they're good at different things:
blocks are discoverable and hard to get wrong (you can't build nonsense); text is
fast to edit, easy to diff, and easy to share. Same script, pick your tool.

---

## Naming note

- **Allomone** = the whole rules engine + language (runtime + blocks + text).
- **Allomone Script** = the textual surface (working file extension `.allo`).
- This retires the earlier working name "**Hormiga Script**." Say the word if you
  prefer keeping "Hormiga Script" for the text surface — trivial to switch.
- **Void Script** is unchanged: the CLI/dispatcher command language Allomone
  *lowers to*. Allomone Script is the high language; Void Script is the assembly.
