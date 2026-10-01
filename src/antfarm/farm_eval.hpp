/* antfarm/farm_eval.hpp — the pure side, evaluated live (types.md §3).
 *
 * Geometry Nodes re-evaluates everything on every change; so does this, for the
 * Chambers stratum only: a filter's count, a document's reach, a folder's size.
 * NOTHING here writes. A write is a run, a command, logged (farm_verbs.hpp).
 *
 * The evaluator knows nothing about Hormiga. What a chamber holds, whether a
 * file exists, whether a key is in this device's vault: each is a question the
 * host answers through a Context. That is what keeps this folder separable. */
#pragma once

#include "antfarm/farm.hpp"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace farm {

/* One rune as the Antfarm sees it: which chamber, its projection, its size. */
struct Rune {
    std::string chamber; // data | assets | network | documents
    maiz::SceneNode node;
    std::shared_ptr<const maiz::Scene> scene; // the scene it came from (date queries borrow edges)
    long long bytes = 0;
};

/* A query is a rule, not a set (◆). Tag expressions go to the host's matcher,
 * which is the same `query_matches` every renderer calls, so the Antfarm and a
 * document can never disagree about what `date:future` selects. */
struct Query {
    enum class Op { All, Expr, FieldSet, FieldEmpty, FieldEq, And, Or, Not } op = Op::All;
    std::string a, b;          // Expr: the expression · Field*: key, value
    std::vector<Query> kids;   // And / Or / Not
    std::string text() const;  // what it compiled to, for faces and `farm eval`
};

struct Value {
    std::string type;          // the port's type
    std::vector<Rune> runes;   // mantle
    Query query;               // query
    double number = 0;         // value
    bool has_number = false;
    std::string text;          // rendition / value / reference: a one-line summary
    std::vector<std::string> refs; // river: its reservoir node names · key/domain/profile: the node
};

struct Folder {
    bool exists = false;
    long long files = 0, bytes = 0, free = -1; // free < 0: unknown
};

struct Context {
    std::function<std::vector<Rune>(const std::string& chamber)> chamber;
    std::function<bool(const std::string& expr, const Rune& r)> match; // default: Void Core's tag grammar
    std::function<bool(const std::string& path)> exists;               // relative to the database folder
    std::function<Folder(const std::string& path)> folder;
    std::function<std::vector<std::string>()> asset_files;             // names in the assets folder
    std::function<std::string(const std::string& entry)> key_state;    // present | missing | locked | no-vault
    std::function<bool(const std::string& name)> document_exists;      // a document by name, any kind
    std::function<std::string(const std::string& username)> presence; // "here" | "last seen …" | ""
    bool serves_local = true;  // can THIS device serve a local address (a phone cannot, today)
    std::string device = "desktop";
    std::string me;            // this profile's username
};

/* The readiness contract (A6, answered): one state per node, per device. */
struct Ready {
    std::string state = "ready"; // planned | unconfigured | needs | ready | failing | idle
    std::string why;             // the one thing missing, in words
};

struct Face {
    Ready ready;
    std::vector<std::string> lines;       // what the face prints, top to bottom
    std::map<std::string, int> strands;   // output port → strand count (types.md §5)
};

class Evaluator {
public:
    Evaluator(const Graph& g, const Context& ctx) : g_(g), ctx_(ctx) {}
    Value eval(const std::string& node, const std::string& port);
    Face face(const std::string& node);
    std::vector<Rune> chamber(const std::string& name); // cached

private:
    Value compute(const Node& n, const std::string& port);
    Value input(const Node& n, const std::string& port, const std::string& type);
    std::vector<Value> inputs(const Node& n, const std::string& port);
    bool test(const Query& q, const Rune& r);

    const Graph& g_;
    const Context& ctx_;
    std::map<std::string, Value> memo_;
    std::map<std::string, std::vector<Rune>> chambers_;
    std::vector<std::string> stack_; // cycle guard
};

std::string human_bytes(long long b);
int strands_for(long long count); // 1 + floor(log10 n), 1..5

} // namespace farm
