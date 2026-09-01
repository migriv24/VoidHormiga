/* vault_smoke.cpp — the credential-store crypto, headless.
 *
 * The security properties that matter, exercised end to end:
 *   1. round-trip: create → set → save → new Vault → unlock(right pass) →
 *      the secret comes back intact;
 *   2. a WRONG passphrase fails (no secret, no crash, no oracle);
 *   3. a TAMPERED envelope fails (AEAD authentication catches it);
 *   4. multiple secrets survive; a re-save with a changed secret round-trips.
 */
#include "../src/platform/vault.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

static int failures = 0;
#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            ++failures;                                                            \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                          \
    } while (0)

int main() {
    namespace fs = std::filesystem;
    CHECK(hormiga::Vault::global_init());

    fs::path vpath = fs::temp_directory_path() / "hormiga-vault-smoke.miga";
    fs::remove(vpath);
    const std::string pass = "correct horse battery staple";
    const std::string secret = "454e6b38b655104f725d7fd9b15d4ac2"; // an api key shape

    // ── 1: create, set, save ────────────────────────────────────────────────
    {
        hormiga::Vault v;
        CHECK(v.create(pass));
        CHECK(v.unlocked());
        v.set("imgbb_key", secret);
        v.set("note", "a second secret with \"quotes\" and \\ backslash");
        CHECK(v.save(vpath.string()));
    }
    CHECK(hormiga::Vault::exists(vpath.string()));

    // the file must NOT contain the plaintext secret anywhere
    {
        std::ifstream in(vpath.string(), std::ios::binary);
        std::string blob((std::istreambuf_iterator<char>(in)), {});
        CHECK(blob.find(secret) == std::string::npos);   // encrypted at rest
        CHECK(blob.find("imgbb_key") == std::string::npos); // keys hidden too
        CHECK(blob.find("MIGA") != std::string::npos);   // but it's our envelope
    }

    // ── 2: unlock with the RIGHT passphrase recovers the secret ─────────────
    {
        hormiga::Vault v;
        CHECK(v.unlock(vpath.string(), pass));
        CHECK(v.unlocked());
        CHECK(v.get("imgbb_key") == secret);
        CHECK(v.get("note") == "a second secret with \"quotes\" and \\ backslash");
        CHECK(v.keys().size() == 2);
    }

    // ── 3: a WRONG passphrase fails cleanly ─────────────────────────────────
    {
        hormiga::Vault v;
        CHECK(!v.unlock(vpath.string(), "wrong passphrase"));
        CHECK(!v.unlocked());
        CHECK(v.get("imgbb_key").empty());
    }

    // ── 4: a TAMPERED ciphertext fails (AEAD authentication) ────────────────
    {
        std::string blob;
        { std::ifstream in(vpath.string(), std::ios::binary);
          blob.assign((std::istreambuf_iterator<char>(in)), {}); }
        auto p = blob.find("\"ct\": \"");
        CHECK(p != std::string::npos);
        blob[p + 8] = (blob[p + 8] == 'A' ? 'B' : 'A'); // flip a ciphertext byte
        fs::path tpath = fs::temp_directory_path() / "hormiga-vault-tampered.miga";
        { std::ofstream out(tpath.string(), std::ios::binary); out << blob; }
        hormiga::Vault v;
        CHECK(!v.unlock(tpath.string(), pass)); // authentication catches it
        fs::remove(tpath);
    }

    // ── 5: re-save with a changed secret round-trips ────────────────────────
    {
        hormiga::Vault v;
        CHECK(v.unlock(vpath.string(), pass));
        v.set("imgbb_key", "deadbeef");
        v.erase("note");
        CHECK(v.save(vpath.string()));
        hormiga::Vault v2;
        CHECK(v2.unlock(vpath.string(), pass));
        CHECK(v2.get("imgbb_key") == "deadbeef");
        CHECK(v2.keys().size() == 1);
    }

    fs::remove(vpath);
    if (failures == 0) {
        std::cout << "OK — vault: argon2id + XChaCha20-Poly1305 round-trip, "
                     "wrong-pass + tamper rejected\n";
        return 0;
    }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
