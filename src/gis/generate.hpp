/* gis/generate.hpp — geometry made from what is selected: one system, several
 * generators.
 *
 * The author (2026-11-14): *"the modules and code that create this SHOULD work for
 * both of the functionalities ... we should start thinking in terms of
 * generalizability"*, with two generators to begin: *Generate perimeter* (around
 * areas and points, which need not touch) and *Generate path* (through points,
 * in the order they were selected). Design: okf/concepts/sections/gis/generation.md.
 *
 * THREE PIECES, AND THE GENERATORS ARE MADE OF THEM:
 *
 *   SEEDS   what was selected, in order: each a point (a contact, an event, any
 *           placed kind) or an area (a shape's ring). A generator reads seeds,
 *           never runes, so this file needs no database.
 *   GROUND  what the canvas lets you travel on (okf gis/ground.md): `snap` a point
 *           onto it, `trace` the way from one point to another along it, `cover`
 *           a region with its blocks. Free space has no blocks and goes straight;
 *           a plan's grid goes in steps and its blocks are its cells; streets
 *           (planned) go along streets and their blocks are what streets enclose.
 *   MADE    the result: vertices, open (a line) or closed (a ring), or why none.
 *
 *   perimeter = cover(padded hull of every seed)          -> a ring
 *   path      = trace(stop 1, stop 2) + trace(2, 3) + ... -> a line
 *
 * A new generator is a new composition of the same three (a loop is a path back
 * to its first stop; a corridor would be a path grown into a ring), and a new
 * ground makes every generator work on a new kind of map.
 *
 * COST AND THE BAR. A ground says what a call will cost (`cost`, in units of
 * work) before it is made, and reports progress through a `Progress` that may
 * stop it; the caller puts anything long behind the busy curtain's bar (the
 * author: "always remember to use loading bars"). Free and grid grounds are
 * cheap; streets are not.
 *
 * Pure: points in, points out. Dependency-free, as everything in src/gis/. */
#pragma once

#include "gis/polygon.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace hormiga::gis {

/* ── seeds ─────────────────────────────────────────────────────────────────── */
struct Seed {
    std::string name;     // the rune it came from: what a generated shape links to
    std::vector<XY> ring; // an area's outline; empty for a point
    XY at;                // a point's place; an area's centre
    bool area() const { return ring.size() >= 3; }
};

/* The centre of a ring by area (the shoelace centroid), or the mean of its
 * points when it has no area. Where a path goes to reach an area. */
inline XY centroid(const std::vector<XY>& r) {
    double a = 0, cx = 0, cy = 0;
    const std::size_t n = r.size();
    for (std::size_t i = 0; i < n; ++i) {
        const XY& p = r[i];
        const XY& q = r[(i + 1) % n];
        const double c = p.x * q.y - q.x * p.y;
        a += c, cx += (p.x + q.x) * c, cy += (p.y + q.y) * c;
    }
    if (std::fabs(a) > 1e-18) return {cx / (3 * a), cy / (3 * a)};
    XY m{0, 0};
    for (const auto& p : r) m.x += p.x / (double)n, m.y += p.y / (double)n;
    return m;
}

inline Seed point_seed(std::string name, XY at) { return {std::move(name), {}, at}; }
inline Seed area_seed(std::string name, std::vector<XY> ring) {
    Seed s{std::move(name), std::move(ring), {}};
    s.at = centroid(s.ring);
    return s;
}

/* Progress: the fraction done; return false to stop. Empty: nobody is watching. */
using Progress = std::function<bool(double)>;

/* ── the ground ───────────────────────────────────────────────────────────── */
struct Ground {
    virtual ~Ground() = default;
    virtual const char* kind() const = 0; // "free", "grid", "streets": stored with what it made
    virtual XY snap(XY p) const = 0;
    /* The way from a to b along the ground, both ends included. Empty: no way
     * within the ground's reach (`why` says so). */
    virtual std::vector<XY> trace(XY a, XY b, std::string& why) const = 0;
    /* The outline of the ground's blocks that `region` meets, holes filled. The
     * free ground has no blocks: it returns the region itself. */
    virtual std::vector<XY> cover(const std::vector<XY>& region, std::string& why, const Progress& p) const = 0;
    /* Units of work `cover` would do on this region (the bar's denominator). */
    virtual double cover_cost(const std::vector<XY>& region) const { (void)region; return 0; }
};

/* FREE SPACE: no streets, no grid. A trace is the straight segment; a cover is the
 * region itself. Every canvas has this ground when nothing better is loaded. */
