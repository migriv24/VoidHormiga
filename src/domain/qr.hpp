/* domain/qr.hpp — a link, as a QR code.
 *
 * The author (2026-09-13): *"a QR code generator. You give it a link, and it
 * makes a qr code. This isn't even directly linked to hormiga, I just want it
 * (it could potentially have usefulness for some newsletter feature later on)."*
 *
 * The encoding is Project Nayuki's QR Code generator, vendored under
 * `vendor/qrcodegen/` (MIT). QR encoding is Reed-Solomon error correction, eight
 * mask patterns scored by a penalty function and a version table — exactly the
 * code where "it compiles" means nothing and a hand-written encoder produces a
 * picture that no phone will read. Ground rule 5 says vendor it; this file is
 * only the part that is ours: a padded module grid and a pixel buffer.
 *
 * Pure: text in, pixels out. The GUI draws the grid and `stb_image_write` saves
 * it, and `tests/niche_smoke.cpp` checks it without a window — which is the
 * seam that would let a newsletter block put one on a page later without a
 * second encoder.
 */
#pragma once

#include "qrcodegen.hpp"

#include <exception>
#include <string>
#include <vector>

namespace hormiga::qr {

enum class Ecc { Low, Medium, Quartile, High };

struct Matrix {
    int size = 0;           // modules per side, INCLUDING the quiet-zone border
    std::vector<bool> dark; // row-major: dark[y * size + x]
    std::string error;      // non-empty when nothing could be encoded

    bool at(int x, int y) const {
        return x >= 0 && y >= 0 && x < size && y < size && dark[(size_t)(y * size + x)];
    }
};

/* The code for `text`, with a quiet zone of `border` light modules on each
 * side. The QR specification asks for four; a scanner given less may fail to
 * find the code at all, which is why four is the default rather than zero.
 *
 * Refuses an empty string rather than encoding it: a valid QR code that opens
 * nothing is worse than no code, because it looks like it works. */
inline Matrix make(const std::string& text, Ecc ecc = Ecc::Medium, int border = 4) {
    Matrix m;
    if (text.empty()) {
        m.error = "nothing to encode";
        return m;
    }
    using Q = qrcodegen::QrCode;
    const Q::Ecc level = ecc == Ecc::Low        ? Q::Ecc::LOW
                         : ecc == Ecc::Quartile ? Q::Ecc::QUARTILE
                         : ecc == Ecc::High     ? Q::Ecc::HIGH
                                                : Q::Ecc::MEDIUM;
    try {
        const Q code = Q::encodeText(text.c_str(), level);
        if (border < 0) border = 0;
        m.size = code.getSize() + 2 * border;
        m.dark.assign((size_t)(m.size * m.size), false);
        for (int y = 0; y < code.getSize(); ++y)
            for (int x = 0; x < code.getSize(); ++x)
                m.dark[(size_t)((y + border) * m.size + (x + border))] = code.getModule(x, y);
    } catch (const qrcodegen::data_too_long&) {
        m = Matrix{};
        m.error = "too long for a QR code at this error-correction level";
    } catch (const std::exception& e) {
        m = Matrix{};
        m.error = e.what();
    }
    return m;
}

/* 8-bit RGB, `scale` pixels per module, dark modules black on white. */
inline std::vector<unsigned char> to_rgb(const Matrix& m, int scale) {
    if (scale < 1) scale = 1;
    const int w = m.size * scale;
    std::vector<unsigned char> px((size_t)w * (size_t)w * 3, 255);
    for (int y = 0; y < m.size; ++y)
        for (int x = 0; x < m.size; ++x) {
            if (!m.at(x, y)) continue;
            for (int dy = 0; dy < scale; ++dy)
                for (int dx = 0; dx < scale; ++dx) {
                    const size_t i =
                        ((size_t)((y * scale + dy) * w + (x * scale + dx))) * 3;
                    px[i] = px[i + 1] = px[i + 2] = 0;
                }
        }
    return px;
}

} // namespace hormiga::qr
