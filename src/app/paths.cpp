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
