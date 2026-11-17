/* domain/shape_geom.hpp — what a `mapshape` rune is, as geometry.
 *
 * Four kinds (2026-11-14): `rect` and `ellipse`, defined by the box between
 * `geo1` and `geo2`; `polygon`, defined by its vertices in `pts`; and `path`, an
 * OPEN line through its vertices in `pts` (two at least), usually generated
 * (domain/generated.hpp). A path has no area: it gives its tags to what lies
 * within its `reach` of it (metres on Earth, the plan's unit on a plan; blank =
 * only what it touches). `pts` is spelled by gis/polygon.hpp. A polygon or a
 * path ALSO keeps `geo1`/`geo2` as its box,
 * written with every change to `pts`, so anything that reads only boxes (an older
 * Hormiga, which draws it as that box; an overlap pre-check) is still right about
 * where it is. A polygon with fewer than three readable points is its box.
 *
 * Two styles, the author's (2026-11-14): an AREA (the default) is filled; a
 * PERIMETER is a dashed outline with nothing inside, hit only on its border, so
 * what is inside it can be clicked and moved. *"computationally they are the
 * same"*: both give their tags and colours by the same containment
 * (domain/bestow.hpp). The style is how it is drawn and hit, nothing else.
 *
 * Pure: a rune in, geometry out (x = longitude or a plan's x, y = latitude or a
 * plan's y). One memo, invisible to callers: a polygon's parsed vertices are kept
 * per rune until its stored `pts` text changes (`geometry`, below). */
#pragma once

#include "domain/scene_value.hpp" // field_value
#include "gis/geo.hpp"
#include "gis/polygon.hpp"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hormiga::shape {

inline std::string kind_of(const maiz::SceneNode& n) {
    const std::string k = field_value(n, "kind");
    return k == "ellipse" || k == "polygon" || k == "path" ? k : "rect";
}

/* A line (a path) rather than an area. */
inline bool is_line(const maiz::SceneNode& n) { return kind_of(n) == "path"; }

inline double reach_of(const maiz::SceneNode& n) {
    const std::string v = field_value(n, "reach");
    return v.empty() ? 0.0 : std::max(0.0, std::atof(v.c_str()));
}

inline bool is_perimeter(const maiz::SceneNode& n) { return field_value(n, "style") == "perimeter"; }

/* The box between geo1 and geo2, as two corners. */
inline bool box_of(const maiz::SceneNode& n, gis::Coord& a, gis::Coord& b) {
    return gis::parse_geo(field_value(n, "geo1"), a) && gis::parse_geo(field_value(n, "geo2"), b);
}

/* A POLYGON'S VERTICES, PARSED ONCE PER CHANGE (2026-11-17). A real boundary
 * has thousands of vertices (an imported boundary: tens of KB of `pts`), and
 * the map, the hit test, the inspector's "inside it" count and the overlap test
 * each re-parsed that text, several times a frame: 900 ms a frame with one
 * boundary selected. Parsed vertices are kept per rune, with the stored text they came
 * from, and parsed again only when that text differs. Handed out shared, so a
 * caller holding two (an overlap test) is safe when the memo is cleared. One per
 * thread: the GUI's and a headless renderer's never meet. */
/*
 * SEVERAL RINGS (2026-11-17). A polygon may also carry `rings`: its holes and
 * detached parts, spelled as `pts` is and separated by '|' (gis::parse_rings).
 * The region is every ring by the even-odd rule (gis::rings_contain). `pts` stays
 * the MAIN outline: the one the vertex handles edit, and the one an older
 * Hormiga, which knows no `rings`, draws. The box covers every ring. */
struct Geometry {
    std::vector<gis::Coord> v; // the main ring (`pts`) as stored: lat, lon
    std::vector<gis::XY> xy;   // the same, as x = lon, y = lat
    std::vector<std::vector<gis::XY>> rings; // every ring, the main one first (a path: just its line)
    std::vector<gis::XY> all;                // every ring's vertices, end to end
    std::vector<std::size_t> starts;         // where each ring starts in `all`; all.size() last
    gis::XY lo{0, 0}, hi{0, 0};              // the box around every ring
    /* A polygon's fill as triangles over `all` (gis::triangulate_rings), cut on
     * first use: what the map draws every frame without cutting it again. */
    const std::vector<unsigned>& triangles() const {
        if (!cut_) tris_ = gis::triangulate_rings(rings), cut_ = true;
        return tris_;
    }
    /* Its area, holes left out: the triangles' areas added up. */
    double area() const {
        double a = 0;
        const auto& t = triangles();
        for (std::size_t i = 0; i + 2 < t.size(); i += 3) a += gis::ring_area({all[t[i]], all[t[i + 1]], all[t[i + 2]]});
        return a;
    }
    bool contains(gis::XY p) const { return gis::rings_contain(rings, p); }

  private:
    mutable std::vector<unsigned> tris_;
    mutable bool cut_ = false;
};

inline const maiz::SceneField* stored_field(const maiz::SceneNode& n, std::string_view key) {
    for (const auto& f : n.fields)
        if (f.key == key) return &f;
    return nullptr;
}

/* A polygon's or a path's vertices, parsed; null when the rune is neither, or
 * has too few (three for a polygon, two for a path). */
