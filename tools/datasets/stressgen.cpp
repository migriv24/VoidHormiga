/* stressgen.cpp — a large, linked, clustered database for stress-testing Hormiga.
 *
 * NOT part of Hormiga (tools/datasets/README.md). Built on demand:
 *     cmake --build build --target hormiga_stressgen
 *     hormiga_stressgen --raw <folder of downloads> --out <folder> [--total 500000]
 *
 * ── WHAT IT MAKES ────────────────────────────────────────────────────────────
 *
 * One `.miga` whose data mantle holds:
 *  - REAL ORGANIZATIONS: every exempt organization in the IRS Business Master
 *    File for the states downloaded (public domain, irs.gov/pub/irs-soi). Kept:
 *    name, city, state, ZIP, NTEE category, revenue, assets. DROPPED, because
 *    they can name a person or a home: the "in care of" name and the street;
 *  - SYNTHETIC contacts and events, so no real person is in the file: names
 *    from fixed lists, `example.org` emails, 555 phone numbers;
 *  - LINKS with the structure a real outreach database has: a contact belongs
 *    to an organization and knows colleagues there; an event is hosted by an
 *    organization and attended by contacts; organizations partner within a city
 *    and a field; revenue and assets are VALUES on edges to two measure runes
 *    (SPEC §3.7.1, Hormiga okf/concepts/foundation/measures.md);
 *  - CLUSTERS: Void Maiz's Leiden hierarchy over that graph (value edges left
 *    out, per Void Core's note that values and strengths are not commensurable),
 *    written as tags `c1:12`, `c2:12.4`, `c3:12.4.3`. How well level 1 recovers
 *    the real structure is printed as NMI against city and against field.
 *
 * ── WHY IT WRITES THE STATE DOCUMENT ITSELF ──────────────────────────────────
 *
 * Every Hormiga change goes through the dispatcher (CLAUDE.md rule 3), and this
 * file is not a change: it is a fixture, like the Cat Colony. At this size the
 * dispatcher cannot build it at all — Void Core's per-command lookup inside a
 * batch makes an import superquadratic (MESSAGE_FOR_VOIDCORE_hormiga-what-one-
 * edit-costs-...), days at 5×10^5. So it writes the state in Void Core's
 * documented format (SPEC §2, verified against a document Core itself wrote) and
 * then hands it to Void Core: `--validate` loads it into a real core and
 * projects every rune and link back out, so the format is checked by the only
 * authority on it (not the `validate` verb, which is quadratic: see below).
 *
 * Deterministic: the same downloads, the same --seed, the same state document;
 * only the envelope's `saved` time differs. */
#include "voidmaiz/embed.hpp"
#include "voidmaiz/graph.hpp"
#include "voidmaiz/project.hpp"

#include "json.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using nlohmann::json;
using clk = std::chrono::steady_clock;

