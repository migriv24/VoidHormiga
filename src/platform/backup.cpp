/* backup.cpp — see backup.hpp for why this exists and what it promises.
 *
 * The crypto is libsodium's, used through its own high-level file API. Nothing
 * here is invented; the only decisions are the container layout and the chunk
 * size, and both are written down below so a future reader does not have to
 * infer them from the code.
 */
#include "platform/backup.hpp"

#include "sodium.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;

namespace hormiga::backup {
namespace {

/* ── THE CONTAINER ───────────────────────────────────────────────────────────
 *
 *   offset  size  field
 *   0       8     magic, "MIGABKP" + a version byte
 *   8       8     argon2id opslimit   (u64, little-endian)
 *   16      8     argon2id memlimit   (u64, little-endian)
 *   24      16    salt                (crypto_pwhash_SALTBYTES)
 *   40      24    secretstream header (…_HEADERBYTES)
 *   64      …     the stream's chunks
 *
 * BINARY, not the vault's base64-in-JSON envelope. The vault seals a few
 * kilobytes; a `.miga` can be tens of megabytes (a real one is 87 MB) and base64
 * would add a third again to every backup for no benefit — nothing needs to
 * read this in a text editor.
 *
 * Everything needed to re-derive the key is here EXCEPT the passphrase, which
 * only the operator holds. That is the same split the vault makes and it is the
 * whole security property: the store keeps the file and can do nothing with it.
 *
 * The KDF parameters are STORED rather than assumed, so raising them later does
 * not strand every backup taken before the change. A restore uses the file's
 * own numbers. */
constexpr char kMagic[8] = {'M', 'I', 'G', 'A', 'B', 'K', 'P', '\x01'};
constexpr std::size_t kHeaderBytes =
    sizeof kMagic + 8 + 8 + crypto_pwhash_SALTBYTES +
    crypto_secretstream_xchacha20poly1305_HEADERBYTES;

/* 64 KiB of plaintext per chunk. Each chunk costs 17 bytes of tag, so the
 * overhead is ~0.026% — small enough to ignore, and the chunks are small enough
 * that a backup does not need the whole file resident. */
constexpr std::size_t kChunk = 64 * 1024;

void put_u64(unsigned char* p, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (unsigned char)((v >> (8 * i)) & 0xff);
}
std::uint64_t get_u64(const unsigned char* p) {
    std::uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v |= (std::uint64_t)p[i] << (8 * i);
    return v;
}

Result fail(const std::string& why) {
    Result r;
    r.error = why;
    return r;
}

bool init_sodium() { return sodium_init() >= 0; }

} // namespace

bool looks_like_backup(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char m[sizeof kMagic];
    in.read(m, sizeof m);
    return in.gcount() == (std::streamsize)sizeof m &&
           std::memcmp(m, kMagic, sizeof m) == 0;
}

Result seal_file(const std::string& src, const std::string& dst,
                 const std::string& passphrase) {
    if (!init_sodium()) return fail("libsodium failed to initialise");
    if (passphrase.empty())
        /* Refused rather than accepted-and-weak. An empty passphrase produces a
         * file that looks encrypted and is not, which is the worst outcome
         * available here — the operator would believe they were protected. */
        return fail("a backup needs a passphrase - an empty one would produce a "
                    "file that looks encrypted and is not");

    std::ifstream in(src, std::ios::binary);
    if (!in) return fail("cannot read " + src);

    const std::uint64_t ops = crypto_pwhash_OPSLIMIT_INTERACTIVE;
    const std::uint64_t mem = crypto_pwhash_MEMLIMIT_INTERACTIVE;
    unsigned char salt[crypto_pwhash_SALTBYTES];
    randombytes_buf(salt, sizeof salt);

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    if (crypto_pwhash(key, sizeof key, passphrase.c_str(), passphrase.size(),
                      salt, ops, (size_t)mem, crypto_pwhash_ALG_ARGON2ID13) != 0)
        return fail("key derivation failed (out of memory?)");

    crypto_secretstream_xchacha20poly1305_state st;
    unsigned char sh[crypto_secretstream_xchacha20poly1305_HEADERBYTES];
    crypto_secretstream_xchacha20poly1305_init_push(&st, sh, key);

    /* WRITE TO A TEMPORARY AND RENAME. An interrupted backup must never replace
     * a good one with a partial file — the same rule the vault and the `.miga`
     * writer already follow, and the one that matters most here because the
     * thing being overwritten may be the only other copy. */
    const fs::path tmp = fs::path(dst).string() + ".part";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
        sodium_memzero(key, sizeof key);
        return fail("cannot write " + tmp.string());
    }

    unsigned char hdr[kHeaderBytes];
    std::memcpy(hdr, kMagic, sizeof kMagic);
    put_u64(hdr + 8, ops);
    put_u64(hdr + 16, mem);
    std::memcpy(hdr + 24, salt, sizeof salt);
    std::memcpy(hdr + 24 + sizeof salt, sh, sizeof sh);
    out.write((const char*)hdr, sizeof hdr);

