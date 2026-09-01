/* theme.hpp — what the website's stylesheet and script are, as an interface.
 *
 * These were `static` inside the one big renderer file, so "the site builder can
 * use the theme" was true by proximity. Now it is a declaration: `site.cpp`
 * asks for a stylesheet, `theme.cpp` decides what one is, and neither knows the
 * other's internals.
 *
 * Which is also the seam a second output would plug into. A print stylesheet, or
 * an AMP variant, is another implementation of this two-function interface
 * rather than an edit inside somebody else's function.
 */
#pragma once

#include "app/app_internal.hpp"

#include <string>

/* The full stylesheet for one organization's theme: the computed half (contrast
 * tokens derived host-side from its colours) followed by the static half
 * (render/web/style.css, embedded at build time). */
std::string site_css(const SiteTheme& t);

/* The page script: lightbox, card filter, scroll-reveal, the map and calendar
 * widgets. Vanilla, no framework, no external load — the CSP-clean,
 * deploy-anywhere path. */
std::string site_js();
