/* ui/map_shapes.hpp — what one frame of the map canvas hands its shape tools
 * (ui/map_shapes.cpp, split out of ui/map.cpp on 2026-11-14). Built once per
 * frame in draw_map_section, where the projection, the pointer and the markers'
 * drawn places are known. */
#pragma once

#include "imgui.h"
#include "voidmaiz/scene.hpp"

#include <functional>

struct MapFrame {
    ImDrawList* dl = nullptr;
    ImVec2 mouse, ctr;                    // the pointer; the canvas's centre
    bool hovered = false;                 // the pointer is over the canvas
    bool left_press = false;              // the left button went down on it this frame
    bool poly_editing = false;            // a polygon's corners are in hand
    const maiz::SceneNode* marker_hit = nullptr;
    std::function<ImVec2(double, double)> to_screen;
    std::function<void(ImVec2, double&, double&)> to_geo;
    std::function<ImVec2(ImVec2)> snap;   // the ground's snap; the point itself when off
    std::function<bool(const maiz::SceneNode*, ImVec2&)> where_drawn; // a marker's place on screen
};