inline std::shared_ptr<const Geometry> geometry(const maiz::SceneNode& n) {
    const std::string k = kind_of(n);
    if (k != "polygon" && k != "path") return nullptr;
    const maiz::SceneField* f = stored_field(n, "pts");
    if (!f) return nullptr;
    const maiz::SceneField* fr = k == "polygon" ? stored_field(n, "rings") : nullptr;
    static const std::string none;
    const std::string& rraw = fr ? fr->value_json : none;
    struct Memo {
        std::string raw, rraw, kind;
        std::shared_ptr<const Geometry> g;
    };
    thread_local std::unordered_map<std::string, Memo> memo;
    if (memo.size() > 50000) memo.clear(); // a bound, not a policy: what is drawn is parsed again
    Memo& m = memo[n.name];
    if (m.raw != f->value_json || m.rraw != rraw || m.kind != k) {
        auto g = std::make_shared<Geometry>();
        g->v = gis::parse_ring(field_value(n, "pts"));
        if (g->v.size() < (k == "path" ? 2u : 3u)) g->v.clear();
        g->xy.reserve(g->v.size());
        for (const auto& c : g->v) g->xy.push_back(gis::xy_of(c));
        if (!g->xy.empty()) {
            g->rings.push_back(g->xy);
            if (fr)
                for (const auto& r : gis::parse_rings(field_value(n, "rings"))) {
                    std::vector<gis::XY> x;
                    x.reserve(r.size());
                    for (const auto& c : r) x.push_back(gis::xy_of(c));
                    g->rings.push_back(std::move(x));
                }
            for (const auto& r : g->rings) {
                g->starts.push_back(g->all.size());
                g->all.insert(g->all.end(), r.begin(), r.end());
            }
            g->starts.push_back(g->all.size());
            g->lo = g->hi = g->all.front();
            for (const auto& p : g->all) {
                g->lo.x = std::min(g->lo.x, p.x), g->lo.y = std::min(g->lo.y, p.y);
                g->hi.x = std::max(g->hi.x, p.x), g->hi.y = std::max(g->hi.y, p.y);
            }
        }
        m.raw = f->value_json;
        m.rraw = rraw;
        m.kind = k;
        m.g = g->v.empty() ? nullptr : std::move(g);
    }
    return m.g;
}

/* A polygon's or a path's vertices; empty when the rune is neither, or has too
 * few (three for a polygon, two for a path). */
inline std::vector<gis::Coord> vertices(const maiz::SceneNode& n) {
    const auto g = geometry(n);
    return g ? g->v : std::vector<gis::Coord>{};
}

/* The shape's outline as a ring in the geo plane: a polygon's vertices, an
 * ellipse sampled, a box's four corners; a path's vertices, OPEN (read
 * `is_line` before treating it as a ring). Empty when it has no position. */
inline std::vector<gis::XY> ring(const maiz::SceneNode& n, int ellipse_steps = 64) {
    if (const auto g = geometry(n)) return g->xy;
    gis::Coord a, b;
    if (!box_of(n, a, b)) return {};
    if (kind_of(n) == "ellipse") return gis::ellipse_ring(gis::xy_of(a), gis::xy_of(b), ellipse_steps);
    return gis::rect_ring(gis::xy_of(a), gis::xy_of(b));
}

/* Its area in the geo plane (square degrees on Earth): what decides which of
 * two covering shapes is the smaller when colours are given. */
inline double area(const maiz::SceneNode& n) {
    if (is_line(n)) return 0.0;
    if (const auto g = geometry(n)) return g->area(); // every ring, holes left out
    return gis::ring_area(ring(n, 32));
}

/* A polygon's further rings (`rings`), as stored; empty for any other shape. */
inline std::vector<std::vector<gis::Coord>> extra_rings(const maiz::SceneNode& n) {
    if (kind_of(n) != "polygon") return {};
    return gis::parse_rings(field_value(n, "rings"));
}

/* The commands that write a polygon's vertices: `pts`, and the box that keeps
 * every box-only reader right. One list, so the draw tool, the vertex handles,
 * a move and "Generate perimeter" cannot write one without the other. */
inline std::vector<std::string> vertex_commands(const std::string& name, const std::vector<gis::Coord>& v,
                                                bool line = false,
                                                const std::vector<std::vector<gis::Coord>>& rings = {}) {
    gis::Coord lo, hi;
    if (v.size() < (line ? 2u : 3u) || !gis::ring_bounds(v, lo, hi)) return {};
    std::vector<std::string> out = {"set " + name + " pts \"" + gis::format_ring(v) + "\""};
    if (!rings.empty() && !line) { // its further rings: written with it, and inside its box
        for (const auto& r : rings) {
            gis::Coord a, b;
            if (!gis::ring_bounds(r, a, b)) continue;
            lo.lat = std::min(lo.lat, a.lat), lo.lon = std::min(lo.lon, a.lon);
            hi.lat = std::max(hi.lat, b.lat), hi.lon = std::max(hi.lon, b.lon);
        }
        out.push_back("set " + name + " rings \"" + gis::format_rings(rings) + "\"");
    }
    out.push_back("set " + name + " geo1 \"" + gis::format_geo(lo) + "\"");
    out.push_back("set " + name + " geo2 \"" + gis::format_geo(hi) + "\"");
    return out;
}

} // namespace hormiga::shape
