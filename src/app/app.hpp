/* app.hpp — the Hormiga application, platform-free (phase B skeleton).
 *
 * Everything the app IS lives here: the core, the projection, the workspace,
 * the command bar. The platform shells (main_desktop.cpp: GLFW) own only the
 * window, the GL context, and the input source — they call frame() once per
 * ImGui frame and provide the seams below. The split costs nothing now and
 * keeps the door open (DESIGN.md §9; the pattern InteractionCombinators
 * proved on Android).
 */
#pragma once

#include "domain/civic.hpp"            // the civic record: policies over time
#include "domain/hormiga_allomone.hpp" // Hormiga's domain Allomone (Void Maiz's engine)
#include "platform/miga.hpp"            // the .miga v3 database bundle (pack/unpack)
#include "platform/preview_server.hpp" // the live-preview localhost server (B2)
#include "domain/templates.hpp"       // starter templates for the Builder
#include "platform/storage.hpp" // the SQLite Data holiday (phase C)
#include "platform/vault.hpp"   // the passphrase-locked credential store (.miga v2)

#include "voidmaiz/action.hpp" // named canvas actions (Territory's vocabulary)
#include "voidmaiz/canvas.hpp"
#include "voidmaiz/code.hpp" // the from-scratch code editor that shipped with Allomone
#include "voidmaiz/embed.hpp"
#include "voidmaiz/face.hpp" // FaceRegistry is a member, not just a pointer
#include "voidmaiz/widget.hpp"
#include "voidmaiz/widgets.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <functional>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hormiga {

/* ── THE LANGUAGES THIS SITE CARRIES (2026-08-20) ────────────────────────────
 *
 * ONE list, because **publishing is not a per-language operation** and every
 * bug this constant exists to prevent came from pretending it was.
 *
 * What happened: the operator edited both halves of a bilingual site — the
 * console shows `summary_en` and `summary_es` set in the same sitting — then
 * rendered and published. `effect render-site` with no argument defaulted to
 * `en`. So `site/index-es.html` was never rebuilt, the deploy uploaded the
 * whole folder including a STALE Spanish page, and the `deployment` rune
 * recorded `lang 'en'` as though that were the truth of what went out. It was
 * not: what went out was one current language and one old one, under one URL.
 *
 * The Spanish half of this site is not a translation of the site. For most of
 * the people it is published for it IS the site, which is why the author's
 * rule is unconditional:
 *
 *   > publishing isn't just in one language, its for all languages.
 *
 * A `site/` folder is ONE artifact carrying every language, so the render that
 * precedes a publish renders all of them and the record names all of them. A
 * single language is a legitimate thing to render — the preview iterates on
 * one — but never a legitimate thing to PUBLISH.
 *
 * Adding a third language is this list, plus the `ui()` table in
 * `render/site.cpp` (which is a table for exactly this reason). Nothing that
 * publishes needs to change, because nothing that publishes names a language. */
inline const std::vector<std::string>& site_langs() {
    static const std::vector<std::string> ls{"en", "es"};
    return ls;
}

/* The languages, as one string for a log line or a `deployment` rune's `lang`
 * field: "en es". A record that names one language when two were published is
 * a record that lies, and this history is the thing a rollback reads. */
inline std::string site_langs_str() {
    std::string s;
    for (const auto& l : site_langs()) s += (s.empty() ? "" : " ") + l;
    return s;
}

} // namespace hormiga

struct HormigaApp {
    // ── platform seams (set before init) ────────────────────────────────────
    std::filesystem::path base_dir; // the org file lives here for now
    /* EVERY OTHER FOLDER AN ORGANIZATION OWNS, and the one door to it.
     * `data_dir("assets")`, `"tiles"`, `"site"`, `"exports"`, `"backups"`,
     * `"documents"`, `"templates"`, `"fonts"` — each defaults to that name
     * under `base_dir` and each is movable with
     * `config set paths.<name> <path>` (absolute, or relative to the
     * database). Resolved on first use and cached; see src/app/paths.cpp. */
    std::filesystem::path data_dir(const std::string& name) const;
    mutable std::map<std::string, std::filesystem::path> data_dir_cache;
    /* ── WHICH DOCUMENT IS THIS APPLICATION EDITING (2026-08-21) ────────────
     *
     * The state document's FILENAME, beside `base_dir`. It was the literal
     * "demo-org.json" in two places, which made the question above unanswerable
     * by anyone who was not the process: the GUI opened whatever sat in the
     * folder it happened to be launched from.
     *
     * That cost more than any other bug this month. Launched from the source
     * tree, the app opened — and then SAVED — `VoidHormiga/demo-org.json`,
     * while the organization's real database sat in the data folder. The
     * operator and the field agent spent two days editing two different copies of
     * one database and merged them by hand twice; the agent only noticed
     * because the data folder is under git and `git status` said nothing had
     * changed when something plainly had.
     *
     * `--state <path>` on either binary now sets both this and `base_dir`, so a
     * shortcut can pin the answer, and the resolved absolute path is in the
     * title bar so a person can SEE it. The `.miga` is supposed to be the thing
     * you hand somebody; until this, the thing you handed somebody was a path,
     * and paths are exactly what does not travel. */
    std::string state_name = "demo-org.json";
    /* WHERE THE BINARY LIVES, which is NOT where the data lives (2026-08-19).
     *
     * `render_site` staged its webfonts from `current_path()/vendor/fonts/web`.
     * `current_path()` is the DATABASE folder — an org folder is not a checkout
     * and has no `vendor/`, so the `if (exists)` guard stepped over it in
     * silence and every deploy went out with an empty `site/fonts/` behind
     * `@font-face` rules pointing at nothing. It looked right on the developer
     * machine, where the two paths happen to be the same folder.
     *
     * Anything Hormiga SHIPS (fonts, the guide, the OKF bundle) is found here;
     * anything the ORGANIZATION owns is found under `base_dir`. Both front-ends
     * set this from argv[0]; empty falls back to the working directory. */
    std::filesystem::path ship_dir;
    ImFont* mono_font = nullptr;    // JetBrains Mono for the script IDE (set by the shell)
    /* MAY THIS FRONT-END OFFER AN UPDATE? Set by the DESKTOP shell and by
     * nothing else -- it is in the public section beside `ship_dir` because a
     * shell is what sets it. The headless front-end builds a `HormigaApp` to
     * render a newsletter from, and an agent asking for a newsletter is not
     * consent to make a network request; its updater is
     * `voidhormiga-cli update`, a verb somebody typed. See `updates_boot`. */
    bool offer_updates = false;
    std::function<void(const std::string&)> on_title; // window title (optional)
    std::function<void()> on_quit;                    // File > Quit (optional)
    std::function<void(const std::string&)> on_open;  // open path/URL in the OS
    // OS file dialog ("" = cancelled) — powers the "path" editor kind
    std::function<std::string(std::string_view current)> on_pick_file;
    // OS SAVE dialog for a .miga (pick where to write) — (suggested name) →
    // chosen path, "" = cancelled. "Choose where to save" (author 2026-07-24).
    std::function<std::string(std::string_view suggested)> on_save_file;
    // decode + upload an image file to the GPU; id 0 = failed
    struct HostTexture {
        unsigned long long id = 0;
        int w = 0, h = 0;
    };
    std::function<HostTexture(const std::string& abs_path)> on_load_texture;
    // run a command line, capture stdout (the ImgBB holiday's transport —
    // curl ships with Windows; nothing to vendor)
    std::function<std::string(const std::string& cmd)> on_shell_capture;

    /* ── WHY A MERGED DOCUMENT IS NOT APPLIED WHERE IT IS COMPUTED ───────────
     *
     * It is tempting to have the sync effect just call `reload_from_state`. That
     * is a USE-AFTER-FREE and it cost an afternoon to see, so it is written down
     * rather than left as a rule nobody can explain:
     *
     *   `reload_from_state` assigns to `core`. The effect handler is a
     *   `std::function` OWNED BY `core`. So replacing the core from inside an
     *   effect destroys the callable that is currently executing, and the return
     *   path walks over freed memory.
     *
     * Headless it presented perfectly: the process died mid-effect, before the
     * session could save, leaving a stale lock and a database that looked
     * untouched -- the merge reported "applied" and nothing had been applied.
     *
     * So the merged document is CARRIED OUT of the effect and applied at a point
     * where nothing is mid-callback. `SyncReport::merged_state` is that carrier,
     * and each front-end drains it where it is safe to: the GUI at the top of
     * the next frame (`pending_state`), the CLI after the session has closed.
     * This is the same discipline `deployment_record` already states -- an
     * effect returns what should happen; it does not reach around into the model
     * it is an effect of. */
    std::string pending_state;  // GUI: a merged document waiting for the next frame

