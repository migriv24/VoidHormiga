/* app_internal.hpp — what the app's translation units share.
 *
 * WHY THIS EXISTS. `app.cpp` was 12,696 lines: the shell, six sections, and the
 * helpers all of them lean on, in one file. The split (Q30a, 2026-08-17) makes
 * each SECTION its own translation unit and leaves `app.cpp` as the **shell** —
 * lifecycle, the host seams, projection, persistence, the frame. That is not a
 * new architecture; it is the one okf/concepts/sections/workspace-and-sections.md already
 * describes, finally visible in the file listing.
 *
 * The boundary is deliberately NOT a new abstraction. Every section is still a
 * set of `HormigaApp::` methods declared in `app.hpp`, sharing one struct and
 * one dispatcher, because inventing a "section interface" to justify the split
 * would have added a seam nothing needed and made the one-sync rule harder to
 * see rather than easier. What moved is text. What did not move is the design.
 *
 * WHY THE BODIES ARE NOT HERE. Compiled once in app_shared.cpp rather than
 * inlined into all seven units. Seven copies of every helper is waste, and the
 * rule below is easier to follow when a body has one home.
 *
 * A CORRECTION, because the first version of this note got it wrong. The split
 * was followed by a link that died SILENTLY — no diagnostic at all — and it was
 * diagnosed here as PE's 16-bit section ceiling, with measurements (44,341
 * sections as one unit, 81,867 as seven) and a `-Og` fix. The measurements were
 * real and **the diagnosis was not**: retested 2026-08-18 by disabling `-Og`,
 * the tree links fine at `-O0`. The actual cause was one ordinary undefined
 * symbol — `read_site_theme`, whose body was lost while moving it here — and it
 * was fixed in the same sitting, which is how it took the credit for the wrong
 * thing.
 *
 * What made it convincing was a control build of the pre-split file that linked;
 * that was consistent with the section theory and equally consistent with the
 * missing symbol, since the control had the function defined in the same unit.
 *
 * THE REAL LESSON, which is worth more than either diagnosis: **the gcc driver
 * swallows ld's stderr in this environment.** Every link failure prints
 * `collect2: error: ld returned N exit status` and nothing else, whatever the
 * cause. Running `ld.exe` directly prints the real errors — that is how the
 * headless CLI's two missing stb symbols were found in a minute, after the same
 * silent failure had cost an afternoon.
 *
 * THE TEST FOR WHAT BELONGS HERE: more than one unit needs it, and it does not
 * know about HormigaApp. Anything used by a single section stays `static` in
 * that section's file, where a reader can see at a glance that nothing else
 * depends on it.
 */
#pragma once

#include "app/app.hpp"
#include "domain/import.hpp"
#include "domain/doc_actions.hpp"
#include "domain/map_actions.hpp"
#include "domain/hormiga_allomone.hpp" // the Void Maiz language + merge (`allo-script`)
#include "domain/temper.hpp"
#include "gis/source.hpp"   // the map engine: worlds, projections, metrics

#include "stb_image.h" // decls only (impl lives in main_desktop.cpp) — the
                       // map PNG export decodes cached tiles CPU-side

#include "voidmaiz/face.hpp"
#include "voidmaiz/gesture.hpp"
#include "voidmaiz/inspector.hpp"
#include "voidmaiz/project.hpp"

#include "imgui.h"
#include "imgui_internal.h" // DockBuilder* — seed the default dock arrangement


#include "IconsFontAwesome6.h" // vendored codepoints (vendor/fonts, zlib)

#include <cstring>

#include <algorithm>
#include <climits>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

inline const char* kDataMantle = "demo-org";
inline const char* kIssueMantle = "issue-demo"; // the default/legacy document
inline const char* kAntfarmMantle = "antfarm";
inline const char* kAlloMantle = "allomone"; // Allomone rules live in their own mantle
inline const char* kCivicMantle = "civic";   // the civic record (policies, terms)

