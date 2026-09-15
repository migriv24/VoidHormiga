/* domain/hosting.hpp — "host it online": a holiday as a function call.
 *
 * The author (2026-09-15): *"there should be a button that's like 'host it
 * online'. THIS is where the holiday really matters for the antfarm, because we
 * don't know HOW we are going to host it online ... a button to automatically do
 * something like 'host it online' should have its protocols be called upon via
 * the antfarm. Because then, it should return with a link. essentially its kinda
 * like a function call, where we expect a link to be given in return."*
 *
 * THE SHAPE. A capability is a question with a typed answer — here, a local file
 * in and a public link out. The Antfarm answers it by finding a node whose
 * holiday implements that protocol, and the caller neither knows nor cares which
 * kind of node it was: ImgBB, an S3 or R2 bucket, and the organization's own
 * website all answer the same call. This table is the whole registry of who can
 * answer, so a new host is one row here and one branch in
 * `HormigaApp::host_online` (src/publish/push.cpp); the image editor, the Data
 * tab, the Antfarm faces, its inspector panel and `effect host-online` all read
 * the list rather than naming a vendor.
 *
 * `after_publish`: a website host answers with a link that goes live on the NEXT
 * publish of the site. That is still a link, and saying when it works is part of
 * the answer rather than a footnote to it.
 *
 * Pure data, no GUI and no I/O, so every surface reads the same rows.
 */
#pragma once

#include <string>

namespace hormiga::hosting {

struct AssetHost {
    const char* glyph;  // the Antfarm node kind that answers
    const char* label;  // what a person calls it
    const char* how;    // what happens when it is asked
    const char* needs;  // what it needs before it can answer
    bool after_publish; // the link works once the website is published again
};

/* In order of preference when nothing is chosen: a host whose link works at once
 * comes before one whose link waits for a publish. */
inline constexpr AssetHost kAssetHosts[] = {
    {"hol_imgbb", "ImgBB", "uploads the file to ImgBB and returns its link",
     "an ImgBB key (the vault, imgbb.key beside the database, or the node's key_file)",
     false},
    {"hol_object_store", "Object store (S3, Cloudflare R2)",
     "uploads the file to the bucket and returns the bucket's public address for it",
     "a bucket, an access key and its secret, and public_url (an r2.dev address or "
     "your own domain)",
     false},
    {"hol_static_host", "Your website (static host)",
     "copies the file into the website; the link works after the next publish",
     "the website's address, site.base_url (Style > Site)", true},
    {"hol_github", "Your website (GitHub Pages)",
     "copies the file into the website; the link works after the next publish",
     "the website's address, site.base_url (Style > Site)", true},
};

inline const AssetHost* asset_host(const std::string& glyph) {
    for (const auto& h : kAssetHosts)
        if (glyph == h.glyph) return &h;
    return nullptr;
}

} // namespace hormiga::hosting