struct FreeGround final : Ground {
    const char* kind() const override { return "free"; }
    XY snap(XY p) const override { return p; }
    std::vector<XY> trace(XY a, XY b, std::string&) const override { return {a, b}; }
    std::vector<XY> cover(const std::vector<XY>& region, std::string&, const Progress&) const override { return region; }
};

/* A PLAN'S GRID: a lattice of `step` (the plan's unit). Snapping rounds to the
 * nearest crossing; a trace is the staircase the author described ("a step map"):
 * along the grid's lines, first across then down, the same every time; the
 * blocks are the cells, and a cover is the outline of the cells whose centre
 * the region holds (the cell under the region's centre when it holds none). */
struct GridGround final : Ground {
    double step = 1;
    std::int64_t max_cells = 1'000'000; // past this the region is too large for one cover
    explicit GridGround(double s) : step(s > 0 ? s : 1) {}
    const char* kind() const override { return "grid"; }
    XY snap(XY p) const override { return {snap_to(p.x, step), snap_to(p.y, step)}; }
    std::vector<XY> trace(XY a, XY b, std::string&) const override {
        a = snap(a), b = snap(b);
        std::vector<XY> out{a};
        if (a.x != b.x && a.y != b.y) out.push_back({b.x, a.y}); // the one corner of the staircase
        if (a.x != b.x || a.y != b.y) out.push_back(b);
        return out;
    }
    double cover_cost(const std::vector<XY>& region) const override {
        Coord lo, hi;
        std::vector<Coord> c;
        for (const auto& p : region) c.push_back(coord_of(p));
        if (!ring_bounds(c, lo, hi)) return 0;
        return std::ceil((hi.lon - lo.lon) / step + 1) * std::ceil((hi.lat - lo.lat) / step + 1);
    }
    std::vector<XY> cover(const std::vector<XY>& region, std::string& why, const Progress& prog) const override;
};

/* The outline of a set of grid cells, holes filled: every cell's four edges,
 * counter-clockwise; an edge two cells share runs both ways and cancels; what is
 * left is the boundary, chained into loops, and the loop enclosing most is the
 * outline. Cells are (i, j) = [i*step, (i+1)*step) x [j*step, (j+1)*step). */
inline std::vector<XY> cells_outline(const std::set<std::pair<std::int64_t, std::int64_t>>& cells, double step) {
    using P = std::pair<std::int64_t, std::int64_t>;
    std::set<std::pair<P, P>> edges;
    auto add = [&](P a, P b) {
        if (edges.erase({b, a})) return; // shared with a neighbour: inside
        edges.insert({a, b});
    };
    for (const auto& [i, j] : cells) {
        add({i, j}, {i + 1, j});
        add({i + 1, j}, {i + 1, j + 1});
        add({i + 1, j + 1}, {i, j + 1});
        add({i, j + 1}, {i, j});
    }
    std::multimap<P, P> next;
    for (const auto& [a, b] : edges) next.insert({a, b});
    std::vector<XY> best;
    double best_area = -1;
    while (!next.empty()) {
        auto it = next.begin();
        const P start = it->first;
        std::vector<P> loop{start};
        P at = it->second;
        next.erase(it);
        while (at != start) {
            loop.push_back(at);
            auto nx = next.find(at);
            if (nx == next.end()) break; // an open chain cannot happen with whole cells; stop rather than spin
            at = nx->second;
            next.erase(nx);
        }
        std::vector<XY> ring;
        for (std::size_t k = 0; k < loop.size(); ++k) { // drop the points in the middle of a straight run
            const P& a = loop[(k + loop.size() - 1) % loop.size()];
            const P& b = loop[k];
            const P& c = loop[(k + 1) % loop.size()];
            const bool straight = (a.first == b.first && b.first == c.first) || (a.second == b.second && b.second == c.second);
            if (!straight) ring.push_back({(double)b.first * step, (double)b.second * step});
        }
        const double ar = ring_area(ring);
        if (ar > best_area) best_area = ar, best = std::move(ring);
    }
    return best;
}

