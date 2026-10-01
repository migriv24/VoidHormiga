/* antfarm/farm.hpp — Antfarm v2: the types, the node kinds, the graph, the check.
 *
 * okf/concepts/platform/antfarm/v2/. The v2 Antfarm is a separable layer (Q92):
 * this folder reaches Void Maiz and the vendored JSON and nothing of Hormiga's,
 * which tools/check_layering.py enforces. Everything the application knows (its
 * data mantle, its vault, its devices) reaches the evaluator through a Context
 * the host fills (farm_eval.hpp), so a second application could take this folder
 * the way Void Maiz took networking.
 *
 * A PROTOTYPE (2026-09-28). The author asked to build it before refining the
 * design, "I'll be able to give better feedback after messing around with a
 * prototype". What is here is V0 of the phases page and the read side of V1 to
 * V5: every kind can be placed, wired and checked, and faces are computed; the
 * writes that reach the world are the ones v1 already has, and the kinds with
 * no code behind them say `planned`. */
#pragma once

#include "voidmaiz/embed.hpp"
#include "voidmaiz/scene.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace farm {

inline constexpr const char* kMantle = "farm"; // v1 stays in "antfarm"
inline constexpr int kVersion = 2;

/* ── socket types (types.md §1) ───────────────────────────────────────────
 * The SHAPE says what kind of thing a wire carries: a payload flows (circle),
 * a field is a rule applied where it lands (diamond), a reference names a thing
 * that exists elsewhere (square), a value is a small number or word (ring). */
enum class Shape { Payload, Field, Reference, Value };

struct TypeInfo {
    const char* id;    // the port hint's `type`
    const char* label; // for people
    Shape shape;
    unsigned rgb;      // 0xRRGGBB
};
const std::vector<TypeInfo>& types();
const TypeInfo* type_info(std::string_view id);

/* ── strata (canvas.md §1): where a node's work happens ─────────────────── */
enum class Stratum { Chambers, Ground, Surface, Network };
const char* stratum_name(Stratum s);

struct PortDecl {
    std::string name;
    std::string type;       // "" = fits anything (the reroute)
    bool out = false;
    bool many = false;      // an INPUT that accepts several wires (outputs always fan out)
    bool optional = false;  // the node works without it
    bool writes = false;    // a run writes here (types.md §3)
};

struct FieldDecl {
    std::string key;
    std::string label;
    std::string editor; // Void Maiz widget binding ("combo:a,b"), "" = text
};

struct Kind {
    std::string id;     // the CLI's name: `farm add filter …`
    std::string glyph;  // "farm_filter" — prefixed so v2 never collides with a v1 or data glyph
    std::string label;
    std::string group;  // the palette heading
    Stratum stratum = Stratum::Chambers;
    bool planned = false; // the kind exists and no code answers it yet (A7)
    std::string doc;      // one sentence, for `farm kinds` and the palette tooltip
    unsigned rgb = 0x5a6070;
    int face_h = 60;
    std::vector<FieldDecl> fields;
    std::vector<PortDecl> ports;

    const PortDecl* port(std::string_view name) const;               // either direction (outputs first)
    const PortDecl* port(std::string_view name, bool out) const;     // a pass-through has `river` both ways
};

const std::vector<Kind>& kinds();
const Kind* kind_by_id(std::string_view id);
const Kind* kind_by_glyph(std::string_view glyph);
std::string glyph_json(const Kind& k); // the Void Core glyph declaration
void register_glyphs(maiz::Core& core); // every kind, beside v1's (never instead of)

/* ── the graph, read from a projection of the `farm` mantle ─────────────── */
struct Wire {
    std::string from, out, to, in;
    std::string relation; // as stored
    bool named = true;    // false: a v1-style numeric label (made by raw `link`)
};

struct Node {
    std::string name;
    const Kind* kind = nullptr;     // null: a rune whose glyph is not a v2 kind
    const maiz::SceneNode* sn = nullptr;
    std::string field(std::string_view key) const;
    std::string placement() const;  // "any" when unset
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Wire> wires;
    const Node* find(std::string_view name) const;
    std::vector<const Wire*> into(std::string_view node, std::string_view port) const;
    std::vector<const Wire*> out_of(std::string_view node, std::string_view port) const;
};

/* Read the scene. Wires keep the names they were stored under; a numeric
 * `i:j` label is translated through the glyph's port list and marked !named. */
Graph read(const maiz::Scene& farm_scene);

/* The canvas projects `out:in` labels as loose links because Void Maiz reads
 * only numeric labels as port wires. Rewrite them in the projected scene so they
 * draw between the right sockets (a view fix-up; the model is untouched). */
void resolve_named_wires(maiz::Scene& scene);

/* The door (A1): "" when the wire may be made, else the sentence that refuses
 * it. When the input takes one wire and has one, `replace` receives the wire it
 * would replace, so the caller can make it one rewire. */
std::string check_plug(const Graph& g, std::string_view from, std::string_view out,
                       std::string_view to, std::string_view in,
                       const Wire** replace = nullptr);

/* What went around the door: wires whose ports or types do not match. */
std::vector<std::string> audit(const Graph& g);

// helpers the other files share
std::string json_quote(std::string_view s);
std::string field_of(const maiz::SceneNode& n, std::string_view key); // unquoted, "" if unset

} // namespace farm
