/* paths.cpp — where an organization's folders actually are.
 *
 * THE RULE, unchanged since headless learned it: everything an organization
 * owns hangs off the folder its state document lives in. `--state
 * ../org-data/org.state.json` makes `../org-data` the base, and `assets/`, `tiles/`,
 * `site/`, `exports/`, `backups/`, `documents/`, `templates/` and `fonts/`
 * follow it there. One database, one folder, its own everything.
 *
 * WHAT THIS FILE ADDS (2026-09-01). That rule was right and it was also the
 * only option: the eight names were spelled as string literals at forty-odd
 * call sites, so a database could not put its tiles on another drive, and an
 * operator whose photo library already lives somewhere had to move it. Each
 * folder is now `config set paths.<name> <path>` — absolute, or relative to the
 * database — and unset still means the plain default. Nothing about the
 * one-database-one-folder rule changed; it stopped being the only sentence the
 * app could say.
 *
 * TILES ARE NOT ASSETS, which is the distinction that made this worth doing.
 * `assets/` holds originals that exist nowhere else and must be backed up;
 * `tiles/` is a re-fetchable cache (platform/miga.cpp files it under
 * `rebuild`). Being able to point them at different places is the difference
 * between a backup that means something and one that carries a map of Oregon.
 *
 * RESOLVED ON FIRST USE, not at init(): the desktop front-end calls `init()`
 * and the headless one never does, so anything resolved there would be right in
 * one binary and empty in the other. First use is after the document is loaded
 * in both. Cached because the answer cannot change within a run — the CLI is
 * one process per command, so `config set` is always visible to the next one.
 */
#include "app/app_internal.hpp"
#include "app/paths.hpp"

namespace hormiga {

std::filesystem::path resolve_data_dir(maiz::Core& core,
                                       const std::filesystem::path& base,
                                       const std::string& name) {
    /* `config get` hands back a JSON-ish scalar: quoted when set, the literal
     * `null` when it never was. Both mean "no override" once unwrapped. */
    std::string v = core.dispatch("config get paths." + name).data;
    if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
        v = v.substr(1, v.size() - 2);
    if (v == "null") v.clear();

    if (v.empty()) return base / name;
    /* A RELATIVE OVERRIDE IS RELATIVE TO THE DATABASE, never to the process's
     * working directory. `paths.assets = ../shared-photos` has to mean the same
     * folder whether the operator launched from their home directory or from
     * inside the database, or the setting is a trap. */
    std::filesystem::path p(v);
    return p.is_relative() ? base / p : p;
}

} // namespace hormiga

/* CONST, because asking where the assets folder is does not change the
 * application — `assets_dir()` and `load_user_templates()` are both const and
 * both need the answer. The cache is `mutable` for the same reason, and the
 * dispatcher is reached through a cast because `config get` is a read wearing a
 * command's clothes: Void Core's `dispatch` is non-const for the ninety-nine
 * verbs that do write something. */
std::filesystem::path HormigaApp::data_dir(const std::string& name) const {
    auto it = data_dir_cache.find(name);
    if (it != data_dir_cache.end()) return it->second;
    const std::filesystem::path p = hormiga::resolve_data_dir(
        const_cast<maiz::Core&>(core), base_dir, name);
    data_dir_cache.emplace(name, p);
    return p;
}

