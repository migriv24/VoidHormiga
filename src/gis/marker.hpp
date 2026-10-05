/* gis/marker.hpp — the outline of a map marker, as points. Nothing else.
 *
 * The author, 2026-10-04: "we need a slight overhaul on symbol design for
 * markers on the map. right now it's just a circle ... having real looking
 * 'location markers' would be cool ... look up the formula or the method of how
 * to draw that, so we have our own custom version, without needing to rely on
 * some asset or external library."
 *
 * So the marker is GEOMETRY, computed here once and filled by whoever draws:
 * the live canvas (ImGui's polygon fill), the PNG export (a scanline fill into
 * a pixel buffer) and the website's widget (a port of this function to canvas
 * JS, render/web/app.js — keep the two in step). Three surfaces, one outline,
 * which is what the old shapes never had: the pin was a triangle under a
 * circle, drawn three slightly different ways.
 *
 * ── THE PIN, AS A CONSTRUCTION ──────────────────────────────────────────────
 *
 *          .-"""-.         a circle of radius r (the head), centred h above
 *        /    C    \        the tip T. h = 2.2 r is the classic proportion
 *       |     .     |       (a 27 x 43 pin).
 *        \         /
 *       P_l\     /P_r      P_l and P_r are where a line from T just touches
 *            \   /          the circle: CP is perpendicular to PT, so the
 *             \ /           angle at C between "down" and CP is
 *              T               phi = acos(r / h).
 *
 * A pin with STRAIGHT sides is the circle, an arc over the top from P_r to P_l,
 * and the two tangent lines. It reads as a cone with a ball on it. What makes
 * a pin read as a DROP is that its sides curve in toward the tip, so each side
 * here is a cubic Bezier from P to T whose first control point lies ON the
 * tangent line (so the side leaves the circle smoothly: no kink where they
 * meet) and whose second sits just inside it near the tip (so the side bends
 * in and arrives at a narrow, still-sharp point). The two numbers that shape
 * it (0.45 and 0.18/0.5) are taste, named below, and are the knobs to turn.
 *
 * ── THE BALLOON ─────────────────────────────────────────────────────────────
 * A rounded square standing on a short pointer: the "place card" marker, for a
 * thing that is not a point so much as a note about one. Notes on the map use
 * it by default. Same anchor rule as the pin: the TIP is the location.
 *
 * ── ANCHORS ──────────────────────────────────────────────────────────────────
 * Every outline is relative to (0,0), which is the exact map location: the
 * CENTRE of a circle, square or diamond, the TIP of a pin or balloon. Screen
 * space: y grows downward. `face` is where an icon is drawn and what a finger
 * aims at, which for a pin is the head, not the tip it hides under itself.
 */
#pragma once

#include <cmath>
#include <string>
#include <vector>

