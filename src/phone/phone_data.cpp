/* phone/phone_data.cpp — the phone's Data and Calendar screens, and one rune's
 * detail (moved out of phone.cpp on 2026-09-25, when the phone grew from four
 * screens to eight). What each screen is for: okf/concepts/sections/mobile.md. */
#include "phone/phone_ui.hpp"

#include "domain/date_query.hpp" // days_from_civil, today_days
#include "domain/quick_add.hpp"  // the calendar's one-line creation grammar

#include <cfloat>
#include <cmath>

using hormiga::phone::Stack;

namespace hormiga::phone {

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
    if (n.glyph == "note") { // the first line of the text
        std::string t = f("text");
        const auto nl = t.find('\n');
        return nl == std::string::npos ? t : t.substr(0, nl);
    }
    return n.label;
}

/* A kind's colour on the phone's cards: stable (from the name), soft enough for
 * white text on it, and different for the kinds an organization has most. */
ImU32 kind_colour(const std::string& glyph) {
    static const ImU32 k[] = {IM_COL32(74, 134, 217, 255), IM_COL32(64, 160, 120, 255), IM_COL32(214, 128, 52, 255),
                              IM_COL32(150, 102, 204, 255), IM_COL32(200, 80, 110, 255), IM_COL32(60, 150, 170, 255),
                              IM_COL32(170, 140, 50, 255),  IM_COL32(110, 120, 140, 255)};
    if (glyph == "contact") return k[0];
    if (glyph == "organization") return k[1];
    if (glyph == "event") return k[2];
    if (glyph == "note") return k[6];
    unsigned h = 2166136261u;
    for (char c : glyph) h = (h ^ (unsigned char)c) * 16777619u;
    return k[3 + h % 5];
}

const char* kind_icon(const std::string& glyph) {
    if (glyph == "contact") return ICON_FA_USER;
    if (glyph == "organization") return ICON_FA_BUILDING;
    if (glyph == "event") return ICON_FA_CALENDAR_DAYS;
    if (glyph == "incident") return ICON_FA_TRIANGLE_EXCLAMATION;
    if (glyph == "job") return ICON_FA_BRIEFCASE;
    if (glyph == "note") return ICON_FA_NOTE_STICKY;
    if (glyph == "image") return ICON_FA_IMAGE;
    return ICON_FA_TAG;
}

std::string kind_label(const std::string& glyph) {
    std::string s = glyph;
    if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    for (char& c : s)
        if (c == '_' || c == '-') c = ' ';
    return s;
}

std::string human_bytes(long long b) {
    char buf[32];
    if (b < 1024) std::snprintf(buf, sizeof buf, "%lld B", b);
    else if (b < 1024 * 1024) std::snprintf(buf, sizeof buf, "%.0f KB", b / 1024.0);
    else if (b < 1024LL * 1024 * 1024) std::snprintf(buf, sizeof buf, "%.1f MB", b / (1024.0 * 1024.0));
    else std::snprintf(buf, sizeof buf, "%.2f GB", b / (1024.0 * 1024.0 * 1024.0));
    return buf;
}

} // namespace hormiga::phone

namespace {

using namespace hormiga::phone;

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

/* ── DETAIL: one rune, every field it declares, its tags ─────────────────── */
void HormigaApp::PhoneUi::detail(HormigaApp& app, PhoneUi& ph, Frame& f, const maiz::SceneNode& n) {
    if (n.glyph == "contact" || n.glyph == "organization") {
        const float r = 34.0f * ph.dp;
        const ImVec2 c = ImGui::GetCursorScreenPos();
        app.draw_avatar(n, ImVec2(c.x + r, c.y + r), r);
        ImGui::Dummy(ImVec2(2 * r, 2 * r));
        ImGui::SameLine();
    }
    ImGui::BeginGroup();
    ImGui::TextUnformatted(title_of(n).c_str());
    ImGui::TextDisabled("%s", n.label.c_str());
    ImGui::EndGroup();
    maiz::presence_item(app.surfaces, app.roster, app.net_settings.show,
                        ph.screen == kCalendar ? "calendar" : "table:data", n.id, maiz::Mark::Badge,
                        app.share_now && !app.share_now(n));
    ImGui::Spacing();
    // LABELS ABOVE, fields full width: a phone reads down, not across. The
    // field's own right-hand label is pushed past the edge by the width.
    maiz::WidgetContext ctx{app.scene, f.out, std::string(), 0.0f};
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
        maiz::widget_field(ctx, app.widgets, n, fl); // tells the keyboard what the field is, too
        ImGui::PopClipRect();
        if (ImGui::IsItemActivated()) ImGui::SetScrollHereY(0.3f); // above the keyboard
        ImGui::PopID();
        ImGui::PopItemWidth();
    }
    ImGui::SeparatorText("Tags");
    app.draw_tag_editor(n, f.out);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.22f, 0.20f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH "  Delete", ImVec2(-FLT_MIN, 0))) {
        f.out.push_back("rm " + n.name);
        maiz::show_snackbar(ph.snack, "Deleted " + title_of(n), "UNDO");
        f.stack->pop();
    }
    ImGui::PopStyleColor();
}

