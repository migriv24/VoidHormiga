/* gis/source.hpp — a MAP SOURCE: the world a map is of.
 *
 * ── THE ASSUMPTION THIS FILE EXISTS TO REMOVE ────────────────────────────────
 *
 * `BaseSource` (app_internal.hpp) described a tile source as five fields — key,
 * label, url, attribution, labeled — and everything else about the world was
 * hardcoded into the callers: web-mercator projection, z/x/y URLs, 256-pixel
 * tiles, zoom 0..19, longitude wrapping at the antimeridian, and haversine
 * distance on a 6,371 km sphere.
 *
 * That was right while there was one world. It is the first thing in the way of
 * the map builder, which the author confirmed on 2026-08-21 is a real
 * intention. A drawn map of a fictional city has no antimeridian to wrap at, no
 * datum, and no metres.
 *
 * So a `MapSource` now declares what it actually is. Every previously-implicit
 * assumption is a field, defaulted so the three Earth sources behave EXACTLY as
 * before — this is a widening, not a change. The whole test of that claim is
 * that the golden render stays byte-identical, and it does.
 *
 * ── WHY A PLAIN STRUCT AND NOT AN INTERFACE ──────────────────────────────────
 *
 * A source must be *declarable*: today as a table in this file, tomorrow as a
 * rune in a mantle, and eventually as something the map builder writes out.
 * Data serializes; a vtable does not. When a source needs behaviour that is not
 * expressible as a field, that is the moment to reconsider, and not before —
 * okf/concepts/sections/territory.md's "the map is a SOURCE, not an assumption" is a
 * statement about data, and this keeps it one.
 */
#pragma once

#include "gis/geo.hpp"
#include "gis/projection.hpp"

#include <string>

namespace hormiga::gis {

/* How a source's imagery is addressed. */
enum class TileScheme {
    XYZ,    // url is printf(z, x, y) — the slippy standard
    Single, // one image for the whole world; url is the path, no z/x/y
    None,   // no imagery: vector/authored content draws itself
};

/* How distance is measured in this world. */
enum class Metric {
    Haversine, // Earth, metres
    Euclidean, // the source's own units, flat
};

/* A world a map can be of.
 *
 * Aggregate-initializable, and every field after `attribution` has a default
 * that reproduces the historical Earth behaviour — so the existing table reads
 * exactly as it did and a new authored world overrides only what differs. */
struct MapSource {
    // --- identity -----------------------------------------------------------
    const char* key = "";         // tiles/<key>/ cache subdir + config value
    const char* label = "";       // what a person sees in the picker
    const char* url = "";         // XYZ: printf z,x,y. Single: a path. None: ""
    const char* attribution = ""; // shown on the map AND in every export

    /* Does this style bake labels/icons into the imagery? A "no labels" base is
     * a different SOURCE, not a filter — you cannot strip labels from a raster,
     * you switch to a style that never drew them. The UI toggle picks between
     * a labeled and a label-free source; it does not turn anything off. */
    bool labeled = true;

    // --- the world ----------------------------------------------------------
    ProjectionKind projection = ProjectionKind::WebMercator;
    TileScheme scheme = TileScheme::XYZ;
    Metric metric = Metric::Haversine;

    /* Width of the world in coordinate units, for `Flat` only. Ignored by
     * WebMercator, whose span is 360 degrees by definition. */
    double span = 360.0;

    int tile_px = 256; // edge length of one tile image
    int min_zoom = 0;
    int max_zoom = 19;

    /* Does the horizontal axis wrap? Earth does at the antimeridian, which is
     * why the tile loop computes `((tx % n) + n) % n`. An authored world has
     * EDGES, and wrapping one would tile a city into an infinite plane — which
     * is the exact bug this field exists to prevent, found by asking what
     * `Flat` would do with the existing loop. */
    bool wraps_x = true;

    /* Clamp the vertical axis to the world? Mercator already clamps near the
     * poles because the projection diverges; a flat world clamps at its edge. */
    bool clamps_y = true;

    // --- derived, so a caller never re-implements the world's rules ---------

    /* Distance between two coordinates, in this world's units. THE point of the
     * type: `near` and the proximity ring ask the source, not a global. */
    double distance(double lat1, double lon1, double lat2, double lon2) const {
        return metric == Metric::Haversine ? haversine_m(lat1, lon1, lat2, lon2)
                                           : euclidean(lat1, lon1, lat2, lon2);
    }

    /* The unit `distance` returns, for a label. A world with no metres should
     * not print "m" after a number. */
    const char* distance_unit() const {
        return metric == Metric::Haversine ? "m" : "u";
    }

    void project_to_tile(double lat, double lon, int z, double& tx, double& ty) const {
        project(projection, lat, lon, z, span, tx, ty);
    }
    void tile_to_coord(double tx, double ty, int z, double& lat, double& lon) const {
        unproject(projection, tx, ty, z, span, lat, lon);
    }

    /* Tiles across the world at zoom `z` — the `n` every tile loop needs. */
    int tiles_across(int z) const { return 1 << z; }

    /* Wrap or clamp a tile column, per this world's rules. Replaces the
     * open-coded `((txr % n) + n) % n`, which silently assumed Earth. Returns
     * false when the column is off the edge of a non-wrapping world, so the
     * caller draws nothing rather than drawing the far side. */
    bool tile_column(int txr, int z, int& out) const {
        const int n = tiles_across(z);
        if (wraps_x) {
            out = ((txr % n) + n) % n;
            return true;
        }
        if (txr < 0 || txr >= n) return false;
        out = txr;
        return true;
    }
};

/* ── THE BUILT-IN WORLDS ──────────────────────────────────────────────────────
 *
 * Three Earth tile styles, unchanged in behaviour from the `BaseSource` table
 * they replace. Attribution is per-source and is not optional: it is shown on
 * the map and in every export, because these tiles are somebody else's work.
 *
 * An authored world does not belong in this table — it belongs in a document,
 * which is what the map builder is for. This table is only "the worlds Hormiga
 * ships knowing about". */
inline const MapSource kBuiltinSources[] = {
    {"osm", "OpenStreetMap (labeled)",
     "https://tile.openstreetmap.org/%d/%d/%d.png",
     "(c) OpenStreetMap contributors", true},
    {"carto_light_nolabels", "Light - no labels or icons",
     "https://basemaps.cartocdn.com/light_nolabels/%d/%d/%d.png",
     "(c) OpenStreetMap (c) CARTO", false},
    {"carto_dark_nolabels", "Dark - no labels or icons",
     "https://basemaps.cartocdn.com/dark_nolabels/%d/%d/%d.png",
     "(c) OpenStreetMap (c) CARTO", false},
};
inline constexpr int kBuiltinSourceCount =
    (int)(sizeof kBuiltinSources / sizeof kBuiltinSources[0]);

} // namespace hormiga::gis