/* ── EVERY FILE THE MODEL POINTS AT, WHEREVER IT LIVES (2026-09-03) ──────────
 *
 * The portfolio agent, and it is the worst shape a bug can have:
 *
 *   > the site was correct, the bundle was correct, and the combination was
 *   > broken.
 *
 * `download.file` is documented as "a path relative to the database", so they
 * put a resume in `resume/` beside the database and pointed a block at it. The
 * render staged it. The deploy published it. `effect pack-database` then wrote
 * a `.miga` that **silently did not contain it** — because `pack()` bundles the
 * assets folder and nothing else — and the bundle, opened anywhere else,
 * reported "This file is not available." Correctly, honestly, and with no way
 * to tell that the file had never been packed rather than deleted. Every
 * individual step said `ok`.
 *
 * ── WHY THIS IS DERIVED FROM THE GLYPH DECLARATIONS AND NOT FROM A LIST ─────
 *
 * The obvious implementation is a list of path-bearing field names —
 * `image.path`, `hero.image`, `audio.src`, `download.file` — and it is the
 * wrong one for a reason this codebase has now paid for three times: a
 * hand-maintained list of things the model can do goes stale silently, in the
 * direction where the newest feature is the one that breaks. `image_grid`'s
 * `columns`, the whole of `hol_github`, and `resource` were all "declared and
 * read by nothing"; a pack list would be the same trap pointed at data loss.
 *
 * So the question asked here is the one the DECLARATION already answers: a
 * projected `SceneField` carries its `editor`, which is `hints.editors[key]`
 * from the glyph. A field an operator picks a FILE for is declared `path`; one
 * they pick an image for is declared `image`. Add a glyph tomorrow with a
 * `path` field and it is bundled without anybody editing this function.
 *
 * ── AND WHY A CREDENTIAL DOES NOT COME ALONG ────────────────────────────────
 *
 * This is the part to keep in mind before widening it. `hol_static_host` has a
 * `token_file`, `hol_object_store` has a `secret_file`, `hol_sqlite` has a
 * `file` — all paths, all beside the database, and a `.miga` is **not
 * encrypted** (its own effect docstring says so: it "carries the organization's
 * data in readable form"). Bundling a deploy token into a file people hand to
 * each other would be a genuine leak.
 *
 * None of them is declared with a `path` EDITOR — they are labelled strings,
 * because an operator types or browses them through the Antfarm rather than
 * through the widget registry — so asking the declaration rather than guessing
 * from the field name is what keeps them out. That is not luck, but it is not a
 * guarantee either: **if a secret-bearing field is ever declared `path`, it
 * will be bundled.** The `kNeverBundle` list below is the second lock, by file
 * shape rather than by field name, so both would have to be wrong at once.
 *
 * Returns bundle-key → source path. The key is the model's own relative string,
 * so `open` puts the file back exactly where the field expects to find it.
 */
std::map<std::string, fs::path> HormigaApp::referenced_files(
    const std::string& state_json) {
    /* Boots its own core from the document it is handed, like `sync_ops.cpp`
     * and `translation_report`: this walks EVERY mantle, so it cannot ride the
     * active projection. */
    core = maiz::Core(state_json);
    if (on_register_glyphs) on_register_glyphs(core);
    std::map<std::string, fs::path> out;

    /* A file that must never travel in a bundle, whatever declared it. Matched
     * on the resolved NAME rather than on the field, so a mistake in a glyph
     * declaration cannot open this door on its own. */
    auto never_bundle = [](const fs::path& p) {
        static const char* kNeverBundle[] = {".key",   ".token", ".pem",
                                             ".p12",   ".pfx",   ".bkp",
                                             ".miga",  ".lock",  ".vault"};
        std::string ext;
        for (char c : p.extension().string())
            ext += (char)std::tolower((unsigned char)c);
        for (const char* b : kNeverBundle)
            if (ext == b) return true;
        std::string fn;
        for (char c : p.filename().string())
            fn += (char)std::tolower((unsigned char)c);
        return fn.find("secret") != std::string::npos ||
               fn.find("token") != std::string::npos ||
               fn.find("credential") != std::string::npos ||
               fn.find(".state.json") != std::string::npos;
    };

    std::vector<std::string> mantles;
    for (std::string line : core.dispatch("mantles").lines) {
        while (!line.empty() && (line.front() == '*' || line.front() == ' '))
            line.erase(line.begin());
        if (auto p = line.find(" ("); p != std::string::npos) line.resize(p);
        while (!line.empty() && line.back() == ' ') line.pop_back();
        if (!line.empty() && line != "(no mantles)") mantles.push_back(line);
    }

    std::error_code ec;
    for (const std::string& mt : mantles) {
        maiz::ProjectOptions po;
        po.mantle = mt;
        const maiz::Scene sc = maiz::project_scene(core, po);
        for (const auto& n : sc.nodes)
            for (const auto& f : n.fields) {
                if (f.editor != "path" && f.editor != "image") continue;
                const std::string v = field_value(n, f.key);
                /* An `image` editor names an image RUNE on some glyphs and a
                 * file on others. Resolving against the filesystem settles it
                 * without either of them having to say which: a rune name is
                 * not a file and simply does not match. */
                if (v.empty()) continue;
                const fs::path src =
                    fs::path(v).is_absolute() ? fs::path(v) : base_dir / v;
                if (!fs::is_regular_file(src, ec)) continue;
                if (never_bundle(src)) continue;
                /* ABSOLUTE PATHS ARE NOT BUNDLED, and this is the honest limit.
                 * The bundle key has to be a path the opener can restore to and
                 * the field can still resolve; `C:\Users\somebody\...` is
                 * neither. The pack reports these rather than pretending. */
                const std::string rel =
                    fs::relative(src, base_dir, ec).generic_string();
                if (ec || rel.empty() || rel.rfind("..", 0) == 0) continue;
                out[rel] = src;
            }
    }
    return out;
}
