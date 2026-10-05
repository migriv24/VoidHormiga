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
#include "../src/gis/marker.hpp"
#include "../src/gis/view.hpp"

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

    std::printf("gis: the marker outlines (2026-10-04)\n");
    {
        const float r = 10;
        MarkerOutline pin = marker_outline(MarkerForm::Pin, r, 2);
        ok(std::fabs(pin.pts[0].x) < 1e-4f && std::fabs(pin.pts[0].y) < 1e-4f,
           "a pin starts at its tip, and its tip is the location");
        near_eq(pin.lo.y, -(kPinHeight + 1) * r, 0.05, "a pin is 2.2 radii to the centre, one more to the top");
        near_eq(pin.hi.y, 0, 1e-4, "...and nothing of it hangs below the tip");
        near_eq(pin.hi.x, r, 0.05, "its widest is the head's radius");
        near_eq(pin.lo.x, -r, 0.05, "...on both sides");
        ok(marker_contains(pin, 0, -kPinHeight * r), "the head's centre is inside");
        ok(marker_contains(pin, 0, -0.3f * r), "just above the tip is inside");
        ok(!marker_contains(pin, 0.6f * r, -0.4f * r), "beside the tip is outside: the sides curve in");
        ok(!pin.convex, "a pin is not convex (its sides are concave)");
        near_eq(pin.face.y, -kPinHeight * r, 1e-4, "the icon goes in the head, not at the tip");
        // the top of the pin is exactly its head: nothing above the centre
        // lies outside the head's circle
        bool inside_head = true;
        for (const Pt& p : pin.pts)
            if (p.y < -kPinHeight * r && std::hypot(p.x, p.y + kPinHeight * r) > r + 0.01f) inside_head = false;
        ok(inside_head, "the top of a pin is exactly its head");

        MarkerOutline bal = marker_outline(MarkerForm::Balloon, r);
        ok(bal.pts[0].x == 0 && bal.pts[0].y == 0, "a balloon stands on its tip too");
        ok(marker_contains(bal, bal.face.x, bal.face.y), "a balloon's face is inside it");
        ok(bal.hi.y <= 1e-4f, "...and nothing of it hangs below the tip");

        MarkerOutline c = marker_outline(MarkerForm::Circle, r);
        near_eq(c.hi.x - c.lo.x, 2 * r, 0.01, "a circle is as wide as it should be");
        ok(marker_contains(c, 0, 0) && c.face.x == 0 && c.face.y == 0, "a circle is centred on its location");
        ok(!marker_on_tip(MarkerForm::Diamond) && marker_on_tip(MarkerForm::Pin), "which forms stand on a tip");
        ok(marker_form("balloon") == MarkerForm::Balloon && marker_form("") == MarkerForm::Circle,
           "names read back, and an unknown one is a circle");
    }

    std::printf("gis: a continuous zoom (the phone's map)\n");
    {
        SlippyView v;
        v.lat = 44.05;
        v.lon = -123.09;
        v.zoom = 12.4;
        v.w = 400;
        v.h = 800;
        ok(v.level() == 12, "12.4 draws level-12 tiles...");
        near_eq(v.scale(), std::pow(2.0, 0.4), 1e-9, "...scaled up by 2^0.4");
        float sx, sy;
        v.to_screen(v.lat, v.lon, sx, sy);
        near_eq(sx, 200, 1e-3, "the centre is in the middle (x)");
        near_eq(sy, 400, 1e-3, "the centre is in the middle (y)");
        double la, lo;
        v.to_geo(123, 456, la, lo);
        v.to_screen(la, lo, sx, sy);
        near_eq(sx, 123, 1e-3, "screen -> geo -> screen round-trips (x)");
        near_eq(sy, 456, 1e-3, "screen -> geo -> screen round-trips (y)");
        double pla, plo;
        v.to_geo(300, 200, pla, plo);
        v.zoom_about(1.7, 300, 200);
        v.to_screen(pla, plo, sx, sy);
        near_eq(sx, 300, 1e-2, "zoom about a point: it stays under the fingers (x)");
        near_eq(sy, 200, 1e-2, "zoom about a point: it stays under the fingers (y)");
        v.to_screen(pla, plo, sx, sy);
        v.pan(25, -40);
        float sx2, sy2;
        v.to_screen(pla, plo, sx2, sy2);
        near_eq(sx2 - sx, 25, 1e-2, "a pan carries the map with the finger (x)");
        near_eq(sy2 - sy, -40, 1e-2, "a pan carries the map with the finger (y)");
        v.zoom = 18.9;
        v.zoom_about(4.0, 0, 0);
        near_eq(v.zoom, 19, 1e-9, "zoom stops at its ceiling");
        SlippyView d;
        d.lon = 179.9;
        d.zoom = 10;
        d.w = d.h = 400;
        d.to_screen(0, -179.9, sx, sy);
        ok(sx > 200 && sx < 400, "a point just across 180 is just to the right, not a world away");
        near_eq(d.metres_per_px() * 256 * 1024, 40075016.686, 1.0, "metres per px at the equator");
    }


    std::printf(failures ? "\nFAILED (%d)\n" : "\nOK - the map engine holds\n",
                failures);
    return failures ? 1 : 0;
}
