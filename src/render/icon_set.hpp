/* render/icon_set.hpp — the ONE icon vocabulary an author picks from.
 *
 * The author (2026-09-13): *"easier icon selection for narrative bits, and
 * giving them some sort of icons … also icons are useful with these! remember
 * icons!"*
 *
 * Hormiga already draws icons three ways, one per medium, and each is right for
 * its medium:
 *
 *   the app     Font Awesome, merged into the ImGui atlas (app_internal.hpp)
 *   the website Lucide, as inline SVG (vendor/icons/lucide_icons.hpp)
 *   the email   EMOJI — Gmail strips inline SVG and no mail client loads an
 *               icon font, so a picture that is also text is the only icon
 *               that survives every inbox
 *
 * What was missing was a single list of NAMES tying the three together, so a
 * choice made once in the Builder means the same thing in every output. The
 * name IS the Lucide name — the website already calls `icons::svg("map-pin")`
 * — and `tests/icon_smoke.cpp` proves every name here exists in the vendored
 * Lucide set, so an icon cannot look fine in the app and render as nothing on a
 * page a stranger is reading.
 *
 * Pure: no ImGui, no HTML. The Font Awesome half lives in `fa_icon_for()`
 * because a codepoint macro belongs with the font it indexes.
 */
#pragma once

#include <string_view>

namespace hormiga::iconset {

struct Entry {
    const char* name;  // the Lucide name, and the value stored on a rune
    const char* label; // what the picker's tooltip says
    const char* emoji; // the email rendering (UTF-8)
};

inline constexpr Entry kIcons[] = {
    {"house", "Home", "\xF0\x9F\x8F\xA0"},
    {"user", "Person", "\xF0\x9F\x91\xA4"},
    {"users", "People", "\xF0\x9F\x91\xA5"},
    {"star", "Star", "\xE2\xAD\x90"},
    {"heart", "Heart", "\xE2\x9D\xA4\xEF\xB8\x8F"},
    {"triangle-alert", "Alert", "\xE2\x9A\xA0\xEF\xB8\x8F"},
    {"graduation-cap", "School", "\xF0\x9F\x8E\x93"},
    {"stethoscope", "Health", "\xF0\x9F\xA9\xBA"},
    {"utensils", "Food", "\xF0\x9F\x8D\xBD\xEF\xB8\x8F"},
    {"bus", "Transit", "\xF0\x9F\x9A\x8C"},
    {"briefcase", "Work", "\xF0\x9F\x92\xBC"},
    {"calendar", "Date", "\xF0\x9F\x93\x85"},
    {"clock", "Time", "\xF0\x9F\x95\x92"},
    {"scale", "Rights / legal", "\xE2\x9A\x96\xEF\xB8\x8F"},
    {"map-pin", "Place", "\xF0\x9F\x93\x8D"},
    {"phone", "Phone", "\xF0\x9F\x93\x9E"},
    {"mail", "Email", "\xE2\x9C\x89\xEF\xB8\x8F"},
    {"globe", "Website", "\xF0\x9F\x8C\x90"},
    {"languages", "Language", "\xF0\x9F\x97\xA3\xEF\xB8\x8F"},
    {"link", "Link", "\xF0\x9F\x94\x97"},
    {"image", "Photo", "\xF0\x9F\x96\xBC\xEF\xB8\x8F"},
    {"video", "Video", "\xF0\x9F\x8E\xAC"},
    {"music", "Music", "\xF0\x9F\x8E\xB5"},
    {"download", "Download", "\xE2\xAC\x87\xEF\xB8\x8F"},
    {"check", "Done", "\xE2\x9C\x85"},
    {"info", "Info", "\xE2\x84\xB9\xEF\xB8\x8F"},
    {"ticket", "Ticket", "\xF0\x9F\x8E\x9F\xEF\xB8\x8F"},
    {"megaphone", "Announcement", "\xF0\x9F\x93\xA3"},
    {"hand-heart", "Care", "\xF0\x9F\xA4\xB2"},
    {"book-open", "Reading", "\xF0\x9F\x93\x96"},
    {"baby", "Children", "\xF0\x9F\x91\xB6"},
    {"circle-dollar-sign", "Money", "\xF0\x9F\x92\xB2"},
    {"building-2", "Organization", "\xF0\x9F\x8F\xA2"},
    {"file-text", "Document", "\xF0\x9F\x93\x84"},
    {"search", "Search", "\xF0\x9F\x94\x8D"},
    {"party-popper", "Celebration", "\xF0\x9F\x8E\x89"},
};

inline const Entry* find(std::string_view name) {
    for (const Entry& e : kIcons)
        if (name == e.name) return &e;
    return nullptr;
}

/* "" for an unknown or empty name, never a placeholder: a missing icon should
 * cost a newsletter a little decoration, not print a question mark at a reader. */
inline const char* emoji(std::string_view name) {
    const Entry* e = find(name);
    return e ? e->emoji : "";
}

inline const char* label(std::string_view name) {
    const Entry* e = find(name);
    return e ? e->label : "";
}

} // namespace hormiga::iconset