    /* REGISTER THIS APPLICATION'S GLYPHS ON A FRESHLY-BUILT CORE.
     *
     * A seam for an INCLUDE reason rather than a design one, and it is the same
     * one that put `render_from_state` in the headless unit: `seed.hpp` drags
     * the whole of nlohmann/json in, and it is deliberately off the include path
     * of the units that would otherwise push the link past PE's 16-bit section
     * ceiling (see app_internal.hpp). Both front-ends already include it, so
     * both can supply this; `sync_ops.cpp` cannot, and calls it instead. */
    std::function<void(maiz::Core&)> on_register_glyphs;

    void init();     // build (or reload) the core, seed the demo org, read view config
    void frame();    // one ImGui frame (between NewFrame and Render)
    void shutdown(); // window closing: save the org (no silent data loss)

    bool light_mode = true; // shells read this for the clear color

    /* ── the HEADLESS seam (2026-08-18) ──────────────────────────────────────
     *
     * Build the newsletter (`render`) or the website (`render-site`) from a
     * state document, with no window and no ImGui context, and return the path
     * written ("" on failure).
     *
     * WHY IT LIVES ON THE APP. The Output domain is `HormigaApp::` methods and
     * `section_web.cpp` is view-free — measured 2026-08-18, zero ImGui
     * references — so the headless front-end wants exactly these functions and
     * nothing else in the struct. Handing it the state document rather than a
     * `Core&` keeps `core` a value member (the GUI reassigns it in three
     * places, so it cannot be an alias) and costs one replay of a document
     * small enough to save on every edit.
     *
     * The point is that there is ONE renderer: the newsletter an agent
     * generates is byte-identical to the one the button generates, because it
     * is the same function over the same state. A second implementation would
     * be the drift this whole architecture exists to prevent. */
    std::string render_from_state(const std::string& state_json,
                                  std::string_view op, std::string_view lang,
                                  std::string_view document);

    /* Deploy the built `site/` folder through an Antfarm static-host node.
     * Returns the deployed URL (or ""), and reports through `log`.
     *
     * Takes the state document for the same reason `render_from_state` does —
     * the headless front-end's Core belongs to the session, and this app's is a
     * value member. `node` names the `hol_static_host` rune; empty picks the
     * only one, and refuses if there are several rather than guessing which
     * website to publish. */
    /* PUBLISHING (src/publish.cpp — a unit BOTH front-ends compile, which is
     * the point of it; see that file's header). The antfarm arrives already
     * projected, so neither of these needs `seed.hpp` and neither is confined
     * to one main(). */
    std::string deploy_site(const maiz::Scene& farm, std::string_view node);
    std::string rollback_site(const maiz::Scene& farm, std::string_view node,
                              std::string_view deployment);
    /* The commands that RECORD a publish. Returned, never dispatched here: an
     * effect must not reach around into the model it is an effect of, so the
     * caller applies them as an ordinary batch and the history is as logged,
     * attributed and replayable as every other change. */
    /* Push files to a `hol_object_store` node. Returns how many went, or -1 on
     * a refusal (reported through `log`).
     *
     * `what` selects WHICH artifact, and the set is deliberately closed rather
     * than a path the caller supplies: "index" is the published projection,
     * "backup" is the encrypted blob. A push effect that took an arbitrary path
     * would be a way to upload anything on the operator's disk from a command,
     * which is a hole nobody asked for. See okf/concepts/platform/data-planes.md. */
    int push_to_store(const maiz::Scene& farm, std::string_view node,
                      std::string_view what);
    /* Can these credentials reach this bucket? Performs the smallest REAL
     * operation against it, never a credential validator in the abstract — a
     * green light that does not predict the operation is worse than no light. */
    int check_store(const maiz::Scene& farm, std::string_view node);
    /* Can these credentials publish to this host? The sibling of `check_store`
     * for the other one-way door (field report A5). Makes the smallest REAL
     * reads a deploy performs — never a vendor "is this token valid" call, which
     * has been observed to lie. 0 = every check passed, 1 = one did not,
     * -1 = could not run. */
    int check_host(const maiz::Scene& farm, std::string_view node);
    /* How much of this database exists in `lang`, and a replayable script that
     * closes the gap. Writes `exports/translate-<lang>.hormiga`; returns the
     * number of untranslated fields (0 = complete, -1 = refused).
     *
     * Takes the state document rather than reading `core`, like the sync verbs
     * and for the same reason: it walks EVERY mantle, so it cannot ride the
     * active projection. It boots its own core from what it is handed.
     *
     * The answer to the 2026-09-02 field report's "let me publish one language"
     * — which the author declined. Bilingual is the resting state; what was
     * missing was the tooling that makes it cheap. See app/translate.cpp. */
    int translation_report(const std::string& state_json, std::string_view lang);
    /* Say so when `site.languages` is set: it is accepted by Void Core's
     * free-form config, stored, read back — and read by nothing here. A key
     * that confirms itself and does nothing is worse than a missing one
     * (portfolio report D1, 2026-09-02). */
    void warn_unread_language_key();
    /* Bundle-key -> source path for every file the MODEL points at, derived
     * from the glyph declarations (a `path`/`image` editor) rather than from a
     * list of field names that would go stale. `.miga` bundled only the assets
     * folder, so a `download.file` beside the database was silently absent from
     * every bundle (portfolio report D2, 2026-09-03). See app/paths.cpp for
     * why a deploy token does not come along. */
    std::map<std::string, std::filesystem::path> referenced_files(
        const std::string& state_json);
    /* Resolving a store node into a signing config lives in the PUBLISH layer,
     * not here: `app` may not depend on `publish` (tools/check_layering.py), and
     * a member whose parameter is an `aws::Config` would drag the whole vendor
     * surface into this header. It is a file-local helper in push.cpp instead,
     * which is where the vendor knowledge belongs anyway. */

    std::vector<std::string> deployment_record(const maiz::Scene& farm,
                                               const std::string& host,
                                               const std::string& url,
                                               const std::string& document,
                                               const std::string& lang);

    /* ── SYNC (okf/concepts/platform/collaboration.md) ───────────────────────
     *
     * Four operations, all of which REPORT BY DEFAULT AND WRITE ONLY ON
     * `apply`. That default is the whole ergonomic decision here: a merge is
     * the one operation in this application that can lose somebody's work, and
     * the shape that protects a volunteer is the one where seeing what would
     * happen costs nothing and is what you get by typing the obvious thing.
     *
     * They live in `src/app/sync_ops.cpp` and are compiled into BOTH front-ends,
     * because a verb that reaches one caller is a broken surface (founding
     * commitment 1, and the bug that taught it: `deploy_site` once lived in the
     * headless unit, so the GUI could not publish at all). */

    struct SyncReport {
        int rc = 0;                        // 0 = the verb did what it said
        std::string value;                 // e.g. the version name, for stdout
        std::vector<maiz::LogEntry> lines; // what a person reads. THE PRODUCT.
        /* The merged document, set ONLY when `apply` was asked for and the
         * merge succeeded. Empty otherwise -- including on a no-op merge of two
         * identical states, because there is nothing to write. The caller
         * applies it somewhere safe; see `pending_state` above. */
        std::string merged_state;
    };

    /* ONE ENTRY POINT FOR ALL FIVE SYNC VERBS, and it takes the state document
     * rather than assuming this app already holds it.
     *
     * That shape is forced by a real difference between the front-ends and it
     * is better for having been forced: the GUI owns its core, the headless CLI
     * borrows one from a Void Maiz `Session`, and a signature that reads the
     * state from `this` would work in one and silently operate on an empty
     * database in the other. Passing it in makes that impossible to get wrong.
     *
     * `op` is one of: sync-version, sync-merge, lan-peers, lan-serve, lan-sync.
     * Nothing is written unless `apply`; when it is, the merged document goes
     * out through `on_take_state` (above), which is what keeps this one verb
     * across two hosts. */
    SyncReport sync_op(std::string_view op, const std::vector<std::string>& args,
                       const std::string& state_json, bool apply);

