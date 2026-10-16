/* ui/inspector.cpp — a rune's form, as a list of widgets (2026-10-15).
 *
 * The author, after an afternoon of real use:
 *
 *   - *"'size (w,h)' does not need to be a visible property of an event ... it
 *     can go in some advanced properties section"*;
 *   - *"in the data tab while looking at an event, the date should just be
 *     plainly written as text, with a button to 'edit in calendar view' ... We
 *     should focus more on presenting that information in a more appealing way
 *     to the user, rather than confusing them with input fields"*;
 *   - *"instead of manually giving a color tag, we should always have a '+'
 *     button at the end ... the list of field entries ... should be a list of
 *     widgets (a vertical list, auto adjust stuff so it looks good). And we can
 *     add a widget for different style tags, such as adding a color wheel"*;
 *   - *"I really want this in the inspector in the map tab as well"*; and for
 *     shapes, a label that can be *"bigger, different colors, bold, italic"*,
 *     placed at any of nine positions, and *"the 'canvas it is drawn' field
 *     makes no sense"*.
 *
 * So the Data tab's detail pane, the map's Inspector and the calendar's
 * Inspector draw ONE form, top to bottom:
 *
 *   1. WHEN, for anything dated: the date and times as a sentence, never as
 *      inputs, and a way to the calendar, where moving a thing is a drag;
 *   2. the everyday fields, each a card with its name above a full-width
 *      editor, the title first;
 *   3. STYLE WIDGETS, which are tags (`color:`, `icon:`, `shape:`) and, for a
 *      map shape, its label, each a card that can be removed, and a "+" at the
 *      end that adds one;
 *   4. tags;
 *   5. ADVANCED PROPERTIES, closed: every raw field that the cards above
 *      present, still editable, plus the node size nobody reads.
 *
 * Nothing here is a new kind of edit. Every card ends in the same `set`/`tag`
 * commands the old inspector sent, collected into `out` for the caller to
 * dispatch after the frame (a dispatch mid-form would dangle `n`).
 */
#include "app/app_internal.hpp"
#include "domain/bestow.hpp" // a shape giving its colour
#include "domain/naming.hpp" // which field is the title
#include "domain/rrule.hpp"  // a recurrence in words

#include <cfloat>
#include <set>

namespace {

/* Fields that are not everyday: dates and times are PRESENTED (the When card),
 * positions belong to the map, and the rest is plumbing or legacy. */
bool advanced_key(const std::string& k) {
    static const std::set<std::string> adv = {
        "date",     "days",     "start_time", "end_time", "time",   "rrule",  "ext_uid",
        "geo",      "geo1",     "geo2",       "ref",      "ref_off", "icon_url", "image_url",
        "canvas",   "label_css", "bio",       "color",    "avatar"};
    return adv.count(k) || k.rfind("geo_", 0) == 0;
}

// a shape's label and its styling are the Label widget, not loose fields
bool label_key(const std::string& k) { return k == "label" || k.rfind("label_", 0) == 0; }

std::string long_date(const std::string& iso) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3 || m < 1 || m > 12 || d < 1 ||
        d > cal_dim(y, m))
        return iso;
    static const char* wd[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    char b[64];
    std::snprintf(b, sizeof b, "%s, %s %d, %d", wd[cal_dow(y, m, d)], kMonthNames[m - 1], d, y);
    return b;
}

/* One widget: a rounded, bordered card as tall as what is in it, with its name
 * on top. Returns true the frame its remove button is pressed. */
