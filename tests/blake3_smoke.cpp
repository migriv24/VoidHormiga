/* blake3_smoke.cpp — the vendored BLAKE3 against its own published vectors.
 *
 * The port in `vendor/blake3/blake3.hpp` was written by hand from the Rust
 * reference implementation, so "it compiles" says nothing at all. These are the
 * BLAKE3 project's own test vectors: the input of length N is the bytes
 * 0,1,2,…,250,0,1,2,… repeated, and the expected digests come from
 * `test_vectors/test_vectors.json` upstream.
 *
 * The lengths are chosen to cross every structural boundary the algorithm has:
 * empty, sub-block, exactly one block, one chunk, one chunk plus a byte (the
 * first parent node), and several chunks (a deeper tree). A hash that is right
 * for 1024 bytes and wrong for 1025 is a hash whose chunk chaining is broken,
 * and that is the bug this file exists to catch.
 *
 * It matters because Cloudflare keys every uploaded asset by this hash. A wrong
 * digest does not fail loudly — it uploads a file under a name the deployment
 * manifest does not reference, and the page 404s.
 */
#include "../vendor/blake3/blake3.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static void check(const char* what, const std::string& got,
                  const std::string& want) {
    if (got == want) {
        std::printf("  ok   %s\n", what);
    } else {
        std::printf("  FAIL %s\n       got  %s\n       want %s\n", what,
                    got.c_str(), want.c_str());
        ++failures;
    }
}

/* The upstream vector input: byte i is (i mod 251). */
static std::string vector_input(size_t n) {
    std::string s;
    s.reserve(n);
    for (size_t i = 0; i < n; ++i) s += (char)(uint8_t)(i % 251);
    return s;
}

int main() {
    struct Case { size_t len; const char* hash; };
    // from BLAKE3's test_vectors/test_vectors.json (first 64 hex chars)
    const Case cases[] = {
        {0, "af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"},
        {1, "2d3adedff11b61f14c886e35afa036736dcd87a74d27b5c1510225d0f592e213"},
        {2, "7b7015bb92cf0b318037702a6cdd81dee41224f734684c2c122cd6359cb1ee63"},
        {3, "e1be4d7a8ab5560aa4199eea339849ba8e293d55ca0a81006726d184519e647f"},
        {4, "f30f5ab28fe047904037f77b6da4fea1e27241c5d132638d8bedce9d40494f32"},
        {63, "e9bc37a594daad83be9470df7f7b3798297c3d834ce80ba85d6e207627b7db7b"},
        {64, "4eed7141ea4a5cd4b788606bd23f46e212af9cacebacdc7d1f4c6dc7f2511b98"},
        {65, "de1e5fa0be70df6d2be8fffd0e99ceaa8eb6e8c93a63f2d8d1c30ecb6b263dee"},
        {1023, "10108970eeda3eb932baac1428c7a2163b0e924c9a9e25b35bba72b28f70bd11"},
        {1024, "42214739f095a406f3fc83deb889744ac00df831c10daa55189b5d121c855af7"},
        {1025, "d00278ae47eb27b34faecf67b4fe263f82d5412916c1ffd97c8cb7fb814b8444"},
        {2048, "e776b6028c7cd22a4d0ba182a8bf62205d2ef576467e838ed6f2529b85fba24a"},
        {3072, "b98cb0ff3623be03326b373de6b9095218513e64f1ee2edd2525c7ad1e5cffd2"},
        {4096, "015094013f57a5277b59d8475c0501042c0b642e531b0a1c8f58d2163229e969"},
        {8192, "aae792484c8efe4f19e2ca7d371d8c467ffb10748d8a5a1ae579948f718a2a63"},
    };

    std::printf("blake3 vectors:\n");
    for (const Case& c : cases) {
        char label[64];
        std::snprintf(label, sizeof label, "input_len %zu", c.len);
        check(label, blake3::hex(vector_input(c.len)), c.hash);
    }

    /* Streaming must agree with one-shot, because the uploader hashes a
     * base64 string built in pieces. */
    {
        const std::string in = vector_input(5000);
        blake3::Hasher h;
        size_t off = 0;
        for (size_t step : {1u, 63u, 64u, 65u, 1000u, 1024u, 1025u}) {
            const size_t take = off + step > in.size() ? in.size() - off : step;
            h.update(in.data() + off, take);
            off += take;
        }
        h.update(in.data() + off, in.size() - off);
        check("streaming == one-shot (5000 bytes, ragged chunks)", h.hex(),
              blake3::hex(in));
    }

    /* The exact shape Cloudflare Pages keys an asset by:
     * blake3(base64(contents) + extension).hex()[:32]. Pinned as a length +
     * determinism check; the digest itself is covered by the vectors above. */
    {
        const std::string b64 = "PGh0bWw+aGk8L2h0bWw+"; // "<html>hi</html>"
        const std::string k = blake3::hex(b64 + "html").substr(0, 32);
        check("pages asset key is 32 hex chars",
              std::to_string(k.size()) + " " +
                  (k.find_first_not_of("0123456789abcdef") == std::string::npos
                       ? "hex"
                       : "NOT-HEX"),
              "32 hex");
    }

    if (failures) {
        std::printf("blake3: %d failure(s)\n", failures);
        return 1;
    }
    std::printf("blake3: all vectors pass\n");
    return 0;
}