    /* The GUI's adapter over `sync_op`: parse `{"args":[…]}`, run it, push the
     * report into the log strip, and park any merged document in
     * `pending_state`. Lives in sync_ops.cpp so app.cpp stays a shell. */
    bool gui_sync_effect(std::string_view op, std::string_view args, std::string& result);
    // the operator's panel: build / preview / publish / history (a dockable
    // window, drawn from Windows > Publish)
    void draw_publish_body();
    bool win_publish = false;
    int publish_lang = 0;            // 0 EN, 1 ES — which language to build
    std::string publish_host;        // chosen hol_static_host ("" = the only one)
    std::string publish_doc;         // chosen website document mantle
    bool publish_confirm = false;    // the one-way-door dialog is open
    std::string publish_restore;     // the deployment [restore] is confirming
    /* The host's own id for the deployment just created. Only the vendor knows
     * it, and a rollback needs it, so it rides out of `deploy_site` to the
     * `deployment` rune the caller writes. */
    std::string last_deploy_vendor_id;
    // the last "Test this token" result: "" = never run, "ok", or the vendor's
    // own refusal, kept verbatim
    std::string publish_token_check;
    // the paste box in the Publish panel; zeroed the moment it reaches the vault
    char publish_token_paste[128] = {};

private:
    // ── the model + projection (the one-sync rule: dispatch → project) ──────
    maiz::Core core;
    maiz::Scene scene;
    maiz::EditorState ed;
    maiz::CanvasStyle canvas_style;
    maiz::CommandBarState cmdbar;
    maiz::AddPalette palette;         // data glyphs (Data's connections canvas)
    maiz::AddPalette palette_blocks;  // block glyphs (the Builder canvas)
    maiz::AddPalette palette_antfarm; // holiday glyphs (the Antfarm canvas)
    maiz::AddPalette palette_allomone; // Allomone block glyphs (the rules canvas)
    maiz::FaceRegistry faces;
    maiz::WidgetRegistry widgets = maiz::WidgetRegistry::defaults();
    std::vector<maiz::LogEntry> log;
    int undo_depth = 0;

    // ── user feedback (toasts: dispatch errors, saves, renders) ─────────────
    struct Toast {
        std::string msg;
        float ttl = 4.0f;
        bool error = false;
    };
    std::vector<Toast> toasts;
    void toast(std::string msg, bool error = false);
    void draw_toasts();

    // ── view settings (config tier: logged, persisted, undo-exempt) ─────────
    float canvas_frac = 0.76f; // main pane | inspector split
    float log_frac = 0.74f;    // workspace | log strip split
    // Data tab pane fractions (config view.data_panels) — the sidebar|list|detail
    // split is now user-adjustable (UI/UX phase, 2026-08-03; author's #1 pain:
    // "things cant really resize" in fullscreen). Two splitters, two fractions.
    float data_side_frac = 0.17f; // sidebar | (list + detail)
    float data_list_frac = 0.34f; // list | detail, within the remainder
    void flush_data_panels();

    // ── the sections, now dockable windows (Void Maiz enabled ImGui docking,
    // 2026-07-20; workspace-and-sections concept). Each workflow is a window
    // the user can drag/float/tab/re-dock; the active one drives the mantle. ──
    enum Section { Data = 0, Builder = 1, Antfarm = 2, Map = 3 };
    int section = Data;
    int boot_section = -1;      // HORMIGA_SECTION: window to focus at boot
    int boot_focus_frames = 45; // held ~1s so the saved layout can't out-vote it
    bool boot_focus_cal = false; // HORMIGA_SECTION=calendar focuses that window
    bool boot_focus_notes = false; // HORMIGA_SECTION=notes focuses the Notes window
    bool dock_seeded = false; // one-shot default DockBuilder arrangement
    bool data_show_connections = false; // Data: browse (default) vs edge canvas
    int data_view_mode = 0;             // Data list pane: 0 list, 1 cards (ui.data_view)
    int data_card_size = 1;             // card view: 0 small, 1 medium, 2 large (ui.data_card_size)
    char filter[256] = {};              // Data: the compiled tag-grammar expression
    char search[128] = {};              // Data: name substring search
    // ── the TAG-FILTER builder (#2, 2026-08-03): filtering by clicking tags
    // instead of typing "@x AND NOT y". A flat list of conditions joined by one
    // connector, each optionally negated — compiles to the one grammar. Reusable
    // (the coming rules engine drives its own instance); the raw box stays as an
    // escape hatch for nested expressions. ──────────────────────────────────
    struct FilterTerm { bool neg = false; std::string tag; };
    std::vector<FilterTerm> data_filter_terms;
    int data_filter_join = 0;          // 0 = AND, 1 = OR (between conditions)
    bool data_filter_advanced = false; // show the raw expression box instead
    char filter_add_buf[96] = {};      // the "+ filter" picker's search buffer
    // Compile a term list + connector into a grammar expression ("" = match all).
    static std::string compile_filter(const std::vector<FilterTerm>& terms, int join);
    // The reusable builder bar. Renders chips + picker; on any change recompiles
    // into `out` and returns true. `addbuf` backs the picker's search input.
    bool draw_tag_filter(const char* id, std::vector<FilterTerm>& terms, int& join,
                         char* out, size_t outsz, char* addbuf, size_t addsz);
    std::string kind_sel;               // Data sidebar: "" = all runes
    int next_block_id = 1;              // Builder: palette-minted rune names
    int preview_lang = 0;               // Builder preview: 0 = EN, 1 = ES

    void switch_section(int s); // `use <mantle>` + selection reset + reproject
    // one section window: focus → activate; draw content only when active
    void section_window(const char* title, int which);
    void draw_data_section(float avail_h);
    // ── Data detail pane (UI/UX phase #4): a curated, TYPED form for people
    // (avatar + role dropdown + the fields that matter, no map-display noise),
    // a conditional location section, and connections moved to the bottom.
    // Widgets collect commands into `out`; the caller dispatches once at frame
    // end (re-projection would dangle `n`, so never dispatch mid-render). ────
    void draw_person_detail(const maiz::SceneNode& n, std::vector<std::string>& out);
    void draw_location_section(const maiz::SceneNode& n, std::vector<std::string>& out);
    void draw_relations(const maiz::SceneNode& n, std::vector<std::string>& out);
    // a reusable Tags editor (chips + a type-ahead to add) — appends `tag …`
    // commands to `out`; used by the contact/org form and the script editor.
    void draw_tag_editor(const maiz::SceneNode& n, std::vector<std::string>& out);
    char detail_tag_buf[96] = {}; // the tag editor's add picker buffer
    // Tag RECOMMENDER (okf/concepts/allomone/tag-recommender.md): suggest tags
    // to add, over the tag co-occurrence graph, in one of three modes. A read of
    // graph structure — same substrate Allomone reads, run in reverse.
    int tag_rec_mode = 0; // config ui.tags.recommend_mode: 0 similar,1 dissimilar,2 comprehensive
    std::string tag_rec_key;               // cache key (target + tags + mode)
    std::vector<std::string> tag_rec_cache; // last computed suggestions
    std::vector<std::string> compute_tag_suggestions(
        const maiz::SceneNode& target, int mode, int k) const;
    void draw_builder_section(float avail_h);
    void draw_antfarm_section();
    void draw_map_section();  // Territory placeholder (concept: territory.md)
    void draw_console();      // log strip + command bar (its own dock window)

