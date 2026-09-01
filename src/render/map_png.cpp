/* render/map_png.cpp — a saved map view composed to a PNG, CPU-side.
 *
 * An OUTPUT, not a view: it draws no ImGui and takes no window, which is why it
 * belongs beside the newsletter and the website rather than inside the map tab.
 * The newsletter embeds its result; a headless render produces it with no
 * display attached. */

#include "app/app_internal.hpp"
#include "render/mercator.hpp"

using hormiga::merc_lat;
using hormiga::merc_lon;
using hormiga::merc_x;
using hormiga::merc_y;
#include "stb_image_write.h" // decls only - the map exports as a PNG; the ONE implementation lives in app.cpp
#include "json.hpp" // the position channel is a JSON payload

std::string HormigaApp::export_map_png(const std::string& view_name,
                                       bool announce) {
    const maiz::SceneNode* view = scene.find(view_name);
    std::string ch = view ? hormiga::temper::field_value(*view, "channel") : "main";
    if (ch.empty()) ch = "main";
    // rules for coloring (shared parser; independent of the active view)
    std::vector<MapRule> vrules;
    if (view) vrules = parse_view_rules_of(*view);
    bool show_labels = view_show_labels(view); // map config (#6)
    float label_scale = view_label_scale(view);
    bool no_overlap = view_no_overlap(view);
    unsigned lcol = view_label_color(view);
    // an export can run BEFORE the map section ever drew (boot command,
    // effect from the Console) — load the persisted camera first or we'd
    // compose at the default viewport while the cache holds the real one
    if (!map_cam_loaded) {
        map_cam_loaded = true;
        maiz::Camera saved;
        if (maiz::parse_camera(core.dispatch("config get view.map.camera").data,
                               saved))
            map_cam = saved;
    }
    const BaseSource& bsrc =
        kBaseSources[std::clamp(basemap_src, 0, kBaseSourceCount - 1)];
    const int W = 1200, H = 800;
    std::vector<unsigned char> img((size_t)W * H * 3, 235);
    int z = std::clamp((int)std::lround(map_cam.zoom), 3, 19);
    double cx = merc_x(map_cam.x, z), cy = merc_y(map_cam.y, z);
    int n = 1 << z;
    // tiles
    int tx0 = (int)std::floor(cx - W * 0.5 / 256.0) - 1;
    int tx1 = (int)std::floor(cx + W * 0.5 / 256.0) + 1;
    int ty0 = std::max(0, (int)std::floor(cy - H * 0.5 / 256.0) - 1);
    int ty1 = std::min(n - 1, (int)std::floor(cy + H * 0.5 / 256.0) + 1);
    for (int ty = ty0; ty <= ty1; ++ty)
        for (int txr = tx0; txr <= tx1; ++txr) {
            int tx = ((txr % n) + n) % n;
            char rel[96];
            std::snprintf(rel, sizeof rel, "tiles/%s/%d/%d/%d.png", bsrc.key, z,
                          tx, ty);
            fs::path full = base_dir / rel;
            if (!fs::exists(full)) continue;
            int tw, thh, tc;
            unsigned char* px = stbi_load(full.string().c_str(), &tw, &thh, &tc, 3);
            if (!px) continue;
            int ox = (int)std::lround((txr - cx) * 256.0 + W * 0.5);
            int oy = (int)std::lround((ty - cy) * 256.0 + H * 0.5);
            for (int yy = 0; yy < thh; ++yy) {
                int dy = oy + yy;
                if (dy < 0 || dy >= H) continue;
                for (int xx = 0; xx < tw; ++xx) {
                    int dx = ox + xx;
                    if (dx < 0 || dx >= W) continue;
                    std::memcpy(&img[((size_t)dy * W + dx) * 3],
                                &px[((size_t)yy * tw + xx) * 3], 3);
                }
            }
            stbi_image_free(px);
        }
    // base-map color treatment (author #3): same brightness/fade as the live
    // canvas, applied to the composed pixels BEFORE markers so the export
    // matches what you see (WYSIWYG). Brightness = multiply; Fade = mix toward
    // mid-gray. Markers drawn after stay crisp.
    if (basemap_brightness < 0.999f || basemap_fade > 0.001f) {
        float br = basemap_brightness, fd = basemap_fade;
        for (size_t i = 0; i < img.size(); ++i) {
            float v = img[i] * br;                 // brightness
            v = v * (1.0f - fd) + 150.0f * fd;     // fade toward gray
            img[i] = (unsigned char)std::clamp(v, 0.0f, 255.0f);
        }
    }
    // markers: the view's channel + rules (icons are canvas-only for now)
    auto rule_expr = [](const MapRule& r) { return rule_expr_of(r); };
    // marker SHAPE into the raw buffer (matches the canvas): the anchor is the
    // center for circle/square/diamond, the tip for a pin. Draws a white ring
    // then the colored fill; returns the icon-center y-offset from the anchor.
    auto fill_shape = [&](int cx_px, int cy_px, int r, unsigned col, MShape sh) {
        auto set = [&](int dx, int dy, unsigned c) {
            if (dx < 0 || dx >= W || dy < 0 || dy >= H) return;
            unsigned char* p = &img[((size_t)dy * W + dx) * 3];
            p[0] = c & 0xFF; p[1] = (c >> 8) & 0xFF; p[2] = (c >> 16) & 0xFF;
        };
        auto blob = [&](int ox, int oy, int rr, unsigned c) { // filled shape
            for (int yy = -rr; yy <= rr; ++yy)
                for (int xx = -rr; xx <= rr; ++xx) {
                    bool in = sh == MShape::Square
                                  ? (std::max(std::abs(xx), std::abs(yy)) <= rr)
                              : sh == MShape::Diamond
                                  ? (std::abs(xx) + std::abs(yy) <= rr)
                                  : (xx * xx + yy * yy <= rr * rr); // circle/pin bulb
                    if (in) set(ox + xx, oy + yy, c);
                }
        };
        if (sh == MShape::Pin) {
            int hy = cy_px - (int)(r * 1.55f); // bulb above; tip at (cx,cy)
            for (int yy = 0; yy <= cy_px - hy; ++yy) { // triangle down to the tip
                float t = (float)yy / (float)std::max(1, cy_px - hy);
                int half = (int)((1.0f - t) * r * 0.8f);
                for (int xx = -half; xx <= half; ++xx) set(cx_px + xx, hy + yy, col);
            }
            blob(cx_px, hy, r + 2, IM_COL32(255, 255, 255, 255));
            blob(cx_px, hy, r, col);
            return hy - cy_px; // icon centered in the bulb
        }
        blob(cx_px, cy_px, r + 2, IM_COL32(255, 255, 255, 255));
        blob(cx_px, cy_px, r, col);
        return 0;
    };
    // TEXT into the raw buffer via ImGui's baked font atlas (author #6: the
    // PNG must show readable labels). We sample each glyph's alpha coverage
    // from the Alpha8 atlas and alpha-blend it — a 1px white halo underneath
    // keeps dark labels legible over busy tiles. label_scale sizes it.
    ImFontBaked* font = ImGui::GetFontBaked(); // 1.92 font system
    float font_px = font ? font->Size : 14.0f;
    // read the live font atlas pixels directly (1.92's dynamic atlas has no
    // legacy Alpha8 copy — GetTexDataAsAlpha8 asserts; use TexRef._TexData)
    unsigned char* atlas = nullptr;
    int atlas_w = 0, atlas_h = 0, atlas_bpp = 4;
    if (ImTextureData* tex = ImGui::GetIO().Fonts->TexRef._TexData) {
        if (tex->Pixels) {
            atlas = tex->Pixels;
            atlas_w = tex->Width;
            atlas_h = tex->Height;
            atlas_bpp = tex->BytesPerPixel; // 4 (RGBA32) or 1 (Alpha8)
        }
    }
    // stamp one string's glyph coverage at an integer offset, in one color
    auto stamp_text = [&](int px, int py, int ox, int oy, const std::string& text,
                          unsigned char tr, unsigned char tg, unsigned char tb) {
        if (!font) return;
        float scale = label_scale * 1.15f; // 1.0 == the baked font size
        float penx = (float)px;
        for (unsigned char ch2 : text) {
            const ImFontGlyph* gl = font->FindGlyph((ImWchar)ch2);
            if (!gl) continue;
            if (gl->Visible) {
                float gx0 = penx + gl->X0 * scale, gy0 = py + gl->Y0 * scale;
                float gx1 = penx + gl->X1 * scale, gy1 = py + gl->Y1 * scale;
                int ix0 = (int)std::floor(gx0), iy0 = (int)std::floor(gy0);
                int ix1 = (int)std::ceil(gx1), iy1 = (int)std::ceil(gy1);
                for (int yy = iy0; yy < iy1; ++yy) {
                    int dy = yy + oy;
                    if (dy < 0 || dy >= H) continue;
                    float v = (yy + 0.5f - gy0) / (gy1 - gy0);
                    int ay = std::clamp((int)((gl->V0 + v * (gl->V1 - gl->V0)) *
                                              atlas_h), 0, atlas_h - 1);
                    for (int xx = ix0; xx < ix1; ++xx) {
                        int dx = xx + ox;
                        if (dx < 0 || dx >= W) continue;
                        float u = (xx + 0.5f - gx0) / (gx1 - gx0);
                        int ax = std::clamp((int)((gl->U0 + u * (gl->U1 - gl->U0)) *
                                                  atlas_w), 0, atlas_w - 1);
                        // alpha channel: RGBA32 => byte 3, Alpha8 => byte 0
                        size_t off = ((size_t)ay * atlas_w + ax) * atlas_bpp;
                        float a = atlas[off + (atlas_bpp == 4 ? 3 : 0)] / 255.0f;
                        if (a <= 0.02f) continue;
                        unsigned char* p = &img[((size_t)dy * W + dx) * 3];
                        p[0] = (unsigned char)(p[0] * (1 - a) + tr * a);
                        p[1] = (unsigned char)(p[1] * (1 - a) + tg * a);
                        p[2] = (unsigned char)(p[2] * (1 - a) + tb * a);
                    }
                }
            }
            penx += gl->AdvanceX * scale;
        }
    };
    auto text_w = [&](const std::string& text) { // width at the label scale
        if (!font) return 0.0f;
        float scale = label_scale * 1.15f, w = 0;
        for (unsigned char c : text)
            if (const ImFontGlyph* gl = font->FindGlyph((ImWchar)c))
                w += gl->AdvanceX * scale;
        return w;
    };
    auto blit_text = [&](int px, int py, const std::string& text,
                         bool force = false) {
        if (!atlas || (!show_labels && !force)) return; // watermark forces
        // white outline (4 offsets), then the label color — legible on tiles
        for (int d = 0; d < 4; ++d) {
            static const int ox[4] = {-1, 1, 0, 0}, oy[4] = {0, 0, -1, 1};
            stamp_text(px, py, ox[d], oy[d], text, 255, 255, 255);
        }
        stamp_text(px, py, 0, 0, text, lcol & 0xFF, (lcol >> 8) & 0xFF,
                   (lcol >> 16) & 0xFF);
    };
    std::vector<ImVec4> placed_labels; // no-overlap bookkeeping (x0,y0,x1,y1)
    // #3: MAP SHAPES into the export (under the markers) — rect/ellipse, filled
    // translucent + outlined, colored by the same rules/tags as on the canvas.
    auto sxy = [&](double la, double lo, int& sx, int& sy) {
        sx = (int)std::lround((merc_x(lo, z) - cx) * 256.0 + W * 0.5);
        sy = (int)std::lround((merc_y(la, z) - cy) * 256.0 + H * 0.5);
    };
    auto blend_px = [&](int x, int y, unsigned c, float a) {
        if (x < 0 || x >= W || y < 0 || y >= H) return;
        unsigned char* p = &img[((size_t)y * W + x) * 3];
        p[0] = (unsigned char)(p[0] * (1 - a) + (c & 0xFF) * a);
        p[1] = (unsigned char)(p[1] * (1 - a) + ((c >> 8) & 0xFF) * a);
        p[2] = (unsigned char)(p[2] * (1 - a) + ((c >> 16) & 0xFF) * a);
    };
    for (const auto& node : scene.nodes) {
        if (node.glyph != "mapshape") continue;
        double la1, lo1, la2, lo2;
        if (!hormiga::parse_geo(hormiga::temper::field_value(node, "geo1"), la1, lo1) ||
            !hormiga::parse_geo(hormiga::temper::field_value(node, "geo2"), la2, lo2))
            continue;
        int x1, y1, x2, y2;
        sxy(la1, lo1, x1, y1);
        sxy(la2, lo2, x2, y2);
        int mnx = std::min(x1, x2), mxx = std::max(x1, x2);
        int mny = std::min(y1, y2), mxy = std::max(y1, y2);
        unsigned scol = IM_COL32(46, 107, 79, 255);
        for (const auto& r : vrules) {
            if (r.tags.empty() || !maiz::node_matches(rule_expr(r), node)) continue;
            for (const auto& c : kMarkerColors)
                if (r.color == c.tag) scol = c.col;
            break;
        }
        std::string ct = tag_value(node, "color");
        for (const auto& c : kMarkerColors)
            if (ct == c.tag) scol = c.col;
        bool ell = hormiga::temper::field_value(node, "kind") == "ellipse";
        float rx = (mxx - mnx) * 0.5f, ry = (mxy - mny) * 0.5f;
        float ccx = (mnx + mxx) * 0.5f, ccy = (mny + mxy) * 0.5f;
        for (int y = std::max(0, mny); y <= std::min(H - 1, mxy); ++y)
            for (int x = std::max(0, mnx); x <= std::min(W - 1, mxx); ++x) {
                bool inside = true, edge = false;
                if (ell) {
                    float nx = (x - ccx) / std::max(1.0f, rx);
                    float ny = (y - ccy) / std::max(1.0f, ry);
                    float d = nx * nx + ny * ny;
                    inside = d <= 1.0f;
                    edge = inside && d > 0.86f;
                } else {
                    edge = (x <= mnx + 1 || x >= mxx - 1 || y <= mny + 1 ||
                            y >= mxy - 1);
                }
                if (edge) blend_px(x, y, scol, 0.9f);
                else if (inside) blend_px(x, y, scol, 0.2f);
            }
    }
    auto ex_fans = ref_fans(scene, ch); // #4: fan ref-children in the export too
    int placed = 0;
    for (const auto& node : scene.nodes) {
        if (node.glyph == "refpoint") continue; // gizmos never export
        double la, lo;
        int sx, sy;
        auto ff = ex_fans.find(node.name);
        if (ff != ex_fans.end()) { // fanned child: refpoint geo + pixel offset
            la = ff->second.lat; lo = ff->second.lon;
            sx = (int)std::lround((merc_x(lo, z) - cx) * 256.0 + W * 0.5 +
                                  ff->second.dx);
            sy = (int)std::lround((merc_y(la, z) - cy) * 256.0 + H * 0.5 +
                                  ff->second.dy);
        } else {
            std::string g = view_geo(node, ch);
            if (g.empty() || !hormiga::parse_geo(g, la, lo)) continue;
            sx = (int)std::lround((merc_x(lo, z) - cx) * 256.0 + W * 0.5);
            sy = (int)std::lround((merc_y(la, z) - cy) * 256.0 + H * 0.5);
        }
        if (sx < -12 || sx > W + 12 || sy < -12 || sy > H + 12) continue;
        unsigned col = node.glyph == "incident"       ? IM_COL32(200, 50, 50, 255)
                       : node.glyph == "organization" ? IM_COL32(138, 109, 59, 255)
                       : node.glyph == "event"        ? IM_COL32(63, 111, 174, 255)
                                                      : IM_COL32(179, 89, 46, 255);
        std::string shp;
        for (const auto& r : vrules) {
            if (r.tags.empty() || !maiz::node_matches(rule_expr(r), node)) continue;
            for (const auto& c : kMarkerColors)
                if (r.color == c.tag) col = c.col;
            if (!r.shape.empty()) shp = r.shape;
            break;
        }
        std::string ctag = tag_value(node, "color");
        for (const auto& c : kMarkerColors)
            if (ctag == c.tag) col = c.col;
        std::string stag = tag_value(node, "shape");
        if (!stag.empty()) shp = stag;
        fill_shape(sx, sy, 8, col, shape_from(shp));
        // labels: same no-overlap candidates as the canvas (right/left/up/down)
        if (show_labels) {
            float tw2 = text_w(node.name), th2 = font_px * label_scale * 1.2f;
            float cand[4][2] = {
                {(float)sx + 14, sy - th2 * 0.5f},
                {(float)sx - 14 - tw2, sy - th2 * 0.5f},
                {(float)sx - tw2 * 0.5f, (float)sy - 14 - th2},
                {(float)sx - tw2 * 0.5f, (float)sy + 14}};
            int pick = no_overlap ? -1 : 0;
            for (int c = 0; c < 4 && pick < 0; ++c) {
                bool clear = true;
                for (const auto& pl : placed_labels)
                    if (cand[c][0] < pl.z && cand[c][0] + tw2 > pl.x &&
                        cand[c][1] < pl.w && cand[c][1] + th2 > pl.y) {
                        clear = false;
                        break;
                    }
                if (clear) pick = c;
            }
            if (pick >= 0) {
                placed_labels.push_back({cand[pick][0], cand[pick][1],
                                         cand[pick][0] + tw2,
                                         cand[pick][1] + th2});
                blit_text((int)cand[pick][0], (int)cand[pick][1], node.name);
            }
        }
        ++placed;
    }
    // ── WATERMARK (author 2026-07-24): the org's logo + name, bottom-right, on
    // every map export — automatic branding for newsletters/sites. ──────────
    {
        // read branding from config (robust: reflects the saved database, not
        // a possibly-stale member)
        auto cfg = [&](const char* k) {
            std::string v = core.dispatch(std::string("config get ") + k).data;
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
                v = v.substr(1, v.size() - 2);
            return v == "null" ? std::string() : v;
        };
        int wx = W - 12, wy = H - 12; // bottom-right anchor (grows leftward/up)
        std::string label = cfg("org.name");
        std::string logo_path = cfg("org.logo");
        int logo_w = 0, logo_h = 0;
        std::vector<unsigned char> logo_px;
        if (!logo_path.empty()) {
            fs::path lp = fs::path(logo_path).is_absolute() ? fs::path(logo_path)
                                                            : base_dir / logo_path;
            int lw, lh, lc;
            if (unsigned char* px =
                    stbi_load(lp.string().c_str(), &lw, &lh, &lc, 4)) {
                logo_w = lw; // source dimensions (sampled nearest below)
                logo_h = lh;
                logo_px.assign(px, px + (size_t)lw * lh * 4);
                stbi_image_free(px);
            }
        }
        if (!label.empty() || !logo_px.empty()) {
            float lw_draw = logo_px.empty() ? 0 : 40.0f * logo_w / std::max(1, logo_h);
            float tw = label.empty() ? 0 : text_w(label);
            int panel_w = (int)(lw_draw + (lw_draw > 0 && tw > 0 ? 8 : 0) + tw) + 20;
            int panel_h = 52;
            int px0 = wx - panel_w, py0 = wy - panel_h;
            // a soft translucent plate behind the watermark
            for (int y = std::max(0, py0); y < wy && y < H; ++y)
                for (int x = std::max(0, px0); x < wx && x < W; ++x) {
                    unsigned char* p = &img[((size_t)y * W + x) * 3];
                    p[0] = (unsigned char)(p[0] * 0.35f + 255 * 0.65f);
                    p[1] = (unsigned char)(p[1] * 0.35f + 255 * 0.65f);
                    p[2] = (unsigned char)(p[2] * 0.35f + 255 * 0.65f);
                }
            // the logo (nearest-neighbour into a 40px-tall box)
            if (!logo_px.empty()) {
                int dh = 40, dw = (int)lw_draw;
                for (int y = 0; y < dh; ++y)
                    for (int x = 0; x < dw; ++x) {
                        int sxp = x * logo_w / std::max(1, dw);
                        int syp = y * logo_h / std::max(1, dh);
                        const unsigned char* sp =
                            &logo_px[((size_t)syp * logo_w + sxp) * 4];
                        float a = sp[3] / 255.0f;
                        if (a < 0.05f) continue;
                        int dx = px0 + 10 + x, dy = py0 + 6 + y;
                        if (dx < 0 || dx >= W || dy < 0 || dy >= H) continue;
                        unsigned char* p = &img[((size_t)dy * W + dx) * 3];
                        p[0] = (unsigned char)(p[0] * (1 - a) + sp[0] * a);
                        p[1] = (unsigned char)(p[1] * (1 - a) + sp[1] * a);
                        p[2] = (unsigned char)(p[2] * (1 - a) + sp[2] * a);
                    }
            }
            if (!label.empty()) {
                int tx = px0 + 10 + (int)lw_draw + (lw_draw > 0 ? 8 : 0);
                blit_text(tx, py0 + 16, label, true); // force: ignore show_labels
            }
        }
    }
    std::error_code ec;
    fs::create_directories(data_dir("exports"), ec);
    char stamp[32];
    std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&t));
    std::string safe = view_name; // view names are filenames here — sanitize
    for (char& c : safe)
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') c = '_';
    fs::path out = data_dir("exports") / (safe + "-" + stamp + ".png");
    if (stbi_write_png(out.string().c_str(), W, H, 3, img.data(), W * 3)) {
        if (announce) {
            toast("exported " + out.filename().string() + " (" +
                  std::to_string(placed) + " markers) - credit \"" +
                  std::string(bsrc.attribution) + "\" when publishing");
            if (on_open) on_open(out.string());
        }
        return out.string();
    }
    if (announce) toast("export failed: " + out.string(), true);
    return {};
}


// ── the Territory section: a real slippy map (v1: OSM Earth source) ─────────
