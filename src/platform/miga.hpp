/* miga.hpp — the .miga v3 database BUNDLE (okf/concepts/platform/miga-format.md).
 *
 * A .miga v3 file is the WHOLE database in one portable file: the full core
 * state (every mantle), the irreplaceable local assets (inlined base64), and
 * metadata — packed from / unpacked to the working copy (base_dir). This is
 * NOT the v2 credential vault (that was a secrets-only file, now legacy);
 * v3 supersedes it. Re-derivable data (tiles/site/exports) is NOT bundled —
 * it rebuilds from protocols (the Antfarm graph) after an open.
 *
 * Format (a JSON envelope; large is fine — see the concept):
 *   { "magic":"MIGA", "version":3,
 *     "meta":  { name, created, saved, app },
 *     "state": <full export_state() JSON>,
 *     "assets":{ "assets/x.png": "<base64>", … } }
 *
 * Whole-bundle encryption is the E2EE end state (documented); v3.0 writes the
 * envelope in the clear with the secrets section still handled by the Vault.
 */
#pragma once

#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace hormiga::miga {

/* PROGRESS THROUGH A BUNDLE (2026-10-24, okf/concepts/platform/scale.md).
 * `fraction` in [0, 1], never decreasing, and proportional to the BYTES of
 * work done rather than to the number of files or phases: a bundle is one
 * 40 MB state document and four hundred pictures, and a bar that advanced per
 * file would race through the small ones and stall on the state. `phase` is a
 * short human sentence ("Reading the file", "Restoring pictures"). Called from
 * the thread that called open/pack, never from a worker, so a caller may write
 * plain fields from it; it is cheap to call often. */
using ProgressFn = std::function<void(double fraction, const char* phase)>;

struct PackResult {
    bool ok = false;
    std::string error;
    int assets = 0;      // how many asset files were bundled
    long long bytes = 0; // final file size
    /* The bundled files that were NOT under the assets folder — a
     * `download.file` in `resume/`, a `hero.image` somewhere else. Reported so
     * the operator learns where their originals actually are; before 2026-09-03
     * these were silently absent from every bundle. */
    std::vector<std::string> beyond_assets;
};

struct OpenResult {
    bool ok = false;
    std::string error;
    std::string state;   // the core state document to replay (v3), or "" (legacy)
    int version = 0;     // 3 = full bundle; 2 = legacy secrets-only vault
    int assets = 0;      // assets written
    int assets_kept = 0; // assets already on disk with the same bytes, left alone
    int assets_removed = 0; // files in the assets folder the bundle does not have (mirror)
    std::string name;    // meta.name if present
};

struct OpenOptions {
    ProgressFn progress;
    /* Make the assets folder EXACTLY the bundle's: write what differs, keep what
     * is identical, delete what the bundle does not have. This replaces "delete
     * the folder, then write everything" for a database switch, with the same
     * end state (databases never bleed, author 2026-07-24) and none of the cost
     * when most pictures are already there. */
    bool mirror_assets = false;
};

/* Pack the working copy (state_json + everything under base_dir/assets/, plus
 * every file `extra` names) into
 * a v3 .miga at out_path. `name` is stored in meta. Write-temp-then-rename. */
/* `assets_dir` is where THIS database's originals actually live — normally
 * `base_dir/assets`, but movable since 2026-09-01 (`config set paths.assets`).
 * It is passed rather than assumed so that a relocated assets folder is still
 * the one that gets bundled; a bundler that silently backs up an empty default
 * folder is worse than one that refuses. Entries are still keyed `assets/<rel>`
 * inside the envelope, so bundles written before the move still open. */
/* `skip` (2026-09-16): bundle keys to leave out -- a LAN share omits pictures
 * the Antfarm already hosts online, because the joining device downloads them
 * (lan-sharing.md §3). Empty for every ordinary save. */
PackResult pack(const std::string& state_json, const std::filesystem::path& base_dir,
                const std::filesystem::path& out_path, const std::string& name,
                const std::filesystem::path& assets_dir,
                const std::map<std::string, std::filesystem::path>& extra = {},
                const std::set<std::string>& skip = {},
                const ProgressFn& progress = {});

/* Open a .miga: read it, and for a v3 bundle EXTRACT its assets into
 * base_dir/assets/ and return its state document. A v2 legacy file is detected
 * (version=2, no state) so the caller can migrate.
 *
 * Touches nothing but the filesystem, so it may run on a worker thread while
 * the window keeps drawing (issue #1). */
OpenResult open(const std::filesystem::path& miga_path,
                const std::filesystem::path& base_dir,
                const std::filesystem::path& assets_dir,
                const OpenOptions& options = {});
inline OpenResult open(const std::filesystem::path& miga_path,
                       const std::filesystem::path& base_dir,
                       const std::filesystem::path& assets_dir, const ProgressFn& progress) {
    OpenOptions o;
    o.progress = progress;
    return open(miga_path, base_dir, assets_dir, o);
}

/* True if the file looks like any .miga (v2 or v3) — cheap magic/JSON peek. */
bool looks_like_miga(const std::filesystem::path& p);

} // namespace hormiga::miga
