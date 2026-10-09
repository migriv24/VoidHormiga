/* domain/canvas.hpp — canvases with data (okf/concepts/sections/gis/canvases.md).
 *
 * The author, 2026-10-06: maps generalize into "canvases with data"; "a normal
 * earth map is a very useful canvas. And we would wanna make maps of anything",
 * starting with indoor layouts and a grocery store's aisles.
 *
 * A CANVAS is a world with its own layers, regions and positions. It is a
 * `canvas` rune; layers (`map`), regions (`mapshape`) and reference points
 * belong to one through their `canvas` field, and BLANK MEANS EARTH, so every
 * database made before canvases keeps its territory exactly as it was.
 *
 * POSITIONS ON A DRAWN CANVAS NEVER FALL BACK TO EARTH. A position channel
 * whose name starts `cv_` belongs to a canvas; `view_geo` (ui/map.cpp) reads
 * only `geo_cv_...` for it, never the Earth `geo`. Latitude 47.6 is not 47.6
 * metres into a store. Kept as a naming rule rather than a flag so every caller
 * of `view_geo` (desktop, phone, the PNG, containment) obeys it without being
 * told.
 *
 * A PLAN's coordinates are metres from its top-left corner, stored the way
 * every position is ("lat,lon" = "y,x"), and projected flat (gis/projection.hpp)
 * over a world `kPlanSpan` metres wide, so 2^z screen px per metre at zoom z.
 *
 * ImGui-free: the desktop, the phone and the CLI read the same answers. */
#pragma once

#include "domain/scene_value.hpp" // temper::field_value

#include "voidmaiz/scene.hpp"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace hormiga::canvas {

inline constexpr double kPlanSpan = 256.0; // metres across the flat world at zoom 0
inline constexpr double kPlanMinZoom = 1, kPlanMaxZoom = 9; // 2 .. 512 px per metre

struct Canvas {
    std::string name;            // "" = the Earth canvas (no rune)
    std::string title = "Earth"; // what a person reads
    std::string world = "earth"; // earth | plan
    double w = 40, h = 24;       // a plan's size, metres
    double grid = 1;             // a plan's grid, in its unit
    std::string unit = "m";      // what its numbers are: m (a floor), ft or sq (a campaign's map)
    bool plan() const { return world == "plan"; }
    /* The flat world's width at zoom 0: 256 units for a room or a store, more
     * for a campaign's map of a forest, so the whole of it fits at the first zooms. */
    double span() const {
        double s = kPlanSpan;
        while (s < w || s < h) s *= 2;
        return s;
    }
    double min_zoom() const { return 0; }
    /* The whole zoom level at which the canvas fits a view of px_w by px_h
     * (256 px a tile, as the desktop draws): a room close up, a forest far out. */
    int fit_zoom(double px_w = 900, double px_h = 420) const {
        const double per_unit = std::min(px_w / w, px_h / h) * 0.9;
        const double z = std::floor(std::log2(per_unit * span() / kPlanSpan));
        return (int)std::max(min_zoom(), std::min(z, kPlanMaxZoom + std::log2(span() / kPlanSpan)));
    }
    double max_zoom() const { return kPlanMaxZoom + std::log2(span() / kPlanSpan); }
};

inline double num(const maiz::SceneNode& n, const char* key, double d) {
    const std::string v = field_value(n, key);
    if (v.empty()) return d;
    const double x = std::atof(v.c_str());
    return x > 0 ? x : d;
}

inline Canvas from_rune(const maiz::SceneNode& n) {
    Canvas c;
    c.name = n.name;
    c.title = field_value(n, "title");
    if (c.title.empty()) c.title = n.name;
    c.world = field_value(n, "world");
    if (c.world.empty()) c.world = "plan";
    c.w = num(n, "width", 40);
    c.h = num(n, "height", 24);
    c.grid = num(n, "grid", 1);
    c.unit = field_value(n, "unit");
    if (c.unit.empty()) c.unit = "m";
    return c;
}

/* Every canvas: Earth first, then the canvas runes as the scene has them. */
inline std::vector<Canvas> all(const maiz::Scene& s) {
    std::vector<Canvas> out(1);
    for (const auto& n : s.nodes)
        if (n.glyph == "canvas") out.push_back(from_rune(n));
    return out;
}

/* The canvas named `name`, or Earth when it is blank or gone. */
inline Canvas find(const maiz::Scene& s, const std::string& name) {
    if (!name.empty())
        for (const auto& n : s.nodes)
            if (n.glyph == "canvas" && n.name == name) return from_rune(n);
    return Canvas{};
}

/* Which canvas a layer, region or reference point belongs to ("" = Earth). */
inline std::string of(const maiz::SceneNode& n) { return field_value(n, "canvas"); }

/* Does this rune draw on canvas `name`? Only the kinds that belong to one. */
inline bool on(const maiz::SceneNode& n, const std::string& name) { return of(n) == name; }

/* The position channel a canvas's layers share, and one a layer keeps alone.
 * Earth's are the ones layers always had ("main", the layer's own name). */
inline std::string shared_channel(const std::string& canvas) {
    return canvas.empty() ? std::string("main") : "cv_" + canvas;
}
inline std::string own_channel(const std::string& canvas, const std::string& layer) {
    return canvas.empty() ? layer : "cv_" + canvas + "_" + layer;
}
/* Is this a canvas channel (no fallback to the Earth position)? */
inline bool strict(const std::string& channel) { return channel.rfind("cv_", 0) == 0; }

/* The position field a channel reads and writes ("geo", "geo_<channel>"). */
inline std::string geo_field(const std::string& channel) {
    return channel.empty() || channel == "main" ? std::string("geo") : "geo_" + channel;
}

/* A canvas name read back from `config get` (a value loaded from a file comes
 * back quoted; one set this session does not). */
inline std::string config_name(std::string v) {
    auto junk = [](char c) { return c == '"' || std::isspace((unsigned char)c); };
    while (!v.empty() && junk(v.back())) v.pop_back();
    while (!v.empty() && junk(v.front())) v.erase(0, 1);
    return v;
}

/* Where the camera of each canvas is remembered (config tier, undo-exempt). */
inline std::string camera_key(const std::string& base, const std::string& canvas) {
    return canvas.empty() ? base : base + "." + canvas;
}

} // namespace hormiga::canvas
