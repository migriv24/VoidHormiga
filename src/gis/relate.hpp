/* gis/relate.hpp — how two drawn things stand to each other: a point, a line (a
 * path) or a ring (an area or a perimeter).
 *
 * The author (2026-11-14): *"How will paths 'interact' with each other? If they
 * overlap? If they are near each other?"* The field's answer is the OGC Simple
 * Features relations (DE-9IM: disjoint, touches, crosses, overlaps, within,
 * contains), named on how two shapes' interiors, boundaries and exteriors meet.
 * This file measures what those names are made of, in the plane, for the three
 * kinds Hormiga draws, and names the relation (okf gis/geometry.md):
 *
 *   distance     how far apart, 0 when they meet
 *   crossings    points where two lines cross properly (not along a run)
 *   shared       length two lines run together, within a tolerance
 *   a_in_b       a lies wholly inside b (b a ring)
 *
 * NOTHING HERE IS STORED. Like the tags a shape gives (domain/bestow.hpp), a
 * relation is worked out when something asks, from the shapes as they are now,
 * so it cannot go stale when one of them moves.
 *
 * Pure, dependency-free (src/gis/ may include only itself). Units are the plane's:
 * a caller on Earth hands in local metres (gis::local_metres), on a plan the
 * plan's own unit. */
#pragma once

#include "gis/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace hormiga::gis {

enum class GeomKind { Point, Line, Ring };

struct Geom {
    GeomKind kind = GeomKind::Point;
    std::vector<XY> pts; // one point; a line's vertices in order; a ring's, not closed
};

/* Earth's degrees as local metres about a reference latitude: an equirectangular
 * projection, within a fraction of a percent over the few kilometres a drawn path
 * or perimeter spans. Distances and tolerances in metres then mean metres. */
inline XY local_metres(const XY& lonlat, double lat0) {
    constexpr double kMPerDegLat = 110574.0, kMPerDegLonEq = 111320.0;
    return {lonlat.x * kMPerDegLonEq * std::cos(lat0 * kPi / 180.0), lonlat.y * kMPerDegLat};
}

/* Where two segments cross at a single point strictly inside both (a proper
 * crossing). Parallel or merely touching segments are not crossings. */
inline bool proper_cross(XY a, XY b, XY c, XY d, XY* at = nullptr) {
    const double r_x = b.x - a.x, r_y = b.y - a.y, s_x = d.x - c.x, s_y = d.y - c.y;
    const double den = r_x * s_y - r_y * s_x;
    if (std::fabs(den) < 1e-15) return false;
    const double t = ((c.x - a.x) * s_y - (c.y - a.y) * s_x) / den;
    const double u = ((c.x - a.x) * r_y - (c.y - a.y) * r_x) / den;
    constexpr double e = 1e-12;
    if (t <= e || t >= 1 - e || u <= e || u >= 1 - e) return false;
    if (at) *at = {a.x + t * r_x, a.y + t * r_y};
    return true;
}

inline double seg_seg_dist(XY a, XY b, XY c, XY d) {
    if (proper_cross(a, b, c, d)) return 0;
    return std::min({seg_dist(a, c, d), seg_dist(b, c, d), seg_dist(c, a, b), seg_dist(d, a, b)});
}

/* The segments of a geometry: a line's runs, a ring's runs closed, a point's none. */
inline std::vector<std::pair<XY, XY>> segments(const Geom& g) {
    std::vector<std::pair<XY, XY>> out;
    const std::size_t n = g.pts.size();
    if (g.kind == GeomKind::Point || n < 2) return out;
    for (std::size_t i = 0; i + 1 < n; ++i) out.push_back({g.pts[i], g.pts[i + 1]});
    if (g.kind == GeomKind::Ring && n >= 3) out.push_back({g.pts[n - 1], g.pts[0]});
    return out;
}

/* How far two runs go together: for each segment of `a`, the length of it that
 * lies within `tol` of `b` and runs along it (not merely across it). Two routes
 * that follow the same street share that street's length. */
