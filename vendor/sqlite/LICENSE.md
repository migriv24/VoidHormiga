# SQLite — vendored amalgamation

- **What**: `sqlite3.c` / `sqlite3.h`, the SQLite 3.53.3 amalgamation,
  downloaded 2026-07-19 from https://www.sqlite.org/2026/sqlite-amalgamation-3530300.zip
- **License**: SQLite is in the **public domain** (https://www.sqlite.org/copyright.html):
  "Anyone is free to copy, modify, publish, use, compile, sell, or distribute
  the original SQLite code, either in source code form or as a compiled
  binary, for any purpose, commercial or non-commercial, and by any means."
- **Why vendored**: the default Data holiday (okf/concepts/antfarm.md) — one
  C file, in-process, zero sidecar; the vendor-don't-depend policy's ideal
  case (CLAUDE.md ground rule 5).
- **Local changes**: none. Update by replacing both files with a newer
  amalgamation and bumping this note.
