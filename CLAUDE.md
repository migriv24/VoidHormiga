# Void Hormiga — agent instructions

You are working on **Void Hormiga**, a native C++20 desktop application for
community-outreach organizations, built on **Void Core** (`../VoidCore`, the
engine — C ABI) and **Void Maiz** (`../VoidMaiz`, the node-graph view library
— we are its client #2).

## Ground rules

1. **OKF first.** `okf/` is the source of truth for design; code follows
   concepts, never the other way. Every session that changes design or code
   captures it in `okf/log.md` and trues up the affected concept docs. Read
   `okf/index.md` before doing anything.
2. **The repo root is a public artifact.** This repo is public from day one.
   No member data, no exports, no test fixtures containing real people, no
   credentials — ever, in any commit. Real org data lives only where the app
   puts it at runtime.
3. **The model lives in Void Core; the dispatcher is the only door.** Every
   change — a contact edit, a block snap, a tag pass, a deploy — is a logged,
   replayable dispatcher command. If a feature can't be expressed as commands
   (and thus replayed headless), its design is wrong.
4. **Upstream messages, not upstream edits.** Void Hormiga *uses* Void Core
   and Void Maiz; it never patches them. Needs and gaps are drafted at the
   repo root for the author to relay, **uniquely titled** (convention set
   2026-07-21 — a bare generic name lets an agent read a stale message as the
   live one): outgoing is
   `MESSAGE_FOR_<RECIPIENT>_hormiga-<topic>-<YYYY-MM-DD>.md`
   (e.g. `MESSAGE_FOR_VOIDMAIZ_hormiga-map-tools-2026-07-22.md`); replies
   arrive as `MESSAGE_FOR_VOIDHORMIGA_<sender>-<topic>-<date>.md`. A consumed
   message is folded into the OKF log and then deleted — the log is the
   durable record, not the message file.
5. **Vendor, don't depend.** Third-party pieces (SQLite amalgamation,
   libsodium, inja, stb-class single-file libs) are vendored with licenses.
   No package managers, no CDNs, no frameworks.
6. **Privacy is enforced at seams.** Internal-notes-class fields never reach
   an Output-interface holiday — checked at the render/export seam, testable.
   Do not add an export path that bypasses that seam. Crypto is libsodium
   only; never hand-roll primitives, never add a fallback secret.
7. **The predecessor is referenced through `DESIGN.md` only.** The old Hormiga
   app (`../Hormiga`) is the behavioral precedent for domain workflows, but
   the OKF describes what Void Hormiga *is*, not what it replaced —
   `DESIGN.md` is the one dated document that references the old project
   freely. It keeps shipping newsletters until roadmap phase D's exit test;
   nothing here may break it (it's a separate repo — just don't plan as if
   it's gone).
8. **The author decides.** Open design decisions go to
   `okf/developer_questions.md` with a lean; answers fold into concepts and
   clear from there.

## Reference implementations

- **Workspace/shell pattern**: `../InteractionCombinators` — the blessed Void
  Maiz host pattern (one window, splitters, pane fractions as config-tier
  view state restored on boot).
- **Block editing**: `../NodeBlocks/src/main.cpp` — the working reference for
  the `"block"` shape kind, snap/tear/heal, face-widget arguments.
- **Query-backed content deployed as a static site**: `../PortfolioManager` —
  the pattern in miniature (runes behind a local holiday, tag-queried,
  GitHub Pages deploy).
- **Void Maiz API surface for the builder**: `../VoidMaiz/include/voidmaiz/`
  — `gesture.hpp`, `face.hpp`, `scene.hpp`. What shipped on our upstream asks
  is recorded in `okf/log.md`; the founding reply itself was consumed and
  deleted per rule 4.
- **The Allomone language**: `../VoidAllomone/okf/concepts/start-here.md`, then
  `host-protocol.md`. It left Void Maiz on 2026-08-29 and is its own project;
  we reach it through Void Maiz's forwarding headers under the `maiz::` names.
