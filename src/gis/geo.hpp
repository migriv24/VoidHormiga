/* gis/geo.hpp — coordinates, parsing, and distance. The bottom of the GIS layer.
 *
 * ── WHAT `src/gis/` IS, AND WHY IT IS THE STRICTEST FOLDER IN THE TREE ───────
 *
 * This is Void Hormiga's map ENGINE, kept deliberately separable from Void
 * Hormiga. It may depend on nothing but itself and the standard library — not
 * `HormigaApp`, not Void Maiz, not Void Core, not ImGui. `tools/check_layering.py`
 * enforces that, and the enforcement is the whole point: a folder that merely
 * *happens* not to depend on the app today will depend on it by Thursday.
 *
 * The reason it exists is a decision recorded in
 * okf/concepts/foundation/application-boundaries.md and okf/developer_questions.md Q42.
 * The map is four layers, and only one of them is separable:
 *
 *   1. interaction vocabulary  -> Void Maiz's (a flagged gap upstream)
 *   2. the ENGINE              -> THIS FOLDER
 *   3. the map VIEW            -> the host's, per Maiz's custom-view ruling
 *   4. Void GIS, the app       -> its own repo, if and when
 *
 * The author confirmed (2026-08-21) that the **map builder for non-Earth maps
 * is a real intention**, not a someday. That is the second client this folder
 * is waiting for, and it is why the abstractions below are written for authored
 * worlds rather than retrofitted to them later.
 *
 * **A folder is a hypothesis; a repo is a commitment.** This is the hypothesis,
 * and it costs nothing if it is wrong.
 *
 * ── THE COMPUTE BOUNDARY, WHICH PREDATES THIS FOLDER ─────────────────────────
 *
 * No geo type reaches Void Core. A rune stores `geo` as TEXT ("lat,lon") and the
 * model never interprets it; interpretation happens here, on the view side,
 * against whichever source is loaded. That boundary is what makes "don't assume
 * Earth" true in the model rather than merely aspirational — the same rune sits
 * on a map of a real city or a map of a fictional one, and the difference is the
 * source, not the data.
 */
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

namespace hormiga::gis {

inline constexpr double kPi = 3.14159265358979323846;

/* A position, in whatever the loaded source's coordinate system is.
 *
 * The field names say `lat`/`lon` because that is what every caller and every
 * stored rune already says, and renaming them to `y`/`x` across the tree would
 * be churn that buys nothing. On an authored non-Earth map they are simply the
 * two numbers the source's projection understands — the names are historical,
 * the meaning is the source's. */
struct Coord {
    double lat = 0.0;
    double lon = 0.0;
};

/* Read a stored `geo` field. Accepts "lat,lon" and "lat lon" because both have
 * been typed into this application by real people. */
inline bool parse_geo(const std::string& s, double& lat, double& lon) {
    return std::sscanf(s.c_str(), "%lf,%lf", &lat, &lon) == 2 ||
           std::sscanf(s.c_str(), "%lf %lf", &lat, &lon) == 2;
}

inline bool parse_geo(const std::string& s, Coord& c) {
    return parse_geo(s, c.lat, c.lon);
}

/* Write a `geo` field. `%.7g` is ~1 cm at the equator and is what the map's
 * place/move actions have always emitted; keeping one spelling means a rune
 * written by a click and a rune written by an agent look the same in a diff. */
inline std::string format_geo(double lat, double lon) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.7g,%.7g", lat, lon);
    return buf;
}
inline std::string format_geo(const Coord& c) { return format_geo(c.lat, c.lon); }

/* ── DISTANCE IS A PROPERTY OF THE WORLD, NOT OF THE PROGRAM ─────────────────
 *
 * `geo_distance_m` was a free function hardcoded to the haversine formula on a
 * 6,371 km sphere, with a comment that already knew it was wrong to be one:
 *
 *   > Only `near` interprets geo as Earth — a non-Earth map would swap this in
 *   > its source holiday's metric; the model never cares.
 *
 * That comment was right and is now structure: a `Metric` is carried by a
 * `MapSource` (see source.hpp), so "how far apart are these two things" is
 * answered by the world they are in. An authored city map measures in whatever
 * unit its author drew; Earth measures in metres.
 *
 * The free functions stay because they are the metrics themselves, and a metric
 * is a pure function. What changed is that nothing calls one *directly* to mean
 * "distance" — it calls the source's. */

/* Metres between two Earth coordinates (haversine, spherical Earth).
 *
 * `2R·asin(√a)` rather than the `2R·atan2(√a, √(1−a))` form that textbooks
 * prefer for antipodal stability. They are the same function and the atan2
 * spelling is better, but this is the one that has been computing distances in
 * this application, and swapping it would move the last bits of every
 * proximity result for no benefit at the scale of a county. Changing it is a
 * decision to take on purpose, with the golden render as the check — not a
 * detail to smuggle in during a move. */
inline double haversine_m(double lat1, double lon1, double lat2, double lon2) {
    constexpr double R = 6371000.0, D = kPi / 180.0;
    const double dlat = (lat2 - lat1) * D, dlon = (lon2 - lon1) * D;
    const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(lat1 * D) * std::cos(lat2 * D) * std::sin(dlon / 2) *
                         std::sin(dlon / 2);
    return 2 * R * std::asin(std::sqrt(a));
}

/* Plain Euclidean distance in the source's own units — the metric an authored
 * flat world wants, where "3 units" means three units and there is no sphere to
 * correct for. */
inline double euclidean(double y1, double x1, double y2, double x2) {
    const double dy = y2 - y1, dx = x2 - x1;
    return std::sqrt(dy * dy + dx * dx);
}

} // namespace hormiga::gis