// the FONT PALETTE — indices shared by the heading and body axes (Style tab +
// site CSS). The first three are VENDORED, embedded webfonts (OFL, shipped in
// site/fonts/ via @font-face — see site_css); the rest are OS font stacks (no
// download). Author 2026-07-31: "more fonts" — real typefaces, identical on
// every visitor's screen.
inline const char* kFontStacks[] = {
    "'Inter',system-ui,-apple-system,\"Segoe UI\",Roboto,Helvetica,Arial,sans-serif",
    "'Source Serif 4',Georgia,\"Times New Roman\",serif",
    "'Space Grotesk',\"Segoe UI\",system-ui,sans-serif",
    "system-ui,-apple-system,\"Segoe UI\",Roboto,Helvetica,Arial,sans-serif",
    "Georgia,\"Times New Roman\",\"Iowan Old Style\",serif",
    "ui-monospace,\"Cascadia Code\",Consolas,\"Courier New\",monospace"};
inline const int kNumFonts = (int)(sizeof(kFontStacks) / sizeof(*kFontStacks));
inline const char* kFontLabels[] = {"Inter (sans)", "Source Serif (serif)",
                                    "Space Grotesk (display)", "System sans",
                                    "System serif", "Monospace"};

// ── small helpers ────────────────────────────────────────────────────────────

std::string json_str(const std::string& s);

/* Wrap a JSON value as ONE dispatcher argument. The arg tokenizer
 * (../VoidCore/core/src/dispatch/args.c) STRIPS bare quote characters, so raw
 * JSON reaches `setjson` with its "quotes" gone → cJSON rejects it → it lands
 * as a broken string (the vanished-rules bug, 2026-07-22). Inside SINGLE
 * quotes the tokenizer keeps every inner `"` literal and spaces intact; a
 * literal `'` escapes as `\'`. This is the sanctioned way to pass structured
 * JSON through the command line. */
std::string json_arg(const std::string& json);

/* A field SET VIA setjson comes back from field_value in its JSON-escaped form
 * (\n, \", …). Decode it to the real text for editing/parsing (multi-line
 * bodies: Allomone scripts, note text). Wrapping in quotes and JSON-parsing
 * undoes exactly the escaping the value carries; on any surprise, pass through. */
std::string unjson_str(const std::string& raw);

/* A commit-on-release slider over a value that lives in the MODEL (a rune
 * field or config read back each frame). The naive pattern — fresh local from
 * the store every frame + IsItemDeactivatedAfterEdit — SNAPS BACK: ImGui does
 * not apply mouse movement on the release frame, so the local still holds the
 * stale stored value when the commit fires (every slider was doing this,
 * 2026-07-22). The in-drag value must PERSIST between frames; ImGui's
 * per-window storage holds it while editing, and the stored value shows the
 * rest of the time. Returns true on the commit frame with *out = final. */
bool slider_field(const char* label, float stored, float mn, float mx,
                         const char* fmt, float* out);

// ── base-map SOURCES: now `gis::MapSource` (2026-08-21) ────────────────────
//
// `BaseSource` was five fields and six hidden assumptions: web-mercator, z/x/y
// URLs, 256-pixel tiles, zoom 0..19, longitude wrapping, and haversine metres.
// Every one of those is now a FIELD on `hormiga::gis::MapSource`, defaulted so
// these three Earth styles behave exactly as before — the golden render is the
// proof — and overridable so an authored, non-Earth world can be described at
// all. That is the first thing the map builder needs and the reason `src/gis/`
// exists (okf/concepts/foundation/application-boundaries.md, Q42).
//
// The original note still holds and is why `labeled` is a property of a SOURCE
// rather than a toggle: for raster tiles, labels and POI icons are baked INTO
// the pixels — you cannot strip them from a tile, you switch to a style that
// never drew them. So "no labels" is a different source, not a filter, and the
// UI toggle picks between them.
//
// The names `BaseSource` / `kBaseSources` / `kBaseSourceCount` are kept as
// aliases: the map view refers to them in a dozen places and renaming those in
// the same step that moved the type would forfeit the ability to say the move
// changed nothing.
using BaseSource = hormiga::gis::MapSource;
inline const BaseSource* const kBaseSources = hormiga::gis::kBuiltinSources;
inline constexpr int kBaseSourceCount = hormiga::gis::kBuiltinSourceCount;

