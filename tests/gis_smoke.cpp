/* gis_smoke.cpp — the map engine, and specifically the paths nothing else runs.
 *
 * `src/gis/` was carved out on 2026-08-21 (okf/concepts/foundation/application-boundaries.md,
 * Q42). Most of it is a MOVE, and a move is checked by the golden render staying
 * byte-identical — which it did.
 *
 * But the move came with a widening: `MapSource` made six previously-hardcoded
 * assumptions into fields, and two of the new settings — `ProjectionKind::Flat`
 * and `wraps_x = false` — are reachable by **no caller in the tree today**. The
 * three built-in Earth sources never take those branches. So the golden render,
 * which is the strongest check this project has, says nothing whatsoever about
 * them.
 *
 * That is the exact shape this project keeps getting burned by: a new code path
 * that looks right, is never executed, and is discovered to be wrong by the
 * first person who needs it — who, here, would be the author building the
 * non-Earth map builder. The whole reason the widening happened now is that the
 * builder is a real intention, so it would be absurd to leave its half of the
 * abstraction unrun.
 *
 * Hence this file. It is small on purpose: it tests the arithmetic and the world
 * rules, not the view.
 */
#include "../src/gis/geo.hpp"
#include "../src/gis/projection.hpp"
#include "../src/gis/source.hpp"

#include <cmath>
#include <cstdio>
#include <string>

using namespace hormiga::gis;

static int failures = 0;

static void ok(bool cond, const char* what) {
    std::printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) ++failures;
}
static void near_eq(double a, double b, double eps, const char* what) {
    ok(std::fabs(a - b) < eps, what);
}