namespace {

double secs(clk::time_point t) { return std::chrono::duration<double>(clk::now() - t).count(); }

struct Rng {
    std::uint64_t s;
    std::uint64_t next() {
        std::uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    std::uint32_t below(std::uint32_t n) { return n ? (std::uint32_t)(next() % n) : 0; }
    double unit() { return (next() >> 11) * (1.0 / 9007199254740992.0); }
};

// ── CSV (RFC 4180: quoted fields, doubled quotes) ───────────────────────────
std::vector<std::string> csv_fields(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    bool q = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (q) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
            else if (c == '"') q = false;
            else cur += c;
        } else if (c == '"') q = true;
        else if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    out.push_back(cur);
    return out;
}

std::string slug(const std::string& s, std::size_t max = 40) {
    std::string o;
    for (unsigned char c : s) {
        if (std::isalnum(c)) o += (char)std::tolower(c);
        else if (!o.empty() && o.back() != '-') o += '-';
        if (o.size() >= max) break;
    }
    while (!o.empty() && o.back() == '-') o.pop_back();
    return o.empty() ? "x" : o;
}

std::string title(const std::string& s) {
    static const std::set<std::string> small = {"of", "the", "and", "for", "in", "a", "an", "at", "on", "to", "de", "del", "la", "los"};
    std::string o, word;
    bool first = true;
    const auto flush = [&] {
        if (word.empty()) return;
        std::string w;
        for (unsigned char c : word) w += (char)std::tolower(c);
        if (!(small.count(w) && !first)) w[0] = (char)std::toupper((unsigned char)w[0]);
        if (word.size() <= 3 && word.find_first_of("aeiouAEIOU") == std::string::npos) w = word; // ymca, usa, ii
        o += w;
        first = false;
        word.clear();
    };
    for (char c : s) {
        if (c == ' ') { flush(); o += ' '; }
        else word += c;
    }
    flush();
    return o;
}

const char* ntee_field(char major) {
    switch (major) {
    case 'A': return "arts";             case 'B': return "education";
    case 'C': return "environment";      case 'D': return "animals";
    case 'E': return "health";           case 'F': return "mental-health";
    case 'G': return "diseases";         case 'H': return "medical-research";
    case 'I': return "crime-and-legal";  case 'J': return "employment";
    case 'K': return "food";             case 'L': return "housing";
    case 'M': return "public-safety";    case 'N': return "recreation";
    case 'O': return "youth";            case 'P': return "human-services";
    case 'Q': return "international";    case 'R': return "civil-rights";
    case 'S': return "community";        case 'T': return "philanthropy";
    case 'U': return "science";          case 'V': return "social-science";
    case 'W': return "public-benefit";   case 'X': return "religion";
    case 'Y': return "mutual-benefit";   default:  return "unclassified";
    }
}

// ── the records ─────────────────────────────────────────────────────────────
struct Rec {
    std::string name, glyph, display;
    std::vector<std::string> tags;
    std::vector<std::pair<std::string, std::string>> content;
    std::string city, state, field;
    double lat = 0, lon = 0;
};

struct Link {
    std::uint32_t from, to;
    std::string relation;
    double weight = 1;
};

const char* kFirst[] = {"Maria", "Jose", "Ana", "Luis", "Carmen", "Juan", "Sofia", "Miguel", "Lucia", "Carlos",
    "Elena", "Jorge", "Rosa", "Pedro", "Isabel", "Diego", "Valeria", "Andres", "Camila", "Javier", "Daniela",
    "Ricardo", "Gabriela", "Fernando", "Paula", "Alejandro", "Adriana", "Roberto", "Natalia", "Eduardo", "James",
    "Mary", "John", "Patricia", "Robert", "Jennifer", "Michael", "Linda", "William", "Elizabeth", "David",
    "Barbara", "Richard", "Susan", "Joseph", "Jessica", "Thomas", "Sarah", "Charles", "Karen", "Mei", "Wei",
    "Hiroshi", "Yuki", "Minh", "Linh", "Amir", "Fatima", "Omar", "Leila", "Kwame", "Amara", "Olu", "Zainab",
    "Ivan", "Olga", "Nikolai", "Anya", "Sean", "Aoife", "Priya", "Arjun", "Ravi", "Anjali", "Tomas", "Ingrid",
    "Lars", "Freya", "Mateo", "Ximena", "Santiago", "Renata", "Emilio", "Paloma", "Joaquin", "Marisol"};
const char* kLast[] = {"Garcia", "Rodriguez", "Martinez", "Hernandez", "Lopez", "Gonzalez", "Perez", "Sanchez",
    "Ramirez", "Torres", "Flores", "Rivera", "Gomez", "Diaz", "Cruz", "Morales", "Reyes", "Gutierrez", "Ortiz",
    "Ruiz", "Alvarez", "Castillo", "Jimenez", "Vasquez", "Romero", "Herrera", "Medina", "Aguilar", "Vargas",
    "Smith", "Johnson", "Williams", "Brown", "Jones", "Miller", "Davis", "Wilson", "Anderson", "Taylor", "Thomas",
    "Moore", "Jackson", "White", "Harris", "Martin", "Thompson", "Nguyen", "Tran", "Le", "Pham", "Kim", "Lee",
    "Park", "Chen", "Wang", "Li", "Zhang", "Patel", "Shah", "Singh", "Khan", "Ali", "Okafor", "Mensah", "Ivanova",
    "Petrov", "Murphy", "Kelly", "Sullivan", "Hansen", "Larsen", "Silva", "Santos", "Oliveira", "Costa"};
const char* kRoles[] = {"volunteer", "staff", "board", "coordinator", "director", "organizer", "member", "donor"};
const char* kEvents[] = {"Community dinner", "Food drive", "Volunteer orientation", "Board meeting",
    "Health fair", "ESL class", "Clothing drive", "Know your rights workshop", "Legal aid clinic",
    "Youth soccer clinic", "Job fair", "Tenant meeting", "Book drive", "Art night", "Park cleanup",
    "Fundraiser", "Vaccination clinic", "Open house", "Town hall", "Tutoring night"};

std::string rune_id(std::uint64_t i) {
    std::uint64_t z = i * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
    z = (z ^ (z >> 31)) * 0xBF58476D1CE4E5B9ull;
    char b[24];
    std::snprintf(b, sizeof b, "rune_%010llx", (unsigned long long)(z & 0xFFFFFFFFFFull));
    return b;
}

std::string geo(double lat, double lon) {
    char b[48];
    std::snprintf(b, sizeof b, "%.5f,%.5f", lat, lon);
    return b;
}

} // namespace

