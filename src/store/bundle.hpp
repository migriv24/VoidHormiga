/* store/bundle.hpp — the .miga v4 file: the store itself, with the pictures
 * (okf/concepts/platform/store.md; the author's choice, 2026-10-27).
 *
 * A v4 .miga IS a SQLite database: the store's tables, plus `assets` (each
 * picture's path, bytes and digest) and `store_meta` saying it is a bundle.
 * Opening one is copying a file, at any size, in constant memory; the
 * database inside is already indexed. Packing one is SQLite's `VACUUM INTO`
 * (a consistent, compacted copy taken without stopping the store) and then the
 * pictures. A v3 .miga (one JSON document) is told apart by its first bytes
 * and still opens, by streaming it into a store (Store::import).
 *
 * Soft-deleted rows are not packed: a bundle is what the database IS, and no
 * undo crosses a file. */
#pragma once

#include "store/store.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace hormiga::store {

enum class BundleKind { Missing, V4, V3, Other };

/* What a file is, from its first bytes. */
BundleKind bundle_kind(const std::filesystem::path& file);

struct BundleResult {
    bool ok = false;
    std::string error;
    long long assets = 0, asset_bytes = 0, runes = 0;
    long long refused = 0; // files whose path would have landed outside the working copy
    std::string name; // the bundle's name (v4 meta, or a v3 envelope's meta.name)
};

/* Pack the store at `store`, every file under `assets_dir` (stored as
 * "assets/<relative path>"), and each of `extra` (paths relative to `base_dir`
 * of files a rune names outside the assets folder) into a v4 bundle at `out`.
 * Written to a temporary file and renamed, so a failed pack leaves `out` as it
 * was. */
BundleResult pack_v4(Store& store, const std::filesystem::path& base_dir, const std::filesystem::path& assets_dir,
                     const std::vector<std::string>& extra, const std::filesystem::path& out,
                     const std::string& name, const Progress& progress = {});

/* Open the bundle at `in` (v4 or v3) as the working store at `store_path`
 * (replaced). Its pictures go under `assets_dir` and its other files under
 * `base_dir`. A path that would land outside them — absolute, or climbing with
 * ".." — is refused and counted, never written: a bundle is a file from
 * somewhere else. */
BundleResult open_bundle(const std::filesystem::path& in, const std::filesystem::path& store_path,
                         const std::filesystem::path& base_dir, const std::filesystem::path& assets_dir,
                         const Progress& progress = {});

} // namespace hormiga::store
