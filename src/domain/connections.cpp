/* domain/connections.cpp — the connections view's model (domain/connections.hpp). */
#include "domain/connections.hpp"

#include <string_view>
#include <utility>

namespace hormiga::connections {

namespace ov = maiz::overview;

Graph gather(const maiz::Scene& scene) {
    const std::uint32_t n = (std::uint32_t)scene.nodes.size();
    std::unordered_map<std::string_view, std::uint32_t> at;
    at.reserve(n * 2);
    for (std::uint32_t i = 0; i < n; ++i) at.emplace(scene.nodes[i].name, i);
    std::vector<std::uint32_t> degree(n, 0), stubs(n, 0);
    std::vector<std::pair<std::uint32_t, std::uint32_t>> raw;
    raw.reserve(scene.wires.size());
    std::uint64_t stub_total = 0;
    for (const auto& w : scene.wires) {
        const auto a = at.find(w.from), b = at.find(w.to);
        if (a != at.end() && b != at.end()) {
            if (a->second == b->second) continue;
            raw.push_back({a->second, b->second});
            ++degree[a->second];
            ++degree[b->second];
        } else if (a != at.end() || b != at.end()) { // one end held: a mojón
            ++stubs[a != at.end() ? a->second : b->second];
            ++stub_total;
        }
    }
    Graph g;
    g.scene_runes = n;
    g.stub_total = stub_total;
    std::vector<std::uint32_t> local(n, ov::Tree::kNone);
    for (std::uint32_t i = 0; i < n; ++i) {
        if (!degree[i] && !stubs[i]) continue;
        local[i] = (std::uint32_t)g.names.size();
        g.index.emplace(scene.nodes[i].name, local[i]);
        g.names.push_back(scene.nodes[i].name);
        g.glyphs.push_back(scene.nodes[i].glyph);
        g.degree.push_back(degree[i]);
        g.stubs.push_back(stubs[i]);
    }
    g.links.reserve(raw.size());
    for (const auto& [a, b] : raw) g.links.push_back({local[a], local[b], 1.0f});
    return g;
}

Model build(Graph graph, std::uint64_t key) {
    Model m;
    m.key = key;
    ov::TreeOptions o;
    o.roots = kRoots;
    o.leaf_size = kLeaf;
    m.tree = ov::build_tree((std::uint32_t)graph.names.size(), graph.links, o);
    const std::uint32_t count = m.tree.count();
    m.label.assign(count, ov::Tree::kNone);
    m.stubs.assign(count, 0);
    for (std::uint32_t v = 0; v < m.tree.items; ++v) {
        for (std::uint32_t x = v; x != ov::Tree::kNone; x = m.tree.parent[x]) {
            m.stubs[x] += graph.stubs[v];
            const std::uint32_t best = m.label[x];
            if (best == ov::Tree::kNone || graph.degree[v] > graph.degree[best]) m.label[x] = v;
        }
    }
    m.graph = std::move(graph);
    return m;
}

std::uint64_t key_of(const maiz::Scene& scene) {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&h](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            h ^= (v >> (i * 8)) & 0xff;
            h *= 1099511628211ull;
        }
    };
    for (char c : scene.mantle) mix((unsigned char)c);
    mix(scene.source);
    mix(scene.nodes.size());
    mix(scene.wires.size());
    if (scene.source == 0) // no digest: the ends of every wire stand in for it
        for (const auto& w : scene.wires)
            for (char c : w.from + "\x1f" + w.to) mix((unsigned char)c);
    return h;
}

} // namespace hormiga::connections