// ── marker styling: icons + colors ARE tags (icon:<name>, color:<name>),
// chosen from dropdowns rather than typed — the tag system stays the truth,
// the menu is just a friendlier front-end (author, 2026-07-22) ──────────────
/* ── A CLAMP THAT SURVIVES A WINDOW TOO SMALL FOR IT (2026-09-02) ────────────
 *
 * `std::clamp(v, lo, hi)` has a PRECONDITION: `hi >= lo`. Violating it is
 * undefined behaviour, and libstdc++ built with assertions on — which is how
 * this project builds — turns that into `abort()`. Not a wrong layout: the
 * process dies.
 *
 * It killed the application at boot, and the report was *"i double click the
 * desktop shortcut … it briefly opens for a bit (with the terminal) then it
 * closes."* The Data section computes its three pane widths as
 *
 *     side_w = clamp(frac * total, 110, total * 0.4)
 *     list_w = clamp(frac * rest,  150, rest - 120)
 *
 * and both bounds cross when the pane is narrow: the first at `total < 275`,
 * the second whenever `rest < 270` — and `rest` has a floor of 160, so the
 * second inverts *by construction* in a narrow window. The pattern is
 * everywhere in this codebase's layout code, because it is the natural way to
 * write "at least this readable, at most this share of the space", and it is
 * correct right up until the space cannot honour both.
 *
 * ── WHY `lo` WINS ───────────────────────────────────────────────────────────
 *
 * Every one of these is a MINIMUM READABLE SIZE and a MAXIMUM SHARE. When they
 * conflict there is no arrangement that satisfies both, and the useful answer
 * is the minimum: a 110px sidebar that overflows a 200px window can still be
 * read and dragged back, and ImGui clips it. Taking `hi` instead would collapse
 * the pane to nothing, which looks exactly like the crash it replaced.
 *
 * ── WHY A HELPER AND NOT `std::max(lo, hi)` AT EACH CALL ────────────────────
 *
 * Because the next call site will be written the same natural way by somebody
 * who has not read this comment, and the failure is a hard abort on somebody
 * else's screen size rather than a visible glitch on ours. One name to reach
 * for, one place that explains the rule. `tools/find_long.py` cannot catch this;
 * a grep for `std::clamp` with a computed upper bound can, and that is what
 * found the other two.
 */
template <class T>
inline T clamp_fit(T v, T lo, T hi) {
    return hi < lo ? lo : (v < lo ? lo : (hi < v ? hi : v));
}

