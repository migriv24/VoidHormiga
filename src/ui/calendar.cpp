/* section_calendar.cpp — the Calendar (okf/concepts/sections/calendar.md): dated runes
 * on a time grid. Split out of app.cpp 2026-08-17 (Q30a); see app_internal.hpp.
 *
 * The date arithmetic it rests on is shared (app_internal.hpp) because the
 * Builder's query blocks and Allomone's date predicates need the same civil
 * calendar; everything else about drawing a month is local to this file.
 */
#include "app/app_internal.hpp"
#include "stb_image_write.h" // decls only - the month exports as a PNG; the ONE implementation lives in app.cpp

// ── the CALENDAR (okf/concepts/sections/calendar.md): dated runes on a time grid,
// styled by the SAME rules engine as the map. Model grounds in RFC 5545
// (date/start_time/end_time ≈ DTSTART/DTEND; `days` ≈ RRULE-lite, future);
// view vocabulary grounds in FullCalendar (month/week/day grids). ──────────

/* Mint a dated rune on a day — the calendar as a CREATION tool (C1b/C1c).
 * Timed when t0/t1 are given (drag-created); all-day otherwise. ONE commit,
 * selected so the inspector opens ready for the name/details. */
void HormigaApp::cal_new_dated(const char* glyph, int y, int m, int d, float t0,
                               float t1) {
    std::string name;
    for (int i = 1;; ++i) {
        name = std::string(glyph) + "-" + std::to_string(i);
        if (!scene.find(name)) break;
    }
    char date[16];
    std::snprintf(date, sizeof date, "%04d-%02d-%02d", y, m, d);
    std::vector<std::string> cmds = {
        "rune new " + std::string(glyph) + " " + name,
        "set " + name + " date \"" + date + "\"",
        "tag " + name + " +type:" + glyph};
    if (t0 >= 0) {
        if (std::string(glyph) == "incident") // a POINT in time, not a span
            cmds.push_back("set " + name + " time \"" + cal_fmt_hhmm(t0) + "\"");
        else {
            cmds.push_back("set " + name + " start_time \"" + cal_fmt_hhmm(t0) +
                           "\"");
            cmds.push_back("set " + name + " end_time \"" + cal_fmt_hhmm(t1) +
                           "\"");
        }
    }
    pending_cmds.push_back(maiz::compile_commit(cmds));
    ed.selection = {name};
    toast("created " + name + " on " + date +
          (t0 >= 0 ? " " + cal_fmt_hhmm(t0) + "-" + cal_fmt_hhmm(t1) : "") +
          " - name it in the inspector");
}

/* Everything dated on one day, styled: the selected map view's rules give
 * icon/color (one engine, two surfaces), explicit tags win, glyph kinds
 * ground the defaults. Incidents are ALWAYS visibly distinct (directive). */
std::vector<HormigaApp::CalEntry> HormigaApp::cal_entries_on(int y, int m,
                                                             int d) const {
    std::vector<CalEntry> out;
    const maiz::SceneNode* view = scene.find(map_sel);
    std::vector<MapRule> rules;
    if (view) rules = parse_view_rules_of(*view);
    for (const auto& node : scene.nodes) {
        if (node.glyph == "map") continue;
        // `day` runes are date-keyed TAG holders, not scheduled entries — they
        // get a subtle cell indicator (cal_day_tags), never an entry row.
        if (node.glyph == "day") continue;
        int yy, mm, dd;
        std::string ds = hormiga::temper::field_value(node, "date");
        if (std::sscanf(ds.c_str(), "%d-%d-%d", &yy, &mm, &dd) != 3) continue;
        if (yy != y || mm != m || dd != d) continue;
        // C4a: kind + tag FILTER (privacy — a published calendar shows only
        // what's chosen; incidents can be sensitive, so hide them by default
        // of the kind filter, never by accident)
        if (cal_kind == 1 && node.glyph != "event") continue;
        if (cal_kind == 2 && node.glyph != "incident") continue;
        if (cal_filter[0] && !maiz::node_matches(cal_filter, node)) continue;
        CalEntry e{&node, nullptr, 0, node.glyph == "incident"};
        e.col = e.incident                ? IM_COL32(200, 50, 50, 255)
                : node.glyph == "event"   ? IM_COL32(63, 111, 174, 255)
                                          : IM_COL32(110, 110, 118, 255);
        for (const auto& r : rules) {
            if (r.tags.empty() || !maiz::node_matches(rule_expr_of(r), node))
                continue;
            for (const auto& c : kMarkerColors)
                if (r.color == c.tag) e.col = c.col;
            for (const auto& ic : kMarkerIcons)
                if (r.icon == ic.tag) e.icon = ic.glyph;
            break;
        }
        std::string ct = tag_value(node, "color"); // explicit tags win
        for (const auto& c : kMarkerColors)
            if (ct == c.tag) e.col = c.col;
        std::string it = tag_value(node, "icon");
        for (const auto& ic : kMarkerIcons)
            if (it == ic.tag) e.icon = ic.glyph;
        // ALLOMONE, the `cal` surface. `when upcoming "3" then cal-color "…"`
        // is the calendar rule the bespoke engine could never express, because
        // "three days from now" is not a tag — which is the whole argument for
        // the predicate seam, arriving on the surface that needed it most.
        if (const AlloStyle* st = allo_style_for("cal", node.name)) {
            if (st->has_color) e.col = st->rgba;
            for (const auto& ic : kMarkerIcons)
                if (st->icon == ic.tag) e.icon = ic.glyph;
        }
        out.push_back(e);
    }
    // Sorted by `cal.priority` DESCENDING first, then by clock. A day with
    // fifteen entries shows five; which five is a decision, and Max over
    // priority is how a script makes it — the loudest opinion wins, and
    // several scripts can shout without one silencing another.
    std::sort(out.begin(), out.end(), [this](const CalEntry& a, const CalEntry& b) {
        const AlloStyle* pa = allo_style_for("cal", a.node->name);
        const AlloStyle* pb = allo_style_for("cal", b.node->name);
        double wa = pa ? pa->priority : 0, wb = pb ? pb->priority : 0;
        if (wa != wb) return wa > wb;
        return hormiga::temper::field_value(*a.node, "start_time") <
               hormiga::temper::field_value(*b.node, "start_time");
    }); // all-day (empty time) first, then by clock
    return out;
}

