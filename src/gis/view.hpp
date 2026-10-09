/* gis/view.hpp — a slippy map's viewport with a CONTINUOUS zoom.
 *
 * The desktop canvas zooms by whole levels, a mouse wheel's notch at a time,
 * and draws tiles at exactly 256 px. A finger does not zoom in notches: a pinch
 * is a continuous scale, a double tap is an animation, a quick zoom follows the
 * finger. So the phone's map keeps `zoom` as a real number, draws the nearest
 * whole tile LEVEL, and scales those tiles by 2^(zoom - level) — the way every
 * phone map does it, which is why a pinch never jumps.
 *
 * Pure arithmetic in the engine folder (the strictest in the tree:
 * tools/check_layering.py), so it is tested in tests/gis_smoke.cpp without a
 * window. Web mercator, or (2026-10-06, the first drawn canvas on a phone) a
 * FLAT world: `flat` set, `span` units across at zoom 0, no wrapping at an
 * antimeridian and no polar clamp, because a store floor has neither.
 */
#pragma once

#include "gis/projection.hpp"

#include <algorithm>
#include <cmath>

namespace hormiga::gis {

struct SlippyView {
    double lat = 0, lon = 0; // the geographic point at the viewport's centre
    double zoom = 3;         // continuous: 3.0 .. 19.0
    float w = 0, h = 0;      // the viewport, px
    float tile_px = 256;     // one tile at a whole zoom, px (a phone may use 256 * density)
    double min_zoom = 3, max_zoom = 19;
    bool flat = false;       // a drawn canvas (domain/canvas.hpp): coordinates are units, not degrees
    double span = 256;       // a flat world's width at zoom 0, in its units

    static constexpr double kMaxLat = 85.0511287798;

    /* The tile level to draw, and how much its tiles are scaled. */
    int level() const { return (int)std::clamp(std::floor(zoom + 0.5), min_zoom, max_zoom); }
    double scale() const { return std::pow(2.0, zoom - level()); }

    /* World pixels at this zoom (the whole Earth is tile_px * 2^zoom wide). */
    double world() const { return tile_px * std::pow(2.0, zoom); }
    double wx(double lo) const { return (flat ? flat_x(lo, 0, span) : merc_x(lo, 0)) * world(); }
    double wy(double la) const { return (flat ? flat_y(la, 0, span) : merc_y(la, 0)) * world(); }
    double ux(double t) const { return flat ? flat_inv(t, 0, span) : merc_lon(t, 0); } // world fraction -> x
    double uy(double t) const { return flat ? flat_inv(t, 0, span) : merc_lat(t, 0); }

    /* Screen px relative to the viewport's top-left. */
    void to_screen(double la, double lo, float& sx, float& sy) const {
        double dx = wx(lo) - wx(lon);
        const double W = world();
        // the shorter way round: a marker across the antimeridian is on screen
        if (!flat && dx > W / 2) dx -= W;
        if (!flat && dx < -W / 2) dx += W;
        sx = (float)(w * 0.5 + dx);
        sy = (float)(h * 0.5 + (wy(la) - wy(lat)));
    }
    void to_geo(float sx, float sy, double& la, double& lo) const {
        const double W = world();
        lo = ux((wx(lon) + (sx - w * 0.5)) / W);
        la = uy((wy(lat) + (sy - h * 0.5)) / W);
        if (flat) return;
        lo = wrap_lon(lo);
        la = std::clamp(la, -kMaxLat, kMaxLat);
    }

    /* The content follows the finger: a drag of (dx,dy) px. */
    void pan(float dx, float dy) {
        const double W = world();
        if (flat) {
            const double nx = ux((wx(lon) - dx) / W), ny = uy((wy(lat) - dy) / W);
            lon = nx;
            lat = ny;
            return;
        }
        lon = wrap_lon(merc_lon((wx(lon) - dx) / W, 0));
        lat = std::clamp(merc_lat((wy(lat) - dy) / W, 0), -kMaxLat, kMaxLat);
    }

    /* Multiply the scale by `factor` about the screen point (sx,sy), which
     * stays over the same place. Returns false when clamped to no change. */
    bool zoom_about(double factor, float sx, float sy) {
        if (!(factor > 0)) return false;
        const double z1 = std::clamp(zoom + std::log2(factor), min_zoom, max_zoom);
        if (std::fabs(z1 - zoom) < 1e-9) return false;
        double la, lo;
        to_geo(sx, sy, la, lo);
        zoom = z1;
        // put (la,lo) back under (sx,sy)
        float nx, ny;
        to_screen(la, lo, nx, ny);
        pan(sx - nx, sy - ny);
        return true;
    }

    /* Metres per screen pixel at the centre (for an accuracy circle, a scale bar). */
    double metres_per_px() const {
        constexpr double kEarth = 40075016.686; // equator, metres
        if (flat) return span / world();         // a plan's units are metres already
        return kEarth * std::cos(lat * kPi / 180.0) / world();
    }

    static double wrap_lon(double lo) {
        while (lo < -180) lo += 360;
        while (lo >= 180) lo -= 360;
        return lo;
    }
};

} // namespace hormiga::gis
