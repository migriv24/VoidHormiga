/* glyphs_blocks.hpp - the BLOCK glyphs: what a document is made of.
 *
 * Split out of `seed.hpp` on 2026-08-20, when the file crossed its length
 * budget and `tools/find_long.py` said so. The seam was already there — these
 * were three `register_*` functions in one header — which is what a good split
 * looks like: the file was long because three things had been put in it, not
 * because any one of them was.
 *
 * A glyph declaration is DATA. It is long because an outreach organization has
 * a lot of kinds of thing, and that length is honest; what was wrong was
 * keeping every family in one place.
 */
#pragma once

#pragma once
#include "domain/civic.hpp"            // the civic record owns its own glyphs too
#include "domain/hormiga_allomone.hpp" // the domain Allomone owns its own glyphs
#include "voidmaiz/embed.hpp"
#include "json.hpp"
#include <algorithm>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace hormiga {

/* ── THE PALETTE IS DECLARED WHERE THE GLYPH IS (2026-08-20) ─────────────────
 *
 * The Builder's palette used to be a hand-written list in `app.cpp`, kept in
 * sync with these declarations by memory. It was not in sync: `directory`
 * (2026-08-19), `event_feature` and `event_flier` (2026-08-20) were all
 * registered, renderable, documented and reachable from a script — and absent
 * from the palette, so the only way to place one was to type a command.
 *
 * Worse, this had been asserted to the author as already working. A list that
 * must be updated in a second file is a list that will be wrong, and the
 * failure is silent in exactly the direction that matters: the GUI quietly
 * offers less than the CLI, which is founding commitment 1 breaking in the
 * direction it always breaks.
 *
 * So `block()` records every glyph it declares, and the shell reads that. One
 * declaration, one palette, and a new block appears in the GUI by existing. */
struct BlockPaletteEntry {
    std::string glyph, label, category;
};

inline std::vector<BlockPaletteEntry>& block_palette() {
    static std::vector<BlockPaletteEntry> v;
    return v;
}

