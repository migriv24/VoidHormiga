---
type: Design
title: The application as a program — the lines it holds, the phone, and what Allomone must become
description: "An investigation (2026-11-17) of the mutable application Hormiga is becoming: tabs, layouts and screens stored in the .miga, built and changed from the desktop and the phone at once. What a database may change and what it may not (the lines to hold), how every component reaches the phone, what an Allomone script IS, and the write path it needs (plans: derived, previewed, scoped, budgeted, applied as one batch) so that changing data with it stops being frightening. Ends with a phased plan and the questions it opens."
tags: [status:direction, audience:dev, confidence:exploratory, design]
timestamp: 2026-11-17T00:00:00Z
---

# The application as a program (2026-11-17)

The author, after asking for Shift and Ctrl selection in one of the map's
panels:

> *"UI/UX features like that should just come with any sort of list view that
> exists ... for an application where you essentially construct the app itself
> as well, we should definitely have some rules and lines to hold ... if a future
> user of hormiga wants the entire thing to JUST be the map tab, then they can
> save that in their miga file and that's it! ... It should be easy for these
> clients to create changes from mobile and hormiga at the same time ... if
> someone wants to create a new tab that has a new kind of functionality, it has
> to have some sort of effect for mobile as well ... we need a custom
> programming language for all void based applications ... right now, it's
> scary to use allomone to actually transform or modify data ... what even ARE
> its scripts? what does allomone need to work on to really work?"*

This page answers in order: what is already built, the lines to hold, the
phone, what an Allomone script is, the write path, and a plan. It builds on
[the shell](/concepts/platform/shell.md) (tabs, layouts, screens, actions,
devices, 2026-11-15) and on Allomone's own design pages in Void Allomone
(`materialization.md`, `emergence.md`, `governance.md`).

## 1. What is already the program

A `.miga` already holds more than data. Four layers, each stored and synced
the same way (dispatcher commands, so logged, undoable, replayable):

| layer | stored as | today |
|---|---|---|
| **the shell** | config: `shell.tabs`, `shell.layout`, `shell.id` | which tabs, in what order, which closed, the default arrangement ([shell](/concepts/platform/shell.md)) |
| **screens** | Builder documents of kind `screen`: component runes (`ui_list`, `ui_form`, `ui_input`, `ui_button`) | a new tab, laid out by rows and spans |
| **actions** | a component's `action` field: console lines with quoted placeholders | what a button or an input does; one batch, one undo |
| **rules** | Allomone scripts (`allo-script` runes) in the data mantle | derive-only: colours, labels, badges, visibility on the web |
| **wiring** | Antfarm nodes (`farm` mantle) | where things go: sites, buckets, devices |

So "the entire thing is just the map tab" is already one command:
`shell hide` for every other tab, then *Save this layout to the database*. What
is missing is not storage. It is **behaviour**: screens can only run console
lines, lists can only filter by a search string, nothing on a screen can depend
on anything else on it, and the phone shows none of it.

## 2. The lines to hold

A program that its users rewrite needs a part they cannot rewrite, or every
database becomes a different application. The rule: **a database composes; the
host guarantees.** What a database may change is *which* parts appear, *where*,
*bound to what*, and *what they do through commands*. What it may not change is
how a part behaves under a person's hand, what reaches the outside, and how a
change is recorded.

**Held by the host, never by a database:**

1. **Interaction.** Every list and canvas keeps [the conventions](/concepts/platform/conventions.md):
   click, Ctrl, Shift, Ctrl+A, Escape, click away, box select, Delete with undo;
   the wheel, the middle button. Components are built on Void Maiz's
   `ListSelect`, so a screen's list has them without asking. *Built
   2026-11-17.*
2. **Every change is a command.** A screen, a rule, a button: nothing writes
   except through the dispatcher, so everything is in the log, undoes, and
   replays headless (rule 3). A component that cannot be expressed as commands
   is not a component.
3. **The built-in tabs can be hidden, never removed** (the shell, 2026-11-15),
   so no database can lock a person out of their own data. *Settings* cannot be
   hidden at all, so there is always a way back.
4. **Privacy at the seams.** Internal-notes-class fields never reach an output
   (rule 6), whatever a screen or a script asks for. A component reads through
   the same seam the exports do.
5. **Effects are capabilities.** Reaching outside (the network, a device, a
   publish) goes through an Antfarm node a person wired, never from a script
   directly ([capabilities](/concepts/platform/antfarm/capabilities.md)).
6. **What a database's text can do is quoted.** A typed or scanned value is one
   quoted argument, never command text ([shell](/concepts/platform/shell.md),
   "Safety").
