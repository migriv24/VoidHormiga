/* phone/phone.cpp — Hormiga on a phone: a navigation bar and eight screens over
 * the same core, the same sync and the same commands as the desktop.
 *
 * The author's scope (2026-09-23), which is the brief for this front-end:
 *   no Builder, no Map, no visible console, no windows at all; screens under
 *   a navigation bar; the DATA screen is the thing tested across networked
 *   devices, and the CALENDAR gets a phone-shaped overhaul.
 * And (2026-09-25): the bar is the person's to arrange, from Data, Calendar,
 * Notes, the network, Profile, Settings and the Antfarm, around ONE locked
 * centre button, the Hormiga button, which opens a gallery of everything.
 * See okf/concepts/sections/mobile.md for the reasoning.
 *
 * WHAT IS SHARED WITH THE DESKTOP, deliberately, because the author's other
 * instruction was that interactions stay consistent across devices:
 *   - every edit is the same dispatcher command (`set`, `tag`, `rm`, `undo`),
 *     through the same `dispatch_and_reproject`, so a phone edit replays;
 *   - presence declares the SAME surface ids the desktop does (`table:data`,
 *     `calendar`, `list:notes`), so a desktop member sees "in Data" whichever
 *     device you are on, and your selection is the rune on your screen;
 *   - the join flow and the profile are the desktop's own bodies
 *     (LanRuntime::draw_discover_body, draw_profile_body), not copies.
 *
 * WHAT IS NOT: layout. `phone/` may not include `ui/` (tools/check_layering.py);
 * it calls HormigaApp's methods through app.hpp and draws its own screens,
 * which live in phone_data.cpp, phone_more.cpp, phone_migos.cpp and
 * phone_migas.cpp. This file is the frame: the bars, the gallery, which screen.
 *
 * RUN IT on a desktop with `voidhormiga --phone <database>`: a phone-sized
 * window, the mouse as a finger. `--touch` adds the drawn keyboard.
 */
#include "phone/phone_ui.hpp"

#include "domain/date_query.hpp" // today_days

#include <cfloat>

namespace hormiga::phone {

namespace {

const ScreenInfo kInfo[kScreens] = {
    {"data", ICON_FA_ADDRESS_BOOK, "Data", "People, organizations and events, as cards"},
    {"calendar", ICON_FA_CALENDAR_DAYS, "Calendar", "The week, the day, and what is coming"},
    {"notes", ICON_FA_NOTE_STICKY, "Notes", "Notes, shared with the database or kept private"},
    {"migos", ICON_FA_USERS, "Migos", "Your network: who is here, transfers, joining, sharing"},
    {"migas", ICON_FA_DATABASE, "Migas", "The databases on this phone: open, new, import, export"},
    {"profile", ICON_FA_USER, "Profile", "Who you are to the people you share with"},
    {"settings", ICON_FA_GEAR, "Settings", "This app on this phone: the look, the bar"},
    {"antfarm", ICON_FA_DIAGRAM_PROJECT, "Antfarm", "The database's machinery: backends and devices"},
};

} // namespace

const ScreenInfo& screen_info(int s) { return kInfo[s >= 0 && s < kScreens ? s : 0]; }

int screen_by_key(const std::string& key) {
    for (int i = 0; i < kScreens; ++i)
        if (key == kInfo[i].key) return i;
    return -1;
}

std::vector<int> default_bar() { return {kData, kCalendar, kNotes, kMigos}; }

std::vector<int> load_bar() {
    std::vector<int> bar;
    for (const auto& k : hormiga::app_settings::load().phone_nav) {
        const int s = screen_by_key(k);
        if (s >= 0 && std::find(bar.begin(), bar.end(), s) == bar.end()) bar.push_back(s);
    }
    return bar.empty() ? default_bar() : bar;
}

void save_bar(const std::vector<int>& bar) {
    auto st = hormiga::app_settings::load();
    st.phone_nav.clear();
    for (int s : bar) st.phone_nav.push_back(screen_info(s).key);
    hormiga::app_settings::save(st);
}

} // namespace hormiga::phone

using namespace hormiga::phone;

