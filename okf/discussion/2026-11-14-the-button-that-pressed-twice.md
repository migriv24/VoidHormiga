---
type: Discussion
title: The button that pressed twice
description: The map's canvas took both mouse buttons, and every press handler read a right press as a left one, so a right-click re-selected what it touched and a right-drag drew a rectangle. Found only when multi-selection needed its own right-click menu. Hypotheses.
tags: [status:current, audience:all, confidence:speculative]
timestamp: 2026-11-14T00:00:00Z
---

# The button that pressed twice (2026-11-14)

## What happened

- **Since the map had context menus**, its canvas has been one invisible button
  that accepts the left and the right mouse button, so a right-click reaches the
  menus. ImGui reports *activated* for a press of either.
- **Every press handler asked only "activated?"**: select or move a marker,
  select a shape, start a box selection, grab a handle, start a drawn shape.
  So a right press did all of those too. A right-click on a shape selected that
  shape alone before its menu opened; a right-drag with the rectangle tool drew
  a rectangle.
- **Nobody saw it**, because for one thing under the pointer, "select it, then
  show its menu" looks like the right behaviour.
- **Multi-selection exposed it** (2026-11-14). Shift-selecting two shapes
  worked, and right-clicking one of them showed the single shape's menu: the
  right press had already replaced the selection. A scripted run in the window
  harness found it; the harness gained `shift down|up` for it. Fixed by giving
  every press and release to the left button and letting the right-click
  select for itself ([territory](/concepts/sections/gis/territory.md),
  "Polygons and perimeters").

## Hypotheses

**1. A surface with two buttons hides the second in every later handler.** The
flag that let the right button in was written once, at the top; each handler
written afterwards was written thinking of the left button, and nothing at the
handler says otherwise. *Wrong if:* a handler written after the flag shows it
was considered.

**2. A bug that does something plausible is invisible.** Re-selecting what was
right-clicked is what many applications do on purpose. It was a defect only
once a selection of several had to survive the click. *Wrong if:* someone had
reported the right-drag rectangle and it was dismissed.

**3. Scripted input finds what a person adapts around.** A person stops
right-dragging after one stray rectangle. A script does exactly what it says,
and its screenshot showed one selected shape where two were expected.

## What changed

- `left_press` and `left_release` in `ui/map.cpp`; no handler reads
  `IsItemActivated` alone any more.
- The harness takes `shift down|up` and `key escape|backspace`.

## Still unknown

- Whether other canvases that take both buttons (the Builder's, Void Maiz's
  node canvas) have the same shape. Not checked.
