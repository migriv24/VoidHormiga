// allomone_legacy.hpp — the ORIGINAL Hormiga-local Allomone interpreter.
//
// ── LEGACY, FROZEN 2026-08-10 ───────────────────────────────────────────────
// Allomone moved into Void Maiz (`voidmaiz_allomone`), where it became a base
// language with a composition engine underneath it: 7 merge laws, ⊤ as a
// first-class conflict, per-cell provenance, an analysis layer. Hormiga's
// domain vocabulary now lives in `hormiga_allomone.hpp` and the app derives
// through `maiz::allo_parse / allo_eval / merge` (MESSAGE_FOR_VOIDHORMIGA_
// maiz-allomone-ready-to-adopt-2026-08-10.md, folded into okf/log.md).
//
// This file stays for one reason: the twelve seeded `script` runes in existing
// databases are written in THIS dialect, and a stored script that stops
// parsing is data loss by another name. It is READ-ONLY history — no new
// features, no bug fixes beyond crashes. Nothing new should include it, and
// when the last legacy script is migrated it can be deleted outright.
//
// The two dialects are distinguished by GLYPH, not by a flag: a `script` rune
// is legacy (imperative, `for each rune do … rune:color(…)`), an `allo-script`
// rune is the Void Maiz language (declarative, `when has "x" then color "…"`).
// They derive independently and both feed the same merge, as two sources.
// ────────────────────────────────────────────────────────────────────────────
//
// Allomone is Hormiga's LOGIC language over Void Core's graph. It speaks Void
// Core's own vocabulary: a **rune** is an instance (a contact, an event), typed
// by its **glyph**; runes carry **tags** and fields and links. A script reads
// the runes of a mantle and produces DERIVE-ONLY styling (a color per rune, for
// now). Deriving is a pure projection — it changes nothing, so it is never
// logged; only MUTATION (a planned, gated capability) emits dispatcher commands
// that get logged/replayed (okf/concepts/allomone/execution.md).
//
// A rune is a FIRST-CLASS VALUE: you can bind it (`for each rune`), pass it to a
// function, filter a set of them (`runes where …`), and read `rune.tags` /
// `rune.glyph` / `rune has "tag"` / `rune is contact`. Set theory + quantifiers
// are the logical core (`tags_of`, `union`/`intersect`/`minus`, `overlaps`,
// `any`/`all`). Self-contained: the host passes plain `Rune`s and a color sink.
//
// Grammar (v3):
//   stmt   := 'local' IDENT '=' expr | IDENT '=' expr
//           | 'function' IDENT '(' params? ')' stmt* 'end' | 'return' expr?
//           | 'for' 'each' 'rune' 'do' stmt* 'end'      -- binds `rune`
//           | 'for' IDENT 'in' expr 'do' stmt* 'end'
//           | 'if' expr 'then' stmt* ('elseif' expr 'then' stmt*)* ('else' stmt*)? 'end'
//           | IDENT ':' IDENT '(' expr ')'              -- effect: rune:color("#hex")
//           | expr                                       -- a call statement
//   expr   := where
//   where  := or ('where' or)*                          -- filter a list; binds `rune`
//   or     := and ('or' and)* ;  and := cmp ('and' cmp)*
//   cmp    := match (('=='|'!='|'<'|'>'|'<='|'>=') match | 'has' match | 'is' glyphs)?
//   match  := add ('matching' add)* ;  add := mul (('+'|'-') mul)*
//   mul    := postfix (('*'|'/'|'%') postfix)*
//   postfix:= primary ('.' IDENT)*                       -- .tags .glyph .name
//   primary:= NUM | STR | 'true' | 'false' | 'not' primary | '-' primary
//           | '{' (expr (',' expr)*)? '}' | IDENT '(' args? ')' | '(' expr ')' | IDENT
//   glyphs := IDENT | '(' IDENT ('or' IDENT)* ')'
//   builtins: count/len, head, tail, push, contains, field, tags_of, union,
//             intersect, minus, overlaps, any(list[,fn]), all(list,fn),
//             abs, floor, min, max, lower, upper
//   special vars: `runes` = all runes in the mantle (a list of rune values)
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace allo {

struct FuncDef; // a user function (params + body); a Func value points to one

// ── a rune the interpreter reads (host fills these from the data mantle) ──────
struct Rune {
    std::string name, glyph;
    std::vector<std::string> tags;
    // the rune's own field values (role, email, date, bio…), decoded to plain
    // strings — read in a script as `rune.<field>` or `field(rune, "key")`.
    std::vector<std::pair<std::string, std::string>> fields;
    // edges touching this rune (relation, neighbour-name), BOTH directions —
    // Allomone traverses the graph structurally (neighbours, clusters), not by
    // naming specific runes. The host fills these from the mantle's wires.
    std::vector<std::pair<std::string, std::string>> links;
    // GLOBAL graph measures the HOST computes once per run (budgeted) and passes
    // in — read as `centrality(rune)` (normalised [0,1]) and `community(rune)`.
    double centrality = 0.0;
    int community = 0;
    const std::string& field(const std::string& key) const {
        static const std::string empty;
        for (const auto& f : fields) if (f.first == key) return f.second;
        return empty;
    }
};
using Thing = Rune; // legacy alias (older host code)

// ── values ──────────────────────────────────────────────────────────────────
struct Value {
    enum T { Nil, Num, Str, Bool, List, Func, RuneRef } t = Nil;
    double num = 0;
    std::string str;
    bool boolean = false;
    std::vector<Value> list;
    std::shared_ptr<FuncDef> fn; // Func — first-class, so functions are values
    const Rune* rn = nullptr;    // RuneRef — points into the host's rune vector
    static Value N(double d) { Value v; v.t = Num; v.num = d; return v; }
    static Value S(std::string s) { Value v; v.t = Str; v.str = std::move(s); return v; }
    static Value B(bool b) { Value v; v.t = Bool; v.boolean = b; return v; }
    static Value L(std::vector<Value> l) { Value v; v.t = List; v.list = std::move(l); return v; }
    static Value F(std::shared_ptr<FuncDef> f) { Value v; v.t = Func; v.fn = std::move(f); return v; }
    static Value R(const Rune* r) { Value v; v.t = RuneRef; v.rn = r; return v; }
    bool truthy() const {
        switch (t) {
        case Bool: return boolean;
        case Num: return num != 0;
        case Str: return !str.empty();
        case List: return !list.empty();
        case RuneRef: return rn != nullptr;
        case Func: return (bool)fn;
        default: return false;
        }
    }
    double as_num() const { return t == Num ? num : t == Bool ? (boolean ? 1 : 0) : 0; }
};

// ── AST ───────────────────────────────────────────────────────────────────
struct Expr;
using ExprP = std::shared_ptr<Expr>;
struct Expr {
    enum K { Lit, ListLit, Var, Call, Member, Is, Bin, Unary } k;
    Value val;                  // Lit
    std::vector<ExprP> items;   // ListLit items / Call args
    std::string name;           // Var / Call callee / Member field / Bin-or-Unary op
    ExprP a, b;                 // Bin(a,b) / Unary(a) / Member(a) / Is(a)
    std::vector<std::string> types; // Is: candidate glyph names
};

struct Stmt;
using StmtP = std::shared_ptr<Stmt>;
struct Stmt {
    enum K { Local, Assign, ForEach, ForIn, If, Effect, FuncDecl, Return, ExprStmt } k;
    std::string name;   // Local/Assign var / Effect subject var / FuncDecl name / ForIn var
    std::string name2;  // Effect verb (color)
    ExprP expr;         // Local/Assign value / If cond / Effect arg / Return / ForIn list / ExprStmt
    std::vector<std::string> params;                         // FuncDecl params
    std::vector<StmtP> body;                                 // For/Func body / If then
    std::vector<std::pair<ExprP, std::vector<StmtP>>> elifs; // If elseif chain
    std::vector<StmtP> elseBody;                             // If else
    bool hasElse = false;
};

struct FuncDef {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtP> body;
};

struct Script {
    std::vector<StmtP> top;
    std::vector<std::string> errors;
    bool ok() const { return errors.empty(); }
};

// ── lexer ───────────────────────────────────────────────────────────────────
struct Tok {
    enum K { Ident, Str, Num, Op, Eof } k = Eof;
    std::string s;
    int line = 1;
};

inline std::vector<Tok> lex(const std::string& src) {
    std::vector<Tok> out;
    size_t i = 0, n = src.size();
    int line = 1;
    auto push = [&](Tok::K k, std::string s) { out.push_back({k, std::move(s), line}); };
    while (i < n) {
        char c = src[i];
        if (c == '\n') { ++line; ++i; continue; }
        if (std::isspace((unsigned char)c)) { ++i; continue; }
        if (c == '-' && i + 1 < n && src[i + 1] == '-') { // comment to EOL
            while (i < n && src[i] != '\n') ++i;
            continue;
        }
        if (c == '"') {
            size_t j = i + 1;
            std::string s;
            while (j < n && src[j] != '"') { s += src[j]; ++j; }
            if (j < n) ++j;
            push(Tok::Str, s);
            i = j;
            continue;
        }
        if (std::isdigit((unsigned char)c)) {
            size_t j = i;
            while (j < n && (std::isdigit((unsigned char)src[j]) || src[j] == '.')) ++j;
            push(Tok::Num, src.substr(i, j - i));
            i = j;
            continue;
        }
        if (std::isalpha((unsigned char)c) || c == '_') {
            size_t j = i;
            while (j < n && (std::isalnum((unsigned char)src[j]) || src[j] == '_')) ++j;
            push(Tok::Ident, src.substr(i, j - i));
            i = j;
            continue;
        }
        static const char* two[] = {"==", "!=", "<=", ">=", nullptr};
        bool matched = false;
        for (int k = 0; two[k]; ++k)
            if (i + 1 < n && src[i] == two[k][0] && src[i + 1] == two[k][1]) {
                push(Tok::Op, two[k]);
                i += 2;
                matched = true;
                break;
            }
        if (matched) continue;
        push(Tok::Op, std::string(1, c));
        ++i;
    }
    push(Tok::Eof, "");
    return out;
}

// ── parser (recursive descent) ───────────────────────────────────────────────
struct Parser {
    std::vector<Tok> t;
    size_t p = 0;
    std::vector<std::string>& errs;
    Parser(std::vector<Tok> toks, std::vector<std::string>& e) : t(std::move(toks)), errs(e) {}

    const Tok& cur() const { return t[p]; }
    const Tok& peek() const { return t[p + 1 < t.size() ? p + 1 : t.size() - 1]; }
    bool is(const std::string& s) const { return cur().s == s; }
    bool isKw(const std::string& s) const { return cur().k == Tok::Ident && cur().s == s; }
    bool isOp(const std::string& s) const { return cur().k == Tok::Op && cur().s == s; }
    void adv() { if (p + 1 < t.size()) ++p; }
    void err(const std::string& m) {
        if (errs.size() < 20)
            errs.push_back("line " + std::to_string(cur().line) + ": " + m +
                           " (near '" + (cur().k == Tok::Eof ? "<end>" : cur().s) + "')");
    }
    bool eat(const std::string& s) {
        if (cur().s == s) { adv(); return true; }
        err("expected '" + s + "'");
        return false;
    }
    ExprP mk(Expr::K k) { auto e = std::make_shared<Expr>(); e->k = k; return e; }

    std::vector<StmtP> parseProgram() {
        std::vector<StmtP> out;
        while (cur().k != Tok::Eof && errs.size() < 20) {
            auto s = parseStmt();
            if (s) out.push_back(s); else adv();
        }
        return out;
    }
    std::vector<StmtP> parseBlock(std::initializer_list<const char*> terms) {
        std::vector<StmtP> out;
        auto atTerm = [&] {
            for (auto x : terms) if (cur().s == x) return true;
            return cur().k == Tok::Eof;
        };
        while (!atTerm() && errs.size() < 20) {
            auto s = parseStmt();
            if (s) out.push_back(s); else break;
        }
        return out;
    }

    StmtP parseStmt() {
        if (isKw("local")) {
            adv();
            auto s = std::make_shared<Stmt>(); s->k = Stmt::Local;
            if (cur().k != Tok::Ident) { err("expected a name after 'local'"); return nullptr; }
            s->name = cur().s; adv();
            if (!eat("=")) return nullptr;
            s->expr = parseExpr();
            return s;
        }
        if (isKw("for")) {
            adv();
            if (isKw("each")) { // for each rune do … end
                adv();
                eat("rune");
                eat("do");
                auto s = std::make_shared<Stmt>(); s->k = Stmt::ForEach;
                s->body = parseBlock({"end"}); eat("end");
                return s;
            }
            auto s = std::make_shared<Stmt>(); s->k = Stmt::ForIn;
            if (cur().k != Tok::Ident) { err("expected a loop variable after 'for'"); return nullptr; }
            s->name = cur().s; adv();
            eat("in");
            s->expr = parseExpr();
            eat("do"); s->body = parseBlock({"end"}); eat("end");
            return s;
        }
        if (isKw("if")) return parseIf();
        if (isKw("function")) {
            adv();
            auto s = std::make_shared<Stmt>(); s->k = Stmt::FuncDecl;
            if (cur().k != Tok::Ident) { err("expected a function name"); return nullptr; }
            s->name = cur().s; adv();
            eat("(");
            while (cur().k == Tok::Ident) {
                s->params.push_back(cur().s); adv();
                if (isOp(",")) adv(); else break;
            }
            eat(")"); s->body = parseBlock({"end"}); eat("end");
            return s;
        }
        if (isKw("return")) {
            adv();
            auto s = std::make_shared<Stmt>(); s->k = Stmt::Return;
            if (cur().k != Tok::Eof && !isKw("end") && !isKw("else") && !isKw("elseif"))
                s->expr = parseExpr();
            return s;
        }
        // an EFFECT: <var> : verb ( expr )   e.g.  rune:color("#hex")
        if (cur().k == Tok::Ident && peek().s == ":" && peek().k == Tok::Op) {
            auto s = std::make_shared<Stmt>(); s->k = Stmt::Effect;
            s->name = cur().s; adv();  // subject variable
            adv();                     // ':'
            s->name2 = cur().s; adv(); // verb
            eat("("); s->expr = parseExpr(); eat(")");
            return s;
        }
        // assignment to an existing variable
        if (cur().k == Tok::Ident && peek().s == "=" && peek().k == Tok::Op) {
            auto s = std::make_shared<Stmt>(); s->k = Stmt::Assign;
            s->name = cur().s; adv(); adv();
            s->expr = parseExpr();
            return s;
        }
        // a bare expression statement (a call)
        if (cur().k == Tok::Ident || isOp("(")) {
            auto s = std::make_shared<Stmt>(); s->k = Stmt::ExprStmt;
            s->expr = parseExpr();
            return s;
        }
        err("unexpected statement");
        return nullptr;
    }

    StmtP parseIf() {
        adv();
        auto s = std::make_shared<Stmt>(); s->k = Stmt::If;
        s->expr = parseExpr(); eat("then");
        s->body = parseBlock({"elseif", "else", "end"});
        while (isKw("elseif")) {
            adv(); ExprP c = parseExpr(); eat("then");
            auto blk = parseBlock({"elseif", "else", "end"});
            s->elifs.emplace_back(c, std::move(blk));
        }
        if (isKw("else")) { adv(); s->hasElse = true; s->elseBody = parseBlock({"end"}); }
        eat("end");
        return s;
    }

    // expression precedence: where < or < and < cmp < match < add < mul < postfix
    ExprP parseExpr() { return parseWhere(); }
    ExprP parseWhere() {
        ExprP a = parseOr();
        while (isKw("where")) { adv(); auto e = mk(Expr::Bin); e->name = "where"; e->a = a; e->b = parseOr(); a = e; }
        return a;
    }
    ExprP parseOr() {
        ExprP a = parseAnd();
        while (isKw("or")) { adv(); auto e = mk(Expr::Bin); e->name = "or"; e->a = a; e->b = parseAnd(); a = e; }
        return a;
    }
    ExprP parseAnd() {
        ExprP a = parseCmp();
        while (isKw("and")) { adv(); auto e = mk(Expr::Bin); e->name = "and"; e->a = a; e->b = parseCmp(); a = e; }
        return a;
    }
    ExprP parseCmp() {
        ExprP a = parseMatch();
        if (isKw("has")) { adv(); auto e = mk(Expr::Bin); e->name = "has"; e->a = a; e->b = parseMatch(); return e; }
        if (isKw("is")) { // a is contact  |  a is (contact or event)
            adv();
            auto e = mk(Expr::Is); e->a = a;
            if (isOp("(")) {
                adv();
                while (cur().k == Tok::Ident) {
                    e->types.push_back(cur().s); adv();
                    if (isKw("or")) adv(); else break;
                }
                eat(")");
            } else if (cur().k == Tok::Ident) { e->types.push_back(cur().s); adv(); }
            else err("expected a glyph name after 'is'");
            return e;
        }
        if (cur().k == Tok::Op &&
            (is("==") || is("!=") || is("<") || is(">") || is("<=") || is(">="))) {
            std::string op = cur().s; adv();
            auto e = mk(Expr::Bin); e->name = op; e->a = a; e->b = parseMatch();
            return e;
        }
        return a;
    }
    ExprP parseMatch() {
        ExprP a = parseAdd();
        while (isKw("matching")) { adv(); auto e = mk(Expr::Bin); e->name = "matching"; e->a = a; e->b = parseAdd(); a = e; }
        return a;
    }
    ExprP parseAdd() {
        ExprP a = parseMul();
        while (isOp("+") || isOp("-")) { std::string op = cur().s; adv(); auto e = mk(Expr::Bin); e->name = op; e->a = a; e->b = parseMul(); a = e; }
        return a;
    }
    ExprP parseMul() {
        ExprP a = parsePostfix();
        while (isOp("*") || isOp("/") || isOp("%")) { std::string op = cur().s; adv(); auto e = mk(Expr::Bin); e->name = op; e->a = a; e->b = parsePostfix(); a = e; }
        return a;
    }
    ExprP parsePostfix() {
        ExprP a = parsePrimary();
        while (isOp(".")) {
            adv();
            auto e = mk(Expr::Member); e->a = a; e->name = cur().s; adv();
            a = e;
        }
        return a;
    }
    ExprP parsePrimary() {
        const Tok& c = cur();
        if (c.k == Tok::Num) { adv(); auto e = mk(Expr::Lit); e->val = Value::N(std::atof(c.s.c_str())); return e; }
        if (c.k == Tok::Str) { adv(); auto e = mk(Expr::Lit); e->val = Value::S(c.s); return e; }
        if (isKw("true")) { adv(); auto e = mk(Expr::Lit); e->val = Value::B(true); return e; }
        if (isKw("false")) { adv(); auto e = mk(Expr::Lit); e->val = Value::B(false); return e; }
        if (isKw("not")) { adv(); auto e = mk(Expr::Unary); e->name = "not"; e->a = parsePostfix(); return e; }
        if (isOp("-")) { adv(); auto e = mk(Expr::Unary); e->name = "neg"; e->a = parsePostfix(); return e; }
        if (isOp("{")) {
            adv();
            auto e = mk(Expr::ListLit);
            if (!isOp("}"))
                do { e->items.push_back(parseExpr()); } while (isOp(",") && (adv(), true));
            eat("}");
            return e;
        }
        if (isOp("(")) { adv(); ExprP e = parseExpr(); eat(")"); return e; }
        if (c.k == Tok::Ident && peek().s == "(" && peek().k == Tok::Op) {
            std::string callee = c.s; adv(); adv();
            auto e = mk(Expr::Call); e->name = callee;
            if (!isOp(")"))
                do { e->items.push_back(parseExpr()); } while (isOp(",") && (adv(), true));
            eat(")");
            return e;
        }
        if (c.k == Tok::Ident) { adv(); auto e = mk(Expr::Var); e->name = c.s; return e; }
        err("unexpected expression");
        adv();
        return mk(Expr::Lit);
    }
};

inline Script parse(const std::string& src) {
    Script s;
    Parser P(lex(src), s.errors);
    s.top = P.parseProgram();
    return s;
}

// ── evaluator ────────────────────────────────────────────────────────────────
inline bool tag_matches(const std::string& tag, const std::string& pat) {
    if (tag == pat) return true;
    return !pat.empty() && pat.back() == ':' && tag.rfind(pat, 0) == 0;
}

using ColorSink = std::function<void(const std::string& rune, const std::string& hex)>;

struct Interp {
    const std::vector<Rune>* runes = nullptr;
    const ColorSink* sink = nullptr;
    std::vector<std::string>* errors = nullptr;
    std::map<std::string, Value> globals;
    std::map<std::string, const Rune*> by_name; // resolve a neighbour name → rune
    long fuel = 4000000;
    int depth = 0;
    struct Flow { bool ret = false; Value val; };

    bool halted() const { return fuel < 0; }
    void fail(const std::string& m) { if (errors && errors->size() < 20) errors->push_back(m); fuel = -1; }
    bool burn() {
        if (fuel <= 0) { if (fuel == 0) fail("step budget exceeded — a loop or recursion may not terminate"); return false; }
        --fuel; return true;
    }

    // string list of a value's tags (rune) — helper for set ops
    static std::vector<std::string> tags_of_rune(const Rune* r) { return r ? r->tags : std::vector<std::string>{}; }

    Value eval(const ExprP& e, std::map<std::string, Value>& sc) {
        if (!e || !burn()) return {};
        switch (e->k) {
        case Expr::Lit: return e->val;
        case Expr::Var: {
            auto it = sc.find(e->name);
            if (it != sc.end()) return it->second;
            if (e->name == "runes") { // the whole mantle as a list of rune values
                std::vector<Value> l;
                if (runes) for (const auto& r : *runes) l.push_back(Value::R(&r));
                return Value::L(std::move(l));
            }
            auto g = globals.find(e->name);
            return g == globals.end() ? Value{} : g->second;
        }
        case Expr::ListLit: {
            std::vector<Value> l;
            for (auto& x : e->items) l.push_back(eval(x, sc));
            return Value::L(std::move(l));
        }
        case Expr::Call: return evalCall(e, sc);
        case Expr::Member: {
            Value o = eval(e->a, sc);
            if (o.t == Value::RuneRef && o.rn) {
                if (e->name == "tags") { std::vector<Value> l; for (auto& t : o.rn->tags) l.push_back(Value::S(t)); return Value::L(std::move(l)); }
                if (e->name == "glyph") return Value::S(o.rn->glyph);
                if (e->name == "name") return Value::S(o.rn->name);
                // anything else is a FIELD: rune.email, rune.date, rune.bio ("" if absent)
                return Value::S(o.rn->field(e->name));
            }
            if (o.t == Value::List) { if (e->name == "count" || e->name == "len") return Value::N((double)o.list.size()); }
            return {};
        }
        case Expr::Is: {
            Value o = eval(e->a, sc);
            if (o.t != Value::RuneRef || !o.rn) return Value::B(false);
            for (auto& ty : e->types) if (ty == o.rn->glyph) return Value::B(true);
            return Value::B(false);
        }
        case Expr::Unary: {
            Value v = eval(e->a, sc);
            if (e->name == "neg") return Value::N(-v.as_num());
            return Value::B(!v.truthy());
        }
        case Expr::Bin: return evalBin(e, sc);
        }
        return {};
    }

    Value evalBin(const ExprP& e, std::map<std::string, Value>& sc) {
        const std::string& op = e->name;
        if (op == "and") { Value a = eval(e->a, sc); return a.truthy() ? eval(e->b, sc) : a; }
        if (op == "or")  { Value a = eval(e->a, sc); return a.truthy() ? a : eval(e->b, sc); }
        if (op == "where") { // filter a list: keep elems where pred holds (binds `rune`/`it`)
            Value a = eval(e->a, sc);
            // save/restore the bindings so a `where` nested inside `for each rune`
            // doesn't clobber the outer loop's `rune`
            bool hadR = sc.count("rune") != 0, hadI = sc.count("it") != 0;
            Value savedR = hadR ? sc["rune"] : Value{}, savedI = hadI ? sc["it"] : Value{};
            std::vector<Value> out;
            for (auto& elem : a.list) {
                sc["rune"] = elem; sc["it"] = elem;
                if (eval(e->b, sc).truthy()) out.push_back(elem);
                if (halted()) break;
            }
            if (hadR) sc["rune"] = savedR; else sc.erase("rune");
            if (hadI) sc["it"] = savedI; else sc.erase("it");
            return Value::L(std::move(out));
        }
        if (op == "has") { // rune has "tag" | rune has "ns:"
            Value a = eval(e->a, sc), b = eval(e->b, sc);
            if (a.t != Value::RuneRef || !a.rn) return Value::B(false);
            for (auto& t : a.rn->tags) if (tag_matches(t, b.str)) return Value::B(true);
            return Value::B(false);
        }
        if (op == "matching") {
            Value a = eval(e->a, sc), b = eval(e->b, sc);
            std::vector<Value> out;
            for (auto& t : a.list)
                for (auto& pat : b.list)
                    if (tag_matches(t.str, pat.str)) { out.push_back(t); break; }
            return Value::L(std::move(out));
        }
        Value a = eval(e->a, sc), b = eval(e->b, sc);
        if (op == "+") {
            if (a.t == Value::Str || b.t == Value::Str) return Value::S(a.str + b.str);
            if (a.t == Value::List) { auto l = a.list; for (auto& x : b.list) l.push_back(x); return Value::L(std::move(l)); }
            return Value::N(a.as_num() + b.as_num());
        }
        if (op == "-") return Value::N(a.as_num() - b.as_num());
        if (op == "*") return Value::N(a.as_num() * b.as_num());
        if (op == "/") return Value::N(b.as_num() != 0 ? a.as_num() / b.as_num() : 0);
        if (op == "%") { long yy = (long)b.as_num(); return Value::N(yy != 0 ? (double)((long)a.as_num() % yy) : 0); }
        // ordering compares STRINGS lexicographically (so ISO dates "2026-08-15"
        // order correctly) and everything else numerically
        bool strs = (a.t == Value::Str || b.t == Value::Str);
        if (op == "<")  return Value::B(strs ? a.str <  b.str : a.as_num() <  b.as_num());
        if (op == ">")  return Value::B(strs ? a.str >  b.str : a.as_num() >  b.as_num());
        if (op == "<=") return Value::B(strs ? a.str <= b.str : a.as_num() <= b.as_num());
        if (op == ">=") return Value::B(strs ? a.str >= b.str : a.as_num() >= b.as_num());
        if (op == "==") { if (a.t == Value::Str || b.t == Value::Str) return Value::B(a.str == b.str); return Value::B(a.as_num() == b.as_num()); }
        if (op == "!=") { if (a.t == Value::Str || b.t == Value::Str) return Value::B(a.str != b.str); return Value::B(a.as_num() != b.as_num()); }
        return {};
    }

    // equality across values (for set ops): strings by str, numbers by num, runes by name
    static bool same(const Value& a, const Value& b) {
        if (a.t == Value::RuneRef && b.t == Value::RuneRef) return a.rn == b.rn || (a.rn && b.rn && a.rn->name == b.rn->name);
        if (a.t == Value::Str || b.t == Value::Str) return a.str == b.str;
        return a.as_num() == b.as_num();
    }
    static bool member_of(const Value& x, const std::vector<Value>& l) {
        for (auto& e : l) if (same(x, e)) return true;
        return false;
    }

    Value evalCall(const ExprP& e, std::map<std::string, Value>& sc) {
        std::vector<Value> args;
        args.reserve(e->items.size());
        for (auto& x : e->items) args.push_back(eval(x, sc));
        Value bi; if (builtin(e->name, args, bi)) return bi;
        Value fv;
        auto it = sc.find(e->name);
        if (it != sc.end() && it->second.t == Value::Func) fv = it->second;
        else { auto g = globals.find(e->name); if (g != globals.end()) fv = g->second; }
        if (fv.t != Value::Func || !fv.fn) { fail("unknown function '" + e->name + "'"); return {}; }
        return callFunc(fv.fn, args);
    }

    Value callFunc(const std::shared_ptr<FuncDef>& fn, std::vector<Value>& args) {
        if (halted()) return {};
        if (depth >= 300) { fail("call depth exceeded — recursion too deep (limit 300)"); return {}; }
        std::map<std::string, Value> local;
        for (size_t i = 0; i < fn->params.size(); ++i) local[fn->params[i]] = i < args.size() ? args[i] : Value{};
        ++depth;
        Flow f = exec(fn->body, local);
        --depth;
        return f.ret ? f.val : Value{};
    }

    bool builtin(const std::string& name, std::vector<Value>& a, Value& out) {
        auto arg = [&](size_t i) -> Value { return i < a.size() ? a[i] : Value{}; };
        if (name == "count" || name == "len") {
            Value v = arg(0);
            out = Value::N(v.t == Value::List ? (double)v.list.size() : v.t == Value::Str ? (double)v.str.size() : 0);
            return true;
        }
        if (name == "head") { Value v = arg(0); out = (v.t == Value::List && !v.list.empty()) ? v.list.front() : Value{}; return true; }
        if (name == "tail") { Value v = arg(0); std::vector<Value> r; if (v.t == Value::List && v.list.size() > 1) r.assign(v.list.begin() + 1, v.list.end()); out = Value::L(std::move(r)); return true; }
        if (name == "push") { Value v = arg(0); auto l = v.t == Value::List ? v.list : std::vector<Value>{}; l.push_back(arg(1)); out = Value::L(std::move(l)); return true; }
        if (name == "contains") { Value v = arg(0), x = arg(1); out = Value::B(v.t == Value::List && member_of(x, v.list)); return true; }
        if (name == "field") { // field(rune, "key") — a rune's field value ("" if absent)
            Value v = arg(0), k = arg(1);
            out = Value::S(v.t == Value::RuneRef && v.rn ? v.rn->field(k.str) : "");
            return true;
        }
        // ── set theory over lists (of tags OR runes) ──────────────────────────
        if (name == "tags_of") { // union of all tags across a list of runes
            Value v = arg(0); std::vector<Value> out2;
            if (v.t == Value::List)
                for (auto& r : v.list)
                    if (r.t == Value::RuneRef && r.rn)
                        for (auto& t : r.rn->tags) { Value tv = Value::S(t); if (!member_of(tv, out2)) out2.push_back(tv); }
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "union") {
            Value x = arg(0), y = arg(1); std::vector<Value> out2;
            for (auto& e : x.list) if (!member_of(e, out2)) out2.push_back(e);
            for (auto& e : y.list) if (!member_of(e, out2)) out2.push_back(e);
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "intersect") {
            Value x = arg(0), y = arg(1); std::vector<Value> out2;
            for (auto& e : x.list) if (member_of(e, y.list) && !member_of(e, out2)) out2.push_back(e);
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "minus" || name == "except") {
            Value x = arg(0), y = arg(1); std::vector<Value> out2;
            for (auto& e : x.list) if (!member_of(e, y.list) && !member_of(e, out2)) out2.push_back(e);
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "overlaps") { // ∃ a common element between two lists
            Value x = arg(0), y = arg(1);
            for (auto& e : x.list) if (member_of(e, y.list)) { out = Value::B(true); return true; }
            out = Value::B(false); return true;
        }
        // ── quantifiers: any/all over a list, optionally with a function pred ──
        if (name == "any") {
            Value v = arg(0), f = arg(1);
            if (f.t == Value::Func) { for (auto& e : v.list) { std::vector<Value> as{e}; if (callFunc(f.fn, as).truthy()) { out = Value::B(true); return true; } } out = Value::B(false); return true; }
            out = Value::B(v.t == Value::List && !v.list.empty()); return true; // ∃ an element
        }
        if (name == "all") {
            Value v = arg(0), f = arg(1);
            if (f.t == Value::Func) { for (auto& e : v.list) { std::vector<Value> as{e}; if (!callFunc(f.fn, as).truthy()) { out = Value::B(false); return true; } } out = Value::B(true); return true; }
            out = Value::B(true); return true;
        }
        // ── graph traversal (structural, not by name) ─────────────────────────
        // neighbours(rune)      — every rune directly linked to it (any relation)
        // linked(rune, "rel")   — neighbours via a relation ("rel:" prefix ok)
        // cluster(rune)         — the connected component it belongs to (a set)
        // degree(rune)          — how many distinct neighbours (degree centrality)
        if (name == "neighbours" || name == "neighbors") {
            Value v = arg(0); std::vector<Value> out2; std::map<std::string, bool> seen;
            if (v.t == Value::RuneRef && v.rn)
                for (auto& lk : v.rn->links) {
                    if (seen[lk.second]) continue;
                    seen[lk.second] = true;
                    auto it = by_name.find(lk.second); if (it != by_name.end()) out2.push_back(Value::R(it->second));
                }
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "linked") {
            Value v = arg(0), rel = arg(1); std::vector<Value> out2; std::map<std::string, bool> seen;
            if (v.t == Value::RuneRef && v.rn)
                for (auto& lk : v.rn->links)
                    if (tag_matches(lk.first, rel.str) && !seen[lk.second]) {
                        seen[lk.second] = true;
                        auto it = by_name.find(lk.second); if (it != by_name.end()) out2.push_back(Value::R(it->second));
                    }
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "degree") {
            Value v = arg(0); std::map<std::string, bool> seen;
            if (v.t == Value::RuneRef && v.rn) for (auto& lk : v.rn->links) seen[lk.second] = true;
            out = Value::N((double)seen.size()); return true;
        }
        if (name == "cluster") { // connected component via BFS over links
            Value v = arg(0); std::vector<Value> comp;
            if (v.t == Value::RuneRef && v.rn) {
                std::map<std::string, bool> seen; std::vector<const Rune*> stack{v.rn};
                seen[v.rn->name] = true;
                while (!stack.empty() && burn()) {
                    const Rune* r = stack.back(); stack.pop_back();
                    comp.push_back(Value::R(r));
                    for (auto& lk : r->links) {
                        if (seen[lk.second]) continue;
                        seen[lk.second] = true;
                        auto it = by_name.find(lk.second); if (it != by_name.end()) stack.push_back(it->second);
                    }
                }
            }
            out = Value::L(std::move(comp)); return true;
        }
        // ── global graph MEASURES (host-computed) + BFS distance/reach ─────────
        // centrality(rune)  — eigenvector centrality, normalised [0,1]
        // community(rune)   — the detected community it belongs to (a set)
        // distance(a, b)    — hop distance between two runes (-1 if unreachable)
        // within(rune, k)   — every rune within k hops (incl. itself)
        if (name == "centrality") {
            Value v = arg(0);
            out = Value::N(v.t == Value::RuneRef && v.rn ? v.rn->centrality : 0);
            return true;
        }
        if (name == "community") {
            Value v = arg(0); std::vector<Value> out2;
            if (v.t == Value::RuneRef && v.rn && runes)
                for (const auto& o : *runes) if (o.community == v.rn->community) out2.push_back(Value::R(&o));
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "distance") {
            Value a = arg(0), b = arg(1);
            if (a.t != Value::RuneRef || b.t != Value::RuneRef || !a.rn || !b.rn) { out = Value::N(-1); return true; }
            if (a.rn == b.rn) { out = Value::N(0); return true; }
            std::map<std::string, int> dist; std::deque<const Rune*> q;
            dist[a.rn->name] = 0; q.push_back(a.rn); int ans = -1;
            while (!q.empty() && burn()) {
                const Rune* r = q.front(); q.pop_front(); int d = dist[r->name];
                for (auto& lk : r->links) {
                    if (dist.count(lk.second)) continue;
                    auto it = by_name.find(lk.second); if (it == by_name.end()) continue;
                    dist[lk.second] = d + 1;
                    if (lk.second == b.rn->name) { ans = d + 1; break; }
                    q.push_back(it->second);
                }
                if (ans >= 0) break;
            }
            out = Value::N(ans); return true;
        }
        if (name == "within") {
            Value v = arg(0); int k = (int)arg(1).as_num(); std::vector<Value> out2;
            if (v.t == Value::RuneRef && v.rn) {
                std::map<std::string, int> dist; std::deque<const Rune*> q;
                dist[v.rn->name] = 0; q.push_back(v.rn);
                while (!q.empty() && burn()) {
                    const Rune* r = q.front(); q.pop_front(); int d = dist[r->name];
                    out2.push_back(Value::R(r));
                    if (d >= k) continue;
                    for (auto& lk : r->links) {
                        if (dist.count(lk.second)) continue;
                        auto it = by_name.find(lk.second); if (it == by_name.end()) continue;
                        dist[lk.second] = d + 1; q.push_back(it->second);
                    }
                }
            }
            out = Value::L(std::move(out2)); return true;
        }
        if (name == "abs")   { out = Value::N(std::abs(arg(0).as_num())); return true; }
        if (name == "floor") { out = Value::N((double)(long)arg(0).as_num()); return true; }
        if (name == "min")   { out = Value::N(std::min(arg(0).as_num(), arg(1).as_num())); return true; }
        if (name == "max")   { out = Value::N(std::max(arg(0).as_num(), arg(1).as_num())); return true; }
        if (name == "lower" || name == "upper") {
            std::string s = arg(0).str;
            for (char& c : s) c = name == "lower" ? (char)std::tolower((unsigned char)c) : (char)std::toupper((unsigned char)c);
            out = Value::S(std::move(s)); return true;
        }
        return false;
    }

    Flow exec(const std::vector<StmtP>& body, std::map<std::string, Value>& sc) {
        for (const auto& s : body) {
            Flow f = execOne(s, sc);
            if (f.ret || halted()) return f;
        }
        return {};
    }

    Flow execOne(const StmtP& s, std::map<std::string, Value>& sc) {
        if (halted() || !burn()) return {};
        switch (s->k) {
        case Stmt::Local:
        case Stmt::Assign: sc[s->name] = eval(s->expr, sc); return {};
        case Stmt::FuncDecl: return {};
        case Stmt::ExprStmt: eval(s->expr, sc); return {};
        case Stmt::Return: { Flow f; f.ret = true; f.val = s->expr ? eval(s->expr, sc) : Value{}; return f; }
        case Stmt::ForEach: { // for each rune do … end  → binds `rune`
            if (!runes) return {};
            bool had = sc.count("rune") != 0; Value saved = had ? sc["rune"] : Value{};
            Flow f;
            for (const auto& r : *runes) {
                sc["rune"] = Value::R(&r);
                f = exec(s->body, sc);
                if (f.ret || halted()) break;
            }
            if (had) sc["rune"] = saved; else sc.erase("rune");
            return f;
        }
        case Stmt::ForIn: {
            Value lst = eval(s->expr, sc);
            bool had = sc.count(s->name) != 0; Value saved = had ? sc[s->name] : Value{};
            Flow f;
            for (auto& item : lst.list) { sc[s->name] = item; f = exec(s->body, sc); if (f.ret || halted()) break; }
            if (had) sc[s->name] = saved; else sc.erase(s->name);
            return f;
        }
        case Stmt::If: {
            if (eval(s->expr, sc).truthy()) return exec(s->body, sc);
            for (auto& ei : s->elifs) if (eval(ei.first, sc).truthy()) return exec(ei.second, sc);
            if (s->hasElse) return exec(s->elseBody, sc);
            return {};
        }
        case Stmt::Effect: { // <subject>:color("#hex")
            auto it = sc.find(s->name);
            if (it != sc.end() && it->second.t == Value::RuneRef && it->second.rn && s->name2 == "color") {
                std::string hex = eval(s->expr, sc).str;
                if (!hex.empty() && sink) (*sink)(it->second.rn->name, hex);
            }
            return {};
        }
        }
        return {};
    }
};

// Run a parsed script over the runes. Top-level FUNCTIONS are hoisted first,
// then the top level runs once. DERIVE-ONLY: the only effect is `rune:color`.
// `runtime_errors`, if given, receives fuel/arity/recursion failures.
inline void run(const Script& s, const std::vector<Rune>& runes,
                const ColorSink& sink, std::vector<std::string>* runtime_errors = nullptr) {
    Interp in;
    in.runes = &runes;
    in.sink = &sink;
    in.errors = runtime_errors;
    for (const auto& r : runes) in.by_name[r.name] = &r; // resolve links → runes
    for (const auto& st : s.top)
        if (st->k == Stmt::FuncDecl) {
            auto fn = std::make_shared<FuncDef>();
            fn->name = st->name; fn->params = st->params; fn->body = st->body;
            in.globals[st->name] = Value::F(fn);
        }
    for (const auto& st : s.top) {
        if (st->k == Stmt::FuncDecl) continue;
        Interp::Flow f = in.execOne(st, in.globals);
        if (f.ret || in.halted()) break;
    }
}

} // namespace allo
