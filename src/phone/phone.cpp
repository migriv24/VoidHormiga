/* phone/phone.cpp — Hormiga on a phone: a navigation bar and four screens over
 * the same core, the same sync and the same commands as the desktop.
 *
 * The author's scope (2026-09-23), which is the whole brief for this file:
 *   no Builder, no Map, no Antfarm, no visible console, no windows at all;
 *   screens under a navigation bar; the DATA screen is the thing tested across
 *   networked devices, and the CALENDAR gets a phone-shaped overhaul.
 * See okf/concepts/sections/mobile.md for the reasoning.
 *
 * WHAT IS SHARED WITH THE DESKTOP, deliberately, because the author's other
 * instruction was that interactions stay consistent across devices:
 *   - every edit is the same dispatcher command (`set`, `tag`, `rm`, `undo`),
 *     through the same `dispatch_and_reproject`, so a phone edit replays;
 *   - presence declares the SAME surface ids the desktop does (`table:data`,
 *     `calendar`), so a desktop member sees "in Data" whichever device you are
 *     on, and your selection is the rune on your screen;
 *   - the join flow and the profile are the desktop's own bodies
 *     (LanRuntime::draw_discover_body, draw_profile_body), not copies.
 *
 * WHAT IS NOT: layout. `phone/` may not include `ui/` (tools/check_layering.py);
 * it calls HormigaApp's methods through app.hpp and draws its own screens.
 *
 * RUN IT on a desktop with `voidhormiga --phone <database>`: a phone-sized
 * window, the mouse as a finger. `--touch` adds the drawn keyboard.
 */
#include "app/app_internal.hpp"
#include "app/lan_share.hpp"
#include "domain/date_query.hpp" // days_from_civil, today_days
#include "domain/quick_add.hpp"  // the calendar's one-line creation grammar
#include "phone/nav.hpp"
#include "voidmaiz/mobile.hpp"
#include "voidmaiz/netview.hpp"
#include "voidmaiz/textinputview.hpp" // the platform's own keyboard (Void Maiz's text-input holiday)

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdio>

