---
type: Report
title: Networking stack audit — 2026-10-18
description: "The measured state of Void Hormiga's networking after Void Maiz moved its engine to Void Hammer on 2026-10-17. Sibling versions and commits, the build and test baseline, what each library actually owns today, every old include and target still in Hormiga, the delta ledger, the privacy and mobile findings, and the decisions the author must make before any transport changes."
tags: [status:current, audience:dev, confidence:measured]
timestamp: 2026-10-18T00:00:00Z
---

# Why this audit exists

Void Maiz changed a great deal between 2026-09-22 and 2026-10-17. The largest
change is that the family's networking engine moved to **Void Hammer** on
2026-10-17. Void Hormiga still builds, but only because Void Maiz keeps
forwarding headers and alias targets. The author asked for an investigation
before any update, and for the current behaviour of each library to be written
down because several are still moving.

This report is the evidence. The durable map is
[the networking stack](/concepts/platform/networking-stack.md). The plan that
produced it is `plans/networking-library-investigation.md`.

# Method

Read every sibling's OKF and the code entry points it names; inventory every
Hormiga include, target, workflow and document statement that still names the
old ownership; build and test the current tree; and record what was and was not
run. Nothing was changed in `src/`, `CMakeLists.txt`, the Android build or the
workflows during this audit.

## Sibling versions on 2026-10-18

| repository | commit | date | note |
|---|---|---|---|
| VoidCore | `859ef17` | 2026-10-01 | 0.2.14 release block |
| VoidMaiz | `5ea6720` | 2026-10-17 | networking moved to Void Hammer |
| VoidHammer | **no Git repository** | 2026-10-17 | directory exists; no `.git`, no public remote |
| VoidPalabra | `603a7db` | 2026-10-05 | Reticulum pipes and runtime interfaces |
| VoidAllomone | `327732d` | 2026-10-04 | inbox and lattice merge |
| VoidVerguenza | `ce2db3a` | 2026-10-03 | first desktop vault runs |
| VoidChisme | `d200eeb` | 2026-10-03 | phase 0 library, CLI, window |
| VoidHormiga | `80881fd` | 2026-10-15 | 0.2.1 |

**Void Hammer has no Git history.** That is not a documentation detail: CI and
release cannot clone it, so they build without networking and report green. The
build cache confirms the local tree is configured with
`VOIDHAMMER_ROOT=C:/Users/migri/Documents/Projects/VoidHammer`.

# Build and test baseline

The existing `build/` was up to date and configured with Reticulum enabled
(`MAIZ_RETICULUM:BOOL=ON`). `cmake --build build --parallel 8` succeeded.

`ctest --test-dir build --output-on-failure` under `cmd.exe` produced **53 of
56 passing**:

| test | result | reading |
|---|---|---|
| `lan_smoke` | **passed** | sockets, interface facts |
| `net_smoke` | **passed** | `Network` over an in-memory lossy mesh |
| `lanlink_smoke` | **passed** | two Cores over real TCP, join / Allow / Deny |
| `radio_smoke` | **passed** | Reticulum over the loopback “LE” radio, two processes |
| `rnslink_smoke` | **passed** | two Cores over Reticulum links, Allow, re-admit by identity |
| `reduce_conformance` | **failed 17/25** | Void Maiz's known reducer gap, unrelated |
| `hormiga_golden_render` | **failed** | `dirname`, `mktemp`, `cat`, `grep`, `rm` absent from `cmd.exe` |
| `hormiga_headless_smoke` | **failed** | same shell-tooling cause |

Re-running the two shell tests with `C:\msys64\usr\bin` on `PATH` removed the
missing-tool errors, but both still failed at `cd /tmp/...` because MSYS2's
`/tmp` and the shell's working directory did not agree. They are environment
failures, not product failures, and neither exercises networking. **The five
networking tests are green.**

# What each library does now

## Void Hammer — reaching devices and the sync engine

Void Hammer was founded 2026-10-17. Its `net/` directory contains the code that
moved from Void Maiz:

