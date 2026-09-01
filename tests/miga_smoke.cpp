/* miga_smoke.cpp — the .miga v3 bundle round-trips (okf/concepts/platform/miga-format.md).
 * Pack a state + an asset into a bundle, unpack into a fresh working copy, and
 * assert the state and the asset's exact bytes survive. */
#include "../src/platform/miga.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(c)                                                                \
    do {                                                                        \
        if (!(c)) { ++failures; std::cerr << "FAIL " << __LINE__ << " " #c "\n"; } \
    } while (0)

int main() {
    fs::path base = fs::temp_directory_path() / "hormiga-miga-a";
    fs::remove_all(base);
    fs::create_directories(base / "assets" / "sub");
    const std::string bytes = "\x89PNG\r\n\x1a\n binary-ish asset bytes \0 with a NUL";
    { std::ofstream f(base / "assets" / "sub" / "logo.png", std::ios::binary);
      f.write(bytes.data(), (std::streamsize)bytes.size()); }

    const std::string state =
        R"({"mantles":[{"spirit":{"name":"demo-org"},"runes":[]},)"
        R"({"spirit":{"name":"my-newsletter"},"runes":[]}],"config":{"ui.scale":"1.2"}})";
    fs::path miga = base / "db.miga";
    auto pr = hormiga::miga::pack(state, base, miga, "my-org", base / "assets");
    CHECK(pr.ok);
    CHECK(pr.assets == 1);
    CHECK(pr.bytes > 0);
    CHECK(fs::exists(miga));

    // unpack into a DIFFERENT working copy (a real "open on another machine")
    fs::path base2 = fs::temp_directory_path() / "hormiga-miga-b";
    fs::remove_all(base2);
    fs::create_directories(base2);
    auto orr = hormiga::miga::open(miga, base2, base2 / "assets");
    CHECK(orr.ok);
    CHECK(orr.version == 3);
    CHECK(orr.name == "my-org");
    CHECK(orr.assets == 1);
    CHECK(orr.state.find("demo-org") != std::string::npos);
    CHECK(orr.state.find("my-newsletter") != std::string::npos); // all mantles

    fs::path restored = base2 / "assets" / "sub" / "logo.png";
    CHECK(fs::exists(restored));
    if (fs::exists(restored)) {
        std::ifstream f(restored, std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        CHECK(got == bytes); // base64 preserved every byte, NUL included
    }

    fs::remove_all(base);
    fs::remove_all(base2);
    if (failures == 0) { std::cout << "OK — .miga v3 pack/unpack round-trip\n"; return 0; }
    std::cerr << failures << " check(s) failed\n";
    return 1;
}