    // ── Territory's action vocabulary (one definition, two front-ends) ──────
    maiz::ActionRegistry map_actions;         // place / move (map_actions.hpp)
    bool try_map_verb(const std::string& cmd); // `map <action> …` → ONE batch
    maiz::ActionRegistry doc_actions;         // the Builder's verbs (B1)
    bool try_doc_verb(const std::string& cmd); // `doc <action> …` → ONE batch
    // ── the THEME socket (builder.md QE: the Style tab grows into this) ─────
    bool win_style = true;              // the Style tab (theme editor)
    // the THEME (builder.md QE): a first-class style set the render packs read.
    // Config-tier (theme.*), logged, rides the org. Grows in parallel with the
    // Builder — the two tabs are one system (author, 2026-07-23).
    /* Style-tab buffers for the axes added 2026-08-20. They mirror
     * CONFIG — the tab reads them at boot and writes `config set` on
     * edit, so an agent changing the same key and a person moving the
     * control are one change in one log. */
    char theme_accent_lite[16] = {};
    char theme_accent_dark[16] = {};
    char theme_font_custom[64] = {};
    int theme_contrast = 1;      // 0 off · 1 AA · 2 AAA
    int theme_banner_filter = 0; // 0 none … 5 soft blur
    int theme_banner_dim = 45;   // scrim strength, 0-100
    int theme_grid_gap = 2;      // 0 tight … 4 airy
    bool theme_grid_even = true;
    bool theme_icons = true;

    float theme_accent[3] = {0.83f, 0.63f, 0.09f}; // #d4a017 (config theme.accent)
    float theme_accent2[3] = {0.83f, 0.63f, 0.09f}; // secondary accent (theme.accent2)
    float theme_bg[3] = {0.98f, 0.97f, 0.95f};     // #faf8f3 (config theme.bg)
    float theme_ink[3] = {0.125f, 0.125f, 0.114f}; // #20201d (config theme.ink)
    int theme_preset = 0;   // 0 clean 1 soft 2 bold 3 editorial 4 glass
    int theme_font = 0;     // HEADING font index (kFontStacks; theme.font)
    int theme_bodyfont = 0; // BODY font index (kFontStacks; theme.bodyfont)
    int theme_scale = 2;    // type scale 0..4 (theme.scale)
    int theme_radius = 2;   // corner radius 0..4 (theme.radius)
    int theme_texture = 0;  // page texture 0..3 (theme.texture)
    bool theme_dark = true; // site honors the visitor's prefers-color-scheme
    // W5 site meta (SEO + social): the deploy domain + a default description
    char site_base_url[160] = {}; // config site.base_url (absolute og:url/sitemap)
    char site_desc_buf[256] = {}; // config site.desc (default meta description)
    /* The colophon: the small line under the footer block. It defaults to
     * "Built with Void Hormiga" and is EDITABLE like everything else — an
     * operator can put their own credit there, or the literal "none" to print
     * no line at all. Unset means the default, which is why "none" carries the
     * erasure rather than an empty string (`config get` cannot tell an unset
     * key from one set to ""; `none` is the same sentinel the band background
     * fields already use in site.cpp). */
    char site_colophon_buf[160] = {}; // config site.colophon ("none" = omit)
    // branding (author 2026-07-24): a database belongs to an org — a company
    // NAME + a LOGO image path (config org.*, ride the database). The map
    // export stamps them as a watermark; the site can use them too.
    char org_name[80] = {};       // config org.name
    std::string org_logo;         // config org.logo (image path)
    char org_logo_buf[260] = {};
    void draw_style_tab();
    // ── LIVE PREVIEW (B2): localhost server + edit-debounced re-render ──────
    hormiga::PreviewServer preview_srv;
    bool preview_live = false;   // auto re-render + browser reload active
    bool preview_dirty = false;  // an edit landed since the last render
    double preview_edit_t = 0;   // when (debounce: render 0.45s after quiet)
    bool preview_start();        // start server + first render + open browser
    // ── LOCAL WEB HOST (Antfarm hol_localhost): the website served on
    // localhost like a real domain — the Q10 self-host rung, local edition.
    // Distinct from the dev preview: clean/deploy-faithful, its own port. ──
    hormiga::PreviewServer host_srv;
    bool host_start();           // render site + serve site/ clean + open
    // ── the DOCUMENT CANVAS (B3): near-WYSIWYG rows, every gesture a verb ───
    bool builder_doc_view = true;    // false = the legacy block canvas
    bool doc_migrate_queued = false; // auto-migrate fired once this session
    std::string cur_page;            // W2: the page being edited ("" = home/all)
    // ── DOCUMENTS as projects (author 2026-07-23): many newsletters/websites,
    // each a named mantle; switch between them. The store is the Data holiday
    // (local db now; Antfarm-decided later). See okf/concepts/sections/builder.md. ────
    std::string cur_doc = "issue-demo"; // the active Builder document (a mantle)
    char new_doc_name[64] = {};
    std::vector<std::string> list_documents(); // mantles that are documents
    void new_document(const std::string& name); // mantle new + switch + seed hero
    /* Rename / delete the current Builder document. Both were listed as blocked
     * on Void Core verbs that have since landed (`mantle rename`, `mantle rm`).
     * Delete refuses on the last remaining document — see builder.cpp. */
    void rename_document(const std::string& to);
    void delete_document(const std::string& name);
    int document_element_count(const std::string& name);
    char rename_doc_name[96] = {};  // staged document rename (Builder picker)
    /* Model edits since the last `save`, counted at the ONE door every GUI edit
     * goes through (`dispatch_and_reproject`). The Builder's Save button reads
     * it; nothing else should write it. The author asked for a Save in the tab
     * where the work happens (2026-09-02) — the writing was never the missing
     * part, being told was. */
    int edits_since_save = 0;
    void doc_new_page();             // mint a `page` rune + switch to it
    void draw_page_manager();        // the nothing-selected overview (map-style)
    char page_tag_input[64] = {};    // tag_picker buffer for page tags
    char filter_tag_input[64] = {};  // tag_picker buffer for the grid Filter UI
    bool filter_raw = false;         // show the raw query string (advanced)
    // ── TEMPLATES (builder.md enablers): start from a designed layout+theme,
    // or save the current document as one. Built-ins + user templates/*.json ─
    bool show_templates = false;
    char template_save_name[64] = {};
    void draw_templates_window();
    void apply_template(const hormiga::DocTemplate& t); // clears + builds (undoable)
    void save_current_as_template(const std::string& name);
    std::vector<hormiga::DocTemplate> load_user_templates() const;
    // a document/template's portable shape: name/kind/theme + build-commands,
    // serialized (JSON string) so the header stays light
    std::string capture_doc_json(const std::string& name);
    void export_document();       // cur_doc → documents/<slug>.json (a file)
    void import_document();       // pick a file → a NEW document mantle from it
    std::string doc_drag;            // component being drag-reordered
    std::string doc_grip;            // component whose span grip is held
    // in-place text editing (double-click a text element on the canvas)
    std::string doc_edit_node, doc_edit_base; // "" = not editing
    char doc_edit_buf[2048] = {};
    bool doc_edit_focus = false;     // grab keyboard focus the first frame
    std::map<std::string, std::string> doc_map_thumb; // view → exports PNG
    void draw_document_canvas(float body_h);
    void doc_palette_place(const std::string& glyph); // append via `doc place`

    // ── the live map canvas (mode 2, v1: OSM slippy map) ────────────────────
    maiz::Camera map_cam{-123.09f, 44.05f, 12.0f}; // x=lon, y=lat, zoom (a default view)
    std::string map_sel;         // selected map rune ("" = first available)
    bool map_cam_loaded = false; // view.map.camera restored once
    std::string map_drag_marker; // rune being marker-dragged ("" = none/pan)
    std::string map_drag_ref;    // refpoint GIZMO being dragged (#4 move)
    double map_ctx_lat = 0, map_ctx_lon = 0; // right-click point (place target)
    void map_new_earth();        // "New Earth map" → a map rune, one batch
    void map_place_new(const char* glyph);   // context: place new rune at ctx
    void map_place_existing(const std::string& name); // set geo on existing

    // background tile fetcher: pure I/O (curl → tiles/ dir), never touches
    // the core — same discipline as the mirror job's worker. FOUR workers:
    // tile fetches are latency-bound, and serial downloads made zooming
    // near-unusable (author, 2026-07-22).
    struct TileFetcher {
        std::vector<std::thread> workers;
        std::mutex mu;
        std::condition_variable cv;
        std::deque<std::string> queue;         // tile paths wanted
        std::map<std::string, std::string> url_of; // path → url
        std::set<std::string> queued;          // de-dupe
        std::atomic<bool> stop{false};
        std::atomic<int> in_flight{0};
        std::function<std::string(const std::string&)> shell;
        void start(std::function<std::string(const std::string&)> sh);
        void want(const std::string& url, const std::string& path);
        int pending(); // queued + downloading (the map's loading indicator)
        ~TileFetcher();
    };
    TileFetcher tiles;

