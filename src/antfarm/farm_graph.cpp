/* antfarm/farm_graph.cpp — reading the `farm` mantle, and the door.
 *
 * Wires are stored as Void Core relations labelled `<out>:<in>` with port NAMES
 * (types.md §4). v1 stored `3:1`, which made the glyph's port ORDER a contract
 * nothing pinned (B8). A numeric label still reads, translated through the
 * glyph's ports, and is marked so the audit can say it went around the door. */
#include "antfarm/farm.hpp"

#include <cctype>

namespace farm {

std::string Node::field(std::string_view key) const { return sn ? farm::field_of(*sn, key) : ""; }

std::string Node::placement() const {
    std::string p = field("placement");
    return p.empty() ? "any" : p;
}

const Node* Graph::find(std::string_view name) const {
    for (const auto& n : nodes)
        if (n.name == name) return &n;
    return nullptr;
}

std::vector<const Wire*> Graph::into(std::string_view node, std::string_view port) const {
    std::vector<const Wire*> r;
    for (const auto& w : wires)
        if (w.to == node && w.in == port) r.push_back(&w);
    return r;
}

std::vector<const Wire*> Graph::out_of(std::string_view node, std::string_view port) const {
    std::vector<const Wire*> r;
    for (const auto& w : wires)
        if (w.from == node && (port.empty() || w.out == port)) r.push_back(&w);
    return r;
}

namespace {

bool numeric(std::string_view s) {
    if (s.empty()) return false;
    for (char c : s)
        if (!std::isdigit((unsigned char)c)) return false;
    return true;
}

/* Port i (1-based, inputs and outputs together, declaration order), the
 * reduce contract Void Maiz projects by. */
std::string port_at(const maiz::SceneNode& n, int i) {
    for (const auto& p : n.inputs)
        if (p.index == i) return p.name;
    for (const auto& p : n.outputs)
        if (p.index == i) return p.name;
    return "";
}

} // namespace

Graph read(const maiz::Scene& s) {
    Graph g;
    for (const auto& n : s.nodes) g.nodes.push_back({n.name, kind_by_glyph(n.glyph), &n});
    for (const auto& w : s.wires) {
        const auto colon = w.relation.find(':');
        if (colon == std::string::npos) continue; // a loose semantic link, not a wire
        Wire x{w.from, w.relation.substr(0, colon), w.to, w.relation.substr(colon + 1), w.relation, true};
        if (numeric(x.out) && numeric(x.in)) {
            const maiz::SceneNode* a = s.find(w.from);
            const maiz::SceneNode* b = s.find(w.to);
            x.named = false;
            if (a) x.out = port_at(*a, std::atoi(x.out.c_str()));
            if (b) x.in = port_at(*b, std::atoi(x.in.c_str()));
        }
        g.wires.push_back(x);
    }
    return g;
}

void resolve_named_wires(maiz::Scene& s) {
    for (auto& w : s.wires) {
        if (w.kind != maiz::SceneWire::Kind::Loose) continue;
        const auto colon = w.relation.find(':');
        if (colon == std::string::npos) continue;
        const maiz::SceneNode* a = s.find(w.from);
        const maiz::SceneNode* b = s.find(w.to);
        if (!a || !b) continue;
        const std::string on = w.relation.substr(0, colon), in = w.relation.substr(colon + 1);
        int fi = -1, ti = -1;
        for (const auto& p : a->outputs)
            if (p.name == on) fi = p.index;
        for (const auto& p : b->inputs)
            if (p.name == in) ti = p.index;
        if (fi < 0 || ti < 0) continue; // a broken wire stays loose, dim, and the audit names it
        w.kind = maiz::SceneWire::Kind::Linguine;
        w.from_port = fi;
        w.to_port = ti;
    }
}

std::string check_plug(const Graph& g, std::string_view from, std::string_view out,
                       std::string_view to, std::string_view in, const Wire** replace) {
    if (replace) *replace = nullptr;
    const Node* a = g.find(from);
    const Node* b = g.find(to);
    if (!a) return "refused: there is no node called " + std::string(from) + ".";
    if (!b) return "refused: there is no node called " + std::string(to) + ".";
    if (!a->kind) return "refused: " + a->name + " is not a v2 node.";
    if (!b->kind) return "refused: " + b->name + " is not a v2 node.";
    if (a->name == b->name) return "refused: a node cannot feed itself.";
    const PortDecl* po = a->kind->port(out, true);
    const PortDecl* pi = b->kind->port(in, false);
    auto list = [](const Kind& k, bool outs) {
        std::string s;
        for (const auto& p : k.ports)
            if (p.out == outs) s += (s.empty() ? "" : ", ") + p.name;
        return s.empty() ? std::string("none") : s;
    };
    if (!po || !po->out)
        return "refused: " + a->name + " (" + a->kind->label + ") has no output called " +
               std::string(out) + ". Its outputs: " + list(*a->kind, true) + ".";
    if (!pi || pi->out)
        return "refused: " + b->name + " (" + b->kind->label + ") has no input called " +
               std::string(in) + ". Its inputs: " + list(*b->kind, false) + ".";
    if (!po->type.empty() && !pi->type.empty() && po->type != pi->type) {
        std::string hint;
        if (po->type == "mantle" && pi->type == "rendition")
            hint = " A mantle becomes a rendition through a document.";
        else if (po->type == "rendition" && pi->type == "mantle")
            hint = " A rendition is already past the privacy seam; it only goes to a domain.";
        return "refused: " + b->name + "." + std::string(in) + " takes a " + pi->type + ", and " +
               a->name + "." + std::string(out) + " is a " + po->type + "." + hint;
    }
    for (const Wire* w : g.into(b->name, in))
        if (w->from == a->name && w->out == out)
            return "refused: " + a->name + "." + std::string(out) + " is already plugged into " +
                   b->name + "." + std::string(in) + ".";
    if (!pi->many) {
        const auto existing = g.into(b->name, in);
        if (!existing.empty()) {
            if (!replace)
                return "refused: " + b->name + "." + std::string(in) + " accepts one wire and already has " +
                       existing.front()->from + "." + existing.front()->out + ". Unplug it first.";
            *replace = existing.front();
        }
    }
    return "";
}

std::vector<std::string> audit(const Graph& g) {
    std::vector<std::string> bad;
    for (const auto& w : g.wires) {
        const Node* a = g.find(w.from);
        const Node* b = g.find(w.to);
        const std::string label = w.from + " -[" + w.relation + "]-> " + w.to;
        if (!a || !b || !a->kind || !b->kind) {
            bad.push_back(label + ": joins something that is not a v2 node");
            continue;
        }
        const PortDecl* po = a->kind->port(w.out, true);
        const PortDecl* pi = b->kind->port(w.in, false);
        if (!po || !po->out || !pi || pi->out)
            bad.push_back(label + ": names a port that does not exist");
        else if (!po->type.empty() && !pi->type.empty() && po->type != pi->type)
            bad.push_back(label + ": a " + po->type + " wired into a " + pi->type);
        else if (!w.named)
            bad.push_back(label + ": numbered, not named (made by a raw `link`)");
    }
    for (const auto& n : g.nodes)
        if (n.kind)
            for (const auto& p : n.kind->ports)
                if (!p.out && !p.many && g.into(n.name, p.name).size() > 1)
                    bad.push_back(n.name + "." + p.name + ": takes one wire and has " +
                                  std::to_string(g.into(n.name, p.name).size()));
    return bad;
}

} // namespace farm
