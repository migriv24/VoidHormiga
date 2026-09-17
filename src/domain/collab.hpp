/* domain/collab.hpp — how a database is collaborated on, as the Antfarm says.
 *
 * okf/concepts/platform/lan-sharing.md §5. The author (2026-09-16): *"we shoud
 * also be considering how the antfarm determines how a database is collaborated
 * with … determining how data is updated and shared with each other … what is
 * local private data? … admins and permissions."*
 *
 * The same shape as `domain/hosting.hpp`: a CAPABILITY is a question, the table
 * says which kind of Antfarm node answers it, and a row marked `planned` names
 * the node that will answer it later so a relay is a row and a branch, not a
 * redesign. Every surface -- the Share window, Discover, presence, the CLI --
 * reads these rows and these readers rather than naming a node kind itself.
 *
 * Also here, because it is pure and every front-end needs the same answer:
 * which runes are private, and what colour each present person is shown in.
 *
 * Pure data and pure functions over a Scene. No GUI, no I/O.
 */
#pragma once

#include "domain/scene_value.hpp"
#include "voidmaiz/embed.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace hormiga::collab {

struct Capability {
    const char* id;        // "share", "members", "presence"
    const char* question;  // what it answers, in a person's words
    const char* glyph;     // the node kind that answers
    const char* how;       // how it answers
    bool planned;          // named so the choice exists, not built
};

inline constexpr Capability kCapabilities[] = {
    {"share", "Can this database be shared, and how does a copy reach someone?",
     "hol_lan_share", "over the local network: announce, ask, allow, send sealed", false},
    {"share", "Can this database be shared, and how does a copy reach someone?",
     "hol_sync_relay",
     "through a relay that holds only ciphertext - an object store or queue (lan-sharing.md §8)",
     true},
    {"members", "Who is in, and where is that list kept?", "hol_membership",
     "a small database of its own beside the .miga (store: local-file)", false},
    {"presence", "Who else is here right now, and on what?", "hol_lan_share",
     "a beacon on the local network, sealed to the room key", false},
    {"private", "What never leaves this device?", "hol_lan_share",
     "runes carrying one of its private_tags, withheld at every share and sync", false},
};

/* Twelve colours that stay distinct on both themes; presence assigns from these
 * first (lan-sharing.md §6). A profile's first colour comes from here too. */
inline constexpr const char* kPalette[12] = {"#e0555a", "#e8903a", "#d6b52e", "#5bb05b",
                                             "#2fa4a0", "#3c8fd6", "#6a6ee0", "#9a5bd6",
                                             "#d65bb0", "#8a6a4a", "#5a7a8a", "#c0506e"};

inline constexpr int kShareStreamPort = 47733;  // joining (lan-sharing.md §2)
inline constexpr int kMemberSyncPort = 47734;   // members keeping in sync (§3b)

/* ── the Antfarm's answers, read once ───────────────────────────────────────── */

struct ShareSettings {
    std::string node;                        // "" = no hol_lan_share in the Antfarm
    bool allow = false;
    bool presence = true;
    int port = kShareStreamPort;
    std::string key_file = "lan-room.key";   // the room key, beside the .miga
    std::vector<std::string> private_tags{"private"};
    bool send_hosted = false;                // send files that are already online
};

struct MembershipSettings {
    std::string node;                        // "" = no hol_membership
    std::string store = "local-file";        // local-file | mantle | relay
    std::string file = "members.json";
    std::string default_role = "admin";      // admin | editor | viewer
    std::string precedence = "everyone-equal";  // everyone-equal | admins-first
};

inline bool yes(const std::string& v, bool fallback) {
    if (v.empty()) return fallback;
    return v == "yes" || v == "true" || v == "on" || v == "1";
}

inline ShareSettings share_settings(const maiz::Scene& farm) {
    ShareSettings s;
    for (const auto& n : farm.nodes) {
        if (n.glyph != "hol_lan_share") continue;
        s.node = n.name;
        s.allow = yes(field_value(n, "allow"), false);
        s.presence = yes(field_value(n, "presence"), true);
        if (const int p = std::atoi(field_value(n, "port").c_str()); p > 0 && p < 65536) s.port = p;
        if (const std::string k = field_value(n, "key_file"); !k.empty()) s.key_file = k;
        if (const std::string t = field_value(n, "private_tags"); !t.empty()) {
            s.private_tags.clear();
            std::istringstream ts(t);
            for (std::string w; ts >> w;) {
                while (!w.empty() && (w.back() == ',' || w.back() == ';')) w.pop_back();
                if (!w.empty()) s.private_tags.push_back(w);
            }
        }
        s.send_hosted = yes(field_value(n, "send_hosted"), false);
        break;  // one per database; the first answers
    }
    return s;
}

inline MembershipSettings membership_settings(const maiz::Scene& farm) {
    MembershipSettings m;
    for (const auto& n : farm.nodes) {
        if (n.glyph != "hol_membership") continue;
        m.node = n.name;
        if (const std::string v = field_value(n, "store"); !v.empty()) m.store = v;
        if (const std::string v = field_value(n, "file"); !v.empty()) m.file = v;
        if (const std::string v = field_value(n, "default_role"); !v.empty()) m.default_role = v;
        if (const std::string v = field_value(n, "precedence"); !v.empty()) m.precedence = v;
        break;
    }
    return m;
}

