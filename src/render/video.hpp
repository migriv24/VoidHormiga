/* video.hpp — a pasted video URL, understood.
 *
 * ── why this file exists ─────────────────────────────────────────────────────
 *
 * The operator, through the field agent, 2026-08-28: *"I want to have a mini
 * section for the Youth HUB… we probably need to ask hormiga to develop the
 * ability so we can put in a youtube video link. cuz it'd be nice to include
 * the youtube video from the email."*
 *
 * There was no block that took a video. `image_grid` takes images, `map_embed`
 * and `calendar_embed` take widgets, and `link` makes a button that navigates
 * away — which is the honest workaround and also the one that loses the reader.
 *
 * ── a URL parser, not an embed field ────────────────────────────────────────
 *
 * The report is specific about this and it is the more important half: *"People
 * paste `youtu.be/…` and `youtube.com/watch?v=…`; asking a volunteer for an
 * iframe snippet means pasting arbitrary HTML into a field, which is a
 * different kind of problem."*
 *
 * It is a different kind of problem in both directions. A volunteer who is
 * asked for an embed code will paste whatever the share dialog gave them, and
 * that string is markup: a field that accepts it either escapes it (so the
 * block silently does nothing) or interpolates it (so any block author can
 * inject script into a public page, which is a hole, not a feature). A field
 * that accepts a URL and yields a provider plus an id can only ever produce the
 * markup THIS file writes.
 *
 * So the whole surface is: text in, `{provider, id}` out, and an id that is
 * checked character by character. Anything unrecognised comes back empty and
 * the block says so on the page rather than emitting a broken frame.
 *
 * ── two providers, deliberately ─────────────────────────────────────────────
 *
 * YouTube and Vimeo, because those are what a community organization's video
 * is on and because each additional provider is another URL shape to get right,
 * another privacy posture to check, and another thing that breaks silently when
 * a vendor changes a path. "I would not go further than those two."
 *
 * Pure: string in, string out. No I/O, no network, no `HormigaApp` — the same
 * discipline as `text.hpp`, and what lets `tests/spine_smoke.cpp` pin every URL
 * shape without linking a renderer.
 */
#pragma once

#include <cctype>
#include <functional>
#include <string>
#include <string_view>

namespace hormiga {

struct VideoRef {
    std::string provider; // "youtube" | "vimeo" | "" when unrecognised
    std::string id;
    bool ok() const { return !provider.empty() && !id.empty(); }

    /* The PRIVACY-RESPECTING player URL, and the only place one is built.
     *
     * `youtube-nocookie.com` rather than `youtube.com`: same player, and
     * Google's own no-cookie host. It is not a complete answer — the request
     * still reaches Google and still carries an IP — which is why the block
     * does not load this at all until a person clicks. See the facade note in
     * `render/site.cpp`.
     *
     * The report's reasoning, kept because it is the reason and not a
     * preference: *"For an organization whose members include immigration and
     * survivor-services groups, that is not a small detail."* A visitor to a
     * mutual-aid site should not be reported to a third party for having read
     * the page. */
    std::string embed_url() const {
        if (provider == "youtube")
            return "https://www.youtube-nocookie.com/embed/" + id +
                   "?autoplay=1&rel=0";
        if (provider == "vimeo")
            return "https://player.vimeo.com/video/" + id + "?autoplay=1";
        return {};
    }

    /* Where the video LIVES — for the email domain, which cannot embed
     * anything, and for the "watch on…" fallback under the player. */
    std::string watch_url() const {
        if (provider == "youtube") return "https://www.youtube.com/watch?v=" + id;
        if (provider == "vimeo") return "https://vimeo.com/" + id;
        return {};
    }

