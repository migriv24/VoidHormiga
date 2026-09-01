/* app_shared.cpp — the helpers every section leans on, compiled ONCE.
 *
 * Declared in app_internal.hpp; the note there says why they are not inline.
 * Nothing here knows about HormigaApp: these are pure functions over strings,
 * dates, scene nodes and ImGui draw lists, which is what made them safe to
 * share in the first place and is the test for whether anything else belongs.
 */
extern "C" {
#include "voidcore.h"   // the SPEC 6.1 codec, exported in 0.2.7
}
#include <cstdlib>
#include "app/app_internal.hpp"

#include "json.hpp" // unjson_str decodes a setjson-escaped field

std::string json_str(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out + "\"";
}

std::string json_arg(const std::string& json) {
    // TRAILING BACKSLASHES GO OUTSIDE THE QUOTES. Void Core SPEC §6.1, added
    // 2026-08-17 after they found the third spelling of a bug we had already
    // hit twice. A value ENDING in a backslash puts it immediately before the
    // closing quote, where the tokenizer reads the pair as an escaped
    // apostrophe: the argument never closes, it swallows the rest of the line,
    // and dispatch still returns ok:true. Pinned upstream in
    // VoidCore/conformance/cases/12-arg-quoting.vs, where the naive spelling
    // demonstrably turns the value `C:\` into `C:'`.
    //
    // Outside the quotes a backslash is literal and, being neither whitespace
    // nor a quote, still belongs to the same token — so the run is emitted bare.
    //
    // Everything else passes through untouched: inside single quotes the ONLY
    // escape args.c honours is a backslash before an apostrophe, so escaping a
    // backslash would itself be a bug. Emitting the backslash AND a closing
    // quote is what truncated an Allomone comment saying "don't" in August.
    //
    // NOT using maiz::arg() yet: Void Maiz shipped it 2026-08-18 with the naive
    // spelling, one day after Void Core documented the trap. Reported; adopt
    // theirs once it carries this fix.
    size_t end = json.size();
    while (end > 0 && json[end - 1] == '\\') --end; // the trailing run
    std::string out = "'";
    for (size_t i = 0; i < end; ++i) {
        if (json[i] == '\'') out += '\\';
        out += json[i];
    }
    out += '\'';
    out.append(json, end, std::string::npos); // the run, bare and literal
    return out;
}

std::string unjson_str(const std::string& raw) {
    if (raw.empty() || raw.find('\\') == std::string::npos) return raw;
    try {
        return nlohmann::json::parse("\"" + raw + "\"").get<std::string>();
    } catch (...) {
        return raw;
    }
}

bool slider_field(const char* label, float stored, float mn, float mx,
                         const char* fmt, float* out) {
    ImGui::PushID(label);
    ImGuiID vkey = ImGui::GetID("##drag"), akey = ImGui::GetID("##editing");
    ImGui::PopID();
    ImGuiStorage* st = ImGui::GetStateStorage();
    float v = st->GetBool(akey, false) ? st->GetFloat(vkey, stored) : stored;
    ImGui::SliderFloat(label, &v, mn, mx, fmt);
    st->SetFloat(vkey, v);
    st->SetBool(akey, ImGui::IsItemActive());
    *out = v;
    return ImGui::IsItemDeactivatedAfterEdit();
}

std::string tag_value(const maiz::SceneNode& n, const char* ns) {
    std::string prefix = std::string(ns) + ":";
    for (const auto& t : n.tags)
        if (t.rfind(prefix, 0) == 0) return t.substr(prefix.size());
    return {};
}

MShape shape_from(const std::string& s) {
    if (s == "pin") return MShape::Pin;
    if (s == "square") return MShape::Square;
    if (s == "diamond") return MShape::Diamond;
    return MShape::Circle;
}