bool card_begin(const char* id, const char* caption, bool removable = false) {
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::BeginChild("##card", ImVec2(-FLT_MIN, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                          ImGuiChildFlags_AlwaysUseWindowPadding);
    if (*caption) ImGui::TextDisabled("%s", caption);
    bool rm = false;
    if (removable) {
        ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
        rm = ImGui::SmallButton(ICON_FA_XMARK);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("remove this widget (and the tag it is)");
    }
    return rm;
}
void card_end() {
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

/* The named colours as a row of swatches, then the wheel behind "Custom".
 * Returns the picked value ("" = nothing this frame): a name, or six hex
 * digits. `cur` is what is set now, outlined. */
std::string color_row(const std::string& cur, bool& wheel_open) {
    std::string picked;
    unsigned curcol = IM_COL32(60, 110, 180, 255);
    const bool have = color_value(cur, curcol);
    const float sw = ImGui::GetFrameHeight();
    float x = 0;
    for (const auto& c : kMarkerColors) {
        if (x > 0 && x + sw > ImGui::GetContentRegionAvail().x) x = 0;
        else if (x > 0) ImGui::SameLine();
        const bool on = cur == c.tag;
        if (on) ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
        if (ImGui::ColorButton(c.tag, ImGui::ColorConvertU32ToFloat4(c.col), ImGuiColorEditFlags_NoTooltip,
                               ImVec2(sw, sw)))
            picked = c.tag;
        if (on) ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", c.tag);
        x += sw + ImGui::GetStyle().ItemSpacing.x;
    }
    if (ImGui::SmallButton(wheel_open ? "Hide the wheel" : "Custom colour...")) wheel_open = !wheel_open;
    if (have) {
        ImGui::SameLine();
        ImGui::ColorButton("##now", ImGui::ColorConvertU32ToFloat4(curcol), 0, ImVec2(sw * 1.6f, sw * 0.7f));
    }
    unsigned out = curcol;
    if (wheel_open && color_wheel("##wheel", curcol, out)) {
        const std::string t = color_tag_of(out); // "color:rrggbb"
        picked = t.substr(6);
    }
    return picked;
}

} // namespace

void HormigaApp::open_in_calendar(const std::string& rune, const std::string& iso, bool timed) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) == 3 && m >= 1 && m <= 12 && d >= 1 && d <= cal_dim(y, m)) {
        cal_year = y;
        cal_month = m;
        cal_day = d;
    }
    if (timed && cal_mode == 0) cal_mode = 1; // a time is moved by dragging it, in a week
    win_calendar = true;
    cal_focus_next = true;
    ed.selection = {rune};
}

/* ── 1. WHEN ─────────────────────────────────────────────────────────────── */
void HormigaApp::draw_when_card(const maiz::SceneNode& n, FormAt at) {
    const hormiga::kinds::Kind* k = hormiga::kinds::current().find(n.glyph);
    const bool incident = n.glyph == "incident";
    if (!(incident || n.glyph == "event" || (k && k->dated))) return;
    std::string dfield = "date";
    if (k && !k->builtin && !k->date_field.empty()) dfield = k->date_field;
    const std::string date = hormiga::temper::field_value(n, dfield.c_str());
    const std::string t0 = hormiga::temper::field_value(n, incident ? "time" : "start_time");
    const std::string t1 = incident ? std::string() : hormiga::temper::field_value(n, "end_time");
    const std::string days = hormiga::temper::field_value(n, "days");
    const std::string rr = hormiga::temper::field_value(n, "rrule");
    const std::string venue = hormiga::temper::field_value(n, "venue");

    card_begin("w-when", "When");
    if (date.empty() && days.empty()) {
        ImGui::TextDisabled(ICON_FA_CALENDAR "  No date yet");
    } else if (!date.empty()) {
        ImGui::Text(ICON_FA_CALENDAR "  %s", long_date(date).c_str());
    }
    if (!date.empty() || !days.empty()) {
        if (!t0.empty())
            ImGui::Text(ICON_FA_CLOCK "  %s%s%s", clock_label(t0, clock24).c_str(), t1.empty() ? "" : "  to  ",
                        t1.empty() ? "" : clock_label(t1, clock24).c_str());
        else
            ImGui::TextDisabled(ICON_FA_CLOCK "  All day");
    }
    if (!rr.empty())
        ImGui::Text(ICON_FA_REPEAT "  Repeats %s", hormiga::rrule::describe(hormiga::rrule::parse(rr)).c_str());
    else if (!days.empty())
        ImGui::Text(ICON_FA_REPEAT "  %s", days.c_str());
    if (!venue.empty()) ImGui::Text(ICON_FA_LOCATION_DOT "  %s", venue.c_str());
    if (at != FormAt::Calendar) {
        ImGui::Spacing();
        if (ImGui::Button(ICON_FA_CALENDAR_DAYS "  Edit in calendar view"))
            open_in_calendar(n.name, date, !t0.empty());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("open the calendar on this day, with this selected:\n"
                              "drag it to another day, or drag its edges to change the time.\n"
                              "The exact values are under Advanced properties.");
    } else {
        ImGui::TextDisabled("drag it on the grid to move it; exact values are under Advanced");
    }
    card_end();
    ImGui::Spacing();
}

