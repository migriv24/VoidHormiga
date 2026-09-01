/* backup_smoke.cpp — the encrypted backup, tested against the ways it can fail
 * QUIETLY.
 *
 * This guards the organization's entire database: the whole `.miga`, internal
 * notes and all, sitting in a store the vendor can read if we get this wrong.
 * "It round-trips" is the least interesting thing to assert about it, and is
 * roughly the only thing that would be asserted by accident.
 *
 * So the suite is written around the three failures that produce a WRONG ANSWER
 * rather than an error, because those are the ones a person cannot notice:
 *
 *   1. a wrong passphrase that returns garbage instead of refusing;
 *   2. a tampered file that opens anyway;
 *   3. a TRUNCATED file that restores a plausible prefix in silence.
 *
 * The third is the reason `backup.cpp` uses libsodium's `secretstream` rather
 * than a single-shot seal. A cut-short backup contains no corrupted chunk at
 * all — every byte it does have is authentic — so per-chunk authentication does
 * not catch it. Only the stream's FINAL tag distinguishes "the file stops here"
 * from "the file ends here". A restore that hands somebody 90% of their
 * database without saying so is worse than one that fails.
 *
 * Multi-chunk sizes are used deliberately: the chunk is 64 KiB, and framing
 * bugs live exactly at the boundaries, so the fixtures straddle them.
 */
#include "platform/backup.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace hormiga::backup;

static int failures = 0;
static void ok(bool cond, const char* what) {
    std::printf("  %s %s\n", cond ? "ok  " : "FAIL", what);
    if (!cond) ++failures;
}

static std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}
static void spew(const fs::path& p, const std::string& s) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o.write(s.data(), (std::streamsize)s.size());
}

/* Deterministic pseudo-random bytes: incompressible enough to be a fair
 * stand-in for a real `.miga` (which is mostly base64 image data), and
 * reproducible so a failure can be re-run. */
static std::string blob(std::size_t n, unsigned seed) {
    std::string s;
    s.resize(n);
    unsigned x = seed * 2654435761u + 1;
    for (std::size_t i = 0; i < n; ++i) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        s[i] = (char)(x & 0xff);
    }
    return s;
}