inline double shared_length(const Geom& a, const Geom& b, double tol) {
    double total = 0;
    const auto sb = segments(b);
    for (const auto& [p, q] : segments(a)) {
        const double L = std::hypot(q.x - p.x, q.y - p.y);
        if (L <= 0) continue;
        const int steps = std::max(1, (int)std::ceil(L / std::max(tol, 1e-9)));
        int on = 0;
        for (int k = 0; k < steps; ++k) { // sample the segment's middles
            const double t = (k + 0.5) / steps;
            const XY m{p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t};
            for (const auto& [c, d] : sb) {
                if (seg_dist(m, c, d) > tol) continue;
                // along, not across: the two directions within ~25 degrees
                const double cx = d.x - c.x, cy = d.y - c.y, cl = std::hypot(cx, cy);
                if (cl <= 0) continue;
                const double cosang = std::fabs(((q.x - p.x) * cx + (q.y - p.y) * cy) / (L * cl));
                if (cosang > 0.9) { ++on; break; }
            }
        }
        total += L * on / steps;
    }
    return total;
}

struct Relation {
    double distance = 0;  // 0 when they meet
    int crossings = 0;    // proper crossing points between their outlines
    double shared = 0;    // length their outlines run together
    bool a_in_b = false;  // a wholly inside b (b is a ring)
    bool b_in_a = false;
    /* The OGC name for it, the vocabulary a rule or a tooltip uses. A shared run
     * counts as overlapping (lines that go together), a proper crossing as
     * crossing, meeting at a point only as touching, and `near` is ours: apart
     * by at most the distance the caller asked about. */
    std::string name(double near_within = 0) const {
        if (a_in_b) return "within";
        if (b_in_a) return "contains";
        if (shared > 0) return "overlaps";
        if (crossings > 0) return "crosses";
        if (distance <= 0) return "touches";
        if (distance <= near_within) return "near";
        return "disjoint";
    }
};

inline bool inside_ring(const Geom& a, const std::vector<XY>& ring) {
    if (a.pts.empty()) return false;
    for (const auto& p : a.pts)
        if (!ring_contains(ring, p)) return false;
    // a line could leave and come back between two inside vertices: its segments must not cross the ring
    Geom r{GeomKind::Ring, ring};
    for (const auto& [p, q] : segments(a))
        for (const auto& [c, d] : segments(r))
            if (proper_cross(p, q, c, d)) return false;
    return true;
}

inline Relation relate(const Geom& a, const Geom& b, double tol) {
    Relation r;
    const auto sa = segments(a), sb = segments(b);
    double best = 1e300;
    if (a.kind == GeomKind::Point && b.kind == GeomKind::Point && !a.pts.empty() && !b.pts.empty())
        best = std::hypot(a.pts[0].x - b.pts[0].x, a.pts[0].y - b.pts[0].y);
    for (const auto& p : a.pts)
        for (const auto& [c, d] : sb) best = std::min(best, seg_dist(p, c, d));
    for (const auto& p : b.pts)
        for (const auto& [c, d] : sa) best = std::min(best, seg_dist(p, c, d));
    for (const auto& [p, q] : sa)
        for (const auto& [c, d] : sb) {
            if (proper_cross(p, q, c, d)) ++r.crossings;
            best = std::min(best, seg_seg_dist(p, q, c, d));
        }
    r.distance = best <= tol ? 0 : best;
    if (a.kind != GeomKind::Point && b.kind != GeomKind::Point) r.shared = shared_length(a, b, tol);
    if (b.kind == GeomKind::Ring) r.a_in_b = inside_ring(a, b.pts) && r.crossings == 0;
    if (a.kind == GeomKind::Ring) r.b_in_a = inside_ring(b, a.pts) && r.crossings == 0;
    if (r.a_in_b || r.b_in_a) r.distance = 0;
    return r;
}

} // namespace hormiga::gis
