/* gis/projection.hpp — coordinate space <-> tile/pixel space.
 *
 * Moved from `render/mercator.hpp` (2026-08-21) when `src/gis/` was carved out.
 * Its original header made the argument for its own existence and it still
 * holds, so it is kept here:
 *
 *   > These were four `static` functions inside the map tab's file. Splitting
 *   > the PNG export out of that file is what revealed they were shared: the
 *   > export needs exactly the same projection as the on-screen canvas, and had
 *   > been getting it by being in the same translation unit rather than by
 *   > depending on anything. […] A shared primitive hidden by file-level
 *   > `static` is invisible until something moves, and then it either becomes a
 *   > second copy or it becomes a header.
 *
 * ── WHAT IS NEW: PROJECTION IS A CHOICE, NOT A CONSTANT ──────────────────────
 *
 * The four `merc_*` functions were called directly by the canvas and the
 * export, which hardcoded web-mercator-on-Earth into every caller. That was
 * correct while there was one world; it is the first thing in the way of the
 * map builder.
 *
 * So a projection is now a VALUE a `MapSource` carries (see source.hpp), with
 * web-mercator as one implementation and a plain flat projection as the other.
 * `Flat` is not a placeholder — it is what an authored world wants: a drawn map
 * has pixels, not a datum, and pretending otherwise means every fictional city
 * gets curvature it did not ask for.
 */
#pragma once

#include "gis/geo.hpp"

#include <cmath>

namespace hormiga::gis {

/* ── WEB MERCATOR (the OSM/CARTO slippy-tile scheme) ─────────────────────────
 *
 * Kept as free functions as well as behind `Projection`, because they are the
 * projection itself and a pure function should stay callable. The indirection
 * exists for the caller who does not know which world it is drawing; a caller
 * that genuinely means "web mercator" should say so. */

/* Fractional tile X for a longitude at zoom `z`. */
inline double merc_x(double lon, int z) {
    return (lon + 180.0) / 360.0 * (double)(1 << z);
}

/* Fractional tile Y for a latitude at zoom `z`. */
inline double merc_y(double lat, int z) {
    const double r = lat * kPi / 180.0;
    return (1.0 - std::asinh(std::tan(r)) / kPi) / 2.0 * (double)(1 << z);
}

inline double merc_lon(double x, int z) {
    return x / (double)(1 << z) * 360.0 - 180.0;
}

inline double merc_lat(double y, int z) {
    const double n = kPi * (1.0 - 2.0 * y / (double)(1 << z));
    return std::atan(std::sinh(n)) * 180.0 / kPi;
}

/* ── FLAT: an authored world ─────────────────────────────────────────────────
 *
 * One "tile unit" is `span` coordinate units at zoom 0, halving each zoom in,
 * which is the same doubling structure the slippy scheme uses — so a flat world
 * gets pyramid tiles, ancestor/child stand-ins while a tile loads, and the whole
 * pan/zoom feel for free, without inheriting a sphere.
 *
 * Y increases DOWNWARD, matching both image coordinates and mercator's tile Y,
 * so a view written against one works against the other. */
inline double flat_x(double x, int z, double span) {
    return x / span * (double)(1 << z);
}
inline double flat_y(double y, int z, double span) {
    return y / span * (double)(1 << z);
}
inline double flat_inv(double t, int z, double span) {
    return t / (double)(1 << z) * span;
}

/* Which projection a source uses. Deliberately a small closed enum rather than
 * a virtual interface: there are two, adding a third is a considered act, and a
 * `MapSource` needs to be a plain value that can be declared in a table and one
 * day authored in a file. A vtable would make it neither. */
enum class ProjectionKind {
    WebMercator, // Earth, the OSM/CARTO tile scheme
    Flat,        // an authored world: coordinates are just coordinates
};

/* Project a coordinate to fractional tile space at zoom `z`. `span` is ignored
 * for WebMercator and is the world's width in coordinate units for Flat. */
inline void project(ProjectionKind k, double lat, double lon, int z, double span,
                    double& tx, double& ty) {
    if (k == ProjectionKind::WebMercator) {
        tx = merc_x(lon, z);
        ty = merc_y(lat, z);
    } else {
        tx = flat_x(lon, z, span);
        ty = flat_y(lat, z, span);
    }
}

/* The inverse: fractional tile space back to a coordinate. */
inline void unproject(ProjectionKind k, double tx, double ty, int z, double span,
                      double& lat, double& lon) {
    if (k == ProjectionKind::WebMercator) {
        lon = merc_lon(tx, z);
        lat = merc_lat(ty, z);
    } else {
        lon = flat_inv(tx, z, span);
        lat = flat_inv(ty, z, span);
    }
}

} // namespace hormiga::gis
