/* gis/polygon.hpp — the geometry of a drawn region: rings, inside, near, hulls,
 * dashes and a grid.
 *
 * The author (2026-11-14): *"polygon area creation. not just boxes or elipsoids,
 * but multiple verticies to make a polygon shape"*, and a PERIMETER: *"computationally
 * they are the same ... i just want an outline ... i also want the border to look a
 * bit like a dashed line"*. Answers Q102 (okf/concepts/sections/gis/territory.md,
 * "Polygons and perimeters").
 *
 * ONE RING FOR EVERY KIND. A rectangle, an ellipse and a polygon are all a closed
 * ring of points here, so containment, the hull a perimeter is generated from and
 * the dashed outline are written once. The renderers keep drawing a rectangle and
 * an ellipse natively (a crisp ellipse beats a 64-gon); a polygon, and every
 * dashed outline, come from the ring.
 *
 * Pure: points in, answers out, in whatever plane the caller works in (lat/lon,
 * metres on a plan, or screen pixels). Like everything in src/gis/ it includes
 * nothing but itself and the standard library, so it can leave with Void GIS. */
#pragma once

#include "gis/geo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace hormiga::gis {

struct XY {
    double x = 0.0, y = 0.0;
};

inline XY xy_of(const Coord& c) { return {c.lon, c.lat}; }
inline Coord coord_of(const XY& p) { return {p.y, p.x}; }

/* ── the stored form ─────────────────────────────────────────────────────────
 * A polygon's vertices are one field, `pts`: "lat,lon;lat,lon;...", each point
 * spelled as `format_geo` spells a `geo`, so a polygon written by a click and one
 * written by an agent diff alike. Fewer than three readable points is no ring. */
inline std::vector<Coord> parse_ring(const std::string& s) {
    std::vector<Coord> out;
    std::size_t i = 0;
    while (i <= s.size()) {
        std::size_t j = s.find(';', i);
        if (j == std::string::npos) j = s.size();
        Coord c;
        if (j > i && parse_geo(s.substr(i, j - i), c)) out.push_back(c);
        i = j + 1;
    }
    return out;
}

inline std::string format_ring(const std::vector<Coord>& ring) {
    std::string out;
    for (const auto& c : ring) out += (out.empty() ? "" : ";") + format_geo(c);
    return out;
}

/* The box around a ring: what a polygon also writes as `geo1`/`geo2`, so every
 * reader that knows only boxes (an older Hormiga, an overlap pre-check) still has
 * a right answer about where it is. */
inline bool ring_bounds(const std::vector<Coord>& ring, Coord& lo, Coord& hi) {
    if (ring.empty()) return false;
    lo = hi = ring.front();
    for (const auto& c : ring) {
        lo.lat = std::min(lo.lat, c.lat), lo.lon = std::min(lo.lon, c.lon);
        hi.lat = std::max(hi.lat, c.lat), hi.lon = std::max(hi.lon, c.lon);
    }
    return true;
}

/* ── rings for the box kinds ────────────────────────────────────────────────── */
inline std::vector<XY> rect_ring(XY a, XY b) {
    const double x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x);
    const double y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

inline std::vector<XY> ellipse_ring(XY a, XY b, int n = 64) {
    const double cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2;
    const double rx = std::fabs(b.x - a.x) / 2, ry = std::fabs(b.y - a.y) / 2;
    std::vector<XY> out;
    out.reserve((std::size_t)n);
    for (int i = 0; i < n; ++i) {
        const double t = 2 * kPi * i / n;
        out.push_back({cx + rx * std::cos(t), cy + ry * std::sin(t)});
    }
    return out;
}

/* ── inside, near ────────────────────────────────────────────────────────────
 * Even-odd ray casting: a point is inside when a ray from it crosses the ring an
 * odd number of times. A self-crossing polygon (a figure eight someone clicked)
 * then has the parts a person would call inside, and the lobe where it crosses
 * itself twice is out, which is what every drawing program shows. */
inline bool ring_contains(const std::vector<XY>& ring, XY p) {
    bool in = false;
    const std::size_t n = ring.size();
    if (n < 3) return false;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const XY& a = ring[i];
        const XY& b = ring[j];
        if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) in = !in;
    }
    return in;
}