/* ── AN ICON FOR EVERY GLYPH (2026-09-02) ────────────────────────────────────
 *
 * The author: *"i'd really like to get some icons in this application. like not
 * just emojis or something, but icons … especially for the builder, like little
 * icons next to the drag and drop button."*
 *
 * ── NOTHING WAS DOWNLOADED, AND THAT IS THE POINT ────────────────────────────
 *
 * The ask included *"if we can download some icon assets from somewhere"*, and
 * the answer is that both halves of it are already vendored and have been for
 * months, each for the surface it suits:
 *
 *   - **Font Awesome 6 Free Solid**, `vendor/fonts/fa-solid-900.ttf`, merged
 *     into the ImGui atlas in `main/desktop.cpp`. Right for a GUI: one glyph,
 *     one draw call, and it inherits the text colour and the DPI scale for
 *     free. It has only ever been used for map markers.
 *   - **Lucide**, `vendor/icons/lucide_icons.hpp`, as SVG path data. Right for
 *     a WEB page, where an icon font is a download that blocks first paint,
 *     renders as a box when it fails, and is invisible to a screen reader.
 *
 * So this table is not new capability. It is the GUI half of the icon story
 * finally being used past the map, and it needs no new dependency, no license
 * to add (both are already in THIRD-PARTY-NOTICES.md) and no network at all.
 *
 * ── WHY A TABLE AND NOT A FIELD ON THE GLYPH ─────────────────────────────────
 *
 * `block()` already takes nine parameters and a tenth would be a tenth thing to
 * get right at every call site. More importantly, an icon is a property of THIS
 * FRONT-END: the web renderer draws SVG, the newsletter draws nothing, and a
 * headless run has no atlas. Putting it in the glyph declaration would push a
 * desktop concern into the model that every other caller has to ignore.
 *
 * One table, in one place, that a person can read top to bottom and see whether
 * two things that should look different do. An unlisted glyph falls back to a
 * neutral square rather than to nothing, because a palette where some rows have
 * an icon and some have empty space reads as broken rather than as sparse.
 */
inline const char* glyph_icon(std::string_view glyph) {
    struct Row { const char* glyph; const char* icon; };
    static const Row kRows[] = {
        // ── document blocks (the Builder palette) ───────────────────────────
        {"hero", ICON_FA_PANORAMA},
        {"narrative", ICON_FA_PARAGRAPH},
        {"section_header", ICON_FA_HEADING},
        {"event_grid", ICON_FA_CALENDAR_DAYS},
        {"event_feature", ICON_FA_STAR},
        {"event_flier", ICON_FA_RECTANGLE_AD},
        {"image_grid", ICON_FA_IMAGES},
        {"job_grid", ICON_FA_BRIEFCASE},
        {"directory", ICON_FA_ADDRESS_BOOK},
        {"video", ICON_FA_VIDEO},
        {"audio", ICON_FA_MUSIC},
        {"download", ICON_FA_DOWNLOAD},
        {"footer", ICON_FA_GRIP_LINES},
        {"link", ICON_FA_LINK},
        {"quote", ICON_FA_QUOTE_LEFT},
        {"stat", ICON_FA_CHART_SIMPLE},
        {"divider", ICON_FA_MINUS},
        {"map_embed", ICON_FA_MAP},
        {"calendar_embed", ICON_FA_CALENDAR},
        {"page", ICON_FA_FILE_LINES},
        // ── the five kinds of thing, plus the ones that grew beside them ────
        {"contact", ICON_FA_USER},
        {"organization", ICON_FA_BUILDING},
        {"event", ICON_FA_CALENDAR},
        {"incident", ICON_FA_TRIANGLE_EXCLAMATION},
        {"job", ICON_FA_BRIEFCASE},
        {"image", ICON_FA_IMAGE},
        {"resource", ICON_FA_BOOK_OPEN},
        {"role", ICON_FA_BRIEFCASE},
        {"project", ICON_FA_CUBE},
        {"location", ICON_FA_LOCATION_DOT},
        {"note", ICON_FA_NOTE_STICKY},
        {"map", ICON_FA_MAP},
        {"deployment", ICON_FA_CLOUD_ARROW_UP},
        {"peer", ICON_FA_USERS},
        {"submission", ICON_FA_FILE_IMPORT},
    };
    for (const Row& r : kRows)
        if (glyph == r.glyph) return r.icon;
    return ICON_FA_SQUARE;
}

