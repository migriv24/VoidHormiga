/* audio.hpp — the audio block's markup, in both output domains.
 *
 * ── why this file exists ─────────────────────────────────────────────────────
 *
 * The same reason `video.hpp` does, and by the precedent it set: the block's
 * markup is pure — fields in, a string out — so it belongs beside its own
 * concern rather than inside `render_site`'s emit lambda, which is the function
 * `tools/find_long.py` has twice told us to stop growing. `render/site.cpp`
 * keeps what only it can do (resolving the cover rune, staging the file,
 * choosing the language) and hands the pieces here.
 *
 * Pure also means testable without a `Core`, a scene, or a filesystem, which is
 * the property `render/text.hpp` opens by naming.
 *
 * ── the block itself ─────────────────────────────────────────────────────────
 *
 * The Click LaFont report's A1, first rung: *"a music artist's website cannot
 * play the artist's music."* Not a niche of one — an organization with a
 * podcast, a recorded meeting, or a Spanish-language radio spot has exactly the
 * same absence, and for that organization the recording is often the most
 * accessible thing on the site, because it does not require reading.
 *
 * ── NO FACADE, WHICH IS THE OPPOSITE OF `video` AND FOR THE SAME REASON ──────
 *
 * `video` ships a click-to-load facade because a YouTube iframe reports every
 * visitor to Google whether or not they press play. Nothing here leaves the
 * organization's own site, so the honest thing is the plain element:
 * `preload="none"` makes the browser fetch no audio bytes until somebody
 * presses play, which is the same promise the facade makes, kept by the
 * standard rather than by our JavaScript. A native `<audio controls>` also
 * works with scripting off — a property worth choosing on purpose after the
 * 2026-09-02 `.reveal` finding.
 *
 * The transport control is left to the browser. Rebuilding it would mean
 * rebuilding keyboard access, screen-reader labelling and the media-session
 * integration a phone's lock screen uses, and getting all three right is not
 * something a stylesheet is going to do.
 */
#pragma once

#include "render/text.hpp"

#include <string>

namespace hormiga {

/* Everything the two renderers resolve before either of them can draw. Paths
 * are already staged/published by the caller; this file never touches a disk. */
struct AudioCard {
    std::string src;      // site-relative for web, absolute URL for email ("" = none)
    std::string title;
    std::string artist;
    std::string duration;
    std::string cover;    // site-relative href ("" = none)
    std::string caption;
};

/* The WEB rendering: a card, then the player.
 *
 * `missing_msg` is what stands in when there is no playable file — the caller
 * knows whether that is "no file chosen yet" or "the file is not on this
 * machine", and those are different sentences with different fixes. An empty
 * `<audio>` would be a play button that does nothing, which is the shape of
 * failure the two-pass `image_grid` rewrite exists to end.
 *
 * `download_label` is the anchor inside `<audio>`: what a browser that cannot
 * play the file shows, and what a reader who would rather keep the recording
 * than stream it clicks. Both beat an element that renders as nothing. */
inline std::string audio_web(const AudioCard& a, const std::string& missing_msg,
                             const std::string& download_label) {
    std::string h = "<figure class=\"audioblk reveal\">\n";
    if (!a.cover.empty() || !a.title.empty() || !a.artist.empty() ||
        !a.duration.empty()) {
        h += "<div class=\"audio-head\">";
        if (!a.cover.empty())
            h += "<img class=\"audio-cover\" src=\"" + html_escape(a.cover) +
                 "\" alt=\"\" loading=\"lazy\">";
        h += "<div class=\"audio-meta\">";
        if (!a.title.empty())
            h += "<span class=\"audio-title\">" + html_escape(a.title) + "</span>";
        if (!a.artist.empty())
            h += "<span class=\"audio-artist\">" + html_escape(a.artist) + "</span>";
        if (!a.duration.empty())
            h += "<span class=\"audio-dur\">" + html_escape(a.duration) + "</span>";
        h += "</div></div>\n";
    }
    if (a.src.empty()) {
        h += "<p class=\"empty\">" + html_escape(missing_msg) + "</p>\n";
    } else {
        h += "<audio class=\"audio-player\" controls preload=\"none\" src=\"" +
             html_escape(a.src) + "\"><a href=\"" + html_escape(a.src) + "\">" +
             html_escape(download_label) + "</a></audio>\n";
    }
    if (!a.caption.empty())
        h += "<figcaption class=\"meta caption\">" + html_escape(a.caption) +
             "</figcaption>\n";
    h += "</figure>\n";
    return h;
}

/* The EMAIL rendering: a card and a link, never a player.
 *
 * `<audio>` is stripped by Gmail and Outlook both, and the clients that would
 * honour it are not the ones an outreach organization's members read mail in.
 * So the newsletter's job is the same as it is for `video` — get the reader TO
 * the recording rather than pretend to hold it.
 *
 * `a.src` here must be an ABSOLUTE url, which is why the caller resolves it
 * from `site.base_url`: an email has no `site/` folder beside it, so a staged
 * relative path opens nothing in somebody's inbox. Empty means there is no such
 * address, and `no_link_msg` says where the recording lives instead — which is
 * an honest answer, unlike an anchor that 404s. */
inline std::string audio_email(const AudioCard& a, const std::string& accent,
                               const std::string& listen_label,
                               const std::string& no_link_msg) {
    std::string h =
        "<table role=\"presentation\" cellpadding=\"0\" cellspacing=\"0\" "
        "width=\"100%\" style=\"margin:14px 0;border:1px solid #e2e2e2;"
        "border-radius:8px\"><tr><td style=\"padding:14px\">";
    if (!a.title.empty())
        h += "<div style=\"font-weight:bold;font-size:16px;color:#2c2c2c\">" +
             html_escape(a.title) + "</div>";
    if (!a.artist.empty())
        h += "<div style=\"color:#666;font-size:14px\">" + html_escape(a.artist) +
             "</div>";
    if (!a.duration.empty())
        h += "<div style=\"color:#888;font-size:13px\">" +
             html_escape(a.duration) + "</div>";
    if (!a.src.empty())
        h += email_button(a.src, listen_label, accent);
    else
        h += "<div style=\"color:#888;font-size:13px;margin-top:8px\">" +
             html_escape(no_link_msg) + "</div>";
    if (!a.caption.empty())
        h += "<div style=\"color:#777;font-size:13px;margin-top:6px\">" +
             prose(a.caption) + "</div>";
    h += "</td></tr></table>\n";
    return h;
}

} // namespace hormiga
