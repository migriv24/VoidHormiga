/* argon2id + XChaCha20-Poly1305 round trip: what Hormiga's vault does. */
#include <sodium.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    if (sodium_init() < 0) return puts("init failed"), 1;
    unsigned char salt[crypto_pwhash_SALTBYTES] = {0}, key[32];
    if (crypto_pwhash(key, sizeof key, "pass", 4, salt, crypto_pwhash_OPSLIMIT_INTERACTIVE,
                      crypto_pwhash_MEMLIMIT_INTERACTIVE, crypto_pwhash_ALG_ARGON2ID13) != 0)
        return puts("argon2id failed"), 1;
    unsigned char n[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES], c[64], m[16];
    unsigned long long cl, ml;
    randombytes_buf(n, sizeof n);
    crypto_aead_xchacha20poly1305_ietf_encrypt(c, &cl, (const unsigned char*)"hello", 5, 0, 0, 0, n, key);
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(m, &ml, 0, c, cl, 0, 0, n, key) != 0 || ml != 5 ||
        memcmp(m, "hello", 5) != 0)
        return puts("aead failed"), 1;
    printf("sodium ok %s\n", sodium_version_string());
    return 0;
}