/* ── DATA: search, kinds, a list you can swipe ──────────────────────────── */
void HormigaApp::PhoneUi::data(HormigaApp& app, PhoneUi& ph, Frame& f) {
    const float dp = ph.dp;
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##search", ICON_FA_MAGNIFYING_GLASS "  Search", ph.search, sizeof ph.search);
    maiz::text_input_kind(maiz::InputKind::Search); // the keyboard's action key says "search"
    std::map<std::string, int> counts;
    for (const auto& n : app.scene.nodes) counts[n.glyph]++;
    // kinds as a row of chips that scrolls sideways rather than wrapping
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 3.0f * ph.dp); // a hint that it scrolls, not a desktop bar
    ImGui::BeginChild("##kinds", ImVec2(0, ImGui::GetFrameHeightWithSpacing() + 6 * ph.dp), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    auto chip = [&](const char* label, const std::string& kind) {
        const bool on = ph.kind == kind;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label)) ph.kind = kind;
        if (on) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    chip("All", "");
    for (const auto& e : app.palette.entries)
        if (counts[e.glyph] > 0) chip((e.label + " " + std::to_string(counts[e.glyph])).c_str(), e.glyph);
    ImGui::EndChild();
    ImGui::PopStyleVar();

    const std::string q = lower(ph.search);
    std::vector<const maiz::SceneNode*> rows;
    for (const auto& n : app.scene.nodes) {
        bool data_kind = false;
        for (const auto& e : app.palette.entries) data_kind |= (e.glyph == n.glyph);
        if (!data_kind || (!ph.kind.empty() && n.glyph != ph.kind)) continue;
        if (!q.empty() && lower(title_of(n)).find(q) == std::string::npos && lower(n.name).find(q) == std::string::npos)
            continue;
        rows.push_back(&n);
    }
    std::sort(rows.begin(), rows.end(), [](const maiz::SceneNode* a, const maiz::SceneNode* b) {
        return lower(title_of(*a)) < lower(title_of(*b));
    });
    if (rows.empty())
        maiz::dim_wrapped(app.scene.nodes.empty() ? "Nothing here yet. Join a shared database on Migos, or add "
                                                    "something with +."
                                                  : "Nothing matches.");
    /* CARDS, ONE LAYOUT (the author, 2026-09-25: "list view vs card view
     * should just not exist, and card view should be the default"). A card
     * is a finger-sized target (~76 dp) with the person's face or the kind's
     * icon, the name at a size you can read at arm's length, one line of
     * context, and the kind in the corner. Tap opens it; swipe left deletes.
     * Drawn over the swipe row's own button, so the gestures are unchanged. */
    const float card_h = 76.0f * dp, pad = 12.0f * dp;
    for (const maiz::SceneNode* n : rows) {
        const int act = maiz::begin_swipe_row(ph.swipe, n->name.c_str(), {"Delete"}, card_h);
        // a TAP, not a swipe: released, and never dragged past a few pixels
        const bool tapped = ImGui::IsItemDeactivated() && ImGui::GetIO().MouseDragMaxDistanceSqr[0] < 36.0f * dp * dp;
        const bool held = ImGui::IsItemActive();
        // the card IS the row's button: read its rectangle before anything else
        // submits an item (presence_item does, and was measured by mistake)
        const ImVec2 r0 = ImGui::GetItemRectMin(), r1 = ImGui::GetItemRectMax();
        maiz::presence_item(app.surfaces, app.roster, app.net_settings.show, "table:data", n->id, maiz::Mark::Badge,
                            app.share_now && !app.share_now(*n));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float round = 12.0f * dp;
        dl->AddRectFilled(r0, r1, ImGui::GetColorU32(held ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg), round);
        dl->AddRectFilled(ImVec2(r0.x, r0.y), ImVec2(r0.x + 4.0f * dp, r1.y), kind_colour(n->glyph), round,
                          ImDrawFlags_RoundCornersLeft); // the kind, at a glance
        const float r = 22.0f * dp, cy = (r0.y + r1.y) * 0.5f;
        const ImVec2 face(r0.x + pad + r, cy);
        if (n->glyph == "contact" || n->glyph == "organization") {
            app.draw_avatar(*n, face, r);
        } else {
            dl->AddCircleFilled(face, r, kind_colour(n->glyph));
            const char* icon = kind_icon(n->glyph);
            const ImVec2 is = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * 1.1f, FLT_MAX, 0, icon);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 1.1f, ImVec2(face.x - is.x * 0.5f, face.y - is.y * 0.5f),
                        IM_COL32(255, 255, 255, 235), icon);
        }
        const float tx = face.x + r + pad;
        const float title_px = ImGui::GetFontSize() * 1.12f, sub_px = ImGui::GetFontSize() * 0.92f;
        const std::string kind = kind_label(n->glyph);
        const float kind_w = ImGui::GetFont()->CalcTextSizeA(sub_px, FLT_MAX, 0, kind.c_str()).x;
        dl->PushClipRect(ImVec2(tx, r0.y), ImVec2(r1.x - pad - kind_w - pad, r1.y), true);
        dl->AddText(ImGui::GetFont(), title_px, ImVec2(tx, cy - title_px - 1.0f * dp), ImGui::GetColorU32(ImGuiCol_Text),
                    title_of(*n).c_str());
        dl->AddText(ImGui::GetFont(), sub_px, ImVec2(tx, cy + 3.0f * dp), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                    subtitle_of(*n).c_str());
        dl->PopClipRect();
        dl->AddText(ImGui::GetFont(), sub_px, ImVec2(r1.x - pad - kind_w, r0.y + pad * 0.8f),
                    ImGui::GetColorU32(ImGuiCol_TextDisabled), kind.c_str());
        // begin_swipe_row leaves the cursor mid-row, for widget content; the card
        // is drawn, not laid out, so the next one starts at this one's bottom edge
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + ImGui::GetStyle().WindowPadding.x, r1.y));
        maiz::end_swipe_row(ph.swipe); // its spacing is the gap between cards
        if (act == 0) {
            f.out.push_back("rm " + n->name);
            maiz::show_snackbar(ph.snack, "Deleted " + title_of(*n), "UNDO");
        } else if (tapped && ph.swipe.open_id.empty()) {
            f.stack->push("detail:" + n->name);
        }
    }
    ImGui::Dummy(ImVec2(0, card_h)); // room under the last card for the add button
}