/* ── 3. STYLE WIDGETS ────────────────────────────────────────────────────── */
void HormigaApp::draw_style_widgets(const maiz::SceneNode& n, std::vector<std::string>& out) {
    if (form_adding.rfind(n.name + "/", 0) != 0) form_adding.clear(); // another rune now
    const std::string adding = form_adding.empty() ? std::string() : form_adding.substr(n.name.size() + 1);
    auto swap = [&](const char* ns, const std::string& cur, const std::string& to) {
        std::string c = "tag " + n.name;
        if (!cur.empty()) c += " -" + std::string(ns) + ":" + cur;
        if (!to.empty()) c += " +" + std::string(ns) + ":" + to;
        if (to != cur) out.push_back(c);
    };
    const hormiga::kinds::Kind* k = hormiga::kinds::current().find(n.glyph);
    const bool located = (k && k->located) || n.glyph == "contact" || n.glyph == "organization" ||
                         n.glyph == "event" || n.glyph == "incident";

    // COLOR: the `color:` tag, any colour (named, or the wheel's hex)
    const std::string col = tag_value(n, "color");
    if (!col.empty() || adding == "color") {
        if (card_begin("w-color", ICON_FA_PALETTE "  Colour", true)) {
            swap("color", col, "");
            form_adding.clear();
        }
        const std::string p = color_row(col, form_wheel_open);
        if (!p.empty()) {
            swap("color", col, p);
            form_adding.clear();
        }
        /* A SHAPE CAN GIVE ITS COLOUR (2026-10-15): `color:<it>` in the shape's
         * gives list, which colours everything inside it live (bestow.hpp,
         * given_values). Kept in step when the colour changes. */
        if (n.glyph == "mapshape" && !col.empty()) {
            auto gives = hormiga::bestow::bestowed_tags(n);
            const auto own = std::find(gives.begin(), gives.end(), "color:" + col);
            bool giving = own != gives.end();
            gives.erase(std::remove_if(gives.begin(), gives.end(),
                                       [](const std::string& t) { return t.rfind("color:", 0) == 0; }),
                        gives.end());
            if (giving && !p.empty()) // recoloured: give the new colour instead
                gives.push_back("color:" + p), out.push_back("set " + n.name + " bestows " +
                                                             text_arg(hormiga::bestow::join_bestowed(gives)));
            else if (ImGui::Checkbox("Give this colour to everything inside it", &giving)) {
                if (giving) gives.push_back("color:" + col);
                out.push_back("set " + n.name + " bestows " + text_arg(hormiga::bestow::join_bestowed(gives)));
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("what is inside takes this colour at once, and loses it when it leaves;\n"
                                  "something with a colour of its own keeps its own");
        }
        card_end();
        ImGui::Spacing();
    }
    // ICON: the `icon:` tag
    const std::string ic = tag_value(n, "icon");
    if (!ic.empty() || adding == "icon") {
        if (card_begin("w-icon", ICON_FA_ICONS "  Icon", true)) {
            swap("icon", ic, "");
            form_adding.clear();
        }
        const float bw = ImGui::GetFrameHeight() * 1.3f;
        float x = 0;
        for (const auto& mi : kMarkerIcons) {
            if (x > 0 && x + bw > ImGui::GetContentRegionAvail().x) x = 0;
            else if (x > 0) ImGui::SameLine();
            ImGui::PushID(mi.tag);
            const bool on = ic == mi.tag;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(mi.glyph, ImVec2(bw, bw))) {
                swap("icon", ic, mi.tag);
                form_adding.clear();
            }
            if (on) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", mi.label);
            ImGui::PopID();
            x += bw + ImGui::GetStyle().ItemSpacing.x;
        }
        card_end();
        ImGui::Spacing();
    }
    // MARKER SHAPE: the `shape:` tag, for anything with a place on a map
    const std::string sh = tag_value(n, "shape");
    if (n.glyph != "mapshape" && (!sh.empty() || adding == "shape")) {
        if (card_begin("w-shape", ICON_FA_SHAPES "  Marker shape", true)) {
            swap("shape", sh, "");
            form_adding.clear();
        }
        for (int i = 0; i < (int)(sizeof kMarkerShapes / sizeof *kMarkerShapes); ++i) {
            if (i) ImGui::SameLine();
            if (ImGui::RadioButton(kMarkerShapes[i], sh == kMarkerShapes[i] || (sh.empty() && i == 0))) {
                swap("shape", sh, kMarkerShapes[i]);
                form_adding.clear();
            }
        }
        card_end();
        ImGui::Spacing();
    }
    if (n.glyph == "mapshape") draw_label_widget(n, out);

    // "+": the widgets this rune does not have yet
    if (ImGui::Button(ICON_FA_PLUS "  Add a widget", ImVec2(-FLT_MIN, 0))) ImGui::OpenPopup("##addwidget");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("style this %s: a colour (any colour), an icon, a marker shape.\n"
                          "Each one is a tag, so rules and searches see it too.",
                          hormiga::kinds::current().title(n.glyph).c_str());
    if (ImGui::BeginPopup("##addwidget")) {
        bool any = false;
        if (col.empty() && adding != "color" && (any = true, ImGui::MenuItem(ICON_FA_PALETTE "  Colour")))
            form_adding = n.name + "/color", form_wheel_open = false;
        if (ic.empty() && adding != "icon" && (any = true, ImGui::MenuItem(ICON_FA_ICONS "  Icon")))
            form_adding = n.name + "/icon";
        if (located && n.glyph != "mapshape" && sh.empty() && adding != "shape" &&
            (any = true, ImGui::MenuItem(ICON_FA_SHAPES "  Marker shape")))
            form_adding = n.name + "/shape";
        if (!any) ImGui::TextDisabled("it has every widget there is");
        ImGui::EndPopup();
    }
    ImGui::Spacing();
}

