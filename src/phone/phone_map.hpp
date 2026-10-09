/* phone/phone_map.hpp — the phone map's own state and its small helpers, shared
 * by phone_map.cpp (the canvas: tiles, markers, gestures, the floating
 * controls) and phone_map_sheet.cpp (the bottom sheet and the map's own
 * screens: search, put-something-here, views). Nobody else includes it.
 * Split on 2026-10-04 when the map outgrew one file's budget; see
 * phone_map.cpp for what the screen is and why. */
#pragma once

#include "phone/phone_ui.hpp"

#include "gis/marker.hpp"
#include "gis/view.hpp"
#include "voidmaiz/location.hpp"

#include <cfloat>
#include <cmath>
#include <string>
#include <vector>

namespace hormiga::phone::mapx {
inline constexpr const char* kCamKey = "view.phone.map.camera";

inline float ease_out(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
}

inline bool has_tag(const maiz::SceneNode& n, const std::string& t) {
    return std::find(n.tags.begin(), n.tags.end(), t) != n.tags.end();
}

inline std::string geo_str(double la, double lo) {
    char b[64];
    std::snprintf(b, sizeof b, "%.7g,%.7g", la, lo);
    return b;
}

/* A distance a person reads: "12 m", "1.3 km". */
inline std::string human_m(double m) {
    char b[32];
    if (m < 1000) std::snprintf(b, sizeof b, "%.0f m", m);
    else std::snprintf(b, sizeof b, "%.1f km", m / 1000.0);
    return b;
}

inline ImU32 with_alpha(ImU32 c, int a) { return (c & 0x00FFFFFFu) | ((ImU32)a << 24); }

} // namespace hormiga::phone::mapx

/* ── the map's own state ─────────────────────────────────────────────────────
 * View ephemera, all of it: dropping it loses at most a gesture in flight and
 * where the camera was since the last flush. Nothing here is truth. */
struct HormigaApp::PhoneUi::MapUi {
    hormiga::gis::SlippyView v;
    bool loaded = false;
    std::string canvas_shown = "\x01"; // the canvas the camera was loaded for (domain/canvas.hpp)

    // motion: a coast after a fling, and an animation (zoom about a point, or a flight)
    float coast_x = 0, coast_y = 0;
    bool coasting = false;
    struct Anim {
        bool on = false, fly = false;
        double t0 = 0, dur = 0.28;
        double z0 = 0, z1 = 0;
        double fx0 = 0, fy0 = 0, fx1 = 0, fy1 = 0; // fly: mercator fractions
        float px = 0, py = 0;                       // zoom: the point that stays put
    } anim;
    double moved_at = 0; // the last time the camera changed
    bool cam_dirty = false;

    // the press on the canvas
    bool pressing = false;
    ImVec2 press_at{0, 0};
    double press_t = 0;
    float travel = 0;      // the most the press moved, px
    bool long_done = false;
    int since_pan = 99;    // frames since a pan released (a fling arrives a frame or two late)
    std::string lift;      // a marker picked up by a long press
    bool lift_ref = false; // …which is a reference point
    ImVec2 drop_at{-1, -1};

    // what is selected, and what the sheet shows
    enum Sheet { None, Marker, Place, Pick, Shape, Ref };
    Sheet sheet = None;
    std::string sel;
    std::vector<std::string> pick; // several under one tap: which?
    maiz::BottomSheetState bs;
    int bs_for = -1; // the sheet mode its detents were set for
    double place_la = 0, place_lo = 0;
    float place_acc = 0; // > 0: the place came from the phone's location

    // placement: a pin fixed in the middle, the map moved under it
    bool adjust = false;
    std::string adjust_rune; // "" = something new

    // drawing a region: 0 off, 1 rectangle, 2 ellipse
    int draw = 0;
    bool drawing = false;
    double d_la = 0, d_lo = 0;

    // location
    bool follow = false;
    bool want_fix = false; // asked, waiting for the first fix to fly to it
    double reveal_la = 0, reveal_lo = 0;
    bool reveal = false;   // slide the map so this point shows above the sheet

    char search[96] = {};
    char existing[96] = {};
    int missing = 0;          // frames the selection has not existed
    bool focus_field = false; // a search screen just opened: the keyboard goes to its field
    maiz::LocationAccess last_access = maiz::LocationAccess::Unasked;
    std::string focus; // map_focus: a rune to show (or to place) when the map next draws

    /* THIS FRAME'S CONTEXT, set by the canvas (phone_map.cpp) for the
     * sub-screens and the sheet (phone_map_sheet.cpp). */
    struct Placed {
        const maiz::SceneNode* node;
        double la, lo;
        ImVec2 s;    // the anchor (a pin's tip)
        ImVec2 face; // where a finger aims
        MarkerLook look;
        bool fanned;
    };
    std::vector<const maiz::SceneNode*> views;
    const maiz::SceneNode* view = nullptr;
    std::string channel = "main", field = "geo";
    bool plan = false; // the canvas is a drawn plan: metres, no tiles, no GPS
    std::vector<HormigaApp::MapRule> rules;
    std::vector<Placed> placed;
    maiz::LocationFix fix;
    bool have_fix = false;

