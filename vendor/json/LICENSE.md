# nlohmann/json — vendored single header

- **What**: `json.hpp` v3.12.0, downloaded 2026-07-19 from
  https://github.com/nlohmann/json (releases).
- **License**: MIT (SPDX header in the file).
- **Why vendored**: JSON parsing for Import holidays (the Supabase rescue
  dump, future Sheets/CSV variants) and the inja template engine to come in
  phase D (which builds on this library). Vendor-don't-depend (CLAUDE.md
  rule 5).
- **Local changes**: none.