/* A map shape's LABEL: its text, then how it is drawn. Every value is an
 * ordinary field (label_size, label_color, label_bold, label_italic,
 * label_pos), so the canvas, the phone, the PNG and the website read the same
 * thing; `label_css` (website only) is under Advanced. */
void HormigaApp::draw_label_widget(const maiz::SceneNode& n, std::vector<std::string>& out) {
    card_begin("w-label", ICON_FA_FONT "  Label");
    maiz::WidgetContext ctx{scene, out, std::string(), ImGui::GetContentRegionAvail().x};
    for (const auto& fl : n.fields)
        if (fl.key == "label") {
            maiz::SceneField f = fl;
            f.label = "##";
            maiz::widget_field(ctx, widgets, n, f);
        }
    auto fv = [&](const char* k) { return hormiga::temper::field_value(n, k); };
    auto set = [&](const char* k, const std::string& v) {
        if (fv(k) != v) out.push_back("set " + n.name + " " + k + " " + text_arg(v));
    };
    // size
    float stored = 1.0f;
    if (const std::string s = fv("label_size"); !s.empty()) stored = std::clamp((float)std::atof(s.c_str()), 0.5f, 4.0f);
    float v = stored;
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    if (slider_field(ICON_FA_TEXT_HEIGHT "  size", stored, 0.5f, 4.0f, "%.1fx", &v)) {
        char b[16];
        std::snprintf(b, sizeof b, "%.2g", v);
        set("label_size", b);
    }
    // bold, italic
    auto toggle = [&](const char* icon, const char* key, const char* tip) {
        const bool on = fv(key) == "1";
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(on ? ImGuiCol_ButtonActive : ImGuiCol_FrameBg));
        if (ImGui::Button(icon, ImVec2(ImGui::GetFrameHeight() * 1.3f, 0))) set(key, on ? "" : "1");
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    };
    toggle(ICON_FA_BOLD, "label_bold", "bold");
    ImGui::SameLine();
    toggle(ICON_FA_ITALIC, "label_italic", "italic");
    // where: nine positions inside the shape
    ImGui::SameLine(0, 18);
    ImGui::BeginGroup();
    static const char* pos[9] = {"tl", "tc", "tr", "cl", "c", "cr", "bl", "bc", "br"};
    static const char* tip[9] = {"top left", "top centre", "top right", "centre left", "centre",
                                 "centre right", "bottom left", "bottom centre", "bottom right"};
    std::string cur = fv("label_pos");
    if (cur.empty()) cur = fv("kind") == "ellipse" ? "tc" : "tl"; // the defaults every surface uses
    const float cell = ImGui::GetFrameHeight() * 0.8f;
    for (int i = 0; i < 9; ++i) {
        if (i % 3) ImGui::SameLine(0, 2);
        ImGui::PushID(i);
        const bool on = cur == pos[i];
        ImGui::PushStyleColor(ImGuiCol_Button, on ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)
                                                  : ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        if (ImGui::Button("##p", ImVec2(cell, cell))) set("label_pos", pos[i]);
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("label at the %s", tip[i]);
        ImGui::PopID();
    }
    ImGui::EndGroup();
    // colour: blank = the shape's own outline colour
    ImGui::TextDisabled("label colour");
    const std::string lc = fv("label_color");
    if (!lc.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("same as the shape")) set("label_color", "");
    }
    const std::string p = color_row(lc, form_label_wheel_open);
    if (!p.empty()) set("label_color", p);
    card_end();
    ImGui::Spacing();
}