int main(int argc, char** argv) {
    fs::path raw = "raw", out_dir = ".";
    std::uint64_t seed = 7;
    std::size_t total = 500000, contacts_n = 150000;
    std::uint32_t target = 2000;
    std::string name = "stress-500k";
    bool validate = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto val = [&] { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--raw") raw = val();
        else if (a == "--out") out_dir = val();
        else if (a == "--total") total = std::stoull(val());
        else if (a == "--contacts") contacts_n = std::stoull(val());
        else if (a == "--seed") seed = std::stoull(val());
        else if (a == "--cluster-size") target = (std::uint32_t)std::stoul(val());
        else if (a == "--name") name = val();
        else if (a == "--validate") validate = true;
        else { std::cerr << "unknown option " << a << "\n"; return 2; }
    }
    const auto t0 = clk::now();
    Rng rng{seed};

    // ── ZIP → coordinates (Census 2020 ZCTA gazetteer, public domain) ──────────
    std::unordered_map<std::string, std::pair<double, double>> zips;
    {
        std::ifstream in(raw / "2020_Gaz_zcta_national.txt");
        std::string line;
        std::getline(in, line);
        while (std::getline(in, line)) {
            std::istringstream ss(line);
            std::string z, a, b, c, d, lat, lon;
            ss >> z >> a >> b >> c >> d >> lat >> lon;
            if (!lat.empty() && !lon.empty()) zips[z] = {std::stod(lat), std::stod(lon)};
        }
    }

    // ── organizations, from every eo_*.csv in name order ───────────────────────
    std::vector<Rec> recs;
    std::vector<double> revenue, assets;
    std::unordered_set<std::string> used;
    const auto unique = [&](std::string s) {
        std::string t = s;
        for (int k = 2; used.count(t); ++k) t = s + "-" + std::to_string(k);
        used.insert(t);
        return t;
    };
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(raw))
        if (e.path().filename().string().rfind("eo_", 0) == 0) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    std::size_t dropped = 0;
    for (const auto& f : files) {
        std::ifstream in(f);
        std::string line;
        std::getline(in, line);
        const auto head = csv_fields(line);
        const auto col = [&](const char* nm) {
            return (std::size_t)(std::find(head.begin(), head.end(), nm) - head.begin());
        };
        const std::size_t cNAME = col("NAME"), cCITY = col("CITY"), cST = col("STATE"), cZIP = col("ZIP"),
                          cNTEE = col("NTEE_CD"), cREV = col("REVENUE_AMT"), cASS = col("ASSET_AMT");
        while (std::getline(in, line)) {
            const auto v = csv_fields(line);
            if (v.size() < head.size()) { ++dropped; continue; }
            const std::string zip5 = v[cZIP].substr(0, 5);
            const auto z = zips.find(zip5);
            if (z == zips.end()) { ++dropped; continue; } // no coordinates: not placeable, left out
            Rec r;
            r.glyph = "organization";
            r.display = title(v[cNAME]);
            r.name = unique(slug(v[cNAME]));
            r.city = slug(v[cCITY], 30);
            r.state = slug(v[cST], 4);
            const std::string ntee = v[cNTEE];
            r.field = ntee_field(ntee.empty() ? 'Z' : ntee[0]);
            r.lat = z->second.first + (rng.unit() - 0.5) * 0.02;
            r.lon = z->second.second + (rng.unit() - 0.5) * 0.02;
            r.tags = {"type:organization", "city:" + r.city, "state:" + r.state, "field:" + r.field};
            if (!ntee.empty()) r.tags.push_back("ntee:" + slug(ntee, 8));
            r.content = {{"display_name", r.display}, {"kind", r.field},
                         {"location", title(v[cCITY]) + ", " + v[cST]}, {"geo", geo(r.lat, r.lon)}};
            recs.push_back(std::move(r));
            revenue.push_back(v[cREV].empty() ? 0 : std::atof(v[cREV].c_str()));
            assets.push_back(v[cASS].empty() ? 0 : std::atof(v[cASS].c_str()));
        }
    }
    /* A database smaller than the real organizations is a SAMPLE of them, never a
     * prefix (a prefix is one state's worth, and its clusters would be one
     * state's). They may take four fifths of what the contacts leave; the rest
     * is events. At 5×10^5 this keeps every organization, as before. */
    const std::size_t room = total > contacts_n + 2 ? (total - contacts_n - 2) * 4 / 5 : 0;
    if (recs.size() > room) {
        std::vector<std::size_t> pick(recs.size());
        for (std::size_t i = 0; i < pick.size(); ++i) pick[i] = i;
        for (std::size_t i = 0; i < room; ++i) std::swap(pick[i], pick[i + rng.below((std::uint32_t)(pick.size() - i))]);
        pick.resize(room);
        std::sort(pick.begin(), pick.end()); // the sample keeps the file's order
        std::vector<Rec> kept;
        std::vector<double> rv, av;
        for (const std::size_t i : pick) {
            kept.push_back(std::move(recs[i]));
            rv.push_back(revenue[i]);
            av.push_back(assets[i]);
        }
        recs = std::move(kept);
        revenue = std::move(rv);
        assets = std::move(av);
    }
    const std::size_t orgs_n = recs.size();
    if (orgs_n + contacts_n + 2 > total) contacts_n = total > orgs_n + 2 ? total - orgs_n - 2 : 0;
    const std::size_t events_n = total - orgs_n - contacts_n - 2;
    std::cerr << "organizations: " << orgs_n << " (" << dropped << " rows without coordinates left out), "
              << secs(t0) << " s\n";

    // indexes: organizations by city, and by (city, field)
    std::map<std::string, std::vector<std::uint32_t>> by_city, by_city_field;
    for (std::uint32_t i = 0; i < orgs_n; ++i) {
        by_city[recs[i].state + "/" + recs[i].city].push_back(i);
        by_city_field[recs[i].state + "/" + recs[i].city + "/" + recs[i].field].push_back(i);
    }
    // a host is chosen in proportion to 1 + log10(1 + revenue): bigger organizations have more people
    std::vector<double> cum(orgs_n);
    double acc = 0;
    for (std::size_t i = 0; i < orgs_n; ++i) cum[i] = (acc += 1 + std::log10(1 + std::max(0.0, revenue[i])));
    const auto pick_org = [&] {
        const double x = rng.unit() * acc;
        return (std::uint32_t)(std::lower_bound(cum.begin(), cum.end(), x) - cum.begin());
    };

    std::vector<Link> links;
    std::vector<std::vector<std::uint32_t>> members(orgs_n);

    // ── contacts ───────────────────────────────────────────────────────────────
    const std::size_t nf = sizeof kFirst / sizeof *kFirst, nl = sizeof kLast / sizeof *kLast;
    for (std::size_t i = 0; i < contacts_n; ++i) {
        const std::uint32_t home = pick_org();
        const Rec& o = recs[home];
        const std::string first = kFirst[rng.below((std::uint32_t)nf)], last = kLast[rng.below((std::uint32_t)nl)];
        const std::string role = kRoles[rng.below(8)];
        Rec r;
        r.glyph = "contact";
        r.display = first + " " + last;
        r.name = unique(slug(r.display) + "-" + std::to_string(i + 1));
        r.city = o.city;
        r.state = o.state;
        r.field = o.field;
        r.lat = o.lat + (rng.unit() - 0.5) * 0.05;
        r.lon = o.lon + (rng.unit() - 0.5) * 0.05;
        char phone[16];
        std::snprintf(phone, sizeof phone, "541-555-01%02u", rng.below(100));
        r.tags = {"type:contact", "city:" + r.city, "state:" + r.state, "role:" + role};
        r.content = {{"display_name", r.display}, {"role", role},
                     {"email", slug(first) + "." + slug(last) + "." + std::to_string(i + 1) + "@example.org"},
                     {"phone", phone}, {"geo", geo(r.lat, r.lon)}};
        const std::uint32_t id = (std::uint32_t)recs.size();
        recs.push_back(std::move(r));
        links.push_back({id, home, "member"});
        // colleagues: up to two people already at the same organization
        auto& m = members[home];
        for (int k = 0; k < 2 && !m.empty(); ++k) links.push_back({id, m[rng.below((std::uint32_t)m.size())], "knows"});
        m.push_back(id);
        // a second organization in the same city, for one in five
        if (rng.unit() < 0.2) {
            const auto& city = by_city[o.state + "/" + o.city];
            const std::uint32_t other = city[rng.below((std::uint32_t)city.size())];
            if (other != home) links.push_back({id, other, "volunteers_at"});
        }
        // and a little noise across the region, as real social graphs have
        if (rng.unit() < 0.05 && id > orgs_n) {
            const std::uint32_t anyone = (std::uint32_t)orgs_n + rng.below((std::uint32_t)(id - orgs_n));
            links.push_back({id, anyone, "knows"});
        }
    }
    std::cerr << "contacts: " << contacts_n << ", " << secs(t0) << " s\n";

    // ── events ─────────────────────────────────────────────────────────────────
    const std::size_t ne = sizeof kEvents / sizeof *kEvents;
    for (std::size_t i = 0; i < events_n; ++i) {
        const std::uint32_t host = pick_org();
        const Rec& o = recs[host];
        const int month = 1 + (int)rng.below(24), day = 1 + (int)rng.below(28), hour = 9 + (int)rng.below(10);
        char date[16], start[16], end[16];
        std::snprintf(date, sizeof date, "%04d-%02d-%02d", 2026 + (month - 1) / 12, (month - 1) % 12 + 1, day);
        std::snprintf(start, sizeof start, "%d:00 %s", hour > 12 ? hour - 12 : hour, hour >= 12 ? "PM" : "AM");
        std::snprintf(end, sizeof end, "%d:00 %s", hour + 2 > 12 ? hour + 2 - 12 : hour + 2, hour + 2 >= 12 ? "PM" : "AM");
        const std::string what = kEvents[rng.below((std::uint32_t)ne)];
        Rec r;
        r.glyph = "event";
        r.display = what;
        r.name = unique(slug(what) + "-" + std::to_string(i + 1));
        r.city = o.city;
        r.state = o.state;
        r.field = o.field;
        r.lat = o.lat;
        r.lon = o.lon;
        r.tags = {"type:event", "city:" + r.city, "state:" + r.state, "field:" + r.field};
        r.content = {{"title_en", what + " - " + o.display}, {"date", date}, {"start_time", start},
                     {"end_time", end}, {"venue", o.content[2].second}, {"geo", geo(r.lat, r.lon)}};
        const std::uint32_t id = (std::uint32_t)recs.size();
        recs.push_back(std::move(r));
        links.push_back({id, host, "hosted_by"});
        const auto& m = members[host];
        const std::uint32_t attend = m.empty() ? 0 : rng.below(std::min<std::uint32_t>(8, (std::uint32_t)m.size()) + 1);
        for (std::uint32_t k = 0; k < attend; ++k) links.push_back({m[rng.below((std::uint32_t)m.size())], id, "attends"});
    }
    std::cerr << "events: " << events_n << ", " << secs(t0) << " s\n";

    // ── partnerships: within a city and a field, and a few across the state ────
    for (std::uint32_t i = 0; i < orgs_n; ++i) {
        const auto& peers = by_city_field[recs[i].state + "/" + recs[i].city + "/" + recs[i].field];
        if (peers.size() > 1) {
            const std::uint32_t p = peers[rng.below((std::uint32_t)peers.size())];
            if (p != i) links.push_back({i, p, "partners"});
        }
        if (rng.unit() < 0.1) {
            const std::uint32_t p = rng.below((std::uint32_t)orgs_n);
            if (p != i && recs[p].field == recs[i].field) links.push_back({i, p, "partners"});
        }
    }
    const std::size_t structural = links.size();

    // ── two measures, and their values (SPEC §3.7.1) ───────────────────────────
    const std::uint32_t m_rev = (std::uint32_t)recs.size(), m_ass = m_rev + 1;
    for (const char* mname : {"revenue", "assets"}) {
        Rec r;
        r.glyph = "metric";
        r.name = mname;
        r.display = mname;
        r.tags = {"type:measure"};
        recs.push_back(std::move(r));
    }
    for (std::uint32_t i = 0; i < orgs_n; ++i) {
        if (revenue[i] > 0) links.push_back({i, m_rev, "revenue", revenue[i]});
        if (assets[i] > 0) links.push_back({i, m_ass, "assets", assets[i]});
    }
    std::cerr << "links: " << structural << " structural + " << links.size() - structural << " values, "
              << secs(t0) << " s\n";

    // ── clusters: Leiden, hierarchically, over the structural links ────────────
    {
        const auto tc = clk::now();
        std::vector<maiz::graph::Edge> ge;
        ge.reserve(structural);
        for (std::size_t i = 0; i < structural; ++i) ge.push_back({links[i].from, links[i].to, 1.0});
        const auto g = maiz::graph::Graph::from_edges((std::uint32_t)recs.size(), ge);
        maiz::graph::LeidenOptions lo;
        lo.seed = seed;
        const auto h = maiz::graph::hierarchy(g, target, lo, 4);
        std::vector<std::uint32_t> l1(recs.size()), city(recs.size()), field(recs.size());
        std::map<std::string, std::uint32_t> cid, fid;
        std::map<std::uint32_t, std::size_t> sizes;
        for (std::uint32_t v = 0; v < recs.size(); ++v) {
            const auto& p = h.path[v];
            l1[v] = p.empty() ? UINT32_MAX : p[0];
            if (!p.empty()) ++sizes[p[0]];
            city[v] = cid.emplace(recs[v].state + "/" + recs[v].city, (std::uint32_t)cid.size()).first->second;
            field[v] = fid.emplace(recs[v].field, (std::uint32_t)fid.size()).first->second;
            std::string acc_path;
            for (std::size_t d = 0; d < p.size(); ++d) {
                acc_path += (d ? "." : "") + std::to_string(p[d]);
                recs[v].tags.push_back("c" + std::to_string(d + 1) + ":" + acc_path);
            }
        }
        std::size_t biggest = 0;
        for (const auto& [_, s] : sizes) biggest = std::max(biggest, s);
        std::vector<std::uint32_t> l1_only(l1);
        const auto part = maiz::graph::leiden(g, lo); // level 1 alone, for its modularity
        std::cerr << "clusters: " << sizes.size() << " at level 1 (largest " << biggest << "), depth " << h.depth
                  << ", modularity " << part.modularity << ", NMI vs city " << maiz::graph::nmi(l1_only, city)
                  << ", vs field " << maiz::graph::nmi(l1_only, field) << ", " << secs(tc) << " s\n";
    }

    // ── write the state, then the bundle around it ─────────────────────────────
    fs::create_directories(out_dir);
    const fs::path miga = out_dir / (name + ".miga");
    {
        std::ofstream o(miga, std::ios::binary);
        char saved[32];
        const std::time_t t = std::time(nullptr);
        std::strftime(saved, sizeof saved, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
        o << "{\"magic\":\"MIGA\",\"version\":3,\"meta\":" << json{{"name", name}, {"app", "hormiga"}, {"saved", saved}}.dump()
          << ",\"rebuild\":{},\"assets\":{},\"state\":";
        o << "{\"version\":1,\"domains\":{},\"mantles\":[{\"id\":\"mantle_5e1f00d001\",\"name\":\"demo-org\","
             "\"domain\":null,\"runes\":[";
        for (std::size_t i = 0; i < recs.size(); ++i) {
            const Rec& r = recs[i];
            json c = json::object();
            for (const auto& [k, v] : r.content) c[k] = v;
            json rune = {{"spirit", {{"id", rune_id(i)}, {"name", r.name}}},
                         {"glyph", r.glyph},
                         {"facets", {{"who", ""}, {"what", ""}, {"when", ""}, {"where", ""}, {"why", ""}, {"how", ""}}},
                         {"tags", r.tags},
                         {"content", c},
                         {"placement", nullptr},
                         {"relations", json::array()}};
            if (r.glyph == "metric") rune["quantity"] = {{"level", "ratio"}, {"unit", "USD"}};
            o << (i ? "," : "") << rune.dump();
        }
        o << "],\"tags\":{},\"layout\":{\"edges\":[";
        for (std::size_t i = 0; i < links.size(); ++i) {
            const Link& l = links[i];
            o << (i ? "," : "") << "{\"from\":" << json(recs[l.from].name).dump() << ",\"to\":"
              << json(recs[l.to].name).dump() << ",\"relation\":\"" << l.relation << "\",\"weight\":" << l.weight
              << ",\"directed\":true}";
        }
        o << "]},\"rules\":[]}],\"scripts\":{},"
             "\"glyphs\":{\"metric\":{\"glyph\":\"metric\",\"label\":\"Measure\",\"kind\":\"measure\",\"fields\":[]}},"
             "\"config\":{\"actor\":\"human:hormiga\"},\"active\":{\"mantle\":\"demo-org\",\"domain\":null},"
             "\"bindings\":[]}}";
    }
    std::error_code ec;
    std::cerr << "wrote " << miga.string() << " (" << fs::file_size(miga, ec) / (1024 * 1024) << " MB, "
              << recs.size() << " runes, " << links.size() << " links), " << secs(t0) << " s\n";

    if (validate) { // the only authority on the format: a real core
        const auto tv = clk::now();
        std::ifstream in(miga, std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(in)), {});
        const std::size_t at = text.find(",\"state\":") + 9;
        maiz::Core core(std::string_view(text).substr(at, text.size() - at - 1));
        text.clear();
        text.shrink_to_fit();
        /* NOT the `validate` verb: it checks duplicate names with a nested loop
         * over every pair of runes (O(N^2)) and resolves each edge by scan
         * (O(E*N)) -- minutes at 5×10^4, days at 5×10^5 (reported to Void
         * Core). This file has unique names and resolving endpoints by
         * construction; what needs a core's word is that it LOADS and that every
         * rune and edge survives into a projection. */
        const double load_s = secs(tv);
        const auto te = clk::now();
        const std::size_t exported = core.export_state().size(); // what every projection of a live core starts with
        const double export_s = secs(te);
        const auto tp = clk::now();
        const auto sc = maiz::project_scene(core);
        const bool ok = sc.nodes.size() == recs.size() && sc.wires.size() == links.size();
        // Core's `save` keeps a copy of every mantle as the dirty-tracking baseline, inside the state
        core.dispatch("save");
        const std::size_t after_save = core.export_state().size();
        std::cerr << "validate: " << (ok ? "ok" : "MISMATCH") << ": a core loaded it and projected "
                  << sc.nodes.size() << " of " << recs.size() << " runes, " << sc.wires.size() << " of "
                  << links.size() << " links; load " << load_s << " s, one export " << export_s << " s ("
                  << exported / (1024 * 1024) << " MB), projection " << secs(tp) << " s; after a save the export is "
                  << after_save / (1024 * 1024) << " MB\n";
        return ok ? 0 : 1;
    }
    return 0;
}