/* Load a saved calendar view's settings into the live filter/kind/mode
 * (C4d). The active view name persists in cal_view so edits can write back. */
void HormigaApp::cal_apply_view(const maiz::SceneNode& v) {
    cal_view = v.name;
    std::snprintf(cal_filter, sizeof cal_filter, "%s",
                  hormiga::temper::field_value(v, "filter").c_str());
    std::string k = hormiga::temper::field_value(v, "kind");
    cal_kind = k == "events" ? 1 : k == "incidents" ? 2 : 0;
    std::string m = hormiga::temper::field_value(v, "mode");
    cal_mode = m == "week" ? 1 : m == "3day" ? 2 : m == "agenda" ? 3 : 0;
}

void HormigaApp::draw_calendar_body() {
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
    ImGui::Text("%s %d", kMonthNames[cal_month - 1], cal_year);
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
    ImGui::Separator();

    // ── the grid + an inspector pane when something is selected ────────────
    bool have_sel = !ed.selection.empty() && scene.find(ed.selection.front());
    ImGui::BeginChild("##calgrid", ImVec2(have_sel ? -300.0f : 0.0f, 0));
    int ty, tm2, td2;
    cal_today(ty, tm2, td2);
    // one entry widget everywhere (month cells + the all-day lane): select on
    // click, and C1a — the SAME context menu as a map marker, plus calendar-
    // specific "Unschedule"
    auto entry_row = [&](const CalEntry& e, bool with_time, float w) {
        ImGui::PushID(e.node->name.c_str());
        std::string lbl;
        if (e.incident) lbl += ICON_FA_TRIANGLE_EXCLAMATION " ";
        else if (e.icon) lbl += std::string(e.icon) + " ";
        if (with_time) {
            std::string st = hormiga::temper::field_value(*e.node, "start_time");
            if (!st.empty()) lbl += st + " ";
        }
        lbl += e.node->name;
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(e.col));
        if (ImGui::Selectable(lbl.c_str(), ed.selected(e.node->name), 0,
                              ImVec2(w, 0)))
            ed.selection = {e.node->name};
        ImGui::PopStyleColor();
        // C1d: drag an entry onto another day to RESCHEDULE it (month view)
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            const char* nm = e.node->name.c_str();
            ImGui::SetDragDropPayload("CAL_ENTRY", nm, e.node->name.size() + 1);
            ImGui::Text("move %s", nm);
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginPopupContextItem("##calentrymenu")) { // C1a
            marker_menu_items(*e.node);
            ImGui::Separator();
            if (ImGui::MenuItem("Unschedule (clear date)"))
                pending_cmds.push_back("set " + e.node->name + " date \"\"");
            ImGui::EndPopup();
        } else if (ImGui::IsItemHovered()) {
            std::string tip = e.node->glyph + "  (right-click for actions)";
            std::string en = hormiga::temper::field_value(*e.node, "end_time");
            if (!en.empty()) tip += "  until " + en;
            ImGui::SetTooltip("%s", tip.c_str());
        }
        ImGui::PopID();
    };
    if (cal_mode == 0) {
        // MONTH: 7 × rows grid; other-month cells blank; today tinted;
        // C1b — right-click a day's empty space creates on that date
        int first = cal_dow(cal_year, cal_month, 1);
        int dim = cal_dim(cal_year, cal_month);
        int rows = (first + dim + 6) / 7;
        if (ImGui::BeginTable("##month", 7,
                              ImGuiTableFlags_Borders |
                                  ImGuiTableFlags_SizingStretchSame)) {
            for (const char* dn : kDowNames) ImGui::TableSetupColumn(dn);
            ImGui::TableHeadersRow();
            float cell_h =
                std::max(48.0f, ImGui::GetContentRegionAvail().y / rows - 4);
            for (int r = 0; r < rows; ++r) {
                ImGui::TableNextRow(ImGuiTableRowFlags_None, cell_h);
                for (int c = 0; c < 7; ++c) {
                    ImGui::TableSetColumnIndex(c);
                    int dnum = r * 7 + c - first + 1;
                    if (dnum < 1 || dnum > dim) continue;
                    ImGui::PushID(dnum);
                    bool is_today = cal_year == ty && cal_month == tm2 && dnum == td2;
                    if (is_today)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                               IM_COL32(255, 236, 190, 100));
                    float cell_y0 = ImGui::GetCursorPosY();
                    ImGui::Text("%d", dnum);
                    { // C-day: subtle indicator for a tagged (event-less) day
                        char d[16];
                        std::snprintf(d, sizeof d, "%04d-%02d-%02d", cal_year,
                                      cal_month, dnum);
                        if (const maiz::SceneNode* dr =
                                scene.find("day-" + std::string(d))) {
                            std::string lbl;
                            for (const auto& t : dr->tags) {
                                if (t == "type:day") continue; // housekeeping
                                lbl += (lbl.empty() ? "# " : " ") + t;
                            }
                            if (!lbl.empty()) {
                                ImGui::PushStyleColor(ImGuiCol_Text,
                                                      IM_COL32(150, 138, 70, 255));
                                ImGui::TextWrapped("%s", lbl.c_str());
                                ImGui::PopStyleColor();
                            }
                        }
                    }
                    for (const auto& e : cal_entries_on(cal_year, cal_month, dnum))
                        entry_row(e, false, 0);
                    // the cell's empty remainder is the "create here" surface.
                    // BOUNDED by the row height — GetContentRegionAvail().y in
                    // a table cell reports the WINDOW's remainder, and an
                    // unbounded Dummy inflates the row to fill it (the
                    // month-view-never-expands bug, 2026-07-22)
                    float used = ImGui::GetCursorPosY() - cell_y0;
                    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x,
                                        std::max(8.0f, cell_h - used - 6)));
                    // C1d: this cell is a drop target — drop a dragged entry
                    // here to reschedule it to this day (one `set date`)
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* pl =
                                ImGui::AcceptDragDropPayload("CAL_ENTRY")) {
                            std::string nm((const char*)pl->Data);
                            char d[16];
                            std::snprintf(d, sizeof d, "%04d-%02d-%02d",
                                          cal_year, cal_month, dnum);
                            pending_cmds.push_back("set " + nm + " date \"" + d +
                                                   "\"");
                        }
                        ImGui::EndDragDropTarget();
                    }
                    ImGui::OpenPopupOnItemClick("##daymenu",
                                                ImGuiPopupFlags_MouseButtonRight);
                    if (ImGui::BeginPopup("##daymenu")) {
                        ImGui::TextDisabled("%s %d, %d",
                                            kMonthNames[cal_month - 1], dnum,
                                            cal_year);
                        ImGui::Separator();
                        if (ImGui::MenuItem("New event here (all-day)"))
                            cal_new_dated("event", cal_year, cal_month, dnum);
                        if (ImGui::MenuItem("New incident here"))
                            cal_new_dated("incident", cal_year, cal_month, dnum);
                        ImGui::Separator();
                        // Tag this day WITHOUT an event (C-day, 2026-08-04):
                        // ensure a `day-YYYY-MM-DD` rune exists, then open the
                        // shared tag editor over it next frame.
                        if (ImGui::MenuItem("Tag this day…")) {
                            char d[16];
                            std::snprintf(d, sizeof d, "%04d-%02d-%02d",
                                          cal_year, cal_month, dnum);
                            cal_daytag_date = d;
                            std::string rn = "day-" + std::string(d);
                            if (!scene.find(rn))
                                pending_cmds.push_back(maiz::compile_commit(
                                    {"rune new day " + rn,
                                     "set " + rn + " date \"" + std::string(d) +
                                         "\"",
                                     "set " + rn + " name \"" + std::string(d) +
                                         "\"",
                                     "tag " + rn + " +type:day"}));
                            cal_daytag_open = true;
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        // day-tag popup: opened at table scope so it isn't nested in the
        // transient day context menu. Draws the shared tag editor over the
        // `day-<date>` rune (created last frame by the menu item).
        if (cal_daytag_open) {
            ImGui::OpenPopup("Tag day");
            cal_daytag_open = false;
        }
        if (ImGui::BeginPopup("Tag day")) {
            ImGui::TextDisabled("%s", cal_daytag_date.c_str());
            ImGui::Separator();
            const maiz::SceneNode* dn = scene.find("day-" + cal_daytag_date);
            if (dn) {
                std::vector<std::string> dcmds;
                draw_tag_editor(*dn, dcmds);
                for (const auto& c : dcmds) pending_cmds.push_back(c);
                if (dn->tags.empty())
                    ImGui::TextDisabled("(no tags — a bare day rune. removing "
                                        "the last tag leaves it in the data.)");
            } else {
                ImGui::TextDisabled("creating day rune…");
            }
            ImGui::EndPopup();
        }
    } else if (cal_mode == 3) {
        // ── AGENDA (C2a): a chronological list of what's coming up, from
        // today forward, grouped by date — the newsletter's list twin, and
        // the quickest "what's next" scan. Honors the same filter + styling.
        ImGui::BeginChild("##agenda");
        int cy = ty, cm = tm2, cd = td2, total = 0;
        for (int i = 0; i < 180; ++i) {
            auto es = cal_entries_on(cy, cm, cd);
            if (!es.empty()) {
                bool is_today = (i == 0);
                char hdr[64];
                std::snprintf(hdr, sizeof hdr, "%s %s %d, %d%s",
                              kDowNames[cal_dow(cy, cm, cd)], kMonthNames[cm - 1],
                              cd, cy, is_today ? "  - today" : "");
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(theme_accent[0], theme_accent[1],
                                          theme_accent[2], 1.0f),
                                   "%s", hdr);
                ImGui::Separator();
                for (const auto& e : es) {
                    ImGui::Indent(8);
                    entry_row(e, true, 0);
                    ImGui::Unindent(8);
                }
                total += (int)es.size();
            }
            cal_add_days(cy, cm, cd, 1);
        }
        if (total == 0)
            ImGui::TextDisabled("nothing scheduled in the next 180 days"
                                "%s", cal_filter[0] || cal_kind ? " (with this "
                                                                  "filter)" : "");
        ImGui::EndChild();
    } else {
        // ── WEEK / 3-DAY: the TIME GRID (C1c — the author's meaning for these
        // views): hour rows 06:00–22:00, timed entries as positioned blocks,
        // today tinted, a red now-line — and DRAG on empty time to create an
        // event with real start/end times (snapped to :30). The input model is
        // the map canvas's: one InvisibleButton, drawlist, manual hit tests. ──
        int n = cal_mode == 1 ? 7 : 3;
        int sy = cal_year, sm = cal_month, sd = cal_day;
        if (cal_mode == 1) // week starts on its Sunday
            cal_add_days(sy, sm, sd, -cal_dow(sy, sm, sd));
        const float H0 = 6.0f, H1 = 22.0f, hour_h = 44.0f, gutter = 46.0f;
        float avail_w = ImGui::GetContentRegionAvail().x - 16; // scrollbar room
        float col_w = std::max(60.0f, (avail_w - gutter) / n);
        // day headers + the all-day lane (untimed entries as chips)
        {
            int cy = sy, cm = sm, cd = sd;
            for (int c = 0; c < n; ++c) {
                ImGui::SetCursorPosX(gutter + c * col_w + 4);
                if (c) ImGui::SameLine(gutter + c * col_w + 4);
                bool is_today = cy == ty && cm == tm2 && cd == td2;
                ImGui::TextColored(is_today ? ImVec4(0.85f, 0.55f, 0.1f, 1)
                                            : ImGui::GetStyle().Colors[ImGuiCol_Text],
                                   "%s %d", kDowNames[cal_dow(cy, cm, cd)], cd);
                cal_add_days(cy, cm, cd, 1);
            }
            cy = sy; cm = sm; cd = sd;
            for (int c = 0; c < n; ++c) { // all-day chips, up to two per column
                auto es = cal_entries_on(cy, cm, cd);
                int shown = 0;
                for (const auto& e : es) {
                    bool timed = cal_parse_hhmm(hormiga::temper::field_value(
                                     *e.node, "start_time")) >= 0 ||
                                 (e.incident &&
                                  cal_parse_hhmm(hormiga::temper::field_value(
                                      *e.node, "time")) >= 0);
                    if (timed) continue; // → lives on the grid below
                    ImGui::SetCursorPosX(gutter + c * col_w + 4);
                    entry_row(e, false, col_w - 10);
                    if (++shown >= 2) break;
                }
                cal_add_days(cy, cm, cd, 1);
            }
        }
        ImGui::BeginChild("##tgrid", ImVec2(0, 0));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        float grid_h = (H1 - H0) * hour_h;
        ImGui::InvisibleButton("##tgsurface", ImVec2(gutter + n * col_w, grid_h),
                               ImGuiButtonFlags_MouseButtonLeft |
                                   ImGuiButtonFlags_MouseButtonRight);
        bool hovered = ImGui::IsItemHovered();
        ImVec2 mouse = ImGui::GetIO().MousePos;
        auto col_of = [&](float x) { // day column under a screen x (-1 = none)
            int c = (int)((x - p0.x - gutter) / col_w);
            return (x < p0.x + gutter || c < 0 || c >= n) ? -1 : c;
        };
        auto hour_of = [&](float y) {
            return std::clamp(H0 + (y - p0.y) / hour_h, H0, H1);
        };
        auto day_of_col = [&](int c, int& y, int& m, int& d) {
            y = sy; m = sm; d = sd;
            cal_add_days(y, m, d, c);
        };
        // today's column tinted
        for (int c = 0; c < n; ++c) {
            int cy, cm, cd;
            day_of_col(c, cy, cm, cd);
            if (cy == ty && cm == tm2 && cd == td2)
                dl->AddRectFilled(ImVec2(p0.x + gutter + c * col_w, p0.y),
                                  ImVec2(p0.x + gutter + (c + 1) * col_w,
                                         p0.y + grid_h),
                                  IM_COL32(255, 236, 190, 45));
        }
        // hour rows + labels; column separators
        for (int h = (int)H0; h <= (int)H1; ++h) {
            float y = p0.y + (h - H0) * hour_h;
            dl->AddLine(ImVec2(p0.x + gutter, y),
                        ImVec2(p0.x + gutter + n * col_w, y),
                        IM_COL32(200, 200, 205, h % 12 == 0 ? 180 : 90));
            char hl[8];
            std::snprintf(hl, sizeof hl, "%02d:00", h);
            dl->AddText(ImVec2(p0.x + 2, y - 7), IM_COL32(120, 120, 125, 255), hl);
        }
        for (int c = 0; c <= n; ++c)
            dl->AddLine(ImVec2(p0.x + gutter + c * col_w, p0.y),
                        ImVec2(p0.x + gutter + c * col_w, p0.y + grid_h),
                        IM_COL32(200, 200, 205, 120));
        // the red NOW line
        {
            std::time_t t = std::time(nullptr);
            std::tm* lt = std::localtime(&t);
            float nowh = lt->tm_hour + lt->tm_min / 60.0f;
            for (int c = 0; c < n; ++c) {
                int cy, cm, cd;
                day_of_col(c, cy, cm, cd);
                if (cy == ty && cm == tm2 && cd == td2 && nowh >= H0 && nowh <= H1) {
                    float y = p0.y + (nowh - H0) * hour_h;
                    dl->AddLine(ImVec2(p0.x + gutter + c * col_w, y),
                                ImVec2(p0.x + gutter + (c + 1) * col_w, y),
                                IM_COL32(220, 60, 50, 220), 2.0f);
                }
            }
        }
        // timed entries as BLOCKS; manual hit test (map-canvas discipline)
        const maiz::SceneNode* hit = nullptr;
        float hit_t0 = 0, hit_t1 = 0, hit_bottom = 0;
        int hit_col = -1;
        for (int c = 0; c < n; ++c) {
            int cy, cm, cd;
            day_of_col(c, cy, cm, cd);
            for (const auto& e : cal_entries_on(cy, cm, cd)) {
                float t0 = cal_parse_hhmm(
                    hormiga::temper::field_value(*e.node, "start_time"));
                if (t0 < 0 && e.incident) // incidents carry `time`, a POINT
                    t0 = cal_parse_hhmm(
                        hormiga::temper::field_value(*e.node, "time"));
                if (t0 < 0) continue; // all-day → the lane above
                float t1 = cal_parse_hhmm(
                    hormiga::temper::field_value(*e.node, "end_time"));
                if (t1 <= t0) t1 = t0 + 1.0f;
                float y0 = p0.y + (std::clamp(t0, H0, H1) - H0) * hour_h;
                float y1 = p0.y + (std::clamp(t1, H0, H1) - H0) * hour_h;
                ImVec2 a(p0.x + gutter + c * col_w + 3, y0 + 1);
                ImVec2 b(p0.x + gutter + (c + 1) * col_w - 5, y1 - 1);
                bool sel = ed.selected(e.node->name);
                ImU32 fill = (e.col & 0x00FFFFFF) | (sel ? 0xE0000000 : 0x59000000);
                dl->AddRectFilled(a, b, fill, 4.0f);
                dl->AddRect(a, b, e.col, 4.0f, 0, e.incident ? 2.5f : 1.5f);
                std::string bl;
                if (e.incident) bl += "! ";
                bl += e.node->name;
                dl->AddText(ImVec2(a.x + 4, a.y + 2),
                            sel ? IM_COL32(255, 255, 255, 255)
                                : IM_COL32(30, 30, 35, 255),
                            bl.c_str());
                if (y1 - y0 > 30)
                    dl->AddText(ImVec2(a.x + 4, a.y + 18),
                                IM_COL32(90, 90, 95, 255),
                                (cal_fmt_hhmm(t0) + "-" + cal_fmt_hhmm(t1)).c_str());
                // a resize grip on the bottom edge (visual hint on hover)
                if (hovered && mouse.x >= a.x && mouse.x <= b.x &&
                    mouse.y >= b.y - 6 && mouse.y <= b.y + 2)
                    dl->AddLine(ImVec2(a.x + 4, b.y - 2), ImVec2(b.x - 4, b.y - 2),
                                IM_COL32(40, 40, 45, 200), 2.5f);
                if (hovered && mouse.x >= a.x && mouse.x <= b.x && mouse.y >= a.y &&
                    mouse.y <= b.y) {
                    hit = e.node;
                    hit_t0 = t0; hit_t1 = t1; hit_bottom = b.y; hit_col = c;
                }
            }
        }
        // input: click a block = select; drag empty = CREATE (snap :30);
        // right-click block = the entry menu; right-click empty = the day menu
        auto snap = [](float t) { return std::round(t * 2.0f) / 2.0f; };
        // grab: on a block → MOVE (or RESIZE if near its bottom edge); on empty
        // → CREATE. Incidents are a point in time (no resize/duration).
        if (ImGui::IsItemActivated() && hovered &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (hit) {
                cal_move = hit->name;
                cal_grab_t0 = hit_t0; cal_grab_t1 = hit_t1; cal_grab_col = hit_col;
                cal_grab_off = hour_of(mouse.y) - hit_t0;
                cal_resize = (mouse.y >= hit_bottom - 6) &&
                             hit->glyph != "incident";
            } else if (int c = col_of(mouse.x); c >= 0) {
                cal_drag_col = c;
                cal_drag_t0 = cal_drag_t1 = snap(hour_of(mouse.y));
            }
        }
        // MOVE / RESIZE preview + commit
        if (!cal_move.empty()) {
            const maiz::SceneNode* mv = scene.find(cal_move);
            float dur = cal_grab_t1 - cal_grab_t0;
            int mc;
            float nt0, nt1;
            if (cal_resize) { // bottom edge → new end (min 30 min), same day
                nt0 = cal_grab_t0;
                nt1 = std::max(nt0 + 0.5f, snap(hour_of(mouse.y)));
                mc = cal_grab_col;
            } else { // whole block → new start (+day), duration preserved
                int c = col_of(mouse.x);
                mc = c >= 0 ? c : cal_grab_col;
                nt0 = std::clamp(snap(hour_of(mouse.y) - cal_grab_off), H0, H1 - dur);
                nt1 = nt0 + dur;
            }
            // ghost at the new position
            float gy0 = p0.y + (nt0 - H0) * hour_h, gy1 = p0.y + (nt1 - H0) * hour_h;
            ImVec2 ga(p0.x + gutter + mc * col_w + 3, gy0);
            ImVec2 gb(p0.x + gutter + (mc + 1) * col_w - 5, gy1);
            dl->AddRectFilled(ga, gb, IM_COL32(46, 107, 79, 90), 4.0f);
            dl->AddRect(ga, gb, IM_COL32(46, 107, 79, 220), 4.0f);
            dl->AddText(ImVec2(ga.x + 4, ga.y + 2), IM_COL32(20, 50, 35, 255),
                        (cal_fmt_hhmm(nt0) + "-" + cal_fmt_hhmm(nt1)).c_str());
            if (ImGui::IsItemDeactivated() || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                bool changed = cal_resize ? (nt1 != cal_grab_t1)
                                          : (mc != cal_grab_col ||
                                             nt0 != cal_grab_t0);
                if (mv && changed) {
                    std::vector<std::string> cmds;
                    if (cal_resize) {
                        cmds.push_back("set " + cal_move + " end_time \"" +
                                       cal_fmt_hhmm(nt1) + "\"");
                    } else {
                        int ny, nm, nd; day_of_col(mc, ny, nm, nd);
                        char d[16];
                        std::snprintf(d, sizeof d, "%04d-%02d-%02d", ny, nm, nd);
                        cmds.push_back("set " + cal_move + " date \"" + d + "\"");
                        cmds.push_back("set " + cal_move + " start_time \"" +
                                       cal_fmt_hhmm(nt0) + "\"");
                        cmds.push_back("set " + cal_move + " end_time \"" +
                                       cal_fmt_hhmm(nt1) + "\"");
                    }
                    pending_cmds.push_back(maiz::compile_commit(cmds));
                } else if (mv && !changed) {
                    ed.selection = {cal_move}; // a click, not a drag → select
                }
                cal_move.clear();
            }
        }
        if (ImGui::IsItemActive() && cal_drag_col >= 0) {
            cal_drag_t1 = snap(hour_of(mouse.y));
            float a0 = std::min(cal_drag_t0, cal_drag_t1);
            float a1 = std::max(cal_drag_t0, cal_drag_t1);
            if (a1 > a0) { // live preview with the times it will commit
                float y0 = p0.y + (a0 - H0) * hour_h, y1 = p0.y + (a1 - H0) * hour_h;
                ImVec2 a(p0.x + gutter + cal_drag_col * col_w + 3, y0);
                ImVec2 b(p0.x + gutter + (cal_drag_col + 1) * col_w - 5, y1);
                dl->AddRectFilled(a, b, IM_COL32(63, 111, 174, 70), 4.0f);
                dl->AddRect(a, b, IM_COL32(63, 111, 174, 200), 4.0f);
                dl->AddText(ImVec2(a.x + 4, a.y + 2), IM_COL32(40, 60, 90, 255),
                            (cal_fmt_hhmm(a0) + " - " + cal_fmt_hhmm(a1) +
                             "  new event")
                                .c_str());
            }
        }
        if (ImGui::IsItemDeactivated() && cal_drag_col >= 0) {
            float a0 = std::min(cal_drag_t0, cal_drag_t1);
            float a1 = std::max(cal_drag_t0, cal_drag_t1);
            if (a1 - a0 >= 0.5f) { // a real drag (>= 30 min), not a click
                int cy, cm, cd;
                day_of_col(cal_drag_col, cy, cm, cd);
                cal_new_dated("event", cy, cm, cd, a0, a1);
            }
            cal_drag_col = -1;
        }
        // (block selection is handled by the move/resize path: a grab with no
        //  movement selects; a real drag moves or resizes)
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (hit) {
                cal_ctx_entry = hit->name;
                ImGui::OpenPopup("##tgentrymenu");
            } else if (int c = col_of(mouse.x); c >= 0) {
                day_of_col(c, cal_ctx_y, cal_ctx_m, cal_ctx_d);
                cal_drag_t0 = snap(hour_of(mouse.y)); // the clicked hour
                ImGui::OpenPopup("##tgdaymenu");
            }
        }
        if (ImGui::BeginPopup("##tgentrymenu")) {
            const maiz::SceneNode* mk = scene.find(cal_ctx_entry);
            if (!mk) ImGui::CloseCurrentPopup();
            else {
                marker_menu_items(*mk);
                ImGui::Separator();
                if (ImGui::MenuItem("Unschedule (clear date)"))
                    pending_cmds.push_back("set " + mk->name + " date \"\"");
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopup("##tgdaymenu")) {
            ImGui::TextDisabled("%04d-%02d-%02d at %s", cal_ctx_y, cal_ctx_m,
                                cal_ctx_d, cal_fmt_hhmm(cal_drag_t0).c_str());
            ImGui::Separator();
            if (ImGui::MenuItem("New 1h event here"))
                cal_new_dated("event", cal_ctx_y, cal_ctx_m, cal_ctx_d,
                              cal_drag_t0, cal_drag_t0 + 1.0f);
            if (ImGui::MenuItem("New incident here"))
                cal_new_dated("incident", cal_ctx_y, cal_ctx_m, cal_ctx_d,
                              cal_drag_t0, cal_drag_t0 + 1.0f);
            ImGui::TextDisabled("tip: drag on empty time to size an event");
            ImGui::EndPopup();
        }
        ImGui::EndChild();
    }
    ImGui::EndChild();
    if (have_sel) { // the one selection, the one inspector (shared with Data/Map)
        ImGui::SameLine();
        ImGui::BeginChild("##caldetail", ImVec2(0, 0), ImGuiChildFlags_Borders);
        if (ImGui::SmallButton("< back to calendar")) ed.selection.clear();
        ImGui::Separator();
        maiz::CanvasIO iio = maiz::draw_inspector(scene, ed, &widgets);
        for (const auto& cmd : iio.commands) pending_cmds.push_back(cmd);
        ImGui::EndChild();
    }
}

