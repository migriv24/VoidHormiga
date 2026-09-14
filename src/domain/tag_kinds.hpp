/* domain/tag_kinds.hpp — which KIND of tag this is, so it can be drawn as one.
 *
 * The author (2026-09-13): *"there are colon tags, tags like 'something:state'
 * — one is the clearance tag. I understand these are more special tags that
 * could have some data significance. However, there should still be easier ways
 * to assign them. For clearance specifically … they shouldn't be so hidden in
 * the GUI. They should have a different color though. Also suggested tags should
 * be a different color as well, because it gets confusing."*
 *
 * They were not merely quiet. The tag editor skipped EVERY tag containing a
 * colon (`if (t.find(':') != npos) continue;`) and the tag picker's vocabulary
 * skipped them too, so `clearance:public` — the tag that decides whether a
 * person's name reaches a public website — could be neither seen nor set from
 * the pane that shows that person. Hiding the most consequential tag in the
 * database from the screen where the decision is made is backwards: it is
 * exactly the tag a reviewer most needs to see at a glance.
 *
 * The fix is to show them all and give each kind its own colour, which needs a
 * single answer to "what kind is this?" — this file. Pure, and tested in
 * `tests/niche_smoke.cpp`.
 */
#pragma once

#include <string_view>

namespace hormiga::tagkind {

enum class Kind {
    Plain,        // `volunteer`         — a word a person chose
    Namespaced,   // `kw:food`, `lang:es` — a value on an axis
    Clearance,    // `clearance:public`  — decides what may be PUBLISHED
    Housekeeping, // `type:event`        — the glyph restated; every rune has one
};

inline std::string_view ns_of(std::string_view tag) {
    const size_t c = tag.find(':');
    return c == std::string_view::npos ? std::string_view{} : tag.substr(0, c);
}

inline std::string_view value_of(std::string_view tag) {
    const size_t c = tag.find(':');
    return c == std::string_view::npos ? tag : tag.substr(c + 1);
}

inline Kind classify(std::string_view tag) {
    const std::string_view ns = ns_of(tag);
    if (ns.empty()) return Kind::Plain;
    if (ns == "clearance") return Kind::Clearance;
    if (ns == "type") return Kind::Housekeeping;
    return Kind::Namespaced;
}

/* The two clearances the publishing seam reads (render/published.hpp): `public`
 * lets a contact or organization reach a page at all, and `contact` — a second,
 * independent consent — releases their email and phone. Listed here so the GUI
 * offers exactly these as switches rather than a free-text box where a typo
 * would silently publish nothing, or silently keep someone private. */
struct ClearanceOption {
    const char* tag;
    const char* label;
    const char* help;
};

inline constexpr ClearanceOption kClearances[] = {
    {"clearance:public", "Public",
     "may appear on the published website and in the live directory"},
    {"clearance:contact", "Contact details",
     "ALSO releases email and phone - a second consent, only meaningful with Public"},
};

} // namespace hormiga::tagkind
