/* mercator.hpp — MOVED (2026-08-21).
 *
 * The web-mercator tile math now lives in `src/gis/projection.hpp`, alongside a
 * flat projection for authored worlds, because a projection is a property of the
 * SOURCE rather than a constant of the program (okf/concepts/foundation/application-boundaries.md,
 * Q42). `src/gis/` is the map engine and may depend on nothing, which is what
 * keeps it liftable.
 *
 * This header stays as a forward so the two callers that had it did not have to
 * change in the same commit that moved it — a move and an edit in one step is
 * how you lose the ability to say "nothing changed". It should go once they are
 * updated; the golden render is the proof either way.
 */
#pragma once

#include "gis/projection.hpp"

namespace hormiga {
using gis::kPi;
using gis::merc_x;
using gis::merc_y;
using gis::merc_lon;
using gis::merc_lat;
} // namespace hormiga