inline std::vector<XY> GridGround::cover(const std::vector<XY>& region, std::string& why, const Progress& prog) const {
    if (region.size() < 3) {
        why = "nothing to cover";
        return {};
    }
    double x0 = region[0].x, x1 = x0, y0 = region[0].y, y1 = y0;
    for (const auto& p : region) x0 = std::min(x0, p.x), x1 = std::max(x1, p.x), y0 = std::min(y0, p.y), y1 = std::max(y1, p.y);
    const auto i0 = (std::int64_t)std::floor(x0 / step), i1 = (std::int64_t)std::floor(x1 / step);
    const auto j0 = (std::int64_t)std::floor(y0 / step), j1 = (std::int64_t)std::floor(y1 / step);
    if ((i1 - i0 + 1) * (j1 - j0 + 1) > max_cells) {
        why = "the region spans more grid cells than one perimeter is allowed";
        return {};
    }
    std::set<std::pair<std::int64_t, std::int64_t>> cells;
    for (auto i = i0; i <= i1; ++i) {
        if (prog && !prog((double)(i - i0) / (double)(i1 - i0 + 1))) {
            why = "stopped";
            return {};
        }
        for (auto j = j0; j <= j1; ++j)
            if (ring_contains(region, {(i + 0.5) * step, (j + 0.5) * step})) cells.insert({i, j});
    }
    if (cells.empty()) { // a region smaller than a cell: the cell under its centre
        const XY c = centroid(region);
        cells.insert({(std::int64_t)std::floor(c.x / step), (std::int64_t)std::floor(c.y / step)});
    }
    return cells_outline(cells, step);
}

/* ── what a generator makes ───────────────────────────────────────────────── */
struct Made {
    std::vector<XY> pts;
    bool closed = false; // a ring (an area or a perimeter) or a line (a path)
    std::string error;   // non-empty: nothing was made, and why
    bool ok() const { return error.empty() && pts.size() >= (closed ? 3u : 2u); }
};

struct PerimeterOptions {
    double margin_x = 0, margin_y = 0; // kept clear of every seed (the plane's units)
    int round_steps = 8;               // points round each seed's margin
};

/* PERIMETER: one ring around every seed, which need not touch. The padded hull of
 * every area's outline and every point, covered by the ground's blocks: on free
 * space the hull itself, on a grid the cells it holds, on streets (planned) the
 * blocks it meets. */
inline Made make_perimeter(const std::vector<Seed>& seeds, const Ground& g, const PerimeterOptions& o,
                           const Progress& prog = {}) {
    Made m;
    m.closed = true;
    std::vector<XY> pts;
    for (const auto& s : seeds) {
        if (s.area()) pts.insert(pts.end(), s.ring.begin(), s.ring.end());
        else pts.push_back(s.at);
    }
    if (pts.empty()) {
        m.error = "nothing selected has a place on this map";
        return m;
    }
    const auto hull = padded_hull(pts, o.margin_x, o.margin_y, o.round_steps);
    if (hull.size() < 3) {
        m.error = "too little to go around";
        return m;
    }
    m.pts = g.cover(hull, m.error, prog);
    if (m.error.empty() && m.pts.size() < 3) m.error = "the ground left nothing to go around";
    return m;
}

struct PathOptions {
    bool loop = false; // back to the first stop (a round)
};

/* PATH: a line through the seeds in the order given, each leg traced along the
 * ground. A point is a stop where it is; an area is a stop at its centre. Legs
 * are joined without repeating the point where one ends and the next begins. */
inline Made make_path(const std::vector<Seed>& seeds, const Ground& g, const PathOptions& o, const Progress& prog = {}) {
    Made m;
    m.closed = false;
    if (seeds.size() < 2) {
        m.error = "a path needs at least two stops";
        return m;
    }
    std::vector<XY> stops;
    for (const auto& s : seeds) stops.push_back(s.at);
    if (o.loop) stops.push_back(stops.front());
    for (std::size_t k = 0; k + 1 < stops.size(); ++k) {
        if (prog && !prog((double)k / (double)(stops.size() - 1))) {
            m.error = "stopped";
            m.pts.clear();
            return m;
        }
        auto leg = g.trace(stops[k], stops[k + 1], m.error);
        if (leg.empty()) {
            if (m.error.empty()) m.error = "no way between two of the stops";
            m.pts.clear();
            return m;
        }
        for (std::size_t i = 0; i < leg.size(); ++i) {
            if (!m.pts.empty() && i == 0 && leg[0].x == m.pts.back().x && leg[0].y == m.pts.back().y) continue;
            m.pts.push_back(leg[i]);
        }
    }
    return m;
}

/* The generators by name: what a menu, the CLI and a stored recipe all call. */
inline Made generate(const std::string& which, const std::vector<Seed>& seeds, const Ground& g,
                     const PerimeterOptions& po, const Progress& prog = {}) {
    if (which == "perimeter") return make_perimeter(seeds, g, po, prog);
    if (which == "path") return make_path(seeds, g, {}, prog);
    if (which == "loop") return make_path(seeds, g, {true}, prog);
    Made m;
    m.error = "no generator called '" + which + "'";
    return m;
}

} // namespace hormiga::gis
