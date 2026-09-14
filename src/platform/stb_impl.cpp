/* stb_impl.cpp — the stb_image implementation, for the HEADLESS binary.
 *
 * In the GUI, main/desktop.cpp compiles this; the CLI does not link that file,
 * so without this unit the link fails on `stbi_load` / `stbi_image_free`, which
 * `ui/map.cpp` uses to decode cached tiles for the map PNG export.
 *
 * WHY ITS OWN FILE. `stb_image.h`'s implementation block sits OUTSIDE its
 * include guard, so a unit that defines the macro and then includes the header
 * a second time — which `app_internal.hpp` does, for the declarations —
 * compiles the whole implementation twice and fails with a wall of
 * redefinitions. One TU that includes nothing else is the only arrangement that
 * cannot go wrong.
 */
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