    std::string provider_label() const {
        if (provider == "youtube") return "YouTube";
        if (provider == "vimeo") return "Vimeo";
        return {};
    }
};

namespace video_detail {

/* A YouTube id is 11 characters of the URL-safe base64 alphabet. Vimeo's is
 * digits. Checking the SHAPE is what makes it safe to interpolate the string
 * into an attribute later: nothing that passes this can carry a quote, an angle
 * bracket, or a path separator, so there is no escape to forget. */
inline bool is_youtube_id(const std::string& s) {
    if (s.size() < 8 || s.size() > 16) return false;
    for (char c : s)
        if (!std::isalnum((unsigned char)c) && c != '-' && c != '_') return false;
    return true;
}

inline bool is_vimeo_id(const std::string& s) {
    if (s.empty() || s.size() > 12) return false;
    for (char c : s)
        if (!std::isdigit((unsigned char)c)) return false;
    return true;
}

/* Everything up to the next `/`, `?`, `&` or `#`. */
inline std::string first_segment(const std::string& s, size_t from) {
    const size_t end = s.find_first_of("/?&#", from);
    return s.substr(from, end == std::string::npos ? std::string::npos : end - from);
}

/* The value of a query parameter, or "" — enough of a query parser for
 * `?v=…&t=30s`, which is the only shape that matters here. */
inline std::string query_param(const std::string& s, const std::string& key) {
    const size_t q = s.find('?');
    if (q == std::string::npos) return {};
    size_t i = q + 1;
    while (i < s.size()) {
        const size_t amp = s.find('&', i);
        const std::string pair =
            s.substr(i, amp == std::string::npos ? std::string::npos : amp - i);
        const size_t eq = pair.find('=');
        if (eq != std::string::npos && pair.substr(0, eq) == key)
            return pair.substr(eq + 1);
        if (amp == std::string::npos) break;
        i = amp + 1;
    }
    return {};
}

} // namespace video_detail

/* Parse whatever a person pasted.
 *
 * The shapes that actually arrive, all of which a volunteer can produce without
 * knowing they are producing different things:
 *
 *     https://www.youtube.com/watch?v=ID&t=42s     the address bar
 *     https://youtu.be/ID?si=…                     the Share button
 *     https://www.youtube.com/shorts/ID            a phone
 *     https://www.youtube.com/embed/ID             copied out of an embed code
 *     https://www.youtube.com/live/ID              a stream, after it ends
 *     https://vimeo.com/123456789
 *     https://player.vimeo.com/video/123456789
 *     ID                                           the bare id, pasted alone
 *
 * A bare 11-character id is accepted as YouTube because that is what somebody
 * who has done this once before will type, and because nothing else can be
 * confused with it: a Vimeo id is digits, and a URL has a slash.
 *
 * Scheme and `www.` are optional throughout — a pasted `youtu.be/x` with no
 * `https://` is a URL to every person who has ever pasted one, and refusing it
 * on a technicality is the kind of correctness nobody thanks you for. */
inline VideoRef parse_video_url(std::string_view raw) {
    VideoRef out;
    std::string s(raw);
    // trim, then drop the scheme and a leading www.
    while (!s.empty() && (unsigned char)s.front() <= ' ') s.erase(s.begin());
    while (!s.empty() && (unsigned char)s.back() <= ' ') s.pop_back();
    if (s.empty()) return out;
    for (const char* pre : {"https://", "http://", "//"})
        if (s.rfind(pre, 0) == 0) { s = s.substr(std::string(pre).size()); break; }
    if (s.rfind("www.", 0) == 0) s = s.substr(4);
    if (s.rfind("m.", 0) == 0) s = s.substr(2);

    using namespace video_detail;

    if (s.rfind("youtu.be/", 0) == 0) {
        const std::string id = first_segment(s, 9);
        if (is_youtube_id(id)) { out.provider = "youtube"; out.id = id; }
        return out;
    }
    if (s.rfind("youtube-nocookie.com/", 0) == 0 || s.rfind("youtube.com/", 0) == 0) {
        const size_t slash = s.find('/');
        const std::string path = s.substr(slash + 1);
        for (const char* pre : {"embed/", "shorts/", "live/", "v/"})
            if (path.rfind(pre, 0) == 0) {
                const std::string id = first_segment(path, std::string(pre).size());
                if (is_youtube_id(id)) { out.provider = "youtube"; out.id = id; }
                return out;
            }
        const std::string v = query_param(s, "v");
        if (is_youtube_id(v)) { out.provider = "youtube"; out.id = v; }
        return out;
    }
    if (s.rfind("player.vimeo.com/video/", 0) == 0) {
        const std::string id = first_segment(s, 23);
        if (is_vimeo_id(id)) { out.provider = "vimeo"; out.id = id; }
        return out;
    }
    if (s.rfind("vimeo.com/", 0) == 0) {
        const std::string id = first_segment(s, 10);
        if (is_vimeo_id(id)) { out.provider = "vimeo"; out.id = id; }
        return out;
    }
    // a bare id, pasted on its own
    if (s.find('/') == std::string::npos && s.find('.') == std::string::npos &&
        s.size() == 11 && is_youtube_id(s)) {
        out.provider = "youtube";
        out.id = s;
    }
    return out;
}


/* The web block's markup, beside the parser that feeds it.
 *
 * Here rather than in `render/site.cpp` for two reasons. The obvious one is
 * that `site.cpp` is at its length budget and the ratchet is a real check. The
 * better one is that the FACADE and the PARSER are one decision: the reason the
 * page ships a poster and a `data-embed` attribute instead of an `<iframe>` is
 * the same reason `id` is shape-checked, and splitting them across two files is
 * how somebody later "simplifies" one half without seeing the other.
 *
 * Pure: strings in, a string out. `poster_href` is already staged and escaped-
 * safe (it came out of `stage_site_asset`); `esc` is the caller's escaper, so
 * this file needs no opinion about HTML escaping and cannot have a second one.
 */
inline std::string video_block_html(
    const VideoRef& vid, const std::string& poster_href,
    const std::string& ratio, const std::string& play_label,
    const std::string& watch_label,
    const std::function<std::string(const std::string&)>& esc) {
    const std::string rcls = ratio == "4:3"    ? "r43"
                             : ratio == "1:1"  ? "r11"
                             : ratio == "9:16" ? "r916"
                                               : "r169";
    const std::string watch = vid.watch_url();
    std::string h;
    h += "<div class=\"videoblk " + rcls + (poster_href.empty() ? " plain" : "") +
         " reveal\" data-embed=\"" + esc(vid.embed_url()) + "\">";
    h += "<button class=\"vplay\" type=\"button\" aria-label=\"" + esc(play_label) +
         "\"";
    if (!poster_href.empty())
        h += " style=\"background-image:url('" + esc(poster_href) + "')\"";
    h += "><span class=\"vtri\" aria-hidden=\"true\"></span></button>";
    /* The no-JavaScript reader gets a working link rather than a dead
     * rectangle. Three lines, and it is the difference between a page that
     * degrades and a page that breaks. */
    h += "<noscript><a class=\"vfallback\" href=\"" + esc(watch) +
         "\" rel=\"noopener\">" + esc(watch_label) + "</a></noscript>";
    h += "</div>\n";
    /* THE LINK UNDERNEATH IS NOT DECORATION. A reader on a locked-down network,
     * a reader whose browser blocks the frame, and a reader who would rather
     * watch it in the app all need the address itself - and printing the host
     * is what tells a cautious visitor where pressing play would send them. */
    h += "<p class=\"meta vwatch\"><a href=\"" + esc(watch) +
         "\" rel=\"noopener\">" + esc(watch_label) + "</a></p>\n";
    return h;
}

} // namespace hormiga