    Result r;
    std::vector<unsigned char> plain(kChunk);
    std::vector<unsigned char> ct(kChunk +
                                  crypto_secretstream_xchacha20poly1305_ABYTES);
    for (;;) {
        in.read((char*)plain.data(), (std::streamsize)plain.size());
        const std::size_t n = (std::size_t)in.gcount();
        const bool last = !in || in.peek() == EOF;
        unsigned long long ct_len = 0;
        const unsigned char tag =
            last ? crypto_secretstream_xchacha20poly1305_TAG_FINAL : 0;
        crypto_secretstream_xchacha20poly1305_push(&st, ct.data(), &ct_len,
                                                   plain.data(), n, nullptr, 0,
                                                   tag);
        out.write((const char*)ct.data(), (std::streamsize)ct_len);
        r.in += n;
        r.out += ct_len;
        if (last) break;
    }
    sodium_memzero(key, sizeof key);
    sodium_memzero(plain.data(), plain.size());
    out.flush();
    if (!out) {
        std::error_code ec;
        fs::remove(tmp, ec);
        return fail("write failed (disk full?): " + tmp.string());
    }
    out.close();

    std::error_code ec;
    fs::rename(tmp, dst, ec);
    if (ec) {
        fs::remove(dst, ec);
        fs::rename(tmp, dst, ec);   // Windows will not rename onto an existing file
        if (ec) {
            fs::remove(tmp, ec);
            return fail("cannot replace " + dst);
        }
    }
    r.out += kHeaderBytes;
    r.ok = true;
    return r;
}

Result open_file(const std::string& src, const std::string& dst,
                 const std::string& passphrase) {
    if (!init_sodium()) return fail("libsodium failed to initialise");

    std::ifstream in(src, std::ios::binary);
    if (!in) return fail("cannot read " + src);

    unsigned char hdr[kHeaderBytes];
    in.read((char*)hdr, sizeof hdr);
    if (in.gcount() != (std::streamsize)sizeof hdr ||
        std::memcmp(hdr, kMagic, sizeof kMagic) != 0)
        return fail(src + " is not a Hormiga backup");

    const std::uint64_t ops = get_u64(hdr + 8);
    const std::uint64_t mem = get_u64(hdr + 16);
    const unsigned char* salt = hdr + 24;
    const unsigned char* sh = hdr + 24 + crypto_pwhash_SALTBYTES;

    unsigned char key[crypto_secretstream_xchacha20poly1305_KEYBYTES];
    if (crypto_pwhash(key, sizeof key, passphrase.c_str(), passphrase.size(),
                      salt, ops, (size_t)mem, crypto_pwhash_ALG_ARGON2ID13) != 0)
        return fail("key derivation failed (out of memory?)");

    crypto_secretstream_xchacha20poly1305_state st;
    if (crypto_secretstream_xchacha20poly1305_init_pull(&st, sh, key) != 0) {
        sodium_memzero(key, sizeof key);
        return fail("this backup could not be opened - wrong passphrase, or the "
                    "file is damaged");
    }

    /* ASSEMBLE BESIDE THE TARGET AND RENAME ONLY ON SUCCESS. A restore that
     * writes as it goes would leave a truncated database behind when the stream
     * turns out to be damaged three quarters of the way through — and a
     * truncated database is exactly what a person restoring a backup cannot
     * afford to be handed. */
    const fs::path tmp = fs::path(dst).string() + ".part";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
        sodium_memzero(key, sizeof key);
        return fail("cannot write " + tmp.string());
    }

    Result r;
    std::vector<unsigned char> ct(kChunk +
                                  crypto_secretstream_xchacha20poly1305_ABYTES);
    std::vector<unsigned char> plain(kChunk);
    bool finished = false;
    std::error_code ec;
    for (;;) {
        in.read((char*)ct.data(), (std::streamsize)ct.size());
        const std::size_t n = (std::size_t)in.gcount();
        if (n == 0) break;
        unsigned long long pt_len = 0;
        unsigned char tag = 0;
        if (crypto_secretstream_xchacha20poly1305_pull(
                &st, plain.data(), &pt_len, &tag, ct.data(), n, nullptr, 0) != 0) {
            sodium_memzero(key, sizeof key);
            out.close();
            fs::remove(tmp, ec);
            return fail("this backup could not be opened - wrong passphrase, or "
                        "the file is damaged");
        }
        out.write((const char*)plain.data(), (std::streamsize)pt_len);
        r.out += pt_len;
        r.in += n;
        if (tag == crypto_secretstream_xchacha20poly1305_TAG_FINAL) {
            finished = true;
            break;
        }
    }
    sodium_memzero(key, sizeof key);
    sodium_memzero(plain.data(), plain.size());

    /* THE TRUNCATION CHECK, and the reason `secretstream` was chosen over a
     * single-shot seal. Every chunk authenticates itself, so a corrupted one is
     * caught above — but a backup that was cut short has no corrupted chunk at
     * all, only a missing end. The FINAL tag is what makes "the file stops here"
     * distinguishable from "the file ends here", and without this check a
     * truncated backup would restore a plausible prefix in silence. */
    if (!finished) {
        out.close();
        fs::remove(tmp, ec);
        return fail("this backup is incomplete - it was cut short, and "
                    "restoring part of a database is worse than restoring none");
    }
    out.flush();
    if (!out) {
        out.close();
        fs::remove(tmp, ec);
        return fail("write failed (disk full?): " + tmp.string());
    }
    out.close();

    fs::rename(tmp, dst, ec);
    if (ec) {
        fs::remove(dst, ec);
        fs::rename(tmp, dst, ec);
        if (ec) {
            fs::remove(tmp, ec);
            return fail("cannot replace " + dst);
        }
    }
    r.ok = true;
    return r;
}

} // namespace hormiga::backup