void HormigaApp::enable_phone(bool touch, std::unique_ptr<maiz::TextInputPlatform> keyboard, float screen_density) {
    phone = std::make_shared<PhoneUi>();
    phone->dp = density = screen_density < 1.0f ? 1.0f : screen_density;
    phone->touch = touch;
    phone->text_input = std::move(keyboard);
    phone->day = hormiga::today_days();
    phone->bar = load_bar();
}

/* ── THE HORMIGA BUTTON'S GALLERY ─────────────────────────────────────────────
 * Every screen as a tile, two to a row: its icon, its name, one line of what it
 * is for, and a pin on the ones already on the bar. The ant at the top is the
 * button's own face, so the gallery reads as what the button opened. */
void HormigaApp::PhoneUi::home(HormigaApp& app, PhoneUi& ph, Frame&) {
    const float dp = ph.dp;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        const float r = 26.0f * dp;
        const ImVec4 acc = ImGui::GetStyle().Colors[ImGuiCol_CheckMark];
        draw_ant_button(dl, ImVec2(at.x + r, at.y + r), r, ImGui::GetColorU32(acc), IM_COL32(255, 255, 255, 240));
        ImGui::Dummy(ImVec2(2 * r, 2 * r));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Dummy(ImVec2(0, r * 0.25f));
        ImGui::PushFont(nullptr, ImGui::GetFontSize() * 1.2f);
        ImGui::TextUnformatted("Void Hormiga");
        ImGui::PopFont();
        const std::string db = app.cur_miga.empty() ? std::string("an unsaved database")
                                                    : std::filesystem::path(app.cur_miga).stem().string();
        ImGui::TextDisabled("%s", db.c_str());
        ImGui::EndGroup();
    }
    ImGui::Spacing();
    const float gap = 10.0f * dp;
    const float w = (ImGui::GetContentRegionAvail().x - gap) * 0.5f, h = 118.0f * dp;
    for (int s = 0; s < kScreens; ++s) {
        const ScreenInfo& in = screen_info(s);
        if (s % 2) ImGui::SameLine(0, gap);
        ImGui::PushID(s);
        const bool tapped = ImGui::InvisibleButton("##tile", ImVec2(w, h));
        const bool held = ImGui::IsItemActive();
        ImGui::PopID();
        const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
        dl->AddRectFilled(r0, r1, ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg), 14.0f * dp);
        const float pad = 12.0f * dp, ir = 18.0f * dp;
        const ImVec2 ic(r0.x + pad + ir, r0.y + pad + ir);
        dl->AddCircleFilled(ic, ir, kind_colour(in.key));
        const float ipx = ImGui::GetFontSize() * 1.05f;
        const ImVec2 is = ImGui::GetFont()->CalcTextSizeA(ipx, FLT_MAX, 0, in.icon);
        dl->AddText(ImGui::GetFont(), ipx, ImVec2(ic.x - is.x * 0.5f, ic.y - is.y * 0.5f), IM_COL32_WHITE, in.icon);
        if (std::find(ph.bar.begin(), ph.bar.end(), s) != ph.bar.end()) // on the bar already
            dl->AddText(ImVec2(r1.x - pad - ImGui::CalcTextSize(ICON_FA_THUMBTACK).x, r0.y + pad),
                        ImGui::GetColorU32(ImGuiCol_TextDisabled), ICON_FA_THUMBTACK);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.08f, ImVec2(r0.x + pad, ic.y + ir + 6 * dp),
                    ImGui::GetColorU32(ImGuiCol_Text), in.label);
        const float sub = ImGui::GetFontSize() * 0.82f;
        dl->AddText(ImGui::GetFont(), sub, ImVec2(r0.x + pad, ic.y + ir + 8 * dp + ImGui::GetFontSize() * 1.1f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), in.blurb, nullptr, w - 2 * pad);
        if (tapped) {
            ph.screen = s;
            ph.stacks[s].reset();
        }
    }
    ImGui::Spacing();
    if (ImGui::Button(ICON_FA_BARS_STAGGERED "  Arrange the bar", ImVec2(-FLT_MIN, 0))) {
        ph.screen = kSettings;
        ph.stacks[kSettings].reset();
    }
}