struct MarkerIcon { const char* tag; const char* label; const char* glyph; };
inline const MarkerIcon kMarkerIcons[] = {
    {"house", "House / building", ICON_FA_HOUSE},
    {"user", "Person", ICON_FA_USER},
    {"users", "Group", ICON_FA_USERS},
    {"flag", "Flag", ICON_FA_FLAG},
    {"star", "Star", ICON_FA_STAR},
    {"heart", "Heart", ICON_FA_HEART},
    {"warning", "Warning", ICON_FA_TRIANGLE_EXCLAMATION},
    {"school", "School", ICON_FA_SCHOOL},
    {"hospital", "Hospital", ICON_FA_HOSPITAL},
    {"food", "Food", ICON_FA_UTENSILS},
    {"bus", "Transit", ICON_FA_BUS},
    {"tree", "Park / nature", ICON_FA_TREE},
    {"briefcase", "Work / jobs", ICON_FA_BRIEFCASE},
    {"calendar", "Event", ICON_FA_CALENDAR},
    {"scale", "Legal / rights", ICON_FA_SCALE_BALANCED},
    {"pin", "Pin", ICON_FA_LOCATION_DOT},
};
struct MarkerColor { const char* tag; unsigned col; };
inline const MarkerColor kMarkerColors[] = {
    {"red", IM_COL32(200, 60, 50, 255)},   {"orange", IM_COL32(224, 130, 40, 255)},
    {"yellow", IM_COL32(212, 170, 40, 255)}, {"green", IM_COL32(70, 150, 80, 255)},
    {"teal", IM_COL32(50, 140, 140, 255)}, {"blue", IM_COL32(60, 110, 180, 255)},
    {"purple", IM_COL32(130, 90, 180, 255)}, {"pink", IM_COL32(210, 100, 150, 255)},
    {"brown", IM_COL32(138, 109, 59, 255)}, {"gray", IM_COL32(120, 120, 125, 255)},
};
std::string tag_value(const maiz::SceneNode& n, const char* ns);

// ── marker SHAPES (author 2026-07-24): a marker can be a circle (default), a
// traditional map PIN (teardrop, tip at the exact point), a square, or a
// diamond. Set by a `shape:` tag or a style rule; shared by the canvas, the
// PNG export, and the web widget so a styled marker looks the same everywhere.
enum class MShape { Circle, Pin, Square, Diamond };
inline const char* kMarkerShapes[] = {"circle", "pin", "square", "diamond"};
MShape shape_from(const std::string& s);
/* Draw a marker of shape `sh` at `s` (which is the anchor: the CENTER for
 * circle/square/diamond, the TIP for a pin). Fills with `col`, outlines with
 * `ring`; returns the point where an icon/label glyph should be centered. */
ImVec2 draw_marker_shape(ImDrawList* dl, ImVec2 s, float r, ImU32 col,
                                ImU32 ring, MShape sh);

// ── calendar date math (okf/concepts/sections/calendar.md; used by the Calendar tab,
// its exports, and the email/web calendar blocks) ───────────────────────────
inline const char* kMonthNames[] = {"January", "February", "March",    "April",
                                    "May",     "June",     "July",     "August",
                                    "September", "October", "November", "December"};
inline const char* kDowNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
bool cal_leap(int y);
int cal_dim(int y, int m);
int cal_dow(int y, int m, int d);
void cal_today(int& y, int& m, int& d);
void cal_add_days(int& y, int& m, int& d, int delta);
float cal_parse_hhmm(const std::string& s);
std::string cal_fmt_hhmm(float t);

bool parse_two(const std::string& data, float& a, float& b);

/* A field's value as plain text ("" when unset/null). */
std::string field_value(const maiz::SceneNode& n, std::string_view key);

/* The list row's subtitle: the first non-empty field, truncated. */
std::string subtitle(const maiz::SceneNode& n);

/* Whitespace tokenizer with double-quote grouping ("45.5, -122.6" is one
 * token) — the `map` verb front-end and the near-query both parse with it. */
std::vector<std::string> tokenize(const std::string& s);

bool contains_ci(std::string_view hay, std::string_view needle);