inline double seg_dist(XY p, XY a, XY b) {
    const double dx = b.x - a.x, dy = b.y - a.y, L = dx * dx + dy * dy;
    double t = L > 0 ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / L : 0.0;
    t = std::clamp(t, 0.0, 1.0);
    const double ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
    return std::sqrt(ex * ex + ey * ey);
}

/* The distance from a point to the ring's outline: how a PERIMETER is hit. A
 * perimeter is only its border, so a click inside it reaches what is inside it
 * (the author: "i want to be able to easily edit things inside it"). */
inline double ring_edge_dist(const std::vector<XY>& ring, XY p) {
    double best = 1e300;
    const std::size_t n = ring.size();
    for (std::size_t i = 0; i < n; ++i) best = std::min(best, seg_dist(p, ring[i], ring[(i + 1) % n]));
    return best;
}

/* Twice the signed area (shoelace); its absolute value halved is the area. */
inline double ring_area(const std::vector<XY>& ring) {
    double s = 0;
    const std::size_t n = ring.size();
    for (std::size_t i = 0; i < n; ++i) {
        const XY& a = ring[i];
        const XY& b = ring[(i + 1) % n];
        s += a.x * b.y - b.x * a.y;
    }
    return std::fabs(s) / 2;
}

/* ── the hull a generated perimeter starts from ─────────────────────────────
 * Andrew's monotone chain, O(n log n). Counter-clockwise, no repeated last point.
 * GENERATE PERIMETER (the author's item 6) encloses several regions that need not
 * touch. Without the ground under the map (gis/ground, okf "ground.md") the honest
 * enclosure is the convex hull of every region's ring, grown by a margin so the
 * outline does not run along the regions' own borders. With the ground, the same
 * call becomes the blocks the hull covers; this is the first rung, not the last. */
