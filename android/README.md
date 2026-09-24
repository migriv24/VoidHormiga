# Void Hormiga on Android

The phone front-end (`src/phone/`, [okf/concepts/sections/mobile.md](../okf/concepts/sections/mobile.md))
as an APK: the same `HormigaApp` the desktop runs, in a NativeActivity shell
(`src/main_android.cpp`), built without Gradle. Interaction Combinators'
`android/` build is the template.

**The phone is a member, never the host.** It joins a desktop's shared database
and works on it: Data, Calendar, Together, Me. Publishing, map tiles and
anything else that needs `curl` is not on the phone yet.

## Building

```powershell
powershell -File android\build_apk.ps1            # -> android\VoidHormiga-<version>.apk
powershell -File android\build_apk.ps1 -Install   # and adb install to a connected phone
```

It needs the Android SDK (NDK, a platform, build-tools, CMake) and Android
Studio's bundled JBR. The root `CMakeLists.txt` is configured with the NDK
toolchain, and on `ANDROID` it builds only `libvoidhormiga.so`
(`hormiga_android.cmake`). arm64-v8a only: `vendor/libsodium/lib/android-arm64-v8a/`
is the one Android libsodium built.

## The release key: back it up

The first build minted:

- `android/hormiga-release.keystore`: the key that signs every Hormiga APK
- `android/hormiga-release.keystore.pass`: its password (random)

**Both are gitignored, and both must be backed up somewhere safe, together.**
Android installs an update only when it is signed by the same key as the app
already on the phone. Lose the key, and every phone has to uninstall Hormiga
(and its local data) to get the next version. Commit it, and anyone can sign an
"update". If the script finds the password without the keystore, it stops and
asks for the backup rather than minting a new key.

The certificate's SHA-256 is
`37d0ee2427d8e1714fc73c8494f90d3feaf7cbaed974a20dae63e205d35521f4` (minted
2026-09-24). `apksigner verify --print-certs` on any release must print it.
