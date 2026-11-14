/* connections_smoke.cpp — the connections view's model, without a window
 * (domain/connections.hpp, okf/concepts/sections/connections.md).
 *
 *  - a link to a rune the scene does not hold is a mojón: counted on the rune
 *    it leaves from and on every group above it, never a node;
 *  - a rune with no link is not in the view;
 *  - a scene the size of a loaded part (6×10^4 runes) gives a tree within the
 *    view's bounds, with every rune under one root;
 *  - the key changes when a link does. */
#include "domain/connections.hpp"

#include <chrono>
#include <iostream>
#include <random>

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            ++failures;                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                        \
    } while (0)

namespace cx = hormiga::connections;
namespace ov = maiz::overview;

static maiz::SceneNode node(const std::string& name) {
    maiz::SceneNode n;
    n.name = name;
    n.glyph = "contact";
    return n;
}

static maiz::SceneWire wire(const std::string& a, const std::string& b) {
    maiz::SceneWire w;
    w.from = a;
    w.to = b;
    return w;
}

int main() {
    // 1. small: mojones and isolates
    {
        maiz::Scene s;
        s.mantle = "data";
        for (const char* n : {"a", "b", "c", "alone"}) s.nodes.push_back(node(n));
        s.wires = {wire("a", "b"), wire("b", "c"), wire("c", "elsewhere-1"), wire("elsewhere-2", "c")};
        const cx::Graph g = cx::gather(s);
        CHECK(g.names.size() == 3); // "alone" has no link
        CHECK(g.index.count("alone") == 0);
        CHECK(g.index.count("elsewhere-1") == 0);
        CHECK(g.links.size() == 2);
        CHECK(g.stub_total == 2);
        CHECK(g.stubs[g.index.at("c")] == 2);
        CHECK(g.scene_runes == 4);
        const auto key = cx::key_of(s);
        const cx::Model m = cx::build(g, key);
        CHECK(m.tree.count() == 3); // small: no groups
        CHECK(m.stubs[g.index.at("c")] == 2);
        s.wires.push_back(wire("a", "c"));
        CHECK(cx::key_of(s) != key);
    }

    // 2. a loaded part's size: groups of ~300 with ties between, 1 link in 30 leaving
    {
        const int n = 60000;
        maiz::Scene s;
        s.mantle = "data";
        s.nodes.reserve(n);
        for (int i = 0; i < n; ++i) s.nodes.push_back(node("r" + std::to_string(i)));
        std::mt19937 rng(7);
        for (int i = 0; i < n; ++i)
            for (int k = 0; k < 3; ++k) {
                const int base = (i / 300) * 300;
                const int j = rng() % 40 == 0 ? (int)(rng() % n) : base + (int)(rng() % 300);
                if (j != i && j < n) s.wires.push_back(wire(s.nodes[i].name, s.nodes[(std::size_t)j].name));
            }
        for (int i = 0; i < n; i += 30) s.wires.push_back(wire(s.nodes[(std::size_t)i].name, "not-loaded"));
        const auto t0 = std::chrono::steady_clock::now();
        cx::Graph g = cx::gather(s);
        const auto t1 = std::chrono::steady_clock::now();
        const cx::Model m = cx::build(std::move(g), cx::key_of(s));
        const auto t2 = std::chrono::steady_clock::now();
        const auto& t = m.tree;
        CHECK(t.items == (std::uint32_t)n);
        CHECK(t.roots.size() <= cx::kRoots);
        for (std::uint32_t id = t.items; id < t.count(); ++id) CHECK(t.children[id].size() <= cx::kLeaf);
        std::uint64_t under_roots = 0, stubs_at_roots = 0;
        for (auto r : t.roots) under_roots += t.size[r], stubs_at_roots += m.stubs[r];
        CHECK(under_roots == (std::uint64_t)n);
        CHECK(stubs_at_roots == m.graph.stub_total);
        CHECK(m.graph.stub_total == (std::uint64_t)(n / 30));
        const auto closed = ov::cut(t, {});
        CHECK(closed.size() <= cx::kMaxBodies);
        std::cout << "60000 runes: gather "
                  << std::chrono::duration<double, std::milli>(t1 - t0).count() << " ms, build "
                  << std::chrono::duration<double, std::milli>(t2 - t1).count() << " ms, " << t.roots.size()
                  << " roots, " << t.count() - t.items << " groups\n";
    }

    if (failures) std::cerr << failures << " failure(s)\n";
    else std::cout << "connections_smoke: ok\n";
    return failures ? 1 : 0;
}
