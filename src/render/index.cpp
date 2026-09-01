/* index.cpp — the OUTPUT domain, second format: the published subset as DATA.
 *
 * `render/site.cpp` turns the published subset into pages. This turns the SAME
 * subset — through the SAME gate, `render/published.hpp` — into rows.
 *
 * It is its own translation unit rather than more of `site.cpp` because it is
 * genuinely a different output domain, and because `site.cpp` had already been
 * granted a budget increase earlier the same day. Appending to a file you just
 * widened is how a file becomes 12,000 lines; `okf/log.md` records that this
 * project has been there once already.
 */
#include "app/app_internal.hpp"
#include "render/published.hpp" // the shared clearance gate
#include "render/text.hpp"
#include "json.hpp"

#include <fstream>

/* ── publish_index — THE PUBLISHED SUBSET AS DATA (2026-08-21) ────────────────
 *
 * The author's ask: the website should respond to database changes *"rather
 * than needing to be redeployed every time."*
 *
 * The instinct is to put the database in the cloud. That is wrong for the
 * reason ground rule 3 exists, and it is also more than the problem needs. The
 * published subset is already computed — `render/published.hpp` is the gate,
 * shared with the directory block — so "live" is not a new capability, it is a
 * second FORMAT for an existing computation:
 *
 *     the published subset  ->  HTML files   (render_site)
 *                           ->  index rows   (here)
 *
 * Same seam, same filter, same withheld count. A second exporter that
 * re-implemented the clearance check would be a second privacy surface, and it
 * would drift silently, because a website showing too much still looks correct.
 * That is why the gate moved into a header both call rather than being copied.
 *
 * ── WHY A FILE, AND NOT A DATABASE ───────────────────────────────────────────
 *
 * This writes JSON into `site/index/`, which means an ordinary deploy carries
 * it with no new machinery — and, more usefully, it is a few kilobytes that can
 * be pushed ON ITS OWN. Updating a contact then costs one small upload instead
 * of a full render and a full site deploy, which is the operator's actual
 * complaint, solved with no new vendor, no Lambda and no table.
 *
 * A real datastore becomes necessary at the point where different VISITORS must
 * see different rows — a signed-in member seeing more than the public — because
 * a static file cannot be filtered per reader. That is a later phase and it
 * should not be paid for early (okf/concepts/platform/data-planes.md §7).
 *
 * ── DETERMINISTIC ON PURPOSE ─────────────────────────────────────────────────
 *
 * No timestamp, no generation marker. An index that changes on every run cannot
 * be hashed to decide whether it needs pushing, cannot be diffed by a reviewer,
 * and cannot be content-addressed by Void Palabra later. The data is the
 * document; when it changes, the bytes change, and not before.
 */
std::string HormigaApp::publish_index() {
    maiz::ProjectOptions dio;
    dio.mantle = kDataMantle;
    const maiz::Scene data = maiz::project_scene(core, dio);

    /* NO QUERY. The index is "everything this organization publishes", and the
     * blocks or clients that read it apply their own narrowing. Filtering here
     * would bake one page's editorial choice into the data, and a second page
     * wanting a different slice would need a second index. */
    auto pub = hormiga::published::directory(
        data, /*query=*/"", /*kind=*/"",
        [this](const std::string& rune) { return allo_web_hidden(rune); });

    nlohmann::json rows = nlohmann::json::array();
    for (const auto& p : pub.people) {
        nlohmann::json r;
        r["name"] = p.name;          // the stable id a live index addresses by
        r["kind"] = p.glyph;
        r["display"] = p.display;
        if (!p.role.empty()) r["role"] = p.role;
        if (!p.place.empty()) r["place"] = p.place;
        if (!p.bio.empty()) r["bio"] = p.bio;
        if (!p.website.empty()) r["website"] = p.website;
        /* The avatar is staged like any other site asset, so the index points
         * at a file the site already serves rather than at a path on the
         * operator's disk — the same rule every image on the site follows. */
        if (!p.avatar.empty()) {
            const std::string href = stage_site_asset(p.avatar);
            if (!href.empty()) r["avatar"] = href;
        }
        // present ONLY for a rune carrying the second consent; see published.hpp
        if (!p.email.empty()) r["email"] = p.email;
        if (!p.phone.empty()) r["phone"] = p.phone;
        r["tags"] = p.tags;
        rows.push_back(std::move(r));
    }

    nlohmann::json doc;
    doc["kind"] = "hormiga.directory";
    doc["v"] = 1;
    doc["people"] = std::move(rows);

    std::error_code ec;
    fs::create_directories(data_dir("site") / "index", ec);
    const fs::path out = data_dir("site") / "index" / "directory.json";
    {
        std::ofstream o(out, std::ios::binary | std::ios::trunc);
        if (!o) {
            log.push_back({"error", "render",
                           "cannot write the index: " + out.string()});
            return {};
        }
        o << doc.dump(1, '\t') << "\n";
    }
    /* SAY WHAT WAS WITHHELD, exactly as the directory block does. An index that
     * comes out empty because nobody carries `clearance:public` looks identical
     * to a broken exporter, and the person who has to fix it is reading this. */
    log.push_back({"info", "render",
                   "index: " + std::to_string(pub.people.size()) +
                       " published, " + std::to_string(pub.withheld) +
                       " withheld (only runes tagged `clearance:public` are "
                       "published; add `clearance:contact` too to publish an "
                       "email or phone)"});
    return out.string();
}

/* ── the live directory fragment ──────────────────────────────────────────────
 *
 * A `live` directory block writes its cards here as well as into the page, so
 * that refreshing the directory costs one small file republished rather than a
 * whole site deploy. The page renderer hands over the bytes it already emitted,
 * which is what makes the two incapable of disagreeing — there is one buffer
 * and it is written twice.
 *
 * It lives in this unit rather than in `site.cpp` because it is a published
 * ARTIFACT, the same job as `publish_index` above, and because `site.cpp` has
 * been over its length budget twice in one day. The right response to that is to
 * put things where they belong, not to widen the file again.
 */
bool HormigaApp::write_live_fragment(const std::string& block,
                                     const std::string& html) {
    std::error_code ec;
    fs::create_directories(data_dir("site") / "index", ec);
    const fs::path p = data_dir("site") / "index" / ("dir-" + block + ".html");
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (f) {
        f << html;
        return true;
    }
    /* NOT fatal. The page carries the same cards, so the site is correct and
     * merely not refreshable — but a `live` directory that silently is not live
     * is exactly the kind of thing nobody notices for a month. */
    log.push_back({"warn", "render",
                   "directory " + block +
                       ": live is on but its fragment could not be written to " +
                       p.string() + " - the page is correct, but it will not "
                       "refresh"});
    return false;
}
