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
 * "everything whose location is inside me, and every shape I overlap"
 * (shapes since 2026-10-15). The shape does not carry the tag;
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

#include "domain/canvas.hpp"      // which canvas a shape is on, and its positions
#include "domain/scene_value.hpp" // temper::field_value
#include "domain/shape_geom.hpp"  // kinds, rings, polygons (2026-11-14)
#include "gis/relate.hpp"          // a path's reach, and what it meets (2026-11-14)
#include "gis/geo.hpp"            // parse_geo

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <map>
#include <memory>
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
 * corners; `ellipse` is the ellipse inscribed in that box; `polygon` (2026-11-14)
 * is its vertices, by the even-odd rule (gis/polygon.hpp). A PERIMETER contains
 * exactly what an area of the same outline does: the style is only how it is
 * drawn (the author: "computationally they are the same"). */
/* Two geometries in local metres (Earth) or the plan's unit, for `gis::relate`. A
 * rune on a plan is already in its unit; on Earth, degrees become metres about
 * the first one's latitude. */
inline gis::Geom geom_of(const maiz::SceneNode& n, bool earth, double lat0) {
    gis::Geom g;
    g.kind = shape::is_line(n) ? gis::GeomKind::Line : gis::GeomKind::Ring;
    for (const auto& p : shape::ring(n, 32)) g.pts.push_back(earth ? gis::local_metres(p, lat0) : p);
    return g;
}

/* An area's region, read ONCE for many points (2026-11-17): its box, its kind,
 * and a polygon's parsed vertices. The overlap test samples 256 points; each
 * read the box and the vertex text again, and a boundary of a thousand-odd vertices made the
 * inspector take 900 ms a frame. */
struct Region {
    bool ok = false;
    std::string kind;
    double laL = 0, laH = 0, loL = 0, loH = 0;
    std::shared_ptr<const shape::Geometry> g;
};

inline Region region_of(const maiz::SceneNode& shape) {
    Region r;
    double la1, lo1, la2, lo2;
    if (!parse_geo(temper::field_value(shape, "geo1"), la1, lo1) ||
        !parse_geo(temper::field_value(shape, "geo2"), la2, lo2))
        return r;
    r.ok = true;
    r.laL = std::min(la1, la2), r.laH = std::max(la1, la2);
    r.loL = std::min(lo1, lo2), r.loH = std::max(lo1, lo2);
    r.kind = shape::kind_of(shape);
    if (r.kind == "polygon") r.g = shape::geometry(shape);
    return r;
}

inline bool region_contains(const Region& r, double lat, double lon) {
    if (!r.ok || lat < r.laL || lat > r.laH || lon < r.loL || lon > r.loH) return false;
    if (r.kind == "polygon") return !r.g || r.g->contains({lon, lat}); // every ring; too few vertices: its box
    if (r.kind != "ellipse") return true;
    const double ry = (r.laH - r.laL) / 2.0, rx = (r.loH - r.loL) / 2.0;
    if (rx <= 0.0 || ry <= 0.0) return false;
    const double dx = (lon - (r.loL + r.loH) / 2.0) / rx;
    const double dy = (lat - (r.laL + r.laH) / 2.0) / ry;
    return dx * dx + dy * dy <= 1.0;
}

inline bool shape_contains(const maiz::SceneNode& shape, double lat, double lon) {
    if (shape::is_line(shape)) { // a PATH gives to what lies within its reach (2026-11-14)
        const double reach = shape::reach_of(shape);
        const bool earth = canvas::of(shape).empty();
        gis::Geom pt{gis::GeomKind::Point, {earth ? gis::local_metres({lon, lat}, lat) : gis::XY{lon, lat}}};
        const gis::Geom line = geom_of(shape, earth, lat);
        return !line.pts.empty() && gis::relate(pt, line, 1e-9).distance <= reach;
    }
    return region_contains(region_of(shape), lat, lon);
}

/* THE TAGS A SHAPE GIVES (several since 2026-10-05: the author found one raw
 * string unclear, "it doesn't really make it clear how to do multiple tag
 * granting"). Stored as one field, the tags separated by commas or spaces; a
 * leading `+`, `@` or `#` someone typed is not part of the tag. Order kept,
 * duplicates dropped. */
inline std::vector<std::string> bestowed_tags(const maiz::SceneNode& shape) {
    std::vector<std::string> out;
    const std::string v = temper::field_value(shape, "bestows");
    std::string cur;
    auto flush = [&] {
        while (!cur.empty() && (cur[0] == '+' || cur[0] == '@' || cur[0] == '#')) cur.erase(0, 1);
        if (!cur.empty() && std::find(out.begin(), out.end(), cur) == out.end()) out.push_back(cur);
        cur.clear();
    };
    for (char c : v) {
        if (c == ',' || std::isspace((unsigned char)c)) flush();
        else cur += c;
    }
    flush();
    return out;
}