namespace {

enum Tab { kData = 0, kCalendar = 1, kTogether = 2, kMe = 3 };

/* Hinnant's civil_from_days, the inverse of domain/date_query.hpp's
 * days_from_civil: the week strip walks days as integers. */
void civil_from_days(long long z, int& y, int& m, int& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y = (int)(yoe + era * 400 + (m <= 2));
}

int weekday_mon0(long long days) { return (int)(((days + 3) % 7 + 7) % 7); } // 1970-01-01 was a Thursday

const char* kWeekday[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const char* kMonth[12] = {"January", "February", "March",     "April",   "May",      "June",
                          "July",    "August",   "September", "October", "November", "December"};

std::string lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

/* What a person calls a rune: the printed name if it has one, else its title,
 * else its handle. */
std::string title_of(const maiz::SceneNode& n) {
    for (const char* k : {"display_name", "title_en", "title", "description"}) {
        std::string v = hormiga::temper::field_value(n, k);
        if (!v.empty()) return v;
    }
    return n.name;
}

/* The one line under a row's title. */
std::string subtitle_of(const maiz::SceneNode& n) {
    auto f = [&](const char* k) { return hormiga::temper::field_value(n, k); };
    if (n.glyph == "contact") {
        std::string r = f("role");
        if (!f("phone").empty()) r += (r.empty() ? "" : "  ·  ") + f("phone");
        return r.empty() ? f("email") : r;
    }
    if (n.glyph == "event" || n.glyph == "incident" || n.glyph == "job") {
        std::string d = f("date");
        if (!f("start_time").empty()) d += "  " + f("start_time");
        return d.empty() ? n.label : d;
    }
    if (n.glyph == "organization") return f("kind").empty() ? n.label : f("kind");
    return n.label;
}

/* Fields a phone never shows: canvas geometry, map coordinates (there is no map
 * here), and anything the glyph itself marks hidden. */
bool phone_skips(const maiz::SceneField& f) {
    static const char* kSkip[] = {"pos", "size", "geo", "ref", "ref_off", "label_scale", "image_url"};
    if (f.editor == "hidden") return true;
    for (const char* k : kSkip)
        if (f.key == k) return true;
    return false;
}

} // namespace

struct HormigaApp::PhoneUi {
    int tab = kData;
    hormiga::phone::Stack stacks[4];
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
};

void HormigaApp::enable_phone(bool touch, std::unique_ptr<maiz::TextInputPlatform> keyboard, float screen_density) {
    phone = std::make_shared<PhoneUi>();
    phone->dp = density = screen_density < 1.0f ? 1.0f : screen_density;
    phone->touch = touch;
    phone->text_input = std::move(keyboard);
    phone->day = hormiga::today_days();
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
     * only while the Together screen is open (a beacon is a request nobody made
     * while they are reading their contacts). A phone never hosts, so the one
     * window it can meet is a request to join THIS device, which it never
     * offers; draw_request is kept for the day it does. */
    LanRuntime& rt = LanRuntime::of(*this);
    rt.discovering = (ph.tab == kTogether);
    LanRuntime::tick(*this, ImGui::GetTime());
    if (lan) LanRuntime::draw_request(*this);

    // ── the navigation bar: reserve the bottom edge before anything else ─────
    const int others = (int)rt.present.size();
    const int before = ph.tab;
    if (hormiga::phone::nav_bar({{ICON_FA_ADDRESS_BOOK, "Data"},
                                 {ICON_FA_CALENDAR_DAYS, "Calendar"},
                                 {ICON_FA_USERS, "Together", others},
                                 {ICON_FA_USER, "Me"}},
                                ph.tab) &&
        ph.tab == before)
        ph.stacks[ph.tab].reset(); // a second tap: back to the top, as phones do
    if (hormiga::phone::back_pressed()) {
        if (!ph.stacks[ph.tab].pop() && ph.tab != kData) ph.tab = kData; // Back at a root goes home
    }
    hormiga::phone::Stack& stack = ph.stacks[ph.tab]; // bound AFTER Back may have moved us
    const std::string& route = ph.stacks[ph.tab].top();
    const bool is_detail = route.rfind("detail:", 0) == 0;
    const std::string rune = is_detail ? route.substr(7) : std::string();

    // Data and Calendar both read the organization's mantle
    if ((ph.tab == kData || ph.tab == kCalendar) && scene.mantle != kDataMantle)
        dispatch_and_reproject(std::string("use ") + kDataMantle);
    section = Data; // what the 0.1.4 presence fields say; the surfaces say more
    if (!is_detail) ed.selection.clear();
    else ed.selection = {rune}; // presence: the rune on MY screen is what I am on

    // ── the app bar ──────────────────────────────────────────────────────────
    const maiz::SceneNode* open_node = is_detail ? scene.find(rune) : nullptr;
    std::string title = is_detail ? (open_node ? title_of(*open_node) : std::string("Removed"))
                        : ph.tab == kData     ? std::string("Data")
                        : ph.tab == kCalendar ? std::string("Calendar")
                        : ph.tab == kTogether ? std::string("Together")
                                              : std::string("Me");
    if (hormiga::phone::begin_app_bar(title.c_str(), stack.can_pop())) stack.pop();
    if (!is_detail && (ph.tab == kData || ph.tab == kCalendar))
        LanRuntime::draw_presence_strip(*this); // who else is here
    if (ph.tab == kCalendar && !is_detail) {
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(),
                                 ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Today").x -
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
    ImGui::Begin("##phone-screen", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoSavedSettings);
    std::vector<std::string> out;

    /* ── DETAIL: one rune, every field it declares, its tags ──────────────── */
    auto draw_detail = [&](const maiz::SceneNode& n) {
        if (n.glyph == "contact" || n.glyph == "organization") {
            const float r = 34.0f * phone->dp;
            const ImVec2 c = ImGui::GetCursorScreenPos();
            draw_avatar(n, ImVec2(c.x + r, c.y + r), r);
            ImGui::Dummy(ImVec2(2 * r, 2 * r));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::TextUnformatted(title_of(n).c_str());
        ImGui::TextDisabled("%s", n.label.c_str());
        ImGui::EndGroup();
        maiz::presence_item(surfaces, roster, net_settings.show,
                            ph.tab == kCalendar ? "calendar" : "table:data", n.id, maiz::Mark::Badge,
                            share_now && !share_now(n));
        ImGui::Spacing();
        // LABELS ABOVE, fields full width: a phone reads down, not across. The
        // field's own right-hand label is pushed past the edge by the width.
        maiz::WidgetContext ctx{scene, out, std::string(), 0.0f};
        for (const auto& fl : n.fields) {
            if (phone_skips(fl)) continue;
            ImGui::TextDisabled("%s", fl.label.empty() ? fl.key.c_str() : fl.label.c_str());
            ImGui::PushItemWidth(-FLT_MIN);
            ImGui::PushID(fl.key.c_str());
            // clip at the content edge, so ImGui's right-hand label never peeks in
            const ImVec2 wp = ImGui::GetWindowPos();
            ImGui::PushClipRect(ImVec2(wp.x, wp.y),
                                ImVec2(wp.x + ImGui::GetWindowContentRegionMax().x, wp.y + ImGui::GetWindowHeight()),
                                true);
            maiz::widget_field(ctx, widgets, n, fl); // tells the keyboard what the field is, too
            ImGui::PopClipRect();
            if (ImGui::IsItemActivated()) ImGui::SetScrollHereY(0.3f); // above the keyboard
            ImGui::PopID();
            ImGui::PopItemWidth();
        }
        ImGui::SeparatorText("Tags");
        draw_tag_editor(n, out);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
        if (ImGui::Button(ICON_FA_TRASH "  Delete", ImVec2(-FLT_MIN, 0))) {
            out.push_back("rm " + n.name);
            maiz::show_snackbar(ph.snack, "Deleted " + title_of(n), "UNDO");
            stack.pop();
        }
        ImGui::PopStyleColor();
    };

    if (is_detail) {
        if (open_node) {
            draw_detail(*open_node);
        } else {
            // removed on this device or another one while it was open
            maiz::dim_wrapped("This was removed, here or on another device.");
            if (ImGui::Button("Back")) stack.pop();
        }
    } else if (ph.tab == kData) {
        /* ── DATA: search, kinds, a list you can swipe ────────────────────── */
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##search", ICON_FA_MAGNIFYING_GLASS "  Search", ph.search,
                                 sizeof ph.search);
        maiz::text_input_kind(maiz::InputKind::Search); // the keyboard's action key says "search"
        std::map<std::string, int> counts;
        for (const auto& n : scene.nodes) counts[n.glyph]++;
        // kinds as a row of chips that scrolls sideways rather than wrapping
        ImGui::BeginChild("##kinds", ImVec2(0, ImGui::GetFrameHeightWithSpacing() + 6 * ph.dp),
                          ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        auto chip = [&](const char* label, const std::string& kind) {
            const bool on = ph.kind == kind;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(label)) ph.kind = kind;
            if (on) ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        chip("All", "");
        for (const auto& e : palette.entries)
            if (counts[e.glyph] > 0) chip((e.label + " " + std::to_string(counts[e.glyph])).c_str(), e.glyph);
        ImGui::EndChild();

        const std::string q = lower(ph.search);
        std::vector<const maiz::SceneNode*> rows;
        for (const auto& n : scene.nodes) {
            bool data_kind = false;
            for (const auto& e : palette.entries) data_kind |= (e.glyph == n.glyph);
            if (!data_kind || (!ph.kind.empty() && n.glyph != ph.kind)) continue;
            if (!q.empty() && lower(title_of(n)).find(q) == std::string::npos &&
                lower(n.name).find(q) == std::string::npos)
                continue;
            rows.push_back(&n);
        }
        std::sort(rows.begin(), rows.end(), [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
            return lower(title_of(*a)) < lower(title_of(*b));
        });
        if (rows.empty())
            maiz::dim_wrapped(scene.nodes.empty()
                                  ? "Nothing here yet. Join a shared database under Together, or add "
                                    "something with +."
                                  : "Nothing matches.");
        const float row_h = ImGui::GetFontSize() * 3.4f;
        for (const maiz::SceneNode* n : rows) {
            const int act = maiz::begin_swipe_row(ph.swipe, n->name.c_str(), {"Delete"}, row_h);
            // a TAP, not a swipe: released, and never dragged past a few pixels
            const bool tapped = ImGui::IsItemDeactivated() && ImGui::GetIO().MouseDragMaxDistanceSqr[0] < 36.0f * ph.dp * ph.dp;
            maiz::presence_item(surfaces, roster, net_settings.show, "table:data", n->id, maiz::Mark::Badge,
                                share_now && !share_now(*n));
            const ImVec2 at = ImGui::GetCursorScreenPos();
            float text_x = at.x;
            if (n->glyph == "contact" || n->glyph == "organization") {
                const float r = row_h * 0.3f;
                draw_avatar(*n, ImVec2(at.x + r, at.y + ImGui::GetTextLineHeight() * 0.5f), r);
                text_x += 2 * r + 12;
            }
            ImGui::SetCursorScreenPos(ImVec2(text_x, at.y - ImGui::GetTextLineHeight() * 0.55f));
            ImGui::TextUnformatted(title_of(*n).c_str());
            ImGui::SetCursorScreenPos(ImVec2(text_x, ImGui::GetCursorScreenPos().y - 6));
            ImGui::TextDisabled("%s", subtitle_of(*n).c_str());
            // the next row starts at THIS row's bottom edge, not where the text ended
            const float row_top = at.y - (row_h - ImGui::GetTextLineHeight()) * 0.5f;
            ImGui::SetCursorScreenPos(
                ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, row_top + row_h));
            maiz::end_swipe_row(ph.swipe);
            if (act == 0) {
                out.push_back("rm " + n->name);
                maiz::show_snackbar(ph.snack, "Deleted " + title_of(*n), "UNDO");
            } else if (tapped && ph.swipe.open_id.empty()) {
                stack.push("detail:" + n->name);
            }
        }
        ImGui::Dummy(ImVec2(0, row_h)); // room under the last row for the add button
    } else if (ph.tab == kCalendar) {
        /* ── CALENDAR: a week you can step through, the day, what is coming ──
         * The phone's calendar is an AGENDA, not a grid: a month of cells at
         * 360 dp is a month of dots. The week strip picks a day; the day lists
         * what is on it; "Coming up" is the next fortnight at a glance. */
        const long long today = hormiga::today_days();
        const long long monday = ph.day - weekday_mon0(ph.day);
        int y, m, d;
        civil_from_days(ph.day, y, m, d);
        ImGui::AlignTextToFramePadding();
        if (ImGui::ArrowButton("##prev", ImGuiDir_Left)) ph.day -= 7;
        ImGui::SameLine();
        ImGui::Text("%s %d", kMonth[m - 1], y);
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::GetFrameHeight());
        if (ImGui::ArrowButton("##next", ImGuiDir_Right)) ph.day += 7;
        // the week strip: seven equal cells, a dot under a day that has anything
        const float cw = ImGui::GetContentRegionAvail().x / 7.0f;
        const float ch = ImGui::GetFontSize() * 3.2f;
        const ImVec2 strip = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (int i = 0; i < 7; ++i) {
            const long long day = monday + i;
            int yy, mm, dd;
            civil_from_days(day, yy, mm, dd);
            ImGui::SetCursorScreenPos(ImVec2(strip.x + cw * i, strip.y));
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##day", ImVec2(cw, ch))) ph.day = day;
            ImGui::PopID();
            const ImVec2 c(strip.x + cw * (i + 0.5f), strip.y);
            const bool sel = day == ph.day, now = day == today;
            if (sel)
                dl->AddRectFilled(ImVec2(c.x - cw * 0.42f, c.y), ImVec2(c.x + cw * 0.42f, c.y + ch),
                                  ImGui::GetColorU32(ImGuiCol_ButtonActive), 10.0f * ph.dp);
            const ImU32 col = now && !sel ? ImGui::GetColorU32(ImGuiCol_CheckMark) : ImGui::GetColorU32(ImGuiCol_Text);
            const ImVec2 ws = ImGui::CalcTextSize(kWeekday[i]);
            dl->AddText(ImVec2(c.x - ws.x * 0.5f, c.y + 4), ImGui::GetColorU32(ImGuiCol_TextDisabled), kWeekday[i]);
            char num[4];
            std::snprintf(num, sizeof num, "%d", dd);
            const ImVec2 ns = ImGui::CalcTextSize(num);
            dl->AddText(ImVec2(c.x - ns.x * 0.5f, c.y + 6 + ws.y), col, num);
            if (!cal_entries_on(yy, mm, dd).empty())
                dl->AddCircleFilled(ImVec2(c.x, c.y + ch - 7), 3.0f, col);
        }
        ImGui::SetCursorScreenPos(ImVec2(strip.x, strip.y + ch + 8));

        // the day, and a one-line add for it ("Food drive 3pm-5pm")
        ImGui::SeparatorText((std::string(kWeekday[weekday_mon0(ph.day)]) + ", " + std::to_string(d) + " " +
                              kMonth[m - 1])
                                 .c_str());
        ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Add").x - ImGui::GetStyle().FramePadding.x * 2 - 10);
        const bool go = ImGui::InputTextWithHint("##quick", "Add: Food drive 3pm-5pm", ph.quick, sizeof ph.quick,
                                                 ImGuiInputTextFlags_EnterReturnsTrue);
        maiz::text_input_kind(maiz::InputKind::Text, maiz::InputAction::Go); // "Go" adds it
        ImGui::SameLine();
        if ((ImGui::Button("Add") || go) && ph.quick[0]) {
            hormiga::quick::Parsed qa;
            if (hormiga::quick::parse(ph.quick, qa)) {
                cal_new_dated(qa.glyph.c_str(), y, m, d, qa.t0, qa.t1, qa.name.c_str());
                ph.quick[0] = 0;
            } else {
                maiz::show_snackbar(ph.snack, "Nothing to name in that line");
            }
        }
        auto entry_row = [&](const CalEntry& e) {
            const maiz::SceneNode& n = *e.node;
            std::string when = hormiga::temper::field_value(n, "start_time");
            if (when.empty()) when = hormiga::temper::field_value(n, "time");
            if (when.empty()) when = "all day";
            ImGui::PushID(n.name.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(e.col));
            const bool hit = ImGui::Selectable(("##e" + n.name).c_str(), false, 0,
                                               ImVec2(0, ImGui::GetTextLineHeight() * 2.2f));
            ImGui::PopStyleColor();
            maiz::presence_item(surfaces, roster, net_settings.show, "calendar", n.id, maiz::Mark::Badge,
                                share_now && !share_now(n));
            const ImVec2 r0 = ImGui::GetItemRectMin();
            ImGui::GetWindowDrawList()->AddText(ImVec2(r0.x + 6, r0.y + 4), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                                when.c_str());
            ImGui::GetWindowDrawList()->AddText(ImVec2(r0.x + 6 + ImGui::CalcTextSize("00:00  all").x, r0.y + 4),
                                                e.incident ? IM_COL32(220, 90, 70, 255) : ImGui::GetColorU32(ImGuiCol_Text),
                                                ((e.incident ? "! " : "") + title_of(n)).c_str());
            ImGui::PopID();
            if (hit) stack.push("detail:" + n.name);
        };
        const auto today_list = cal_entries_on(y, m, d);
        if (today_list.empty()) ImGui::TextDisabled("Nothing on this day.");
        for (const auto& e : today_list) entry_row(e);

        ImGui::SeparatorText("Coming up");
        int shown = 0;
        for (long long day = std::max(today, ph.day + 1); day < std::max(today, ph.day + 1) + 14; ++day) {
            int yy, mm, dd;
            civil_from_days(day, yy, mm, dd);
            const auto list = cal_entries_on(yy, mm, dd);
            if (list.empty()) continue;
            ImGui::TextDisabled("%s %d %s", kWeekday[weekday_mon0(day)], dd, kMonth[mm - 1]);
            for (const auto& e : list) entry_row(e);
            ++shown;
        }
        if (!shown) ImGui::TextDisabled("Nothing in the next two weeks.");
    } else if (ph.tab == kTogether) {
        /* ── TOGETHER: who is here, the sync, joining ─────────────────────── */
        ImGui::SeparatorText("Here now");
        if (rt.present.empty()) ImGui::TextDisabled("Nobody else, right now.");
        for (const auto& [fp, a] : rt.present) {
            ImGui::PushID(fp.c_str());
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float sz = ImGui::GetFontSize() * 2.2f;
            auto av = rt.member_avatar.find(fp);
            LanRuntime::draw_avatar(*this, av != rt.member_avatar.end() ? av->second : "", a.user,
                                    LanRuntime::color_of(rt, fp), at.x, at.y, sz);
            ImGui::Dummy(ImVec2(sz, sz));
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextUnformatted(a.user.c_str());
            if (!a.selection.empty() && scene.find(a.selection.front())) {
                const maiz::SceneNode* on = scene.find(a.selection.front());
                ImGui::TextDisabled("on %s", title_of(*on).c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Go there")) {
                    ph.tab = kData;
                    ph.stacks[kData].reset();
                    ph.stacks[kData].push("detail:" + on->name);
                }
            } else {
                ImGui::TextDisabled("in %s", a.section.empty() ? "Hormiga" : a.section.c_str());
            }
            ImGui::EndGroup();
            ImGui::PopID();
        }
        LanRuntime::draw_sync_section(*this); // brings its own heading
        ImGui::SeparatorText("Join a shared database");
        LanRuntime::draw_discover_body(*this);
    } else {
        /* ── ME: the profile, the look, the database on this device ───────── */
        LanRuntime::draw_profile_body(*this);
        ImGui::SeparatorText("Look");
        if (ImGui::Button(light_mode ? ICON_FA_MOON "  Dark mode" : ICON_FA_SUN "  Light mode")) {
            light_mode = !light_mode;
            apply_theme();
        }
        ImGui::SeparatorText("This device");
        ImGui::TextDisabled("Database: %s", cur_miga.empty() ? state_name.c_str() : cur_miga.c_str());
        if (ImGui::Button(ICON_FA_FLOPPY_DISK "  Save now")) do_save();
        maiz::dim_wrapped("The Builder, the Map and the Antfarm are on the desktop. A phone keeps "
                          "the organization's people and dates with you, and in sync.");
    }
    ImGui::End();

    // the add button, Data's list only: the kinds a field worker adds
    if (ph.tab == kData && !is_detail) {
        static const std::vector<std::string> kAdd = {"Contact", "Organization", "Event"};
        static const char* kGlyph[] = {"contact", "organization", "event"};
        const int pick = maiz::speed_dial("##phone-add", ICON_FA_PLUS, kAdd, ph.dial_open);
        if (pick >= 0) {
            ph.dial_open = false;
            new_rune(kGlyph[pick]);                 // mints (device-scoped), tags, selects
            if (!ed.selection.empty()) stack.push("detail:" + ed.selection.front());
        }
    }

    for (const auto& c : out) dispatch_and_reproject(c);
    if (maiz::draw_snackbar(ph.snack)) dispatch_and_reproject("undo");

    draw_vault_modal();       // a joined database may carry sealed credentials
    draw_busy_overlay();
    draw_toasts();
    ImGui::PopStyleVar(4);
}
