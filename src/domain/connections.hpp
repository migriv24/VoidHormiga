/* domain/connections.hpp — the Data tab's connections view, without a window
 * (okf/concepts/sections/connections.md).
 *
 * The view draws what Core holds, which the store's plan keeps within the
 * machine's budget, and draws it in bounded work: a graph past a few hundred
 * runes is shown as groups (Void Maiz's voidmaiz/overview.hpp) that open one
 * at a time. A link whose far end the scene does not hold is a MOJÓN (the
 * frontier's stub, okf/concepts/platform/frontier.md): it is counted on the
 * rune and on every group above it, never drawn to nowhere.
 *
 * Scene in, a model out; the drawing is ui/connections.cpp. */
#pragma once

#include "voidmaiz/overview.hpp"
#include "voidmaiz/scene.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace hormiga::connections {

/* At most this many bodies with nothing open, children per opened group, and
 * bodies on screen at once. */
constexpr std::uint32_t kRoots = 400, kLeaf = 250, kMaxBodies = 1500;

/* Every rune with a link, its links inside the scene, and how many of its
 * links lead to runes the scene does not hold. A rune with no link at all is
 * not in the view (a cloud of isolates is not a graph). */
struct Graph {
    std::vector<std::string> names, glyphs;
    std::vector<std::uint32_t> degree, stubs; // per node
    std::vector<maiz::overview::Link> links;  // between nodes, each wire once
    std::unordered_map<std::string, std::uint32_t> index;
    std::uint64_t stub_total = 0;
    std::uint64_t scene_runes = 0; // every rune the scene holds, linked or not
};

Graph gather(const maiz::Scene& scene);

/* What the view draws from. Built off the UI thread: Leiden over every held
 * link is the one cost here that grows with the scene. */
struct Model {
    Graph graph;
    maiz::overview::Tree tree;
    std::vector<std::uint32_t> label; // per tree id: its best-connected node (itself for a node)
    std::vector<std::uint64_t> stubs; // per tree id: links leaving the scene from inside it
    std::uint64_t key = 0;
};

Model build(Graph graph, std::uint64_t key);

/* Changes whenever the scene's runes or links could have: the view rebuilds on it. */
std::uint64_t key_of(const maiz::Scene& scene);

} // namespace hormiga::connections
