/* phone/phone_ui.hpp — the phone front-end's own state and its screens, shared
 * by the files of src/phone/ and nobody else.
 *
 * HormigaApp::PhoneUi is a nested type, so its static members may read the
 * app's private parts the way HormigaApp's own methods do. That is what lets the
 * screens live in several files without a line more in app.hpp (which is at its
 * budget): each screen is `static void x(HormigaApp&, PhoneUi&, Frame&)`.
 *
 * THE SCREENS (the author, 2026-09-25). Seven places a person can go, plus the
 * database manager this change adds: any four sit on the navigation bar, in the
 * order the person chose in Settings, around one locked centre button, the
 * Hormiga button, which opens a gallery of all of them. Keys are stable strings
 * because they are saved (app_settings.phone_nav); indices are not. */
#pragma once

#include "app/app_internal.hpp"
#include "app/lan_share.hpp"
#include "platform/app_settings.hpp"
#include "phone/nav.hpp"
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/textinputview.hpp"

#include <memory>
#include <string>
#include <vector>

namespace hormiga::phone {

enum Screen { kData, kCalendar, kNotes, kMigos, kMigas, kProfile, kSettings, kAntfarm, kScreens };
inline constexpr int kHome = 100; // the Hormiga button's gallery: not a screen, not on the bar

struct ScreenInfo {
    const char* key;   // saved: never rename
    const char* icon;
    const char* label; // one word, on the bar
    const char* blurb; // one line, in the gallery
};
const ScreenInfo& screen_info(int s);
int screen_by_key(const std::string& key); // -1 when unknown

/* The bar's four slots. Defaults: Data, Calendar | Hormiga | Notes, Migos. */
std::vector<int> default_bar();
std::vector<int> load_bar();                 // from this device's settings, defaults if unset
void save_bar(const std::vector<int>& bar);

// ── what a card says (phone_data.cpp) ─────────────────────────────────────────
std::string title_of(const maiz::SceneNode& n);
std::string subtitle_of(const maiz::SceneNode& n);
ImU32 kind_colour(const std::string& glyph);
const char* kind_icon(const std::string& glyph);
std::string kind_label(const std::string& glyph);
std::string lower(std::string s);
std::string human_bytes(long long b);

/* Requests a phone makes of the system (document_arrived tells them apart). */
enum DocRequest { kDocPhoto = 1, kDocImportMiga = 2, kDocExportMiga = 3, kDocOpened = -1 };

} // namespace hormiga::phone

struct HormigaApp::PhoneUi {
    int screen = hormiga::phone::kData;
    int before_home = hormiga::phone::kData; // where the Hormiga button was pressed
    hormiga::phone::Stack stacks[hormiga::phone::kScreens];
    std::vector<int> bar;             // the navigation bar's slots (screen ids)
    bool touch = false;               // a real finger: the drawn keyboard, if nothing better
    maiz::KeyboardState keys;
    /* THE PLATFORM'S KEYBOARD (Android's, through org.voidmaiz.MaizActivity),
     * when the shell found one. It is the keyboard; the drawn one above is only
     * for a touch host that has none (the author, 2026-09-23: "Custom keyboard is
     * too much of a hassle ... we should focus on the integration of the
     * keyboard"). */
    std::unique_ptr<maiz::TextInputPlatform> text_input;
    maiz::TextInputSession text_session;
    maiz::SwipeListState swipe;
    maiz::SnackbarState snack;
    bool dial_open = false;           // the Data screen's add button
    char search[96] = {};
    std::string kind;                 // "" = every kind
    long long day = 0;                // the calendar's selected day (days since 1970)
    char quick[160] = {};             // the calendar's one-line add
    float dp = 1.0f;                  // pixels per design pixel: the screen's density
    double use_failed_at = -100.0;    // the last time switching to a screen's mantle was refused

    // Notes
    char note_search[96] = {};
    std::string note_for;             // which note note_buf holds
    std::vector<char> note_buf = std::vector<char>(16384, 0);
    bool note_dirty = false;          // typed into, not yet committed
    int note_frame = -1;              // the last frame the editor was drawn

    // a photo on its way from the system picker: where it goes when it lands
    std::string photo_rune, photo_field;

    // Migas (the databases)
    char new_name[64] = {};
    std::string confirm_remove;       // a path waiting for "Remove" to be pressed again
    double listed_at = -100.0;
    std::vector<hormiga::app_settings::KnownDatabase> known;

    // Antfarm
    bool farm_graph = false;          // the node graph, rather than the list
    maiz::CanvasStyle farm_style;
    bool farm_style_ready = false;

    /* One frame's shared things: the route on the current screen, and the
     * commands the screen wants dispatched after it is drawn. */
    struct Frame {
        hormiga::phone::Stack* stack = nullptr;
        std::string route;
        std::vector<std::string> out;
    };

    // ── the screens (each its own file's) ─────────────────────────────────────
    static void home(HormigaApp& app, PhoneUi& ph, Frame& f);      // phone.cpp: the gallery
    static void data(HormigaApp& app, PhoneUi& ph, Frame& f);      // phone_data.cpp
    static void detail(HormigaApp& app, PhoneUi& ph, Frame& f, const maiz::SceneNode& n);
    static void calendar(HormigaApp& app, PhoneUi& ph, Frame& f);
    static void notes(HormigaApp& app, PhoneUi& ph, Frame& f);     // phone_more.cpp
    static void antfarm(HormigaApp& app, PhoneUi& ph, Frame& f);
    static void settings(HormigaApp& app, PhoneUi& ph, Frame& f);
    static void profile(HormigaApp& app, PhoneUi& ph, Frame& f);
    static void migos(HormigaApp& app, PhoneUi& ph, Frame& f);     // phone_migos.cpp: the network
    static void migas(HormigaApp& app, PhoneUi& ph, Frame& f);     // phone_migas.cpp: the databases
    static void add_button(HormigaApp& app, PhoneUi& ph, Frame& f); // the screen's + (if it has one)
    /* A note being typed is committed when its editor stops being drawn (Back,
     * the bar, the Hormiga button all act BEFORE the screen draws, so the text
     * box never sees itself lose focus). Called once a frame, after the screen. */
    static void flush_note(HormigaApp& app, PhoneUi& ph);
    /* Save the database open now before anything replaces it; one never saved
     * gets a dated name in this device's databases folder (phone_migas.cpp). */
    static void keep_current(HormigaApp& app);
};
