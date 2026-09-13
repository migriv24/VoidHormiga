/* icon_smoke.cpp — the one icon vocabulary, checked against the icons that
 * actually ship.
 *
 * `render/icon_set.hpp` names the icons an author can pick in the Builder, and
 * each name has to mean something in every output. The website draws the name
 * as a Lucide SVG out of `vendor/icons/lucide_icons.hpp`, and `icons::svg`
 * returns NOTHING for a name it does not carry — deliberately, so a missing icon
 * costs a page decoration rather than showing a broken box.
 *
 * That kindness is exactly what makes a typo invisible: an icon chosen in the
 * app, drawn correctly in the app, and silently absent from a page a stranger
 * reads. This is the check that it cannot happen. Links nothing.
 */
#include "../src/render/icon_set.hpp"
#include "lucide_icons.hpp"

#include <cstring>
#include <iostream>
#include <set>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            ++failures;                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                          \
    } while (0)

int main() {
    std::set<std::string> seen;
    for (const auto& e : hormiga::iconset::kIcons) {
        CHECK(seen.insert(e.name).second); // no name listed twice
        const bool on_web = !hormiga::icons::body(e.name).empty();
        CHECK(on_web);
        if (!on_web)
            std::cerr << "  the Builder offers \"" << e.name
                      << "\" but the website has no Lucide icon by that name\n";
        CHECK(std::strlen(e.emoji) > 0); // the newsletter has something to show
        CHECK(std::strlen(e.label) > 0); // the picker has something to say
    }
    CHECK(seen.size() >= 30);

    // an unknown or empty name renders as nothing, never as a placeholder
    CHECK(std::string(hormiga::iconset::emoji("no-such-icon")).empty());
    CHECK(std::string(hormiga::iconset::emoji("")).empty());
    CHECK(std::string(hormiga::iconset::label("")).empty());
    CHECK(hormiga::iconset::find("map-pin") != nullptr);

    if (failures == 0) std::cout << "icon smoke: ok (" << seen.size() << " icons)\n";
    else std::cerr << "icon smoke: " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
