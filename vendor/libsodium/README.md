# libsodium — vendored (built from source)

- **What**: `lib/libsodium.a` (static) + `include/sodium.h` + `include/sodium/`,
  the one crypto dependency (okf/concepts/security.md — libsodium only, never
  hand-roll). ISC license (`LICENSE`).
- **Version**: libsodium 1.0.20-stable, downloaded 2026-07-20 from
  https://download.libsodium.org/libsodium/releases/libsodium-1.0.20-stable.tar.gz
- **Built with the app's own toolchain** (MSYS2 UCRT64 gcc), so no ABI
  mismatch with the ucrt runtime:
  ```
  ./configure --disable-shared --enable-static --disable-dependency-tracking
  make && make install
  ```
  (run inside the MSYS2 UCRT64 login shell). Verified: argon2id + AEAD
  round-trip links and runs.
- **Why the .a is committed, not the source**: the source tree is thousands
  of files; the built static lib is 596 KB. This is the "vendored prebuilt
  per-platform" path (DESIGN.md §9). Other platforms (macOS/Linux CI) rebuild
  from the same tarball with the same commands — this is Windows/UCRT64.
- **Used by**: `src/vault.{hpp,cpp}` — the passphrase-locked `.miga` v2
  credential store (argon2id KDF + XChaCha20-Poly1305 AEAD).
