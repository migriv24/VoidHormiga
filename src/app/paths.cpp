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
#include "platform/app_settings.hpp" // the recent databases
#include "app/app_internal.hpp"
#include "app/paths.hpp"

#include "json.hpp" // the machine-local note beside the working copy

#include <fstream>

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
    /* ITS OWN CORE, NEVER THE APP'S (2026-09-16). This read `core = maiz::Core(
     * state_json)`, which replaced the RUNNING core -- and `install_host()` is
     * what puts the effect handler and the log sink on a core, so afterwards
     * every `effect render-site`, `deploy-site` or `host-online` answered "no
     * host effect handler for 'effect'" until the app was restarted, with the
     * core's own warning going nowhere because the sink went with it. The
     * comment above always said this boots its own core; now it does. */
    maiz::Core probe(state_json);
    if (on_register_glyphs) on_register_glyphs(probe);
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
    for (std::string line : probe.dispatch("mantles").lines) {
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
        const maiz::Scene sc = maiz::project_scene(probe, po);
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

/* ── A FILE A RUNE NAMES, FOUND WHERE IT ACTUALLY IS (2026-09-15) ────────────
 *
 * The author, on a fresh install: *"none of the cat images are included in the
 * cat database for the install."* The Cat Colony seed names its photos
 * `demo-assets/cat-NN.jpg`, and every reader resolved that against the DATABASE's
 * folder. That is right in a checkout, where the database sits beside
 * `demo-assets/`, and wrong in an installation, where the photos travel beside
 * the PROGRAM (`ship_dir`) and the database lives wherever its owner put it.
 *
 * The database folder still wins, so an organization's own `assets/` is never
 * shadowed by something shipped. The program's folder is only the fallback, and
 * only for a relative path that does not exist under the database. */
std::filesystem::path HormigaApp::resolve_file(const std::string& raw) const {
    /* A value written before 2026-09-16 can carry its own quotes (the `set` /
     * `setjson` mix-up), and a path that cannot be opened is a picture that
     * silently is not there. They are stripped here, at the one door every local
     * file goes through, so a database that already has them still works. */
    std::string rel = raw;
    if (rel.size() >= 2 && rel.front() == 0x22 && rel.back() == 0x22)
        rel = rel.substr(1, rel.size() - 2);
    const std::filesystem::path p(rel);
    if (rel.empty() || p.is_absolute()) return p;
    std::error_code ec;
    const std::filesystem::path here = base_dir / p;
    if (std::filesystem::exists(here, ec) || ship_dir.empty()) return here;
    const std::filesystem::path shipped = ship_dir / p;
    return std::filesystem::exists(shipped, ec) ? shipped : here;
}

/* ── WHERE THIS WORKING COPY'S FILES LIVE, ON THIS MACHINE (2026-09-16) ──────
 *
 * The author, first: *"we have the IDs and stuff correct in the antfarm, but
 * still can't publish? or it still ask for keys."* The IDs travel inside the
 * database. The keys do not, by design: `cloudflare_token.txt` and `imgbb.key`
 * sat beside `LON_*.miga` in the organization's folder, the hosting nodes named
 * them relatively, and every reader resolved that against `base_dir` -- the
 * folder the program was LAUNCHED from. The same database published or asked
 * for keys depending on how Hormiga was started.
 *
 * Then, the same day: *"we should also have an option of selecting like, the
 * folder where things live. and it looks for a json that has the information.
 * there should be a 'priority folder' where the files there will be searched
 * first."*
 *
 * So a relative key file is looked for in, in order:
 *
 *   1. the PRIORITY FOLDER, when one is chosen (Niche Tools > Where is this
 *      database?) -- a person's own answer beats any inference;
 *   2. the folder of the `.miga` this working copy was opened from or saved to;
 *   3. the working folder, which keeps every setup that already worked.
 *
 * THE JSON is `<state>.local.json` beside the working copy (covered by
 * `demo-org.json.*` in .gitignore). It is a fact about THIS computer, so it is
 * never `config`: config travels inside the database, and a path into one
 * person's Documents folder must not travel to the next person. Paths are
 * written as UTF-8, because nlohmann refuses anything else and a user folder
 * named with an accent is ordinary.
 *
 * Deliberately NOT `cur_miga`. Restoring that on boot would also change what
 * Save does after a restart, which is a different decision. */
namespace hormiga {

std::filesystem::path find_key_file(const std::string& raw,
                                    const std::vector<std::filesystem::path>& dirs,
                                    std::string* tried) {
    namespace fs = std::filesystem;
    std::string name = raw;
    if (name.size() >= 2 && name.front() == 0x22 && name.back() == 0x22)
        name = name.substr(1, name.size() - 2);
    if (tried) tried->clear();
    const fs::path p(name);
    if (name.empty()) return {};
    if (p.is_absolute() || dirs.empty()) {
        if (tried) *tried = p.string();
        return p;
    }
    std::error_code ec;
    for (const auto& d : dirs) {
        const fs::path c = d / p;
        if (fs::exists(c, ec)) return c;
        if (tried) *tried += (tried->empty() ? "" : ", or ") + c.string();
    }
    return dirs.front() / p;
}

} // namespace hormiga

namespace {

std::string to_u8(const std::filesystem::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}

std::filesystem::path from_u8(const std::string& s) {
    return std::filesystem::path(std::u8string(s.begin(), s.end()));
}

void write_local_note(const std::filesystem::path& note, const std::filesystem::path& bundle,
                      const std::filesystem::path& priority) {
    std::error_code ec;
    if (bundle.empty() && priority.empty()) {
        std::filesystem::remove(note, ec);
        return;
    }
    nlohmann::json j = nlohmann::json::object();
    j["about"] = "Where this working copy's files live on THIS computer. Written by "
                 "Void Hormiga; never part of the database, never shared.";
    if (!bundle.empty()) j["bundle"] = to_u8(bundle);
    if (!priority.empty()) j["priority_dir"] = to_u8(priority);
    std::ofstream(note, std::ios::binary) << j.dump(2) << "\n";
}

} // namespace

std::filesystem::path HormigaApp::local_note() const {
    return base_dir / (state_name + ".local.json");
}

std::vector<std::filesystem::path> HormigaApp::key_dirs() const {
    namespace fs = std::filesystem;
    if (!local_read) {
        local_read = true;
        std::ifstream in(local_note(), std::ios::binary);
        const nlohmann::json j =
            in ? nlohmann::json::parse(in, nullptr, false) : nlohmann::json();
        if (j.is_object()) {
            if (j.contains("bundle") && j["bundle"].is_string())
                bundle_file = from_u8(j["bundle"].get<std::string>());
            if (j.contains("priority_dir") && j["priority_dir"].is_string())
                priority_dir = from_u8(j["priority_dir"].get<std::string>());
        }
    }
    std::vector<fs::path> dirs;
    auto add = [&dirs](const fs::path& d) {
        if (d.empty()) return;
        std::error_code ec;
        for (const auto& x : dirs)
            if (x == d || fs::equivalent(x, d, ec)) return;
        dirs.push_back(d);
    };
    add(priority_dir);
    add(bundle_file.parent_path());
    add(base_dir);
    return dirs;
}

void HormigaApp::remember_bundle(const std::string& miga) {
    std::error_code ec;
    key_dirs(); // read the note first, so the priority folder survives this write
    bundle_file = miga.empty() ? std::filesystem::path()
                               : std::filesystem::absolute(miga, ec);
    write_local_note(local_note(), bundle_file, priority_dir);
    hormiga::app_settings::note_recent(miga); // File > Recent databases, on this device
}

void HormigaApp::set_priority_dir(const std::string& dir) {
    std::error_code ec;
    key_dirs();
    priority_dir = dir.empty() ? std::filesystem::path() : std::filesystem::absolute(dir, ec);
    write_local_note(local_note(), bundle_file, priority_dir);
}