7. **Every component has a phone form** (§3), or the database cannot add it.
8. **Per-person stays per-person.** A person's own arrangement of tabs stays on
   their device until they save it to the database (Q136's ruling).

**Composed by the database:** the tabs and their order, screens and their
components, what a component is bound to (a query, a field list), what a button
does, the rules that colour and label, the Antfarm's wiring.

## 3. The phone

A screen that is a picture of a desktop layout cannot be shown on a phone. A
screen that is a **list of meanings** can. The component kinds are therefore
semantic, and each one has two renderers, the desktop's and the phone's:

| component | desktop | phone |
|---|---|---|
| `ui_list` | a table with columns, the list contract | a list of cards (title, the first two columns as subtitle); long-press selects (conventions §phone) |
| `ui_form` | label + field rows | the phone's existing form sheet, one field per row |
| `ui_input` | a text box; Enter runs its action | a text box with the matching keyboard (Maiz's `InputKind`); Enter or Done runs it |
| `ui_button` | a button | a full-width button; inside a list's selection bar when it acts on `{{selected}}` |
| section, narrative, stat | as in documents | as in documents (already phone-ready) |

**The layout translates by rule, not by a second design:** a screen's rows
become the phone's vertical order; spans are ignored; a list and the form that
reads its selection become a list that opens the form as a sheet. A screen may
add hints (`phone.order`, `phone.hide`), never a second layout. A new
component kind is accepted only with both renderers; that is line 7.

**Editing from both at once.** Everything a screen is lives in runes and config
written by commands, so the phone and the desktop edit the same database the
way they already edit contacts: through sync, merged by the core, attributed to
who changed it. Two consequences to design for:

- *The shell is one config value.* Two people reordering tabs at once is a
  last-write-wins on `shell.tabs`. Acceptable while tabs are few; if it bites,
  `shell.tabs` becomes one rune per tab with an `order` field, which merges
  field by field.
- *A screen open on one device while another deletes its list* shows the
  empty-screen message, never a crash: components read by name through
  `scene.find`, and a missing one is absent, not dangling.

**Not built:** screens on the phone at all. Today a screen is a desktop window
only (noted in [shell](/concepts/platform/shell.md)). It is phase 1 below.

## 4. What an Allomone script is

Today, precisely (Void Allomone's `start-here.md`, read 2026-11-17):

- **A script is a rune**, glyph `allo-script`, in the data mantle: a title, an
  `enabled` flag, and a body of rules. It is in the `.miga` because the data
  mantle is; it syncs and undoes like a contact. Editing it is one `set` of its
  body on commit.
- **A script is a set of rules, not a sequence.** `when <condition> then
  <effect>, <effect>`. No order, no loops, no `else`. Every enabled script is
  evaluated over every subject at once, and a **lattice merge** combines what
  all of them say: within one script the sharper rule wins; across scripts an
  equal-strength disagreement becomes **⊤, a visible conflict** a person
  settles with a logged command.
- **A script derives; it never writes.** Its output is an annotation set
  (colour, label, badge, weight, note, hidden-on-web) that renderers read.
  Disabling a script removes its effects with nothing to undo. This is tier 0 of
  Allomone's four tiers (derive, amend, create, reflect), and it is the only one
  built.

That is why using Allomone to change data feels frightening: **it cannot**, and
any way of making it do so by hand (copying a derived value into a field)
bypasses everything the language guarantees.

What a script *should* be, for the application as a program, is three kinds
under one language:

| kind | runs | output | tier |
|---|---|---|---|
| **rule** (today) | always, over everything | annotations: how things look, what is shown | 0, derive |
| **view** (new) | always, over a screen | a component's state: visible, enabled, its label, its list's members | 0, derive |
| **plan** (new) | when asked: a button, a schedule, a person | a list of commands, previewed, applied as one batch | 1, amend; 2, create, budgeted |

A **view** script is still derive-only, so it is safe today: `when screen
"intake" and selected-count 0 then button "check-in" disabled`, or a list whose
members are an Allomone condition instead of a search string. This alone covers
most of what "a new tab with new functionality" needs, without any write path.

## 5. The write path: plans

The frightening part is a script that changes data. The answer is not to make
it less able but to make every write **visible before, reversible after, and
bounded throughout**. Allomone's own design already says how (materialization:
"Allomone compiles to Voidscript", "propose, do not generate", "no write tier
ships before the analyzer that bounds it"). Hormiga's version of it, concretely:

1. **A plan is derived, not executed.** A plan script's rules produce a *list
   of commands* (`tag ana +overdue`, `set ana status "late"`) as their output,
   by the same pure evaluation as a rule. Computing it changes nothing, so it
   can be computed as often as wanted: that is the dry run, for free.
2. **A plan is shown before it is applied.** "This will change 14 runes: +overdue
   on 12, status on 2" with the list, and the conflicts (two rules wanting
   different values for one field are ⊤, exactly as for colours: the plan is
   refused until a person settles it).
3. **A plan is applied as one batch**: one undo frame, attributed to
   `agent:allomone/<script>` with the person who pressed apply, replayable
   headless. A plan run by the Antfarm on a schedule is the same batch with the
   run as its author (Q135's lean, kept).
4. **A plan declares its scope.** The script says what it may touch
   (`writes tag "overdue", field "status" on kind "contact"`), and the host
   refuses a command outside it. A database's script can never reach a field
   it did not declare, and a reader can see a script's reach without reading its
   rules.
5. **A plan has a budget.** At most N commands per application (default 1,000);
   over it, nothing is applied and the plan says so. Creating runes (tier 2)
   needs a budget anyway: new runes are new subjects.
6. **Plans do not feed plans, until the analyzer says they may.** A plan that
   runs because another plan changed something is a reactive write, and the
   loop risk lives there. The effect graph (Void Allomone's `sentinel.hpp`)
   already computes which scripts read what others write; reactive plans wait
   until it is a precondition, as Allomone's rule requires.
7. **A plan carries its own tests**: "given these runes, it proposes these
   commands", stored with the script and run when it is saved. A database whose
   scripts change data then has its own regression suite inside it.
8. **No script writes scripts** (tier 3). A script may *propose* a rule (the
   Weaver does this today); a person accepts it.

With plans, a button's action can be a plan instead of console lines, an
Antfarm schedule can run a plan, and Q135's workflow ("when a scanned item
matches, create a product") is a plan with a budget, previewed or applied by a
run.

## 6. A language for every Void application

The author: *"we need a custom programming language for all void based
applications."* The pieces exist and divide cleanly:

- **Voidscript** (Void Core): the mechanism. Every statement is a dispatcher
  command; it has loops and variables; it is what replay runs.
- **Allomone** (its own project, reached through Void Maiz): the logic.
  Declarative, always terminating, composable across sources, conflicts
  visible. Rules, views and plans are all Allomone; a plan *compiles to*
  Voidscript commands, never the reverse.
- **A host's domain library** (Hormiga's `hormiga_allomone`): the vocabulary.
  Its predicates (dates, places, roles), its properties and their merge laws.
  Another Void application brings its own and shares the rest.

So the language for all Void applications is Allomone, with each application
contributing a vocabulary, and Voidscript underneath as what actually happens.
That is already Void Allomone's stated design (`host-protocol.md`); what is
missing is the write tier, the view kind, and maturity.

## 7. What Allomone needs, to really work

From Void Allomone's own open list (`roadmap.md`, "Everything still open") and
from what Hormiga needs here, in the order they block:

1. **Plans (write tier 1)**: a rule effect that is a command (`then tag
   "+overdue"`, `then set "status" "late"`), a plan output type, scope
   declarations, a budget; the analyzer as a precondition (item 14 there).
2. **Aggregates and sets** (items 7–8): `count`, `any`, `all` over a set
   expression. A view needs "how many are selected"; a plan needs "every event
   with no host".
3. **Name-to-id references** (item 11): a script that names a rune must survive
   its rename. Screens and plans name things far more than colour rules did.
4. **Host context predicates**: `screen`, `selected`, `input`, `device`
   (partly there: `device`, `with`). These are host-supplied, through the host
   protocol, not built into the language.
5. **Bounded evaluation over a store** (item 17's question, under the
   infinite-dataset premise): derivation today runs over the loaded scene. A
   plan over "every contact" must run over the store in bounded pieces, or say
   what it covered.
6. **Errors a non-programmer can act on**: the rule, the subject and the reason,
   on the line that caused it; `check` before save.
7. **Tests as data** (§5.7): an example format the engine runs.

Items 1, 2 and 3 are Void Allomone's to build (rule 4: we do not patch it). The
message to it is drafted locally as
`MESSAGE_FOR_VOIDALLOMONE_hormiga-plans-views-and-the-app-as-a-program-2026-11-17.md`.

## 8. The plan

| phase | what | depends on | safe because |
|---|---|---|---|
| **0** ✅ | the list and canvas contract, everywhere (Void Maiz `ListSelect`; Hormiga's Data, Notes, map Overview, calendar, Antfarm and screen lists; several-selected panels; Ctrl and the middle button on canvases) | nothing | it is interaction, not data |
| **1** | screens on the phone, by the component table in §3 | nothing | rendering only |
| **2** | view scripts: a component's visible/enabled/label and a list's members from Allomone | Allomone's host predicates (`selected`, `screen`) | still derive-only |
| **3** | plans on demand: a button or a person applies a previewed, scoped, budgeted plan as one batch | Allomone write tier 1 | nothing writes unseen; one undo |
| **4** | plans on a schedule, from the Antfarm | phase 3 | a run is accountable; still previewable in its log |
| **5** | reactive plans | the analyzer as a precondition | the effect graph refuses cycles |
| — | tier 3 (scripts writing scripts) | — | not planned: propose only |

## Questions this opens

Q138–Q141 in [developer questions](/developer_questions.md).
