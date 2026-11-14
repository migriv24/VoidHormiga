/* ui/connections.hpp — the connections view's state (ui/connections.cpp).
 * Every field is view state: positions, what is open, the camera. Nothing
 * here is dispatched or stored (domain/connections.hpp has the model). */
#pragma once

#include "domain/connections.hpp"

#include "imgui.h"

#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace hormiga {

struct ConnectionsView {
    using Model = connections::Model;
    std::shared_ptr<const Model> model;               // what is drawn; null before the first build
    std::future<std::shared_ptr<const Model>> job;    // the next model, building off the UI thread
    std::uint64_t job_key = 0;
    double job_started = 0;

    std::vector<std::uint8_t> open;                   // per tree id
    std::vector<std::uint32_t> open_order;            // oldest first: the first closed for room
    std::vector<std::uint32_t> cut, body_of;          // the bodies drawn, and each node's body
    std::vector<maiz::overview::Link> shown, prox;    // links between bodies; proximity springs
    std::vector<maiz::overview::Body> bodies;         // per cut index
    std::vector<std::uint32_t> comp;                  // per cut index: its connected piece
    std::vector<float> radius;                        // per cut index: how large it is drawn
    std::vector<ImVec2> pos;                          // per tree id: where it was last
    std::vector<std::uint8_t> placed;                 // per tree id: pos is real
    std::unordered_map<std::string, ImVec2> kept;     // across a rebuild, by name
    std::vector<std::string> kept_open;               // across a rebuild, by label name

    int iters = 0, total = 0;
    float temp = 64.0f;
    int drag = -1;                                    // the body held
    ImVec2 cam{0, 0};
    float zoom = 1.0f;
    bool prox_on = false;                             // the springs below were built with proximity
    bool fit = true;                                  // frame everything once it has settled
};

} // namespace hormiga
