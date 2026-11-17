/* domain/generated.hpp — a generated shape, as runes, fields and links.
 *
 * gis/generate.hpp makes geometry from seeds on a ground; this is where that meets
 * the database (okf/concepts/sections/gis/generation.md, "Stored as Void Core
 * stores things"):
 *
 *   - SEEDS FROM RUNES. A selected shape is an area seed (its ring); anything else
 *     placed on the canvas is a point seed. The selection's order is the seeds'
 *     order: a path goes through them in the order they were chosen.
 *   - THE GROUND FOR A CANVAS. With snap on, the canvas's own ground (a plan's
 *     grid today; Earth's streets once loaded); with snap off, free space. The
 *     author ties the two together: generation "will greatly utilize the snap to
 *     grid stuff".
 *   - WHAT IS WRITTEN. An ordinary `mapshape`: its vertices (`pts`) and box, so it
 *     never needs the ground again (replay, the phone, an older Hormiga); and its
 *     RECIPE: `made_by` (the generator), `ground` (what it was traced on) and one
 *     LINK to each seed. Links, not a field of names, because a rune's name
 *     follows its title and Void Core repoints links on a rename and drops them on
 *     a removal, which a field of names never gets (domain/naming.hpp). A path's
 *     links are `stop-1`, `stop-2`, ...: the order is the relation's label, since
 *     an edge's weight is a magnitude and a place in a sequence is not one (Void
 *     Core, links.md). A perimeter's are `around`, unordered.
 *   - REGENERATE. The recipe read back (`recipe_of`) and run again: the same
 *     generator, its seeds where they are now, written as new vertices. A seed
 *     that was deleted took its link with it, and the rest still make a shape.
 *
 * Pure: a scene in, commands out. The canvas and `map generate` (map_actions.hpp)
 * both call `compile`, so a click and an agent write the same transcript. */
#pragma once

