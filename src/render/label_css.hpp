/* render/label_css.hpp — a map shape's label CSS, cleaned for the website.
 * Pure (std only), so a smoke test can hold it without the application. */
#pragma once

#include <cctype>
#include <string>

namespace hormiga {

/* A map shape's custom label CSS, as the website may carry it (2026-10-15).
 *
 * The author asked for HTML or CSS on a shape's label "as a hidden extra
 * setting". CSS only, and only declarations: the page applies it with
 * `style.cssText`, which cannot open an element or run a script, and this
 * removes what could still reach outside the page or the label: anything
 * that loads a resource (`url(`, `src(`, `image(`, `image-set(`, `@import`), the old IE
 * `expression(`, `javascript:`, and the characters that only matter to
 * markup. One bad part refuses the whole value; a half-applied style is
 * harder to understand than none. */
inline std::string shape_label_css(const std::string& css) {
    std::string low = css;
    for (char& c : low) c = (char)std::tolower((unsigned char)c);
    for (const char* bad : {"url(", "src(", "image(", "image-set(", "@import", "expression(", "javascript:",
                            "behavior:", "\\"})
        if (low.find(bad) != std::string::npos) return {};
    std::string out;
    for (char c : css)
        if (c != '<' && c != '>' && c != '"' && c != '{' && c != '}' && c != '\n' && c != '\r') out += c;
    return out.size() > 400 ? std::string() : out;
}

} // namespace hormiga