/* The static calendar export (the newsletter's month): the month grid composed
 * CPU-side like the map export — white grid, day numbers, rule-colored entry
 * chips, atlas-blitted text. WYSIWYG with the live view's styling. */
void HormigaApp::export_calendar_png() {
    if (cal_year == 0) cal_today(cal_year, cal_month, cal_day);
    ImFontBaked* font = ImGui::GetFontBaked();
    float font_px = font ? font->Size : 14.0f;
    unsigned char* atlas = nullptr;
    int aw = 0, ah = 0, abpp = 4;
    if (ImTextureData* tex = ImGui::GetIO().Fonts->TexRef._TexData)
        if (tex->Pixels) {
            atlas = tex->Pixels; aw = tex->Width; ah = tex->Height;
            abpp = tex->BytesPerPixel;
        }
    const int W = 1200, H = 850;
    std::vector<unsigned char> img((size_t)W * H * 3, 255);
    auto fill_rect = [&](int x0, int y0, int x1, int y1, unsigned char r,
                         unsigned char g, unsigned char b) {
        x0 = std::max(0, x0); y0 = std::max(0, y0);
        x1 = std::min(W, x1); y1 = std::min(H, y1);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                unsigned char* p = &img[((size_t)y * W + x) * 3];
                p[0] = r; p[1] = g; p[2] = b;
            }
    };
    auto stamp = [&](float px, float py, float scale, const std::string& text,
                     unsigned char tr, unsigned char tg, unsigned char tb) {
        if (!atlas || !font) return;
        float penx = px;
        for (unsigned char ch2 : text) {
            const ImFontGlyph* gl = font->FindGlyph((ImWchar)ch2);
            if (!gl) continue;
            if (gl->Visible) {
                float gx0 = penx + gl->X0 * scale, gy0 = py + gl->Y0 * scale;
                float gx1 = penx + gl->X1 * scale, gy1 = py + gl->Y1 * scale;
                for (int yy = (int)gy0; yy < (int)std::ceil(gy1); ++yy) {
                    if (yy < 0 || yy >= H) continue;
                    float v = (yy + 0.5f - gy0) / (gy1 - gy0);
                    int ay = std::clamp(
                        (int)((gl->V0 + v * (gl->V1 - gl->V0)) * ah), 0, ah - 1);
                    for (int xx = (int)gx0; xx < (int)std::ceil(gx1); ++xx) {
                        if (xx < 0 || xx >= W) continue;
                        float u = (xx + 0.5f - gx0) / (gx1 - gx0);
                        int ax = std::clamp(
                            (int)((gl->U0 + u * (gl->U1 - gl->U0)) * aw), 0,
                            aw - 1);
                        float a =
                            atlas[((size_t)ay * aw + ax) * abpp +
                                  (abpp == 4 ? 3 : 0)] / 255.0f;
                        if (a <= 0.02f) continue;
                        unsigned char* p = &img[((size_t)yy * W + xx) * 3];
                        p[0] = (unsigned char)(p[0] * (1 - a) + tr * a);
                        p[1] = (unsigned char)(p[1] * (1 - a) + tg * a);
                        p[2] = (unsigned char)(p[2] * (1 - a) + tb * a);
                    }
                }
            }
            penx += gl->AdvanceX * scale;
        }
    };
    // header: month title + weekday names
    char title[48];
    std::snprintf(title, sizeof title, "%s %d", kMonthNames[cal_month - 1],
                  cal_year);
    stamp(24, 18, 2.0f, title, 30, 30, 30);
    int first = cal_dow(cal_year, cal_month, 1);
    int dim = cal_dim(cal_year, cal_month);
    int rows = (first + dim + 6) / 7;
    const int gx = 12, gy = 84, gw = W - 24;
    const int gh = H - gy - 12;
    float cw = gw / 7.0f, chh = (float)gh / rows;
    for (int c = 0; c < 7; ++c)
        stamp(gx + c * cw + 8, gy - 22, 1.0f, kDowNames[c], 90, 90, 95);
    // grid lines
    for (int c = 0; c <= 7; ++c)
        fill_rect(gx + (int)(c * cw), gy, gx + (int)(c * cw) + 1, gy + gh, 205,
                  205, 208);
    for (int r = 0; r <= rows; ++r)
        fill_rect(gx, gy + (int)(r * chh), gx + gw, gy + (int)(r * chh) + 1,
                  205, 205, 208);
    // cells: day number + entry chips (color square + name; cap + "+N")
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < 7; ++c) {
            int dnum = r * 7 + c - first + 1;
            if (dnum < 1 || dnum > dim) continue;
            int cx0 = gx + (int)(c * cw), cy0 = gy + (int)(r * chh);
            char dn[8];
            std::snprintf(dn, sizeof dn, "%d", dnum);
            stamp(cx0 + 6, cy0 + 4, 1.0f, dn, 60, 60, 65);
            auto es = cal_entries_on(cal_year, cal_month, dnum);
            int line = 0, maxlines = (int)((chh - 26) / (font_px + 4));
            for (const auto& e : es) {
                if (line >= maxlines) {
                    char more[16];
                    std::snprintf(more, sizeof more, "+%d more",
                                  (int)es.size() - line);
                    stamp(cx0 + 6, cy0 + 24 + line * (font_px + 4), 0.9f, more,
                          120, 120, 125);
                    break;
                }
                float ey = cy0 + 24 + line * (font_px + 4);
                fill_rect(cx0 + 6, (int)ey + 2, cx0 + 15, (int)ey + 11,
                          e.col & 0xFF, (e.col >> 8) & 0xFF,
                          (e.col >> 16) & 0xFF);
                std::string nm = (e.incident ? "! " : "") + e.node->name;
                stamp(cx0 + 19, ey, 0.9f, nm, 40, 40, 45);
                ++line;
            }
        }
    std::error_code ec;
    fs::create_directories(data_dir("exports"), ec);
    char stampt[32];
    std::time_t t = std::time(nullptr);
    std::strftime(stampt, sizeof stampt, "%Y%m%d-%H%M%S", std::localtime(&t));
    char fn[64];
    std::snprintf(fn, sizeof fn, "calendar-%04d-%02d-%s.png", cal_year,
                  cal_month, stampt);
    fs::path out = data_dir("exports") / fn;
    if (stbi_write_png(out.string().c_str(), W, H, 3, img.data(), W * 3)) {
        toast("exported " + out.filename().string() +
              " - the newsletter's month");
        if (on_open) on_open(out.string());
    } else {
        toast("calendar export failed: " + out.string(), true);
    }
}

