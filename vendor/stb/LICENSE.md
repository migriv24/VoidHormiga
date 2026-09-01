# stb_image — vendored single-file library

- **What**: `stb_image.h` v2.30 (image decoder: PNG/JPEG/GIF/BMP/…) and
  `stb_image_write.h` v1.16 (PNG/JPEG writer — demo-asset generation, the
  compositor to come), downloaded 2026-07-19 from
  https://github.com/nothings/stb (master).
- **License**: dual public domain / MIT, at our option (see the header's
  license block). We take the public-domain option.
- **Why vendored**: image decoding for thumbnails, canvas faces, and the
  compositor to come (DESIGN.md §3 — "stb_image + stb_image_write class
  single-file libs"). Vendor-don't-depend (CLAUDE.md rule 5).
- **Local changes**: none. `STB_IMAGE_IMPLEMENTATION` is defined once, in
  the desktop shell (`src/main_desktop.cpp`).