/* ── THE FORM ─────────────────────────────────────────────────────────────── */
void HormigaApp::draw_rune_form(const maiz::SceneNode& n, std::vector<std::string>& out, FormAt at) {
    // a person: the photo first, click to change it
    if (n.glyph == "contact" || n.glyph == "organization") {
        const ImVec2 c = ImGui::GetCursorScreenPos();
        const float r = 30.0f;
        draw_avatar(n, ImVec2(c.x + r, c.y + r), r);
        ImGui::Dummy(ImVec2(2 * r, 2 * r));
        if (ImGui::IsItemClicked() && browse_ingest) {
            const std::string p = browse_ingest(n, "avatar", hormiga::temper::field_value(n, "avatar"));
            if (!p.empty()) out.push_back("set " + n.name + " avatar " + text_arg(p));
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("click to change the photo");
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextUnformatted(rune_title(n).c_str());
        ImGui::TextDisabled("%s", n.label.c_str());
        ImGui::EndGroup();
        ImGui::Spacing();
    }

    draw_when_card(n, at);

    // 2. the everyday fields, title first
    const std::string tf = hormiga::naming::title_field(n.glyph);
    std::vector<const maiz::SceneField*> fields;
    for (const auto& fl : n.fields) {
        if (fl.editor == "hidden" || advanced_key(fl.key)) continue;
        if (n.glyph == "mapshape" && label_key(fl.key)) continue;
        fields.push_back(&fl);
    }
    std::stable_partition(fields.begin(), fields.end(), [&](const maiz::SceneField* f) { return f->key == tf; });
    maiz::WidgetContext ctx{scene, out, std::string(), 0.0f};
    for (const maiz::SceneField* fl : fields) {
        std::string caption = fl->label.empty() ? humanize(fl->key) : fl->label;
        if (fl->key == tf)
            caption = n.glyph == "contact" || n.glyph == "organization" ? "Name" : "Title";
        if (const auto p = caption.find(" ("); p != std::string::npos && fl->key == tf) caption.resize(p);
        /* Void Maiz's own editors draw their label beside the box, so the card
         * captions them and their label is hidden; this application's editors
         * (bestow, image, icon, enum...) draw their label as their heading, so
         * they keep it and the card has no caption of its own. */
        std::string kind, args;
        maiz::split_editor_spec(fl->editor, kind, args);
        static const std::set<std::string> lib = {"",        "text", "number", "bool", "checkbox",
                                                  "multiline", "combo", "date", "knob", "path"};
        const bool library = lib.count(kind) > 0;
        card_begin(fl->key.c_str(), library ? caption.c_str() : "");
        maiz::SceneField f = *fl;
        if (library) f.label = "##";
        ctx.width = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(ctx.width);
        maiz::widget_field(ctx, widgets, n, f);
        card_end();
        ImGui::Spacing();
    }

    // 3. style widgets, and the "+"
    draw_style_widgets(n, out);

    // 4. tags, and (outside the map) where it is
    draw_tag_editor(n, out); // captioned "Tags" itself
    if (at != FormAt::Map && !hormiga::temper::field_value(n, "geo").empty()) draw_location_section(n, out);
    else if (at != FormAt::Map && (n.glyph == "contact" || n.glyph == "organization")) draw_location_section(n, out);

    // 5. advanced: the raw fields the cards present, still editable
    ImGui::Spacing();
    if (ImGui::CollapsingHeader("Advanced properties")) {
        ImGui::TextDisabled("the stored values the widgets above present");
        ImGui::TextDisabled("handle: %s", n.name.c_str());
        maiz::WidgetContext actx{scene, out, std::string(), 0.0f};
        for (const auto& fl : n.fields) {
            const bool shape_geo = n.glyph == "mapshape" && (fl.key == "geo1" || fl.key == "geo2");
            if (fl.key == "canvas" || fl.key == "ref_off") continue; // set by the map itself
            if (fl.editor == "hidden" && !shape_geo) continue;
            if (!advanced_key(fl.key) && !(n.glyph == "mapshape" && label_key(fl.key) && fl.key != "label"))
                continue;
            maiz::SceneField f = fl;
            if (shape_geo) f.editor.clear(); // plain text: "lat,lon"
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
            maiz::widget_field(actx, widgets, n, f);
        }
        // the node's size on a node canvas: meaningless for a map shape, whose
        // size IS its corners (and the handles on the map)
        if (n.glyph != "mapshape") {
            char shown[48];
            std::snprintf(shown, sizeof shown, "%g %g", n.w, n.h);
            if (form_size_for != n.name) {
                form_size_for = n.name;
                std::snprintf(form_size_buf, sizeof form_size_buf, "%s", shown);
            }
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.6f);
            if (ImGui::InputText("size on a node canvas (w h)", form_size_buf, sizeof form_size_buf,
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                float w = 0, h = 0;
                if (std::sscanf(form_size_buf, "%f %f", &w, &h) == 2 && w > 0 && h > 0)
                    out.push_back(maiz::compile_resize(n.name, w, h));
                form_size_for.clear();
            }
        }
    }
}