namespace hormiga::gis {

struct Pt {
    float x = 0, y = 0;
};

enum class MarkerForm { Circle, Pin, Square, Diamond, Balloon };

inline MarkerForm marker_form(const std::string& s) {
    if (s == "pin") return MarkerForm::Pin;
    if (s == "square") return MarkerForm::Square;
    if (s == "diamond") return MarkerForm::Diamond;
    if (s == "balloon") return MarkerForm::Balloon;
    return MarkerForm::Circle;
}

/* A form that stands on a tip (its location is its point, not its middle). */
inline bool marker_on_tip(MarkerForm f) { return f == MarkerForm::Pin || f == MarkerForm::Balloon; }

struct MarkerOutline {
    std::vector<Pt> pts; // closed, clockwise on screen; first point not repeated
    bool convex = true;  // a fill may take the cheap path
    Pt face;             // where the icon goes, and what a finger aims at
    float face_r = 0;    // the icon's room (a radius)
    Pt lo, hi;           // bounding box
};

namespace detail {
inline void arc(std::vector<Pt>& out, Pt c, float r, float a0, float a1, int n) {
    for (int i = 0; i <= n; ++i) {
        const float a = a0 + (a1 - a0) * (float)i / (float)n;
        out.push_back({c.x + r * std::cos(a), c.y + r * std::sin(a)});
    }
}
inline void cubic(std::vector<Pt>& out, Pt p0, Pt p1, Pt p2, Pt p3, int n, bool skip_first) {
    for (int i = skip_first ? 1 : 0; i <= n; ++i) {
        const float t = (float)i / (float)n, u = 1 - t;
        const float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        out.push_back({a * p0.x + b * p1.x + c * p2.x + d * p3.x, a * p0.y + b * p1.y + c * p2.y + d * p3.y});
    }
}
inline void bounds(MarkerOutline& o) {
    o.lo = o.hi = o.pts.empty() ? Pt{} : o.pts[0];
    for (const Pt& p : o.pts) {
        o.lo.x = std::fmin(o.lo.x, p.x), o.lo.y = std::fmin(o.lo.y, p.y);
        o.hi.x = std::fmax(o.hi.x, p.x), o.hi.y = std::fmax(o.hi.y, p.y);
    }
}
} // namespace detail

// the pin's taste, named (see the header)
inline constexpr float kPinHeight = 2.2f;     // tip-to-centre, in head radii
inline constexpr float kPinLeave = 0.45f;     // how far along the tangent the side stays straight-ish
inline constexpr float kPinTipSpread = 0.18f; // half-width of the side's approach to the tip…
inline constexpr float kPinTipRise = 0.5f;    // …this far above it (both in head radii)

/* The outline of a marker whose head (or body) has radius `r` px. `detail`
 * scales the number of points: 1 is enough for a 10-px marker, 2 for a
 * selected or exported one. */
inline MarkerOutline marker_outline(MarkerForm form, float r, int detail = 1) {
    constexpr float kPi = 3.14159265358979f;
    MarkerOutline o;
    const int seg = 12 * (detail < 1 ? 1 : detail);
    switch (form) {
    case MarkerForm::Pin: {
        const float h = kPinHeight * r;
        const float phi = std::acos(r / h);
        const Pt C{0, -h}, T{0, 0};
        const Pt Pr{C.x + r * std::sin(phi), C.y + r * std::cos(phi)};
        const Pt Pl{C.x - r * std::sin(phi), C.y + r * std::cos(phi)};
        // T -> P_l (the left side, drawn from the tip up), over the top, P_r -> T
        const Pt l1{Pl.x + (T.x - Pl.x) * kPinLeave, Pl.y + (T.y - Pl.y) * kPinLeave};
        const Pt l2{-kPinTipSpread * r, -kPinTipRise * r};
        detail::cubic(o.pts, T, l2, l1, Pl, seg / 2, false);
        o.pts.pop_back(); // P_l is the arc's first point
        // the arc from P_l, angle pi/2 + phi, the long way over the top
        // (3pi/2) to P_r at pi/2 - phi + 2pi: clockwise on screen (y is down)
        detail::arc(o.pts, C, r, kPi / 2 + phi, kPi / 2 - phi + 2 * kPi, seg * 2);
        const Pt r1{Pr.x + (T.x - Pr.x) * kPinLeave, Pr.y + (T.y - Pr.y) * kPinLeave};
        const Pt r2{kPinTipSpread * r, -kPinTipRise * r};
        detail::cubic(o.pts, Pr, r1, r2, T, seg / 2, true);
        o.pts.pop_back(); // T is the first point
        o.convex = false;
        o.face = C;
        o.face_r = r * 0.78f;
        break;
    }
    case MarkerForm::Balloon: {
        const float a = 1.12f * r, b = r, tail = 0.62f * r, tw = 0.4f * r, cr = 0.42f * r;
        const float top = -tail - 2 * b, bot = -tail;
        o.pts.push_back({0, 0});
        o.pts.push_back({-tw, bot});
        detail::arc(o.pts, {-a + cr, bot - cr}, cr, kPi / 2, kPi, seg / 2);       // bottom-left
        detail::arc(o.pts, {-a + cr, top + cr}, cr, kPi, 1.5f * kPi, seg / 2);    // top-left
        detail::arc(o.pts, {a - cr, top + cr}, cr, 1.5f * kPi, 2 * kPi, seg / 2); // top-right
        detail::arc(o.pts, {a - cr, bot - cr}, cr, 0, kPi / 2, seg / 2);          // bottom-right
        o.pts.push_back({tw, bot});
        o.convex = false;
        o.face = {0, bot - b};
        o.face_r = r * 0.82f;
        break;
    }
    case MarkerForm::Square: {
        const float cr = 0.28f * r;
        detail::arc(o.pts, {-r + cr, -r + cr}, cr, kPi, 1.5f * kPi, seg / 3);
        detail::arc(o.pts, {r - cr, -r + cr}, cr, 1.5f * kPi, 2 * kPi, seg / 3);
        detail::arc(o.pts, {r - cr, r - cr}, cr, 0, kPi / 2, seg / 3);
        detail::arc(o.pts, {-r + cr, r - cr}, cr, kPi / 2, kPi, seg / 3);
        o.face_r = r * 0.82f;
        break;
    }
    case MarkerForm::Diamond: {
        const float d = 1.25f * r; // a diamond of the same AREA reads as the same size
        o.pts = {{0, -d}, {d, 0}, {0, d}, {-d, 0}};
        o.face_r = r * 0.7f;
        break;
    }
    case MarkerForm::Circle:
    default:
        detail::arc(o.pts, {0, 0}, r, -kPi / 2, 1.5f * kPi, seg * 2);
        o.pts.pop_back(); // the last equals the first
        o.face_r = r * 0.82f;
        break;
    }
    detail::bounds(o);
    return o;
}

/* Is (x,y), relative to the anchor, inside the outline? Even-odd. */
inline bool marker_contains(const MarkerOutline& o, float x, float y) {
    bool in = false;
    const size_t n = o.pts.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const Pt& a = o.pts[i];
        const Pt& b = o.pts[j];
        if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) in = !in;
    }
    return in;
}

} // namespace hormiga::gis
