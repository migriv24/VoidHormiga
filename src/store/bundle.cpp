/* store/bundle.cpp — see bundle.hpp. */
#include "store/bundle.hpp"

#include <cstring>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

namespace hormiga::store {

namespace {

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

/* Write `bytes` to `p` unless it already holds exactly them. */
bool mirror_file(const fs::path& p, const std::string& bytes) {
    std::error_code ec;
    if (fs::exists(p, ec) && fs::file_size(p, ec) == bytes.size() && read_file(p) == bytes) return true;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), (std::streamsize)bytes.size());
    return (bool)out;
}

/* Where a file named in a bundle goes, or nothing if it would leave its folder. */
bool place(const std::string& path, const fs::path& base_dir, const fs::path& assets_dir, fs::path& out) {
    // The same verdict on every platform: a bundle is a file from another
    // machine. POSIX reads "C:/x" as relative and "..\x" as one name, so a drive
    // or a backslash is refused outright (neither occurs in a portable name).
    if (path.find(':') != std::string::npos || path.find('\\') != std::string::npos) return false;
    const bool asset = path.rfind("assets/", 0) == 0;
    const fs::path rel = fs::path(asset ? path.substr(7) : path).lexically_normal();
    if (rel.empty() || rel.is_absolute() || rel.has_root_name() || rel.has_root_directory()) return false;
    for (const auto& part : rel)
        if (part == "..") return false;
    out = (asset ? assets_dir : base_dir) / rel;
    return true;
}

void remove_store(const fs::path& p) {
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm", "-journal"}) fs::remove(p.string() + s, ec);
}

} // namespace

BundleKind bundle_kind(const fs::path& file) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return BundleKind::Missing;
    char head[16] = {};
    in.read(head, sizeof head);
    if (in.gcount() == 16 && std::memcmp(head, "SQLite format 3", 16) == 0) return BundleKind::V4;
    for (std::streamsize i = 0; i < in.gcount(); ++i) {
        if (head[i] == ' ' || head[i] == '\n' || head[i] == '\r' || head[i] == '\t') continue;
        return head[i] == '{' ? BundleKind::V3 : BundleKind::Other;
    }
    return BundleKind::Other;
}

BundleResult pack_v4(Store& store, const fs::path& base_dir, const fs::path& assets_dir,
                     const std::vector<std::string>& extra, const fs::path& out, const std::string& name,
                     const Progress& progress) {
    BundleResult r;
    r.name = name;
    const fs::path tmp = out.string() + ".packing";
    remove_store(tmp);
    if (progress) progress(0.05, "Copying the database");
    if (!store.vacuum_into(tmp)) {
        r.error = "could not copy the database: " + store.error();
        remove_store(tmp);
        return r;
    }
    bool ok = false;
    {
        Store copy(tmp);
        ok = copy.ok();
        if (!ok) r.error = "could not open the copy: " + copy.error();
        if (ok) {
            copy.purge(); // a bundle is what the database IS: no row an undo could still have reached
            // the pictures, one at a time: memory is the largest one, not all of them
            std::vector<std::pair<std::string, fs::path>> files; // name in the bundle, file on disk
            std::error_code ec;
            if (fs::is_directory(assets_dir, ec))
                for (fs::recursive_directory_iterator it(assets_dir, ec), end; !ec && it != end; it.increment(ec))
                    if (it->is_regular_file(ec))
                        files.emplace_back("assets/" + fs::relative(it->path(), assets_dir, ec).generic_string(),
                                           it->path());
            for (const auto& rel : extra) // files a rune names outside the assets folder
                if (rel.rfind("assets/", 0) != 0 && fs::is_regular_file(base_dir / rel, ec))
                    files.emplace_back(fs::path(rel).generic_string(), base_dir / rel);
            ok = copy.begin();
            for (std::size_t i = 0; ok && i < files.size(); ++i) {
                const std::string bytes = read_file(files[i].second);
                copy.put_blob(files[i].first, bytes);
                ++r.assets;
                r.asset_bytes += (long long)bytes.size();
                if (progress) progress(0.3 + 0.6 * (double)(i + 1) / (double)files.size(), "Packing pictures");
            }
            if (ok) {
                copy.set_meta("bundle.version", "4");
                copy.set_meta("bundle.name", name);
                copy.set_meta("bundle.app", "hormiga");
                ok = copy.commit();
            }
            if (!ok) {
                r.error = "could not pack: " + copy.error();
                copy.rollback();
            }
            r.runes = copy.rune_count();
        }
    }
    if (!ok) {
        remove_store(tmp);
        return r;
    }
    std::error_code ec;
    for (const char* s : {"-wal", "-shm"}) fs::remove(tmp.string() + s, ec); // the copy closed: one file
    fs::rename(tmp, out, ec);
    if (ec) {
        r.error = "could not write " + out.string() + ": " + ec.message();
        remove_store(tmp);
        return r;
    }
    if (progress) progress(1.0, "Packed");
    r.ok = true;
    return r;
}

BundleResult open_bundle(const fs::path& in, const fs::path& store_path, const fs::path& base_dir,
                         const fs::path& assets_dir, const Progress& progress) {
    BundleResult r;
    const BundleKind kind = bundle_kind(in);
    if (kind == BundleKind::Missing || kind == BundleKind::Other) {
        r.error = kind == BundleKind::Missing ? "no such file: " + in.string() : "not a .miga bundle";
        return r;
    }
    remove_store(store_path);
    std::error_code ec;
    if (kind == BundleKind::V3) { // one JSON document: streamed into a new store
        Store st(store_path);
        JsonStream js(in.string());
        const auto stats = st.import(js, progress, [&](const std::string& path, const std::string& bytes) {
            fs::path to;
            if (place(path, base_dir, assets_dir, to)) mirror_file(to, bytes);
            else ++r.refused;
        });
        r.ok = stats.ok;
        r.error = stats.error;
        r.assets = stats.assets;
        r.runes = stats.runes;
        r.name = stats.bundle_name;
        return r;
    }
    // v4: the store itself, copied; then its pictures out into the folders
    if (progress) progress(0.05, "Copying the database");
    if (!fs::copy_file(in, store_path, fs::copy_options::overwrite_existing, ec)) {
        r.error = "could not copy " + in.string() + ": " + ec.message();
        return r;
    }
    Store st(store_path);
    if (!st.ok()) {
        r.error = "could not open the copied database: " + st.error();
        return r;
    }
    const auto paths = st.blob_paths();
    for (std::size_t i = 0; i < paths.size(); ++i) {
        fs::path to;
        if (!place(paths[i], base_dir, assets_dir, to)) {
            ++r.refused;
            continue;
        }
        const auto bytes = st.blob(paths[i]);
        if (!bytes) continue;
        mirror_file(to, *bytes);
        ++r.assets;
        r.asset_bytes += (long long)bytes->size();
        if (progress) progress(0.1 + 0.8 * (double)(i + 1) / (double)paths.size(), "Restoring pictures");
    }
    r.name = st.meta("bundle.name");
    r.runes = st.rune_count();
    if (progress) progress(1.0, "Opened");
    r.ok = true;
    return r;
}

} // namespace hormiga::store
