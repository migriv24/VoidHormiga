# android/hormiga_android.cmake — Void Hormiga as an Android NativeActivity library.
#
# Included by the root CMakeLists.txt when configured with the NDK toolchain
# (android/build_apk.ps1 does that). The application is HORMIGA_APP_SOURCES, the
# same list the desktop and the CLI compile; only the entry point differs:
# android/src/main_android.cpp, the EGL + GLES3 + ImGui shell that Interaction
# Combinators proved (its android/ build is the template). Void Maiz, on
# ANDROID, compiles Void Core from source and swaps GLFW for ImGui's android
# backend by itself.
#
# THE PHONE IS A MEMBER, NEVER THE HOST (okf/concepts/sections/mobile.md): the
# library runs the phone front-end (src/phone/), and nothing desktop-only is
# offered on it. What needs `curl` (publishing, tiles, feeds) has no HTTP seam
# on a phone yet, and says so where it is reached.

set(HORMIGA_PLATFORM_TAG "android-arm64")
if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
  message(FATAL_ERROR "Void Hormiga's Android build is arm64-v8a only: "
                      "vendor/libsodium/lib/android-arm64-v8a is the one libsodium built for it")
endif()

add_library(voidhormiga SHARED
  ${HORMIGA_APP_SOURCES}
  src/platform/stb_impl.cpp
  android/src/main_android.cpp
  "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(voidhormiga PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore/core/include
  src vendor/stb vendor/json vendor/fonts vendor/icons vendor/blake3 vendor/qrcodegen ${HORMIGA_GEN}
  "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
target_compile_definitions(voidhormiga PRIVATE
  HORMIGA_VERSION="${PROJECT_VERSION}" HORMIGA_PLATFORM="${HORMIGA_PLATFORM_TAG}")
add_dependencies(voidhormiga hormiga_web_assets)
target_link_libraries(voidhormiga PRIVATE
  voidmaiz_view hormiga_allomone voidmaiz_headless sqlite3 sodium hormiga_sync
  android log EGL GLESv3)
if(TARGET voidmaiz_net)
  target_link_libraries(voidhormiga PRIVATE voidmaiz_net)
  target_compile_definitions(voidhormiga PRIVATE HORMIGA_HAVE_NET=1)
endif()
# the glue's entry point is only referenced by the framework: keep it
target_link_options(voidhormiga PRIVATE -u ANativeActivity_onCreate)
target_compile_options(voidhormiga PRIVATE -Wall -Wextra)
