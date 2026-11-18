---
type: Concept
title: Conventions — every list and canvas in Hormiga behaves the same
description: "How Hormiga keeps Void Maiz's interaction contract (conventions.md there) on every list and canvas, including screens a database describes: which surfaces are converted, the several-selected panel, Delete with undo, Ctrl and the middle button on canvases, and what is not yet done."
tags: [status:current, audience:dev, confidence:asserted]
timestamp: 2026-11-17T00:00:00Z
---

# Conventions in Hormiga (2026-11-17)

The contract itself is Void Maiz's (`../VoidMaiz/okf/concepts/conventions.md`):
in any list, click selects one, Ctrl+click toggles, Shift+click selects a run,
Ctrl+A selects all, Escape and a click on empty space clear, a drag across rows
selects them, Delete deletes what is selected (undoably); on any canvas, the
wheel zooms, the middle button pans, Ctrl or Shift adds to or takes from the
selection, a click on nothing clears. It is one of the
[lines a database cannot move](/concepts/platform/app-as-program.md).

The author: *"UI/UX features like that should just come with any sort of list
view that exists ... build systems such that they are in every list thing, not
just specifically the map."*

## Where it holds

| surface | how | Delete | several selected |
|---|---|---|---|
| Data: list and cards | `maiz::ListSelect` (cards: box select by area); each row is one selectable under its picture and text | ✅ one batch | the shared panel (`draw_many_selected`) |
| Map: Overview | one `ListSelect` across every group, so Shift runs across kinds; a plain click still goes to the place | ✅ | the map Inspector: the shared panel, then colour, icon, remove from the map |
| Notes | `ListSelect` | ✅ | — |
| Calendar: a day's entries, Upcoming | `ListSelect` (Upcoming across days); a plain click moves the grid to the day | — | the calendar Inspector: the shared panel |
| Antfarm: connections | `ListSelect`; double-click finds the node on the graph | — | — |
| Screens: `ui_list` | `ListSelect`, so a database's own lists keep the contract with no setting | — | a button whose action names `{{selected}}` runs once for each, in one batch; the form shows one at a time |
| Map canvas | Ctrl is Shift (add/take); middle-drag pans; Escape and a click on nothing clear (already) | — | — |
| Connections view | Ctrl or Shift toggles a node; a click on nothing and Escape clear; middle-drag pans | — | — |
| Node canvases (Builder, Antfarm wiring) | Void Maiz's canvas: wheel, middle and right drag, marquee, click away (already) | — | — |

**The several-selected panel** (`HormigaApp::draw_many_selected`, ui/widgets.cpp)
is one panel for every tab: how many, which, *tag all selected*, *delete all
selected* (one batch, one undo), *deselect all*. A tab adds what is its own
under it.

## Not yet

- **Arrow keys** (move, Shift to extend): needs ImGui's keyboard navigation on,
  which also changes what arrow keys do on the canvases; not turned on.
- **Choice lists** (the active layer, kinds in Edit kinds, the Allomone script
  being edited, Builder's document list): they choose one thing to show, and stay
  single. Allomone's scripts list may want several selected (enable/disable/delete
  several); its selection is not a rune list yet.
- **The phone's long-press selection mode** (Maiz conventions §phone): designed,
  not built in Hormiga's phone lists.
- **Right-click selects first.** A right-click on an unselected row opens its
  menu on that row only, as before; whether it should also become the selection
  is the platform question both ways.