/* ── CALENDAR: a week you can step through, the day, what is coming ────────
 * The phone's calendar is an AGENDA, not a grid: a month of cells at 360 dp is
 * a month of dots. The week strip picks a day; the day lists what is on it;
 * "Coming up" is the next fortnight at a glance. */
void HormigaApp::PhoneUi::calendar(HormigaApp& app, PhoneUi& ph, Frame& f) {
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
        if (!app.cal_entries_on(yy, mm, dd).empty()) dl->AddCircleFilled(ImVec2(c.x, c.y + ch - 7), 3.0f, col);
    }
    ImGui::SetCursorScreenPos(ImVec2(strip.x, strip.y + ch + 8));

    // the day, and a one-line add for it ("Food drive 3pm-5pm")
    ImGui::SeparatorText(
        (std::string(kWeekday[weekday_mon0(ph.day)]) + ", " + std::to_string(d) + " " + kMonth[m - 1]).c_str());
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Add").x - ImGui::GetStyle().FramePadding.x * 2 - 10);
    const bool go = ImGui::InputTextWithHint("##quick", "Add: Food drive 3pm-5pm", ph.quick, sizeof ph.quick,
                                             ImGuiInputTextFlags_EnterReturnsTrue);
    maiz::text_input_kind(maiz::InputKind::Text, maiz::InputAction::Go); // "Go" adds it
    ImGui::SameLine();
    if ((ImGui::Button("Add") || go) && ph.quick[0]) {
        hormiga::quick::Parsed qa;
        if (hormiga::quick::parse(ph.quick, qa)) {
            app.cal_new_dated(qa.glyph.c_str(), y, m, d, qa.t0, qa.t1, qa.name.c_str());
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
        const bool hit =
            ImGui::Selectable(("##e" + n.name).c_str(), false, 0, ImVec2(0, ImGui::GetTextLineHeight() * 2.2f));
        ImGui::PopStyleColor();
        maiz::presence_item(app.surfaces, app.roster, app.net_settings.show, "calendar", n.id, maiz::Mark::Badge,
                            app.share_now && !app.share_now(n));
        const ImVec2 r0 = ImGui::GetItemRectMin();
        ImGui::GetWindowDrawList()->AddText(ImVec2(r0.x + 6, r0.y + 4), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                            when.c_str());
        ImGui::GetWindowDrawList()->AddText(ImVec2(r0.x + 6 + ImGui::CalcTextSize("00:00  all").x, r0.y + 4),
                                            e.incident ? IM_COL32(220, 90, 70, 255) : ImGui::GetColorU32(ImGuiCol_Text),
                                            ((e.incident ? "! " : "") + title_of(n)).c_str());
        ImGui::PopID();
        if (hit) f.stack->push("detail:" + n.name);
    };
    const auto today_list = app.cal_entries_on(y, m, d);
    if (today_list.empty()) ImGui::TextDisabled("Nothing on this day.");
    for (const auto& e : today_list) entry_row(e);

    ImGui::SeparatorText("Coming up");
    int shown = 0;
    for (long long day = std::max(today, ph.day + 1); day < std::max(today, ph.day + 1) + 14; ++day) {
        int yy, mm, dd;
        civil_from_days(day, yy, mm, dd);
        const auto list = app.cal_entries_on(yy, mm, dd);
        if (list.empty()) continue;
        ImGui::TextDisabled("%s %d %s", kWeekday[weekday_mon0(day)], dd, kMonth[mm - 1]);
        for (const auto& e : list) entry_row(e);
        ++shown;
    }
    if (!shown) ImGui::TextDisabled("Nothing in the next two weeks.");
}