    // map UX state (search / arm-to-place / marker menu / rename staging)
    char map_search[128] = {};   // the canvas's database search bar
    std::string map_place_arm;   // "click the map to place <name>" mode
    std::string map_ctx_marker;  // marker under the right-click menu
    // #3 drawable map SHAPES: a draw mode + the in-progress drag
    int map_draw_shape = 0;      // 0 off, 1 rect, 2 ellipse (arm to draw)
    bool map_shape_dragging = false;
    double map_shape_la0 = 0, map_shape_lo0 = 0; // drag-start geo corner
    std::string map_ctx_shape;   // mapshape under the right-click menu
    std::string map_ctx_ref;     // refpoint under the right-click menu
    // #4 REFERENCE POINTS: an entity whose `ref` names a refpoint is FANNED out
    // around the refpoint's location (they share coords but don't overlap).
    struct RefFan { double lat, lon; float dx, dy; }; // ref geo + pixel offset
    std::map<std::string, RefFan> ref_fans(const maiz::Scene& s,
                                           const std::string& channel) const;
    /* The rune-name editor: staged, commits one `rune rename` on Enter.
     * Shared by the Data detail pane and the Notes tab — a note is a rune the
     * Data tab deliberately skips, which is why it was the one kind that could
     * not be renamed (author, 2026-09-02).
     *
     * Returns TRUE when a rename landed, which means the caller's node
     * reference is dangling and it must return immediately. */
    bool rune_rename_control(const maiz::SceneNode& sel, float width);
    char rename_buf[96] = {};    // staged name edit (detail pane)
    std::string rename_for;      // which rune the buffer is staged for

    // ── the reusable SEARCH PICKER (author: "the search bar is crucial") ────
    // one component: input + type-ahead results; returns the picked name.
    // Used by the Link section, the map side panel, and future features.
    std::string search_picker(const char* id, char* buf, size_t bufsz,
                              const std::function<bool(const maiz::SceneNode&)>& keep,
                              const char* hint = "search the database...");

    // link section + map panel state
    char link_search[96] = {};
    char link_relation[48] = "connected-to";
    char panel_search[96] = {};

    // ── the RULE EDITOR: its own window (room to grow — author #3) ──────────
    bool show_rule_editor = false;
    int rule_edit_idx = -1;           // -1 = creating a new rule
    char rule_name[64] = {};
    std::vector<std::string> rule_tags; // chips: added/removed visibly
    char rule_tag_input[64] = {};
    int rule_icon = 0, rule_color = 0, rule_shape = 0; // 0 = default/circle
    void draw_rule_editor();          // edits the ACTIVE view's rules

    // ── views: manage window + the active view's parsed state ──────────────
    bool show_manage_views = false;
    void draw_manage_views();
    struct MapRule {                  // rules v2: named, multi-tag
        std::string name;
        std::vector<std::string> tags; // AND-joined for matching
        std::string icon, color, shape; // shape: circle|pin|square|diamond
    };
    std::vector<MapRule> active_rules; // parsed each frame from the view rune
    std::string active_channel = "main";
    void save_rules(const std::string& view_rune); // ONE setjson (undoable)
    static std::string rules_to_json(const std::vector<MapRule>& rules);
    // shared by the map canvas, layer compositor, both PNG exports, and the
    // CALENDAR — the rules engine is cross-view (territory.md)
    static std::vector<MapRule> parse_view_rules_of(const maiz::SceneNode& view);
    static std::string rule_expr_of(const MapRule& r);

    // per-view positions: channel read with copy-on-write fallback to main
    static std::string view_geo(const maiz::SceneNode& n, const std::string& ch);
    static std::string geo_field_for(const std::string& ch); // "geo"/"geo_<ch>"
    std::vector<std::string> channel_fields; // geo_<ch> registered on glyphs

    // ── the physics connections view (Gephi-class, not the node editor) ─────
    std::map<std::string, ImVec2> phys_pos; // layout space positions
    float phys_temp = 0;                    // >0 = still settling
    int phys_total_iters = 0, phys_done_iters = 0;
    std::string phys_drag;                  // node being dragged
    ImVec2 phys_cam{0, 0};                  // pan offset
    float phys_zoom = 1.0f;
    void phys_reset();                      // (re)seed + start settling
    void draw_physics_view(float body_h);

    // ── settings (config tier: logged, persisted with the org) ──────────────
    bool show_settings = true;   // the Settings dock window
    float ui_scale = 1.20f;      // config ui.scale
    bool map_show_prox = false;  // config ui.map_proximity — derived spatial
    float map_prox_m = 500.0f;   // config ui.map_proximity_m   links (hidden
    void draw_settings();        //                              by default)
    bool time_prox = false;      // config ui.time_proximity — derived TEMPORAL
    float time_prox_days = 14;   // config ui.time_proximity_d  links (physics)
    bool gui_anim = true;        // config ui.animations — drag-grow &c (advanced)
    // ── visual-effects toggles (author 2026-08-03: granular + opt-out; the
    // UI/UX phase's 2026-feel polish is expected but costs compute, so each
    // effect is separately switchable — "Performance mode" clears them all) ──
    bool fx_shadows = true;    // config ui.fx.shadows    — drop shadows on panels/cards
    bool fx_highlights = true; // config ui.fx.highlights — accent hover/selection tint
    bool fx_blur = true;       // config ui.fx.blur       — translucent/blur affordances
    bool show_legacy = false;    // config ui.show_legacy — the old block canvas &c
    // base map (shared across views): which tile source + its color treatment.
    // These are GLOBAL config (the base map is singular — "for us, google maps")
    int basemap_src = 0;              // config ui.basemap (index into kBaseSources)
    float basemap_brightness = 1.0f;  // config ui.basemap_brightness (0.3..1)
    float basemap_fade = 0.0f;        // config ui.basemap_fade (0 color .. 1 gray)

    // ── window management (author: every panel closable, restorable) ────────
    bool sec_open[4] = {true, true, true, true}; // Data/Builder/Antfarm/Map
    bool win_calendar = true, win_console = true;
    // ── Notes: its own bare-bones tab (author 2026-08-03). Notes leave the Data
    // sidebar for a dedicated section — just a list of `note` runes + a text
    // editor for now. A real Notes engine (markdown/Obsidian interop, Allomone
    // text mechanics) is a far-future build → Q22. ──────────────────────────
    bool win_notes = true;
    void draw_notes_body();
    char notes_buf[16384] = {};   // the selected note's staged text
    std::string notes_edit_for;   // which note notes_buf currently holds
    char notes_search[128] = {};       // Notes: name substring search
    std::vector<FilterTerm> notes_filter_terms; // Notes: tag-filter chips (reused)
    int notes_filter_join = 0;         // 0 AND, 1 OR
    char notes_filter_add[96] = {};    // the "+ filter" picker buffer
    char notes_filter_expr[256] = {};  // compiled tag-grammar expression
    char notes_tag_input[64] = {};     // add-a-tag picker for the selected note