| target | contains | depends on |
|---|---|---|
| `voidhammer_reach` | `lan`, the radio holiday, the loopback radio | platform sockets only |
| `voidhammer_net` | `Network`, `LanSession`, `RnsSession`, `RadioBridge` | `voidmaiz`, `voidhammer_reach`, `voidpalabra` |

The ownership line is explicit: **Void Hammer reaches devices and runs the sync
engine; Void Maiz owns what networking looks like; Void Palabra owns what
crosses; the application owns who may join and what may leave.** See
[VoidHammer networking](../../../VoidHammer/okf/concepts/networking.md:18).

The public surface Hormiga uses is unchanged in name: `maiz::Network`,
`maiz::LanSession`, `maiz::RnsSession`, `maiz::RadioPlatform`, `maiz::RadioBridge`.
The code keeps `maiz::` names until Void Hammer's Q10 is decided.

**What is built and proven** (all five tests ran in this audit):

- `LanSession`: UDP beacon, unicast reply, TCP join, Allow/Deny, Palabra stream
  framing, idle death at 12 s, `already_allowed`.
- `Network`: one replica, one Palabra session per link, observe → persist →
  share → merge → splice, presence, conflicts, files, `resync`, counters.
- `RnsSession`: Reticulum announces, link join, Allow/Deny, re-admit by proven
  identity, one Palabra frame per message.
- `RadioBridge`: LE peer as an HDLC pipe, Wi-Fi Direct group as a UDP interface,
  `waiting(peer)` for progress.
- The radio holiday: LE advertise/scan/GATT stream, Wi-Fi Direct DNS-SD and
  groups, permissions, loopback stand-in.

**What is not done:** a standalone Hammer build, a public repository, moving
`MaizRadio.java` into Hammer, and renaming to `hammer::`. Q7 (discovery without
broadcast), Q8 (the room host), Q9 (internet probes) and Q11 (who owns the cell
plan) are open.

## Void Palabra — what crosses

Void Palabra is compatible with the version Hormiga builds against and gained
no breaking change since the last adoption. Reticulum is the family network:
the optional `voidpalabra_reticulum` companion carries a vendored microReticulum,
links are encrypted and forward-secret, and Palabra's sync sessions ride them.
The 2026-10-05 work added **pipes** (an interface whose packets the host
carries) and runtime interface add/remove, which is what the radio bridge uses.
The OS-seeded entropy fix of 2026-10-01 is in place. See
[Palabra Reticulum](../../../VoidPalabra/okf/concepts/reticulum.md:83).

## Void Maiz — what networking looks like, and four adjacent surfaces

Void Maiz kept presence, marks, the roster, claims, wires, `netview` and
Allomone's `present`. It moved the engine out. Four adjacent pieces are new or
relevant:

1. **`voidmaiz/nav.hpp`** — Hormiga's navigation bar, lifted. It has
   `NavItem{icon,label,badge,draw}`, `nav_bar`, `nav_bar_centred` with a
   `CentreFace` callback, `begin_app_bar`/`end_app_bar`, a multi-platform
   `back_pressed()` that claims Back, `unclaim_back()`, and `Stack(root)`.
   Hormiga's own [`src/phone/nav.hpp`](../../src/phone/nav.hpp:1) says it was
   written to be lifted.
2. **`voidmaiz/http.hpp`** — one HTTP seam: desktop `curl`, Android
   `HttpURLConnection`, iOS `NSURLSession`. It is implemented in
   `voidmaiz_update`, which Hormiga already links through `voidmaiz_view`.
3. **`voidmaiz/update.hpp` / `updateview.hpp`** — the generalized update client,
   already proven against Hormiga's live feed. Q86 tracks the migration. The
   consent file must be carried over.
4. **The portable app shell** — `maiz::App`, `maiz::Device` and
   `maiz_add_app`. It is the iOS path and would delete Hormiga's Android shell.
   It is a separate, larger migration and is **not** part of this update.

## Void Chisme — watching a network

