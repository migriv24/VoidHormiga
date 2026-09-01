/* blake3.hpp — BLAKE3, ported from the reference implementation.
 *
 * Source: https://github.com/BLAKE3-team/BLAKE3 `reference_impl/reference_impl.rs`
 * Licence: CC0 1.0 Universal (public domain) — see LICENSE-CC0.txt beside this
 * file. Ported from Rust to C++ by hand; the algorithm is unchanged and is
 * verified against the project's own published test vectors in
 * `tests/blake3_smoke.cpp`.
 *
 * ── WHY HORMIGA CONTAINS A HASH FUNCTION ─────────────────────────────────────
 *
 * Cloudflare Pages' Direct Upload protocol keys every asset by
 *
 *     blake3(base64(file_contents) + extension_without_dot).hex()[:32]
 *
 * so publishing a website natively — without npm, Node, wrangler, or a Python
 * wrapper living in the operator's folder — requires exactly this hash and no
 * other. libsodium, which this project already vendors, has BLAKE2b and not
 * BLAKE3; they are different functions and the server will not accept one for
 * the other.
 *
 * THE REFERENCE IMPLEMENTATION ON PURPOSE, not the optimised one. The optimised
 * C library is several thousand lines with SIMD dispatch and per-platform
 * assembly, which is a large surface to vendor for hashing a folder of HTML
 * that is measured in megabytes once per publish. This is ~200 lines, portable,
 * and obviously correct against the published vectors — and if a site ever gets
 * big enough for the speed to matter, the optimised library drops in behind the
 * same two functions.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace blake3 {

inline constexpr size_t kOutLen = 32;
inline constexpr size_t kBlockLen = 64;
inline constexpr size_t kChunkLen = 1024;

inline constexpr uint32_t kChunkStart = 1u << 0;
inline constexpr uint32_t kChunkEnd = 1u << 1;
inline constexpr uint32_t kParent = 1u << 2;
inline constexpr uint32_t kRoot = 1u << 3;

inline constexpr uint32_t kIV[8] = {0x6A09E667u, 0xBB67AE85u, 0x3C6EF372u,
                                    0xA54FF53Au, 0x510E527Fu, 0x9B05688Cu,
                                    0x1F83D9ABu, 0x5BE0CD19u};

inline constexpr size_t kMsgPermutation[16] = {2, 6,  3,  10, 7, 0, 4,  13,
                                               1, 11, 12, 5,  9, 14, 15, 8};

inline uint32_t rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

inline void g(uint32_t s[16], size_t a, size_t b, size_t c, size_t d,
              uint32_t mx, uint32_t my) {
    s[a] = s[a] + s[b] + mx;
    s[d] = rotr(s[d] ^ s[a], 16);
    s[c] = s[c] + s[d];
    s[b] = rotr(s[b] ^ s[c], 12);
    s[a] = s[a] + s[b] + my;
    s[d] = rotr(s[d] ^ s[a], 8);
    s[c] = s[c] + s[d];
    s[b] = rotr(s[b] ^ s[c], 7);
}

inline void round_fn(uint32_t s[16], const uint32_t m[16]) {
    g(s, 0, 4, 8, 12, m[0], m[1]);
    g(s, 1, 5, 9, 13, m[2], m[3]);
    g(s, 2, 6, 10, 14, m[4], m[5]);
    g(s, 3, 7, 11, 15, m[6], m[7]);
    g(s, 0, 5, 10, 15, m[8], m[9]);
    g(s, 1, 6, 11, 12, m[10], m[11]);
    g(s, 2, 7, 8, 13, m[12], m[13]);
    g(s, 3, 4, 9, 14, m[14], m[15]);
}

inline void permute(uint32_t m[16]) {
    uint32_t p[16];
    for (size_t i = 0; i < 16; ++i) p[i] = m[kMsgPermutation[i]];
    std::memcpy(m, p, sizeof p);
}

/* The compression function: 7 rounds, with a permutation between each. */
inline void compress(const uint32_t cv[8], const uint32_t block[16],
                     uint64_t counter, uint32_t block_len, uint32_t flags,
                     uint32_t out[16]) {
    uint32_t s[16] = {cv[0],
                      cv[1],
                      cv[2],
                      cv[3],
                      cv[4],
                      cv[5],
                      cv[6],
                      cv[7],
                      kIV[0],
                      kIV[1],
                      kIV[2],
                      kIV[3],
                      (uint32_t)(counter & 0xFFFFFFFFull),
                      (uint32_t)(counter >> 32),
                      block_len,
                      flags};
    uint32_t m[16];
    std::memcpy(m, block, sizeof m);
    for (int r = 0; r < 7; ++r) {
        round_fn(s, m);
        if (r < 6) permute(m);
    }
    for (size_t i = 0; i < 8; ++i) {
        s[i] ^= s[i + 8];
        s[i + 8] ^= cv[i];
    }
    std::memcpy(out, s, sizeof s);
}

inline void words_from_le(const uint8_t* b, size_t n, uint32_t* out) {
    for (size_t i = 0; i < n / 4; ++i)
        out[i] = (uint32_t)b[4 * i] | ((uint32_t)b[4 * i + 1] << 8) |
                 ((uint32_t)b[4 * i + 2] << 16) | ((uint32_t)b[4 * i + 3] << 24);
}

/* An Output is a not-yet-finalised node: it knows how to produce either a
 * chaining value (for a parent) or the root hash. */
struct Output {
    uint32_t input_cv[8];
    uint32_t block[16];
    uint64_t counter = 0;
    uint32_t block_len = 0;
    uint32_t flags = 0;