    // ── ALLOMONE — the rules engine (okf/concepts/allomone/). Since 2026-08-10
    // this is Void Maiz's language and composition engine, with Hormiga's
    // domain vocabulary in hormiga_allomone.hpp: several independent sources
    // each state what should be true, a lattice merge composes them, and a
    // genuine disagreement across sources surfaces as ⊤ for a human to settle
    // rather than being decided by evaluation order. Derive-only. ────────────
    bool win_allomone = true;
    void draw_allomone_body(); // the Allomone tab (scripts + conflicts)
    void draw_allomone_scripts_pane(const maiz::Scene& as); // left: the sources
    void draw_allomone_conflicts_pane();                    // the ⊤ inspector
    void draw_allomone_reference_pane();                    // laws + predicates
    int allo_tab = 0;                     // 0 scripts, 1 conflicts, 2 reference
    std::string allo_explain_cell;        // "<subject>\t<property>" being explained
    // The whole derivation, rebuilt every reproject: subjects, each source's
    // ConstraintMap, the merged cells, and per-script diagnostics. Everything
    // the tab shows is read out of THIS rather than re-derived, so the UI can
    // never disagree with what the cards are painted from.
    hormiga::allomone::Derivation allo_derived;
    // ── UNSHIPPED 2026-08-11: the legacy Allomone tab and the Allo Dev editor.
    // Both windows are gone from the application — Allomone is Void Maiz's
    // language, the new tab is the only editor, and two extra surfaces for a
    // dialect nobody should write in taught the wrong thing. The code is kept
    // and still COMPILES on demand (-DHORMIGA_LEGACY_ALLOMONE=1); see the big
    // note above the block in app.cpp for why the flag rather than deletion.
    //
    // The legacy INTERPRETER is unaffected: it still derives as one producer
    // (allo_legacy_sources), and the new tab can enable, disable and delete
    // `script` runes. Unshipping an editor must not strand data.
#if HORMIGA_LEGACY_ALLOMONE
    bool win_allomone_legacy = false;
    void draw_allomone_legacy_body();
    bool win_allomone_dev = false;
    void draw_allomone_dev_body();
    bool code_editor(const char* id, std::string& text, const ImVec2& size); // the custom editor
    // custom-editor state (per the one dev editor instance)
    std::string ce_text;              // the buffer being edited
    std::string ce_for;               // which script ce_text holds
    int ce_caret = 0;                 // caret char index
    int ce_sel = -1;                  // selection anchor (-1 = no selection)
    float ce_blink = 0;               // caret blink phase base
    int ce_color_pos = -1;            // byte offset of the "#hex" a wheel is editing
    int ce_date_pos = -1;             // byte offset of the "YYYY-MM-DD" a picker is editing
    int ce_date_y = 2026, ce_date_m = 1; // the month the date picker is showing
    maiz::Scene ce_data;              // data mantle scene (for inline avatars/preview)
    bool ce_dirty = false;            // unsaved edits (commit on Save or blur)
    char allo_script_buf[65536] = {}; // the selected script's staged source text
    std::string allo_script_for;      // which script allo_script_buf holds
    std::string allo_color_edit;      // the hex currently being edited ("" = none)
    float allo_pick[3] = {0, 0, 0};   // the color wheel's working RGB
    std::string allo_sel;             // the selected legacy script ("" = none)
    bool boot_focus_allodev = false;  // HORMIGA_SECTION=allodev (dev editor)
#endif
    // The Void-Maiz-dialect tab's own editor state, kept separate from the
    // legacy tab's above so a half-typed script in one cannot be committed onto
    // the other. Editing is STAGED: the buffer is local, and one `set` command
    // lands on commit (Ctrl+Enter or focus leaving after an edit) — one command
    // per gesture, never per keystroke. Getting that granularity wrong is
    // indistinguishable from the feature being broken.
    // Upstream's editor (voidmaiz/code.hpp), adopted 2026-08-11 in place of a
    // transparent-InputText-plus-overlay of our own. It owns its layout, which
    // is what buys ctrl+wheel zoom with level-of-detail, right-click explain,
    // hover, completion, and inline widgets — a colour literal that IS a colour
    // wheel where it sits. All of it shipped with Allomone; we were reinventing
    // a worse version and getting the glyph alignment wrong doing it.
    maiz::CodeEditorState allo_editor;
    // `kind → renderer`, the same binding a field editor uses, so the picker in
    // a script is the picker on a contact. The kit ships color and date.
    maiz::CodeWidgetRegistry allo_code_kit = maiz::CodeWidgetRegistry::defaults();
    std::string allo_src_for;         // which allo-script rune the buffer holds
    std::string allo_src_sel;         // the selected allo-script rune ("" = none)
    // ── the RENDER SEAM ──────────────────────────────────────────────────────
    // The derivation runs each reproject, INDEPENDENT of the active tab's
    // scene, and is cached per SURFACE here. A rule states something
    // domain-neutral (`color`) or surface-specific (`map-color`), and each
    // surface reads its own resolution — one annotation, many interpretations,
    // which is Hormiga's own domains concept expressed as a property prefix
    // over Void Maiz's opaque strings (okf/concepts/allomone/domains.md).
    //
    // Cached rather than resolved per draw call for two reasons, and the second
    // is the real one: the map's marker loop stays O(1) per node, and every
    // surface provably reads the SAME derivation, so the Allomone tab cannot
    // disagree with what the cards are painted from.
    struct AlloStyle {
        unsigned rgba = 0;
        bool has_color = false;
        std::string icon, label;
        std::vector<std::string> badges, notes;
        double weight = 0, priority = 0;
        bool hide = false; // `web-hide "1"` — the OUTPUT domain's one effect
    };
    // surface prefix ("card"/"map"/"cal"/"web") → rune name → what was derived.
    // Absent = nothing derived, which is also what a CONFLICTED cell produces:
    // a contested value must never reach a renderer.
    std::map<std::string, std::unordered_map<std::string, AlloStyle>> allo_style;
    const AlloStyle* allo_style_for(std::string_view domain,
                                    const std::string& rune) const;
    // Does the OUTPUT domain exclude this rune from every query-backed block?
    // Consulted by the newsletter, the site and the Builder preview alike.
    bool allo_web_hidden(const std::string& rune) const;
    std::unordered_map<std::string, unsigned> allo_colors; // card colours (hot path)
    std::map<std::string, std::vector<std::string>> allo_errors; // script → parse errors
    void refresh_allo_rules();            // re-derive → the render-seam caches
    std::vector<std::string> allo_published_runes(); // what `published ""` answers
    void allo_cmd(const std::string& c);  // run a command in the allomone mantle
    std::string rule_edit_for;            // which rule the editor state holds
    std::vector<FilterTerm> rule_edit_terms; // the selected rule's condition chips
    int rule_edit_join = 0;               // AND/OR
    char rule_filter_add[96] = {};        // the rule condition's "+ filter" buffer
    char rule_cond_expr[256] = {};        // compiled condition (mirrors the chips)
    float rule_edit_color[3] = {0.24f, 0.45f, 0.80f}; // the action color
    // Derive-only: the winning rule color for a node (true = a rule applies).
    bool rule_color_for(const maiz::SceneNode& n, unsigned& out_rgb) const;
    bool boot_focus_allomone = false;     // HORMIGA_SECTION=allomone

    // ── THE CIVIC RECORD (okf/concepts/projects/civic-record.md). A window rather than a
    // section: it is a *reading* surface over runes the Data tab already edits,
    // and its whole reason to exist is that the interesting facts are not on any
    // one rune. "What did 3.3.1 say in 2024" is a composition of dated
    // assertions; "who sat in ward 2" is a query over terms; both are invisible
    // in a list of runes and obvious here.
    bool win_civic = false;
    void draw_civic_body();
    // Import a Void Reyna command transcript as one undoable batch. The seam
    // with ../VoidReyna: it emits verbs, we replay them, nothing is linked.
    void import_reyna_transcript();
    std::string reyna_last_import;
    std::string civic_policy_sel;     // the policy being read ("" = first)
    std::string civic_provision_sel;  // the provision whose history is open
    char civic_date[16] = {};         // the as-of date; empty = today
    bool civic_show_conflicts = true;
    // The resolution for `civic_date`, rebuilt when the date or model changes —
    // never cached across a reproject, because it is a projection.
    std::string civic_resolved_for;   // which date civic_resolved holds
    hormiga::civic::Resolved civic_resolved;

