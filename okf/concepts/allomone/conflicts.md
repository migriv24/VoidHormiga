---
type: Concept
title: Allomone — layering, specificity & interactions
description: "Overlapping rules are a FEATURE, not an error (no warnings): general defaults layered with specific exceptions, resolved by specificity — measured as the 'logical depth' of a rule's derivation (condition literals + conditional nesting), with subsumption as the principled order and per-script recency as the final tiebreak."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md).

> **⚠ ANSWERED UPSTREAM, 2026-08-10 — and two of the answers correct this page.**
> See [adoption](/concepts/allomone/adoption.md); the normative account is
> `../VoidAllomone/okf/concepts/composition.md`.
>
> This page and a later message of ours **contradicted each other**: here
> conflicts resolve silently, there they must be surfaced. Void Maiz found that
> **both are right at different scopes**, and the CSS cascade gave the split:
> *silent within one source, surfaced across sources.* Defaults-and-exceptions
> inside one script is the idiom this page correctly describes; two authors
> disagreeing is a question no algorithm should answer for them, and it becomes
> **⊤ — a first-class conflict** whose value is cleared so nothing can render a
> guess.
>
> The second correction: this page proposes **subsumption first, logical depth
> second**. For a purely conjunctive grammar those are **the same number** — if
> A's terms strictly contain B's then `|A| > |B|` — so the two-level scheme
> collapses to plain term count. It stops being true the moment `or` exists,
> which is one more reason `or` is refused.
>
> **Recency shipped** as `ConflictPolicy::Recency`, opt-in per property, on the
> strength of this page's argument. Hormiga has it switched **off**: it stays
> off until a person has seen a conflict and asked for the newer one.
>
> **Superseded for the imperative core (2026-08-04).** Now that Allomone is an
> **imperative** scripting language (`for each` / `if` / `elseif`), conflicts
> resolve by the natural rule: **last-write-wins** — within a script, the last
> `thing:color(...)` executed; across scripts, mantle order. The declarative
> **specificity/layering** model below applies only if a *declarative* sub-layer
> is ever reintroduced as sugar; it is kept for that reasoning and for the
> map/calendar rules it still describes.

**Reframed by the author
(2026-08-03): overlapping rules are a *feature*, not a problem.** They are how
you express rich interactions — a broad default with sharper exceptions layered
over it. So this is **not** "conflict resolution as error-avoidance" (and there
are **no warnings**); it is **layered defaults with specificity-based override**.
Plain-language version in [developer_explanations](/developer_explanations.md).

# The pattern: defaults and exceptions

The author's example is the textbook case:

> "**all `dog`s are blue**" — a broad default.
> "**if `dog` and `small` and `european` → light blue**" — a sharper exception
> for a subset.

Both rules fire on a small European dog; the **more specific** one should win
*for that subset*, while the default still governs every other dog. This is
exactly **defeasible reasoning with specificity** (Poole/Nute; the "birds fly,
but penguins don't" pattern of default logic). Overlap is **desirable** — it is
where the interesting behavior lives — so Allomone **embraces** it and resolves
by specificity, silently.

# Specificity wins — measured as "logical depth"

The author's instinct: the winner is the rule with the **longer path of logic**
to reach its conclusion — not merely "more tags", but **how much reasoning had
to hold**, including nested conditionals. The technical framing has two layers:

1. **Subsumption (the principled partial order).** Rule A is **strictly more
   specific** than rule B iff A's condition **logically entails** B's — every
   rune that satisfies A also satisfies B (A's matched set ⊆ B's). "small AND
   european AND dog" entails "dog", so it wins wherever both apply. When two
   rules stand in this **entailment order**, specificity is unambiguous and
   correct by construction — no scoring needed.
2. **Logical-depth score (the tiebreak for *incomparable* rules).** When neither
   condition entails the other (they overlap but neither is a subset — e.g.
   "small dogs" vs "european dogs"), we need a scalar. It is the **structural
   weight of the derivation**: the count of **condition literals** **plus** the
   **nesting depth** of the conditionals traversed to reach the action
   (an `if` inside an `if` counts more than two flat `and`s). This is the
   author's "length of logic": both *how many* premises and *how deep* the
   reasoning nests. (Formally this is the **size/depth of the condition's proof
   tree**; we compute it from the rule's AST — cheap, since we own the AST.)

So: **subsumption first** (a real logical order), **logical-depth score** to
break incomparable ties, then recency.

# Recency — and why it needs NO per-line IDs

If two rules are *equally* specific and still both apply, the **more recently
added/edited** one wins — the author's latest intent. The author worried this
means "secretly every line gets a unique ID." **It does not.** Recency is
**per-script**, and a script is a **stored rune** in the Allomone mantle — it
**already has** an identity (`spirit.id`) and a place in the command log's
order. So "which is newer" is answered by data we already keep, at the **rule**
level. The AST's *lines* need no IDs; the **rule** carries the only identity
recency requires. (Nothing leaks into Allomone Script's surface syntax — the id
lives on the stored rule object, not in the text.)

# Determinism

If subsumption, logical depth, and recency **all** tie (genuinely identical
rules), the result is still **deterministic** — a stable order by rule id —
never a flicker. But this case means duplicate rules, which the editor can
gently dedupe rather than warn about.

# Provenance (info, not a warning)

Because a derived annotation can come from several layered rules, each derived
value remembers **which rule produced the winning value** — so hovering a blue
card can show "set by *small european dogs* (more specific than *all dogs*)".
This is **explanatory info**, not a warning: it helps you *read* your layered
rules, which is the whole point of allowing overlap.

# Stratification (still required for correctness)

One ordering constraint survives regardless of taste: a rule that **reads** a
value another rule **derives** must run **after** it (**stratified evaluation** —
[paradigm](/concepts/allomone/paradigm.md) §2). The engine computes strata from
rule dependencies so "derive, then read the derived" is well-defined. A **cyclic**
dependency (A needs B, B needs A) has no stratification; that is the one thing we
must reject — not a style warning but a genuine "this can't be evaluated." (With
Allomone **derive-only** and **materialization deferred**
[domains](/concepts/allomone/domains.md), cross-rule dependencies are limited
early on, so stratification stays simple until the language grows.)

# Lifecycle

Scripts are stored objects in the Allomone mantle, each **enable/disable**-able,
riding the `.miga`; disabling a script cleanly removes its (derived) effects with
nothing to undo — trivially safe precisely *because* Allomone is derive-only.
Enabling/disabling and re-ordering are ordinary dispatcher commands, so managing
the rules is itself replayable and, for shared scope, synced.
