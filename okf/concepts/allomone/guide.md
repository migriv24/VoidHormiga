---
type: Guide
title: Writing Allomone Scripts — a user's guide
description: "The practical, example-driven guide for a Hormiga USER who wants to write Allomone scripts (not a developer of Hormiga). What a script is, the whole vocabulary — runes, glyphs, tags, loops, conditions, functions, sets, quantifiers — and how to run one. The cat colony ships a matching library of commented examples."
tags: [status:current, audience:user, confidence:asserted]
timestamp: 2026-08-05T00:00:00Z
---

> **⚠ STALE, 2026-08-10 — this guide teaches the LEGACY dialect.** Allomone
> moved into Void Maiz and the language changed shape: a script is now a **set
> of rules** (`when has "orange" then color "#e8890c"`), not a program with
> loops and branches. The dialect this guide teaches still runs — a `script`
> rune is the frozen interpreter — but new rules are `allo-script` runes in the
> new language.
>
> **Until this is rewritten, the teaching path is the eight seeded examples**
> (`a1-rules-are-a-set` … `a8-a-conflict`, shipped disabled in the cat colony's
> `allomone` mantle) plus the Reference tab, which lists every condition and
> every merge law generated from the engine's own tables. The precise language
> is `../VoidAllomone/okf/concepts/start-here.md`; what is Hormiga's is
> [adoption](/concepts/allomone/adoption.md).

> **Who this is for.** You *use* Hormiga and want to write a little logic to
> style your data — highlight the contacts that need follow-up, color events by
> month, spot the busiest people. You do **not** need to know how Hormiga is
> built. (That side — the interpreter, the interaction-net model, projection vs
> dispatch — lives in the *developer* docs: [language](/concepts/allomone/language.md),
> [execution](/concepts/allomone/execution.md), [foundations](/concepts/allomone/foundations.md).
> Concepts repeat between the two on purpose.)

# What is an Allomone script?

An **Allomone script** is a little text program that **styles your database**.
Today it can set a **color** on things. It is **derive-only**: it *reads* your
data and decides how to paint it — it **never changes** your data. Turn a script
off and its coloring simply disappears; nothing was altered.

Scripts live in the **Allomone** tab. Each script has a name and an on/off
checkbox. **Enable** a script and it applies live; **disable** it and the color
goes away. You can have many scripts; when two color the same thing, the last one
to run wins.