    // ── the CALENDAR (okf/concepts/sections/calendar.md): dated runes on a time grid,
    // styled by the SAME rules engine as the map (shared icons/colors) ───────
    int cal_year = 0, cal_month = 0, cal_day = 0; // anchor date (0 = today)
    int cal_mode = 0;         // 0 month, 1 week, 2 three-day, 3 agenda (list)
    int cal_kind = 0;         // 0 all, 1 events only, 2 incidents only (C4a)
    char cal_filter[128] = {}; // tag query; "" = no filter (C4a privacy)
    char cal_filter_tag[48] = {}; // the tag_picker buffer for the filter builder
    std::string cal_view;     // C4d: the active saved calendar view ("" = ad-hoc)
    char cal_view_name[48] = {}; // "Save as view" name buffer
    void cal_apply_view(const maiz::SceneNode& v); // load a calview's settings
    void draw_calendar_toolbar();                 // views, nav, filter, quick-add
    void draw_calendar_body();                    // the grid + selection
    void export_calendar_png();                   // static export (newsletter)
    // C1 creation & manipulation state (okf/concepts/sections/calendar.md toolset)
    int cal_ctx_y = 0, cal_ctx_m = 0, cal_ctx_d = 0; // day under a context menu
    std::string cal_daytag_date; // "YYYY-MM-DD" of the day being tagged ("" none)
    bool cal_daytag_open = false; // request to open the day-tag popup next frame
    std::string cal_ctx_entry;   // entry under the time-grid context menu
    int cal_drag_col = -1;       // drag-to-create: which day column (-1 = none)
    float cal_drag_t0 = 0, cal_drag_t1 = 0; // drag range in hours
    // C1d/C1e: drag a timed block to MOVE (day+time) or RESIZE (bottom edge)
    std::string cal_move;        // the block being moved/resized ("" = none)
    bool cal_resize = false;     // true = resizing end_time; false = moving
    float cal_grab_t0 = 0, cal_grab_t1 = 0, cal_grab_off = 0; // block times @ grab
    int cal_grab_col = 0;        // the block's day column at grab (stays valid mid-drag)
    // UI/UX pass 2026-09-10 (okf/concepts/sections/calendar-roadmap.md C-track)
    int cal_allday_open = -1, cal_more_day = -1; // expanded all-day col / month cell
    char cal_quick[160] = {}, cal_jump[16] = {}; // C1f quick-add, C2c jump-to-date
    bool cal_full_day = false;   // time grid: the whole day, not the fitted range
    bool cal_quick_focus = false, cal_jump_open = false; // focus/open next frame
    // mint a dated rune (event/incident) on a day, optionally timed (t in
    // hours, snapped) — ONE commit, selected for immediate editing
    void cal_new_dated(const char* glyph, int y, int m, int d, float t0 = -1,
                       float t1 = -1, const char* title = nullptr);
    // dated runes for a given day, styled: name/glyph + resolved icon/color
    struct CalEntry {
        const maiz::SceneNode* node;
        const char* icon;  // FA glyph or null
        unsigned col;
        bool incident;     // renders visibly distinct (author directive)
    };
    std::vector<CalEntry> cal_entries_on(int y, int m, int d) const;

    // map context-menu search (place-existing, #6) + PNG export. Returns the
    // written path ("" on failure); announce=false renders silently (the email
    // domain's map block — no toast, no auto-open mid-render)
    char ctx_search[64] = {};
    std::string export_map_png(const std::string& view_name,
                               bool announce = true);
    // a reusable marker action menu (shared by the map right-click AND the "on
    // this map" list — author: the list should act like the map). Emits menu
    // ITEMS only; the caller owns BeginPopup/EndPopup. Queues to pending_cmds.
    void marker_menu_items(const maiz::SceneNode& mk);
    float marker_anim = 0;              // 0..1 grow factor for the dragged marker
    // box-select (shift-drag over empty map picks everything inside)
    bool map_box_active = false;
    ImVec2 map_box_start{0, 0};
    // map CONFIG (author #6): view-level display knobs, stored on the view rune
    // so they persist and shape the PNG export. INTERNALLY view fields, surfaced
    // like settings. label_scale 0 => use default; show_labels default on.
    float view_label_scale(const maiz::SceneNode* v) const; // 1.0 default
    bool view_show_labels(const maiz::SceneNode* v) const;   // true default
    bool view_no_overlap(const maiz::SceneNode* v) const;    // true default
    unsigned view_label_color(const maiz::SceneNode* v) const; // dark default
    // per-view LAYER treatment (ask 3): how this view looks when it composites
    // as a ghosted layer under another. opacity default 0.43, brightness 1.0.
    float view_layer_opacity(const maiz::SceneNode* v) const;
    float view_layer_brightness(const maiz::SceneNode* v) const;
    char tag_pick_input[64] = {}; // the tag-adding search picker buffer (#4)
    // a tag entry that searches EXISTING tags as you type (author #4). Returns a
    // chosen/typed tag (without the sigil) or "" ; clears the buffer on commit.
    std::string tag_picker(const char* id, char* buf, size_t bufsz,
                           const char* hint = "add a tag...");
    void new_rune(const std::string& glyph);  // + New: mint, tag, select
    std::string render_preview(std::string_view lang); // email domain → HTML path
    std::string render_site(std::string_view lang);    // web domain → site/ folder
    /* THE PUBLISHED SUBSET AS DATA, not as pages (2026-08-21).
     *
     * The same clearance gate `render_site` uses — literally the same function,
     * `render/published.hpp` — emitted as rows instead of HTML, so that a live
     * site can read the directory without the whole site being rebuilt and
     * redeployed every time a contact changes. See okf/concepts/platform/data-planes.md.
     *
     * Writes `site/index/directory.json` and returns its path. It lands INSIDE
     * `site/` on purpose: an ordinary deploy carries it with no new machinery,
     * and it is also small enough to push on its own, which is the whole point.
     * Deterministic — no timestamp — so it hashes stably and a push can skip an
     * unchanged index. */
    std::string publish_index();
    /* Write one live directory's cards to `site/index/dir-<block>.html`.
     * Defined beside `publish_index` in render/index.cpp because a refreshable
     * fragment is a published ARTIFACT, not part of building a page — the page
     * renderer hands it the bytes it already produced. Returns false and logs a
     * warning on failure; never fatal, because the page carries the same cards
     * and is correct without it. */
    bool write_live_fragment(const std::string& block, const std::string& html);
    // copy a referenced local asset into site/assets/; return its site-relative
    // href ("" if the source is missing). Self-hosting: the site owns its media.
    std::string stage_site_asset(const std::string& rel);
    /* Stage `custom.css` from beside the database into `site/`, if there is
     * one, and say whether the page should link it. The operator's two lines of
     * CSS, the same way `fonts/` already takes the organization's own faces —
     * see render/assets.cpp for why this is a file and never a model field. */
    bool stage_custom_css();
    /* Say so when the organization's runes are in a mantle no block query
     * reads. Silent unless the data mantle is empty AND data-shaped runes exist
     * somewhere else — see render/assets.cpp (field report D6). */
    void warn_if_data_is_elsewhere(const maiz::Scene& data);
    /* A downscaled JPEG beside a staged image, for gallery tiles. "" = use the
     * original (already small, or the decode failed). The lightbox always keeps
     * the original — "see the flier full size" is why a flier is on the page. */
    std::string site_thumb(const std::string& staged_rel, int max_w);

    // ── the data spine (phase C) ────────────────────────────────────────────
    std::unique_ptr<hormiga::Storage> storage; // SQLite: where the org lives
    std::vector<std::string> pending_cmds;     // deferred (e.g. tag suggestions)
    int import_kind = 0;                       // Import CSV popup: palette index

    // ── images (phase C: display + the opt-in ImgBB Output holiday) ─────────
    std::unordered_map<std::string, HostTexture> tex_cache; // by field path
    HostTexture texture_for(const std::string& path); // resolve + cache
    int tex_decodes_this_frame = 0; // per-frame decode budget (boot-stall fix)
    // ── avatars (UI/UX phase #1): every entity shows a face. The best local
    // photo (avatar → image_url → an image's own path), else a procedural
    // placeholder — a hash-colored disc with the initials. ──────────────────
    std::string avatar_path(const maiz::SceneNode& n) const; // "" = none, draw placeholder
    void draw_avatar(const maiz::SceneNode& n, ImVec2 center, float radius);
    maiz::PathBrowseFn browse_ingest;   // dialog + ingest; shared by kinds
    std::string imgbb_key;              // secret; from vault or plaintext; "" = off
    void publish_image(const std::string& rune); // effect publish → url field

    // ── the credential vault (.miga v2; okf/concepts/platform/security.md) ───────────
    hormiga::Vault vault;
    enum class VaultModal { None, Unlock, Create };
    VaultModal vault_modal = VaultModal::None;
    char pass_buf[128] = {};
    char pass_buf2[128] = {};
    std::string vault_msg;             // modal feedback ("wrong passphrase", …)
    bool secrets_locked = false;       // a vault exists but isn't unlocked yet
    std::filesystem::path vault_path() const;
    void load_secrets();               // populate imgbb_key from vault/plaintext
    void draw_vault_modal();           // the passphrase prompt (unlock/create)

