/* domain/bestow.hpp — tags one rune gives to others.
 *
 * The author (2026-09-15): *"in the map part of things, we can draw a shape, and
 * this shape can bestow tags onto runes that are represented within the area of
 * that shape ... we should add another distinction of 'bestowed' tags ... rather
 * than deleting a bestowed tag, you instead would be redirected to whatever rune
 * or thing is bestowing the tag."* And: *"a tag can exist without a rune
 * explicitly owning it. because the elipse giving the tag 'apple' doesn't itself
 * need the tag 'apple'."*
 *
 * A GIVER is a rune that hands a tag to others by a rule rather than by hand.
 * Today there is one kind, a `mapshape` with `bestows` set, and its rule is
 * "everything whose location is inside me". The shape does not carry the tag;
 * the tag exists because the shape can give it, and a tag that exists that way
 * belongs in the vocabulary a person searches before anyone carries it.
 *
 * WHY BESTOWED STATUS IS WORKED OUT WHEN IT IS DRAWN. Applying a shape writes
 * ordinary `tag` commands, so once applied, a bestowed tag is stored exactly like
 * a hand-added one. Recording the giver on the rune would be a second copy of a
 * fact the shape and the location already state, and it would go stale the
 * moment either moved. So the editor asks, each time: which givers cover this
 * rune now, and does it carry what they give? A tag it carries that a covering
 * giver gives is shown as given, and removing it here would only be undone by
 * the next apply, so the editor sends a person to the giver instead.
 *
 * ONE CONTAINMENT TEST, used by the map's "Apply tag to entities inside" and by
 * the tag editor, so the two cannot disagree about who is inside. That also fixed
 * an ellipse, which was applied by its bounding box and so reached its corners.
 *
 * Pure: a scene in, answers out. An Allomone rule is the next kind of giver the
 * author named; it derives colour and icon properties rather than tags today, so
 * it joins here when it gives something a tag editor shows.
 */
#pragma once

#include "domain/scene_value.hpp" // temper::field_value
#include "gis/geo.hpp"            // parse_geo

#include <algorithm>
#include <string>
#include <vector>

namespace hormiga::bestow {

struct Bestower {
    std::string tag;   // what it gives
    std::string rune;  // who gives it
    std::string glyph; // what kind of giver
    std::string how;   // one sentence, for a tooltip
};

/* Is (lat, lon) inside this map shape? `rect` is the box between the two
 * corners; `ellipse` is the ellipse inscribed in that box. */
inline bool shape_contains(const maiz::SceneNode& shape, double lat, double lon) {
    double la1, lo1, la2, lo2;
    if (!parse_geo(temper::field_value(shape, "geo1"), la1, lo1) ||
        !parse_geo(temper::field_value(shape, "geo2"), la2, lo2))
        return false;
    const double laL = std::min(la1, la2), laH = std::max(la1, la2);
    const double loL = std::min(lo1, lo2), loH = std::max(lo1, lo2);
    if (lat < laL || lat > laH || lon < loL || lon > loH) return false;
    if (temper::field_value(shape, "kind") != "ellipse") return true;
    const double ry = (laH - laL) / 2.0, rx = (loH - loL) / 2.0;
    if (rx <= 0.0 || ry <= 0.0) return false;
    const double dx = (lon - (loL + loH) / 2.0) / rx;
    const double dy = (lat - (laL + laH) / 2.0) / ry;
    return dx * dx + dy * dy <= 1.0;
}

inline Bestower shape_giver(const maiz::SceneNode& shape) {
    std::string label = temper::field_value(shape, "label");
    if (label.empty()) label = shape.name;
    return {temper::field_value(shape, "bestows"), shape.name, shape.glyph,
            "the map shape '" + label + "', which gives it to everything inside it"};
}

// every tag something in the data can give, whether or not anything carries it yet
inline std::vector<Bestower> bestowers(const maiz::Scene& data) {
    std::vector<Bestower> out;
    for (const auto& n : data.nodes)
        if (n.glyph == "mapshape" && !temper::field_value(n, "bestows").empty())
            out.push_back(shape_giver(n));
    return out;
}

/* The givers whose rule covers this rune right now: the shapes its `geo` falls
 * inside. A rune with no location is covered by nothing. */
inline std::vector<Bestower> covering(const maiz::Scene& data, const maiz::SceneNode& rune) {
    std::vector<Bestower> out;
    if (rune.glyph == "mapshape" || rune.glyph == "map") return out;
    double lat, lon;
    if (!parse_geo(temper::field_value(rune, "geo"), lat, lon)) return out;
    for (const auto& n : data.nodes)
        if (n.glyph == "mapshape" && !temper::field_value(n, "bestows").empty() &&
            shape_contains(n, lat, lon))
            out.push_back(shape_giver(n));
    return out;
}

} // namespace hormiga::bestow