Chisme has a phase-0 library (`voidchisme`), application services (`chisme_app`),
a CLI and a window. Its own roadmap places the Reticulum adapter and the
observer role in later phases. It has no code that consumes a Hormiga database
today. The observer member (`Q99`) remains Hormiga's to add: a member that
receives presence and sync statistics and never the database.

## Void Verguenza — shared secrets

Verguenza's foundation library `voidverguenza` exists and runs a single-device
desktop vault. The **client seam Hormiga needs is phase 2 and is not built**:
there is no `describe`/`use` protocol and no client library target yet. The
Antfarm Key node therefore keeps using Hormiga's own vault for now. The
envelope library is the natural future home of Hormiga's sealed-secret format
(Q88), but the migration is blocked on Verguenza phase 2.

# What Hormiga uses today

## Includes

| old include | sites |
|---|---|
| `voidmaiz/lan.hpp` | [`src/sync/peer.cpp`](../../src/sync/peer.cpp:9), [`src/app/lan_presence.cpp`](../../src/app/lan_presence.cpp:5), [`src/ui/share.cpp`](../../src/ui/share.cpp:21), [`android/src/main_android.cpp`](../../android/src/main_android.cpp:31) |
| `voidmaiz/net.hpp` | [`src/app/lan_share.hpp`](../../src/app/lan_share.hpp:29) |
| `voidmaiz/radio.hpp` | [`src/app/lan_radio.cpp`](../../src/app/lan_radio.cpp:42), [`src/phone/phone_migos.cpp`](../../src/phone/phone_migos.cpp:30), [`android/src/main_android.cpp`](../../android/src/main_android.cpp:33) |
| `voidmaiz/rnsradio.hpp` | [`src/app/lan_radio.cpp`](../../src/app/lan_radio.cpp:46) |

`voidmaiz/update.hpp` is also used by the Android shell
([`android/src/main_android.cpp`](../../android/src/main_android.cpp:34)) and is
adjacent to the HTTP and update work.

## Targets and flags

- [`CMakeLists.txt`](../../CMakeLists.txt:102) links `hormiga_sync` to
  `voidmaiz_lan`.
- [`CMakeLists.txt`](../../CMakeLists.txt:110), `335` and `407` test for and link
  `voidmaiz_net` for the GUI and CLI.
- [`android/hormiga_android.cmake`](../../android/hormiga_android.cmake:37) does
  the same for the APK.
- `VOIDMAIZ_RETICULUM` gates the radio bridge in
  [`src/app/lan_radio.cpp`](../../src/app/lan_radio.cpp:44); `HORMIGA_HAVE_NET`
  gates the sync seam.

The local build proves that `voidmaiz_lan` and `voidmaiz_net` are interface
aliases over `voidhammer_reach` and `voidhammer_net`, and that Void Hammer is
addressed as `build/voidmaiz/voidhammer_net/`.

## What actually carries bytes

- **LAN sync:** Hormiga's own sealed session. `sync/peer.*` owns the multicast
  beacon, X25519 handshake, short authentication string and `secretstream`;
  `app/lan_wire.*` seals presence to the room key; `app/lan_net.cpp` drives
  `maiz::Network` over it. The room key, not Reticulum identity, is the
  membership boundary.
- **Radios:** Hormiga drives `voidpalabra::reticulum::Node` and
  `maiz::RadioBridge` directly in `app/lan_radio.cpp`. It has its own room-tag
  discovery and HZP1 proof, and its own compression. `RnsSession` is **not**
  used.
- **Membership:** `members.json` is a Void Core document of `member` runes with
  `username`, `color`, `fingerprint`, `public_key` (a libsodium `crypto_kx`
  key), `role`, `joined`, `invited_by`, `last_seen`, `avatar`. There is **no
  Reticulum identity field**.
- **Discovery effect:** `src/app/sync_ops.cpp` still constructs
  `hormiga::sync::Beacon` for `effect sync-discover`.

# Delta ledger

