cmake_minimum_required(VERSION 3.20)
project(voidhormiga CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Void Maiz (which itself locates Void Core via its VOIDCORE_ROOT).
set(VOIDMAIZ_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidMaiz" CACHE PATH
    "Path to the Void Maiz repo")
add_subdirectory("${VOIDMAIZ_ROOT}" voidmaiz)

# ── VOID PALABRA — the merge (Q40, answered by the author 2026-08-27) ───────
#
# okf/concepts/platform/collaboration.md §2. We adopt the MERGE and not (yet)
# the save path: `enrich` / `join` / `conflicts` / `flatten` / `version_name`,
# behind one thin adapter in src/sync/. A join over bare Core state is
# impossible — two peers holding {a} and {} cannot tell "never had it" from
# "removed it" — which is why this is a library and not fifty lines here.
#
# COMPILED FROM THEIR SOURCES RATHER THAN `add_subdirectory`, and the reason is
# specific rather than stylistic: their CMakeLists calls `enable_testing()` and
# registers nine `add_test`s unguarded, two of which use `${CMAKE_SOURCE_DIR}`
# for a working directory. Included as a subdirectory, that resolves to OUR
# source root, so their conformance vectors and OKF checker would run against
# this repo and fail — a sibling library's suite failing our build for a reason
# that has nothing to do with us. Compiling the sources is the same thing Void
# Maiz already does with Void Core's vendored cJSON, so the pattern is the
# stack's own.
#
# THEIR cJSON IS DELIBERATELY OMITTED. Palabra's README: "A consumer that
# already vendors cJSON should link its own copy and drop ours" — Void Maiz's is
# already in every one of our links, and their public header only
# forward-declares the type, so the two never meet at the interface. One cJSON
# in the binary, and no archive-ordering luck involved.
#
# Reported upstream: MESSAGE_FOR_VOIDPALABRA_hormiga-consuming-the-merge-2026-08-27.md
set(VOIDPALABRA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../VoidPalabra" CACHE PATH
    "Path to the Void Palabra repo")
add_library(voidpalabra STATIC
  ${VOIDPALABRA_ROOT}/src/encoding/encode.cpp
  ${VOIDPALABRA_ROOT}/src/encoding/decode.cpp
  ${VOIDPALABRA_ROOT}/src/encoding/sha256.cpp
  ${VOIDPALABRA_ROOT}/src/canonical/bindings.cpp
  ${VOIDPALABRA_ROOT}/src/crdt/orset.cpp
  ${VOIDPALABRA_ROOT}/src/crdt/policy.cpp
  ${VOIDPALABRA_ROOT}/src/crdt/document.cpp
  ${VOIDPALABRA_ROOT}/src/crdt/conflict.cpp
  ${VOIDPALABRA_ROOT}/src/crdt/sequence.cpp
  ${VOIDPALABRA_ROOT}/src/store/chunk.cpp
  ${VOIDPALABRA_ROOT}/src/store/blocks.cpp
  ${VOIDPALABRA_ROOT}/src/store/container.cpp
  ${VOIDPALABRA_ROOT}/src/utterance/journal.cpp
  ${VOIDPALABRA_ROOT}/src/utterance/utterance.cpp
  ${VOIDPALABRA_ROOT}/src/utterance/history.cpp
  ${VOIDPALABRA_ROOT}/src/archive/archive.cpp
  ${VOIDPALABRA_ROOT}/src/archive/miga.cpp)
target_include_directories(voidpalabra
  PUBLIC  ${VOIDPALABRA_ROOT}/include ${VOIDMAIZ_ROOT}/vendor
  PRIVATE ${VOIDPALABRA_ROOT}/src ${VOIDPALABRA_ROOT}/src/encoding
          ${VOIDPALABRA_ROOT}/src/crdt ${VOIDPALABRA_ROOT}/src/store
          ${VOIDPALABRA_ROOT}/src/utterance)
target_compile_definitions(voidpalabra PRIVATE CJSON_HIDE_SYMBOLS)
# Their tree, their warnings: we do not police a sibling's source, and turning
# -Wall -Wextra on someone else's code produces noise we cannot act on without
# breaking ground rule 4.
add_library(hormiga_sync STATIC src/sync/merge.cpp src/sync/peer.cpp)
target_include_directories(hormiga_sync PUBLIC src PRIVATE ${VOIDMAIZ_ROOT}/vendor)
target_link_libraries(hormiga_sync PUBLIC voidpalabra sodium)
if(WIN32)
  target_link_libraries(hormiga_sync PUBLIC ws2_32) # the LAN beacon + stream
endif()
if(NOT MSVC)
  target_compile_options(hormiga_sync PRIVATE -Wall -Wextra)
endif()

# Vendored SQLite (public domain — vendor/sqlite/LICENSE.md): the default
# Data holiday, compiled as one C file into a static lib.
add_library(sqlite3 STATIC vendor/sqlite/sqlite3.c)
target_include_directories(sqlite3 PUBLIC vendor/sqlite)
target_compile_definitions(sqlite3 PRIVATE
  SQLITE_OMIT_LOAD_EXTENSION # no runtime extension loading (and no libdl)
  SQLITE_DQS=0               # reject double-quoted string literals (hygiene)
)

# Vendored libsodium (ISC — vendor/libsodium/README.md): the one crypto
# dependency, built from source with the ucrt64 toolchain (no hand-rolling).
add_library(sodium STATIC IMPORTED)
set_target_properties(sodium PROPERTIES
  IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libsodium/lib/libsodium.a")
target_include_directories(sodium INTERFACE vendor/libsodium/include)
# static libsodium requires SODIUM_STATIC/SODIUM_EXPORT=empty so headers don't
# emit dllimport decorations
target_compile_definitions(sodium INTERFACE SODIUM_STATIC=1)

# Hormiga's DOMAIN Allomone (src/domain/hormiga_allomone.hpp): the small library the
# Void Maiz host protocol asks every adopter to supply — subjects, properties +
# merge laws, domain predicates, the glyphs our storage and upstream's commands
# name. UI-free and view-free on purpose: it links `voidmaiz_allomone` only, so
# the whole derivation is provable headless (tests/allomone_smoke.cpp).
add_library(hormiga_allomone STATIC src/domain/hormiga_allomone.cpp)
target_include_directories(hormiga_allomone PUBLIC src)
target_link_libraries(hormiga_allomone PUBLIC voidmaiz_allomone)
if(NOT MSVC)
  target_compile_options(hormiga_allomone PRIVATE -Wall -Wextra)
endif()

# The app: platform-free app.cpp + the GLFW desktop shell.
# ── WEB ASSETS: real files, embedded at build time ──────────────────────────
#
# `style.css` and `app.js` used to be 816 lines of CSS and JavaScript inside C++
# raw-string literals — no highlighting, no linter, no formatter, and escaping
# hazards in two languages that need tools to get right. They are now real files
# under `src/render/web/`, generated into headers here.
#
# THE BINARY IS EXACTLY AS SELF-CONTAINED AS BEFORE. Nothing is read at runtime
# and nothing is fetched from a network; the generated header is compiled in.
# The only thing that changed is which program can understand the source.
set(HORMIGA_GEN ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${HORMIGA_GEN})
find_package(Python3 COMPONENTS Interpreter QUIET)
if(NOT Python3_Interpreter_FOUND)
  message(FATAL_ERROR "Python 3 is required to embed src/render/web assets")
endif()
function(hormiga_embed_asset SRC OUT SYMBOL)
  add_custom_command(
    OUTPUT ${HORMIGA_GEN}/${OUT}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_asset.py
            ${CMAKE_CURRENT_SOURCE_DIR}/${SRC} ${HORMIGA_GEN}/${OUT} ${SYMBOL}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${SRC}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_asset.py
    COMMENT "Embedding ${SRC}"
    VERBATIM)
endfunction()
hormiga_embed_asset(src/render/web/style.css site_css_data.hpp kSiteCssStatic)
hormiga_embed_asset(src/render/web/app.js    site_js_data.hpp  kSiteJs)
add_custom_target(hormiga_web_assets
  DEPENDS ${HORMIGA_GEN}/site_css_data.hpp ${HORMIGA_GEN}/site_js_data.hpp)

# app.cpp is the SHELL (lifecycle, host seams, projection, persistence, frame);
# each section is its own translation unit (Q30a, 2026-08-17 — src/app/app_internal.hpp
# explains the boundary). They share one HormigaApp struct declared in app.hpp:
# the split is of TEXT, not of design.
#
# `src/publish/publish.cpp src/publish/cloudflare.cpp src/publish/aws.cpp src/publish/push.cpp src/publish/panel.cpp` IS IN BOTH TARGETS ON PURPOSE, and the bug it fixes is the
# reason to keep it that way: `deploy_site` used to live in `headless_main.cpp`,
# which only the CLI compiles, so the desktop application could not publish a
# website at all — not "had no button", had no code. The person who owns the
# site had to ask an agent to run a command. Founding commitment 1 says the GUI,
# the command bar and an agent are three callers of ONE set of verbs; a verb
# that reaches one of them is a broken surface.
add_executable(voidhormiga src/app/app.cpp src/app/app_shared.cpp src/app/paths.cpp
  src/ui/data.cpp src/render/site.cpp
    src/render/index.cpp src/render/email.cpp
  src/render/theme.cpp src/render/assets.cpp src/ui/allomone.cpp
  src/ui/builder.cpp src/ui/map.cpp src/ui/calendar.cpp
  src/ui/style.cpp src/ui/settings.cpp src/ui/widgets.cpp
  src/app/verbs.cpp src/app/sync_ops.cpp src/render/map_png.cpp
  src/publish/publish.cpp src/publish/cloudflare.cpp src/publish/aws.cpp src/publish/push.cpp src/publish/panel.cpp
  src/platform/storage.cpp src/platform/vault.cpp src/platform/backup.cpp
  src/platform/miga.cpp src/platform/preview_server.cpp src/main/desktop.cpp)
# Void Core's C ABI header, for the SPEC 6.1 command codec exported in 0.2.7
# (vc_arg_quote / vc_argv_split_json / vc_transcript_split_json). The library
# itself already arrives transitively through Void Maiz; only the header is new.
# We call these rather than reimplementing 6.1, which is the entire point of
# their shipping it as code -- five implementations of that section have been
# wrong, including ours and including the reference core's own script reader.
target_include_directories(voidhormiga PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore/core/include)
target_include_directories(voidhormiga PRIVATE src vendor/stb vendor/json vendor/fonts
  vendor/icons vendor/blake3 ${HORMIGA_GEN})
add_dependencies(voidhormiga hormiga_web_assets)
target_link_libraries(voidhormiga PRIVATE
  voidmaiz_view # canvas, inspector, widgets, faces (brings voidmaiz + imgui)
  voidmaiz_headless # Session, for the Human floor claim (a person outranks an agent)
  hormiga_allomone # the rules engine (brings voidmaiz_allomone)
  sqlite3
  sodium
  hormiga_sync # the merge (Void Palabra) + the LAN transport
  ws2_32 # the live-preview localhost server (B2; winsock)
)
if(WIN32)
  target_link_libraries(voidhormiga PRIVATE comdlg32) # the OS open-file dialog
endif()
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(voidhormiga PRIVATE Threads::Threads) # background jobs
if(NOT MSVC)
  target_compile_options(voidhormiga PRIVATE -Wall -Wextra)
  # self-contained binaries: no libstdc++/libgcc DLL hunt at load time (a
  # stale runtime on PATH = 0xc0000139 — diagnosed 2026-07-19, okf/log.md)
  target_link_options(voidhormiga PRIVATE -static-libstdc++ -static-libgcc)
endif()

# ── the HEADLESS front-end (Void Maiz's voidmaiz_headless, adopted 2026-08-18) ──
#
# The same application with no window attached, so an agent in a real terminal —
# anywhere, in any language — can update the database and build a newsletter, and
# a person reviews the result when they next open the GUI. There is no sync
# because there is nothing to sync: both front-ends are over ONE state document.
#
# It links the section units and `voidmaiz_view` even though it opens no window,
# because `render_preview`/`render_site` are `HormigaApp::` methods and the struct
# is one struct. The Output domain itself is view-free (section_web.cpp has zero
# ImGui references), so nothing here ever needs a context — the cost is binary
# size, and the alternative was a second renderer.
add_executable(voidhormiga-cli src/main/headless.cpp src/platform/stb_impl.cpp
  src/app/app.cpp src/app/app_shared.cpp src/app/paths.cpp
  src/ui/data.cpp src/render/site.cpp
    src/render/index.cpp src/render/email.cpp
  src/render/theme.cpp src/render/assets.cpp src/ui/allomone.cpp
  src/ui/builder.cpp src/ui/map.cpp src/ui/calendar.cpp
  src/ui/style.cpp src/ui/settings.cpp src/ui/widgets.cpp
  src/app/verbs.cpp src/app/sync_ops.cpp src/render/map_png.cpp
  src/publish/publish.cpp src/publish/cloudflare.cpp src/publish/aws.cpp src/publish/push.cpp src/publish/panel.cpp
  src/platform/storage.cpp src/platform/vault.cpp src/platform/backup.cpp
  src/platform/miga.cpp src/platform/preview_server.cpp)
target_include_directories(voidhormiga-cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore/core/include)
target_include_directories(voidhormiga-cli PRIVATE src vendor/stb vendor/json vendor/fonts
  vendor/icons vendor/blake3 ${HORMIGA_GEN})

# ── STAGE VOID CORE'S RUNTIME DLL BESIDE OUR BINARIES ──────────────────────
#
# Nothing did this. `build/bin/libvoidcore.dll` was a manual copy from July, and
# it went stale the moment Core shipped 0.2.7 — so linking against the new
# import library produced binaries that failed to START (exit 127, no output,
# no message, because the loader gives none). That is a maddening failure to
# diagnose: the build is green and the program does nothing at all.
#
# We link Core through Void Maiz, and Maiz stages the DLL only next to its own
# smoke test. Staging a dependency's runtime artifact beside OUR binary is our
# job, so it happens here, every build, from wherever Core actually built it.
find_file(HORMIGA_VOIDCORE_DLL NAMES libvoidcore.dll voidcore.dll
          PATHS "${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore/core/build/bin"
                "${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore/core/build"
          NO_DEFAULT_PATH)
if(WIN32 AND HORMIGA_VOIDCORE_DLL)
  foreach(t voidhormiga voidhormiga-cli)
    add_custom_command(TARGET ${t} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${HORMIGA_VOIDCORE_DLL}" "$<TARGET_FILE_DIR:${t}>"
      COMMENT "Staging Void Core's DLL beside ${t}")
  endforeach()
elseif(WIN32)
  message(WARNING "libvoidcore.dll not found - binaries will not start. "
                  "Build Void Core first.")
endif()

add_dependencies(voidhormiga-cli hormiga_web_assets)
target_link_libraries(voidhormiga-cli PRIVATE
  voidmaiz_headless # session, lock, journal, effect gate, briefing
  voidmaiz_view     # HormigaApp's other methods live here; never called headless
  hormiga_allomone sqlite3 sodium hormiga_sync ws2_32 Threads::Threads)
if(WIN32)
  target_link_libraries(voidhormiga-cli PRIVATE comdlg32)
endif()
# What an agent needs, staged BESIDE THE BINARY. A real caller runs this in the
# folder holding somebody's database — not a checkout — so the guide and the OKF
# bundle have to travel with the executable or they are dead pointers. (The
# briefing's `okf_root` was exactly that on first contact.)
#
# A TARGET, NOT A POST_BUILD, and that is a lesson borrowed from upstream's
# mistake this week: Void Maiz staged their runtime DLLs on one target's
# POST_BUILD, so a consumer who built a different target got none of them and
# their fix looked broken. A POST_BUILD also only fires when the binary
# RELINKS — so editing AGENT-GUIDE.md alone would leave a stale copy beside the
# binary, which is the worst possible failure for a file whose whole job is to
# be current. `ALL` plus `copy_if_different` runs every build and costs nothing.
#
# THE WEBFONTS TRAVEL TOO (2026-08-19). `render_site` staged them from
# `current_path()/vendor/fonts/web` — the DATA folder, which for a real caller
# is somebody's org folder and has no `vendor/`. The guard stepped over it in
# silence and every headless deploy shipped an empty `site/fonts/` behind
# `@font-face` rules pointing at nothing: "the deployed site carries its own
# fonts, no CDN" broken invisibly, into a fallback that looks fine on the
# developer's machine because there the two paths are the same folder. The
# renderer now resolves them from `ship_dir`; this is what puts them there.
add_custom_target(hormiga_agent_files ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_CURRENT_SOURCE_DIR}/AGENT-GUIDE.md ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/okf ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/okf
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/vendor/fonts/web
          ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/fonts/web
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/AGENT-GUIDE.md
  COMMENT "Staging AGENT-GUIDE.md, the OKF bundle and the webfonts beside voidhormiga-cli")
add_dependencies(voidhormiga-cli hormiga_agent_files)
add_dependencies(voidhormiga hormiga_agent_files) # same renderer, same fonts

if(NOT MSVC)
  target_compile_options(voidhormiga-cli PRIVATE -Wall -Wextra)
  target_link_options(voidhormiga-cli PRIVATE -static-libstdc++ -static-libgcc)
endif()

enable_testing()

# The headless front-end, end to end (tests/headless_smoke.sh). Drives the real
# BINARY, because what broke during adoption was the argument surface — the
# state-file default, the effect gate, the exit codes — none of which a C++ test
# linking the same objects would touch.
# ── THE TOOLS RUN IN CI, which is the only thing that keeps them honest ─────
#
# A linter nobody runs is a comment. Four of these exist because a bug shipped
# that they would have caught, so they run beside the tests: a structural
# regression fails the build the same way a behavioural one does.
#
# `--strict` turns a report into an exit code. `find_long` is a ratchet — the
# budgets in it are the current worst cases and only ever go down.
if(Python3_Interpreter_FOUND)
  add_test(NAME hormiga_layering
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_layering.py
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME hormiga_glyph_fields
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/lint_glyph_fields.py --strict
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME hormiga_i18n
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/lint_i18n.py --strict
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  # The NFC precondition is the CALLER's to hold (Void Palabra SPEC 6.1,
  # normative 2026-08-21, at Hormiga's own proposal). With no argument this
  # checks the working database if one is present and passes trivially if not,
  # so a fresh checkout is not failed for having no data. Not --strict here:
  # failing somebody's BUILD over the contents of their organization's database
  # is the wrong lever. `python tools/lint_nfc.py --strict <doc>` is the lever.
  add_test(NAME hormiga_nfc
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/lint_nfc.py
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME hormiga_file_length
           COMMAND ${Python3_EXECUTABLE}
                   ${CMAKE_CURRENT_SOURCE_DIR}/tools/find_long.py --strict
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

find_program(BASH_EXE NAMES bash)
if(BASH_EXE)
  add_test(NAME hormiga_golden_render
           COMMAND ${BASH_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/golden_render.sh
                   $<TARGET_FILE:voidhormiga-cli> check)
  add_test(NAME hormiga_headless_smoke
           COMMAND ${BASH_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/headless_smoke.sh
                   $<TARGET_FILE:voidhormiga-cli>)
endif()

# The phase B exit test: the demo-org transcript replays headless to an
# identical projected scene (UI-free — links only the embed layer).
add_executable(hormiga_replay_smoke tests/replay_smoke.cpp)
target_link_libraries(hormiga_replay_smoke PRIVATE hormiga_allomone) # seed.hpp registers the allo glyphs
target_include_directories(hormiga_replay_smoke PRIVATE src vendor/json) # seed.hpp
add_test(NAME hormiga_replay_smoke COMMAND hormiga_replay_smoke)

# The phase C spine test: SQLite round-trip (save → reload → identical
# projection) + the CSV import compiler landing as one replayable batch.
add_executable(hormiga_spine_smoke tests/spine_smoke.cpp src/platform/storage.cpp)
target_link_libraries(hormiga_spine_smoke PRIVATE hormiga_allomone sqlite3)
target_include_directories(hormiga_spine_smoke PRIVATE src vendor/json)
add_test(NAME hormiga_spine_smoke COMMAND hormiga_spine_smoke)

# The vault crypto test: create → set → save → reload → unlock (right pass
# recovers the secret; wrong pass fails; tamper fails) — headless.
add_executable(hormiga_vault_smoke tests/vault_smoke.cpp src/platform/vault.cpp)

# The ENCRYPTED BACKUP: a whole `.miga` as a blob a store cannot read
# (okf/concepts/data-planes.md §3). Crypto that guards the organization's entire
# database, so the suite is written against the ways it can fail QUIETLY -- a
# wrong passphrase that returns garbage, a tampered file that opens anyway, a
# truncated file that restores a plausible prefix. The last one is why this uses
# libsodium's secretstream rather than a single-shot seal.
# THE SUBMISSION GATE: what stands between a stranger's text and the database.
# Every case is a real injection that worked against Void Core 0.2.6, or the
# property that stops it. Two of the three routes were found by Core, not by us,
# which is the argument for the gate calling THEIR decoder rather than a parser
# we wrote -- we did not think of routes 2 and 3.
# AWS SIGNATURE V4: the piece every AWS capability bottoms out in -- the index
# push, the encrypted backup upload, and later a visitor's file all reduce to
# one signed request. Built and tested BEFORE anything that needs an account,
# because the primitives are checkable against published vectors and the order
# is checkable against the spec; only "does AWS agree" needs a credential, and
# that is the operator's first call, the same play that found the Cloudflare
# upload-token bug in a minute.
add_executable(hormiga_aws_smoke tests/aws_smoke.cpp src/publish/aws.cpp)
target_link_libraries(hormiga_aws_smoke PRIVATE sodium)
target_include_directories(hormiga_aws_smoke PRIVATE src)
add_test(NAME hormiga_aws_smoke COMMAND hormiga_aws_smoke)

add_executable(hormiga_submission_smoke tests/submission_smoke.cpp)
target_include_directories(hormiga_submission_smoke PRIVATE
  src vendor/json ${CMAKE_CURRENT_SOURCE_DIR}/../VoidCore/core/include)
target_link_libraries(hormiga_submission_smoke PRIVATE voidmaiz_headless)
add_test(NAME hormiga_submission_smoke COMMAND hormiga_submission_smoke)
if(WIN32 AND HORMIGA_VOIDCORE_DLL)
  add_custom_command(TARGET hormiga_submission_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HORMIGA_VOIDCORE_DLL}" "$<TARGET_FILE_DIR:hormiga_submission_smoke>")
endif()

# THE PHASE F SUITE (okf/concepts/platform/data-planes.md §7): the merge, on one
# machine, with no network and no second computer -- "if the merge is wrong,
# that is where it is cheapest to find out." The author's stated fear was
# duplicated data, so most of this suite is about that, and the KNOWN
# limitation (a deletion does not propagate) is pinned as a passing assertion
# rather than left implicit: when somebody fixes it, this test fails and says so.
#
# The socket half runs on loopback in one process. It proves the handshake, the
# key agreement, the SAS agreement and the framing. It does NOT prove two
# machines on one Wi-Fi, and nothing here claims otherwise.
add_executable(hormiga_sync_smoke tests/sync_smoke.cpp)
target_link_libraries(hormiga_sync_smoke PRIVATE hormiga_sync voidmaiz_headless Threads::Threads)
target_include_directories(hormiga_sync_smoke PRIVATE src)
add_test(NAME hormiga_sync_smoke COMMAND hormiga_sync_smoke)
if(WIN32 AND HORMIGA_VOIDCORE_DLL)
  add_custom_command(TARGET hormiga_sync_smoke POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HORMIGA_VOIDCORE_DLL}" "$<TARGET_FILE_DIR:hormiga_sync_smoke>")
endif()

add_executable(hormiga_backup_smoke tests/backup_smoke.cpp src/platform/backup.cpp)
target_link_libraries(hormiga_backup_smoke PRIVATE sodium)
target_include_directories(hormiga_backup_smoke PRIVATE src)
add_test(NAME hormiga_backup_smoke COMMAND hormiga_backup_smoke)
target_include_directories(hormiga_vault_smoke PRIVATE src vendor/libsodium/include)
target_link_libraries(hormiga_vault_smoke PRIVATE sodium)
add_test(NAME hormiga_vault_smoke COMMAND hormiga_vault_smoke)

# The .miga v3 database bundle test: pack → unpack round-trips state + assets.
add_executable(hormiga_miga_smoke tests/miga_smoke.cpp src/platform/miga.cpp)
target_link_libraries(hormiga_miga_smoke PRIVATE sodium)
target_include_directories(hormiga_miga_smoke PRIVATE src vendor/json)
add_test(NAME hormiga_miga_smoke COMMAND hormiga_miga_smoke)

# Void Reyna's PHASE-0 EXIT TEST, run here because the gate is Hormiga's model:
# hand-build one council meeting and see whether it survives. Synthetic people,
# structure modelled on a real Springfield document. Links no view module.
add_executable(hormiga_civic_smoke tests/civic_smoke.cpp)
target_link_libraries(hormiga_civic_smoke PRIVATE hormiga_allomone) # seed.hpp
target_include_directories(hormiga_civic_smoke PRIVATE src vendor/json)
add_test(NAME hormiga_civic_smoke COMMAND hormiga_civic_smoke)

# The SEAM test: a real Void Reyna transcript (produced from Springfield's
# genuine Operating Policies PDF) must replay into a valid civic model. Neither
# repo can check this alone, and a seam nobody tests is a seam that drifts.
add_executable(hormiga_reyna_import_smoke tests/reyna_import_smoke.cpp)
target_include_directories(hormiga_reyna_import_smoke PRIVATE src vendor/json)
target_link_libraries(hormiga_reyna_import_smoke PRIVATE voidmaiz)
target_compile_definitions(hormiga_reyna_import_smoke PRIVATE
  TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
add_test(NAME hormiga_reyna_import_smoke COMMAND hormiga_reyna_import_smoke)

# The Allomone adoption test: the whole Void Maiz loop over HORMIGA's domain,
# headless — glyphs, subjects from a real projection, our merge laws, our
# predicates, two disagreeing scripts producing a ⊤, and settling it through the
# dispatcher. Links no view module, which is the claim worth pinning: the
# derivation is correct without a window, or it is not correct.
add_executable(hormiga_allomone_smoke tests/allomone_smoke.cpp)
target_link_libraries(hormiga_allomone_smoke PRIVATE hormiga_allomone)
target_include_directories(hormiga_allomone_smoke PRIVATE src vendor/json) # seed.hpp
add_test(NAME hormiga_allomone_smoke COMMAND hormiga_allomone_smoke)

# The 2026-08-28 field report's three asks, pinned with `today` held fixed:
# the `date:` predicates in the block grammar (including the two rules a naive
# implementation gets wrong -- a recurring event is not in the past, and a flier
# borrows its event's date through the edge), the video URL parser (every shape
# a volunteer can paste, and every shape that must NOT parse), and the flier
# date-mismatch check. Links no renderer and no view, which is the claim: all
# three are pure functions over a projection.
add_executable(hormiga_dates_smoke tests/dates_smoke.cpp)
target_link_libraries(hormiga_dates_smoke PRIVATE voidmaiz)
target_include_directories(hormiga_dates_smoke PRIVATE src vendor/json)
add_test(NAME hormiga_dates_smoke COMMAND hormiga_dates_smoke)

# The Cat Dataset generator (tools/catgen.cpp): a synthetic, PUBLIC test colony
# built through the real dispatcher, saved as a loadable state snapshot. Not a
# test — a build-time tool. Links only the embed layer, like the replay smoke.
# The vendored BLAKE3 against its own published vectors. A hand port of a hash
# function is exactly the kind of code where "it compiles" means nothing:
# Cloudflare keys every uploaded asset by this digest, and a wrong one does not
# fail loudly — it stores the file under a name the manifest does not reference.
add_executable(hormiga_blake3_smoke tests/blake3_smoke.cpp)
add_test(NAME hormiga_blake3_smoke COMMAND hormiga_blake3_smoke)

# The map ENGINE (src/gis/), and specifically the two settings no caller in the
# tree reaches yet: ProjectionKind::Flat and a non-wrapping world. The golden
# render proves the MOVE changed nothing; it says nothing at all about the
# widening, because the three built-in Earth sources never take those branches.
# The author is building a non-Earth map builder, so leaving its half of the
# abstraction unrun would be exactly the wrong economy.
add_executable(hormiga_gis_smoke tests/gis_smoke.cpp)
target_include_directories(hormiga_gis_smoke PRIVATE src)
add_test(NAME hormiga_gis_smoke COMMAND hormiga_gis_smoke)

add_executable(catgen tools/catgen.cpp)
target_link_libraries(catgen PRIVATE hormiga_allomone)
target_include_directories(catgen PRIVATE src vendor/json) # seed.hpp uses nlohmann

if(NOT MSVC)
  target_link_options(hormiga_replay_smoke PRIVATE -static-libstdc++ -static-libgcc)
  target_link_options(hormiga_spine_smoke PRIVATE -static-libstdc++ -static-libgcc)
  target_link_options(hormiga_vault_smoke PRIVATE -static-libstdc++ -static-libgcc)
  target_link_options(hormiga_miga_smoke PRIVATE -static-libstdc++ -static-libgcc)
  target_link_options(hormiga_allomone_smoke PRIVATE -static-libstdc++ -static-libgcc)
  target_link_options(hormiga_civic_smoke PRIVATE -static-libstdc++ -static-libgcc)
  target_link_options(hormiga_reyna_import_smoke PRIVATE -static-libstdc++ -static-libgcc)
endif()

