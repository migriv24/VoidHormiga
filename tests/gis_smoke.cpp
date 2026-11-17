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
#include "../src/gis/polygon.hpp"
#include "../src/gis/generate.hpp"
#include "../src/gis/relate.hpp"

#include <cmath>
#include <set>
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
        // ~1 cm anywhere: `%.7g` wrote four decimals at a three-digit longitude (2026-11-17)
        ok(format_geo(12.3456789, -170.1234567) == "12.3456789,-170.1234567",
           "format_geo keeps seven decimals at any longitude");
        ok(format_geo(-0.00000001, 0) == "0,0", "and writes a nothing as 0, never -0");
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

    // ── the viewport over a FLAT world (2026-10-06: a store floor on a phone) ──
    {
        std::printf("flat viewport (a drawn plan, metres)\n");
        SlippyView f;
        f.flat = true;
        f.span = 256;
        f.min_zoom = 1;
        f.max_zoom = 9;
        f.tile_px = 256;
        f.w = 400;
        f.h = 300;
        f.lat = 12; // y, metres
        f.lon = 20; // x, metres
        f.zoom = 4; // 16 px a metre
        float sx, sy;
        f.to_screen(12, 20, sx, sy);
        near_eq(sx, 200, 1e-3, "the centre is the middle of the screen");
        f.to_screen(12, 21, sx, sy);
        near_eq(sx, 216, 1e-3, "one metre right is 16 px at zoom 4");
        f.to_screen(-5, 300, sx, sy);
        ok(sx > 400 && sy < 0, "no wrapping and no clamping: 300 m east is off the right edge");
        double la, lo;
        f.to_geo(216, 150, la, lo);
        near_eq(lo, 21, 1e-9, "screen back to metres (x)");
        near_eq(la, 12, 1e-9, "screen back to metres (y)");
        f.pan(-32, 0);
        near_eq(f.lon, 22, 1e-9, "dragging the floor left 32 px looks 2 m east");
        double la0, lo0;
        f.to_geo(216, 150, la0, lo0);
        f.zoom_about(2.0, 216, 150);
        f.to_geo(216, 150, la, lo);
        near_eq(lo, lo0, 1e-9, "a zoom keeps the metre under the finger (x)");
        near_eq(la, la0, 1e-9, "a zoom keeps the metre under the finger (y)");
        near_eq(f.metres_per_px(), 1.0 / 32, 1e-12, "metres per px is the plan's own (32 px a metre at zoom 5)");
    }


    std::printf("gis: polygons, hulls, dashes, the grid (2026-11-14)\n");
    {
        using namespace hormiga::gis;
        const std::vector<XY> L = {{0, 0}, {10, 0}, {10, 4}, {4, 4}, {4, 10}, {0, 10}};
        ok(ring_contains(L, {2, 8}) && ring_contains(L, {8, 2}), "inside both arms of an L");
        ok(!ring_contains(L, {8, 8}), "the L's notch is outside it, though inside its box");
        near_eq(ring_area(L), 64.0, 1e-12, "the L's area is its box less the notch");
        near_eq(ring_edge_dist(L, {2, 8}), 2.0, 1e-12, "the distance to the nearest edge");
        const auto hull = convex_hull({{0, 0}, {10, 0}, {10, 10}, {0, 10}, {5, 5}, {3, 7}});
        ok(hull.size() == 4, "the hull of a square and points inside it is the square");
        const auto grown = padded_hull({{0, 0}, {10, 0}, {5, 5}}, 1, 1);
        bool all_in = true;
        for (const XY& p : std::vector<XY>{{0, 0}, {10, 0}, {5, 5}}) all_in = all_in && ring_contains(grown, p) && ring_edge_dist(grown, p) > 0.9;
        ok(all_in, "a padded hull keeps its margin clear of every point");
        const auto d = dashes({{0, 0}, {30, 0}}, false, 9, 6);
        double on = 0;
        for (const auto& [a, b] : d) on += std::hypot(b.x - a.x, b.y - a.y);
        ok(d.size() == 2 && std::fabs(on - 18.0) < 1e-9, "30 units of 9-on 6-off dashes: two dashes, 18 on");
        const auto sq = dashes(rect_ring({0, 0}, {15, 15}), true, 9, 6);
        on = 0;
        for (const auto& [a, b] : sq) on += std::hypot(b.x - a.x, b.y - a.y);
        near_eq(on, 36.0, 1e-9, "a closed ring keeps the dash pace round its corners");
        const std::vector<Coord> r = {{44.05, -123.09}, {44.06, -123.08}, {44.04, -123.07}};
        const auto back = parse_ring(format_ring(r));
        ok(back.size() == 3 && std::fabs(back[1].lon + 123.08) < 1e-9, "a ring round-trips through its field");
        ok(parse_ring("1,2;oops;3,4").size() == 2, "an unreadable vertex is skipped, not fatal");
        near_eq(snap_to(2.6, 0.5), 2.5, 1e-12, "a value snaps to the nearest grid line");
        near_eq(snap_to(2.6, 0), 2.6, 1e-12, "no grid, no snap");
    }

    std::printf("gis: generators on a ground, and how drawn things relate (2026-11-14)\n");
    {
        using namespace hormiga::gis;
        const FreeGround free;
        const GridGround grid(1.0);
        const std::vector<Seed> three = {point_seed("a", {0.4, 0.2}), point_seed("b", {3.6, 2.7}), point_seed("c", {6.2, 0.9})};
        const Made fp = make_path(three, free, {});
        ok(fp.ok() && fp.pts.size() == 3 && !fp.closed, "on free space a path goes straight through its stops, in order");
        std::string why;
        const auto leg = grid.trace({0.4, 0.2}, {3.6, 2.7}, why);
        ok(leg.size() == 3 && leg[0].x == 0 && leg[0].y == 0 && leg[1].x == 4 && leg[1].y == 0 && leg[2].x == 4 && leg[2].y == 3,
           "on a grid a leg is a staircase along its lines: across, then down");
        const Made gp = make_path(three, grid, {});
        bool axis = gp.ok();
        for (size_t k = 0; axis && k + 1 < gp.pts.size(); ++k)
            axis = gp.pts[k].x == gp.pts[k + 1].x || gp.pts[k].y == gp.pts[k + 1].y;
        ok(axis, "every run of a grid path follows the grid");
        const Made lp = make_path(three, free, {true});
        ok(lp.ok() && lp.pts.back().x == lp.pts.front().x && lp.pts.back().y == lp.pts.front().y, "a loop comes back to its first stop");
        ok(!make_path({three[0]}, free, {}).ok(), "a path needs two stops");
        ok(!generate("nonsense", three, free, {}).ok(), "an unknown generator says so");

        PerimeterOptions po;
        po.margin_x = po.margin_y = 0.6;
        const std::vector<Seed> apart = {point_seed("p", {1.5, 1.5}), area_seed("q", rect_ring({6, 4}, {8, 6}))};
        const Made fper = make_perimeter(apart, free, po);
        ok(fper.ok() && fper.closed && ring_contains(fper.pts, {1.5, 1.5}) && ring_contains(fper.pts, {7, 5}),
           "a perimeter on free space holds what it goes round, though they do not touch");
        const Made gper = make_perimeter(apart, grid, po);
        bool on_lines = gper.ok();
        for (const auto& p : gper.pts) on_lines = on_lines && p.x == std::round(p.x) && p.y == std::round(p.y);
        for (size_t k = 0; on_lines && k < gper.pts.size(); ++k) {
            const XY& a = gper.pts[k];
            const XY& b = gper.pts[(k + 1) % gper.pts.size()];
            on_lines = a.x == b.x || a.y == b.y;
        }
        ok(on_lines && ring_contains(gper.pts, {1.5, 1.5}) && ring_contains(gper.pts, {7, 5}),
           "a perimeter on a grid is made of whole cells and still holds both");
        ok(grid.cover_cost(rect_ring({0, 0}, {9, 9})) == 100, "a grid says what a cover will cost before it runs");
        std::set<std::pair<std::int64_t, std::int64_t>> ringcells; // a 3x3 block less its middle
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                if (i != 1 || j != 1) ringcells.insert({i, j});
        const auto outline = cells_outline(ringcells, 1.0);
        ok(outline.size() == 4 && std::fabs(ring_area(outline) - 9.0) < 1e-12, "cells round a hole: the outline fills the hole");
        const auto ell = cells_outline({{0, 0}, {1, 0}, {0, 1}}, 2.0);
        ok(ell.size() == 6 && std::fabs(ring_area(ell) - 12.0) < 1e-12, "three cells in an L: six corners, their area");

        const Geom h{GeomKind::Line, {{0, 0}, {10, 0}}}, v{GeomKind::Line, {{5, -5}, {5, 5}}};
        const auto hv = relate(h, v, 1e-9);
        ok(hv.crossings == 1 && hv.name() == "crosses", "two roads that cross: one crossing");
        const Geom along{GeomKind::Line, {{2, 0}, {8, 0}, {8, 3}}};
        const auto ha = relate(h, along, 1e-6);
        ok(ha.shared > 5.9 && ha.shared < 6.1 && ha.name() == "overlaps", "two paths down the same street share its length");
        const Geom pt{GeomKind::Point, {{3, 2}}};
        const auto hp = relate(pt, h, 1e-9);
        ok(hp.distance > 1.99 && hp.distance < 2.01 && hp.name(3) == "near" && hp.name(1) == "disjoint", "near is the distance asked about");
        const Geom box{GeomKind::Ring, rect_ring({-1, -1}, {11, 1})};
        ok(relate(h, box, 1e-9).name() == "within", "a path inside an area is within it");
        ok(relate(box, h, 1e-9).name() == "contains", "and the area contains it");
        const XY m = local_metres({0, 1}, 0);
        ok(std::fabs(m.y - 110574) < 1, "a degree of latitude is about 110.6 km");
    }

    /* TRIANGULATED ONCE (2026-11-17): the triangles' areas add up to the ring's,
     * whichever way it runs, for a concave ring and for a keyhole (a ring cut
     * into its own hole by a narrow bridge, as an importer writes a hole). */
    {
        auto tri_area = [](const std::vector<XY>& r, const std::vector<unsigned>& t) {
            double a = 0;
            for (std::size_t i = 0; i + 2 < t.size(); i += 3) a += ring_area({r[t[i]], r[t[i + 1]], r[t[i + 2]]});
            return a;
        };
        const std::vector<XY> ell = {{0, 0}, {0, 10}, {4, 10}, {4, 4}, {10, 4}, {10, 0}};
        const auto t = triangulate(ell);
        ok(t.size() == 12 && std::fabs(tri_area(ell, t) - 64) < 1e-9, "an L: four triangles, its area");
        const std::vector<XY> lle(ell.rbegin(), ell.rend());
        ok(std::fabs(tri_area(lle, triangulate(lle)) - 64) < 1e-9, "the same L the other way round");
        // a 10x10 square with a 4x4 hole, reached by a bridge 0.01 wide
        const std::vector<XY> key = {{0, 0}, {0, 10}, {10, 10}, {10, 0}, {5.01, 0}, {5.01, 3}, {7, 3}, {7, 7},
                                     {3, 7}, {3, 3}, {5, 3}, {5, 0}};
        const auto kt = triangulate(key);
        ok(kt.size() == 3 * (key.size() - 2) && std::fabs(tri_area(key, kt) - ring_area(key)) < 1e-6,
           "a keyhole: every corner used, the hole left out");
        ok(triangulate({{0, 0}, {1, 1}}).empty(), "two points are no area");
    }

    std::printf(failures ? "\nFAILED (%d)\n" : "\nOK - the map engine holds\n",
                failures);
    return failures ? 1 : 0;
}
