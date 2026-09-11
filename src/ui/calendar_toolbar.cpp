/* ui/calendar_toolbar.cpp — the Calendar's chrome: the saved-view picker, the
 * granularity + stride navigation, jump-to-date, the kind/tag filter, and the
 * C1f quick-add bar.
 *
 * Split out of `calendar.cpp` 2026-09-10, alongside `calendar_export.cpp`, when
 * the 2026-09-10 UX pass pushed that file past its `tools/find_long.py` budget.
 * The seam: everything here draws the controls that decide WHAT and WHEN the
 * grid shows, and touches no cell, block or day. `draw_calendar_body()` draws
 * the grid and owns the selection. They meet through the `cal_*` members and
 * nothing else, which is the same arrangement that let the Calendar be split
 * out of `app.cpp` in the first place (Q30a).
 */
#include "app/app_internal.hpp"
#include "domain/quick_add.hpp" // C1f: the one-line creation grammar (pure)

void HormigaApp::draw_calendar_toolbar() {
    if (cal_year == 0) cal_today(cal_year, cal_month, cal_day);
    // ── C4d: saved calendar VIEWS (a `calview` = filter + kind + granularity,
    // named and persistent — so a "Public Events" view bakes the privacy
    // choice in). Gather them from the data the calendar reads. ──────────────
    std::vector<const maiz::SceneNode*> calviews;
    for (const auto& n : scene.nodes)
        if (n.glyph == "calview") calviews.push_back(&n);
    // ── C4d: the saved-view picker (ad-hoc + saved calendars) ──────────────
    ImGui::TextDisabled("view:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    std::string vlabel = cal_view.empty() ? "(ad-hoc)" : cal_view;
    if (ImGui::BeginCombo("##calview", vlabel.c_str())) {
        if (ImGui::Selectable("(ad-hoc)", cal_view.empty())) {
            cal_view.clear();
            cal_filter[0] = 0; cal_kind = 0;
        }
        for (auto* v : calviews) {
            std::string t = hormiga::temper::field_value(*v, "title");
            if (t.empty()) t = v->name;
            if (ImGui::Selectable(t.c_str(), cal_view == v->name))
                cal_apply_view(*v);
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (cal_view.empty()) { // ad-hoc → save the current filter as a new view
        if (ImGui::SmallButton("Save as view")) ImGui::OpenPopup("##savecalview");
    } else { // a view is active → write current filter/kind/mode back, or delete
        if (ImGui::SmallButton("Update")) {
            const char* km = cal_kind == 1 ? "events"
                             : cal_kind == 2 ? "incidents" : "all";
            const char* mm = cal_mode == 1 ? "week" : cal_mode == 2 ? "3day"
                             : cal_mode == 3 ? "agenda" : "month";
            pending_cmds.push_back(maiz::compile_commit(
                {"set " + cal_view + " filter " + json_str(cal_filter),
                 "set " + cal_view + " kind \"" + km + "\"",
                 "set " + cal_view + " mode \"" + mm + "\""}));
            toast("updated view '" + cal_view + "'");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Delete##cv")) {
            pending_cmds.push_back("rm " + cal_view);
            cal_view.clear();
            cal_filter[0] = 0; cal_kind = 0;
        }
    }
    if (ImGui::BeginPopup("##savecalview")) {
        ImGui::TextDisabled("save this filter as a named calendar view");
        ImGui::SetNextItemWidth(180);
        bool go = ImGui::InputTextWithHint("##cvname", "view name...",
                                           cal_view_name, sizeof cal_view_name,
                                           ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if ((ImGui::Button("Save") || go) && cal_view_name[0]) {
            std::string slug = cal_view_name;
            for (char& c : slug)
                if (!std::isalnum((unsigned char)c) && c != '-') c = '-';
            const char* km = cal_kind == 1 ? "events"
                             : cal_kind == 2 ? "incidents" : "all";
            const char* mm = cal_mode == 1 ? "week" : cal_mode == 2 ? "3day"
                             : cal_mode == 3 ? "agenda" : "month";
            pending_cmds.push_back(maiz::compile_commit(
                {"rune new calview " + slug,
                 "set " + slug + " title " + json_str(cal_view_name),
                 "set " + slug + " filter " + json_str(cal_filter),
                 "set " + slug + " kind \"" + km + "\"",
                 "set " + slug + " mode \"" + mm + "\""}));
            cal_view = slug;
            toast("saved calendar view '" + std::string(cal_view_name) + "'");
            cal_view_name[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("a saved view remembers its filter - a 'Public\n"
                          "Events' view can exclude incidents by design");
    ImGui::Separator();

    // ── toolbar: granularity, stride navigation, export ────────────────────
    const char* modes[] = {"Month", "Week", "3 days", "Agenda"};
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("##calmode", &cal_mode, modes, 4);
    ImGui::SameLine();
    int stride = cal_mode == 0 ? 0 : (cal_mode == 1 ? 7 : 3); // 0 = month step
    ImGui::BeginDisabled(cal_mode == 3); // agenda navigates from today
    if (ImGui::SmallButton("<")) {
        if (stride) cal_add_days(cal_year, cal_month, cal_day, -stride);
        else { cal_day = 1; if (--cal_month < 1) { cal_month = 12; --cal_year; } }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Today")) cal_today(cal_year, cal_month, cal_day);
    ImGui::SameLine();
    if (ImGui::SmallButton(">")) {
        if (stride) cal_add_days(cal_year, cal_month, cal_day, stride);
        else { cal_day = 1; if (++cal_month > 12) { cal_month = 1; ++cal_year; } }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    /* C2c: the month/year label is a BUTTON that jumps. It was static text, so
     * reaching next April meant clicking ">" seven times and reaching April of
     * a past year meant giving up. */
    {
        char anchor[48];
        std::snprintf(anchor, sizeof anchor, "%s %d, %d", kMonthNames[cal_month - 1],
                      cal_day, cal_year);
        if (ImGui::SmallButton(anchor)) {
            std::snprintf(cal_jump, sizeof cal_jump, "%04d-%02d-%02d", cal_year,
                          cal_month, cal_day);
            cal_jump_open = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("jump to a date  (or press G)\n"
                              "arrows move a day, shift+arrows a week,\n"
                              "PgUp/PgDn a month, Home = today");
    }
    if (cal_jump_open) { ImGui::OpenPopup("##caljump"); cal_jump_open = false; }
    if (ImGui::BeginPopup("##caljump")) {
        ImGui::TextDisabled("go to date");
        ImGui::SetNextItemWidth(120);
        ImGui::SetKeyboardFocusHere();
        bool go = ImGui::InputTextWithHint("##jumpd", "2026-12-01", cal_jump,
                                           sizeof cal_jump,
                                           ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::SmallButton("Go") || go) {
            /* Permissive on purpose: `2026-12-01`, `12/1/2026`, `12/1` (this
             * year) and a bare `14` (this month) all mean something obvious. */
            int y = 0, m = 0, d = 0;
            std::string j = cal_jump;
            for (char& c : j) if (c == '/' || c == '.') c = '-';
            int nf = std::sscanf(j.c_str(), "%d-%d-%d", &y, &m, &d);
            if (nf == 1) { d = y; m = cal_month; y = cal_year; }
            else if (nf == 2) { d = m; m = y; y = cal_year; }
            // "12/1/2026" scans as 12,1,2026 — month-first, not year-first
            else if (nf == 3 && y < 100) {
                int mo = y, da = m, yr = d;
                y = yr; m = mo; d = da;
            }
            if (nf >= 1 && m >= 1 && m <= 12 && d >= 1 && d <= cal_dim(y, m)) {
                cal_year = y; cal_month = m; cal_day = d;
                ImGui::CloseCurrentPopup();
            } else {
                toast("could not read '" + std::string(cal_jump) + "' as a date");
            }
        }
        ImGui::EndPopup();
    }
    if (cal_mode == 1 || cal_mode == 2) {
        ImGui::SameLine();
        ImGui::Checkbox("24h", &cal_full_day);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("off: the grid fits 06:00-22:00 and STRETCHES to\n"
                              "contain anything earlier or later on screen.\n"
                              "on: always the full day.");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Export PNG")) export_calendar_png();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("the month grid as a PNG (exports/) - for the\n"
                          "newsletter; the interactive web version is the\n"
                          "planned dynamic export");
    // ── C4a: the FILTER row — kind + tag query. Privacy-blocking: a published
    // calendar shows only what you choose (incidents can be sensitive). ──────
    ImGui::SetNextItemWidth(130);
    ImGui::Combo("##calkind", &cal_kind, "All kinds\0Events only\0Incidents only\0");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##calfilter", "tag filter (e.g. @summer)",
                             cal_filter, sizeof cal_filter);
    ImGui::SameLine();
    // the reusable tag picker appends to the AND-filter (no hand-typing AND/OR)
    std::string ftag = tag_picker("##calftag", cal_filter_tag, sizeof cal_filter_tag,
                                  "+ tag");
    if (!ftag.empty()) {
        std::string cur = cal_filter;
        std::string add = cur.empty() ? ftag : cur + " AND " + ftag;
        std::snprintf(cal_filter, sizeof cal_filter, "%s", add.c_str());
    }
    if (cal_filter[0]) {
        ImGui::SameLine();
        if (ImGui::SmallButton("clear##calf")) cal_filter[0] = 0;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", map_sel.empty()
                                  ? "(glyph colors)"
                                  : ("rules: " + map_sel).c_str());

    // ── C1f: QUICK-ADD. One line, Enter, done — the gesture the section was
    // missing. Targets the FOCUSED day (the anchor), which clicking a day cell
    // or the arrow keys move, so "where will this land" is always on screen. ──
    {
        char hint[96];
        std::snprintf(hint, sizeof hint, "quick add to %s %d  -  e.g. Food drive 3pm-5pm",
                      kMonthNames[cal_month - 1], cal_day);
        ImGui::SetNextItemWidth(330);
        if (cal_quick_focus) { ImGui::SetKeyboardFocusHere(); cal_quick_focus = false; }
        bool go = ImGui::InputTextWithHint("##calquick", hint, cal_quick,
                                           sizeof cal_quick,
                                           ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool click = ImGui::SmallButton("Add");
        if ((go || click) && cal_quick[0]) {
            hormiga::quick::Parsed q;
            if (hormiga::quick::parse(cal_quick, q)) {
                cal_new_dated(q.glyph.c_str(), cal_year, cal_month, cal_day,
                              q.t0, q.t1, q.name.c_str());
                cal_quick[0] = 0;
                if (go) cal_quick_focus = true; // Enter keeps the box hot
            } else {
                toast("nothing to name in that line - try 'Food drive 3pm-5pm'");
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Enter creates it on the focused day.\n"
                              "  Food drive 3pm-5pm   timed event\n"
                              "  Standup 9am          one hour from 9\n"
                              "  Volunteer training   all-day\n"
                              "  !Road closure 2pm    an INCIDENT, not an event\n"
                              "press N anywhere in the calendar to jump here");
    }
    ImGui::Separator();
}
