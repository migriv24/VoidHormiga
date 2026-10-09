/* phone/phone_map_ground.cpp — what the phone's map is drawn ON: Earth's tiles
 * at a continuous zoom, or a drawn canvas's floor (domain/canvas.hpp).
 *
 * Split out of phone_map.cpp on 2026-10-06, when canvases pushed that file
 * past its length budget; it is the one part of the canvas that depends on
 * which world it is, so it is also the natural seam. */
#include "phone/phone_map.hpp"

using namespace hormiga::phone;
using namespace hormiga::phone::mapx;

void HormigaApp::PhoneUi::map_ground(HormigaApp& app, PhoneUi& ph, ImDrawList* dl, ImVec2 p0, ImVec2 p1) {
    MapUi& m = *ph.map_ui;
    const hormiga::canvas::Canvas cvs = hormiga::canvas::find(app.scene, app.map_canvas);
    const ImVec2 sz(p1.x - p0.x, p1.y - p0.y);
    const bool plan = cvs.plan();
    auto scr = [&](double la, double lo) {
        float x, y;
        m.v.to_screen(la, lo, x, y);
        return ImVec2(p0.x + x, p0.y + y);
    };
    // ── tiles, at a continuous zoom: the nearest level, scaled ──────────────
    if (plan) {
        const ImVec2 o = scr(0, 0);
        draw_plan_grid(dl, p0, p1, cvs, o, scr(1, 1).x - o.x);
    } else {
        const BaseSource& bsrc = kBaseSources[std::clamp(app.basemap_src, 0, kBaseSourceCount - 1)];
        const int z = m.v.level();
        const int n = 1 << z;
        const double ts = m.v.tile_px * m.v.scale();
        const double cxw = m.v.wx(m.v.lon), cyw = m.v.wy(m.v.lat);
        auto tile_tex = [&](int zz, int tx, int ty) -> HostTexture {
            char rel[96];
            std::snprintf(rel, sizeof rel, "tiles/%s/%d/%d/%d.png", bsrc.key, zz, tx, ty);
            if (!std::filesystem::exists(app.base_dir / rel)) return {};
            return app.texture_for(rel);
        };
        const int tx0 = (int)std::floor((cxw - sz.x * 0.5) / ts), tx1 = (int)std::floor((cxw + sz.x * 0.5) / ts);
        const int ty0 = std::max(0, (int)std::floor((cyw - sz.y * 0.5) / ts));
        const int ty1 = std::min(n - 1, (int)std::floor((cyw + sz.y * 0.5) / ts));
        dl->AddRectFilled(p0, p1, IM_COL32(232, 230, 224, 255)); // the ground, under tiles still coming
        for (int ty = ty0; ty <= ty1; ++ty)
            for (int txr = tx0; txr <= tx1; ++txr) {
                int tx = 0;
                if (!bsrc.tile_column(txr, z, tx)) continue;
                // edges rounded from the same doubles: neighbouring tiles share a pixel edge, no seams
                const ImVec2 a((float)std::round(p0.x + sz.x * 0.5 + txr * ts - cxw),
                               (float)std::round(p0.y + sz.y * 0.5 + ty * ts - cyw));
                const ImVec2 b((float)std::round(p0.x + sz.x * 0.5 + (txr + 1) * ts - cxw),
                               (float)std::round(p0.y + sz.y * 0.5 + (ty + 1) * ts - cyw));
                if (HostTexture t = tile_tex(z, tx, ty); t.id) {
                    dl->AddImage((ImTextureID)(intptr_t)t.id, a, b);
                    continue;
                }
                {
                    char url[160], rel[96];
                    std::snprintf(url, sizeof url, bsrc.url, z, tx, ty);
                    std::snprintf(rel, sizeof rel, "tiles/%s/%d/%d/%d.png", bsrc.key, z, tx, ty);
                    app.tiles.want(url, (app.base_dir / rel).string());
                }
                bool drew = false;
                for (int k = 1; k <= 4 && !drew; ++k) { // a cached ancestor's quarter meanwhile
                    HostTexture pa = tile_tex(z - k, tx >> k, ty >> k);
                    if (!pa.id) continue;
                    const float fr = 1.0f / (float)(1 << k);
                    const ImVec2 uv0((tx & ((1 << k) - 1)) * fr, (ty & ((1 << k) - 1)) * fr);
                    dl->AddImage((ImTextureID)(intptr_t)pa.id, a, b, uv0, ImVec2(uv0.x + fr, uv0.y + fr));
                    drew = true;
                }
                if (!drew && z < 19)
                    for (int q = 0; q < 4; ++q) { // or its four children
                        HostTexture c = tile_tex(z + 1, tx * 2 + (q & 1), ty * 2 + (q >> 1));
                        if (!c.id) continue;
                        const float hw = (b.x - a.x) * 0.5f, hh = (b.y - a.y) * 0.5f;
                        const ImVec2 qa(a.x + (q & 1) * hw, a.y + (q >> 1) * hh);
                        dl->AddImage((ImTextureID)(intptr_t)c.id, qa, ImVec2(qa.x + hw, qa.y + hh));
                    }
            }
        if (app.basemap_brightness < 0.999f)
            dl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, (int)((1.0f - app.basemap_brightness) * 255)));
        if (app.basemap_fade > 0.001f) dl->AddRectFilled(p0, p1, IM_COL32(150, 150, 150, (int)(app.basemap_fade * 200)));
    }
}
