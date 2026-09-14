# Third-party notices

Void Hormiga is MIT licensed (see [LICENSE](LICENSE)). It **vendors** its
dependencies rather than fetching them — no package manager, no CDN — so every
third-party component ships in this repository with its own license beside the
code it covers. This file is an index; those license texts are authoritative.

| component | where | license |
|---|---|---|
| SQLite (amalgamation) | `vendor/sqlite/` | public domain — `LICENSE.md` |
| libsodium | `vendor/libsodium/` | ISC — `LICENSE` |
| nlohmann/json | `vendor/json/` | MIT — `LICENSE.md` |
| stb single-file libraries | `vendor/stb/` | MIT / public domain — `LICENSE.md` |
| BLAKE3 | `vendor/blake3/` | CC0 — `LICENSE-CC0.txt` |
| Lucide icons | `vendor/icons/` | ISC — `LICENSE-lucide.txt` |
| QR Code generator (Project Nayuki) | `vendor/qrcodegen/` | MIT — `LICENSE.txt` |
| Inter, Source Serif, Space Grotesk, Lato, JetBrains Mono | `vendor/fonts/` | SIL Open Font License — `OFL-*.txt` |
| Font Awesome Free | `vendor/fonts/` | `LICENSE.md` |

Void Core, Void Maiz, Void Allomone and Void Palabra are sibling projects, not
vendored copies — they are built from their own repositories beside this one.
See [`void.json`](void.json).

## A note on the fonts

The bundled typefaces are under the SIL Open Font License, which is permissive
but not unconditional: the font files may be bundled with and sold as part of
software, but may **not** be sold on their own, and a modified version may not
reuse a Reserved Font Name. That condition travels with anyone redistributing
this repository, and no license choice made here can change it.

## demo-assets/

The Cat Colony photographs come from [cataas.com](https://cataas.com). The
colony itself is fictional — fifty invented cats with invented biographies —
and it ships so the application has a real dataset to open on first run, and so
that no example anywhere in this repository is ever a real person.
