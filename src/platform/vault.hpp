/* vault.hpp — the passphrase-locked credential store (`.miga` v2, phase F
 * groundwork; okf/concepts/platform/security.md).
 *
 * The org's SECRETS (API keys today; the collaboration keys later) live here,
 * encrypted at rest under a user passphrase — never in the command log, never
 * in exported state, never plaintext beside the app. libsodium only, no
 * hand-rolled primitives, no fallback secret:
 *   - argon2id (crypto_pwhash) derives a key from the passphrase + a random
 *     salt (interactive limits; a slow, memory-hard KDF is the whole point);
 *   - XChaCha20-Poly1305 (crypto_aead_xchacha20poly1305_ietf) seals the
 *     secrets JSON with a random nonce — authenticated, so a wrong passphrase
 *     or a tampered file fails cleanly rather than returning garbage.
 *
 * The on-disk `.miga` is a small JSON envelope: version + KDF params (salt,
 * ops, mem) + nonce + base64 ciphertext. Everything needed to re-derive the
 * key EXCEPT the passphrase, which only the user holds. "Topology plaintext,
 * secrets encrypted" (Antfarm concept): this file is the secrets half; the
 * Antfarm graph stays in the ordinary state document.
 *
 * The plaintext secrets and the derived key live only in memory while
 * unlocked, and are zeroed on lock/destroy (sodium_memzero).
 */
#pragma once

#include <map>
#include <string>
#include <vector>

namespace hormiga {

class Vault {
public:
    Vault() = default;
    ~Vault();
    Vault(const Vault&) = delete;
    Vault& operator=(const Vault&) = delete;

    /* Call once at startup (idempotent). False = libsodium failed to init;
     * treat crypto as unavailable. */
    static bool global_init();

    /* Does an envelope file exist at this path? (Cheap; no crypto.) */
    static bool exists(const std::string& path);

    /* Start a brand-new, empty, unlocked vault under this passphrase. */
    bool create(const std::string& passphrase);

    /* Read the envelope at `path` and decrypt it with `passphrase`. False on
     * a wrong passphrase, a tampered/corrupt file, or a missing file — the
     * caller cannot tell which (deliberate: no oracle). On success the vault
     * is unlocked and its secrets are in memory. */
    bool unlock(const std::string& path, const std::string& passphrase);

    bool unlocked() const { return unlocked_; }

    /* In-memory secret access (only meaningful while unlocked). */
    std::string get(const std::string& key) const;
    void set(const std::string& key, const std::string& value);
    void erase(const std::string& key);
    std::vector<std::string> keys() const;

    /* Re-seal the current secrets and write the envelope to `path`. Re-derives
     * from the passphrase held since create()/unlock() with a FRESH nonce (and
     * a fresh salt if the passphrase changed). False on any crypto/IO failure —
     * the old file is left intact (write-to-temp-then-rename). */
    bool save(const std::string& path);

    /* Change the passphrase (re-derives the key on the next save). */
    void rekey(const std::string& new_passphrase);

    /* Forget the key + plaintext (zeroized). */
    void lock();

    const std::string& error() const { return err_; }

private:
    bool unlocked_ = false;
    std::string passphrase_;                 // held to re-derive on save
    std::vector<unsigned char> salt_;        // argon2id salt (per-vault)
    unsigned long long ops_ = 0, mem_ = 0;   // KDF cost params
    std::map<std::string, std::string> secrets_;
    std::string err_;
};

} // namespace hormiga