ImVec2 draw_marker_shape(ImDrawList* dl, ImVec2 s, float r, ImU32 col,
                                ImU32 ring, MShape sh) {
    switch (sh) {
    case MShape::Square:
        dl->AddRectFilled(ImVec2(s.x - r, s.y - r), ImVec2(s.x + r, s.y + r), col, 2);
        dl->AddRect(ImVec2(s.x - r, s.y - r), ImVec2(s.x + r, s.y + r), ring, 2, 0, 2);
        return s;
    case MShape::Diamond: {
        ImVec2 p[4] = {{s.x, s.y - r}, {s.x + r, s.y}, {s.x, s.y + r}, {s.x - r, s.y}};
        dl->AddConvexPolyFilled(p, 4, col);
        dl->AddPolyline(p, 4, ring, ImDrawFlags_Closed, 2);
        return s;
    }
    case MShape::Pin: {
        ImVec2 head(s.x, s.y - r * 1.55f); // bulb sits above; tip is at s
        ImVec2 tri[3] = {{head.x - r * 0.72f, head.y + r * 0.35f},
                         {head.x + r * 0.72f, head.y + r * 0.35f},
                         {s.x, s.y}};
        dl->AddConvexPolyFilled(tri, 3, col);
        dl->AddCircleFilled(head, r, col);
        dl->AddCircle(head, r, ring, 0, 2);
        return head; // icon goes in the bulb
    }
    default:
        dl->AddCircleFilled(s, r, col);
        dl->AddCircle(s, r, ring, 0, 2);
        return s;
    }
}

bool cal_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int cal_dim(int y, int m) { // days in month
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && cal_leap(y)) ? 29 : d[m - 1];
}

