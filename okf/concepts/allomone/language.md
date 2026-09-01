---
type: Concept
title: Allomone Script — Hormiga's scripting language
description: "Allomone is not just a rules engine — it is Hormiga's scripting language (Lua-flavored), which today is USED for derive-only styling rules. A real language: a type system (contact/organization/event/resource… are TYPES, alongside tag/list/color/number/bool), variables, lists, conditionals, iteration over the dataset, set-operators. Derive-only is enforced by the grammar. Edited in a syntax-highlighting IDE."
tags: [status:direction, audience:dev, confidence:asserted]
timestamp: 2026-08-03T00:00:00Z
---

Part of [Allomone](/concepts/allomone/index.md).

# Reframe (author, 2026-08-04): a scripting language, not a rules engine

**Stop thinking of Allomone as "a rules engine."** Functionally it *is* one today
(derive-only styling), but conceptually it is **Hormiga's scripting language** —
a general language over the whole database, of which styling rules are the first
use. This is a bigger scope, and it makes the design honest: the author's own
example (below) already needs **variables, lists, conditionals, counting, and a
type system**, none of which a "rules engine" framing accounts for.

## The worked example (drives the whole design)

> 1. a variable that is a **list** of *time tags* (tags about time)
> 2. a **list** of *animal tags* (tags about animals)
> 3. if a **contact, event, or organization** has any time tag → blue; if it
>    also has an animal tag → dark blue; if it has only animal tags → yellow
> 4. if the **count** of animal tags equals the count of time tags → yellow

Everything a real language needs is visible here: **named variables** holding
**lists**; **membership/any** tests over lists; **`if/else` branching**;
**counting** and **numeric equality**; and — crucially — **types**: "a contact,
event, or organization" ranges over the **datatypes** of the model. In Allomone
Script:

```lua
-- semiochemicals: this script styles by tag families
local time_tags   = { "month:", "season:", "date", "deadline" }
local animal_tags = { "dog", "cat", "bird", "livestock" }

for each thing where thing is (contact or organization or event) do
  local t = count(thing.tags matching time_tags)
  local a = count(thing.tags matching animal_tags)

  if t > 0 and a > 0 then thing:color("#123a8a")        -- dark blue
  elseif t > 0        then thing:color("#3f6fae")        -- blue
  elseif a > 0        then thing:color("#c0a020")        -- yellow
  end
  if a == t and a > 0 then thing:color("#c0a020") end    -- equal → yellow
end
```

(Syntax provisional; the point is the *shape*.) `thing:color(...)` sets a
*derived* annotation. **Important correction (author, 2026-08-04): derive-only is
a TEMPORARY DEVELOPMENT GATE, not a language invariant.** Allomone Script is
**designed to include mutation** — statements that add/remove tags and set fields
(`thing:add_tag(...)`, `thing:set(...)`). Those are **turned off right now** only
because writing data from a young engine is hard to test safely
([domains](/concepts/allomone/domains.md), q21b). Do **not** design the language
as if mutation is impossible — design it *with* mutation, **gated**. When the
engine matures (sandboxing, dry-run, error prevention), the gate lifts and no
grammar changes. So "the only statement that affects the world is `color`" is
true of *today's enabled subset*, not of the language.

## The type system (the datatypes "emerging")

Allomone is **typed**, and the types come straight from the model
([data model](/concepts/foundation/data-model.md)):

- **Entity types** — `contact`, `organization`, `event`, `incident`, `job`,
  `image`, `resource`, `note` (the glyphs). A `thing` has one. `thing is
  (contact or organization)` is a type test; the engine also exposes them as the
  same tag grammar (`type:contact`) so conditions can use either.
- **Value types** — `tag` (a string on an axis), `list` (an ordered collection,
  usually of tags), `color` (`#rrggbb`), `number`, `bool`, `string`.
- **Later** — `link`/`relation` (edges), and graph-analytic values
  (`centrality`, a `number`).