Open the cat colony (Hormiga's demo) and you'll find a shelf of examples —
`01-hello-allomone` through `09-color-by-glyph`, all switched off. Enable one and
watch the cards. This guide walks the same path.

# The three words: rune, glyph, tag

Everything in Hormiga is built from three ideas, and Allomone speaks them
directly:

- A **rune** is one thing in your database — a contact, an event, an
  organization. (In the cat colony, every cat is a rune.)
- A **glyph** is a rune's *kind* — `contact`, `event`, `organization`. Every
  rune has exactly one glyph.
- A **tag** is a label you stick on a rune — `orange`, `male`, `state:CA`,
  `type:contact`. A rune can carry many tags. Tags are how you slice your data.

A script is almost always: *look at each rune, check its glyph and tags, and give
it a color.*

# Your first script

```
for each rune do
  if rune is contact then
    rune:color("#2e8b57")
  end
end
```

Read it out loud: **for each rune**, **if** the rune **is** a `contact`, **color**
it green. `for each rune do … end` visits every rune (the variable `rune` is the
current one). `rune is contact` checks the glyph. `rune:color("#hex")` is the
paint. `#2e8b57` is a web color (red-green-blue in hex).

# Conditions: choosing which runes to paint

```
for each rune do
  if rune has "orange" then rune:color("#e8890c")
  elseif rune has "black" then rune:color("#2b2b2b")
  else rune:color("#9aa0a6")
  end
end
```

- `rune has "orange"` — true when the rune carries that tag.
- `if … then … elseif … then … else … end` — the first true branch wins.
- Combine tests with **`and`**, **`or`**, **`not`**:
  `if rune has "cat" and not rune has "kitten" then …`
- A tag ending in a colon is a **prefix**: `rune has "state:"` is true for
  `state:CA`, `state:NY`, anything under `state:`.

# Counting and numbers

`count(x)` counts a list. `rune.tags` is the list of a rune's tags. So:

```
for each rune do
  if count(rune.tags) >= 8 then rune:color("#7a2f8a") end   -- a busy rune
end
```

You have the usual math (`+ - * /`, `%`) and comparisons (`== != < > <= >=`).

# Fields: a rune's own values

Tags are labels; **fields** are a rune's actual data — a contact's `role`, `bio`,
`email`; an event's `date`, `summary`. Read a field as **`rune.field-name`**:

```
for each rune do
  -- events happening in August (ISO dates like "2026-08-15" compare as text)
  if rune is event and rune.date >= "2026-08-01" and rune.date <= "2026-08-31" then
    rune:color("#3f6fae")
  -- contacts who haven't written a bio yet
  elseif rune is contact and rune.bio == "" then
    rune:color("#c0392b")
  end
end
```

An absent field reads as `""`, so "is this empty?" is just `rune.email == ""`.
(`field(rune, "email")` does the same thing if you'd rather pass the name as
text.)

**Fields belong to a glyph.** Different kinds of rune have different fields — a
`contact` has `role`/`email`/`bio`, an `event` has `date`/`summary`/`venue`. An
event has no `role`, so `event.role` is just `""`. That's forgiving, but it means
you should usually **guard field access by glyph** so your intent is clear —
note how the example above pairs `rune.date` with `rune is event`, and `rune.bio`
with `rune is contact`. Reach for a field only after you know the kind.

# Variables and lists

Name a value with `local`. Write a list with braces:

```
local regions = { "state:" }
for each rune do
  local here = rune.tags matching regions
  if count(here) > 0 then rune:color("#3f6fae") end
end
```

`matching` keeps only the tags that match your patterns — a handy way to ask
"does this rune have any `state:` tag, and which?"

# Functions: naming your logic

When a test gets long or you use it twice, give it a name:

```
function heavy(r)
  return count(r.tags) >= 7
end

for each rune do
  if heavy(rune) then rune:color("#b3592e") end
end
```

You can pass a **rune** to a function and read `r.tags`, `r.glyph`, `r has "…"`
inside it. Functions can even call themselves (recursion) — and don't worry about
an accidental infinite loop: the engine has a safety budget and will stop with a
friendly error rather than freeze.

# Working with sets — the powerful part

Allomone is really a little **set-theory** language. A few tools:

- **`runes`** — the whole database, as a set you can work with.
- **`runes where <condition>`** — narrow it to the runes that match.
- **`tags_of(set)`** — all the tags carried by a set of runes (combined, no
  duplicates).
- **`union(a,b)`**, **`intersect(a,b)`**, **`minus(a,b)`** — combine, overlap,
  subtract sets.
- **`overlaps(a,b)`** — do two sets share anything?

Here's the showpiece. Find the orange male cats, discover what *other* tags they
have in common, then color everyone who shares those tags — **without ever
naming those tags yourself**:

```
local gs = runes where (rune has "orange" and rune has "cat" and rune has "male")
local spread = minus(tags_of(gs), { "orange", "cat", "male", "type:contact", "located" })
for each rune do
  if overlaps(rune.tags, spread) then rune:color("#ff8800") end
end
```

In the cat colony the orange males all secretly like `lasagna` and hate
`mondays` — so this lights up every cat who shares those traits, discovered on
the fly.

# "For all" and "there exists" — quantifiers

- **`any(list, test)`** — is the test true for *some* item? (there-exists)
- **`all(list, test)`** — is it true for *every* item? (for-all)

The test is a function:

```
local oranges = runes where rune has "orange"
function isMale(r) return r has "male" end

for each rune do
  if rune has "cat" and all(oranges, isMale) then rune:color("#cc6600") end
end
```

"Color the cats only if *every* orange cat is male."

# Following connections — by structure, not by name

Runes are linked to each other (friendships, memberships, events). Allomone lets
you **follow those connections**, and — importantly — it does so **by shape, not
by name**. You rarely say "garfield"; you say "his neighbours," "his group,"
"the well-connected ones." That way your logic keeps working when the data grows
or the connections change.

- **`neighbours(rune)`** — the runes directly linked to this one.
- **`linked(rune, "friend-of")`** — only the neighbours via a relation.
- **`cluster(rune)`** — the whole connected group this rune belongs to (a set).
- **`degree(rune)`** — how many neighbours it has.
- **`within(rune, 2)`** — every rune within 2 hops (its neighbourhood).
- **`distance(a, b)`** — how many hops between two runes.
- **`centrality(rune)`** — how important it is in the whole network (0 to 1).
- **`community(rune)`** — the sub-group it clusters into (a set of runes).

```
for each rune do
  if degree(rune) >= 6 then rune:color("#7a2f8a")        -- a hub (well-connected)
  elseif count(cluster(rune)) >= 20 then rune:color("#2e8b57")  -- in a big group
  end
end
```

Because these return sets of runes, they combine with everything else. "Color the
runes in the same group as a hub," naming nobody:

```
for each rune do
  local group = cluster(rune)
  if count(group where degree(rune) >= 3) > 0 then rune:color("#3f6fae") end
end
```

This is the heart of Allomone: describe a *shape* — "a hub," "a big group,"
"anyone sharing these tags" — and whatever runes fit are swept in, today and
after you've added a thousand more.

# The toolbox (quick reference)

| you write | it means |
|---|---|
| `for each rune do … end` | visit every rune, as `rune` |
| `for x in <list> do … end` | visit each item of a list, as `x` |
| `if … then … elseif … else … end` | branching |
| `rune has "tag"` | does the rune carry this tag? |
| `rune is contact` | is the rune this glyph? (`is (contact or event)` for several) |
| `rune.tags` / `rune.glyph` / `rune.name` | a rune's tags / kind / id |
| `rune.<field>` / `field(rune, "key")` | a rune's field value (role, date, bio…) |
| `rune:color("#hex")` | **the effect** — paint the rune |
| `local x = …` / `x = …` | name / update a value |
| `{ "a", "b" }` | a list |
| `function f(a) … return … end` | a reusable function |
| `count`/`len`, `head`, `tail`, `push`, `contains` | list basics |
| `runes`, `runes where …` | the database, filtered |
| `tags_of`, `union`, `intersect`, `minus`, `overlaps` | set theory |
| `any(list[, fn])`, `all(list, fn)` | there-exists / for-all |
| `neighbours`, `linked`, `cluster`, `degree` | follow connections (by structure) |
| `within`, `distance`, `centrality`, `community` | graph measures (reach, importance, groups) |
| `matching` | keep tags matching patterns |
| `+ - * / %`, `== != < > <= >=`, `and or not` | math, comparison, logic |
| `-- …` | a comment |

# Good to know

- **It never changes your data.** A script only *colors*. (Writing data — adding
  tags, editing fields — is planned but deliberately switched off for now.)
- **Enable to apply, disable to remove.** Coloring is live and reversible.
- **Nothing is logged when a script runs.** Because it only reads and paints,
  running a script isn't a change to your database, so it doesn't appear in the
  history — only *editing* a script does. (The why:
  [execution](/concepts/allomone/execution.md).)
- **It won't hang.** Runaway loops/recursion stop safely with an error.
- **Errors show as you type.** The bar under the editor tells you if the script
  parses and how many things it would style.

# Where to go next

Open the **Allomone** tab in the cat colony and enable the examples one by one —
they're ordered from `01-hello-allomone` to `09-color-by-glyph` and match the
sections above. Change a tag or a color and watch the cards update. That's the
whole loop: read your data, decide how it should look, see it immediately.