    void chaining_value(uint32_t out[8]) const {
        uint32_t full[16];
        compress(input_cv, block, counter, block_len, flags, full);
        std::memcpy(out, full, 8 * sizeof(uint32_t));
    }

    void root_bytes(uint8_t* out, size_t len) const {
        uint64_t ctr = 0;
        size_t done = 0;
        while (done < len) {
            uint32_t full[16];
            compress(input_cv, block, ctr, block_len, flags | kRoot, full);
            for (size_t i = 0; i < 16 && done < len; ++i) {
                for (int b = 0; b < 4 && done < len; ++b)
                    out[done++] = (uint8_t)((full[i] >> (8 * b)) & 0xFF);
            }
            ++ctr;
        }
    }
};

struct ChunkState {
    uint32_t cv[8];
    uint64_t chunk_counter = 0;
    uint8_t buf[kBlockLen] = {};
    size_t buf_len = 0;
    size_t blocks_compressed = 0;
    uint32_t flags = 0;

    ChunkState(const uint32_t key[8], uint64_t counter, uint32_t f)
        : chunk_counter(counter), flags(f) {
        std::memcpy(cv, key, 8 * sizeof(uint32_t));
    }

    size_t len() const { return kBlockLen * blocks_compressed + buf_len; }
    uint32_t start_flag() const { return blocks_compressed == 0 ? kChunkStart : 0; }

    void update(const uint8_t* in, size_t n) {
        while (n > 0) {
            if (buf_len == kBlockLen) {
                uint32_t bw[16];
                words_from_le(buf, kBlockLen, bw);
                uint32_t out[16];
                compress(cv, bw, chunk_counter, kBlockLen, flags | start_flag(),
                         out);
                std::memcpy(cv, out, 8 * sizeof(uint32_t));
                ++blocks_compressed;
                buf_len = 0;
                std::memset(buf, 0, sizeof buf);
            }
            const size_t take = kBlockLen - buf_len < n ? kBlockLen - buf_len : n;
            std::memcpy(buf + buf_len, in, take);
            buf_len += take;
            in += take;
            n -= take;
        }
    }

    Output output() const {
        Output o;
        std::memcpy(o.input_cv, cv, 8 * sizeof(uint32_t));
        std::memset(o.block, 0, sizeof o.block);
        words_from_le(buf, kBlockLen, o.block);
        o.counter = chunk_counter;
        o.block_len = (uint32_t)buf_len;
        o.flags = flags | start_flag() | kChunkEnd;
        return o;
    }
};

inline Output parent_output(const uint32_t left[8], const uint32_t right[8],
                            const uint32_t key[8], uint32_t flags) {
    Output o;
    std::memcpy(o.input_cv, key, 8 * sizeof(uint32_t));
    for (size_t i = 0; i < 8; ++i) {
        o.block[i] = left[i];
        o.block[i + 8] = right[i];
    }
    o.counter = 0;
    o.block_len = kBlockLen;
    o.flags = kParent | flags;
    return o;
}

class Hasher {
  public:
    Hasher() : chunk_(kIV, 0, 0) { std::memcpy(key_, kIV, sizeof key_); }

    void update(const void* data, size_t n) {
        const uint8_t* in = (const uint8_t*)data;
        while (n > 0) {
            if (chunk_.len() == kChunkLen) {
                uint32_t cv[8];
                chunk_.output().chaining_value(cv);
                add_chunk_cv(cv, chunk_.chunk_counter + 1);
                chunk_ = ChunkState(key_, chunk_.chunk_counter + 1, 0);
            }
            const size_t room = kChunkLen - chunk_.len();
            const size_t take = room < n ? room : n;
            chunk_.update(in, take);
            in += take;
            n -= take;
        }
    }

    void update(const std::string& s) { update(s.data(), s.size()); }

    void finalize(uint8_t* out, size_t len) const {
        Output o = chunk_.output();
        size_t remaining = stack_len_;
        while (remaining > 0) {
            --remaining;
            uint32_t cv[8];
            o.chaining_value(cv);
            o = parent_output(stack_[remaining], cv, key_, 0);
        }
        o.root_bytes(out, len);
    }

    std::string hex(size_t bytes = kOutLen) const {
        std::vector<uint8_t> buf(bytes);
        finalize(buf.data(), bytes);
        static const char* h = "0123456789abcdef";
        std::string s;
        s.reserve(bytes * 2);
        for (uint8_t b : buf) {
            s += h[b >> 4];
            s += h[b & 0xF];
        }
        return s;
    }

  private:
    void add_chunk_cv(uint32_t cv[8], uint64_t total_chunks) {
        /* Merge as many pairs as the binary counter says are complete: a
         * chaining value joins its neighbour whenever the chunk count has a
         * trailing zero bit, which is what keeps the tree balanced without
         * holding every node in memory. */
        uint32_t work[8];
        std::memcpy(work, cv, sizeof work);
        while ((total_chunks & 1) == 0) {
            --stack_len_;
            uint32_t merged[8];
            parent_output(stack_[stack_len_], work, key_, 0).chaining_value(merged);
            std::memcpy(work, merged, sizeof work);
            total_chunks >>= 1;
        }
        std::memcpy(stack_[stack_len_], work, sizeof work);
        ++stack_len_;
    }

    uint32_t key_[8];
    ChunkState chunk_;
    uint32_t stack_[54][8] = {};
    size_t stack_len_ = 0;
};

/* One-shot: the hex digest of a buffer. */
inline std::string hex(const void* data, size_t n, size_t out_bytes = kOutLen) {
    Hasher h;
    h.update(data, n);
    return h.hex(out_bytes);
}

inline std::string hex(const std::string& s, size_t out_bytes = kOutLen) {
    return hex(s.data(), s.size(), out_bytes);
}

} // namespace blake3