inline std::vector<XY> convex_hull(std::vector<XY> pts) {
    std::sort(pts.begin(), pts.end(), [](const XY& a, const XY& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    pts.erase(std::unique(pts.begin(), pts.end(), [](const XY& a, const XY& b) { return a.x == b.x && a.y == b.y; }),
              pts.end());
    if (pts.size() < 3) return pts;
    auto cross = [](const XY& o, const XY& a, const XY& b) {
        return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
    };
    std::vector<XY> h(pts.size() * 2);
    std::size_t k = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
        while (k >= 2 && cross(h[k - 2], h[k - 1], pts[i]) <= 0) --k;
        h[k++] = pts[i];
    }
    for (std::size_t i = pts.size() - 1, t = k + 1; i-- > 0;) {
        while (k >= t && cross(h[k - 2], h[k - 1], pts[i]) <= 0) --k;
        h[k++] = pts[i];
    }
    h.resize(k - 1);
    return h;
}

/* The hull of the points, each grown into a small circle of `margin` first: the
 * convex hull of the Minkowski sum, so the outline keeps `margin` clear of every
 * point all the way round. `margin` is in the plane's units (x and y apart, since
 * a degree of longitude is not a degree of latitude away from the equator). */
inline std::vector<XY> padded_hull(const std::vector<XY>& pts, double margin_x, double margin_y, int steps = 12) {
    std::vector<XY> grown;
    grown.reserve(pts.size() * (std::size_t)steps);
    for (const auto& p : pts)
        for (int i = 0; i < steps; ++i) {
            const double t = 2 * kPi * i / steps;
            grown.push_back({p.x + margin_x * std::cos(t), p.y + margin_y * std::sin(t)});
        }
    return convex_hull(std::move(grown));
}

/* ── the dashed outline ──────────────────────────────────────────────────────
 * A perimeter's border, as dash segments along a ring (closed) or a line (open),
 * in the plane the caller draws in: `dash` on, `gap` off, measured along the
 * line, so the pattern keeps its pace round corners rather than restarting at
 * each one. Every surface (the canvas, the phone, the PNG) draws these segments;
 * the website's canvas uses its own setLineDash with the same two numbers. */
inline std::vector<std::pair<XY, XY>> dashes(const std::vector<XY>& line, bool closed, double dash, double gap) {
    std::vector<std::pair<XY, XY>> out;
    const std::size_t n = line.size();
    if (n < 2 || dash <= 0) return out;
    const double period = dash + gap;
    double phase = 0; // distance into the current period
    const std::size_t segs = closed ? n : n - 1;
    for (std::size_t i = 0; i < segs; ++i) {
        const XY a = line[i], b = line[(i + 1) % n];
        const double L = std::hypot(b.x - a.x, b.y - a.y);
        double s = 0;
        while (s < L) {
            const bool on = phase < dash;
            const double left = (on ? dash : period) - phase;
            const double step = std::min(left, L - s);
            if (on) {
                const double t0 = s / L, t1 = (s + step) / L;
                out.push_back({{a.x + (b.x - a.x) * t0, a.y + (b.y - a.y) * t0},
                               {a.x + (b.x - a.x) * t1, a.y + (b.y - a.y) * t1}});
            }
            s += step;
            phase += step;
            if (phase >= period - 1e-12) phase = 0;
        }
    }
    return out;
}

/* ── the grid, the plan canvas's snap ────────────────────────────────────────
 * SNAPPING IS A PROPERTY OF THE GROUND (okf ground.md). A drawn plan's ground is
 * its grid (`Canvas::grid`, in its unit); Earth's is its streets, which need the
 * street data a person imports. This is the plan's half. */
inline double snap_to(double v, double step) { return step > 0 ? std::round(v / step) * step : v; }

/* ── a ring as triangles, once ───────────────────────────────────────────────
 * TRIANGULATED ONCE PER CHANGE, NOT ONCE PER FRAME (2026-11-17). ImGui fills a
 * concave polygon by ear clipping it again every frame, O(n^2) in its vertices:
 * an imported set of boundaries (thousands of vertices) took 13-20 ms a frame to fill. The
 * triangles depend only on the ring, so they are cut once, here, and the screen
 * maps their corners each frame. Ear clipping over a linked ring: a convex
 * corner whose triangle holds no other vertex is cut off; a vertex at the same
 * place as a corner does not block it (a keyhole's two sides meet there). When
 * a whole lap finds no ear (a ring that touches itself), a collinear corner is
 * dropped, else the current one is cut anyway, so it always ends. Either
 * winding. Indices into `ring`, three per triangle. */
inline std::vector<unsigned> triangulate(const std::vector<XY>& ring) {
    const std::size_t n = ring.size();
    std::vector<unsigned> out;
    if (n < 3) return out;
    double s = 0;
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) s += ring[j].x * ring[i].y - ring[i].x * ring[j].y;
    const double dir = s >= 0 ? 1.0 : -1.0; // counter-clockwise (x right, y up) is positive
    std::vector<unsigned> prev(n), next(n);
    for (std::size_t i = 0; i < n; ++i) prev[i] = (unsigned)((i + n - 1) % n), next[i] = (unsigned)((i + 1) % n);
    auto cross = [&](unsigned a, unsigned b, unsigned c) {
        const XY &A = ring[a], &B = ring[b], &C = ring[c];
        return dir * ((B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x));
    };
    auto same = [&](unsigned a, unsigned b) { return ring[a].x == ring[b].x && ring[a].y == ring[b].y; };
    auto is_ear = [&](unsigned a, unsigned b, unsigned c) {
        if (cross(a, b, c) <= 0) return false; // reflex, or flat
        const XY &A = ring[a], &B = ring[b], &C = ring[c];
        const double lx = std::min({A.x, B.x, C.x}), hx = std::max({A.x, B.x, C.x});
        const double ly = std::min({A.y, B.y, C.y}), hy = std::max({A.y, B.y, C.y});
        for (unsigned p = next[c]; p != a; p = next[p]) {
            const XY& P = ring[p];
            if (P.x < lx || P.x > hx || P.y < ly || P.y > hy) continue;
            if (same(p, a) || same(p, b) || same(p, c)) continue;
            if (cross(a, b, p) >= 0 && cross(b, c, p) >= 0 && cross(c, a, p) >= 0) return false;
        }
        return true;
    };
    out.reserve(3 * (n - 2));
    std::size_t left = n;
    unsigned cur = 0;
    std::size_t since = 0; // corners looked at since the last cut
    auto cut = [&](unsigned v, bool emit) {
        if (emit) out.insert(out.end(), {prev[v], v, next[v]});
        next[prev[v]] = next[v];
        prev[next[v]] = prev[v];
        --left;
        since = 0;
        return next[v];
    };
    while (left > 3) {
        if (is_ear(prev[cur], cur, next[cur])) {
            cur = cut(cur, true);
            continue;
        }
        cur = next[cur];
        if (++since < left) continue;
        // a whole lap and no ear: drop a flat corner, or cut this one anyway
        unsigned flat = cur, v = cur;
        bool found = false;
        do {
            if (std::fabs(cross(prev[v], v, next[v])) == 0.0) { flat = v, found = true; break; }
            v = next[v];
        } while (v != cur);
        cur = found ? cut(flat, false) : cut(cur, true);
    }
    out.insert(out.end(), {prev[cur], cur, next[cur]});
    return out;
}

