/* render/image_text.hpp — a picture and the words that go with it.
 *
 * The author (2026-09-13): *"an easier way to make a narrative section where we
 * have like an image on one side, and the text on the other, like they go
 * together (remember to make this an explicit block we place on the document
 * editor, a preview would also be beneficial)."*
 *
 * The workaround this replaces was a row: an image block and a narrative block
 * dropped side by side. It looked right on the website and fell apart in the
 * newsletter, because until 2026-09-13 the email renderer ignored rows and
 * stacked the two — and even with rows working, two blocks are two things to
 * keep in step, where the author means ONE thing that has a picture in it.
 *
 * Markup only, the way `render/video.hpp` and `render/audio.hpp` hold theirs:
 * the renderers resolve the fields (so the glyph-field linter sees every one of
 * them read) and pass the results here. EVERY ARGUMENT ARRIVES ALREADY ESCAPED
 * OR ALREADY MARKUP — this file escapes nothing, so it can never double-escape
 * a heading, and never be the place a raw value reaches a page.
 */
#pragma once

#include <algorithm>
#include <string>

namespace hormiga {

/* The website: a two-column flex row that stacks on a phone (style.css,
 * `.imgtext`). `image_right` swaps the order so a page can alternate. */
inline std::string image_text_web(const std::string& src, const std::string& alt,
                                  const std::string& heading,
                                  const std::string& icon_svg,
                                  const std::string& text_html, bool image_right) {
    std::string o = "<div class=\"imgtext reveal";
    if (image_right) o += " right";
    if (src.empty()) o += " noimg";
    o += "\">\n";
    if (!src.empty())
        o += "<div class=\"imgtext-img\"><img loading=\"lazy\" src=\"" + src +
             "\" alt=\"" + alt + "\"></div>\n";
    o += "<div class=\"imgtext-body\">";
    if (!heading.empty())
        o += "<h3 class=\"prose-heading\">" + icon_svg + heading + "</h3>";
    else if (!icon_svg.empty())
        o += "<div class=\"prose-icon\">" + icon_svg + "</div>";
    if (!text_html.empty())
        o += "<p class=\"prose pre-line\">" + text_html + "</p>";
    o += "</div>\n</div>\n";
    return o;
}

/* The newsletter: a two-cell table, because a table is the only side-by-side
 * layout every mail client honours. `width_px` is the width this block may
 * occupy — 572 on its own, less inside a row — so the image's `width`
 * attribute, which Outlook obeys over any CSS, never overflows the column.
 *
 * `src` must be a PUBLIC url; with none, the block is the text alone rather
 * than a broken image, and the caller has already said why in the render log. */
inline std::string image_text_email(const std::string& src, const std::string& alt,
                                    const std::string& heading,
                                    const std::string& emoji,
                                    const std::string& text_html, bool image_right,
                                    int width_px) {
    const int img_px = std::max(80, width_px * 42 / 100);
    std::string img_td;
    if (!src.empty())
        img_td = std::string("<td valign=\"top\" width=\"42%\" style=\"vertical-align:top;") +
                 (image_right ? "padding:0 0 0 14px" : "padding:0 14px 0 0") +
                 "\"><img src=\"" + src + "\" alt=\"" + alt + "\" width=\"" +
                 std::to_string(img_px) +
                 "\" style=\"display:block;width:100%;max-width:" +
                 std::to_string(img_px) + "px;height:auto;border:0\"></td>";
    const std::string mark = emoji.empty() ? std::string() : emoji + " ";
    std::string text_td = "<td valign=\"top\" style=\"vertical-align:top\">";
    if (!heading.empty())
        text_td += "<p style=\"margin:0 0 4px;font-weight:bold;font-size:17px;"
                   "color:#2c2c2c\">" + mark + heading + "</p>";
    if (!text_html.empty() || (heading.empty() && !mark.empty()))
        text_td += "<p style=\"margin:0;white-space:pre-line;line-height:1.5;"
                   "color:#333\">" + (heading.empty() ? mark : std::string()) +
                   text_html + "</p>";
    text_td += "</td>";
    return "<table role=\"presentation\" width=\"100%\" cellpadding=\"0\" "
           "cellspacing=\"0\" style=\"margin:14px 0\"><tr>" +
           (image_right ? text_td + img_td : img_td + text_td) + "</tr></table>\n";
}

} // namespace hormiga