inline void register_block_glyphs(maiz::Core& core) {
    const char* chain_in =
        R"({"name":"prev","dir":"in","type":"flow","render":"adjacency"})";
    const char* chain_out =
        R"({"name":"next","dir":"out","type":"flow","render":"adjacency"})";
    auto block = [&](const char* glyph, const char* label, const char* fields,
                     const char* color, int face_h, bool hat, const char* category,
                     const char* editors, const char* labels) {
        std::string ports =
            hat ? std::string(chain_out) : std::string(chain_in) + "," + chain_out;
        // every component carries GRID placement (builder.md QA: stored as
        // 12-unit spans, edited as slots) — declared here or projection drops
        // them (the declare-or-vanish rule)
        // builder-internal fields (layout/nav/band) are editor:"hidden" — they
        // drop out of the generic inspector; the Builder manages them with its
        // own dropdowns/handles (author 2026-07-23: raw AND/OR + raw band/nav
        // fields were clunky). meta_desc etc. stay visible.
        std::string ed_full = std::string(editors);
        if (!ed_full.empty()) ed_full += ",";
        ed_full +=
            R"("row":"hidden","col":"hidden","span":"hidden","page":"hidden",)"
            R"("link_to":"hidden","band_bg":"hidden","band_full":"hidden",)"
            R"("band_image":"hidden",)"
            R"("band_filter":"combo:theme,none,mute,mono,warm,cool,soft")";
        std::string g = std::string(R"({"glyph":")") + glyph + R"(","label":")" +
                        label + R"(","fields":[)" + fields +
                        R"(,"row","col","span","page","link_to","band_bg",)"
                        R"("band_full","band_image","band_filter"],)"
                        R"("hints":{"color":")" + color +
                        R"(","shape":{"kind":"block"},"face":{"w":260,"h":)" +
                        std::to_string(face_h) + R"(},"category":")" + category +
                        R"(","editors":{)" + ed_full + R"(},"labels":{)" + labels +
                        R"__(,"row":"Grid row (0 = top)","col":"Order within the row (0-11) - blocks fill left to right",)__"
                        R"__("span":"Width (1-12 units)","page":"Page (website; empty = home)",)__"
                        R"__("link_to":"Navigates to (page slug or URL; makes it a button)",)__"
                        R"__("band_bg":"Band background (none/tint/accent/card/dark/gradient)",)__"
                        R"__("band_full":"Full-bleed band (1 = edge to edge)",)__"
                        R"__("band_image":"Band background image (path)",)__"
                        R"__("band_filter":"Treatment over the band image (theme = the site default)"},)__"
                        R"__("ports":[)__" +
                        ports + "]}}";
        core.register_glyph(g);
        /* …and remember it for the Builder's palette. Guarded because glyph
         * registration runs once per session per front-end, and a palette that
         * grows on every re-registration would show each block twice. */
        for (const auto& e : block_palette())
            if (e.glyph == glyph) return;
        block_palette().push_back({glyph, label, category});
    };
    // a PAGE of a website (W2, author 2026-07-23): title + slug + order + nav
    // visibility. Components name their page via their `page` field; empty =
    // the home page. Newsletters ignore pages (single document by nature).
    core.register_glyph(
        R"({"glyph":"page","label":"Page",)"
        R"("fields":["title_en","title_es","slug","order","in_nav","meta_desc",)"
        R"("social_title_en","social_title_es"],)"
        R"("hints":{"color":"#2e6b4f","face":{"w":200,"h":52},"category":"Content",)"
        R"("editors":{"in_nav":"combo:1,0","meta_desc":"multiline:70"},)"
        R"__("labels":{"title_en":"Title (English)","title_es":"Titulo (espanol)",)__"
        R"__("slug":"URL slug (e.g. about)","order":"Order (0 = home)",)__"
        R"__("in_nav":"Show in nav (1)",)__"
        R"__("meta_desc":"SEO description (search + social preview)",)__"
        R"__("social_title_en":"Share title (English; blank = the page title)",)__"
        R"__("social_title_es":"Titulo al compartir (espanol)"}}})__");
    /* `portrait` joined the banner on 2026-09-03. The author of a portfolio,
     * on their own built site: *"the header image of my face doesn't look good,
     * i would like my face to be in a circle thing, with maybe a separate
     * background banner thing."*
     *
     * Every image slot a `hero` had — `image`, `band_image`, plus the filter
     * and dim that style them — is the BACKGROUND, so a portrait put there is
     * cropped to a 340px band, which is how you get a face with no chin under a
     * headline. `portrait` is drawn as a round inset IN FRONT of `image`,
     * leaving `image` free to be an actual background.
     *
     * Not a portfolio nicety: it is what `directory` already does correctly for
     * a contact (`.card.person` draws a round avatar), and it is the shape a
     * board page and a "meet the director" page want. */
    block("hero", "hero",
          R"("title_en","title_es","subtitle_en","subtitle_es","image","portrait",)"
          R"("image_filter","image_dim")",
          "#d4a017", 90, true, "Content",
          R"("image":"image","portrait":"image",)"
          R"("image_filter":"combo:theme,none,mute,mono,warm,cool,soft")",
          R"__("title_en":"Title (English)","title_es":"Titulo (espanol)",)__"
          R"__("subtitle_en":"Subtitle (English; the line under the title)",)__"
          R"__("subtitle_es":"Subtitulo (espanol)",)__"
          R"__("image_filter":"Treatment over the photo (theme = the site default)",)__"
          R"__("image_dim":"Scrim strength 0-100 (blank = the site default)",)__"
          R"__("portrait":"Portrait shown as a round inset in front of the )__"
          R"__(banner (a person, a logo); blank = none",)__"
          R"("image":"Banner image")");
    // (hero face height covers an optional banner strip)
    /* `heading_en`/`heading_es` joined the text on 2026-09-03, from the
     * portfolio report A9: *"the experience tab just looks super lame. like
     * they're just cards, no bold letters, no different heading styles."*
     *
     * `narrative` renders as ONE `<p class="prose pre-line">`, so a role, an
     * employer, a date range and four bullets are one paragraph at one weight.
     * Line breaks (2026-09-02) made the STRUCTURE expressible and left the
     * HIERARCHY not — the client reached for `::first-line{font-weight:800}`,
     * which means "the first line of this paragraph is secretly a heading".
     *
     * Two fields and an `<h3>` is the honest version. The alternative they
     * offered — a light inline markup vocabulary parsed at render — is a real
     * design conversation about where that line sits, and it is not one to
     * settle inside a block that already works. */
    block("narrative", "narrative",
          R"("icon","heading_en","heading_es","text_en","text_es")", "#4c97ff", 116,
          false, "Content",
          R"("icon":"icon","text_en":"multiline:70","text_es":"multiline:70")",
          R"__("icon":"Icon beside the heading (optional)",)__"
          R"__("heading_en":"Heading above the text (English; optional)",)__"
          R"__("heading_es":"Encabezado (espanol; opcional)",)__"
          R"__("text_en":"Text (English) - start lines with - or 1. for a list",)__"
          R"__("text_es":"Texto (espanol) - empiece lineas con - o 1. para una lista")__");
    /* ── image_text: a picture and the words that belong with it (2026-09-13) ─
     *
     * The author, while making a newsletter: *"an easier way to make a narrative
     * section where we have like an image on one side, and the text on the
     * other, like they go together (remember to make this an explicit block)."*
     * The workaround was an image and a narrative placed side by side in one
     * row: two things to keep in step for what the author means as ONE thing,
     * and a layout the newsletter stacked anyway, because the email renderer
     * ignored rows until the same day. Markup in render/image_text.hpp. `image`
     * is a path (the "image" editor, as on `hero`); the email resolves it to
     * that image's published url. */
    block("image_text", "image + text",
          R"("image","side","icon","heading_en","heading_es","text_en","text_es",)"
          R"("alt_en","alt_es")",
          "#4c97ff", 120, false, "Content",
          R"("image":"image","side":"combo:left,right","icon":"icon",)"
          R"("text_en":"multiline:70","text_es":"multiline:70")",
          R"__("image":"Image",)__"
          R"__("side":"Which side the image sits on",)__"
          R"__("icon":"Icon beside the heading (optional)",)__"
          R"__("heading_en":"Heading (English; optional)",)__"
          R"__("heading_es":"Encabezado (espanol; opcional)",)__"
          R"__("text_en":"Text (English) - start lines with - or 1. for a list",)__"
          R"__("text_es":"Texto (espanol) - empiece lineas con - o 1. para una lista",)__"
          R"__("alt_en":"What the image shows, for a screen reader (English)",)__"
          R"__("alt_es":"Que muestra la imagen (espanol)")__");
    block("section_header", "section header", R"("title_en","title_es")", "#8a6d3b",
          44, false, "Content", R"()",
          R"__("title_en":"Heading (English)","title_es":"Encabezado (espanol)")__");
    block("event_grid", "event grid",
          R"("query","detail","columns","limit","sort","search","caption_en","caption_es")",
          "#9966cc", 84, false, "Data",
          R"("query":"hidden","detail":"combo:compact,title,full",)"
          R"("search":"combo:auto,on,off","columns":"combo:1,2,3")",
          R"__("query":"Event query: tags with AND/OR/NOT, plus date:future | )__"
          R"__(date:past | date:today | date:recurring | date:undated. Check it )__"
          R"__(with `effect query` - `ls --tag` cannot see the date ones.",)__"
          R"__("detail":"How much of each event to show",)__"
          R"__("columns":"Cards per row, 1-3 (blank = the site's automatic grid, )__"
          R"__(one card per row in the newsletter; a phone always stacks)",)__"
          R"__("limit":"Most events to show (blank = all)",)__"
          R"__("sort":"date | date-desc | name (default date)",)__"
          R"__("search":"Search box: auto (only when it helps) | on | off",)__"
          R"__("caption_en":"Caption (English)",)__"
          R"__("caption_es":"Titulo (espanol)")__");

    /* ── event_feature: ONE event, showcased (2026-08-20) ────────────────────
     *
     * The author: *"a separate highlighted event, not just the event grid …
     * similar to a section header, except the image is in the background with
     * the event information and text over it."*
     *
     * A grid answers "what is coming up". This answers "come to THIS" — the one
     * thing an organization most wants a visitor to see, at a size a grid cell
     * cannot give it. Different job, different block, rather than a `featured`
     * flag on `event_grid` that would have made the grid mean two things.
     *
     * The background image is the event's own flier when it has one (found
     * through its `flyer-of` edge), so the common case needs no authoring at
     * all — which is the payoff for reading relations at the render seam. */
    block("event_feature", "featured event",
          R"("event","image","height","image_filter","image_dim","cta_en","cta_es",)"
          R"("cta_link")",
          "#c2410c", 96, false, "Data",
          R"("event":"hidden","image":"image",)"
          R"("height":"combo:compact,tall,full",)"
          R"("image_filter":"combo:theme,none,mute,mono,warm,cool,soft")",
          R"__("event":"Which event (rune name)",)__"
          R"__("image":"Background image (blank = the event's own flier)",)__"
          R"__("height":"How much room it takes",)__"
          R"__("image_filter":"Treatment over the photo",)__"
          R"__("image_dim":"Scrim strength 0-100 (blank = the site default)",)__"
          R"__("cta_en":"Button label (English)","cta_es":"Boton (espanol)",)__"
          R"__("cta_link":"Button target (blank = the event's virtual link)")__");

    /* ── event_flier: the event AND its flier, side by side ──────────────────
     *
     * Distinct from `event_feature` on the author's own distinction: *"this
     * will be different than the previous event highlight because it's a flier
     * we should still be able to click."* A flier is a document a person wants
     * to open full size, save, and share — not wallpaper behind a headline. So
     * here the image is a real tile that opens the lightbox, and the event's
     * details sit beside it.
     *
     * BILINGUAL BY CONSTRUCTION: a flier exists in two languages as two `image`
     * runes carrying `lang:` tags, and the block picks the one matching the
     * page. That is the project's own rule (per-language THINGS get sibling
     * runes) finally being consumed rather than just stated. */
    block("event_flier", "event + flier",
          R"("event","flier","display","caption_en","caption_es")",
          "#7d5bb0", 84, false, "Data",
          R"("event":"hidden","flier":"hidden",)"
          R"("display":"combo:side,stacked")",
          R"__("event":"Which event (rune name)",)__"
          R"__("flier":"Which flier (blank = the event's own, in this language)",)__"
          R"__("display":"Flier beside the details, or above them",)__"
          R"__("caption_en":"Caption (English)","caption_es":"Titulo (espanol)")__");
    // query-backed image grid: the auto-updating flier wall — add a summer
    // flier tagged @summer and it appears here on the next render, no edit
    // display modes (author 2026-07-23): an image grid presents many ways —
    // grid (even cells), masonry (Pinterest wall), carousel (swipe left/right,
    // the "content-horizontal" the author described). Email always renders a
    // simple table grid (no JS/columns); the mode is a WEB property.
    block("image_grid", "image grid",
          R"("query","columns","display","fit","limit","rank_up","rank_down",)"
          R"("caption_en","caption_es")",
          "#7d5bb0", 84, false, "Data",
          R"("query":"hidden","columns":"combo:2,3,4",)"
          R"("display":"combo:grid,masonry,carousel","fit":"combo:crop,whole,natural,stretch",)"
          R"("rank_up":"hidden","rank_down":"hidden")",
          R"__("query":"Image query: tags with AND/OR/NOT, plus date:future | )__"
          R"__(date:past | date:today | date:undated - a flier takes its date )__"
          R"__(from the event it is linked to, so an unlinked one is undated.",)__"
          R"__("columns":"Columns",)__"
          R"__("display":"Display mode (web)",)__"
          R"__("fit":"How each image fills its tile: crop = same shape, trimmed; )__"
          R"__(whole = same shape, nothing cut off; natural = each image keeps its )__"
          R"__(own shape; stretch = fills the tile, distorted",)__"
          R"__("rank_up":"Order: list first (tags that move an entry up)",)__"
          R"__("rank_down":"Order: list last (tags that move an entry down)",)__"
          R"__("caption_en":"Caption (English)","caption_es":"Titulo (espanol)")__");
    /* `detail`/`limit`/`sort` arrive 2026-08-20, matching `event_grid`. Until
     * then this was the only grid with NO clipping at all — one real posting
     * ran 1,150 characters in a card and the operator's one-word review was
     * "yikes". A grid that can only render everything is a grid that cannot be
     * used with real data. */
    block("job_grid", "job grid",
          R"("query","detail","columns","limit","sort","rank_up","rank_down",)"
          R"("caption_en","caption_es")",
          "#5d7d3b", 84, false, "Data",
          R"("query":"hidden","detail":"combo:line,compact,title,full","columns":"combo:1,2,3",)"
          R"("rank_up":"hidden","rank_down":"hidden")",
          R"__("query":"Job query: tags with AND/OR/NOT, plus date:future | )__"
          R"__(date:past - a posting's date is its `deadline`, so date:future )__"
          R"__(means still open.",)__"
          R"__("detail":"How much of each posting to show (line = one line each, )__"
          R"__(the tightest newsletter form)",)__"
          R"__("columns":"Cards per row, 1-3 (blank = the site's automatic grid, )__"
          R"__(one card per row in the newsletter; `line` stays one line each)",)__"
          R"__("limit":"Most postings to show (blank = all)",)__"
          R"__("sort":"deadline | name (default document order)",)__"
          R"__("rank_up":"Order: list first (tags that move an entry up)",)__"
          R"__("rank_down":"Order: list last (tags that move an entry down)",)__"
          R"__("caption_en":"Caption (English)",)__"
          R"__("caption_es":"Titulo (espanol)")__");
    /* THE DIRECTORY (2026-08-19): the block that puts a PERSON or an
     * ORGANIZATION on a page. It is the only query-backed block whose query is
     * not sufficient — every rune it publishes must also carry
     * `clearance:public`, and `clearance:contact` is a second, independent
     * annotation that releases an email or a phone. The rule is in the renderer
     * (render/site.cpp) because that is the seam where data leaves, which is
     * the only place a privacy rule is worth enforcing; see security.md §3 and
     * web-platform.md §4.
     *
     * `query` stays hidden from the generic inspector like every other block
     * query — the Builder gives it a picker — but `kind` and `display` are
     * combos, so the inspector renders them with no GUI code written. */
    /* `live` (2026-08-21): the directory refreshes itself from a small file
     * instead of being frozen at build time. See okf/concepts/platform/data-planes.md —
     * the operator's complaint was having to redeploy the whole site whenever a
     * contact changed. Off by default: a site that needs no freshness should
     * not pay for a fetch, and the built-in cards are what a visitor with no
     * JavaScript sees either way. */
    block("directory", "directory",
          R"("query","kind","display","limit","live","rank_up","rank_down",)"
          R"("caption_en","caption_es")",
          "#b3592e", 84, false, "Data",
          R"("query":"hidden","kind":"combo:contact,organization,both",)"
          R"("live":"combo:off,on",)"
          R"("display":"combo:card,list,carousel","rank_up":"hidden","rank_down":"hidden")",
          R"__("query":"Who to list (tags; AND/OR/NOT). Only runes tagged )__"
          R"__(clearance:public are ever published.",)__"
          R"__("kind":"People, organizations, or both",)__"
          R"__("display":"Card wall, compact list, or swipe carousel",)__"
          R"__("limit":"Most entries to show (blank = all)",)__"
          R"__("live":"Refresh from site/index/ on load, so updating a contact )__"
          R"__(needs only that small file republished - not the whole site",)__"
          R"__("rank_up":"Order: list first (tags that move an entry up)",)__"
          R"__("rank_down":"Order: list last (tags that move an entry down)",)__"
          R"__("caption_en":"Caption (English)","caption_es":"Titulo (espanol)")__");
    /* ── video: THE ONE BLOCK THAT REACHES OFF THE SITE (2026-08-28) ──────
     *
     * The operator: *"I want to have a mini section for the Youth HUB… it'd be
     * nice to include the youtube video from the email."* Nothing here took a
     * video: `image_grid` takes images, the embeds take widgets, and `link`
     * makes a button that navigates away.
     *
     * `url` IS A URL, not an embed code — see render/video.hpp for why that
     * distinction is a security property and not a convenience. `poster` is the
     * organization's OWN still, staged like any other asset: the block will not
     * fetch a thumbnail from the provider, because doing so would report every
     * visitor to that provider before anyone had asked to watch anything, which
     * is the whole thing the click-to-play facade exists to avoid.
     *
     * `caption_en`/`caption_es` rather than a shared `caption`, because every
     * other block on this list learned that lesson already. */
    block("video", "video",
          R"("url","poster","ratio","caption_en","caption_es")",
          "#b3592e", 84, false, "Media",
          R"("poster":"image","ratio":"combo:16:9,4:3,1:1,9:16")",
          R"__("url":"Video link (YouTube or Vimeo; paste the address, not an )__"
          R"__(embed code)","poster":"Still image shown before play (blank = a )__"
          R"__(plain card; nothing is ever fetched from the video host)",)__"
          R"__("ratio":"Shape of the player",)__"
          R"__("caption_en":"Caption (English)","caption_es":"Titulo (espanol)")__");
    /* ── AUDIO: the organization's own recording, on its own page ────────────
     *
     * The Click LaFont report's A1, first rung, and the sentence that carried
     * it: *"a music artist's website cannot play the artist's music."* Thirty-six
     * songs sitting in the folder beside the database, and a site full of
     * buttons pointing at somebody else's player.
     *
     * **Not a niche of one**, which is why it is a built-in block rather than a
     * one-client accommodation. An organization with a podcast, a recorded
     * meeting, or a Spanish-language radio spot has exactly this absence — and
     * for that organization the recording is often the most accessible thing on
     * the site, because it does not require reading.
     *
     * ── THE POSTURE IS `video`'s, MINUS THE THIRD PARTY ─────────────────────
     *
     * `video` ships a facade because a YouTube iframe reports every visitor to
     * Google whether or not they press play. This block has no such problem and
     * therefore no facade: the file is the organization's own, served from the
     * organization's own site, and `preload="none"` means the browser fetches
     * nothing until somebody presses play. A native `<audio controls>` also
     * needs no JavaScript at all, which after the 2026-09-02 `.reveal` finding
     * is a property worth having on purpose rather than by luck.
     *
     * ── `src` IS A PATH, AND THAT IS A DECISION WITH A DEADLINE ─────────────
     *
     * A file beside the database, staged into `site/assets/` at render time,
     * exactly like a `hero`'s image. The GUI's `path` editor browses AND ingests
     * (content-hashed into `assets/`), so the file becomes the organization's.
     *
     * What it is NOT, yet, is an `audio` DATA rune — taggable, queryable,
     * linkable to the event it was recorded at. That is deliberate and it is
     * open as Q59: the author's argument is that a glyph per medium is the wrong
     * shape ("do we really think that a 3D object type is as needed as a
     * contact, event, or image?") and that the answer is a registry for custom
     * types. Adding an `audio` glyph to the core five now would be pre-empting
     * that question in the direction it argues against. A block that plays a
     * file is the part that is certainly right; where the file's METADATA lives
     * is the part still being decided.
     *
     * `cover` is an image RUNE, the same as `video.poster`, because a cover
     * image genuinely is one of the five universal things. */
    block("audio", "audio",
          R"("src","title_en","title_es","artist","duration","cover",)"
          R"("caption_en","caption_es")",
          "#7d5bb0", 104, false, "Media",
          R"("src":"path","cover":"image")",
          R"__("src":"Audio file (mp3, m4a, ogg, wav) - browse to bring it into )__"
          R"__(this organization's assets",)__"
          R"__("title_en":"Title (English)","title_es":"Titulo (espanol)",)__"
          R"__("artist":"Artist / speaker (optional)",)__"
          R"__("duration":"Length as you want it shown, e.g. 3:42 (optional)",)__"
          R"__("cover":"Cover image (optional; an image in this organization)",)__"
          R"__("caption_en":"Caption (English)","caption_es":"Titulo (espanol)")__");
    /* ── DOWNLOAD: a file a visitor can keep (2026-09-02) ────────────────────
     *
     * The portfolio agent's blocking ask. A résumé is what surfaced it, but the
     * thing is general: a flier PDF, the bylaws, an annual report, a
     * know-your-rights sheet, a printable calendar — the documents an outreach
     * organization is most often asked for, and until now the answer was to host
     * them somewhere Hormiga does not manage.
     *
     * `file` TAKES EITHER FORM, which is the report's third lean and it is the
     * one that earns its keep: a path relative to the database, OR the name of a
     * `resource` rune. `resource` (`path`, `topic`) has been declared, editable
     * and rendered by NOTHING since it was written — the third instance in this
     * codebase of a declared field that does nothing, after `image_grid.columns`
     * and the whole of `hol_github`. One field with two accepted forms makes the
     * dead glyph reachable without inventing a second block, exactly the way
     * `audio.cover` already resolves an `image` rune name.
     *
     * NO `clearance:` GATE, and that is a decision rather than an omission.
     * `directory` publishes a QUERY RESULT, so it has to ask permission for each
     * person it might name. This publishes the one file an author pointed at,
     * and the report's sentence for it is the right one: **naming the file is
     * the consent.** A `download_grid` over `resource` runes would inherit
     * `directory`'s question immediately and can arrive later with that
     * conversation attached. */
    block("download", "download",
          R"("file","label_en","label_es","caption_en","caption_es",)"
          R"("download_style","platform")",
          "#2e6b4f", 84, false, "Media",
          R"("file":"path","download_style":"combo:button,card",)"
          R"("platform":"combo:any,windows-x64,macos,linux-x64")",
          R"__("file":"File to publish - a path beside the database, or the name )__"
          R"__(of a `resource` rune",)__"
          R"__("label_en":"Button text (English)","label_es":"Texto del boton (espanol)",)__"
          R"__("caption_en":"Note under it (English)","caption_es":"Nota (espanol)",)__"
          R"__("download_style":"Button, or a wider card with the file details",)__"
          R"__("platform":"Which computer this file is for. Two or more on one )__"
          R"__(row become a platform set: the visitor's own is moved first and )__"
          R"__(marked. None is ever hidden. Blank or `any` = every computer")__");
    block("footer", "footer", R"("text_en","text_es")", "#5cb1d6", 72, false,
          "Content", R"("text_en":"multiline:70","text_es":"multiline:70")",
          R"__("text_en":"Text (English)","text_es":"Texto (espanol)")__");
    // the LINK / BUTTON (author 2026-07-23): explicit navigation — a placeable
    // element that points to a page (slug) or an external URL. Navigation is
    // AUTHORED, not auto-generated; anything else can also navigate via its
    // link_to field ("anything could be a button").
    /* `platform` joins `link` on 2026-09-08, and joining `link` rather than only
     * `download` is the decision. The installer button IS a `link` — a 7 MB
     * `.exe` lives on GitHub Releases, not in the site's `assets/`
     * (download-page.md §3), so the thing that offers it is a navigation to
     * another origin. A platform field that reached only `download` could not
     * have served the page that asked for it. */
    block("link", "link / button",
          R"("label_en","label_es","target","link_style","platform")",
          "#2e6b4f", 44, false, "Content",
          R"("link_style":"combo:button,text",)"
          R"("platform":"combo:any,windows-x64,macos,linux-x64")",
          R"__("label_en":"Label (English)","label_es":"Etiqueta (espanol)",)__"
          R"__("target":"Target (page slug or URL)","link_style":"Style",)__"
          R"__("platform":"Which computer this link is for. Two or more on one )__"
          R"__(row become a platform set: the visitor's own is moved first and )__"
          R"__(marked. None is ever hidden. Blank or `any` = every computer")__");
    // ── richer modern sections (W3, author 2026-07-23): quote/stat/divider —
    // building blocks of a modern site; each a render-pack style, model stays
    // declarative. Often placed side-by-side in a band (e.g. three stats). ──
    block("quote", "quote", R"("text_en","text_es","author")", "#8a6d3b", 84,
          false, "Content", R"("text_en":"multiline:70","text_es":"multiline:70")",
          R"__("text_en":"Quote (English)","text_es":"Cita (espanol)",)__"
          R"__("author":"Attribution (who said it)")__");
    block("stat", "stat / metric", R"("number","label_en","label_es")", "#4c97ff",
          56, false, "Content", R"()",
          R"__("number":"Number (e.g. 500+)","label_en":"Label (English)",)__"
          R"__("label_es":"Etiqueta (espanol)")__");
    /* `bar` joins line/dots/space on 2026-09-02 (field report A6): "both album
     * covers carry the same five-colour swatch bar; it is the brand's strongest
     * repeating device and there is no way to put a coloured bar on a page." A
     * banner stripe is the same request for an organization, so it is a divider
     * style rather than a one-client block. `colors` is comma-separated and
     * validated at the render seam (`css_colors`, render/text.hpp); empty falls
     * back to the theme accent, so `divider_style bar` alone already works.
     * EMAIL renders `bar` as its plain rule: a table-layout mail client and a
     * five-cell coloured row are not a fight worth having for a separator. */
    block("divider", "divider", R"("divider_style","colors","bar_height")",
          "#6f6f78", 56, false,
          "Content", R"("divider_style":"combo:line,dots,space,bar")",
          R"__("divider_style":"Style",)__"
          R"__("colors":"Bar colours, comma-separated (#ff5a5f,#3ddc97,...); blank = the theme accent",)__"
          R"__("bar_height":"Bar height in pixels (2-80; blank = 10)")__");
    // the MAP block (author, 2026-07-22): references a saved map VIEW. Web
    // domain → a READ-ONLY interactive JS widget (pan/zoom/markers; it can
    // never write); email domain → a static PNG at the view's home viewport.
    // PRIVACY SEAM: contacts never render into either (personal coordinates
    // don't leave the machine; okf/concepts/sections/territory.md boundaries).
    block("map_embed", "map", R"("view","caption_en","caption_es")", "#2e6b4f",
          72, false, "Interactive", R"("view":"hidden")", // picker in the builder UI
          R"__("view":"Map view (name of a saved view)",)__"
          R"__("caption_en":"Caption (English)","caption_es":"Titulo (espanol)")__");
    // the CALENDAR block: web → an interactive month/week/3-day JS widget
    // (read-only) with .ics download + add-to-Google links; email → an
    // email-safe HTML table month. Default query is EVENTS ONLY — incidents
    // are opt-in by query (they can be sensitive; publishing is a choice).
    block("calendar_embed", "calendar",
          R"("query","mode","caption_en","caption_es")",
          "#b3592e", 72, false, "Interactive",
          R"("query":"hidden","mode":"combo:agenda,month")",
          R"__("query":"Entry query (tags; empty = events only)",)__"
          R"__("mode":"agenda (a list) or month (a grid); default agenda",)__"
          R"__("caption_en":"Caption (English)","caption_es":"Titulo (espanol)")__");
    // the DOCUMENT rune (builder.md): one per document — its kind names the
    // builder (newsletter = HTML components, website = JS components); the
    // THEME socket lives here from day one (the Style tab grows into it)
    core.register_glyph(
        R"({"glyph":"document","label":"Document",)"
        R"("fields":["kind","title","theme_accent","theme_bg"],)"
        R"("hints":{"color":"#6f6f78","face":{"w":220,"h":56},"category":"Content",)"
        R"("editors":{"kind":"combo:newsletter,website"},)"
        R"("labels":{"kind":"Document kind","title":"Title",)"
        R"__("theme_accent":"Theme accent (hex)","theme_bg":"Theme background (hex)"}}})__");
}

/* The Antfarm glyphs (okf/concepts/platform/antfarm/index.md, now a real canvas): one CORE
 * hub whose out-sockets are the protocol interfaces — connector-shape-as-
 * type, a `data` plug only fits a `data` socket — and one glyph per holiday
 * provider with a single typed plug. Wiring IS configuration: the edge from
 * core.data to a provider is "this is where the org's data lives". The old
 * ANTFARM.md drew exactly this hub-consumer picture; the graph is now real.
 * NOTE: secrets are NEVER fields (fields are exported state) — nodes show
 * key PRESENCE; keys live in gitignored files until .miga v2. */
/* The Antfarm glyphs — the I/O boundary as a TYPED DATAFLOW GRAPH (redesign,
 * Q25, author 2026-08-05). The old model typed ports by a coarse input/output
 * bucket, which lumped unrelated agents (Supabase+Sheets on "import";
 * out-html+out-imgbb on "output"). Now ports are typed by the PAYLOAD that
 * flows — `records` (the runes), `assets` (blobs), `site` (a rendered
 * publication) — so incompatible agents can't be confused, and publishing is a
 * PIPELINE (core → publisher → server/deployer), not siblings on one socket.
 * Locality (local vs cloud) is badged in the label + color. */

} // namespace hormiga
