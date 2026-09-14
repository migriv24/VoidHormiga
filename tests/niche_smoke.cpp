/* niche_smoke.cpp — the pure halves of the 2026-09-13 niche-tools pass.
 *
 * 1. THE QR RASTER (domain/qr.hpp) over the vendored Nayuki encoder. The encoder
 *    is upstream's and well tested there; what is checked here is OURS — the
 *    quiet-zone border, the row-major grid, the pixel buffer — plus the one
 *    property that would make a QR tool dangerous rather than useless: an empty
 *    or oversized input is REFUSED, never quietly turned into a code that scans
 *    to nothing.
 *
 * 2. THE TAG KINDS (domain/tag_kinds.hpp) that decide each chip's colour. The
 *    case that matters is `clearance:`, which the tag editor used to hide and
 *    which now must be told apart from every other namespaced tag.
 *
 * Links only the vendored encoder. No window, no Void Core.
 */
#include "../src/domain/qr.hpp"
#include "../src/domain/tag_kinds.hpp"

#include <iostream>
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
    using namespace hormiga;

    // ── 1. QR ───────────────────────────────────────────────────────────────
    {
        // 19 bytes of lowercase text is byte mode, which fits version 2 (25x25)
        // at MEDIUM; with the default 4-module quiet zone, 33 per side
        const qr::Matrix m = qr::make("https://example.org");
        CHECK(m.error.empty());
        CHECK(m.size == 33);
        CHECK(m.dark.size() == 33u * 33u);
        // the quiet zone is light all the way round
        bool border_light = true;
        for (int i = 0; i < m.size; ++i)
            for (int b = 0; b < 4; ++b)
                if (m.at(i, b) || m.at(b, i) || m.at(i, m.size - 1 - b) ||
                    m.at(m.size - 1 - b, i))
                    border_light = false;
        CHECK(border_light);
        // the top-left finder pattern: dark ring, light ring, dark centre
        CHECK(m.at(4, 4));
        CHECK(!m.at(5, 5));
        CHECK(m.at(7, 7));
        // deterministic: the same text is the same picture
        CHECK(qr::make("https://example.org").dark == m.dark);
        // the pixel buffer: 4px per module, white border, black finder corner
        const auto px = qr::to_rgb(m, 4);
        CHECK(px.size() == (size_t)(33 * 4) * (size_t)(33 * 4) * 3);
        CHECK(px[0] == 255);                         // (0,0) is quiet zone
        const size_t corner = ((size_t)(16 * 132 + 16)) * 3; // module (4,4)
        CHECK(px[corner] == 0);
        // a border of 0 is honoured
        CHECK(qr::make("https://example.org", qr::Ecc::Medium, 0).size == 25);
    }
    // REFUSALS: a code that opens nothing is worse than no code
    CHECK(!qr::make("").error.empty());
    CHECK(qr::make("").size == 0);
    {
        const std::string huge(5000, 'x'); // past the largest version's capacity
        const qr::Matrix m = qr::make(huge, qr::Ecc::High);
        CHECK(!m.error.empty());
        CHECK(m.size == 0);
    }

    // ── 2. tag kinds ────────────────────────────────────────────────────────
    using tagkind::Kind;
    CHECK(tagkind::classify("volunteer") == Kind::Plain);
    CHECK(tagkind::classify("kw:food") == Kind::Namespaced);
    CHECK(tagkind::classify("lang:es") == Kind::Namespaced);
    CHECK(tagkind::classify("clearance:public") == Kind::Clearance);
    CHECK(tagkind::classify("clearance:contact") == Kind::Clearance);
    CHECK(tagkind::classify("type:event") == Kind::Housekeeping);
    CHECK(tagkind::ns_of("kw:food") == "kw");
    CHECK(tagkind::value_of("kw:food") == "food");
    CHECK(tagkind::value_of("volunteer") == "volunteer");
    CHECK(tagkind::ns_of("volunteer").empty());
    // the switches the GUI offers are exactly the two the publishing seam reads
    CHECK(sizeof tagkind::kClearances / sizeof tagkind::kClearances[0] == 2);
    CHECK(std::string(tagkind::kClearances[0].tag) == "clearance:public");
    CHECK(std::string(tagkind::kClearances[1].tag) == "clearance:contact");

    if (failures == 0) std::cout << "niche smoke: ok\n";
    else std::cerr << "niche smoke: " << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}
