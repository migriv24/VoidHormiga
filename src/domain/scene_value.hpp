/* scene_value.hpp — reading a value off a projected rune. ONE implementation.
 *
 * ── why this file exists ─────────────────────────────────────────────────────
 *
 * There were two `field_value` functions — `app_shared.cpp` and `temper.hpp` —
 * with identical bodies, and a decoder (`unjson_str`) applied by hand at five
 * call sites. On 2026-08-20 that arrangement put a literal `\` `n` on a live
 * public page: a real newline in a job description came back as its two-
 * character JSON escape because the primitive stripped the surrounding quotes
 * and stopped. 32 of them in one card.
 *
 * The decoder had existed the whole time. It was simply not where the reading
 * happened. **A correction applied per-caller is a correction missing from
 * every caller nobody thought of** — which is the argument for this file, and
 * in miniature the argument for the whole `src/` restructure.
 *
 * Header-only and dependency-free ON PURPOSE: `temper.hpp` is included by
 * `tests/spine_smoke.cpp`, which links neither the app nor the view, and the
 * whole point of a temper pass is that it is Scene-in / commands-out and
 * testable without a window. A primitive that drags a library behind it is a
 * primitive people will copy instead of include.
 */
#pragma once

#include "voidmaiz/scene.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace hormiga {

/* Decode a JSON string BODY (no surrounding quotes) into its actual bytes.
 *
 * Hand-rolled rather than `nlohmann::json::parse("\"" + s + "\"")`, which is
 * what `unjson_str` did, for two reasons. The GUI reads fields inside its frame
 * loop — every rune, every field, sixty times a second — and spinning up a JSON
 * parser plus a try/catch per read is real cost for a job this small. And a
 * parse that throws on malformed input has to be wrapped anyway; this one
 * cannot fail, because an unrecognised escape is passed through verbatim, which
 * is the behaviour a renderer wants: show what is stored, never throw away a
 * character the reader might need.
 *
 * The fast path is the common one — no backslash, no work, no copy.
 */
inline std::string json_unescape(const std::string& s) {
    if (s.find('\\') == std::string::npos) return s;
    std::string out;
    out.reserve(s.size());
    auto hex4 = [&](size_t i, unsigned& v) {
        if (i + 4 > s.size()) return false;
        v = 0;
        for (size_t k = i; k < i + 4; ++k) {
            const char c = s[k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        return true;
    };
    auto utf8 = [&](unsigned cp) {
        if (cp < 0x80) {
            out += (char)cp;
        } else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    };
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\' || i + 1 >= s.size()) { out += s[i]; continue; }
        const char e = s[++i];
        switch (e) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'u': {
            unsigned cp = 0;
            if (!hex4(i + 1, cp)) { out += '\\'; out += e; break; }
            i += 4;
            // a surrogate pair is two escapes describing one character
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 6 < s.size() &&
                s[i + 1] == '\\' && s[i + 2] == 'u') {
                unsigned lo = 0;
                if (hex4(i + 3, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    i += 6;
                }
            }
            utf8(cp);
            break;
        }
        default:
            // not an escape we know: keep both characters exactly as stored
            out += '\\';
            out += e;
            break;
        }
    }
    return out;
}

/* The value of `key` on `n`, decoded — "" when absent or JSON null.
 *
 * `is_string` is what says the payload is quoted; a number or a bool comes back
 * as its literal text, which is what every caller here already expects. */
inline std::string field_value(const maiz::SceneNode& n, std::string_view key) {
    for (const auto& f : n.fields)
        if (f.key == key) {
            std::string v = f.value_json;
            if (f.is_string && v.size() >= 2) {
                v = v.substr(1, v.size() - 2);
                v = json_unescape(v);
            }
            if (v == "null") v.clear();
            return v;
        }
    return {};
}

inline bool has_tag(const maiz::SceneNode& n, std::string_view tag) {
    for (const auto& t : n.tags)
        if (t == tag) return true;
    return false;
}

/* EVERY value of a namespaced tag. A rune may carry a namespace more than
 * once — a bilingual flier is genuinely `lang:en` AND `lang:es` — and reading
 * only the first is how the most useful sheet in a set (where to drop the
 * backpacks off) appeared on one language's page and not the other's.
 *
 * `tag_value` below returns the first and is still right for a namespace that
 * is single-valued by nature (`color:`). The plural form is for the ones where
 * carrying two is a meaningful thing to say. */
inline std::vector<std::string> tag_values(const maiz::SceneNode& n,
                                           std::string_view ns) {
    const std::string prefix = std::string(ns) + ":";
    std::vector<std::string> out;
    for (const auto& t : n.tags)
        if (t.rfind(prefix, 0) == 0) out.push_back(t.substr(prefix.size()));
    return out;
}

/* The value half of a namespaced tag (`color:red` → `red`), "" if unset. */
inline std::string tag_value(const maiz::SceneNode& n, std::string_view ns) {
    const std::string prefix = std::string(ns) + ":";
    for (const auto& t : n.tags)
        if (t.rfind(prefix, 0) == 0) return t.substr(prefix.size());
    return {};
}

} // namespace hormiga