Order is risk-first, not value-first. “Adopt” means a later, approved change.

| id | finding | impact | recommended action | risk |
|---|---|---|---|---|
| **H1** | Engine moved to Void Hammer; Hormiga uses aliases and forwarding headers | Builds today; aliases are scheduled to disappear after both hosts move | Adopt `voidhammer/*.hpp` and `voidhammer_reach`/`voidhammer_net` directly, once measured | low |
| **H2** | Void Hammer is not a declared sibling in README, AGENT-GUIDE, CI or release | A fresh clone silently builds without networking; CI is green without testing it | Document Hammer as an optional sibling; add it to the build instructions; make the CI omission explicit | low |
| **H3** | Android CMake, manifest comments and shell comments still name `voidmaiz_net`, `voidmaiz/radio.hpp`, `voidmaiz/lan.hpp` | Misleads the next change; no behaviour impact | Update comments and the target name to `voidhammer_net` when H1 lands | low |
| **H4** | `voidmaiz/nav.hpp` now exists | Our `src/phone/nav.*` is duplicated code; `back_pressed` semantics changed upstream | Adopt the library bar, keep `draw_ant_button` as a `CentreFace`, delete our header/source | low |
| **H5** | Void Maiz's update client is proven against Hormiga's feed; Q86 still open | Hormiga's client lacks the Android apply path and shares the HTTP gap | Migrate after H1/H2, with a one-time consent-file migration | medium |
| **H6** | `voidmaiz/http.hpp` is a general seam; Hormiga shells out to `curl` for publishing, tiles and feeds | A phone has no `curl`; the HTTP gap is wider than updates | Adopt the seam for every outbound fetch, keeping `curl` on desktop | medium |
| **H7** | Portable `maiz::App`/`Device` shell exists | The iOS path; deletes the Android shell; no `system()`/`popen()` | Separate project, after the networking update. Do not bundle | high |
| **H8** | `RnsSession` exists but announces display name, colour and database name | Moving LAN sync to it as-is would undo Hormiga's sealed presence | **Do not switch the default transport.** Ask for an opaque announce and a room-key proof, or keep the sealed LAN session | high |
| **H9** | Members registry has a libsodium key but no Reticulum identity | `RnsSession::already_allowed` cannot re-admit a known member | Add a Reticulum identity field when H8 is approved; keep the libsodium key for the sealed LAN path | medium |
| **H10** | Radio path is bespoke: `Node` + `RadioBridge` + HZP1, not `RnsSession` | Two membership/proof implementations; radio and LAN differ | Investigate deduplicating after H8; keep the room proof until RnsSession can carry it | medium |
| **H11** | Chisme observer role not built | No activity measurement today | Design the role in Hormiga's members registry; implement when Chisme reaches its Reticulum phase | low now |
| **H12** | Verguenza client seam not built | Antfarm Key node cannot name a Verguenza secret yet | Keep Hormiga's vault; migrate when Verguenza phase 2 exists | low now |
| **H13** | Many OKF pages still state the old ownership | The next agent will follow the wrong map | Update the pages and index in this pass | low |
| **H14** | CI builds without networking because Hammer has no remote | Green CI is not evidence about the network | Record the gap; add Hammer to CI when it has a public repository | medium |

# Privacy and security

## The room key versus Reticulum announces

Hormiga's LAN path deliberately puts no names in the clear. A beacon carries an
opaque room id and a sealed presence body; an offer is announced only while a
person is sharing. `RnsSession::about_json` instead puts `app`, device `id`,
person's `name`, `rgb`, `host` and the database's `net` into every announce,
and announces are public. That is Q98, found by Void Chisme on 2026-10-01. It
is not a theory: the code is
[`rnslink.cpp`](../../../VoidHammer/net/src/rnslink.cpp:58), and `RnsOptions`
has no opaque mode.

**Consequence:** H8 cannot ship as a simple swap. Either Reticulum announces
become opaque (application plus an opaque room id, names inside the sealed
presence), or Hormiga keeps its sealed LAN session and uses Reticulum only where
the announce content is acceptable.

