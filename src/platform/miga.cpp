/* miga.cpp — pack/unpack the .miga v3 database bundle. See miga.hpp. */
#include "platform/miga.hpp"

#include "json.hpp"
#include <sodium.h>

#include <ctime>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;

namespace hormiga::miga {
namespace {

std::string b64_encode(const std::vector<unsigned char>& bin) {
    size_t n = sodium_base64_encoded_len(bin.size(), sodium_base64_VARIANT_ORIGINAL);
    std::string out(n, '\0');
    sodium_bin2base64(out.data(), n, bin.data(), bin.size(),
                      sodium_base64_VARIANT_ORIGINAL);
    if (!out.empty() && out.back() == '\0') out.pop_back(); // drop the NUL term
    return out;
}

std::vector<unsigned char> b64_decode(const std::string& s) {
    std::vector<unsigned char> out(s.size()); // decoded is always shorter
    size_t real = 0;
    if (sodium_base642bin(out.data(), out.size(), s.c_str(), s.size(), nullptr,
                          &real, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0)
        return {};
    out.resize(real);
    return out;
}

std::vector<unsigned char> read_file(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::string iso_now() {
    std::time_t t = std::time(nullptr);
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    return b;
}

} // namespace

PackResult pack(const std::string& state_json, const fs::path& base_dir,
                const fs::path& out_path, const std::string& name,
                const fs::path& assets_dir,
                const std::map<std::string, fs::path>& extra) {
    (void)base_dir; // kept: the envelope's other paths are still base-relative
    PackResult r;
    json env;
    env["magic"] = "MIGA";
    env["version"] = 3;
    env["meta"] = {{"name", name},
                   {"app", "hormiga"},
                   {"saved", iso_now()}};
    try {
        env["state"] = json::parse(state_json);
    } catch (...) {
        env["state"] = state_json; // keep it as a string rather than lose it
    }

    // bundle every file under assets/ (irreplaceable user media). Re-derivable
    // caches (tiles/, site/, exports/) are deliberately NOT bundled.
    json assets = json::object();
    std::error_code ec;
    const fs::path& adir = assets_dir;
    if (fs::exists(adir)) {
        for (auto it = fs::recursive_directory_iterator(adir, ec);
             !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
            if (!it->is_regular_file()) continue;
            /* THE KEY KEEPS THE FOLDER'S REAL NAME when the assets folder is
             * inside the database — `assets/cat-01.jpg`, or `demo-assets/…` for
             * a database whose photos live there. That matters because image
             * fields in the document are stored relative to the DATABASE
             * (`stage_site_asset` resolves `base_dir / rel`), so a bundle that
             * renamed the folder on the way in would restore fifty photos the
             * document could no longer find. For the default folder this is
             * byte-identical to the old `relative(path, base_dir)`, so bundles
             * written before any of this still open.
             *
             * Only an assets folder OUTSIDE the database falls back to a fixed
             * `assets/` prefix — there is no base-relative name to keep, and
             * `open` puts those back wherever the opener's assets folder is. */
            std::error_code rec;
            const std::string under =
                fs::relative(it->path(), base_dir, rec).generic_string();
            const bool inside =
                !rec && !under.empty() && under.rfind("..", 0) != 0;
            std::string rel =
                inside ? under
                       : ("assets" / fs::relative(it->path(), adir, rec))
                             .generic_string();
            if (rel.empty() || rel == "assets") continue;
            assets[rel] = b64_encode(read_file(it->path()));
            ++r.assets;
        }
    }
    /* ── AND EVERY FILE THE MODEL POINTS AT FROM ANYWHERE ELSE (2026-09-03) ──
     *
     * The assets walk above is "back up the folder of originals", and it was the
     * whole of the bundle until a `download.file` pointed at a resume in a
     * folder beside the database. The render staged it, the deploy published
     * it, and the bundle silently did not contain it — so opening that bundle
     * anywhere else gave "This file is not available", correctly and
     * unhelpfully. The portfolio report's sentence for it: *the site was
     * correct, the bundle was correct, and the combination was broken.*
     *
     * `extra` is resolved by the CALLER (`HormigaApp::referenced_files`), which
     * has the glyph declarations this file deliberately does not: which fields
     * are files is a property of the model, and a copy of that knowledge here
     * would be a second place to keep it right. See app/paths.cpp, including
     * why a deploy token does not arrive in this map.
     *
     * Keyed by the model's own relative string, so `open` restores each file
     * exactly where the field expects it. An entry already bundled by the
     * assets walk is left alone — the folder version is the same bytes, and
     * counting it twice would make the report lie. */
    for (const auto& [rel, src] : extra) {
        if (assets.contains(rel)) continue;
        std::error_code fe;
        if (!fs::is_regular_file(src, fe)) continue;
        assets[rel] = b64_encode(read_file(src));
        ++r.assets;
        r.beyond_assets.push_back(rel);
    }
    env["assets"] = std::move(assets);
    env["rebuild"] = {{"tiles", "re-fetch from the map source node"},
                      {"site", "re-render from the document mantles"}};

    // write-temp-then-rename: a failed write never corrupts the current bundle
    fs::path tmp = out_path;
    tmp += ".part";
    {
        std::ofstream o(tmp, std::ios::binary | std::ios::trunc);
        if (!o) { r.error = "cannot write " + tmp.string(); return r; }
        o << env.dump();
        if (!o) { r.error = "write failed"; return r; }
    }
    fs::rename(tmp, out_path, ec);
    if (ec) { r.error = ec.message(); fs::remove(tmp); return r; }
    r.bytes = (long long)fs::file_size(out_path, ec);
    r.ok = true;
    return r;
}

OpenResult open(const fs::path& miga_path, const fs::path& base_dir,
                const fs::path& assets_dir) {
    OpenResult r;
    std::ifstream in(miga_path, std::ios::binary);
    if (!in) { r.error = "cannot open " + miga_path.string(); return r; }
    std::stringstream ss;
    ss << in.rdbuf();
    json env;
    try {
        env = json::parse(ss.str());
    } catch (...) {
        r.error = "not a readable .miga (parse failed)";
        return r;
    }
    r.version = env.value("version", 0);
    r.name = env.value("meta", json::object()).value("name", std::string());

    if (r.version < 3 || !env.contains("state")) {
        // a v2 legacy vault (secrets only) — no database to load, but valid
        r.version = env.value("version", 2);
        r.error = "legacy .miga (v2, secrets only) — no full database inside";
        return r;
    }

    // extract assets into base_dir/assets/ (they overwrite the working copy)
    std::error_code ec;
    if (env.contains("assets") && env["assets"].is_object()) {
        for (auto& [rel, b64] : env["assets"].items()) {
            if (rel.find("..") != std::string::npos) continue; // no traversal
            /* `assets/<rest>` lands in THIS database's assets folder, wherever
             * that now is; anything else stays base-relative as before. The
             * two are the same path until somebody sets `paths.assets`. */
            fs::path dest = rel.rfind("assets/", 0) == 0
                                ? assets_dir / rel.substr(7)
                                : base_dir / rel;
            fs::create_directories(dest.parent_path(), ec);
            auto bytes = b64_decode(b64.get<std::string>());
            std::ofstream of(dest, std::ios::binary | std::ios::trunc);
            of.write((const char*)bytes.data(), (std::streamsize)bytes.size());
            ++r.assets;
        }
    }

    // the state document — a string (kept verbatim) or an object (re-dump)
    if (env["state"].is_string()) r.state = env["state"].get<std::string>();
    else r.state = env["state"].dump();
    r.ok = true;
    return r;
}

bool looks_like_miga(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    char head[8] = {};
    in.read(head, 7);
    // v3/v2 both start with a JSON object containing "magic":"MIGA"
    std::string s(head);
    return s.find('{') != std::string::npos; // cheap; full check is in open()
}

} // namespace hormiga::miga
