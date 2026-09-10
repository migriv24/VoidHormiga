/* published.hpp — THE PUBLISHED SUBSET, as a type.
 *
 * ── WHY THIS EXISTS ──────────────────────────────────────────────────────────
 *
 * The clearance gate — "a rune reaches a page only if it carries
 * `clearance:public`, and a second, independent `clearance:contact` releases the
 * email and phone" — lived inside `render_site`'s `directory` block, as a loop
 * that also emitted HTML. That was fine while HTML was the only thing the
 * published subset could become.
 *
 * It is about to stop being the only thing. `okf/concepts/platform/web-platform.md` §7.2
 * and `okf/concepts/platform/data-planes.md` describe a **second output format** for the
 * same subset — rows a website can query live, so that changing a contact does
 * not mean redeploying a site:
 *
 *     the published subset  ->  HTML files   (render_site)
 *                           ->  index rows   (publish_index)
 *
 * The whole safety argument for that feature is the word **same**. A second
 * exporter that re-implemented the gate would be a second privacy surface, and
 * it would drift — silently, because a website that shows too much still looks
 * correct. CLAUDE.md rule 6 says an internal-notes-class field must never reach
 * an Output holiday and that the check must be testable; two copies of a check
 * are not testable, they are a diff nobody runs.
 *
 * So the gate is here, once, and both renderers call it. "Same seam" becomes a
 * fact about the call graph rather than a claim in a document.
 *
 * ── THE SHAPE IS THE ENFORCEMENT ─────────────────────────────────────────────
 *
 * `Person` carries exactly the fields that may leave, and **`notes` is not one
 * of them** — not blanked, not guarded, ABSENT. A field that does not exist on
 * the type cannot be leaked by a future caller who forgets, and cannot be added
 * by accident: adding one is an edit to this struct, in this file, under this
 * comment. That is the difference between a rule and a wish, and it is the same
 * reasoning `data-planes.md` §2 applies one layer out to the cloud store.
 *
 * Headless and view-free: Scene in, values out, no ImGui, no HTML. Testable
 * without a window, which is what makes rule 6's "testable at the seam" true.
 */
#pragma once

#include "domain/date_query.hpp"  // date: predicates in the block grammar
#include "domain/scene_value.hpp" // the ONE field reader
#include "render/text.hpp"        // display_name
#include "voidmaiz/scene.hpp"

#include <functional>
#include <string>
#include <vector>

namespace hormiga::published {

/* One published person or organization.
 *
 * `name` is the rune name and is the STABLE ID a live index needs — a row must
 * be addressable across publishes so that updating a contact updates a row
 * rather than appending one. It is a command-safe slug and is already public in
 * every URL the site emits, so it discloses nothing `display` does not. */
struct Person {
    std::string name;    // the rune's name: the stable id
    std::string glyph;   // "contact" | "organization"
    std::string display; // display_name, or the humanized slug
    std::string role;    // contact: role. organization: abbreviation
    std::string place;   // organization: location
    std::string bio;
    std::string website; // contact: website. organization: url
    std::string avatar;  // the raw field; STAGING an asset is the caller's job
    /* Released only by the second, independent consent (`clearance:contact`).
     * Empty otherwise, and empty is the truth rather than a mask: the gate
     * never reads them for a rune that has not consented. */
    std::string email;
    std::string phone;
    std::vector<std::string> tags;
};

struct Directory {
    std::vector<Person> people;
    /* How many runes MATCHED the query and were withheld for want of
     * `clearance:public`. Reported to the operator, because a directory that
     * comes out empty because nobody has been tagged is indistinguishable from
     * a broken block, and the person who has to fix it is the one reading the
     * log. */
    int withheld = 0;
};

/* THE GATE. `hidden` is the Allomone `web-hide` predicate, passed in because
 * this header must not know about `HormigaApp`.
 *
 * `kind` is "contact", "organization", or empty for both. `query` is a tag
 * expression; an empty query matches everything, and **the query is not
 * trusted** — no query, field or flag turns off `clearance:public`. That
 * inversion is deliberate: `web-hide` is a subtraction from a default of
 * publishing, which is right for an event and wrong for a person. */
/* `lang` is the page being built. A directory publishes an organization's own
 * PROSE — a bio, a project description — and until 2026-09-03 that prose was
 * the one text on a bilingual site that could not be bilingual, because the
 * data glyphs had no `_es` sibling. See `lang_text` in render/text.hpp. */
inline Directory directory(const maiz::Scene& data, const std::string& query,
                           const std::string& kind,
                           const std::function<bool(const std::string&)>& hidden,
                           std::string_view lang = "en") {
    Directory out;
    for (const auto& dn : data.nodes) {
        const bool is_c = dn.glyph == "contact";
        const bool is_o = dn.glyph == "organization";
        if (!is_c && !is_o) continue;
        if (kind == "contact" && !is_c) continue;
        if (kind == "organization" && !is_o) continue;
        /* The block's own query goes through the DATE-AWARE matcher (the
         * grammar plus `date:` — see domain/date_query.hpp); the two clearance
         * checks below deliberately do not, because a consent tag is a fact
         * about a rune and must never depend on what day it is. */
        if (!query.empty() && !hormiga::query_matches(query, data, dn)) continue;
        if (hidden && hidden(dn.name)) continue;
        if (!maiz::node_matches("clearance:public", dn)) {
            ++out.withheld;
            continue;
        }
        Person p;
        p.name = dn.name;
        p.glyph = dn.glyph;
        p.display = display_name(dn);
        p.role = is_c ? hormiga::temper::field_value(dn, "role")
                      : hormiga::temper::field_value(dn, "abbreviation");
        p.place = is_o ? hormiga::temper::field_value(dn, "location") : std::string();
        p.bio = lang_text(dn, "bio", lang);
        p.website = is_c ? hormiga::temper::field_value(dn, "website")
                         : hormiga::temper::field_value(dn, "url");
        p.avatar = hormiga::temper::field_value(dn, "avatar");
        if (p.avatar.empty())
            p.avatar = hormiga::temper::field_value(dn, "image_url");
        /* THE SECOND CONSENT, and the only place these two fields are read. An
         * organization's `email` waits for the tag too — a small mutual-aid
         * group's "contact email" is very often one volunteer's personal inbox,
         * and treating it as a front desk by default is how that gets published
         * by someone who never decided to. */
        if (maiz::node_matches("clearance:contact", dn)) {
            p.email = hormiga::temper::field_value(dn, "email");
            p.phone = hormiga::temper::field_value(dn, "phone");
        }
        p.tags = dn.tags;
        out.people.push_back(std::move(p));
    }
    std::stable_sort(out.people.begin(), out.people.end(),
                     [](const Person& a, const Person& b) {
                         return a.display < b.display;
                     });
    return out;
}

} // namespace hormiga::published
