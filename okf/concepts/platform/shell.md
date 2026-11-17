---
type: Concept
title: The application as data — the shell, screens, and actions in the .miga
description: "Decided 2026-11-15: a .miga carries the application, not only the database. Which tabs exist, their order, the database's default layout, and new tabs a client builds themselves: screens, a Builder document kind rendered live inside Hormiga, made of components bound to data, whose buttons run actions written in the console's own language. Each person's arrangement stays on their device unless they save it to the database. Devices are reached through the Antfarm."
tags: [status:current, audience:all, confidence:asserted]
timestamp: 2026-11-15T00:00:00Z
---

# What the author asked

> *"the hormiga application itself, the layout of the tabs, the tabs it has,
> etc, should be able to be modifiable. New tabs and functionalities should be
> modifiable and those modifications should be able to store themselves in a
> miga file. It's not just the database that's being loaded anymore, but the
> configuration of the application itself."*

> *"We shouldn't have specific stuff for printers or label printers. That's too
> granular and specific. We have to think general, and give clients the tools to
> implement the functionality themselves."*

The prompt was issue #3: a client who wants to run the **stock** application
with their whole workflow in one file. The answer is not their workflow built
in; it is the tools to build any workflow, carried in the file.

# Decided (the author, 2026-11-15)

- **New tabs are a Builder document kind, `screen`.** The Builder already
  composes data-bound documents from components on a grid and renders them to a
  website or a newsletter ([builder](/concepts/sections/builder.md): *"a GUI
  builder for data-bound documents"*). A screen is the same thing rendered **live
  inside Hormiga**, as a tab. One editor and one component system, and the
  ancestor Void Maiz's planned GUI maker was always meant to have.
- **The database carries a default; a person's arrangement is theirs.** The
  `.miga` says which tabs exist, their order and a default layout. Rearranging on
  a device changes that device only, until its person chooses *Save this layout
  to the database*.

# The shell

Two config keys in the database (config tier: dispatched, logged, carried in the
`.miga`, undo-exempt like every other piece of view state):

| key | what | written by |
|---|---|---|
| `shell.tabs` | the tabs, in order: `[{"id":"data"}, {"id":"screen:counter","title":"Counter"}, ...]` with `"hidden": true` for one closed by default | the View menu; `shell` verbs |
| `shell.layout` | the database's default arrangement (ImGui's own docking text) | *Save this layout to the database* |

**A tab's id** is a built-in section (`data`, `builder`, `antfarm`, `map`,
`calendar`, `notes`, `publish`, `allomone`, `console`, `settings`, `style`) or
`screen:<document mantle>`. A built-in tab can be hidden by default or
reordered, never removed from the application: the View menu always reopens
it, because a database must not be able to lock a person out of their own data.

**Layout.** A device keeps its own arrangement, as it always has. Opening a
database whose default this device has not applied before applies it once;
*Use the database's layout* applies it again on request.

# Screens

**A screen is a document mantle whose `document` rune says `kind: screen`.** It
is built in the Builder like any document (rows, slots, spans) and appears as a
tab named by its title. Components on a screen:

| component | what it does in the tab |
|---|---|
| any content block (`section_header`, `narrative`, `stat`, `divider`, `quote`) | drawn as text, as in the Builder's preview |
| **`ui_list`** | the runes a query matches (the one search grammar), with the fields it names as columns; clicking a row **selects** it for the screen |
| **`ui_form`** | the selected rune's fields (or those it names), edited in place through the dispatcher |
| **`ui_input`** | a labelled box; Enter runs its action with what was typed as `{{input}}`. A barcode scanner is a keyboard, so a scan queue is an input whose action makes a rune |
| **`ui_button`** | a labelled button that runs its action |

A block the tab cannot draw yet says so in place, rather than vanishing.

**A screen's selection** is one rune, shared by its components: a list sets it,
a form shows it, an action reads it.

# Actions

**An action is a few lines of the console's own language**: dispatcher commands
and Hormiga's verbs (`map …`, `doc …`, `farm …`, `effect …`), one per line, with
placeholders filled when it runs:

| placeholder | becomes |
|---|---|
| `{{input}}` | what was typed in the input that ran it |
| `{{selected}}` | the selected rune's name |
| `{{selected.<field>}}` | one of its fields |
| `{{new}}` | a fresh, unused rune name |
| `{{today}}` | today's date, `YYYY-MM-DD` |

