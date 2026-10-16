/* domain/naming.hpp — a rune's NAME follows its TITLE (2026-10-15).
 *
 * The author: *"i'd like for the 'name' and the 'title' of a rune to be the same
 * if possible. If an event is created with a randomly generated name, then the
 * event is given a name, I believe that they should closely match ... if
 * multiple events were just titled 'birthday' then the actual name should at
 * least have the title in it, plus something to make it unique. the name reacts
 * to the title."* And, on why it matters: the "linked to" search finds runes by
 * title, the calendar export prints titles, and both read wrong the moment the
 * handle and the title drift apart.
 *
 * TWO THINGS, KEPT APART ON PURPOSE. A rune's NAME is its handle: a
 * command-safe slug, what links, the CLI and the transcript call it. Its TITLE
 * is prose a person wrote, with capitals, accents and spaces. They cannot be one
 * field. What this file makes true is that the first is DERIVED from the second
 * for as long as nobody has said otherwise:
 *
 *   - a name the application minted (`event-3`, `event-3fa9-2`) follows;
 *   - a name that is the slug of the old title (`birthday`, `birthday-2`)
 *     follows, because it was derived from that title in the first place;
 *   - a rune with no title yet is read under its humanized name, so an imported
 *     `food-drive` whose title becomes "Food Drive 2026" follows too;
 *   - a name somebody typed by hand that is NOT its title's slug is theirs, and
 *     stays (the author's choice, 2026-10-15: "while it still matches").
 *
 * WHICH RUNES. Data a person names: contacts, organizations, events, jobs,
 * notes, map shapes, and every kind a database made for itself. Not view runes
 * (a map layer, a canvas, a saved calendar view), whose names are also keys in
 * view config that a rename does not repoint; not reference points, whose name
 * is held as a VALUE in their children's `ref` field; not blocks.
 *
 * Pure: a scene in, strings out. `app.cpp`'s `dispatch_and_reproject` asks it
 * about every single `set` that reaches the dispatcher, so the desktop, the
 * phone and the Data tab cannot disagree, and a title edit and the rename it
 * implies land as ONE undo frame.
 */
#pragma once

#include "domain/kinds.hpp"       // own kinds name themselves by `title`
#include "domain/scene_value.hpp" // temper::field_value

