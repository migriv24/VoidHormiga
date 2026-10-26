<!-- Thank you. CONTRIBUTING.md explains each item below; a maintainer will help with any you are unsure of. -->

## What this changes, and why

<!-- One paragraph. Link the issue: "Fixes #12". -->

## Checklist

- [ ] **Nothing private.** No real person's or organization's data, no name of a client or organization, no path from my computer, no key. Examples use the Cat Colony, Riverton, `example.org` and `555-01xx`. (`python tools/lint_public.py --strict` passes.)
- [ ] **Every change is a dispatcher command.** A new behaviour can be done from the command line and replays headless.
- [ ] **Tests.** `ctest --test-dir build --output-on-failure -E reduce_conformance` passes, and the change has a test or says why it cannot.
- [ ] **The OKF.** A design change updates the concept page it touches and adds a dated entry to `okf/log.md`.
- [ ] **No sibling edits.** Nothing under `../VoidCore`, `../VoidMaiz`, `../VoidPalabra`, `../VoidAllomone` or `../VoidHammer` is patched. A gap there is described in this PR so a maintainer can take it upstream.
- [ ] **No new dependency** unless it is vendored with its licence and listed in `THIRD-PARTY-NOTICES.md` and `void.json`.
- [ ] I am contributing this under the project's MIT licence.

## How I checked it

<!-- What you ran, on which platform. Paste output only after checking it for names and paths. -->