int main() {
    const fs::path dir = fs::temp_directory_path() / "hormiga_backup_smoke";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);

    const std::string pass = "correct horse battery staple";

    // ── 1. round trip, across the chunk boundary ────────────────────────────
    std::printf("backup: round trip\n");
    struct Case { const char* what; std::size_t n; };
    const Case cases[] = {
        {"an empty database", 0},
        {"one byte", 1},
        {"just under one chunk", 64 * 1024 - 1},
        {"exactly one chunk", 64 * 1024},
        {"one chunk plus a byte (the first framing boundary)", 64 * 1024 + 1},
        {"several chunks", 200 * 1024 + 7},
    };
    for (const Case& c : cases) {
        const fs::path src = dir / "src.bin", enc = dir / "enc.bkp",
                       out = dir / "out.bin";
        const std::string data = blob(c.n, (unsigned)c.n + 1);
        spew(src, data);
        Result s = seal_file(src.string(), enc.string(), pass);
        Result o = open_file(enc.string(), out.string(), pass);
        const bool same = s.ok && o.ok && slurp(out) == data;
        std::printf("  %s %s (%zu bytes)\n", same ? "ok  " : "FAIL", c.what, c.n);
        if (!same) ++failures;
    }

    // ── 2. the ciphertext does not contain the plaintext ────────────────────
    std::printf("backup: it is actually encrypted\n");
    {
        const fs::path src = dir / "p.bin", enc = dir / "p.bkp";
        // a recognisable secret of the kind a real .miga is full of
        const std::string secret =
            "ana@example.org INTERNAL: do not call after 6pm 555-0100";
        std::string data = blob(80 * 1024, 7);
        data += secret;
        data += blob(80 * 1024, 9);
        spew(src, data);
        ok(seal_file(src.string(), enc.string(), pass).ok, "seals a database");
        const std::string ct = slurp(enc);
        ok(ct.find(secret) == std::string::npos,
           "the plaintext does not appear in the backup");
        ok(ct.find("ana@example.org") == std::string::npos,
           "...nor does an email inside it");
        ok(looks_like_backup(enc.string()), "the magic identifies it as ours");
        ok(!looks_like_backup(src.string()), "...and does not claim a plain file");
    }

    // ── 3. a wrong passphrase REFUSES rather than returning garbage ─────────
    std::printf("backup: a wrong passphrase\n");
    {
        const fs::path src = dir / "w.bin", enc = dir / "w.bkp",
                       out = dir / "w.out";
        spew(src, blob(150 * 1024, 3));
        seal_file(src.string(), enc.string(), pass);
        Result o = open_file(enc.string(), out.string(), "wrong passphrase");
        ok(!o.ok, "is refused");
        ok(!fs::exists(out, ec),
           "...and leaves NO output file - never a partial database");
        // and the message must not distinguish wrong-key from damaged-file
        ok(o.error.find("wrong passphrase, or") != std::string::npos,
           "...with a message that is not an oracle");
    }

    // ── 4. TAMPERING is detected ────────────────────────────────────────────
    std::printf("backup: a tampered file\n");
    {
        const fs::path src = dir / "t.bin", enc = dir / "t.bkp",
                       out = dir / "t.out";
        spew(src, blob(150 * 1024, 11));
        seal_file(src.string(), enc.string(), pass);
        std::string ct = slurp(enc);
        ct[ct.size() / 2] = (char)(ct[ct.size() / 2] ^ 0x01); // one bit
        spew(enc, ct);
        ok(!open_file(enc.string(), out.string(), pass).ok,
           "one flipped bit is caught");
        ok(!fs::exists(out, ec), "...and nothing is written");
    }

    // ── 5. TRUNCATION is detected — the one a single-shot seal would miss ──
    std::printf("backup: a truncated file (the quiet one)\n");
    {
        const fs::path src = dir / "c.bin", enc = dir / "c.bkp",
                       out = dir / "c.out";
        spew(src, blob(300 * 1024, 13));
        seal_file(src.string(), enc.string(), pass);
        std::string ct = slurp(enc);
        /* Cut on a CHUNK BOUNDARY, which is the hard case: every chunk that
         * remains is complete and authentic, so per-chunk authentication is
         * perfectly happy. Only the missing FINAL tag reveals it. This is what
         * an interrupted upload or a partial download actually looks like. */
        const std::size_t frame = 64 * 1024 + 17;   // chunk + ABYTES
        ct.resize(64 + frame * 2);                  // header + two whole chunks
        spew(enc, ct);
        Result o = open_file(enc.string(), out.string(), pass);
        ok(!o.ok, "a backup cut on a chunk boundary is refused");
        ok(o.error.find("incomplete") != std::string::npos,
           "...and says it is incomplete, not that the passphrase is wrong");
        ok(!fs::exists(out, ec),
           "...and restores NOTHING rather than a plausible prefix");
    }

    // ── 6. refusals that are not crypto ─────────────────────────────────────
    std::printf("backup: ordinary refusals\n");
    {
        const fs::path src = dir / "e.bin", enc = dir / "e.bkp",
                       out = dir / "e.out";
        spew(src, blob(1024, 17));
        ok(!seal_file(src.string(), enc.string(), "").ok,
           "an EMPTY passphrase is refused, not silently accepted");
        ok(!seal_file((dir / "nope.bin").string(), enc.string(), pass).ok,
           "a missing source is refused");
        spew(enc, std::string("not a backup at all, just some text"));
        Result o = open_file(enc.string(), out.string(), pass);
        ok(!o.ok && o.error.find("not a Hormiga backup") != std::string::npos,
           "a file that is not a backup is named as such, before any crypto");
    }

    fs::remove_all(dir, ec);
    std::printf(failures ? "\nFAILED (%d)\n" : "\nOK - the backup holds\n",
                failures);
    return failures ? 1 : 0;
}