#include "domain/canvas.hpp"
#include "domain/naming.hpp" // a seed is named by its title
#include "domain/scene_value.hpp"
#include "domain/shape_geom.hpp"
#include "gis/generate.hpp"
#include "voidmaiz/embed.hpp" // maiz::arg: the one quoting rule

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hormiga::generated {

/* Where a placed rune is on this canvas, in the plane (x = lon, y = lat). The
 * canvas answers with where it is DRAWN (a fanned child of a reference point);
 * a headless caller with its stored position. */
using PlaceOf = std::function<bool(const maiz::SceneNode&, gis::XY&)>;

inline PlaceOf stored_place(const std::string& canvas) {
    const std::string field = canvas::geo_field(canvas::shared_channel(canvas));
    return [field](const maiz::SceneNode& n, gis::XY& at) {
        gis::Coord c;
        if (!gis::parse_geo(field_value(n, field.c_str()), c)) return false;
        at = gis::xy_of(c);
        return true;
    };
}

inline std::vector<gis::Seed> seeds_of(const maiz::Scene& s, const std::vector<std::string>& names,
                                       const std::string& canvas, const PlaceOf& place) {
    std::vector<gis::Seed> out;
    for (const auto& nm : names) {
        const maiz::SceneNode* n = s.find(nm);
        if (!n) continue;
        if (n->glyph == "mapshape") {
            if (!canvas::on(*n, canvas)) continue;
            auto r = shape::ring(*n, 32);
            if (shape::is_line(*n)) { // a path as a seed: its vertices as points along it
                for (const auto& p : r) out.push_back(gis::point_seed(nm, p));
                continue;
            }
            if (r.size() >= 3) out.push_back(gis::area_seed(nm, std::move(r)));
        } else if (n->glyph != "map" && n->glyph != "canvas") {
            gis::XY at;
            if (place(*n, at)) out.push_back(gis::point_seed(nm, at));
        }
    }
    return out;
}

/* The ground a generator stands on for this canvas. */
inline std::unique_ptr<gis::Ground> ground_for(const canvas::Canvas& cv, bool use_ground) {
    if (use_ground && cv.plan() && cv.grid > 0) return std::make_unique<gis::GridGround>(cv.grid);
    return std::make_unique<gis::FreeGround>(); // Earth's streets: okf gis/ground.md
}

struct Request {
    std::string which;              // perimeter | path | loop
    std::string name;               // the new shape's name
    std::vector<std::string> seeds; // in order
    std::string canvas;             // "" = Earth
    bool use_ground = false;        // snap on: the canvas's ground
    double margin_x = 0, margin_y = 0; // a perimeter's clearance, in the plane's units
    std::string label;              // its label ("" = made up from the seeds)
    std::string layer;              // a layer to join ("" = the bottom one)
};

struct Result {
    std::vector<std::string> commands; // one batch
    std::string label;                 // what it was named
    std::string error;
};

inline std::string seed_label(const maiz::Scene& s, const std::string& nm) {
    const maiz::SceneNode* n = s.find(nm);
    if (!n) return nm;
    if (n->glyph == "mapshape") {
        const std::string l = field_value(*n, "label");
        return l.empty() ? nm : l;
    }
    // the glyph's own title field (an event's title_en, a contact's display_name), then the usual ones
    for (const std::string& f : {naming::title_field(n->glyph), std::string("title"), std::string("display_name")}) {
        if (f.empty()) continue;
        const std::string v = field_value(*n, f.c_str());
        if (!v.empty()) return v;
    }
    return nm;
}

/* TWO HALVES, SO THE LONG ONE CAN LEAVE THE UI THREAD. `prepare` reads the
 * database (the seeds, the ground, the label) on the thread that owns it, and
 * says what the work will cost; `gis::generate` is pure and may run on a worker
 * behind the busy bar; `finish` turns what it made into commands. `compile` is
 * the three in a row, for a caller that does not need the bar. */
struct Prepared {
    Request rq;
    std::vector<gis::Seed> seeds;
    std::shared_ptr<gis::Ground> ground;
    gis::PerimeterOptions po;
    std::string label;
    double cost = 0; // units of work the ground expects (0: instant)
    std::string error;
};

inline Prepared prepare(const maiz::Scene& s, const Request& rq, const PlaceOf& place) {
    Prepared p;
    p.rq = rq;
    p.seeds = seeds_of(s, rq.seeds, rq.canvas, place);
    p.ground = ground_for(canvas::find(s, rq.canvas), rq.use_ground);
    p.po.margin_x = rq.margin_x, p.po.margin_y = rq.margin_y;
    if (rq.name.empty() || s.find(rq.name)) p.error = "the new shape needs a name nothing has";
    const bool line = rq.which != "perimeter";
    p.label = rq.label;
    if (p.label.empty()) {
        std::vector<std::string> names; // each seed once, in order
        for (const auto& sd : p.seeds)
            if (std::find(names.begin(), names.end(), sd.name) == names.end()) names.push_back(sd.name);
        p.label = line ? "Through " : "Around ";
        for (std::size_t i = 0; i < names.size() && i < 3; ++i) p.label += (i ? ", " : "") + seed_label(s, names[i]);
        if (names.size() > 3) p.label += " and " + std::to_string(names.size() - 3) + " more";
    }
    if (!line && !p.seeds.empty()) { // a perimeter's cost: the ground's cover of the seeds' box, grown by the margin
        double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
        for (const auto& sd : p.seeds) {
            const auto& pts = sd.area() ? sd.ring : std::vector<gis::XY>{sd.at};
            for (const auto& q : pts) x0 = std::min(x0, q.x), x1 = std::max(x1, q.x), y0 = std::min(y0, q.y), y1 = std::max(y1, q.y);
        }
        p.cost = p.ground->cover_cost(gis::rect_ring({x0 - rq.margin_x, y0 - rq.margin_y}, {x1 + rq.margin_x, y1 + rq.margin_y}));
    }
    return p;
}

inline gis::Made make(const Prepared& p, const gis::Progress& prog = {}) {
    return gis::generate(p.rq.which, p.seeds, *p.ground, p.po, prog);
}

inline Result finish(const Prepared& p, const gis::Made& m) {
    Result out;
    out.label = p.label;
    if (!p.error.empty() || !m.ok()) {
        out.error = !p.error.empty() ? p.error : m.error.empty() ? "nothing was made" : m.error;
        return out;
    }
    const Request& rq = p.rq;
    std::vector<gis::Coord> v;
    for (const auto& q : m.pts) v.push_back(gis::coord_of(q));
    const bool line = !m.closed;
    // one command argument, by Void Core's rule (SPEC 6.1: single quotes, an apostrophe escaped)
    auto q = [](const std::string& t) { return maiz::arg(t); };
    auto& c = out.commands;
    c.push_back("rune new mapshape " + rq.name);
    c.push_back("set " + rq.name + " kind \"" + std::string(line ? "path" : "polygon") + "\"");
    if (!line) c.push_back("set " + rq.name + " style \"perimeter\"");
    for (auto& vc : shape::vertex_commands(rq.name, v, line)) c.push_back(std::move(vc));
    c.push_back("set " + rq.name + " label " + q(p.label));
    c.push_back("set " + rq.name + " canvas " + q(rq.canvas));
    c.push_back("set " + rq.name + " made_by " + q(rq.which));
    c.push_back("set " + rq.name + " ground " + q(p.ground->kind()));
    int k = 0;
    std::vector<std::string> linked;
    for (const auto& sd : p.seeds) {
        if (std::find(linked.begin(), linked.end(), sd.name) != linked.end()) continue; // a path seed's many points
        linked.push_back(sd.name);
        c.push_back("link " + rq.name + " " + sd.name + " --relation " +
                    (line ? "stop-" + std::to_string(++k) : std::string("around")));
    }
    c.push_back("tag " + rq.name + " +type:mapshape" + (rq.layer.empty() ? "" : " +layer:" + rq.layer));
    return out;
}

inline Result compile(const maiz::Scene& s, const Request& rq, const PlaceOf& place, const gis::Progress& prog = {}) {
    const Prepared p = prepare(s, rq, place);
    if (!p.error.empty()) {
        Result r;
        r.error = p.error;
        return r;
    }
    return finish(p, make(p, prog));
}

/* A generated shape's recipe, read back: the generator, the ground, and its seeds
 * in order (a path's by the number in `stop-N`, a perimeter's as linked). */
struct Recipe {
    std::string which, ground;
    std::vector<std::string> seeds;
    bool ok() const { return !which.empty() && !seeds.empty(); }
};

inline Recipe recipe_of(const maiz::Scene& s, const maiz::SceneNode& shape) {
    Recipe r;
    r.which = field_value(shape, "made_by");
    r.ground = field_value(shape, "ground");
    if (r.which.empty()) return r;
    std::vector<std::pair<int, std::string>> stops;
    for (const auto& w : s.wires) {
        if (w.from != shape.name) continue;
        if (w.relation == "around") stops.push_back({0, w.to});
        else if (w.relation.rfind("stop-", 0) == 0) stops.push_back({std::atoi(w.relation.c_str() + 5), w.to});
    }
    std::stable_sort(stops.begin(), stops.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    for (auto& [n, nm] : stops) r.seeds.push_back(nm);
    return r;
}

/* Run a shape's recipe again: new vertices from its seeds as they are now. The
 * links and every other field stay. */
inline Result regenerate(const maiz::Scene& s, const maiz::SceneNode& shape, const PlaceOf& place,
                         double margin_x, double margin_y, const gis::Progress& prog = {}) {
    Result out;
    const Recipe r = recipe_of(s, shape);
    if (!r.ok()) {
        out.error = r.which.empty() ? "this shape was drawn, not generated" : "nothing it was made from is left";
        return out;
    }
    const std::string cv = canvas::of(shape);
    const auto seeds = seeds_of(s, r.seeds, cv, place);
    const auto ground = ground_for(canvas::find(s, cv), r.ground == "grid" || r.ground == "streets");
    gis::PerimeterOptions po;
    po.margin_x = margin_x, po.margin_y = margin_y;
    const gis::Made m = gis::generate(r.which, seeds, *ground, po, prog);
    if (!m.ok()) {
        out.error = m.error.empty() ? "nothing was made" : m.error;
        return out;
    }
    std::vector<gis::Coord> v;
    for (const auto& p : m.pts) v.push_back(gis::coord_of(p));
    out.commands = shape::vertex_commands(shape.name, v, !m.closed);
    return out;
}

} // namespace hormiga::generated
