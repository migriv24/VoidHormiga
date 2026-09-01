/* map_actions.hpp — Territory's interaction vocabulary as NAMED actions
 * (Void Maiz `action.hpp` draft, 2026-07-21; okf/concepts/sections/territory.md
 * "Canvas actions as first-class commands").
 *
 * Each ActionDescriptor's `compile` is THE one definition: today the command
 * bar's `map <action> …` verb-macro front-end calls it (Core's ruling: model
 * mutations dispatch as ONE `batch` — pure → undoable → attributed); when the
 * live map canvas lands, its gestures call the SAME compile — a volunteer's
 * click-to-place and an agent's `map place …` are the same transcript entry.
 * The param schemas are written to double as the future Core verb arg-spec
 * (Core's convergence note: keep them one type).
 *
 * Geo convention: `geo` = "lat,lon" (two floats, comma-separated) — map
 * coordinates, not assumed Earth (a fantasy map's units ride the same field;
 * the SOURCE holiday defines the space). Headless: Scene in, commands out.
 */
#pragma once

#include "gis/geo.hpp"       // the engine owns coordinates now — see that file
#include "voidmaiz/action.hpp"

#include <cmath>
#include <cstdio>

namespace hormiga {

/* ── COORDINATES MOVED TO `src/gis/` (2026-08-21) ────────────────────────────
 *
 * `parse_geo` and `geo_distance_m` were defined here, in the ACTIONS header,
 * which is where they were first needed rather than where they belong. They are
 * the map engine's, and the engine is now `src/gis/` — a folder that may depend
 * on nothing, so that the one genuinely separable layer of the map stays
 * separable (okf/concepts/foundation/application-boundaries.md, Q42).
 *
 * Re-exported under their old names because seven call sites across five files
 * say `hormiga::parse_geo`, and renaming them would be churn in a move whose
 * whole claim is that it changes no behaviour. The names are the compatibility
 * surface; `hormiga::gis` is the truth.
 *
 * `geo_distance_m` keeps its name and its Earth metric, and is now the wrong
 * thing for a caller to reach for by default: distance is a property of the
 * WORLD, so a caller that has a `MapSource` should ask it (`src.distance(...)`)
 * and get metres on Earth or the source's own units on an authored map. This
 * stays for the callers that genuinely mean "metres on Earth". */
using gis::parse_geo;
inline double geo_distance_m(double lat1, double lon1, double lat2, double lon2) {
    return gis::haversine_m(lat1, lon1, lat2, lon2);
}

/* The registry: Territory's afforded actions. Host-owned, rebuilt at boot
 * (like FaceRegistry/WidgetRegistry). */
inline maiz::ActionRegistry make_map_actions() {
    maiz::ActionRegistry reg;

    // map place <glyph> <name> <geo> — mint a located rune (one batch)
    reg.add(maiz::ActionDescriptor{
        "place", "Place on the map",
        "Create a new rune of <glyph> named <name> at <geo> (lat,lon). "
        "Tags it +type:<glyph> and +located.",
        {{"glyph", "glyph", true, "rune kind: contact|organization|event"},
         {"name", "text", true, "command-safe rune name (slug)"},
         {"geo", "geo", true, "map coordinates: lat,lon"},
         {"field", "text", false,
          "position field (default geo = the main channel; geo_<view> for an "
          "unlocked view's own positions)"}},
        "click", // the canvas binds click-to-place to THIS action
        [](const maiz::Scene& scene, const maiz::ActionArgs& args) {
            std::vector<std::string> out;
            auto g = args.find("glyph"), n = args.find("name"), p = args.find("geo");
            if (g == args.end() || n == args.end() || p == args.end()) return out;
            double lat, lon;
            if (!parse_geo(p->second, lat, lon)) return out;
            if (scene.find(n->second)) return out; // name taken: decline
            auto f = args.find("field");
            std::string field = (f == args.end() || f->second.empty()) ? "geo"
                                                                       : f->second;
            char geo[64];
            std::snprintf(geo, sizeof geo, "%.7g,%.7g", lat, lon);
            out.push_back("rune new " + g->second + " " + n->second);
            out.push_back("set " + n->second + " " + field + " \"" + geo + "\"");
            out.push_back("tag " + n->second + " +type:" + g->second + " +located");
            return out;
        }});

    // map move <name> <geo> — relocate an existing rune
    reg.add(maiz::ActionDescriptor{
        "move", "Move on the map",
        "Set an existing rune's location to <geo> (lat,lon). Adds +located "
        "if missing.",
        {{"name", "node", true, "an existing rune's name"},
         {"geo", "geo", true, "map coordinates: lat,lon"},
         {"field", "text", false,
          "position field (default geo = main; geo_<view> for an unlocked "
          "view's own positions)"}},
        "drag", // the canvas binds marker-drag to THIS action
        [](const maiz::Scene& scene, const maiz::ActionArgs& args) {
            std::vector<std::string> out;
            auto n = args.find("name"), p = args.find("geo");
            if (n == args.end() || p == args.end()) return out;
            const maiz::SceneNode* node = scene.find(n->second);
            double lat, lon;
            if (!node || !parse_geo(p->second, lat, lon)) return out;
            auto f = args.find("field");
            std::string field = (f == args.end() || f->second.empty()) ? "geo"
                                                                       : f->second;
            char geo[64];
            std::snprintf(geo, sizeof geo, "%.7g,%.7g", lat, lon);
            out.push_back("set " + n->second + " " + field + " \"" + geo + "\"");
            bool located = false;
            for (const auto& t : node->tags)
                if (t == "located") located = true;
            if (!located) out.push_back("tag " + n->second + " +located");
            return out;
        }});

    return reg;
}

} // namespace hormiga