    /* ---- UPDATES (src/update/update.hpp; the ImGui half is ui/updates.cpp) ---
     *
     * THE APP HOLDS NO DECISIONS, only what a frame needs to draw. Whether an
     * update exists, whether the person has been asked, whether a download
     * matched its checksum -- all of it is `hormiga::update`, which is
     * view-free and reachable in full from `voidhormiga-cli update`. What lives
     * here is the answer it gave and which modal is open.
     *
     * NOTHING HERE TOUCHES THE NETWORK ON ITS OWN. `updates_boot()` reads a
     * preferences file and does one of three things: open the permission modal
     * (never asked), start a check (asked, answer yes), or nothing at all
     * (answer no). Somebody who said `never` gets one line in Settings and no
     * traffic, ever. That is the first half of `void.json`'s "no silent
     * updates", and it is the half that is easy to lose -- a check is a network
     * request a person did not make.
     *
     * THE ARTIFACT IS CARRIED AS STRINGS rather than as an `update::Release`,
     * so `update/update.hpp` stays out of a header the whole tree includes.
     * `ui/updates.cpp` reassembles one where it needs it. Four strings against
     * a dependency in every translation unit is the right side of that trade.
     *
     * The check runs OFF THE UI THREAD for the reason every network call here
     * does: a `curl` on the frame thread is a frozen window on a bad
     * connection, and the first impression of an update prompt must not be that
     * the application hung. `UpdateJob` is separate from `Job` above because
     * that one exists to land dispatcher commands and this one changes no
     * model at all -- an update is a fact about the installation, not the org. */
    enum class UpdateModal { None, AskPermission, Offer, Failed };
    struct UpdateJob {
        std::thread worker;
        std::atomic<bool> finished{false};
        // Written by the worker; read by the main thread ONLY after `finished`,
        // which is the whole of the synchronisation and is why there is no mutex.
        bool ok = false;
        bool available = false;
        bool skipped = false;
        std::string error, summary, version, latest;
        std::string art_url, art_file, art_sha256;   // the platform's artifact
        long long art_bytes = 0;
        std::string downloaded;   // set by an install job: a VERIFIED installer
        ~UpdateJob() { if (worker.joinable()) worker.join(); }
    };
    UpdateModal update_modal = UpdateModal::None;
    std::unique_ptr<UpdateJob> update_job;   // a check or a download in flight
    bool update_installing = false;          // which of the two it is
    /* Was the check asked for by a person? A STARTUP check that could not
     * reach the feed is not news -- putting a modal in front of an
     * organization because their cafe Wi-Fi is captive is how you train
     * people to dismiss the one that matters. It goes to the log instead. */
    bool update_by_hand = false;
    bool update_have_offer = false;
    std::string update_summary;   // `describe(offer)` -- exactly what the modal shows
    std::string update_version;   // the version being offered ("" = none)
    std::string update_latest;    // the feed's `latest`, even when we are current
    std::string update_error;     // the last failure, verbatim
    std::string update_file;      // a verified installer waiting to be run
    std::string update_art_url, update_art_file, update_art_sha256;
    long long update_art_bytes = 0;
    /* A CACHED VIEW OF `updates.json`, not a second copy of the truth. The
     * Settings block draws four values out of that file, and Settings is open
     * by default -- reading and parsing a file on disk once per frame is not a
     * thing to do to somebody's laptop. `updates_reload_prefs()` refreshes
     * these after every write and after a check; the file stays authoritative
     * and nothing here is ever written back from these fields. */
    int update_pref_ask = 0;      // 0 unasked, 1 never, 2 startup
    std::string update_pref_skip, update_pref_last, update_pref_feed;
    void updates_reload_prefs();
    void updates_boot();                // once, at the end of init()
    void updates_check(bool by_hand);   // fetch + parse + decide, off-thread
    void updates_install();             // download + verify, off-thread, then launch
    void updates_drain();               // join a finished job (called from frame())
    void draw_update_modal();           // the prompt, and it is never skippable
    void draw_update_settings();        // the block inside the Settings window
    void import_rescue_effect(); // Supabase node's Import now → one batch
    void derive_date_tags();     // temper: event date → month:/season: tags
    bool run_temper_next = false; // queue a date-tag pass after an import
    void mirror_images_effect(); // cloud URLs → local bytes (publish's twin)
    int mirror_count = -1;       // mirror.json entries; -1 = not read yet

    // ── background jobs (long, non-dispatcher I/O — network downloads) ──────
    // The worker does PURE I/O and NEVER touches the core (the dispatcher
    // stays the single door — all model change lands back on the main thread
    // as commands via `results`). Progress is atomic so the UI animates a bar.
    struct Job {
        std::string label;
        std::atomic<int> done{0};
        std::atomic<int> total{0};
        std::atomic<bool> finished{false};
        std::thread worker;
        std::vector<std::string> results; // commands for the main thread to dispatch
        std::string summary;              // toast text, filled by the worker
        ~Job() { if (worker.joinable()) worker.join(); }
    };
    std::unique_ptr<Job> job;
    void draw_job_overlay(); // the progress panel (main thread)
    std::vector<maiz::Scene> project_all_mantles(); // `mantles` verb → scenes
    void run_csv_import(const std::string& glyph);  // pick file → ONE batch
    std::string ingest_asset(const std::string& src, bool quiet = false);
    /* Bring a file in AND mint the `image` rune for it — the author's rule
     * (2026-09-02): "Uploading a NEW image should redirect to creating a new
     * image asset." Returns the site-relative path, "" on failure. */
    std::string ingest_image_rune(const std::string& src);
    /* A popup that picks one of the organization's existing images, returning
     * its `path`. "" = nothing picked this frame. Call `ImGui::OpenPopup` with
     * the same id first. Shared so branding and content pick images the same
     * way. */
    std::string org_image_picker(const char* popup_id, const char* heading);
    char img_pick_search[64] = {}; // filter box inside that popup

    std::filesystem::path org_file() const;
    std::filesystem::path db_file() const;
    std::filesystem::path assets_dir() const;
    void install_host(); // log sink + effect handler (survive core replacement)
    void apply_theme();
    void read_view_config();
    void flush_panels();
    void reproject();
    maiz::Result dispatch_and_reproject(const std::string& cmd);
    void do_save();
    // ── the .miga v3 DATABASE bundle (okf/concepts/platform/miga-format.md) ──────────
    std::string cur_miga;        // the active database bundle path ("" = none yet)
    bool win_share = false;      // the "Share database" (in-development) window
    // ── Data Tools (author 2026-08-03): a detached window holding the Data
    // tab's utilities (CSV import, date-tag temper, …) — declutters the tab's
    // toolbar. Sits behind a "Data Tools" button beside the connections view. ─
    bool win_data_tools = false;
    void draw_data_tools_window();
    // a soft drop shadow behind a rectangle (UI/UX phase; gated by fx_shadows).
    // Drawn on the CURRENT window's draw list, so call before the child/content.
    void panel_shadow(const ImVec2& mn, const ImVec2& mx, float rounding = 6.0f);
    // ── the BUSY overlay (author 2026-07-24: loading bars for save/open/&c) ──
    // A heavy op is DEFERRED one frame so the "working…" overlay paints before
    // the main thread blocks on it. progress<0 = indeterminate spinner.
    std::string busy_label;
    float busy_progress = -1.0f;
    std::function<void()> busy_action;   // runs at the START of the next frame
    void run_busy(const std::string& label, std::function<void()> action);
    void draw_busy_overlay();
    void new_database();         // a fresh, empty database (new working copy)
    // reset the working copy's local files so databases stay ISOLATED: clear
    // tiles/site/exports/preview caches (re-derivable) and — when switching
    // databases — assets/ too (each .miga carries its own). Invalidates the
    // texture cache so freed image paths don't return stale GPU textures.
    void reset_working_copy(bool clear_assets);
    void save_database();        // pack → cur_miga (or Save As if none); backs up
    void save_database_as(const std::string& path);  // pack → chosen .miga path
    void open_database(const std::string& path);     // unpack + reload the core
    void reload_from_state(const std::string& state); // replace core, re-seed host
    void draw_share_window();    // the LAN/P2P placeholder
};
