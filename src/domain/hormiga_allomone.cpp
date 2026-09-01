/* hormiga_allomone.cpp — see hormiga_allomone.hpp for what this library is and
 * why the evaluator is deliberately absent from it. */
#include "domain/hormiga_allomone.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <unordered_map>

namespace hormiga {
namespace allomone {
namespace {

/* A field off a projected node, decoded. Every host writes this; upstream's
 * backlog item T2 is to ship it once. Until then it is four lines, and this is
 * the one place a host must know that a field arrives JSON-encoded. */
std::string field_of(const maiz::SceneNode& n, std::string_view key) {
    for (const auto& f : n.fields) {
        if (f.key != key) continue;
        const std::string& j = f.value_json;
        if (j == "null") return {};
        if (j.size() < 2 || j.front() != '"') return j;
        std::string out;
        for (size_t i = 1; i + 1 < j.size(); ++i) {
            if (j[i] == '\\' && i + 2 < j.size()) {
                char c = j[++i];
                out += (c == 'n') ? '\n' : (c == 't') ? '\t' : c;
            } else out += j[i];
        }
        return out;
    }
    return {};
}

/* Days since the civil epoch (Howard Hinnant's days_from_civil). Dates are
 * compared as day counts rather than by string subtraction so that "30 days"
 * means thirty days across a month boundary. */
long civil_days(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + (long)doe - 719468L;
}

/* Parse a leading ISO date. Returns false for empty, malformed or recurring
 * (`days`-only) values — a subject with no date simply never matches a
 * date predicate, which is silence rather than an error. */
bool iso_days(std::string_view s, long& out) {
    int y = 0, m = 0, d = 0;
    if (s.size() < 10) return false;
    if (std::sscanf(std::string(s.substr(0, 10)).c_str(), "%d-%d-%d", &y, &m, &d) != 3)
        return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
    out = civil_days(y, (unsigned)m, (unsigned)d);
    return true;
}

/* The date a subject is "about": events and days carry `date`, jobs carry a
 * `deadline`, incidents carry `date`. One accessor so `overdue`, `upcoming`
 * and `before`/`after` all agree about which field they mean. */
std::string subject_date(const Frame::Row& r) {
    static const char* keys[] = {"date", "deadline"};
    for (const char* k : keys)
        for (const auto& f : r.fields)
            if (f.first == k && !f.second.empty()) return f.second;
    return {};
}

bool parse_geo(const std::string& s, double& lat, double& lon) {
    return std::sscanf(s.c_str(), "%lf,%lf", &lat, &lon) == 2;
}

/* Great-circle distance in kilometres (haversine, mean Earth radius).
 *
 * Territory's concept is explicit that the map source is swappable and not
 * assumed to be Earth, so this is the EARTH reading of `near` and nothing more.
 * A non-Earth map wants its own predicate rather than a rescaled constant. */
double km_between(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0088, rad = 3.14159265358979323846 / 180.0;
    double dlat = (lat2 - lat1) * rad, dlon = (lon2 - lon1) * rad;
    double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
               std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::sin(dlon / 2) *
                   std::sin(dlon / 2);
    return 2 * R * std::asin(std::min(1.0, std::sqrt(a)));
}

bool icontains(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > hay.size()) return false;
    auto low = [](char c) { return (char)std::tolower((unsigned char)c); };
    for (size_t i = 0; i + needle.size() <= hay.size(); ++i) {
        size_t j = 0;
        while (j < needle.size() && low(hay[i + j]) == low(needle[j])) ++j;
        if (j == needle.size()) return true;
    }
    return false;
}

/* Split "key=value" / "key:value" once. Unsplit text is the whole key. */
void split_pair(std::string_view arg, std::string& key, std::string& val,
                bool& has_val) {
    size_t p = arg.find_first_of("=");
    has_val = p != std::string_view::npos;
    key = std::string(has_val ? arg.substr(0, p) : arg);
    val = has_val ? std::string(arg.substr(p + 1)) : std::string();
}

int to_int(std::string_view s) { return std::atoi(std::string(s).c_str()); }
double to_dbl(std::string_view s) { return std::atof(std::string(s).c_str()); }

} // namespace

// ── glyphs ──────────────────────────────────────────────────────────────────

void register_glyphs(maiz::Core& core) {
    core.register_glyph(
        R"({"glyph":"allo-script","label":"Allomone Script",)"
        R"("fields":["title","source","enabled"],)"
        R"("hints":{"color":"#7a5cc0","face":{"w":200,"h":48},"category":"Allomone",)"
        R"("editors":{"source":"hidden","enabled":"hidden"},)"
        R"__("labels":{"title":"Name"}}})__");
    // The library's own — its commands name it, so it ships its shape. Do not
    // hand-write a substitute: a wrong name fails loudly, a missing field fails
    // silently (see the header, and annotate.hpp's note on the two bugs of
    // exactly that shape that have already shipped upstream).
    core.register_glyph(std::string(maiz::resolution_glyph()));
}

// ── 1. subjects ─────────────────────────────────────────────────────────────

bool is_subject_glyph(const std::string& glyph) {
    // Scaffolding, not data: the map/calendar views, both script dialects, the
    // legacy rule + block runes, and resolutions. A rule about "every contact"
    // should not have to say "and not a map".
    static const std::set<std::string> skip = {
        "map",   "calview", "mapshape", "script", "allo-script",
        "rule",  "allomone-resolution"};
    if (skip.count(glyph)) return false;
    return glyph.rfind("allo_", 0) != 0; // the block-editor AST runes
}

std::vector<maiz::Subject> subjects_from(const maiz::Scene& data) {
    std::vector<maiz::Subject> out;
    out.reserve(data.nodes.size());
    for (const maiz::SceneNode& n : data.nodes) {
        if (!is_subject_glyph(n.glyph)) continue;
        // Designated initializers, always: positional aggregate init silently
        // rebinds when upstream adds a field in the middle, and it already did
        // once (`mantle`, 2026-08-06), after which a tag list matched
        // std::string's two-ITERATOR constructor and segfaulted.
        out.push_back(maiz::Subject{.id = n.name,
                                    .kind = n.glyph,
                                    .name = n.name,
                                    .mantle = data.mantle,
                                    .tags = n.tags});
    }
    return out;
}

// ── 2. properties and their merge laws ──────────────────────────────────────

const std::vector<Property>& vocabulary() {
    // Declared where COMBINING is the meaning; everything else is left Unique
    // on purpose, because surfacing a disagreement is the conservative answer
    // and declaring only ever adds.
    static const std::vector<Property> v = {
        {"color", maiz::Lattice::Unique,
         "the card's colour — one right answer, so two sources disagreeing is a "
         "real question rather than a race"},
        {"icon", maiz::Lattice::Unique, "a glyph shown on the card"},
        {"label", maiz::Lattice::Unique,
         "an override for the card's caption. Deriving it does NOT write the "
         "rune's field — disabling the script restores it with nothing to undo"},
        {"weight", maiz::Lattice::Sum,
         "emphasis; contributions accumulate. Note Sum counts each DISTINCT "
         "value once — two sources both saying 2 total 2, not 4"},
        {"priority", maiz::Lattice::Max, "how urgent; the loudest opinion wins"},
        {"badge", maiz::Lattice::All,
         "short markers on the card; every source's opinion is collected"},
        {"note", maiz::Lattice::All,
         "why this card looks like this; collected, never overridden"},
        {"hide", maiz::Lattice::Unique,
         "keep this rune OUT of a surface. Only `web-hide \"1\"` acts today: it "
         "excludes the rune from every query-backed block in the newsletter, the "
         "website and the preview. Unique, so two scripts disagreeing about "
         "whether something may be published surfaces rather than suppressing it"},
    };
    return v;
}

const std::vector<Domain>& domains() {
    static const std::vector<Domain> d = {
        {"card", "the Data list and card view — the interior, default surface"},
        {"map", "Territory: marker colour and icon, drawn-geometry colour"},
        {"cal", "the Calendar: day and event chips"},
        {"web", "the OUTPUT domain — newsletter and website. The privacy seam "
                "binds here; see internal_fields()"},
    };
    return d;
}

const std::vector<std::string>& internal_fields() {
    // `notes` is the contact field seed.hpp registers as internal-notes-class.
    // A single-entry list looks like overkill until the second one arrives; the
    // point is that there is ONE place to add it and every read consults it.
    static const std::vector<std::string> f = {"notes"};
    return f;
}

bool is_internal_field(std::string_view key) {
    for (const std::string& k : internal_fields())
        if (k == key) return true;
    return false;
}

maiz::MergeOptions merge_options() {
    maiz::MergeOptions o;
    for (const Property& p : vocabulary()) {
        o.lattices.push_back({p.name, p.law, {}});
        // Every property is declared BOTH plain and domain-qualified. A law
        // that applied to `weight` and not `map-weight` would be a trap of
        // exactly the silent kind: the qualified spelling would quietly fall
        // back to Unique and start surfacing conflicts where the plain one
        // accumulated.
        for (const Domain& d : domains())
            o.lattices.push_back({std::string(d.prefix) + "-" + p.name, p.law, {}});
    }
    // Surfacing everywhere. The handover shipped ConflictPolicy::Recency on our
    // own ask, and we are deliberately NOT taking it yet: recency is only worth
    // having once a person has seen a conflict and said "I want the newer one",
    // and nobody here has seen one yet.
    o.default_policy = maiz::ConflictPolicy::Surface;
    return o;
}

std::string cell_for(const maiz::Merged& merged, const std::string& subject,
                     std::string_view domain, std::string_view property) {
    std::string qualified = std::string(domain) + "-" + std::string(property);
    return merged.find(subject, qualified) ? qualified : std::string(property);
}

std::string value_for(const maiz::Merged& merged, const std::string& subject,
                      std::string_view domain, std::string_view property) {
    // A domain-qualified cell that CONFLICTS does not fall through to the plain
    // property. "These two sources disagree about the map" is not an argument
    // for showing the card's answer on the map instead — it is the one case the
    // whole design exists to surface, and quietly substituting a value would
    // hide it behind something that looks deliberate.
    return merged.value(subject, cell_for(merged, subject, domain, property));
}

bool color_for(const maiz::Merged& merged, const std::string& subject,
               std::string_view domain, unsigned& rgba) {
    return parse_hex(value_for(merged, subject, domain, "color"), rgba);
}

std::vector<std::string> list_for(const maiz::Merged& merged,
                                  const std::string& subject,
                                  std::string_view domain,
                                  std::string_view property) {
    // The `All` law joins its set with ", " — split it back so a renderer gets
    // items rather than one long line. Splitting on the library's own separator
    // is a small coupling, and the alternative (every host reinventing it) is
    // worse; it is reported upstream rather than worked around further.
    std::string v = value_for(merged, subject, domain, property);
    std::vector<std::string> out;
    for (size_t i = 0; i < v.size();) {
        size_t j = v.find(", ", i);
        if (j == std::string::npos) j = v.size();
        if (j > i) out.push_back(v.substr(i, j - i));
        i = j + 2;
    }
    return out;
}

double number_for(const maiz::Merged& merged, const std::string& subject,
                  std::string_view domain, std::string_view property,
                  double fallback) {
    std::string v = value_for(merged, subject, domain, property);
    if (v.empty()) return fallback;
    char* end = nullptr;
    double d = std::strtod(v.c_str(), &end);
    return (end && end != v.c_str()) ? d : fallback;
}

// ── 3. predicates ───────────────────────────────────────────────────────────

const Frame::Row* Frame::find(std::string_view id) const {
    auto it = std::lower_bound(
        rows.begin(), rows.end(), id,
        [](const std::pair<std::string, Row>& a, std::string_view b) {
            return a.first < b;
        });
    if (it == rows.end() || it->first != id) return nullptr;
    return &it->second;
}

std::string Frame::field(std::string_view id, std::string_view key) const {
    // THE PRIVACY SEAM, enforced at the read. An internal-notes-class field is
    // not observable by any predicate — not even to test whether it equals
    // something, because `field-has "notes=x"` over a few tries is a way to
    // reproduce it. The only thing a script may learn is `internal ""`, the
    // boolean, which is what a rule needs in order to EXCLUDE a rune from an
    // export and never enough to leak one.
    //
    // It has to be here rather than in each predicate: this is the single
    // accessor every one of them goes through, and a guard that a new predicate
    // can forget to call is not a seam.
    if (is_internal_field(key)) return {};
    const Row* r = find(id);
    if (!r) return {};
    for (const auto& f : r->fields)
        if (f.first == key) return f.second;
    return {};
}

std::shared_ptr<const Frame> make_frame(const maiz::Scene& data, FrameInputs in) {
    auto f = std::make_shared<Frame>();
    f->today = std::move(in.today);
    f->published = std::move(in.published);
    std::sort(f->published.begin(), f->published.end());

    std::unordered_map<std::string, size_t> idx;
    for (const maiz::SceneNode& n : data.nodes) {
        if (!is_subject_glyph(n.glyph)) continue;
        Frame::Row r;
        r.glyph = n.glyph;
        r.tags = n.tags;
        for (const auto& fl : n.fields) {
            std::string v = field_of(n, fl.key);
            // Internal content is not carried into the frame AT ALL — only the
            // fact that it exists. Nothing downstream can read what is not
            // there, which is a stronger guarantee than a checked accessor.
            if (is_internal_field(fl.key)) {
                r.has_internal = r.has_internal || !v.empty();
                v.clear();
            }
            r.fields.push_back({fl.key, std::move(v)});
        }
        for (const auto& fl : r.fields)
            if (fl.first == "geo" && !fl.second.empty())
                r.has_geo = parse_geo(fl.second, r.lat, r.lon);
        idx[n.name] = f->rows.size();
        f->rows.push_back({n.name, std::move(r)});
    }
    // Wires, both directions — Allomone traverses the graph STRUCTURALLY
    // (neighbours, degree), never by naming a specific rune.
    for (const auto& w : data.wires) {
        auto a = idx.find(w.from), b = idx.find(w.to);
        if (a == idx.end() || b == idx.end()) continue;
        f->rows[a->second].second.links.push_back({w.relation, w.to});
        f->rows[b->second].second.links.push_back({w.relation, w.from});
    }
    // Degree and eigenvector centrality, computed ONCE per frame — this is the
    // "expensive, opt-in, host-computed" path, and it is what keeps
    // `central "0.5"` an O(1) lookup inside a pure predicate.
    const size_t n = f->rows.size();
    std::vector<std::vector<size_t>> adj(n);
    for (size_t i = 0; i < n; ++i) {
        std::set<size_t> nb;
        for (const auto& lk : f->rows[i].second.links) {
            auto it = idx.find(lk.second);
            if (it != idx.end() && it->second != i) nb.insert(it->second);
        }
        adj[i].assign(nb.begin(), nb.end());
        f->rows[i].second.degree = (int)nb.size();
    }
    std::vector<double> c(n, 1.0);
    for (int iter = 0; iter < 40; ++iter) {
        std::vector<double> nc(n, 0.0);
        for (size_t i = 0; i < n; ++i)
            for (size_t j : adj[i]) nc[i] += c[j];
        double norm = 0;
        for (double v : nc) norm += v * v;
        norm = std::sqrt(norm);
        if (norm < 1e-9) break;
        for (size_t i = 0; i < n; ++i) nc[i] /= norm;
        c.swap(nc);
    }
    double mx = 0;
    for (double v : c) mx = std::max(mx, v);
    for (size_t i = 0; i < n; ++i)
        f->rows[i].second.centrality = mx > 0 ? c[i] / mx : 0.0;

    std::sort(f->rows.begin(), f->rows.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
    return f;
}

const std::vector<PredicateDoc>& predicate_docs() {
    static const std::vector<PredicateDoc> d = {
        {"under", "\"ns:\"", "carries any tag in that namespace — `under \"coat:\"`"},
        {"field", "\"key=value\"", "a field equals a value; `field \"role\"` = non-empty"},
        {"field-has", "\"key=text\"", "a field CONTAINS text, case-insensitively"},
        {"role", "\"volunteer\"", "shorthand for field \"role=volunteer\""},
        {"named-like", "\"substring\"", "the rune's name contains it, case-insensitively"},
        {"before", "\"2026-09-01\"", "its date/deadline falls before that literal date"},
        {"after", "\"2026-09-01\"", "its date/deadline falls after that literal date"},
        {"overdue", "\"30\"", "dated more than N days BEFORE today"},
        {"upcoming", "\"30\"", "dated within the next N days"},
        // The empty argument on `undated`/`located`/`isolated` is NOT optional:
        // every predicate takes one, so `when isolated then …` is a parse error
        // and `when isolated "" then …` is the rule. Two of the seeded examples
        // shipped without it until the example test caught them.
        {"undated", "\"\"", "carries no date and no deadline at all"},
        {"near", "\"lat,lon,km\"", "its `geo` is within km of that point (Earth)"},
        {"located", "\"\"", "has a usable `geo` at all"},
        {"linked", "\"relation\"", "has an edge of that relation (\"\" = any edge)"},
        {"linked-to", "\"name\"", "is joined to that rune, by any relation"},
        {"degree-over", "\"3\"", "has more than N distinct neighbours"},
        {"central", "\"0.5\"", "eigenvector centrality at or above that, 0..1"},
        {"isolated", "\"\"", "no edges at all — the outreach gap"},
        {"untagged", "\"\"", "carries no tags at all — the hygiene rule"},
        {"has-image", "\"\"", "has an avatar, an image path or an image URL"},
        {"near-rune", "\"name,km\"", "within km of ANOTHER rune's location"},
        {"published", "\"\"", "referenced by a Builder document (newsletter or page)"},
        {"internal", "\"\"",
         "carries internal notes. The ONLY thing a script may learn about them: "
         "enough to exclude a rune from an export, never enough to leak one"},
    };
    return d;
}

maiz::PredicateRegistry predicates(std::shared_ptr<const Frame> frame) {
    maiz::PredicateRegistry p;
    // Every lambda captures the shared_ptr by value, so the registry can outlive
    // the caller's reference to the frame without dangling. All of them are pure
    // functions of (subject, arg, frozen frame) — no clock is reachable from
    // here, which is what keeps merge() order-independent.

    // The legacy dialect's `has "ns:"` matched a tag PREFIX; upstream's kernel
    // `has`/`tag` is exact equality. Restoring the namespace match as a domain
    // predicate is the right shape — it is our tag convention, not the
    // language's — and it is the single most-used condition in the old scripts.
    p.add("under", [](const maiz::Subject& s, std::string_view arg,
                      const maiz::UserGraph*) {
        if (arg.empty()) return false;
        for (const std::string& t : s.tags)
            if (t.size() >= arg.size() && t.compare(0, arg.size(), arg) == 0) return true;
        return false;
    });
    p.add("field", [frame](const maiz::Subject& s, std::string_view arg,
                           const maiz::UserGraph*) {
        std::string k, v; bool hv = false;
        split_pair(arg, k, v, hv);
        std::string got = frame->field(s.id, k);
        return hv ? got == v : !got.empty();
    });
    p.add("field-has", [frame](const maiz::Subject& s, std::string_view arg,
                               const maiz::UserGraph*) {
        std::string k, v; bool hv = false;
        split_pair(arg, k, v, hv);
        return hv && icontains(frame->field(s.id, k), v);
    });
    p.add("role", [frame](const maiz::Subject& s, std::string_view arg,
                          const maiz::UserGraph*) {
        return frame->field(s.id, "role") == arg;
    });
    p.add("named-like", [](const maiz::Subject& s, std::string_view arg,
                           const maiz::UserGraph*) {
        return icontains(s.name, arg);
    });

    // ── dates. `today` is frozen in the frame; nothing here reads a clock. ──
    auto dated = [](const std::shared_ptr<const Frame>& f, const maiz::Subject& s,
                    long& out) {
        const Frame::Row* r = f->find(s.id);
        return r && iso_days(subject_date(*r), out);
    };
    p.add("before", [frame, dated](const maiz::Subject& s, std::string_view arg,
                                   const maiz::UserGraph*) {
        long mine = 0, theirs = 0;
        return dated(frame, s, mine) && iso_days(arg, theirs) && mine < theirs;
    });
    p.add("after", [frame, dated](const maiz::Subject& s, std::string_view arg,
                                  const maiz::UserGraph*) {
        long mine = 0, theirs = 0;
        return dated(frame, s, mine) && iso_days(arg, theirs) && mine > theirs;
    });
    p.add("overdue", [frame, dated](const maiz::Subject& s, std::string_view arg,
                                    const maiz::UserGraph*) {
        long mine = 0, now = 0;
        if (!iso_days(frame->today, now)) return false; // no today = silence
        return dated(frame, s, mine) && (now - mine) > to_int(arg);
    });
    p.add("upcoming", [frame, dated](const maiz::Subject& s, std::string_view arg,
                                     const maiz::UserGraph*) {
        long mine = 0, now = 0;
        if (!iso_days(frame->today, now) || !dated(frame, s, mine)) return false;
        const long ahead = mine - now;
        return ahead >= 0 && ahead <= to_int(arg);
    });
    p.add("undated", [frame, dated](const maiz::Subject& s, std::string_view,
                                    const maiz::UserGraph*) {
        long mine = 0;
        return !dated(frame, s, mine);
    });

    // ── geography ──────────────────────────────────────────────────────────
    p.add("near", [frame](const maiz::Subject& s, std::string_view arg,
                          const maiz::UserGraph*) {
        const Frame::Row* r = frame->find(s.id);
        if (!r || !r->has_geo) return false;
        double lat = 0, lon = 0, km = 0;
        if (std::sscanf(std::string(arg).c_str(), "%lf,%lf,%lf", &lat, &lon, &km) != 3)
            return false;
        return km_between(r->lat, r->lon, lat, lon) <= km;
    });
    p.add("located", [frame](const maiz::Subject& s, std::string_view,
                             const maiz::UserGraph*) {
        const Frame::Row* r = frame->find(s.id);
        return r && r->has_geo;
    });

    // ── the graph ──────────────────────────────────────────────────────────
    p.add("linked", [frame](const maiz::Subject& s, std::string_view arg,
                            const maiz::UserGraph*) {
        const Frame::Row* r = frame->find(s.id);
        if (!r) return false;
        if (arg.empty()) return !r->links.empty();
        for (const auto& lk : r->links)
            if (lk.first == arg) return true;
        return false;
    });
    p.add("linked-to", [frame](const maiz::Subject& s, std::string_view arg,
                               const maiz::UserGraph*) {
        const Frame::Row* r = frame->find(s.id);
        if (!r) return false;
        for (const auto& lk : r->links)
            if (lk.second == arg) return true;
        return false;
    });
    p.add("degree-over", [frame](const maiz::Subject& s, std::string_view arg,
                                 const maiz::UserGraph*) {
        const Frame::Row* r = frame->find(s.id);
        return r && r->degree > to_int(arg);
    });
    p.add("central", [frame](const maiz::Subject& s, std::string_view arg,
                             const maiz::UserGraph*) {
        const Frame::Row* r = frame->find(s.id);
        return r && r->centrality >= to_dbl(arg);
    });
    p.add("isolated", [frame](const maiz::Subject& s, std::string_view,
                              const maiz::UserGraph*) {
        const Frame::Row* r = frame->find(s.id);
        return r && r->links.empty();
    });
    p.add("near-rune", [frame](const maiz::Subject& s, std::string_view arg,
                               const maiz::UserGraph*) {
        // Distance to ANOTHER RUNE rather than to a literal coordinate: "every
        // volunteer within 5km of the food bank" is the question an outreach
        // org actually asks, and it keeps meaning the same thing after the food
        // bank moves. Naming a rune is normally discouraged; here the rune IS
        // the parameter, which is the case the discouragement does not cover.
        const Frame::Row* me = frame->find(s.id);
        if (!me || !me->has_geo) return false;
        std::string a(arg);
        size_t comma = a.rfind(',');
        if (comma == std::string::npos) return false;
        const Frame::Row* other = frame->find(std::string_view(a).substr(0, comma));
        if (!other || !other->has_geo) return false;
        return km_between(me->lat, me->lon, other->lat, other->lon) <=
               to_dbl(std::string_view(a).substr(comma + 1));
    });

    // ── hygiene, output and privacy ────────────────────────────────────────
    p.add("untagged", [](const maiz::Subject& s, std::string_view,
                         const maiz::UserGraph*) { return s.tags.empty(); });
    p.add("has-image", [frame](const maiz::Subject& s, std::string_view,
                               const maiz::UserGraph*) {
        static const char* keys[] = {"avatar", "path", "image_url", "url"};
        for (const char* k : keys)
            if (!frame->field(s.id, k).empty()) return true;
        return false;
    });
    p.add("published", [frame](const maiz::Subject& s, std::string_view,
                               const maiz::UserGraph*) {
        return std::binary_search(frame->published.begin(), frame->published.end(),
                                  s.id);
    });
    p.add("internal", [frame](const maiz::Subject& s, std::string_view,
                              const maiz::UserGraph*) {
        // The privacy seam's one observable. A boolean cannot be mined for
        // content, and it is exactly what `when internal "" then web-hide "1"`
        // needs. Everything else about an internal field is unreadable — the
        // value is not even carried into the frame (see Frame::Row::fields).
        const Frame::Row* r = frame->find(s.id);
        return r && r->has_internal;
    });
    return p;
}

// ── 4. the derivation ───────────────────────────────────────────────────────

std::vector<maiz::Resolution> resolutions_from(const maiz::Scene& scripts) {
    std::vector<maiz::Resolution> out;
    for (const maiz::SceneNode& n : scripts.nodes) {
        if (n.glyph != "allomone-resolution") continue;
        out.push_back({field_of(n, "subject"), field_of(n, "property"),
                       field_of(n, "winner"), field_of(n, "value")});
    }
    return out;
}

/* Report a rule that names an internal-notes-class field.
 *
 * Without this the seam is correct and INVISIBLE: `field "notes"` reads empty,
 * the rule matches nothing, and the author concludes their data is wrong rather
 * than that the language declined. A refusal that cannot be distinguished from
 * a bug is a bad refusal, so it says so. */
static void check_privacy(const maiz::Script& s, std::vector<maiz::Diagnostic>& out) {
    auto flag = [&](int line, const std::string& key) {
        out.push_back({line, "`" + key + "` is an internal-notes field and is "
                             "never readable by a rule - use `internal \"\"` to "
                             "test that a rune has notes, which is all a rule "
                             "may know about them"});
    };
    auto arg_key = [](const std::string& v) {
        size_t p = v.find('=');
        return p == std::string::npos ? v : v.substr(0, p);
    };
    for (const maiz::Rule& r : s.rules)
        for (const maiz::Term& t : r.terms)
            if (t.kind == maiz::Term::Kind::Host &&
                (t.name == "field" || t.name == "field-has") &&
                is_internal_field(arg_key(t.value)))
                flag(r.line, arg_key(t.value));
}

Derivation derive(const maiz::Scene& data, const maiz::Scene& scripts,
                  const Options& opts) {
    Derivation d;
    d.subjects = subjects_from(data);
    d.frame = make_frame(data, opts.inputs);
    maiz::PredicateRegistry preds = predicates(d.frame);

    // Recency is supplied even though the policy is Surface today: it costs a
    // counter, and a source with no recency at all is the case that makes
    // switching the policy on later behave like "nobody has an opinion".
    int order = 0;
    for (const maiz::SceneNode& n : scripts.nodes) {
        if (n.glyph != kScriptGlyph) continue;
        SourceInfo info;
        info.id = n.name;
        info.label = field_of(n, "title");
        if (info.label.empty()) info.label = n.name;
        info.enabled = field_of(n, "enabled") != "0";

        maiz::Script s = maiz::allo_parse(n.name, field_of(n, "source"));
        info.rules = (int)s.rules.size();
        info.diagnostics = s.diagnostics;
        // Unknown predicates are reported SEPARATELY from parse errors on
        // purpose: a script naming a predicate we lack is syntactically fine
        // (and stays readable and diffable), it just never matches. This is how
        // the tab tells an author why nothing happened.
        for (const maiz::Diagnostic& x : maiz::allo_check(s, &preds))
            info.diagnostics.push_back(x);
        check_privacy(s, info.diagnostics); // ours, not upstream's

        if (info.enabled) {
            maiz::ConstraintMap m = maiz::allo_eval(s, d.subjects, nullptr, &preds);
            m.recency = ++order;
            info.cells = (int)m.cells.size();
            d.sources.push_back(std::move(m));
        }
        d.scripts.push_back(std::move(info));
    }

    for (const maiz::ConstraintMap& m : opts.extra_sources) {
        SourceInfo info;
        info.id = m.id;
        info.label = m.id;
        info.enabled = true;
        info.legacy = true;
        info.cells = (int)m.cells.size();
        d.scripts.push_back(std::move(info));
        d.sources.push_back(m);
    }

    maiz::MergeOptions mo = merge_options();
    mo.resolutions = opts.resolutions;
    d.merged = maiz::merge(d.sources, mo);
    return d;
}

// ── 5. meaning ──────────────────────────────────────────────────────────────

bool parse_hex(const std::string& hex, unsigned& rgba) {
    unsigned r = 0, g = 0, b = 0;
    if (hex.size() < 7 || hex[0] != '#') return false;
    if (std::sscanf(hex.c_str(), "#%02x%02x%02x", &r, &g, &b) != 3) return false;
    rgba = 0xFF000000u | (b << 16) | (g << 8) | r; // IM_COL32 order (ABGR)
    return true;
}

std::string explain(const maiz::MergedCell& cell) {
    std::vector<maiz::CellVerdict> vs = maiz::explain_cell(cell);
    auto label = [](const maiz::CellVerdict& v) {
        return v.origin.empty() ? v.source : v.source + " (" + v.origin + ")";
    };
    if (cell.conflicted) {
        std::string out = "unsettled — ";
        bool first = true;
        for (const maiz::CellVerdict& v : vs) {
            if (v.verdict != "tied") continue;
            if (!first) out += " vs ";
            out += label(v) + " says " + v.value;
            first = false;
        }
        return first ? "unsettled" : out;
    }
    if (vs.empty()) return cell.settled_by;
    const maiz::CellVerdict& w = vs.front();
    if (cell.settled_by == "arithmetic" || cell.settled_by == "join") {
        std::string out = cell.value + " — combined from ";
        for (size_t i = 0; i < vs.size(); ++i)
            out += (i ? ", " : "") + label(vs[i]) + "=" + vs[i].value;
        return out;
    }
    if (cell.settled_by == "resolution")
        return cell.value + " — settled by hand (" + (cell.source.empty()
                                                          ? std::string("explicit value")
                                                          : cell.source) + ")";
    if (cell.settled_by == "agreement")
        return cell.value + " — every source agreed";
    std::string out = cell.value + " — " + label(w);
    for (const maiz::CellVerdict& v : vs)
        if (v.lost) { out += ", beating " + label(v) + "=" + v.value; break; }
    return out;
}

// ── 6. teaching the vocabulary ──────────────────────────────────────────────

namespace {

struct Keyword { const char* word; const char* meaning; };

const std::vector<Keyword>& keywords() {
    static const std::vector<Keyword> k = {
        {"when", "when — begins a rule. A script is a SET of rules: moving a\n"
                 "line changes nothing, and there is no first-match-wins."},
        {"then", "then — separates the condition from the effects. Several\n"
                 "effects are separated by commas. (`->` is an older spelling.)"},
        {"and", "and — both conditions must hold. Each term adds 1 to the\n"
                "rule's STRENGTH, and the sharper rule wins."},
        {"not", "not — the term must NOT hold. It works on one term only:\n"
                "negating a conjunction would need `or`, and there is no `or`.\n"
                "Define the negated condition directly instead."},
        {"all", "all — matches everything, at strength 0. The broad default a\n"
                "sharper rule is meant to beat. Careful under a combining law:\n"
                "there it becomes an ingredient rather than being overridden."},
        {"define", "define NAME(args) = <condition> — names a condition so it\n"
                   "can be reused. Expanded where it is called, so its terms\n"
                   "count toward strength: a call can never make a rule broader\n"
                   "than it looks. Script-local, and order-free."},
        {"tag", "tag \"x\" — the subject carries exactly that tag.\n"
                "`has` is the same term in Void Core's spelling.\n"
                "For a whole namespace use Hormiga's `under \"ns:\"`."},
        {"has", "has \"x\" — the subject carries exactly that tag. Void Core's\n"
                "spelling of `tag`; they are exact synonyms, not variants.\n"
                "NOTE: exact, not a prefix. For `role:anything` use `under`."},
        {"kind", "kind \"contact\" — the subject's glyph. `glyph` is the same\n"
                 "term in Void Core's spelling."},
        {"glyph", "glyph \"contact\" — the subject's type. Void Core's spelling\n"
                  "of `kind`; contact, organization, event, image, note, job…"},
        {"name", "name \"ada\" — the subject's name, exactly. `rune` is the\n"
                 "same term in Void Core's spelling. Prefer a tag or a\n"
                 "predicate: logic that names one rune stops meaning anything\n"
                 "when the data changes."},
        {"rune", "rune \"ada\" — this rune, by name. Void Core's spelling of\n"
                 "`name`."},
        {"mantle", "mantle \"demo-org\" — which mantle the subject lives in."},
        {"with", "with \"x\" — reads THE PERSON, not the data: true when the\n"
                 "subject and x are coherent in the user graph. Hormiga ships\n"
                 "no user graph yet, so it never matches — silence, not an\n"
                 "error."},
        {"device", "device \"touch\" — reads THE PERSON: the modality the\n"
                   "subject was last reached through. No user graph here yet,\n"
                   "so it never matches."},
    };
    return k;
}

} // namespace

std::string explain_word(std::string_view word) {
    for (const Keyword& k : keywords())
        if (word == k.word) return k.meaning;
    for (const PredicateDoc& p : predicate_docs())
        if (word == p.name)
            return std::string(p.name) + " " + p.arg + " — " + p.meaning +
                   "\n\nA Hormiga predicate: our domain's condition vocabulary, "
                   "for\nthe things tags encode badly. It counts toward "
                   "strength like\nany other term, and every predicate takes an "
                   "argument — even\nthe ones that ignore it.";
    // Surface-qualified first, so `map-color` explains the surface AND the law.
    for (const Domain& d : domains()) {
        std::string pre = std::string(d.prefix) + "-";
        if (word.size() <= pre.size() || word.compare(0, pre.size(), pre) != 0)
            continue;
        std::string bare(word.substr(pre.size()));
        for (const Property& p : vocabulary())
            if (bare == p.name)
                return std::string(word) + " — " + p.meaning + "\n\nSurface: " +
                       d.what + ".\nOnly that surface reads it; the unprefixed `" +
                       p.name + "` reaches all of\nthem. Same merge law either "
                       "way: " + std::string(maiz::lattice_name(p.law)) + ".";
    }
    for (const Property& p : vocabulary())
        if (word == p.name)
            return std::string(p.name) + " — " + p.meaning +
                   "\n\nMerge law: " + std::string(maiz::lattice_name(p.law)) +
                   ".\nPrefix it with a surface (card- map- cal- web-) to speak "
                   "to\njust one of them.";
    return {};
}

std::vector<std::string> vocabulary_words() {
    std::vector<std::string> out;
    for (const Keyword& k : keywords()) out.push_back(k.word);
    for (const PredicateDoc& p : predicate_docs()) out.push_back(p.name);
    for (const Property& p : vocabulary()) {
        out.push_back(p.name);
        for (const Domain& d : domains())
            out.push_back(std::string(d.prefix) + "-" + p.name);
    }
    return out;
}

const char* starter_script() {
    return "# A new Allomone script. Rules are a SET, not a sequence: order\n"
           "# changes nothing, and the sharper rule wins. Derive-only — this\n"
           "# never edits your data, and disabling it undoes nothing.\n"
           "\n"
           "when all                then color \"#8a9199\"\n"
           "when glyph \"contact\"    then color \"#2e8b57\"\n"
           "when glyph \"contact\" and under \"role:\" then weight 1\n"
           "\n"
           "# Prefix a property with a surface to speak to just that one:\n"
           "# card. / map. / cal. / web.   Unprefixed reaches all of them.\n"
           "when has \"urgent\"        then map-color \"#c0392b\", badge \"urgent\"\n";
}

} // namespace allomone
} // namespace hormiga