int main() {
    std::printf("gis: coordinates\n");
    {
        double la = 0, lo = 0;
        ok(parse_geo("44.05,-123.09", la, lo) && std::fabs(la - 44.05) < 1e-9 &&
               std::fabs(lo + 123.09) < 1e-9,
           "parse_geo reads \"lat,lon\"");
        ok(parse_geo("44.05 -123.09", la, lo) && std::fabs(lo + 123.09) < 1e-9,
           "...and \"lat lon\", which people have really typed");
        ok(!parse_geo("Riverton, OR", la, lo), "a place NAME is not a coordinate");
        ok(!parse_geo("", la, lo), "an empty geo field is not a coordinate");
        // the round trip a place/move action performs
        ok(format_geo(44.05, -123.09) == "44.05,-123.09",
           "format_geo round-trips through parse_geo's spelling");
    }

    std::printf("gis: web mercator (Earth) — unchanged behaviour\n");
    {
        // zoom 0: the whole world is one tile, so the centre is (0.5, 0.5)
        near_eq(merc_x(0.0, 0), 0.5, 1e-12, "lon 0 at z0 is tile x 0.5");
        near_eq(merc_y(0.0, 0), 0.5, 1e-12, "lat 0 at z0 is tile y 0.5");
        near_eq(merc_x(-180.0, 0), 0.0, 1e-12, "the antimeridian is tile x 0");
        // and it inverts
        for (double lon : {-179.0, -123.09, 0.0, 12.5, 179.0}) {
            near_eq(merc_lon(merc_x(lon, 12), 12), lon, 1e-9, "merc_x inverts");
        }
        for (double lat : {-84.0, -44.05, 0.0, 44.05, 84.0}) {
            near_eq(merc_lat(merc_y(lat, 12), 12), lat, 1e-9, "merc_y inverts");
        }
    }

    std::printf("gis: distance\n");
    {
        // Riverton, OR to Fairview, OR — about 7 km, and a number a person can
        // sanity-check, which is why it is this pair and not a synthetic one
        const double d = haversine_m(44.0521, -123.0868, 44.0462, -123.0220);
        ok(d > 4000 && d < 7000, "haversine gives a plausible Riverton-Fairview");
        near_eq(haversine_m(44.0, -123.0, 44.0, -123.0), 0.0, 1e-9,
                "a point is zero metres from itself");
        near_eq(euclidean(0, 0, 3, 4), 5.0, 1e-12, "euclidean is euclidean");
    }

    std::printf("gis: the Earth sources behave exactly as they always did\n");
    {
        const MapSource& osm = kBuiltinSources[0];
        ok(std::string(osm.key) == "osm", "the first built-in is still osm");
        ok(osm.labeled, "...and is the labeled style");
        ok(osm.projection == ProjectionKind::WebMercator, "Earth is web-mercator");
        ok(osm.metric == Metric::Haversine, "...measured in metres");
        ok(osm.wraps_x, "...and wraps at the antimeridian");
        ok(osm.tile_px == 256 && osm.max_zoom == 19, "256px tiles, zoom to 19");
        ok(kBuiltinSourceCount == 3, "three built-in styles");
        ok(!kBuiltinSources[1].labeled && !kBuiltinSources[2].labeled,
           "the two CARTO styles are label-free (a SOURCE, not a filter)");
        // the wrap, which is the behaviour the tile loop used to open-code
        int col = -1;
        ok(osm.tile_column(-1, 2, col) && col == 3,
           "column -1 of 4 wraps to 3, as ((x%n)+n)%n did");
        ok(osm.tile_column(4, 2, col) && col == 0, "...and column 4 wraps to 0");
        ok(osm.distance(44.0, -123.0, 44.0, -123.0) == 0.0,
           "a source's distance agrees with itself");
        ok(std::string(osm.distance_unit()) == "m", "Earth reports metres");
    }

    std::printf("gis: an AUTHORED world — the half no caller reaches yet\n");
    {
        /* A drawn city: 4096 units wide, flat, no wrapping, its own units,
         * a single image rather than a tile pyramid. This is the shape the map
         * builder will produce, and every assertion below is one the Earth
         * sources cannot make. */
        MapSource city{};
        city.key = "test-city";
        city.label = "A drawn city";
        city.projection = ProjectionKind::Flat;
        city.scheme = TileScheme::Single;
        city.metric = Metric::Euclidean;
        city.span = 4096.0;
        city.wraps_x = false;
        city.max_zoom = 5;

        // a flat world's projection is a scale, and it inverts
        double tx = 0, ty = 0, y = 0, x = 0;
        city.project_to_tile(1024.0, 2048.0, 0, tx, ty);
        near_eq(tx, 0.5, 1e-12, "x 2048 of 4096 is tile x 0.5 at z0");
        near_eq(ty, 0.25, 1e-12, "y 1024 of 4096 is tile y 0.25 at z0");
        city.tile_to_coord(tx, ty, 0, y, x);
        near_eq(x, 2048.0, 1e-9, "and it inverts in x");
        near_eq(y, 1024.0, 1e-9, "...and in y");
        // at zoom 3 the same point is 8x further out in tile space
        city.project_to_tile(1024.0, 2048.0, 3, tx, ty);
        near_eq(tx, 4.0, 1e-12, "zoom doubles tile space, as the pyramid needs");

        /* THE ONE THAT MATTERS. A non-wrapping world must REFUSE an off-edge
         * column. With the old open-coded `((txr % n) + n) % n` this returned a
         * valid column and the renderer drew the far side of the city next to
         * itself, forever. */
        int col = -1;
        ok(!city.tile_column(-1, 2, col), "an authored world has an EDGE: -1 is refused");
        ok(!city.tile_column(4, 2, col), "...and so is one past the last column");
        ok(city.tile_column(0, 2, col) && col == 0, "column 0 is fine");
        ok(city.tile_column(3, 2, col) && col == 3, "...and so is the last one");

        near_eq(city.distance(0, 0, 3, 4), 5.0, 1e-12,
                "a flat world measures flat: 3-4-5");
        ok(std::string(city.distance_unit()) == "u",
           "...and does NOT print metres it does not have");
    }

    std::printf(failures ? "\nFAILED (%d)\n" : "\nOK - the map engine holds\n",
                failures);
    return failures ? 1 : 0;
}
