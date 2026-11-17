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
 * plan's y). */
#pragma once

#include "domain/scene_value.hpp" // field_value
#include "gis/geo.hpp"
#include "gis/polygon.hpp"

#include <algorithm>
#include <cstdlib>
#include <string>
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

/* A polygon's or a path's vertices; empty when the rune is neither, or has too
 * few (three for a polygon, two for a path). */
inline std::vector<gis::Coord> vertices(const maiz::SceneNode& n) {
    const std::string k = kind_of(n);
    if (k != "polygon" && k != "path") return {};
    auto v = gis::parse_ring(field_value(n, "pts"));
    if (v.size() < (k == "path" ? 2u : 3u)) v.clear();
    return v;
}

/* The shape's outline as a ring in the geo plane: a polygon's vertices, an
 * ellipse sampled, a box's four corners; a path's vertices, OPEN (read
 * `is_line` before treating it as a ring). Empty when it has no position. */
inline std::vector<gis::XY> ring(const maiz::SceneNode& n, int ellipse_steps = 64) {
    const auto v = vertices(n);
    if (!v.empty()) {
        std::vector<gis::XY> out;
        out.reserve(v.size());
        for (const auto& c : v) out.push_back(gis::xy_of(c));
        return out;
    }
    gis::Coord a, b;
    if (!box_of(n, a, b)) return {};
    if (kind_of(n) == "ellipse") return gis::ellipse_ring(gis::xy_of(a), gis::xy_of(b), ellipse_steps);
    return gis::rect_ring(gis::xy_of(a), gis::xy_of(b));
}

/* Its area in the geo plane (square degrees on Earth): what decides which of
 * two covering shapes is the smaller when colours are given. */
inline double area(const maiz::SceneNode& n) { return is_line(n) ? 0.0 : gis::ring_area(ring(n, 32)); }

/* The commands that write a polygon's vertices: `pts`, and the box that keeps
 * every box-only reader right. One list, so the draw tool, the vertex handles,
 * a move and "Generate perimeter" cannot write one without the other. */
inline std::vector<std::string> vertex_commands(const std::string& name, const std::vector<gis::Coord>& v,
                                                bool line = false) {
    gis::Coord lo, hi;
    if (v.size() < (line ? 2u : 3u) || !gis::ring_bounds(v, lo, hi)) return {};
    return {"set " + name + " pts \"" + gis::format_ring(v) + "\"",
            "set " + name + " geo1 \"" + gis::format_geo(lo) + "\"",
            "set " + name + " geo2 \"" + gis::format_geo(hi) + "\""};
}

} // namespace hormiga::shape
