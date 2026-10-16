/* ui/calendar_panels.cpp — the Calendar's windows beside the grid (2026-10-15):
 * Inspector, Upcoming, Views, Export. Docked as tabs by `draw_calendar_body`
 * the first time, like the map's four (ui/map_panels.cpp), and free after.
 *
 *   Inspector  the selection, through the same form as Data and the map
 *              (ui/inspector.cpp); with nothing selected, the focused day:
 *              what is on it, and a way to add to it.
 *   Upcoming   the next two months of what the filter shows, by day.
 *   Views      saved views and the filter (ui/calendar_toolbar.cpp).
 *   Export     a picture, an .ics file, and a calendar block in a document.
 */
#include "app/app_internal.hpp"
#include "voidmaiz/mobile.hpp" // dim_wrapped

#include <cfloat>

namespace {
const char* kWeekday[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
} // namespace

void HormigaApp::draw_calendar_panels() {
    // one entry as a row: its colour, its title, and (when timed) its time
    auto entry_row = [&](const CalEntry& e, bool selected) {
        const maiz::SceneNode& n = *e.node;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float h = ImGui::GetTextLineHeight();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x, p.y + 2), ImVec2(p.x + 4, p.y + h + 2), e.col, 2.0f);
        ImGui::SetCursorScreenPos(ImVec2(p.x + 10, p.y));
        const std::string t = hormiga::temper::field_value(n, e.incident ? "time" : "start_time");
        std::string label = (e.icon ? std::string(e.icon) + "  " : std::string()) + rune_title(n);
        if (!t.empty()) label = clock_label(t, clock24) + "   " + label;
        if (e.incident) label += "   (incident)";
        ImGui::PushID(n.name.c_str());
        const bool hit = ImGui::Selectable(label.c_str(), selected);
        ImGui::PopID();
        return hit;
    };
    // ── INSPECTOR: the selection, or the focused day ─────────────────────────
    ImGui::Begin("Inspector##cal");
    {
        const maiz::SceneNode* sn =
            ed.selection.size() == 1 ? scene.find(ed.selection.front()) : nullptr;
        if (sn) {
            ImGui::TextUnformatted(rune_title(*sn).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", hormiga::kinds::current().title(sn->glyph).c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("deselect").x -
                            ImGui::GetStyle().FramePadding.x * 2);
            if (ImGui::SmallButton("deselect")) ed.selection.clear();
            std::vector<std::string> out;
            draw_save_check(*sn, out);
            ImGui::Separator();
            draw_rune_form(*sn, out, FormAt::Calendar);
            for (auto& c : out) pending_cmds.push_back(std::move(c));
        } else {
            if (cal_year == 0) cal_today(cal_year, cal_month, cal_day);
            char iso[16];
            std::snprintf(iso, sizeof iso, "%04d-%02d-%02d", cal_year, cal_month, cal_day);
            ImGui::Text(ICON_FA_CALENDAR_DAY "  %s, %s %d, %d", kWeekday[cal_dow(cal_year, cal_month, cal_day)],
                        kMonthNames[cal_month - 1], cal_day, cal_year);
            ImGui::Separator();
            const auto es = cal_entries_on(cal_year, cal_month, cal_day);
            if (es.empty()) ImGui::TextDisabled("nothing on this day");
            for (const auto& e : es)
                if (entry_row(e, false)) ed.selection = {e.node->name};
            ImGui::Spacing();
            if (ImGui::Button(ICON_FA_PLUS "  New event on this day", ImVec2(-FLT_MIN, 0)))
                cal_new_dated("event", cal_year, cal_month, cal_day);
            if (ImGui::Button(ICON_FA_TRIANGLE_EXCLAMATION "  New incident on this day", ImVec2(-FLT_MIN, 0)))
                cal_new_dated("incident", cal_year, cal_month, cal_day);
            ImGui::Spacing();
            maiz::dim_wrapped("Click a day on the grid to focus it, or an entry to see it here. "
                              "Drag on empty time in the week view to make an event that long.");
        }
    }
    ImGui::End();

    // ── UPCOMING: what the filter shows, the next two months, by day ─────────
    ImGui::Begin("Upcoming##cal");
    {
        int y, m, d;
        cal_today(y, m, d);
        int shown = 0;
        for (int i = 0; i < 62 && shown < 80; ++i) {
            const auto es = cal_entries_on(y, m, d);
            if (!es.empty()) {
                char hdr[64];
                std::snprintf(hdr, sizeof hdr, "%s, %s %d%s", kWeekday[cal_dow(y, m, d)], kMonthNames[m - 1], d,
                              i == 0 ? "  - today" : i == 1 ? "  - tomorrow" : "");
                ImGui::SeparatorText(hdr);
                for (const auto& e : es) {
                    if (entry_row(e, ed.selected(e.node->name))) {
                        ed.selection = {e.node->name};
                        cal_year = y, cal_month = m, cal_day = d; // the grid follows
                    }
                    ++shown;
                }
            }
            cal_add_days(y, m, d, 1);
        }
        if (!shown) maiz::dim_wrapped("Nothing in the next two months that the filter shows.");
    }
    ImGui::End();

    // ── VIEWS: saved views and the filter ────────────────────────────────────
    ImGui::Begin("Views##cal");
    draw_calendar_views();
    ImGui::End();

    // ── EXPORT ───────────────────────────────────────────────────────────────
    ImGui::Begin("Export##cal");
    {
        maiz::dim_wrapped("Every export takes what the calendar is showing now: its view and its filter "
                          "decide what leaves this computer.");
        ImGui::SeparatorText(ICON_FA_IMAGE "  A picture (PNG)");
        if (ImGui::Button("Export the month as a picture", ImVec2(-FLT_MIN, 0))) export_calendar_png();
        maiz::dim_wrapped("The month grid as an image, for a newsletter or a printed flier.");
        ImGui::SeparatorText(ICON_FA_FILE_EXPORT "  A file any calendar imports (.ics)");
        if (ImGui::Button("Export an .ics file", ImVec2(-FLT_MIN, 0))) export_calendar_ics();
        maiz::dim_wrapped("Google, Apple, Outlook and Nextcloud import it. Each entry goes by its title.");
        ImGui::SeparatorText(ICON_FA_GLOBE "  On a website, or in a newsletter");
        const std::vector<std::string> docs = list_documents();
        if (docs.empty()) {
            ImGui::TextDisabled("no documents yet (the Builder makes them)");
        } else {
            cal_doc_pick = std::clamp(cal_doc_pick, 0, (int)docs.size() - 1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##caldoc", docs[(size_t)cal_doc_pick].c_str())) {
                for (int i = 0; i < (int)docs.size(); ++i)
                    if (ImGui::Selectable(docs[(size_t)i].c_str(), i == cal_doc_pick)) cal_doc_pick = i;
                ImGui::EndCombo();
            }
            const std::string& doc = docs[(size_t)cal_doc_pick];
            if (ImGui::Button((std::string(ICON_FA_PLUS "  Add this calendar to ") + doc).c_str(),
                              ImVec2(-FLT_MIN, 0))) {
                // the Builder's own verb, in the document's mantle, at its end
                maiz::ProjectOptions po;
                po.mantle = doc;
                const maiz::Scene ds = maiz::project_scene(core, po);
                int maxrow = -1;
                for (const auto& n : ds.nodes) maxrow = std::max(maxrow, hormiga::doc_field_int(n, "row", -1));
                const std::string name = mint_name("calendar-block");
                std::string q = cal_filter;
                if (cal_kind == 2) q = q.empty() ? "type:incident" : "type:incident AND " + q;
                pending_cmds.push_back("use " + doc);
                pending_cmds.push_back("doc place calendar_embed " + name + " " + std::to_string(maxrow + 1));
                if (!q.empty()) pending_cmds.push_back("set " + name + " query " + text_arg(q));
                pending_cmds.push_back("set " + name + " mode " + text_arg(cal_mode == 0 ? "month" : "agenda"));
                pending_cmds.push_back(std::string("use ") + kDataMantle);
                toast("added this calendar to " + doc + ", at the end - move it in the Builder");
            }
            maiz::dim_wrapped("On a website, a calendar visitors can page through and subscribe to; in a "
                              "newsletter, a list or a month. Events only unless the filter says otherwise.");
        }
    }
    ImGui::End();
}