/* The field's value for a list of tags (what an editor writes back). */
inline std::string join_bestowed(const std::vector<std::string>& tags) {
    std::string v;
    for (const auto& t : tags) v += (v.empty() ? "" : ", ") + t;
    return v;
}

inline std::vector<Bestower> shape_givers(const maiz::SceneNode& shape) {
    std::string label = temper::field_value(shape, "label");
    if (label.empty()) label = shape.name;
    std::vector<Bestower> out;
    for (const auto& t : bestowed_tags(shape))
        out.push_back({t, shape.name, shape.glyph,
                       "the map shape '" + label + "', which gives it to everything inside it"});
    return out;
}

// every tag something in the data can give, whether or not anything carries it yet
inline std::vector<Bestower> bestowers(const maiz::Scene& data) {
    std::vector<Bestower> out;
    for (const auto& n : data.nodes)
        if (n.glyph == "mapshape")
            for (auto& b : shape_givers(n)) out.push_back(std::move(b));
    return out;
}

/* DO TWO SHAPES OVERLAP? (2026-10-15) The author: *"overlapping areas DO
 * bestow tags to each other."* A shape is an area, not a point, so "inside"
 * becomes "shares some area with", and it runs both ways: each gives the other
 * what it gives. Same canvas only (an aisle and a neighbourhood are never on
 * one plane). Exact for two rectangles; with an ellipse or a polygon, the shared
 * box is sampled on a 16 x 16 grid and a point inside both decides it, which
 * misses only a sliver thinner than a sixteenth of the box. */
inline bool shapes_overlap(const maiz::SceneNode& a, const maiz::SceneNode& b) {
    if (a.name == b.name || canvas::of(a) != canvas::of(b)) return false;
    double a1, o1, a2, o2, b1, p1, b2, p2;
    if (!parse_geo(temper::field_value(a, "geo1"), a1, o1) || !parse_geo(temper::field_value(a, "geo2"), a2, o2) ||
        !parse_geo(temper::field_value(b, "geo1"), b1, p1) || !parse_geo(temper::field_value(b, "geo2"), b2, p2))
        return false;
    const double laL = std::max(std::min(a1, a2), std::min(b1, b2)), laH = std::min(std::max(a1, a2), std::max(b1, b2));
    const double loL = std::max(std::min(o1, o2), std::min(p1, p2)), loH = std::min(std::max(o1, o2), std::max(p1, p2));
    if (shape::is_line(a) || shape::is_line(b)) { // a path meets what it touches, or comes within its reach of
        const bool earth = canvas::of(a).empty();
        const double lat0 = (a1 + a2) / 2;
        const double reach = std::max(shape::reach_of(a), shape::reach_of(b));
        return gis::relate(geom_of(a, earth, lat0), geom_of(b, earth, lat0), 1e-9).distance <= reach;
    }
    if (laL >= laH || loL >= loH) return false; // the boxes do not even touch
    if (shape::kind_of(a) == "rect" && shape::kind_of(b) == "rect") return true; // two boxes: exact
    const Region ra = region_of(a), rb = region_of(b);
    constexpr int N = 16;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j) {
            const double la = laL + (laH - laL) * (i + 0.5) / N, lo = loL + (loH - loL) * (j + 0.5) / N;
            if (region_contains(ra, la, lo) && region_contains(rb, la, lo)) return true;
        }
    return false;
}

/* Does this giver's rule cover this rune? A shape covers what is placed inside
 * it on its own canvas (an aisle tests a rune's place on the store floor, a
 * neighbourhood its place on Earth) and every shape it overlaps. A rune with no
 * location on a shape's canvas is not covered by it. */
inline bool covers(const maiz::SceneNode& shape, const maiz::SceneNode& rune) {
    if (shape.glyph != "mapshape" || rune.name == shape.name) return false;
    if (rune.glyph == "mapshape") return shapes_overlap(shape, rune);
    if (rune.glyph == "map" || rune.glyph == "canvas" || rune.glyph == "refpoint") return false;
    const std::string field = canvas::geo_field(canvas::shared_channel(canvas::of(shape)));
    double lat, lon;
    return parse_geo(temper::field_value(rune, field.c_str()), lat, lon) && shape_contains(shape, lat, lon);
}

/* Everything a shape gives its tags to right now: the ONE list both the
 * "give them now" action and the tag editor's "given by" read. */
inline std::vector<const maiz::SceneNode*> receivers(const maiz::Scene& data, const maiz::SceneNode& shape) {
    std::vector<const maiz::SceneNode*> out;
    for (const auto& n : data.nodes)
        if (covers(shape, n)) out.push_back(&n);
    return out;
}

