/* domain/image_presets.hpp — "add a flier", not "add an image and remember
 * which tag makes it a flier".
 *
 * The author (2026-09-16): *"for the images, instead of just 'add new image'
 * there should be a preset like 'add flier' or 'add banner option' or something
 * (we can think of others latter)."*
 *
 * A PRESET IS AN IMAGE RUNE WITH A PURPOSE ALREADY SAID. It is not a new glyph:
 * a flier is still an `image`, in the same gallery, published and hosted the same
 * way. What a preset adds is the tags a block looks for, so the picture turns up
 * where it is meant to without a person knowing the query. The seed's flier grid
 * is `type:image AND flier`, so a flier preset that tagged anything else would
 * be a button that quietly did not work.
 *
 * Adding one picks the FILE first -- "add a flier" means a picture, not an empty
 * record -- and goes through `adopt_image`: copied into assets/, deduplicated by
 * content, put online when the Antfarm has an image host. A picture that is
 * already in the gallery gains the tags instead of a second rune.
 *
 * Pure data, so the Data tab's "+ New" row and its "+ New..." menu read the same
 * rows, and the next preset is one line here.
 */
#pragma once

namespace hormiga {

struct ImagePreset {
    const char* id;    // the rune name's stem when the file name gives none
    const char* label; // "+ Flier"
    const char* tags;  // space-separated, added with `+`
    const char* hint;  // the tooltip: where it will show up
};

inline constexpr ImagePreset kImagePresets[] = {
    {"flier", "Flier", "flier",
     "A flier for an event or program. Image grids that show fliers find it by "
     "the tag `flier`."},
    {"banner", "Banner", "banner",
     "A wide picture for the top of a page or a newsletter - a Hero block's "
     "banner. Tagged `banner` so it is easy to find in the gallery."},
};

} // namespace hormiga
