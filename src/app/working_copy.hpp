/* app/working_copy.hpp — the working copy is a cache of the .miga
 * (Void Rancho's authority, ../VoidRancho/okf/concepts/authority.md;
 * okf/concepts/platform/miga-format.md "The authority").
 *
 * The author, 2026-11-18: *"they should always see and deal with the miga
 * file, not the json too much ... If for some reason the json file gets
 * deleted, oh well ... the miga file would be almost impossible to replace."*
 *
 * So a database opened from a .miga is worked on in a CACHE of that file, in
 * Rancho's per-user cache folder, never beside the .miga: the state document,
 * the SQLite mirror, a store, unpacked pictures, tiles and a rendered site are
 * all there, and all rebuildable from the .miga. Saving means the .miga.
 * Credentials and backups are KEEPSAKES, per user, outside the cache.
 *
 * `cache` empty: a working copy of its own (`--state <file>`, the phone, the
 * CLI): the older layout, unchanged. */
#pragma once

#include <filesystem>

struct WorkingCopy {
    std::filesystem::path cache;   // the Rancho cache this working copy is; empty: not a cache
    bool ahead = false;            // its note says it holds edits the .miga has not got
    bool filling = false;          // being filled from the .miga at start: init must not seed the demo
    unsigned long edits = 0;       // every model edit, counted
};