int cal_dow(int y, int m, int d) { // Zeller; 0 = Sunday
    if (m < 3) { m += 12; --y; }
    int k = y % 100, j = y / 100;
    int h = (d + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7; // 0=Sat
    return (h + 6) % 7;
}

void cal_today(int& y, int& m, int& d) {
    std::time_t t = std::time(nullptr);
    std::tm* lt = std::localtime(&t);
    y = lt->tm_year + 1900; m = lt->tm_mon + 1; d = lt->tm_mday;
}

void cal_add_days(int& y, int& m, int& d, int delta) {
    for (; delta > 0; --delta) {
        if (d < cal_dim(y, m)) ++d;
        else { d = 1; if (++m > 12) { m = 1; ++y; } }
    }
    for (; delta < 0; ++delta) {
        if (d > 1) --d;
        else { if (--m < 1) { m = 12; --y; } d = cal_dim(y, m); }
    }
}

float cal_parse_hhmm(const std::string& s) { // "HH:MM" → hours; <0 = none
    int h, mi;
    if (std::sscanf(s.c_str(), "%d:%d", &h, &mi) != 2) return -1.0f;
    if (h < 0 || h > 23 || mi < 0 || mi > 59) return -1.0f;
    return h + mi / 60.0f;
}

std::string cal_fmt_hhmm(float t) { // hours → "HH:MM", clamped
    t = std::clamp(t, 0.0f, 23.99f);
    int h = (int)t, mi = (int)std::lround((t - h) * 60.0f);
    if (mi == 60) { mi = 0; ++h; }
    char b[8];
    std::snprintf(b, sizeof b, "%02d:%02d", h, mi);
    return b;
}

bool parse_two(const std::string& data, float& a, float& b) {
    std::string s = data;
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        s = s.substr(1, s.size() - 2);
    return std::sscanf(s.c_str(), "%f %f", &a, &b) == 2;
}

/* A field's value, DECODED (2026-08-20).
 *
 * `value_json` is the JSON *encoding* of the value, so this stripped the
 * surrounding quotes and stopped — leaving every escape sequence as its literal
 * characters. A real `U+000A` in a job description therefore reached the public
 * page as the two characters `\` and `n`, and the site printed them: 32 in one
 * card on a live page. Every multi-line field on every surface had it — `bio`,
 * `summary_en`, `description`, `narrative.text_en`.
 *
 * The decoder existed the whole time. `unjson_str` sits twenty lines above and
 * was applied BY HAND at five call sites — an Allomone script body, a query, a
 * note's text — which is why the bug was invisible in the places anyone looked
 * and live everywhere else. A correction applied per-caller is a correction
 * that is missing from every caller nobody thought of.
 *
 * So it belongs in the primitive, and there is now exactly one primitive: the
 * duplicate in `temper.hpp` delegates here. Two copies of this function is what
 * let the two drift, and the hand-applied wrappers are removed — double
 * decoding is not harmless, because a decoded value holding a real backslash (a
 * Windows path) would be re-parsed and corrupted. */
std::string field_value(const maiz::SceneNode& n, std::string_view key) {
    return hormiga::field_value(n, key); // scene_value.hpp — the one reader
}

std::string subtitle(const maiz::SceneNode& n) {
    for (const auto& f : n.fields) {
        std::string v = field_value(n, f.key);
        if (v.empty()) continue;
        if (v.size() > 34) v = v.substr(0, 31) + "...";
        return v;
    }
    return {};
}

/* ── WE NO LONGER IMPLEMENT §6.1. WE CALL IT. (2026-08-21 -> 2026-08-25) ─────
 *
 * This function was hand-rolled twice. The first version toggled on `"` and
 * knew nothing else — no single quotes, no escapes, empty arguments dropped —
 * so a command typed into our command bar tokenized differently from the same
 * command dispatched to Core. The second was a careful reimplementation of
 * SPEC §6.1, rule for rule, written on 2026-08-21.
 *
 * The second one was also a mistake, and Void Core said so better than we
 * could:
 *
 *   > Five implementations of that section have been wrong, and the fifth was
 *   > the reference core that the other four were copying from. Documentation
 *   > did not stop the fourth instance, and it did not stop us. Code is the
 *   > only form of a rule that cannot be misread.
 *
 * Core 0.2.7 exports the codec. A careful reimplementation is still a
 * reimplementation — it is correct on the day it is written and drifts the day
 * the spec moves, which it just did (rule 5 changed: an unterminated quote is
 * now an ERROR rather than running to end of input). Ours would have kept the
 * old behaviour and been quietly wrong again.
 *
 * So this is now a thin adapter over `vc_argv_split_json`, and the only thing
 * it decides is what a HOST does with a refusal.
 *
 * ── WHAT WE DO WITH AN UNTERMINATED QUOTE ────────────────────────────────────
 *
 * Core returns `ok:false`. This returns an empty vector, and every caller
 * already treats "no tokens" as "not my verb / nothing to do" — so a malformed
 * line reaches the dispatcher instead, which refuses it with Core's own
 * message. That is deliberate: the error a person should see is the one from
 * the layer that owns the rule, not a paraphrase from us. */
std::vector<std::string> tokenize(const std::string& s) {
    std::vector<std::string> out;
    char* raw = vc_argv_split_json(s.c_str());
    if (!raw) return out;
    try {
        const auto j = nlohmann::json::parse(raw);
        if (j.value("ok", false) && j.contains("argv") && j["argv"].is_array())
            for (const auto& a : j["argv"]) out.push_back(a.get<std::string>());
    } catch (...) {
        // a malformed reply is Core's problem to report, not ours to guess at
    }
    std::free(raw);
    return out;
}

/* Quote an arbitrary value as ONE dispatcher argument (SPEC §6.1).
 *
 * Replaces `json_arg`'s hand-rolled version, whose long comment above is kept
 * because the trailing-backslash trap it describes is real and is exactly the
 * kind of thing a reader should know is handled rather than absent. It is
 * handled by Core now. */
std::string arg_quote(const std::string& value) {
    char* raw = vc_arg_quote(value.c_str());
    if (!raw) return "''";
    std::string out(raw);
    std::free(raw);
    return out;
}

bool contains_ci(std::string_view hay, std::string_view needle) {
    if (needle.empty()) return true;
    auto lower = [](std::string_view s) {
        std::string o(s);
        std::transform(o.begin(), o.end(), o.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return o;
    };
    return lower(hay).find(lower(needle)) != std::string::npos;
}

std::string humanize(const std::string& slug) {
    std::string out;
    bool cap = true;
    for (char c : slug) {
        if (c == '-' || c == '_') { out += ' '; cap = true; }
        else if (cap) { out += (char)std::toupper((unsigned char)c); cap = false; }
        else out += c;
    }
    return out;
}

std::vector<const maiz::SceneNode*> block_chain(const maiz::Scene& issue) {
    std::map<std::string, std::string> next;
    std::map<std::string, bool> has_prev;
    for (const auto& w : issue.wires)
        if (w.style == maiz::SceneWire::Style::Adjacency) {
            next[w.from] = w.to;
            has_prev[w.to] = true;
        }
    std::vector<const maiz::SceneNode*> chain;
    for (const auto& n : issue.nodes)
        if (next.count(n.name) && !has_prev[n.name])
            for (const maiz::SceneNode* c = &n; c;) {
                chain.push_back(c);
                auto it = next.find(c->name);
                c = (it != next.end()) ? issue.find(it->second) : nullptr;
            }
    for (const auto& n : issue.nodes)
        if (std::find(chain.begin(), chain.end(), &n) == chain.end())
            chain.push_back(&n);
    return chain;
}

std::string html_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;"; // attributes carry JSON (widgets);
        else out += c;                      // harmless in text content
    }
    return out;
}