void HormigaApp::phone_frame() {
    PhoneUi& ph = *phone;
    // FIRST, before any window: the platform keyboard's edits become this frame's
    // keystrokes; without one, the drawn keyboard eats its own touches (mobile.hpp)
    if (ph.text_input) maiz::text_input_frame(ph.text_session, ph.text_input.get());
    else maiz::keyboard(ph.keys, ph.touch);

    // a finger needs room: larger targets than the desktop's, everywhere
    const float dp = ph.dp; // the sizes below are design pixels; a phone's screen is ~3x
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12 * dp, 10 * dp));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10 * dp, 10 * dp));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * dp, 10 * dp));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f * dp);

    /* THE NETWORK. On the desktop the Share windows tick it; here there are no
     * windows, so the phone ticks it itself, and listens for shared databases
     * only while Migos is open (a beacon is a request nobody made while they
     * are reading their contacts). A phone that shares its database meets
     * requests to join it here, on any screen. */
    LanRuntime& rt = LanRuntime::of(*this);
    if (win_profile) { // the shared bodies' "Open profile": on a phone, the Profile screen
        win_profile = false;
        ph.screen = kProfile;
    }
    rt.discovering = (ph.screen == kMigos);
    LanRuntime::tick(*this, ImGui::GetTime());
    if (lan) LanRuntime::draw_request(*this);

    // ── the navigation bar: reserve the bottom edge before anything else ─────
    {
        std::vector<hormiga::phone::NavItem> items;
        int current = -1;
        for (int i = 0; i < (int)ph.bar.size(); ++i) {
            const ScreenInfo& in = screen_info(ph.bar[i]);
            items.push_back({in.icon, in.label, ph.bar[i] == kMigos ? (int)rt.present.size() : 0});
            if (ph.bar[i] == ph.screen) current = i;
        }
        const int t = hormiga::phone::nav_bar_centred(items, current, ph.screen == kHome);
        if (t == hormiga::phone::kCentreTapped) {
            if (ph.screen == kHome) {
                ph.screen = ph.before_home;
            } else {
                ph.before_home = ph.screen;
                ph.screen = kHome;
            }
        } else if (t >= 0) {
            if (ph.bar[t] == ph.screen) ph.stacks[ph.screen].reset(); // a second tap: back to the top
            ph.screen = ph.bar[t];
        }
    }
    if (hormiga::phone::back_pressed()) {
        if (ph.screen == kHome) ph.screen = ph.before_home;
        else if (!ph.stacks[ph.screen].pop() && ph.screen != kData) ph.screen = kData; // Back at a root goes home
    }
    const bool home_screen = ph.screen == kHome;
    PhoneUi::Frame f;
    f.stack = home_screen ? nullptr : &ph.stacks[ph.screen]; // bound AFTER Back may have moved us
    f.route = f.stack ? f.stack->top() : std::string("list");
    const bool is_detail = f.route.rfind("detail:", 0) == 0;
    const std::string rune = is_detail ? f.route.substr(7) : std::string();

    // Data, Calendar and Notes read the organization's mantle; the Antfarm its own
    const char* want = ph.screen == kData || ph.screen == kCalendar || ph.screen == kNotes ? kDataMantle
                       : ph.screen == kAntfarm                                             ? kAntfarmMantle
                                                                                           : nullptr;
    /* A database without that mantle (an empty one a tool wrote) refuses the
     * switch; asking again every frame queued a toast every frame. Once every
     * few seconds is enough to notice a mantle that arrives with a sync. */
    if (want && scene.mantle != want && ImGui::GetTime() - ph.use_failed_at > 5.0)
        if (!dispatch_and_reproject(std::string("use ") + want).ok) ph.use_failed_at = ImGui::GetTime();
    section = ph.screen == kAntfarm ? Antfarm : Data; // what the 0.1.4 presence fields say
    if (ph.screen != kAntfarm && ph.screen != kNotes) { // those two keep their own selection
        if (!is_detail) ed.selection.clear();
        else ed.selection = {rune}; // presence: the rune on MY screen is what I am on
    }

    // ── the app bar ──────────────────────────────────────────────────────────
    const maiz::SceneNode* open_node = is_detail ? scene.find(rune) : nullptr;
    const std::string title = home_screen ? std::string("Everything")
                              : is_detail ? (open_node ? title_of(*open_node) : std::string("Removed"))
                                          : std::string(screen_info(ph.screen).label);
    if (hormiga::phone::begin_app_bar(title.c_str(), home_screen || (f.stack && f.stack->can_pop()))) {
        if (home_screen) ph.screen = ph.before_home;
        else f.stack->pop();
    }
    if (!is_detail && (ph.screen == kData || ph.screen == kCalendar || ph.screen == kNotes))
        LanRuntime::draw_presence_strip(*this); // who else is here
    if (ph.screen == kCalendar && !is_detail) {
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x -
                                                             ImGui::CalcTextSize("Today").x -
                                                             ImGui::GetStyle().FramePadding.x * 2));
        if (ImGui::SmallButton("Today")) ph.day = hormiga::today_days();
    }
    hormiga::phone::end_app_bar();

    // ── the screen: the work area between the two bars ───────────────────────
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    // the system keyboard covers the bottom of the window (the navigation bar
    // first): the screen ends where the keyboard begins, so what you type into
    // can always be scrolled into view
    const float covered = std::max(0.0f, ph.text_session.covered_px - hormiga::phone::nav_bar_height());
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, std::max(120.0f, vp->WorkSize.y - covered)));
    // the Antfarm's graph is a canvas that pans itself: its screen must not scroll
    const bool canvas = ph.screen == kAntfarm && ph.farm_graph && !is_detail;
    ImGui::Begin("##phone-screen", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoSavedSettings |
                     (canvas ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse : 0));

    if (home_screen) {
        PhoneUi::home(*this, ph, f);
    } else if (is_detail && ph.screen != kAntfarm) {
        if (open_node) {
            PhoneUi::detail(*this, ph, f, *open_node);
        } else {
            // removed on this device or another one while it was open
            maiz::dim_wrapped("This was removed, here or on another device.");
            if (ImGui::Button("Back")) f.stack->pop();
        }
    } else {
        switch (ph.screen) {
        case kData: PhoneUi::data(*this, ph, f); break;
        case kCalendar: PhoneUi::calendar(*this, ph, f); break;
        case kNotes: PhoneUi::notes(*this, ph, f); break;
        case kMigos: PhoneUi::migos(*this, ph, f); break;
        case kMigas: PhoneUi::migas(*this, ph, f); break;
        case kProfile: PhoneUi::profile(*this, ph, f); break;
        case kSettings: PhoneUi::settings(*this, ph, f); break;
        case kAntfarm: PhoneUi::antfarm(*this, ph, f); break;
        default: break;
        }
    }
    ImGui::End();
    PhoneUi::flush_note(*this, ph);

    if (!home_screen && !is_detail && f.route == "list") PhoneUi::add_button(*this, ph, f);

    for (const auto& c : f.out) dispatch_and_reproject(c);
    if (maiz::draw_snackbar(ph.snack)) dispatch_and_reproject("undo");

    draw_vault_modal(); // a joined database may carry sealed credentials
    draw_busy_overlay();
    draw_toasts();
    ImGui::PopStyleVar(4);
}

/* The add button, where a screen has one thing it adds. */
void HormigaApp::PhoneUi::add_button(HormigaApp& app, PhoneUi& ph, Frame& f) {
    if (ph.screen == kData) { // the kinds a field worker adds
        static const std::vector<std::string> kAdd = {"Contact", "Organization", "Event"};
        static const char* kGlyph[] = {"contact", "organization", "event"};
        const int pick = maiz::speed_dial("##phone-add", ICON_FA_PLUS, kAdd, ph.dial_open);
        if (pick >= 0) {
            ph.dial_open = false;
            app.new_rune(kGlyph[pick]); // mints (device-scoped), tags, selects
            if (!app.ed.selection.empty()) f.stack->push("detail:" + app.ed.selection.front());
        }
    } else if (ph.screen == kNotes) {
        if (maiz::fab(ICON_FA_PLUS "##note-add")) {
            app.new_rune("note");
            if (!app.ed.selection.empty()) f.stack->push("note:" + app.ed.selection.front());
        }
    }
}
