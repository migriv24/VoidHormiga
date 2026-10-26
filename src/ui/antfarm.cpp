/* ui/antfarm.cpp — the Antfarm's tab.
 *
 * ONE ANTFARM (the author, 2026-10-19): "we don't want the user to be able to
 * choose to use the old antfarm. rather, we just wanna convert the old to the
 * new." The tab draws the Antfarm (ui/farm.cpp, the `farm` mantle) and nothing
 * else; a database whose graph is still in the format before 2026-09-28 is
 * converted when it opens (convert_antfarm, app/farm_app.cpp). The old node
 * graph's screen, its Try/Back buttons and its warning were removed: a choice
 * labelled "v1" reads as the safe, stable one, and the safe one is this.
 * okf/concepts/platform/antfarm/index.md. */
#include "app/app_internal.hpp"

void HormigaApp::draw_antfarm_section() { draw_farm_section(); }