/* A COLOUR IS GIVEN LIVE (2026-10-15). The author: *"colors given should be
 * automatically bestowed onto ... runes that they're given to."* A tag in a
 * shape's gives list waits for "Give them now" to be written onto what is
 * inside. A colour does not wait: a `color:` a shape gives colours everything
 * it covers at once, and writes nothing, so a rune moved out loses it and a
 * shape given another colour recolours its contents. A rune's own colour tag
 * still wins. Where several shapes give one rune a colour, the smallest gives
 * it (the aisle, over the store). Between two shapes a colour runs only from
 * the larger to the smaller (tags run both ways): a small shape drawn inside a
 * neighbourhood must not repaint the neighbourhood. `ns` is the tag's
 * namespace ("color"). Rune name -> value. */
inline std::map<std::string, std::string> given_values(const maiz::Scene& data, const std::string& ns) {
    std::map<std::string, std::pair<double, std::string>> best;
    for (const auto& s : data.nodes) {
        if (s.glyph != "mapshape") continue;
        std::string v;
        for (const auto& t : bestowed_tags(s))
            if (t.size() > ns.size() + 1 && t.compare(0, ns.size() + 1, ns + ":") == 0) {
                v = t.substr(ns.size() + 1);
                break;
            }
        if (v.empty()) continue;
        const double area = shape::area(s); // the outline's own area (a polygon is not its box)
        if (area <= 0) continue;
        for (const maiz::SceneNode* r : receivers(data, s)) {
            if (r->glyph == "mapshape" && shape::area(*r) >= area) continue;
            auto it = best.find(r->name);
            if (it == best.end() || area < it->second.first) best[r->name] = {area, v};
        }
    }
    std::map<std::string, std::string> out;
    for (auto& [k, p] : best) out[k] = p.second;
    return out;
}

/* The colours given right now, for the surfaces that draw from the GUI's
 * projection (the canvas, the phone, the calendar, the PNG): rebuilt by
 * `reproject`, like kinds::current(); read on the thread that owns the core.
 * A renderer that projects for itself calls `given_values` on its own scene. */
inline std::map<std::string, std::string>& given_colors() {
    static std::map<std::string, std::string> m;
    return m;
}
inline const std::string* given_color(const std::string& rune) {
    const auto& m = given_colors();
    const auto it = m.find(rune);
    return it == m.end() ? nullptr : &it->second;
}

/* The givers whose rule covers this rune right now (see `covers`). */
inline std::vector<Bestower> covering(const maiz::Scene& data, const maiz::SceneNode& rune) {
    std::vector<Bestower> out;
    if (rune.glyph == "map" || rune.glyph == "canvas") return out;
    for (const auto& n : data.nodes)
        if (covers(n, rune))
            for (auto& b : shape_givers(n)) out.push_back(std::move(b));
    return out;
}

/* ONCE PER CHANGE, NOT ONCE PER FRAME (2026-11-17). The inspector shows what a
 * shape gives to ("Give them now to 12 inside") and who gives a rune its tags,
 * and asked both every frame: a scan of the whole data, and for every shape
 * that shares a box with this one, an overlap test. That answer changes only
 * when the data does, so it is kept until `gen` (the host's count of model
 * changes) or the scene itself differs. Names, never pointers: a scene can be
 * replaced without `gen` moving, and a name that has gone is simply not found.
 * A few entries (the inspector, the tag editor), cleared on each change. */
struct MemoKey {
    std::string mantle;
    std::uint64_t source = 0;
    const void* base = nullptr;
    std::size_t count = 0;
    unsigned gen = 0;
    bool operator==(const MemoKey&) const = default;
};
inline MemoKey memo_key(const maiz::Scene& data, unsigned gen) {
    return {data.mantle, data.source, data.nodes.data(), data.nodes.size(), gen};
}

inline const std::vector<std::string>& receivers_memo(const maiz::Scene& data, const maiz::SceneNode& shape,
                                                      unsigned gen) {
    thread_local MemoKey at;
    thread_local std::map<std::string, std::vector<std::string>> kept;
    if (const MemoKey k = memo_key(data, gen); !(k == at)) at = k, kept.clear();
    auto it = kept.find(shape.name);
    if (it == kept.end()) {
        std::vector<std::string> names;
        for (const maiz::SceneNode* r : receivers(data, shape)) names.push_back(r->name);
        it = kept.emplace(shape.name, std::move(names)).first;
    }
    return it->second;
}

inline const std::vector<Bestower>& covering_memo(const maiz::Scene& data, const maiz::SceneNode& rune, unsigned gen) {
    thread_local MemoKey at;
    thread_local std::map<std::string, std::vector<Bestower>> kept;
    if (const MemoKey k = memo_key(data, gen); !(k == at)) at = k, kept.clear();
    auto it = kept.find(rune.name);
    if (it == kept.end()) it = kept.emplace(rune.name, covering(data, rune)).first;
    return it->second;
}

} // namespace hormiga::bestow