## Membership proof

The current LAN model proves the room key and then treats every member as equal;
Reticulum proves an identity and asks a person to Allow. `RnsSession` remembers
an allowed identity but does **not** persist it; the application must
`set_allowed`. Hormiga's members registry would need the Reticulum identity
field (H9). A short authentication string is also absent from `RnsSession`; the
LAN path has one. If the SAS is kept, it must be layered above Reticulum or
accepted as unnecessary because Reticulum proves the identity.

## What does not change

- The share filter and private tags remain Hormiga's, whichever transport
  carries frames.
- The Antfarm remains excluded from member sync except for its credential-naming
  nodes.
- Files are still fetched by content address, and a file a rune names never
  leaves if the rune may not leave.
- At-rest encryption stays Hormiga's for now (Q88); Reticulum seals what
  travels, not what rests.

# Mobile and iOS

- **Android permissions are already complete** for LAN and radios:
  `INTERNET`, `ACCESS_NETWORK_STATE`, `ACCESS_WIFI_STATE`,
  `CHANGE_WIFI_MULTICAST_STATE`, the legacy Bluetooth pair capped at API 30, the
  three `BLUETOOTH_*` permissions, `NEARBY_WIFI_DEVICES`,
  `CHANGE_WIFI_STATE`, and optional LE / Wi-Fi Direct features. See
  [`AndroidManifest.xml`](../../android/AndroidManifest.xml:14).
- **The multicast lock** is installed by the Android shell and held while the
  LAN beacon runs ([`main_android.cpp`](../../android/src/main_android.cpp:225)).
- **The radio platform** is installed by the shell; a portable `maiz::App` would
  have to install it itself.
- **iOS is not built.** Reticulum's UDP broadcast, and therefore `LanSession`'s
  beacon, needs an Apple-granted entitlement. Discovery must become Bonjour or a
  known address (Hammer Q7). Wi-Fi Direct does not exist on iOS; LE is the
  shared radio; the iOS half of the radio holiday is not written. The portable
  shell is the prerequisite for any iOS work, and it removes `curl` and
  `system()`.

# What was not measured

- Two real phones, or two real machines over Reticulum.
- An iOS build of any kind.
- Void Hammer's suite in a standalone build; the five tests ran in Hormiga's
  build.
- Void Maiz's full suite in this checkout.
- Void Chisme and Void Verguenza build and test runs.
- A two-machine radio or Wi-Fi Direct session.

Every claim above that depends on one of these is marked as read, not measured.

# Recommended sequence

1. **H1 + H2 + H3 + H13** — align names, build instructions and documentation.
   Measure the build and the five networking tests after each step.
2. **H4** — adopt `voidmaiz/nav.hpp` in its own change.
3. **H8 + H9 + H10** — decide the transport. The lean is to keep the sealed LAN
   session until Reticulum announces are opaque and the members registry can
   prove a room or a known identity. If approved, ask Void Hammer for the
   announce change before writing the migration.
4. **H5 + H6** — the update client and the general HTTP seam, with the consent
   migration.
5. **H7** — the portable app shell and iOS, as its own project.
6. **H11 + H12** — Chisme's observer role and Verguenza's client seam when their
   own phases exist.

# Decisions for the author

1. **Transport:** keep the sealed LAN session as the default until Q98 is
   answered, or accept opaque announces as a prerequisite and move to
   `RnsSession`? *Lean: keep the seal; make opaque announces the gate.*
2. **Navigation:** adopt `voidmaiz/nav.hpp` now? *Lean: yes, separately.*
3. **Update client:** migrate to `voidmaiz_update` now or after the transport?
   *Lean: after, with the consent file carried over.*
4. **CI:** what should green mean while Void Hammer has no public repository?
   *Lean: an explicit “networking not built” warning and a documented gap, never
   a silent green.*
5. **Portable shell:** when does the one-`maiz::App` migration start? *Lean:
   after the networking update, and only when iOS work begins.*
