/* backup.hpp — an ENCRYPTED copy of the whole database, for a store that must
 * not be able to read it.
 *
 * ── WHY ──────────────────────────────────────────────────────────────────────
 *
 * okf/concepts/platform/data-planes.md §3 separates two things the phrase "a save in the
 * cloud" can mean, and only one of them is safe:
 *
 *   - a **queryable projection** — rows the vendor can read, containing only
 *     what cleared the publication seam. That is `publish_index`.
 *   - an **encrypted backup** — the whole `.miga`, internal notes and all, as
 *     an opaque blob. That is this file.
 *
 * Both are wanted; they must never be the same store. A queryable cloud
 * database holding the organization's whole database is the failure that page
 * exists to prevent. A blob the vendor cannot read is straightforwardly good,
 * and it is the likelier disaster it protects against: not an attacker, but a
 * volunteer's laptop dying with the only copy of the org's data on it.
 *
 * Because the store never holds the key, this is end-to-end encrypted by
 * construction — S3, a USB stick, a shared drive and somebody's email are all
 * equally acceptable transports for it. That property is what makes the vendor
 * disposable in the strongest sense available.
 *
 * ── NO NEW CRYPTO ────────────────────────────────────────────────────────────
 *
 * libsodium only, mirroring `platform/vault.cpp` exactly — argon2id for the
 * passphrase, XChaCha20-Poly1305 for the data, both through libsodium's own
 * high-level APIs. Nothing here is invented. security.md's rule and CLAUDE.md
 * rule 6 both say the same thing and it is worth restating at every crypto
 * seam: **never hand-roll a primitive, never add a fallback secret.**
 *
 * The one difference from the vault is the shape rather than the algorithm. The
 * vault seals a few kilobytes of JSON in one call and base64s it into an
 * envelope. A `.miga` can be tens of megabytes — a real one is 87 MB — so base64
 * would add a third again for nothing, and a single-shot seal would need the
 * plaintext AND the ciphertext resident at once. So this uses
 * `crypto_secretstream_xchacha20poly1305`, which is libsodium's documented
 * answer for files: chunked, each chunk individually authenticated, and the end
 * of the stream is itself authenticated so a TRUNCATED backup is detected
 * rather than silently restoring a prefix.
 *
 * That last property is the one worth having. A backup that restores 90% of a
 * database without saying so is worse than one that fails.
 *
 * ── THE HAZARD, STATED PLAINLY ───────────────────────────────────────────────
 *
 * There is no recovery path for a forgotten passphrase and there must not be
 * one: a backup the developer could open is a backup the vendor could be
 * compelled to hand over in a readable form. **A lost passphrase is a lost
 * backup.** Anything that reaches an operator must say so before it is their
 * only copy.
 */
#pragma once

#include <cstdint>
#include <string>

namespace hormiga::backup {

struct Result {
    bool ok = false;
    std::string error;      // for a person, not for a parser
    std::uint64_t in = 0;   // plaintext bytes
    std::uint64_t out = 0;  // bytes written
};

/* Seal `src` to `dst` under `passphrase`.
 *
 * Writes to a temporary beside the target and renames, so an interrupted backup
 * never replaces a good one with a partial file — the same rule the vault and
 * the `.miga` writer already follow, and for the same reason. */
Result seal_file(const std::string& src, const std::string& dst,
                 const std::string& passphrase);

/* Open `src` to `dst`. Fails on a wrong passphrase, a tampered file, or a
 * TRUNCATED one — the caller cannot tell which, deliberately, because a message
 * that distinguishes them is an oracle.
 *
 * Never writes a partial `dst`: the output is assembled beside the target and
 * renamed only after the stream's final tag has been verified. */
Result open_file(const std::string& src, const std::string& dst,
                 const std::string& passphrase);

/* Does this look like one of our backups? Reads the magic only; no crypto, no
 * passphrase. For a picker that wants to refuse the wrong file early rather
 * than after a slow key derivation. */
bool looks_like_backup(const std::string& path);

} // namespace hormiga::backup