Types make the IDE's help real: completion knows `thing.` offers `tags`,
`field(...)`, `is`, `color(...)`; a `list` offers `count`, `matching`,
`contains`; a `color` slot wants `#…`. (This is where **IntelliSense** comes
from — the language service answers completion from the types,
[the shared editor engine](#the-shared-text-editor-engine-with-notes--author-2026-08-03).)

## Language elements (v0 → the fuller language)

| element | shipped (2026-08-05) | the fuller language |
|---|---|---|
| **comments** | `-- …` | same |
| **variables** | `local x = …`; reassign `x = …` | typed locals; block scope |
| **lists** | `{ "a", "b" }`; `count`/`len`, `matching`, `head`/`tail`/`push`/`contains` | `select`/`filter`/`map` verbs (R layer) |
| **conditionals** | `if / elseif / else / end` | same + `match`/`case` |
| **iteration** | `for each thing do … end`; `for x in <list> do … end` | `for each thing where …`; piped queries |
| **types** | `thing is (contact or …)`, `thing.type`, `thing.tags`, `thing has "…"` | archetypes + generic dispatch (Q23); `thing.field(...)` |
| **operators** | `+ - * / %`, unary `-`, `== != < > <= >=`, `and/or/not`, `matching`; `+` concats str/list | full numeric + set algebra |
| **functions** | `function name(args) … end`, `return`, calls, **recursion + mutual recursion** (hoisted), first-class `Func` values | closures, named args, the R data-verbs as functions |
| **safety** | **fuel budget + call-depth cap** — runaway loop/recursion errors, never hangs | configurable budget; static termination hints |
| **effects** | `color` (derive-only) | `highlight`, `icon`, `emphasis`, `size`; later, **gated**: `add_tag`, `set` (mutation — off during dev) |
| **rules (legacy)** | `rule "n" / when <tags> / color #…` desugars | — |

v0 is a **strict subset**: today's `rule`/`when`/`color` scripts remain valid as
the language grows, because they desugar into the general form. The interpreter
is now a real **tokenizer + recursive-descent parser + tree-walking evaluator**
(`src/domain/allomone_legacy.hpp`). It lowers to **derived annotations today**; the mutation
statements are **written into the design now but gated off** at the evaluator
until the engine is mature (above).

## Functions, recursion & the termination budget (built 2026-08-05)

Allomone has **first-class functions**: `function name(a, b) … return … end`,
called as `name(args)` in any expression (or as a bare call statement for a
helper that colors). They are **values** (a `Func` variant), and top-level
declarations are **hoisted** before the run, so **forward references and mutual
recursion** resolve (`isEven`/`isOdd` calling each other both work). Scoping is
lexical-ish and simple: a call runs in a fresh scope that sees its parameters and
the **globals** (where all functions and top-level `local`s live); loop bodies
**share** their enclosing scope, so accumulators (`local n = 0; for x in xs do n
= n + 1 end`) work as written. Built-in verbs (`count`/`len`, `head`, `tail`,
`push`, `contains`, `abs`, `floor`, `min`, `max`, `lower`, `upper`) are the seed
of the R data-verb layer below; `+` also **concatenates** strings and lists.

**Why a language this small needs a budget.** Recursion + a *live* editor that
re-runs on every keystroke means a bad script could hang Hormiga. So the
evaluator runs under a **fuel budget** (a shared step counter) and a **call-depth
cap** (native-stack guard): a runaway loop or non-terminating recursion halts and
surfaces a red **runtime error** ("step budget exceeded", "recursion too deep")
instead of freezing the app. This is the concrete mechanism behind the
[reactivity](/concepts/allomone/reactivity.md) doc's **termination guarantee** —
and it is the **prerequisite for the opt-in clock** (Mode 2): bounded per-tick
work is only safe once unbounded work is already caught.

**A rune is a first-class VALUE (built 2026-08-05).** The old `thing` keyword is
gone; the language now speaks **Void Core's vocabulary** — a **rune** is an
instance, typed by its **glyph**. `for each rune do … end` binds the variable
`rune`; a rune can be **passed to a function**, filtered (`runes where …`), and
read with `rune.tags` / `rune.glyph` / `rune.name` / `rune has "tag"` /
`rune is contact`. This is the keystone that unblocks **generic dispatch** (Q23)
and **link traversal** ([inputs](/concepts/allomone/inputs.md) §3) — a verb takes
a rune; a traversal returns runes.

## Evaluation model (unchanged: derive-only, set-at-a-time)

A script **runs over the whole dataset** and produces **derived annotations**
([domains](/concepts/allomone/domains.md)); it is re-run when the data changes
(the delta reactivity of [reactivity](/concepts/allomone/reactivity.md)); it
**never mutates the model**. `for each` is Lua *surface syntax* over what is
still a **set-at-a-time, declarative** evaluation ([paradigm](/concepts/allomone/paradigm.md))
— the imperative feel is a convenience, not a change of semantics. Conflicts
between scripts resolve by specificity/order ([conflicts](/concepts/allomone/conflicts.md)).

## The data-verb layer — R/tidyverse as inspiration (author, 2026-08-05)

Allomone keeps its **Lua-flavored surface**, but the author wants its *data*
half to borrow from **R (and the tidyverse)** — the language built for "messing
with data." Four ideas to fold in, none of which disturb the Lua feel:

- **Pipes.** R's `|>` / magrittr's `%>%` thread a value through a chain of
  verbs left-to-right (`data |> filter(...) |> summarise(...)`). This is the
  natural way to write a query over the rune set: `things |> where(has
  "type:contact") |> related("member-of") |> count`. A pipe is just left-to-
  right function application — it composes cleanly with the existing `for each`
  (the pipe *builds the set*, `for each` *acts on it*), and it reads far better
  than nested calls. Surface token still open (`|>` reads R-native; `|` is
  cleaner but conflicts with `or` in some grammars).
- **Dataframes / the rune set as a table.** R's core object is the dataframe —
  rows × typed columns. Allomone's analog already exists: **a set of runes is a
  table** (rows = runes, columns = fields + derived measures). The data verbs
  operate on that table, and the [Analysis](/concepts/sections/workspace-and-sections.md)
  tab is where a materialized frame would surface. Tags are a **list-column**
  (each rune has many); links are a **join** to another frame (§3 of
  [inputs](/concepts/allomone/inputs.md)).
- **The extract/reshape verbs.** dplyr's small vocabulary is the target shape:
  **`filter`/`where`** (rows), **`select`** (columns), **`mutate`** (derive a
  column — *gated*, like all mutation, during dev), **`summarise`** + **`group
  by`** (aggregate — the counting example is a `group … |> count` in disguise),
  **`arrange`** (sort). These are the "ways of extracting data" the author
  named; they lower to the same set-at-a-time evaluation.
- **Libraries / namespaces.** R ships behavior in packages. Allomone's analog is
  **namespaced verb bundles** (a `geo:` bundle for distance/region, a `text:`
  bundle for name-matching), each a small, documented surface — the same
  boundary discipline the Antfarm uses for I/O, applied to *pure* helper verbs.

This is a **direction, not v1**: today's interpreter has `for each` / `if` /
`count` / `matching`. The data-verb layer is the planned second half — a
**piped query sublanguage over the rune set** that produces the set the
imperative half then styles, and eventually the frames the Analysis tab reads.
Link traversal (§3 of [inputs](/concepts/allomone/inputs.md)) is one of its
verbs.

## Set theory & quantifiers — Allomone as the LOGICAL side (BUILT 2026-08-05)

The author's framing: Allomone is "the logical side of things," and the natural
shape for that is **set theory + first-order logic** over the rune graph. **Built
and running** on the [cat dataset](/concepts/projects/cat-dataset.md) — the motivating
**two-hop set query**, in the real syntax:

```
-- 1. the SEED SET: every rune tagged {orange, cat, male}
local gs = runes where (rune has "orange" and rune has "cat" and rune has "male")
-- 2. collect the OTHER tags those runes carry — a set, stored in a variable;
--    the script never names "lasagna"/"mondays", they are DISCOVERED
local spread = minus(tags_of(gs), { "orange", "cat", "male", "type:contact" })
-- 3. now color everything carrying ANY of those discovered tags
for each rune do
  if overlaps(rune.tags, spread) then rune:color("#ff8800") end
end
```

Verified: this colors garfield/milo (the orange males) **and** every other cat
sharing `lasagna` or `mondays` — a real `runes → their tags → runes` traversal.
The three capabilities it rests on, all shipped:

1. **A rune is a value** — `runes` is the whole mantle (a list of rune values);
   `runes where <pred>` filters it (binding `rune`); `rune.tags`/`.glyph`/`.name`
   read it. (The pending keystone, now built.)
2. **Set algebra** — `tags_of(runes)` (union of their tags, deduped),
   `union` / `intersect` / `minus` / `overlaps` over lists of tags **or** runes.
3. **Quantifiers** — `any(list)` / `all(list)`, and with a **function
   predicate** `any(list, fn)` / `all(list, fn)` (real ∃ / ∀, using first-class
   functions). `overlaps(a, b)` is the common "∃ a shared element."

More logical tests this unlocks (good coverage for the cat dataset):

- **∀ over a group**: color a `state:` gold if **every** cat in it is `lazy`.
- **∃ near-duplicate**: color a cat if **there exists** another sharing ≥3 tags.
- **Subset (⊆)**: color cats whose tags are a subset of garfield's.
- **Union ∩ intersect**: color cats in `(orange ∪ tabby) ∩ male`.
- **Aggregate/mode**: find the most common coat, color those cats.
- **Transitive closure** (with links): color everything within 2 tag-hops of
  garfield — the connectivity read that also serves the recommender's
  "comprehensive" mode ([tag-recommender](/concepts/allomone/tag-recommender.md)).

Semantically this stays **set-at-a-time and declarative** — `things where` is a
comprehension, the quantifiers are folds, and it all remains **derive-only** (the
example *colors*; it never writes the discovered tags). It composes with the
R data-verb layer above: `things where` **is** `filter`, `tags_of` is a
`select`+`flatten`, quantifiers are `summarise` predicates.

---

The author wants to "convert these blocks into a textual scripting language, and
back." (Blocks are now retired — [block-editor](/concepts/allomone/block-editor.md)
— but the round-trip *reasoning* still governs text ⇄ AST.) That is achievable
**losslessly**, and the reason is the same one that makes it worth doing.

# One AST, two projections (projectional editing)

Neither the blocks nor the text is the source of truth — **the AST is**. The
block canvas and the text file are two **projections** of that one tree, exactly
the **projectional editing** model (JetBrains MPS is the reference). "Blocks →
text → blocks" round-trips perfectly because both are just renderings of the same
structure; nothing is parsed *back* into meaning, it is re-*projected*. This is
also why the block shape grammar ([blocks](/concepts/allomone/blocks.md)) and a
typed textual grammar agree: shapes are the AST's node types drawn as puzzle
pieces; Allomone Script is the same node types written as text.

# Allomone Script — the language

**Allomone Script** is the textual surface (working file extension `.allo`). It
is a small, readable rule DSL — a condition-action language over the tag graph.
Illustrative (syntax to be finalized during the build):

```allomone
rule "central orgs run hot" {
  when   type:organization and centrality > 0.7
  looks  emphasis: high, accent: warning
}

rule "auto dog logo" {
  on add "dog"
  when   field logo is empty
  do     materialize set logo = asset("dog-logo")
}
```

`when` = a boolean condition subtree; `looks`/`do` = actions (derive vs
materialize, [domains](/concepts/allomone/domains.md)); `on` = a delta trigger
([reactivity](/concepts/allomone/reactivity.md)).

> **Naming.** Allomone Script supersedes the earlier working name "Hormiga
> Script" — one name for the whole system reads cleaner. (Flagged for the
> author's confirmation; trivial to rename either way.)

# The tower: Allomone Script lowers to Void Script

We are, precisely, **building a language on top of Void Script**:

```
Allomone (blocks ⇆ Allomone Script)   ← the source DSL (the author writes this)
        │  lower
        ▼
Void Script (dispatcher commands)      ← the IR / "assembly"
        │  dispatch
        ▼
Void Core (the model)                  ← the machine
```

- A **materializing** rule **macro-expands into Void Script commands**
  (`tag …`, `set …`, `link …`) — so its run is a normal, logged, replayable,
  undoable transcript. The founding pillar holds unchanged.
- An **observational** (derive) rule lowers to a **standing query** re-evaluated
  at projection — it emits no commands, changes no state, and vanishes when
  disabled.

Void Script stays the CLI-native command language; Allomone Script is the *higher*
language a person authors. One could hand-write Allomone Script, or hand-write
Void Script — both reach the same model.

# Storage & lifecycle

Scripts are **first-class stored objects** — a `script` glyph in the **Allomone
mantle** — so they **ride the `.miga`**, replay, and (for shared scope) sync
E2EE. **Scripts are DATA (author, 2026-08-04): they carry tags too.** A `script`
rune is an ordinary rune, so it can be **tagged** (`status:draft`, `owner:…`,
`topic:animals`), filtered, and organized like everything else — the IDE's script
list is just a view over tagged runes, and the tag-filter builder can narrow it.
This also means Allomone is, in principle, **reflective**: because scripts are
part of the graph, a script *could* read or (later, when mutation is enabled)
tag other scripts — the same input surface, turned on itself. We don't build that
now, but the model already permits it (scripts are not a privileged outside
thing; they're runes). Each script has an **enabled/disabled** flag; a database holds
**many** scripts, individually toggled. Save/load is therefore just the normal
model lifecycle (no bespoke file format needed for storage; `.allo` is an
optional import/export convenience for sharing a script outside a database).
Ordering and clashes between enabled scripts are the concern of
[conflicts](/concepts/allomone/conflicts.md).

# The reconstruction engines — blocks ⇄ text (author 2026-08-04)

For "freely go back and forth between the blocks version and a text version," the
non-negotiable is that **one thing is the source of truth and everything else is
a projection of it.** That one thing is the **AST**, and — crucially — the AST is
**not a separate data structure**: it *is* the `parent`/`order`/`slot` **rune
tree** in the allomone mantle ([block-editor](/concepts/allomone/block-editor.md)).
So round-tripping is a small, well-defined set of engines, not a fragile
sync-two-copies problem.

**The identity/content split (the abstraction that makes it work).** Every block
has an **identity** (its rune name — internal, meaningless to the user) and
**content** (its glyph = block type, its argument fields, and its place in the
tree). **Text encodes content only; it never mentions rune names.** This single
rule is what keeps the projections clean: the blocks carry identity, the text
carries meaning, and the AST (the rune tree) holds both.

The engines, and which exist:

| engine | direction | status | what it is |
|---|---|---|---|
| **renderer** | AST → blocks | **built** (`draw_allomone_stack`) | draws the rune tree as the stack; a pure projection |
| **evaluator** | AST → effect | **built** (`refresh_allo_rules`) | reads the tree, derives the styling |
| **serializer** | AST → text | to build | a deterministic pretty-printer: walk the tree, emit Allomone Script (no rune names) |
| **parser** | text → AST | to build | tokenize + grammar → a **batch of dispatcher commands** (`rune new`/`set`) that reconstructs the tree; the core reconstruction engine |
| **reconciler** | text-edit → minimal AST mutation | later | on a live text edit, **diff** the new parse against the current tree and emit only the changed commands — so block **identity/selection/undo-granularity survive** editing the text |
| **validator** | AST → ok/errors | later | shared well-formedness check (a `when` has an action, tags exist, no dangling `parent`) — feeds both surfaces' diagnostics |

**Round-trip guarantees we design for:**

- **blocks ⇄ AST is direct** — block gestures *are* commands that mutate the rune
  tree; the renderer projects it back. No reconstruction, no drift (this is why
  the overhaul kept the rune model).
- **text → AST → text is stable** — parse then re-serialize yields the same text
  up to canonical formatting (the serializer defines the canonical form).
- **AST → text → AST is structurally identical** — serialize then re-parse yields
  the same *structure*; only rune **identities** are freshly minted (content is
  preserved). That is exactly right: identity is internal, and the user round-trips
  *meaning*, not names.

**Why the parser is the only hard one, and why it's tractable:** it parses **our**
grammar (Allomone Script), which we define, over a **small fixed vocabulary** — so
it's a tiny recursive-descent parser emitting commands, not a general language
front-end. It lowers to Void Script exactly as everything else does. Until the
reconciler exists, editing the text and re-applying is a **full rebuild of that
rule** (remove its blocks, recreate from the parse) — correct, but it drops
selection/identity; the reconciler is the optimization, not a correctness
requirement.

**What this asks of the block model right now (and it already satisfies):** the
tree must be **fully serializable** (every block's meaning lives in its glyph +
fields + parent/order/slot — nothing hidden in view state) and **fully
reconstructable** (the grammar has a production per block type). The current model
meets both, so the text projection is an additive build, never a refactor.

# The shared text-editor engine (with Notes) — author 2026-08-03

Allomone Script and the **Notes** tab both need a real text editor — one with a
buffer, selection, and (for script) **IntelliSense**: completion, syntax
highlighting, error squiggles. The author's call: **do not build two.** Build a
single **document-writer engine** that both surfaces configure differently — the
same core, two dialects.

The clean architecture is the **editor-core + language-services** split (the
shape of a code editor + a mini language server):

- **The editor core** (shared): the text buffer, cursor/selection, undo within
  the field, a completion **popup**, and hooks for **highlighting** spans and
  **inline diagnostics**. It knows nothing about *what* the text means.
- **A language service** (per surface), pluggable:
  - **Notes** → a *markdown* service: light markdown highlighting, and later the
    Obsidian-flavored niceties (link completion `[[…]]`, tag completion `#…`).
  - **Allomone Script** → an *Allomone* service: keyword/block-name completion,
    **tag-vocabulary completion** (reusing the tag-picker's data), field/relation
    completion, and syntax/type diagnostics from the same AST the blocks use
    ([blocks](/concepts/allomone/block-catalog.md)).

So IntelliSense is just "the Allomone service answered a completion request from
the shared core." One engine, two services; the Notes tab gets a better editor
for free as the script side matures, and vice-versa. (Bare-bones today the Notes
tab uses a plain multiline box; it is the **first client** of this engine when we
build it — the script tab is the second.) This engine is **host-side** — a text
editor is application chrome, like the block canvas's chrome — and it composes
the tag-picker and completion widgets we already own.

# On round-tripping and Void Core (q21f — the honest answer)

Can Hormiga do blocks ⇆ text **by itself**, with no message to Void Core?
**Yes — for Allomone's own AST.** Because Hormiga *owns* the Allomone AST, it
authors both projections and never needs an external parser: block edits mutate
the AST; the text view re-projects it; typing in the text view parses **Allomone
Script** (our grammar, our parser) back to the AST. No Void Core dependency.

The **only** place an upstream parser would help is a *narrow* one: turning an
**arbitrary hand-written Void Script / raw tag expression** (that did **not**
originate from Allomone) back into blocks — because that requires parsing *Void
Core's* grammar, which Core owns. We do **not** need that for the core feature;
Allomone round-trips its own scripts fine. So: **no `MESSAGE_FOR_VOIDCORE` now.**
If we later want the raw-Void-Script-import nicety, the ask would be small
("expose the tag-grammar AST / a parse entry point"), and that is the moment to
draft it — not before. (Explained at more length in
[developer_explanations](/developer_explanations.md).)
