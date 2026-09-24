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

## Android (arm64-v8a) — added 2026-09-24

- **What**: `lib/android-arm64-v8a/libsodium.a`, for Hormiga's APK
  (`android/`). The headers in `include/` are shared: same version.
- **Source**: `libsodium-1.0.20-stable.tar.gz` from the URL above, downloaded
  2026-09-24. The stable tarball is re-cut from time to time: this one is signed
  `timestamp:1767711627` (2026-01-06), SHA-256
  `b6b1d2a8802cd8bfa611638c0e9ce31d14ef324e16062c39a76854091cba6d7f`.
  **Its minisign signature was verified** against libsodium's published key
  (`RWQf6LRCGA9i53mlYecO4IzT51TGPpvWucNSCh1CBM0QTaLn73Y7GFO3`) with
  `android/verify_minisig.py`, which needs only Python's `cryptography`.
- **Built without autotools** (it needs a POSIX shell the Windows NDK does not
  have): `android/CMakeLists.txt` compiles every source file with the defines
  libsodium's own `build.zig` uses for Linux, less `HAVE_PTHREAD_PRIO_INHERIT`,
  which Android's API 26 lacks. Unpack the tarball beside it as
  `libsodium-stable/`, copy `include/sodium/version.h` into its
  `src/libsodium/include/sodium/`, and configure with the NDK toolchain
  (arm64-v8a, android-26). `llvm-strip --strip-debug` takes the library from
  3.0 MB to 678 KB. SHA-256 of the committed `.a`:
  `6f85add8ce72a62b05f0c8d6a5fd075277acf69b88f047dd19d8dd07d6eb32a8`.
- **Verified**: `android/smoke.c` (argon2id, then an XChaCha20-Poly1305 round
  trip: what the vault does) links for arm64. **It has not run on a phone**:
  no device was attached when it was built.
