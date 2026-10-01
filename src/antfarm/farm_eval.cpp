/* antfarm/farm_eval.cpp — values for outputs, faces for nodes.
 *
 * Every number a face shows comes from here, and `farm eval` / `farm show` print
 * the same numbers, which is the "no widget without a verb" rule made cheap: the
 * widget and the verb are two printouts of one function. */
#include "antfarm/farm_eval.hpp"

#include "voidmaiz/embed.hpp"   // Core::tag_match
#include "voidmaiz/project.hpp" // filter_bag

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <set>

namespace farm {

std::string Query::text() const {
    switch (op) {
    case Op::All: return "everything";
    case Op::Expr: return a;
    case Op::FieldSet: return a + " is set";
    case Op::FieldEmpty: return a + " is empty";
    case Op::FieldEq: return a + " = " + b;
    case Op::Not: return kids.empty() ? "nothing" : "NOT (" + kids[0].text() + ")";
    case Op::And:
    case Op::Or: {
        if (kids.empty()) return "everything";
        std::string s;
        for (const auto& k : kids)
            s += (s.empty() ? "" : (op == Op::And ? " AND " : " OR ")) + ("(" + k.text() + ")");
        return s;
    }
    }
    return "";
}

std::string human_bytes(long long b) {
    char buf[32];
    if (b < 1024) std::snprintf(buf, sizeof buf, "%lld B", b);
    else if (b < 1024LL * 1024) std::snprintf(buf, sizeof buf, "%.1f KB", b / 1024.0);
    else if (b < 1024LL * 1024 * 1024) std::snprintf(buf, sizeof buf, "%.1f MB", b / 1048576.0);
    else std::snprintf(buf, sizeof buf, "%.1f GB", b / 1073741824.0);
    return buf;
}

int strands_for(long long n) {
    if (n <= 0) return 1;
    return std::clamp(1 + (int)std::floor(std::log10((double)n)), 1, 5);
}

namespace {

std::string with_commas(long long n) {
    std::string s = std::to_string(n);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

long long days_until(const std::string& iso) {
    int y = 0, m = 0, d = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return 1LL << 40;
    std::tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = 12;
    const std::time_t then = std::mktime(&t);
    return (long long)std::floor(std::difftime(then, std::time(nullptr)) / 86400.0);
}

const char* kChambers[] = {"data", "assets", "network", "documents"};

} // namespace

std::vector<Rune> Evaluator::chamber(const std::string& name) {
    auto it = chambers_.find(name);
    if (it != chambers_.end()) return it->second;
    std::vector<Rune> r = ctx_.chamber ? ctx_.chamber(name) : std::vector<Rune>{};
    chambers_[name] = r;
    return r;
}

bool Evaluator::test(const Query& q, const Rune& r) {
    switch (q.op) {
    case Query::Op::All: return true;
    case Query::Op::Expr:
        if (q.a.empty()) return true;
        if (ctx_.match) return ctx_.match(q.a, r);
        try {
            return maiz::Core::tag_match(q.a, maiz::filter_bag(r.node));
        } catch (...) {
            return true; // malformed: degrade to everything, as the renderers do
        }
    case Query::Op::FieldSet: return !farm::field_of(r.node, q.a).empty();
    case Query::Op::FieldEmpty: return farm::field_of(r.node, q.a).empty();
    case Query::Op::FieldEq: return farm::field_of(r.node, q.a) == q.b;
    case Query::Op::Not: return q.kids.empty() || !test(q.kids[0], r);
    case Query::Op::And:
        for (const auto& k : q.kids)
            if (!test(k, r)) return false;
        return true;
    case Query::Op::Or:
        if (q.kids.empty()) return true;
        for (const auto& k : q.kids)
            if (test(k, r)) return true;
        return false;
    }
    return true;
}

std::vector<Value> Evaluator::inputs(const Node& n, const std::string& port) {
    std::vector<Value> r;
    for (const Wire* w : g_.into(n.name, port)) r.push_back(eval(w->from, w->out));
    return r;
}

Value Evaluator::input(const Node& n, const std::string& port, const std::string& type) {
    auto v = inputs(n, port);
    if (v.empty()) {
        Value e;
        e.type = type;
        return e;
    }
    return v.front();
}

Value Evaluator::eval(const std::string& node, const std::string& port) {
    const std::string key = node + "." + port;
    if (auto it = memo_.find(key); it != memo_.end()) return it->second;
    if (std::find(stack_.begin(), stack_.end(), key) != stack_.end()) return Value{}; // a cycle reads as empty
    const Node* n = g_.find(node);
    if (!n || !n->kind) return Value{};
    stack_.push_back(key);
    Value v = compute(*n, port);
    stack_.pop_back();
    if (const PortDecl* p = n->kind->port(port); p && v.type.empty()) v.type = p->type;
    memo_[key] = v;
    return v;
}

Value Evaluator::compute(const Node& n, const std::string& port) {
    const std::string& k = n.kind->id;
    Value v;
    if (const PortDecl* p = n.kind->port(port)) v.type = p->type;
    if (n.kind->planned) return v;

    if (k == "miga") {
        for (const char* c : kChambers)
            for (auto& r : chamber(c)) v.runes.push_back(r);
    } else if (k == "separate") {
        for (auto& r : input(n, "mantle", "mantle").runes)
            if (r.chamber == port) v.runes.push_back(r);
    } else if (k == "filter") {
        const Value m = input(n, "mantle", "mantle");
        const Value q = input(n, "where", "query");
        for (const auto& r : m.runes)
            if (test(q.query, r) == (port == "kept")) v.runes.push_back(r);
        v.query = q.query;
    } else if (k == "join") {
        for (auto& in : inputs(n, "mantles"))
            for (auto& r : in.runes) v.runes.push_back(r);
    } else if (k == "count" || k == "measure") {
        const Value m = input(n, "mantle", "mantle");
        long long bytes = 0;
        for (const auto& r : m.runes) bytes += r.bytes;
        v.has_number = true;
        v.number = port == "bytes" ? (double)bytes : (double)m.runes.size();
        v.text = port == "bytes" ? human_bytes(bytes) : with_commas((long long)m.runes.size()) + " runes";
    } else if (k == "tag") {
        v.query = Query{Query::Op::Expr, n.field("expr"), "", {}};
        if (v.query.a.empty()) v.query.op = Query::Op::All;
    } else if (k == "glyph") {
        v.query.op = Query::Op::Or;
        std::string list = n.field("glyphs"), cur;
        list += ",";
        for (char c : list) {
            if (c == ',') {
                while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
                while (!cur.empty() && cur.back() == ' ') cur.pop_back();
                if (!cur.empty()) v.query.kids.push_back(Query{Query::Op::Expr, "glyph:" + cur, "", {}});
                cur.clear();
            } else cur += c;
        }
        if (v.query.kids.empty()) v.query.op = Query::Op::All;
    } else if (k == "date") {
        const std::string w = n.field("when");
        v.query = w.empty() ? Query{} : Query{Query::Op::Expr, "date:" + w, "", {}};
    } else if (k == "field") {
        const std::string op = n.field("op");
        v.query.a = n.field("key");
        v.query.b = n.field("value");
        v.query.op = v.query.a.empty()   ? Query::Op::All
                     : op == "is empty" ? Query::Op::FieldEmpty
                     : op == "equals"   ? Query::Op::FieldEq
                                        : Query::Op::FieldSet;
    } else if (k == "and" || k == "or") {
        v.query.op = k == "and" ? Query::Op::And : Query::Op::Or;
        for (auto& in : inputs(n, "queries")) v.query.kids.push_back(in.query);
    } else if (k == "not") {
        v.query.op = Query::Op::Not;
        v.query.kids.push_back(input(n, "query", "query").query);
    } else if (k == "website" || k == "newsletter" || k == "calendar" || k == "map") {
        v.runes = input(n, "data", "mantle").runes;
        const std::string doc = n.field("document");
        v.text = k + " '" + (doc.empty() ? n.name : doc) + "' (" + port + "), " +
                 with_commas((long long)v.runes.size()) + " runes";
    } else if (k == "river" || k == "dam" || k == "distributary") {
        for (auto& in : inputs(n, k == "river" ? "reservoirs" : "river"))
            for (auto& r : in.refs) v.refs.push_back(r);
    } else if (k == "gauge") {
        const Value river = input(n, "river", "river");
        long long used = 0, free = -1;
        for (const auto& r : river.refs) {
            const Node* res = g_.find(r);
            if (!res || !res->kind || res->kind->id != "folder" || !ctx_.folder) continue;
            const Folder f = ctx_.folder(res->field("path"));
            used += f.bytes;
            if (f.free >= 0) free = std::max(free, 0LL) + f.free;
        }
        v.has_number = true;
        v.number = port == "used" ? (double)used : (double)free;
        v.text = port == "used" ? human_bytes(used) : free < 0 ? "unknown" : human_bytes(free);
    } else if (k == "expiry") {
        const Value key = input(n, "key", "key");
        const Node* kn = key.refs.empty() ? nullptr : g_.find(key.refs[0]);
        const std::string exp = kn ? kn->field("expires") : "";
        if (!exp.empty()) {
            v.has_number = true;
            v.number = (double)days_until(exp);
            v.text = std::to_string((long long)v.number) + " days";
        } else v.text = "no expiry recorded";
    } else if (k == "profile") {
        const std::string u = n.field("username");
        v.refs.push_back(u.empty() ? ctx_.me : u);
    } else if (k == "reroute") {
        return input(n, "in", "");
    } else if (k == "import-csv") {
        v.text = n.field("file");
    } else {
        v.refs.push_back(n.name); // a reference: the node itself (folder, bucket, key, domain…)
    }
    return v;
}

} // namespace farm