    /* A sheet's state may not be replaced between its begin and its end (it
     * owns a child window then): a change asked for from inside the sheet's
     * own content waits until the sheet has ended. */
    bool in_sheet = false;
    int pending_sheet = -1;
    void set_sheet(Sheet s) {
        if (in_sheet) {
            pending_sheet = (int)s;
            return;
        }
        if (s == sheet) return;
        sheet = s;
        bs = maiz::BottomSheetState{};
        switch (s) {
        case Marker: bs.detents = {0.30f, 0.62f, 0.92f}; break;
        case Place: bs.detents = {0.50f, 0.92f}; break;
        case Pick: bs.detents = {0.40f, 0.85f}; break;
        case Shape:
        case Ref: bs.detents = {0.34f, 0.7f}; break;
        default: bs.detents = {0.14f, 0.5f, 0.92f}; break;
        }
        bs.detent = 0;
    }
    void select(const std::string& name, const maiz::Scene& scene) {
        sel = name;
        pick.clear();
        set_sheet(Marker);
        if (const maiz::SceneNode* n = scene.find(name))
            if (hormiga::parse_geo(HormigaApp::view_geo(*n, channel), reveal_la, reveal_lo)) reveal = true;
    }
    void deselect() {
        sel.clear();
        pick.clear();
        set_sheet(None);
    }
    void open_place(double la, double lo, float acc) {
        place_la = la;
        place_lo = lo;
        place_acc = acc;
        sel.clear();
        set_sheet(Place);
        reveal_la = la;
        reveal_lo = lo;
        reveal = true;
    }
};

namespace hormiga::phone::mapx {

using MapUi = HormigaApp::PhoneUi::MapUi;
using hormiga::gis::SlippyView;

/* Distance in the canvas's own world: metres on Earth (haversine), metres on a
 * drawn plan (flat), so "12 m away" is true on both. */
inline double map_distance(const MapUi& m, double la1, double lo1, double la2, double lo2) {
    return m.plan ? std::hypot(la1 - la2, lo1 - lo2) : hormiga::geo_distance_m(la1, lo1, la2, lo2);
}

inline void start_zoom(MapUi& m, double dz, float px, float py) {
    m.anim = {};
    m.anim.on = true;
    m.anim.t0 = ImGui::GetTime();
    m.anim.z0 = m.v.zoom;
    m.anim.z1 = std::clamp(m.v.zoom + dz, m.v.min_zoom, m.v.max_zoom);
    m.anim.px = px;
    m.anim.py = py;
    m.coasting = false;
}

inline void fly_to(MapUi& m, double la, double lo, double zoom) {
    m.anim = {};
    m.anim.on = m.anim.fly = true;
    m.anim.t0 = ImGui::GetTime();
    m.anim.dur = 0.38;
    m.anim.z0 = m.v.zoom;
    m.anim.z1 = std::clamp(zoom, m.v.min_zoom, m.v.max_zoom);
    m.anim.fx0 = hormiga::gis::merc_x(m.v.lon, 0);
    m.anim.fy0 = hormiga::gis::merc_y(m.v.lat, 0);
    m.anim.fx1 = hormiga::gis::merc_x(lo, 0);
    m.anim.fy1 = hormiga::gis::merc_y(la, 0);
    m.coasting = false;
}

/* The zoom at which a box of mercator fractions fits the viewport, with margin. */
inline double fit_zoom(const MapUi& m, double fx0, double fy0, double fx1, double fy1) {
    const double dx = std::max(1e-9, fx1 - fx0), dy = std::max(1e-9, fy1 - fy0);
    const double zx = std::log2(m.v.w * 0.7 / (dx * m.v.tile_px));
    const double zy = std::log2(m.v.h * 0.55 / (dy * m.v.tile_px));
    return std::clamp(std::min(zx, zy), m.v.min_zoom, 17.0);
}

inline void step_motion(MapUi& m, float dt) {
    if (m.anim.on) {
        const float t = (float)((ImGui::GetTime() - m.anim.t0) / m.anim.dur);
        const double k = ease_out(t);
        if (m.anim.fly) {
            const double fx = m.anim.fx0 + (m.anim.fx1 - m.anim.fx0) * k;
            const double fy = m.anim.fy0 + (m.anim.fy1 - m.anim.fy0) * k;
            m.v.lon = SlippyView::wrap_lon(hormiga::gis::merc_lon(fx, 0));
            m.v.lat = std::clamp(hormiga::gis::merc_lat(fy, 0), -SlippyView::kMaxLat, SlippyView::kMaxLat);
            m.v.zoom = m.anim.z0 + (m.anim.z1 - m.anim.z0) * k;
        } else {
            const double z = m.anim.z0 + (m.anim.z1 - m.anim.z0) * k;
            m.v.zoom_about(std::pow(2.0, z - m.v.zoom), m.anim.px, m.anim.py);
        }
        if (t >= 1.0f) m.anim.on = false;
        m.cam_dirty = true;
        m.moved_at = ImGui::GetTime();
    }
    if (m.coasting) {
        m.v.pan(m.coast_x * dt, m.coast_y * dt);
        const float decay = std::exp(-4.0f * dt); // a map stops sooner than a list
        m.coast_x *= decay;
        m.coast_y *= decay;
        if (std::hypot(m.coast_x, m.coast_y) < 25.0f) m.coasting = false;
        m.cam_dirty = true;
        m.moved_at = ImGui::GetTime();
    }
}

/* A round distance that fits in `max_px` at this scale, for the scale bar. */
inline double nice_metres(double metres_per_px, float max_px) {
    static const double kSteps[] = {5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000};
    double best = kSteps[0];
    for (double s : kSteps)
        if (s / metres_per_px <= max_px) best = s;
    return best;
}

} // namespace hormiga::phone::mapx