#include "voidmaiz/scene.hpp"

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace hormiga::naming {

/* The field a person reads as this kind of rune's title, or "" when a title
 * edit should never move its name (see WHICH RUNES above). */
inline std::string title_field(const std::string& glyph) {
    if (glyph == "contact" || glyph == "organization") return "display_name";
    if (glyph == "event" || glyph == "job") return "title_en";
    if (glyph == "note") return "title";
    if (glyph == "mapshape") return "label";
    const kinds::Kind* k = kinds::current().find(glyph);
    if (k && !k->builtin) return "title"; // a database's own kind (and legacy `product`)
    return {};
}

/* The plain-ASCII letter for a UTF-8 Latin-1 letter (U+00C0..U+00FF), so
 * "José García" slugs to `jose-garcia`, not `jos-garc-a`. "" for anything else
 * in that block (×, ÷), which then reads as a separator. */
inline const char* fold_latin1(unsigned char second) {
    static const char* const tbl[64] = {
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i", // C0-CF
        "d", "n", "o", "o", "o", "o", "o", "",   "o", "u", "u", "u", "u", "y", "th", "ss", // D0-DF
        "a", "a", "a", "a", "a", "a", "ae", "c", "e", "e", "e", "e", "i", "i", "i", "i", // E0-EF
        "d", "n", "o", "o", "o", "o", "o", "",   "o", "u", "u", "u", "u", "y", "th", "y", // F0-FF
    };
    return (second >= 0x80 && second <= 0xBF) ? tbl[second - 0x80] : "";
}

/* A title as a rune name: lowercase ASCII letters and digits, single hyphens,
 * capped (an iCalendar SUMMARY can be a paragraph). Same shape as
 * `quick::slug`, plus accent folding. */
inline std::string slug(std::string_view title, size_t cap = 48) {
    std::string s;
    auto sep = [&] { if (!s.empty() && s.back() != '-') s += '-'; };
    for (size_t i = 0; i < title.size(); ++i) {
        const unsigned char u = (unsigned char)title[i];
        if (u == 0x27) continue; // an apostrophe: "Garfield's" is one word, `garfields`
        if (u < 0x80) {
            if (std::isalnum(u)) s += (char)std::tolower(u);
            else sep();
        } else if (u == 0xE2 && i + 2 < title.size() && (unsigned char)title[i + 1] == 0x80 &&
                   (unsigned char)title[i + 2] == 0x99) {
            i += 2; // the typographic apostrophe, likewise
        } else if (u == 0xC3 && i + 1 < title.size()) {
            const char* f = fold_latin1((unsigned char)title[++i]);
            if (*f) s += f;
            else sep();
        } else {
            // any other multi-byte letter: skip its continuation bytes, keep a seam
            while (i + 1 < title.size() && ((unsigned char)title[i + 1] & 0xC0) == 0x80) ++i;
            sep();
        }
    }
    while (!s.empty() && s.back() == '-') s.pop_back();
    if (s.size() > cap) s.resize(cap);
    while (!s.empty() && s.back() == '-') s.pop_back();
    return s;
}

/* WHAT A TITLE MAY HOLD (2026-10-15, Q112). The author: *"names and titles
 * should be a bit more strict on what characters can go in them. However
 * descriptions and summaries should be more loose."* A name is a slug already
 * (`slug` above). A title is one line a person reads on a card, a calendar or a
 * map, so it keeps every letter in every script, digits, spaces and ordinary
 * punctuation, and loses:
 *   - line breaks and other control characters (a title is one line): a space;
 *   - the backslash and < > { } [ ] | ` ^ ~, which are markup or escapes,
 *     never prose;
 *   - the straight `"`, which becomes a typographic one (“ at the start of a
 *     word, ” at the end), so a quoted word survives as a quoted word;
 *   - runs of spaces, the ends' spaces, and anything past 120 characters.
 * Descriptions and summaries are not cleaned at all: they take any character,
 * now that a `set` keeps them whole (`text_arg`). */
inline std::string clean_title(std::string_view in, size_t cap = 120) {
    static constexpr std::string_view kDrop = "\\<>{}[]|`^~";
    std::string out;
    size_t chars = 0;
    for (size_t i = 0; i < in.size() && chars < cap; ++i) {
        const unsigned char u = (unsigned char)in[i];
        if (u <= 0x20 || u == 0x7F) { // a space, a line break, a control character: one space
            if (!out.empty() && out.back() != ' ') out += ' ', ++chars;
            continue;
        }
        if (kDrop.find((char)u) != std::string_view::npos) continue;
        if (u == '"') { // typographic: opening at a word's start, closing at its end
            const bool opens = out.empty() || out.back() == ' ' || out.back() == '(';
            out += opens ? "\xE2\x80\x9C" : "\xE2\x80\x9D";
            ++chars;
            continue;
        }
        out += (char)u;
        ++chars;
        while (i + 1 < in.size() && ((unsigned char)in[i + 1] & 0xC0) == 0x80) out += in[++i]; // the rest of it
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

/* `base`, or `base-2`, `base-3`, ... — the first that no OTHER rune holds.
 * `self` is the rune being renamed (its own name is never "taken" by it). In a
 * shared database `device_tag` is non-empty and the name carries it
 * (`birthday-3fa9`, then `birthday-3fa9-2`): two members naming two events
 * "birthday" before they sync must not mint one name (Q74; mint_name does the
 * same). The counter only appears when it is needed. */
inline std::string unique(const maiz::Scene& s, const std::string& base, const std::string& self = {},
                          const std::string& device_tag = {}) {
    auto free = [&](const std::string& n) { return n == self || !s.find(n); };
    if (device_tag.empty()) {
        if (free(base)) return base;
        for (int i = 2;; ++i)
            if (std::string n = base + "-" + std::to_string(i); free(n)) return n;
    }
    if (std::string n = base + "-" + device_tag; free(n)) return n;
    for (int i = 2;; ++i)
        if (std::string n = base + "-" + device_tag + "-" + std::to_string(i); free(n)) return n;
}

/* Is `name` exactly `stem`, optionally followed by `-<4 hex>` (a device tag)
 * and/or `-<digits>` (a uniquifier)? */
inline bool stem_of(std::string_view name, std::string_view stem) {
    if (stem.empty() || name.substr(0, stem.size()) != stem) return false;
    std::string_view rest = name.substr(stem.size());
    auto all = [](std::string_view v, auto pred) {
        if (v.empty()) return false;
        for (char c : v)
            if (!pred((unsigned char)c)) return false;
        return true;
    };
    auto is_hex = [](unsigned char c) { return std::isdigit(c) || (c >= 'a' && c <= 'f'); };
    auto is_dig = [](unsigned char c) { return std::isdigit(c) != 0; };
    if (rest.empty()) return true;
    if (rest[0] != '-') return false;
    rest.remove_prefix(1);
    // a device tag, alone (`unique`) or with a counter (`mint_name`). A typed
    // word of four hex letters (`-cafe`) reads as one too; the cost is only
    // that such a name follows its title like a derived one would
    if (rest.size() == 4 && all(rest, is_hex)) return true;
    if (rest.size() >= 6 && rest[4] == '-' && all(rest.substr(0, 4), is_hex)) rest.remove_prefix(5);
    return all(rest, is_dig);
}

/* Was this name minted by the application, rather than said by anyone? The
 * stems `mint_name` is called with for these glyphs. */
inline bool minted(const maiz::SceneNode& n) {
    if (stem_of(n.name, n.glyph)) return true;
    if (n.glyph == "mapshape" && stem_of(n.name, "shape")) return true;
    return false;
}

/* Does this rune's name still follow its title? (see the header) */
inline bool follows(const maiz::SceneNode& n) {
    const std::string f = title_field(n.glyph);
    if (f.empty()) return false;
    if (minted(n)) return true;
    const std::string old = temper::field_value(n, f.c_str());
    if (old.empty()) return true; // no title yet: the name IS what it is called
    return stem_of(n.name, slug(old));
}

/* The name a rune should have after its title becomes `title`, or "" when it
 * should keep the one it has: the title field is not its title, the name no
 * longer follows, the new title slugs to nothing, or the name already fits. */
inline std::string follow(const maiz::Scene& s, const maiz::SceneNode& n, const std::string& field,
                          const std::string& title, const std::string& device_tag = {}) {
    if (field.empty() || field != title_field(n.glyph) || !follows(n)) return {};
    const std::string base = slug(title);
    if (base.empty() || stem_of(n.name, base)) return {};
    return unique(s, base, n.name, device_tag);
}

/* WHAT A RENAME DOES NOT REPOINT, AND WHO DOES (2026-10-15). `rune rename`
 * repoints links; it cannot know that a VALUE is a name. The Builder's
 * featured-event blocks (`event_feature`, `event_flier`) hold their event's
 * name in `event`, in a document's mantle, so renaming the event left the block
 * pointing at nothing. Measured before this was written: a block still read
 * `"event": "fair"` after `rune rename fair spring-fair`. A hand rename had
 * always had that hole; names that follow titles make renames frequent enough
 * that it would be found within the week. The `set`s here go in the SAME batch
 * as the rename (with `use` around them), so one undo takes back all of it.
 * The other name-valued fields point at images, resources and reference
 * points, none of which follows a title. */
inline std::vector<std::string> block_repoints(const maiz::Scene& doc, const std::string& from,
                                               const std::string& to) {
    std::vector<std::string> out;
    for (const auto& n : doc.nodes)
        if (temper::field_value(n, "event") == from) out.push_back("set " + n.name + " event \"" + to + "\"");
    return out;
}

} // namespace hormiga::naming
