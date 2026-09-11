/* ui/calendar_export.cpp — the Calendar's static PNG (C5a).
 *
 * Split out of `calendar.cpp` 2026-09-10, on the same grounds `calendar.cpp`
 * was split out of `app.cpp` on 2026-08-17 (Q30a): the file-length budget in
 * `tools/find_long.py` went red, and a budget is only worth having if going
 * over it means finding the seam rather than raising the number.
 *
 * The seam is real rather than arbitrary. Everything in `calendar.cpp` draws
 * with ImGui into a live window; this composes pixels CPU-side into a buffer
 * and writes a file. They share the date arithmetic and the styling, and they
 * share nothing else — which is exactly the map's arrangement, where the export
 * is its own concern for the same reason.
 */
#include "app/app_internal.hpp"
#include "domain/ical.hpp" // THE PIVOT: the second caller, which is the point
#include "stb_image_write.h" // decls only — the ONE implementation is in app.cpp

#include <fstream>

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


/* ── THE `.ics` EXPORT (C5b, and the proof X0 was worth doing) ───────────────
 *
 * The calendar's iCalendar twin was reachable exactly one way before this: by
 * rendering a whole website whose page happened to carry a `calendar_embed`
 * block. An organization that wanted to hand a partner a file, or subscribe to
 * its own calendar in Google, had to deploy a site first.
 *
 * The argument for extracting the lens (`domain/ical.hpp`) was that a writer
 * living inside a render loop "could only ever serve one caller". This is the
 * second caller, and it cost about thirty lines because the hard parts — the
 * folding, the escaping, the UID, the all-day DTEND, the tag allowlist — are
 * already written down once.
 *
 * IT EXPORTS WHAT THE GRID SHOWS, which is the privacy design and not a
 * shortcut. `cal_entries_on` applies the kind filter and the tag filter, so a
 * saved "Public Events" calview (C4d) bakes its choice into the file exactly as
 * it already does for the PNG — the same rule `calendar.md` states, that the
 * view's filter decides what leaves the machine. Exporting while "Incidents
 * only" is active is a deliberate act; exporting everything by accident is not
 * possible from here. */
std::string HormigaApp::export_calendar_ics() {
    if (cal_year == 0) cal_today(cal_year, cal_month, cal_day);
    /* A YEAR around the anchor, not the visible month. A file somebody hands to
     * a partner or subscribes to is worth more than the twenty-nine days that
     * happen to be on screen, and the filter — not the viewport — is what this
     * feature treats as the privacy boundary. */
    std::vector<hormiga::ical::Event> out;
    int y = cal_year, m = cal_month, d = 1;
    cal_add_days(y, m, d, -180);
    for (int i = 0; i < 545; ++i) {
        for (const auto& e : cal_entries_on(y, m, d)) {
            const maiz::SceneNode& n = *e.node;
            hormiga::ical::Event ie;
            // the FROZEN id: a UID from the editable name tells a subscriber
            // that renaming an event deleted it
            ie.uid = (n.id.empty() ? n.name : n.id) + "@voidhormiga";
            std::string t = hormiga::temper::field_value(n, "title_en");
            ie.summary = t.empty() ? n.name : t;
            ie.description = hormiga::temper::field_value(
                n, n.glyph == "incident" ? "description" : "summary_en");
            ie.location = hormiga::temper::field_value(n, "venue");
            ie.geo = hormiga::temper::field_value(n, "geo");
            ie.categories = hormiga::ical::public_categories(n.tags);
            char ds[16];
            std::snprintf(ds, sizeof ds, "%04d-%02d-%02d", y, m, d);
            ie.date = ds;
            ie.start_time = hormiga::temper::field_value(n, "start_time");
            if (ie.start_time.empty() && e.incident)
                ie.start_time = hormiga::temper::field_value(n, "time");
            ie.end_time = hormiga::temper::field_value(n, "end_time");
            ie.cancelled = std::find(n.tags.begin(), n.tags.end(),
                                     "status:cancelled") != n.tags.end();
            out.push_back(ie);
        }
        cal_add_days(y, m, d, 1);
    }
    if (out.empty()) {
        toast("nothing to export in this view (check the filter)", true);
        return {};
    }
    hormiga::ical::Options opt;
    // the saved view's name, so four subscribed calendars are four names
    opt.name = cal_view.empty() ? "Hormiga Calendar" : cal_view;
    if (const maiz::SceneNode* v = cal_view.empty() ? nullptr : scene.find(cal_view)) {
        std::string t = hormiga::temper::field_value(*v, "title");
        if (!t.empty()) opt.name = t;
    }
    const std::string body =
        hormiga::ical::to_vcalendar(out, opt, hormiga::ical::now_utc());

    std::error_code ec;
    fs::create_directories(data_dir("exports"), ec);
    char stampt[32];
    std::time_t t = std::time(nullptr);
    std::strftime(stampt, sizeof stampt, "%Y%m%d-%H%M%S", std::localtime(&t));
    fs::path out_path =
        data_dir("exports") / ("calendar-" + std::string(stampt) + ".ics");
    std::ofstream f(out_path, std::ios::binary); // binary: the RFC says CRLF
    if (!f) {
        toast("calendar .ics export failed: " + out_path.string(), true);
        return {};
    }
    f << body;
    f.close();
    toast("exported " + out_path.filename().string() + " - " +
          std::to_string(out.size()) + " entries, importable anywhere");
    if (on_open) on_open(out_path.string());
    return out_path.string();
}