/* Turn a rune's slug name back into a display title ("oakshire-inspires-
 * benefit" → "Oakshire Inspires Benefit"). The import slugified titles into
 * names (Q2); this recovers a readable form for rendering. Imperfect on
 * acronyms, good enough for a newsletter. */
std::string humanize(const std::string& slug);

/* The document order of a block issue: the prev/next adjacency chain, then
 * any loose (unlinked) blocks after it. Shared by every domain renderer —
 * one block graph, many outputs (okf/concepts/sections/blocks-and-domains.md). */
std::vector<const maiz::SceneNode*> block_chain(const maiz::Scene& issue);

std::string html_escape(const std::string& s);

/* FNV-1a 64 over file bytes — a DEDUP FILENAME hash, not crypto (content
 * integrity/crypto arrives with libsodium; security concept §1 forbids
 * hand-rolled primitives for anything secret — this names files). */
unsigned long long fnv1a64(const std::string& bytes);

const char* month_name_now();

// ── the SITE THEME ──────────────────────────────────────────────────────────
//
// Shared because it has two readers with equal claim: the Output domain renders
// with it (section_web.cpp) and the Builder previews with it
// (section_builder.cpp). CONFIG is the authoritative source for both — the
// Style tab writes it on every change, and `config set theme.*` from the
// console or an agent reaches the same place — so the two front-ends cannot
// drift. That is the one-definition rule applied to a value rather than a verb.

struct SiteTheme {
    std::string accent, accent2, bg, ink;
    int preset = 0;   // 0 clean 1 soft(neumorphic) 2 bold(maximal) 3 editorial 4 glass
    int font = 0;     // HEADING font index into kFontStacks (see site_css)
    int bodyfont = 0; // BODY font index into kFontStacks
    int scale = 2;    // type scale: 0..4 → 0.90/0.95/1.00/1.08/1.18 root size
    int radius = 2;   // corner radius: 0..4 → 0/6/14/22/32 px
    int texture = 0;  // page texture: 0 none 1 dots 2 grid 3 hatch
    bool dark = true;
    /* THE ORGANIZATION'S OWN TYPEFACE (2026-08-20).
     *
     * `kFontStacks` is six compiled-in families, so an organization that has
     * chosen a face — a real decision, made once, printed on everything — could
     * not use it, and dropping a woff2 into `vendor/fonts/web` would ship a file
     * nothing referenced. A network named Mandali as its typeface and got Inter,
     * the nearest of the six.
     *
     * `theme.font_custom` names the family; the woff2 files live in a `fonts/`
     * folder beside the DATABASE, not in this repo. Same argument as
     * `deploy_cmd`: the thing that varies per organization is configuration, and
     * a new typeface must not need a new version of Hormiga. It becomes the
     * first family in the stack, so the chosen `kFontStacks` entry stays as the
     * fallback and nothing breaks if the file is missing. */
    std::string font_custom;
    // the org's woff2 filenames, filled by render_site when it stages them
    std::vector<std::string> font_files;

    /* ── LIGHT AND DARK PARTNERS, AND A CONTRAST FLOOR (2026-08-20) ──────────
     *
     * The author's ask: *"define 'lighter' colors and 'darker' colors … or
     * purposely define so that colors contrast each other. this is mostly for
     * text, so that light text doesn't appear over light backgrounds."*
     *
     * That is two features and they are worth keeping apart:
     *
     *   `accent_lite` / `accent_dark` are the PARTNERS — the same hue at two
     *   other lightnesses, for tints, hovers and chips. Empty means derived
     *   from `accent`, so an organization that picks one colour gets a coherent
     *   set and one that wants exact brand values may state all three.
     *
     *   `contrast` is the FLOOR, and it is the part that fixes the actual bug.
     *   The site had `color:#fff` written into a dozen rules that sit on
     *   `var(--accent)` — fine for a navy accent, invisible for a yellow one,
     *   and no amount of colour-picking by a volunteer can fix a hex that is
     *   compiled in. The renderer now COMPUTES the text colour for every
     *   generated background (WCAG relative luminance, the same arithmetic a
     *   browser devtools panel uses) and, when the floor is on, walks the
     *   colour until it clears the ratio.
     *
     * This is host compute, which is where the compute boundary says it goes:
     * the page receives finished tokens, the browser decides nothing. It also
     * means the answer is identical in the GUI's preview, the email, and a
     * headless render — one arithmetic, three surfaces. */
    std::string accent_lite, accent_dark; // "" = derived from accent
    int contrast = 1;   // 0 off · 1 AA (4.5:1) · 2 AAA (7:1)