unsigned long long fnv1a64(const std::string& bytes) {
    unsigned long long h = 1469598103934665603ull;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

const char* month_name_now() {
    static const char* names[] = {"january", "february", "march",     "april",
                                  "may",     "june",     "july",      "august",
                                  "september", "october", "november", "december"};
    auto t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return names[tm.tm_mon];
}

/* ── colour arithmetic (2026-08-20) ──────────────────────────────────────────
 * See app_internal.hpp for why this is host compute rather than CSS tricks. */

static void hex_rgb(const std::string& hex, double& r, double& g, double& b) {
    unsigned v = 0;
    const size_t h = hex.find('#');
    if (h != std::string::npos && hex.size() >= h + 7)
        v = (unsigned)std::strtoul(hex.substr(h + 1, 6).c_str(), nullptr, 16);
    r = ((v >> 16) & 0xFF) / 255.0;
    g = ((v >> 8) & 0xFF) / 255.0;
    b = (v & 0xFF) / 255.0;
}

static std::string rgb_hex(double r, double g, double b) {
    auto q = [](double c) {
        int i = (int)std::lround(std::clamp(c, 0.0, 1.0) * 255.0);
        return i;
    };
    char out[10];
    std::snprintf(out, sizeof out, "#%02x%02x%02x", q(r), q(g), q(b));
    return out;
}

double srgb_luminance(const std::string& hex) {
    double c[3];
    hex_rgb(hex, c[0], c[1], c[2]);
    for (double& x : c) // the sRGB → linear transfer function, per WCAG
        x = (x <= 0.04045) ? x / 12.92 : std::pow((x + 0.055) / 1.055, 2.4);
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
}

double contrast_ratio(const std::string& a, const std::string& b) {
    const double la = srgb_luminance(a), lb = srgb_luminance(b);
    const double hi = std::max(la, lb), lo = std::min(la, lb);
    return (hi + 0.05) / (lo + 0.05);
}

std::string ink_on(const std::string& bg) {
    /* Not "is the background light" — the two ratios, compared. They disagree
     * with the intuitive answer in the middle of the range, which is exactly
     * where a mid-tone brand colour lives and exactly where the old hardcoded
     * white was wrong. */
    return contrast_ratio("#ffffff", bg) >= contrast_ratio("#111111", bg)
               ? std::string("#ffffff")
               : std::string("#111111");
}

std::string shade(const std::string& hex, double amount) {
    double r, g, b;
    hex_rgb(hex, r, g, b);
    const double t = std::clamp(std::fabs(amount), 0.0, 1.0);
    const double target = amount >= 0 ? 1.0 : 0.0;
    return rgb_hex(r + (target - r) * t, g + (target - g) * t,
                   b + (target - b) * t);
}

std::string meet_contrast(const std::string& fg, const std::string& bg,
                          double target) {
    if (contrast_ratio(fg, bg) >= target) return fg;
    /* Walk AWAY from the background — darker text on a light ground, lighter on
     * a dark one — in small steps, keeping the hue. Twenty steps is enough to
     * reach black or white from anywhere, and stopping at the first passing
     * value keeps as much of the author's colour as the floor allows. */
    const bool darken = srgb_luminance(bg) > 0.35;
    for (int i = 1; i <= 20; ++i) {
        const std::string c = shade(fg, (darken ? -1.0 : 1.0) * (i * 0.05));
        if (contrast_ratio(c, bg) >= target) return c;
    }
    return ink_on(bg);
}

SiteTheme read_site_theme(maiz::Core& core) {
    SiteTheme t;
    auto hex = [&](const char* key, const char* def) {
        std::string s = core.dispatch(std::string("config get ") + key).data;
        size_t hp = s.find('#');
        return hp != std::string::npos ? s.substr(hp, 7) : std::string(def);
    };
    auto digit = [&](const char* key, int def) {
        std::string s = core.dispatch(std::string("config get ") + key).data;
        for (char c : s)
            if (c >= '0' && c <= '9') return c - '0';
        return def;
    };
    t.accent = hex("theme.accent", "#d4a017");
    t.accent2 = hex("theme.accent2", t.accent.c_str()); // default: same as accent
    t.bg = hex("theme.bg", "#faf8f3");
    t.ink = hex("theme.ink", "#20201d");
    t.preset = std::clamp(digit("theme.preset", 0), 0, 4);
    t.font = std::clamp(digit("theme.font", 0), 0, kNumFonts - 1);
    t.bodyfont = std::clamp(digit("theme.bodyfont", 0), 0, kNumFonts - 1);
    t.scale = std::clamp(digit("theme.scale", 2), 0, 4);
    t.radius = std::clamp(digit("theme.radius", 2), 0, 4);
    t.texture = std::clamp(digit("theme.texture", 0), 0, 3);
    t.dark = core.dispatch("config get theme.dark").data.find('0') ==
             std::string::npos;
    auto str = [&](const char* key) {
        std::string v = core.dispatch(std::string("config get ") + key).data;
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size() - 2);
        return v == "null" ? std::string() : v;
    };
    auto num = [&](const char* key, int def) {
        const std::string v = str(key);
        if (v.empty()) return def;
        char* end = nullptr;
        const long n = std::strtol(v.c_str(), &end, 10);
        return (end && end != v.c_str()) ? (int)n : def;
    };
    t.font_custom = str("theme.font_custom");
    /* The partners: authored when stated, derived otherwise. Derived rather
     * than "same colour at 20% opacity" because a tint over an unknown page
     * background is a different colour on every band, and because an
     * organization should be able to read its own palette out of the config
     * and hand it to a printer. */
    t.accent_lite = str("theme.accent_lite");
    if (t.accent_lite.empty()) t.accent_lite = shade(t.accent, 0.62);
    t.accent_dark = str("theme.accent_dark");
    if (t.accent_dark.empty()) t.accent_dark = shade(t.accent, -0.38);
    t.contrast = std::clamp(num("theme.contrast", 1), 0, 2);
    t.banner_filter = std::clamp(num("theme.banner_filter", 0), 0, 5);
    t.banner_dim = std::clamp(num("theme.banner_dim", 45), 0, 100);
    t.grid_gap = std::clamp(num("theme.grid_gap", 2), 0, 4);
    t.grid_even = num("theme.grid_even", 1) != 0;
    t.icons = num("theme.icons", 1) != 0;
    return t;
}