/* A rune that never leaves this device. By TAG, not by name-as-tag: a rune that
 * happens to be called "private" is not thereby private. */
inline bool is_private(const maiz::SceneNode& n, const ShareSettings& s) {
    for (const auto& t : n.tags)
        for (const auto& p : s.private_tags)
            if (t == p) return true;
    return false;
}

/* The commands that give a database the default collaboration bones. Used by
 * the new-database seed and by "Allow LAN sharing" on a database that has none. */
inline std::vector<std::string> default_nodes(bool with_share, bool with_members) {
    std::vector<std::string> c;
    if (with_share) {
        c.push_back("rune new hol_lan_share lan-share");
        c.push_back("set lan-share allow yes");
        c.push_back("set lan-share presence yes");
        c.push_back("set lan-share port " + std::to_string(kShareStreamPort));
        c.push_back("set lan-share key_file lan-room.key");
        c.push_back("set lan-share private_tags private");
        c.push_back("set lan-share send_hosted no");
        c.push_back("setjson lan-share pos [790,400]");
    }
    if (with_members) {
        c.push_back("rune new hol_membership members");
        c.push_back("set members store local-file");
        c.push_back("set members file members.json");
        c.push_back("set members default_role admin");
        c.push_back("set members precedence everyone-equal");
        c.push_back("setjson members pos [1120,400]");
    }
    return c;
}

/* ── one colour per person on screen (lan-sharing.md §6) ─────────────────────── */

struct Present {
    std::string key;        // the profile's fingerprint
    std::string preferred;  // "#rrggbb"
    std::string joined;     // ISO date it joined the database ("" sorts last)
};

inline int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

inline unsigned rgb_of(const std::string& hex) {
    if (hex.size() != 7 || hex[0] != '#') return 0x808080;
    unsigned v = 0;
    for (int i = 1; i < 7; ++i) v = v * 16 + (unsigned)hex_nibble(hex[i]);
    return v;
}

inline std::string hex_of(unsigned rgb) {
    static const char* d = "0123456789abcdef";
    std::string s = "#";
    for (int sh = 20; sh >= 0; sh -= 4) s += d[(rgb >> sh) & 15];
    return s;
}

inline int color_distance(unsigned a, unsigned b) {
    const int dr = (int)((a >> 16) & 255) - (int)((b >> 16) & 255);
    const int dg = (int)((a >> 8) & 255) - (int)((b >> 8) & 255);
    const int db = (int)(a & 255) - (int)(b & 255);
    return dr * dr + dg * dg + db * db;
}

/* A hue step by the golden angle, for the thirteenth person onward. Saturation
 * and brightness cycle too, because hue alone runs out of tellable colours at
 * around twenty. */
inline std::string golden_color(int i) {
    static constexpr double kSat[] = {0.55, 0.8, 0.35, 0.7};
    static constexpr double kVal[] = {0.85, 0.6, 0.95, 0.45};
    const double h = std::fmod(0.61803398875 * i, 1.0) * 6.0, s = kSat[i % 4], v = kVal[i % 4];
    const int k = (int)h;
    const double f = h - k, p = v * (1 - s), q = v * (1 - s * f), t = v * (1 - s * (1 - f));
    double r = v, g = t, b = p;
    switch (k % 6) {
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        case 5: r = v; g = p; b = q; break;
        default: break;
    }
    return hex_of(((unsigned)(r * 255) << 16) | ((unsigned)(g * 255) << 8) | (unsigned)(b * 255));
}

/* Deterministic: every device reaches the same answer without talking about it.
 * Ordered by when each joined (then by key); each takes its preference unless an
 * earlier person holds it, else the nearest free palette colour, else a golden-
 * angle hue. Returns key -> the colour to SHOW. */
inline std::map<std::string, std::string> assign_colors(std::vector<Present> people) {
    std::sort(people.begin(), people.end(), [](const Present& a, const Present& b) {
        const std::string ja = a.joined.empty() ? "~" : a.joined;
        const std::string jb = b.joined.empty() ? "~" : b.joined;
        return ja == jb ? a.key < b.key : ja < jb;
    });
    std::map<std::string, std::string> out;
    std::vector<unsigned> taken;
    int golden = 0;
    for (const auto& p : people) {
        const unsigned want = rgb_of(p.preferred);
        int near = 40;  // relaxed only when every nearby colour is taken
        auto clashes = [&](unsigned c) {
            for (unsigned t : taken)
                if (t == c || color_distance(t, c) < near * near) return true;
            return false;
        };
        unsigned pick = want;
        if (p.preferred.empty() || clashes(want)) {
            int best = -1, best_d = 0;
            for (int i = 0; i < 12; ++i) {
                const unsigned c = rgb_of(kPalette[i]);
                if (clashes(c)) continue;
                const int d = color_distance(c, want);
                if (best < 0 || d < best_d) best = i, best_d = d;
            }
            if (best >= 0) {
                pick = rgb_of(kPalette[best]);
            } else {
                /* Never the same colour twice: the distance a clash means shrinks
                 * until a colour fits, and at zero only an exact repeat clashes. */
                for (int tries = 0;; ++tries) {
                    pick = rgb_of(golden_color(++golden));
                    if (!clashes(pick)) break;
                    if (tries % 64 == 63) near = near > 0 ? near / 2 : 0;
                }
            }
        }
        taken.push_back(pick);
        out[p.key] = hex_of(pick);
    }
    return out;
}

}  // namespace hormiga::collab