    /* Banner treatment: a photograph behind white text is only legible if
     * something is done to it, and "something" was previously one hardcoded
     * gradient. `filter` is the look, `dim` is how hard the scrim pulls. */
    int banner_filter = 0; // 0 none 1 mute 2 mono 3 warm 4 cool 5 soft-blur
    int banner_dim = 45;   // 0..100, the legibility scrim over an image

    /* Grid rhythm — the Figma/Squarespace axis. `gap` is the spacing step;
     * `even` makes every cell in a row the same height so a wall of cards reads
     * as a grid rather than as a ragged pile. */
    int grid_gap = 2;   // 0 tight … 4 airy
    bool grid_even = true;
    bool icons = true;  // icons beside dates/places/roles on generated cards
    const char* body_class() const {
        static const char* c[] = {"clean", "soft", "bold", "editorial", "glass"};
        return c[std::clamp(preset, 0, 4)];
    }
    // preset + texture + grid rhythm, the full <body> class list
    std::string body_classes() const {
        std::string s = body_class();
        static const char* tx[] = {"", " tx-dots", " tx-grid", " tx-hatch"};
        s += tx[std::clamp(texture, 0, 3)];
        if (grid_even) s += " even-grid";
        return s;
    }
    /* The CSS class for a banner treatment, "" for none. Named rather than
     * numbered on the page so a person reading the HTML can see what it does. */
    const char* filter_class() const {
        static const char* f[] = {"", "f-mute", "f-mono", "f-warm", "f-cool",
                                  "f-soft"};
        return f[std::clamp(banner_filter, 0, 5)];
    }
};
/* The theme from CONFIG (the persisted, authoritative source — the Style tab
 * writes it on every change, and console/agent `config set theme.*` reaches it
 * too). Rendering reads config, not the ImGui members, so both paths agree. */
SiteTheme read_site_theme(maiz::Core& core);

/* ── colour arithmetic, shared by the renderer and the Style tab ─────────────
 *
 * WCAG 2.1's own definitions, because "does this text read" has a published
 * answer and guessing at it is how a website ends up with grey-on-grey. The
 * Style tab shows the same numbers the renderer acts on, so a person can see
 * WHY a colour was overridden rather than watching it change under them.
 */
/* Relative luminance of a #rrggbb colour (WCAG 2.1 §relative-luminance). */
double srgb_luminance(const std::string& hex);
/* Contrast ratio between two #rrggbb colours: 1.0 (identical) … 21.0
 * (black on white). AA body text wants 4.5, AAA wants 7. */
double contrast_ratio(const std::string& a, const std::string& b);
/* Black or white — whichever a reader can actually see on `bg`. */
std::string ink_on(const std::string& bg);
/* Move a colour toward white (amount > 0) or black (amount < 0), 0..1. Hue is
 * preserved by mixing rather than by an HSL round trip, which is both simpler
 * and closer to what a designer means by "a lighter version of this". */
std::string shade(const std::string& hex, double amount);
/* Walk `fg` away from `bg` until it clears `target`, or give up and return the
 * best of black/white. Used to rescue `--muted` and any authored pairing that
 * does not meet the floor. */
std::string meet_contrast(const std::string& fg, const std::string& bg,
                          double target);