```
rune new product {{new}}
set {{new}} barcode {{input}}
tag {{new}} +type:product +to-label
```

**The rules that make a received `.miga` safe to click:**

- **A value is always a quoted argument.** `{{input}}` and every field go in as
  one quoted string, so what someone types or scans can never become a second
  command. A name (`{{selected}}`, `{{new}}`) goes in bare only if it is
  command-safe, and the action is refused otherwise.
- **Commands are one batch, one undo**, run in the data mantle; a verb line
  runs as that verb. Every line is in the transcript, attributed to the person.
- **A button shows what it will do** (its lines, on hover) before it does it.
- **Nothing runs unless a person clicks.** There is no "on open", no timer and
  no trigger: a workflow that must act by itself is Allomone's proposal or an
  Antfarm run (Q135).
- **No new language.** Actions are lines the console already runs. Logic that
  decides belongs to Allomone, which stays derive-only (Q135); effects that leave
  the device go through the Antfarm, which says what leaves.

# Devices

Talking to hardware (a printer, a scale, a sign, a sensor) is the Antfarm's, as
the author directed: *"Drivers, connecting to other devices, and sending data
somewhere to do an action (like printing), would be something for the ant farm
as well."* It is general, never a printer:

- **A `device` node** says where a device is and how it is reached: a transport
  (a TCP socket, UDP, HTTP; Bluetooth LE and serial later) and an address. The
  transports are Void Hammer's (`voidhammer_reach`), which already reaches
  devices for the networking stack, and its **dictionary of hardware** is where
  a device model's facts live (what it speaks, on which port), so a node can name
  a model instead of a person knowing its port.
- **Sending is a capability** (`send-to-device`: text in, a report out), so a
  button's action can say `effect send <device> {{selected.label_text}}` and the
  device node, not the screen, knows how to reach it and says what leaves.
- **What is sent is the client's.** A label is a template the client writes in
  their printer's own language; a reading from a scale is an `ask`. Hormiga
  carries the bytes; it never learns what a label is.

Built in this order (step 3 of this page): `send` over TCP first, then an HTTP
`ask` whose answer maps onto fields, then the rest of Hammer's transports.

# What is built

| | step |
|---|---|
| ✅ | removed the printer and product kinds (2026-11-15): too specific; replaced by the general shape above |
| ✅ | 1. **the shell**: `shell.tabs`, `shell.layout`, `shell.id` (`domain/shell.hpp`, `app/shell_app.cpp`); *Windows > Save this layout to the database* and *Use the database's layout*; the device's record of applied defaults as a section of `imgui.ini`, decided once its first frame has read it, and the layout loaded between frames; the `shell` verbs, with `save-layout` and `use-layout` |
| ✅ | 2. **screens**: the `screen` document kind, `ui_list`, `ui_form`, `ui_input` and `ui_button` in the Builder's palette under *Screen*, drawn live as tabs (`ui/screens.cpp`); actions compiled by `domain/actions.hpp` |
| ✅ | 3. **devices**: the Antfarm's `device` node (TCP), answering `send-to-device`; `effect send <node> <text>`; the bytes moved once a frame through Void Hammer's sockets (`platform/device_link.*`), with a report and anything the device answered |
| ⬜ | a screen opened on the phone |
| ⬜ | `ask` (HTTP: a request whose answer maps onto fields), UDP, Bluetooth LE, serial; names looked up, not only IPv4 addresses |
| ⬜ | a Void Hammer dictionary entry filling a device node's port and line ending from its model |
| ⬜ | `ui_list` reading the store, not only what is loaded (the frontier) |

**Measured in the window** (2026-11-15): a screen with an input, a list of
`@scanned` contacts, a form and a button, built as a document by commands,
appeared as a tab named by its title; typing *Ana O'Brien* and Enter made the
contact in one batch (one undo); selecting her row filled the form; editing the
email wrote it; *Mark done* tagged her and not Ben.

**Tested:** `screen_smoke` (an action's values through a real Void Core: a name
with quotes, an apostrophe, a backslash and a newline followed by `rm ana`
stored exactly as typed, one value, and nothing else ran; names refused unless
command-safe; unknown placeholders and a missing selection refused; the
shell's tabs never losing a built-in, round-tripping through `config set`);
`device_smoke` (a loopback listener received exactly the bytes and its answer
came back in the report; a port with nothing behind it fails plainly);
`farm_smoke` (the `device` kind answers `send-to-device`).