/* ── several rings, one region ───────────────────────────────────────────────
 * A REAL BOUNDARY IS SEVERAL RINGS (2026-11-17). An administrative boundary has holes
 * (enclaves) and detached parts; as one ring each hole had to be cut in by a
 * "keyhole" seam, and one boundary became over a hundred shapes. Now a region is any number of
 * rings, read by the even-odd rule: a point is inside when an odd number of
 * rings hold it. A hole is a ring inside a ring; a parcel is a ring on its own.
 * No ring needs a direction and none needs to say what it is. */
inline bool rings_contain(const std::vector<std::vector<XY>>& rings, XY p) {
    bool in = false;
    for (const auto& r : rings) in ^= ring_contains(r, p);
    return in;
}

/* Every ring, as one triangle list over their vertices laid end to end (ring 0's
 * first, then ring 1's...). How deeply a ring is nested says what it is: inside
 * an even number of others it is an outline, an odd number a hole, and a hole
 * belongs to the outline nested one less deep around it. Each hole is joined to
 * its outline by a bridge from its rightmost vertex to a corner of the outline
 * it can see (as earcut does), and the joined ring is cut by `triangulate`. The
 * bridge is a seam in the triangles only; nothing is stored or drawn along it. */
inline std::vector<unsigned> triangulate_rings(const std::vector<std::vector<XY>>& rings) {
    std::vector<unsigned> out;
    const std::size_t R = rings.size();
    if (R == 1) return triangulate(rings[0]);
    std::vector<unsigned> base(R, 0);
    for (std::size_t i = 1; i < R; ++i) base[i] = base[i - 1] + (unsigned)rings[i - 1].size();
    std::vector<int> depth(R, 0), parent(R, -1);
    for (std::size_t i = 0; i < R; ++i) {
        if (rings[i].size() < 3) continue;
        for (std::size_t j = 0; j < R; ++j)
            if (j != i && rings[j].size() >= 3 && ring_contains(rings[j], rings[i][0])) ++depth[i];
    }
    for (std::size_t i = 0; i < R; ++i) { // a hole's outline: the ring around it one level up
        if (depth[i] % 2 == 0 || rings[i].size() < 3) continue;
        for (std::size_t j = 0; j < R; ++j)
            if (j != i && depth[j] == depth[i] - 1 && rings[j].size() >= 3 && ring_contains(rings[j], rings[i][0])) {
                parent[i] = (int)j;
                break;
            }
    }
    for (std::size_t o = 0; o < R; ++o) {
        if (depth[o] % 2 != 0 || rings[o].size() < 3) continue;
        // the outline, as indices into the rings laid end to end
        std::vector<unsigned> joined;
        for (unsigned k = 0; k < rings[o].size(); ++k) joined.push_back(base[o] + k);
        std::vector<std::size_t> holes;
        for (std::size_t h = 0; h < R; ++h)
            if (parent[h] == (int)o) holes.push_back(h);
        auto at = [&](unsigned gi) -> const XY& { // a vertex by its index across all rings
            std::size_t r = std::upper_bound(base.begin(), base.end(), gi) - base.begin() - 1;
            return rings[r][gi - base[r]];
        };
        auto rightmost = [&](std::size_t h) {
            std::size_t m = 0;
            for (std::size_t k = 1; k < rings[h].size(); ++k)
                if (rings[h][k].x > rings[h][m].x) m = k;
            return m;
        };
        // rightmost hole first, so a later hole bridges past an earlier one's seam
        std::sort(holes.begin(), holes.end(), [&](std::size_t a, std::size_t b) {
            return rings[a][rightmost(a)].x > rings[b][rightmost(b)].x;
        });
        for (const std::size_t h : holes) {
            const std::size_t mk = rightmost(h);
            const XY M = rings[h][mk];
            // the nearest edge of the joined ring a ray to the right of M meets
            double best = 1e300;
            std::size_t pick = 0;
            bool found = false;
            for (std::size_t k = 0; k < joined.size(); ++k) {
                const XY &A = at(joined[k]), &B = at(joined[(k + 1) % joined.size()]);
                if (A.y == B.y || (A.y > M.y && B.y > M.y) || (A.y < M.y && B.y < M.y)) continue;
                const double t = (M.y - A.y) / (B.y - A.y);
                if (t < 0 || t > 1) continue;
                const double x = A.x + t * (B.x - A.x);
                if (x < M.x || x - M.x >= best) continue;
                best = x - M.x;
                pick = A.x > B.x ? k : (k + 1) % joined.size(); // the edge's right end
                found = true;
            }
            if (!found) continue; // not inside after all: left out of the fill
            /* A corner of the ring inside the triangle (M, hit, picked end) could
             * block the bridge; the one making the smallest angle with the ray is
             * visible from M (earcut's rule). */
            const XY I{M.x + best, M.y}, P = at(joined[pick]);
            double tan_best = 1e300;
            for (std::size_t k = 0; k < joined.size(); ++k) {
                const XY& Q = at(joined[k]);
                if (Q.x < M.x || (Q.x == P.x && Q.y == P.y)) continue;
                auto side = [](XY a, XY b, XY c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); };
                const double s1 = side(M, I, Q), s2 = side(I, P, Q), s3 = side(P, M, Q);
                const bool inside = (s1 >= 0 && s2 >= 0 && s3 >= 0) || (s1 <= 0 && s2 <= 0 && s3 <= 0);
                if (!inside) continue;
                const double tn = std::fabs(Q.y - M.y) / std::max(1e-300, Q.x - M.x);
                if (tn < tan_best) tan_best = tn, pick = k;
            }
            // splice: ... joined[pick], M, the hole round to M again, joined[pick] ...
            std::vector<unsigned> splice;
            const std::size_t hn = rings[h].size();
            for (std::size_t s = 0; s <= hn; ++s) splice.push_back(base[h] + (unsigned)((mk + s) % hn));
            splice.push_back(joined[pick]);
            joined.insert(joined.begin() + (long)pick + 1, splice.begin(), splice.end());
        }
        std::vector<XY> pts;
        pts.reserve(joined.size());
        for (const unsigned gi : joined) pts.push_back(at(gi));
        for (const unsigned t : triangulate(pts)) out.push_back(joined[t]);
    }
    return out;
}

/* `rings` spelled: rings separated by '|', each as `pts` spells one. */
inline std::vector<std::vector<Coord>> parse_rings(const std::string& s) {
    std::vector<std::vector<Coord>> out;
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t j = s.find('|', i);
        if (j == std::string::npos) j = s.size();
        auto r = parse_ring(s.substr(i, j - i));
        if (r.size() >= 3) out.push_back(std::move(r));
        i = j + 1;
    }
    return out;
}

inline std::string format_rings(const std::vector<std::vector<Coord>>& rings) {
    std::string out;
    for (const auto& r : rings) out += (out.empty() ? "" : "|") + format_ring(r);
    return out;
}

} // namespace hormiga::gis
