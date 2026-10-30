/* store_bench.cpp — what opening a database into the store costs, on a real file.
 *
 * A measurement, not a test (okf/concepts/platform/store.md). Built on demand:
 *     cmake --build build --target hormiga_store_bench
 *     hormiga_store_bench big.miga [store.db]
 *
 * Imports the file as a stream, then writes it out whole and as one cluster,
 * and reports the time and the PEAK extra memory of each step: the numbers a
 * 2 GB device has to live within. Pictures are counted and dropped. */
#include "platform/process_memory.hpp"
#include "store/bundle.hpp"
#include "store/store.hpp"
#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using clk = std::chrono::steady_clock;

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: hormiga_store_bench <bundle.miga | state.json> [store]\n"; return 2; }
    const fs::path src = argv[1];
    const fs::path db = argc > 2 ? fs::path(argv[2]) : fs::temp_directory_path() / "hormiga-store-bench.db";
    std::error_code ec;
    for (const char* s : {"", "-wal", "-shm"}) fs::remove(db.string() + s, ec);
    const double mb = 1024.0 * 1024.0;

    const auto base = hormiga::memory::process_bytes();
    std::uint64_t peak = base;
    auto sample = [&] { peak = std::max(peak, hormiga::memory::process_bytes()); };

    hormiga::store::Store st(db);
    const auto t0 = clk::now();
    hormiga::store::JsonStream in(src.string());
    long long asset_bytes = 0;
    const auto stats = st.import(in, [&](double, const char*) { sample(); },
                                 [&](const std::string&, const std::string& b) { asset_bytes += (long long)b.size(); });
    sample();
    const double import_s = std::chrono::duration<double>(clk::now() - t0).count();
    if (!stats.ok) { std::cerr << "import failed: " << stats.error << "\n"; return 1; }
    std::printf("import   %7.1f s  peak +%6.1f MB   %lld runes, %lld edges, %lld tags, %lld assets (%.1f MB)\n",
                import_s, (double)(peak - base) / mb, stats.runes, stats.edges, stats.tags, stats.assets,
                (double)asset_bytes / mb);
    std::printf("store    %7.1f MB on disk (file %.1f MB)\n", (double)fs::file_size(db, ec) / mb,
                (double)fs::file_size(src, ec) / mb);

    // one cluster, as a working set would load it
    for (const auto& m : st.mantles()) {
        if (m.runes < 5000) continue;
        hormiga::store::Selection sel = hormiga::store::Selection::none();
        sel.by_tag[m.name] = {"c1:0"};
        const auto before = hormiga::memory::process_bytes();
        peak = before;
        const auto t1 = clk::now();
        const std::string part = st.state(sel);
        sample();
        std::printf("cluster  %7.3f s  peak +%6.1f MB   %.1f KB of state (mantle %s, c1:0)\n",
                    std::chrono::duration<double>(clk::now() - t1).count(), (double)(peak - before) / mb,
                    (double)part.size() / 1024.0, m.name.c_str());
        // what that text costs once Core holds it, and once it is projected: the
        // expansion the working-set budget is computed with
        const auto m0 = hormiga::memory::process_bytes();
        maiz::Core core(part);
        const auto m1 = hormiga::memory::process_bytes();
        const maiz::Scene scene = maiz::project_scene(core);
        const auto m2 = hormiga::memory::process_bytes();
        core.dispatch("set " + (scene.nodes.empty() ? std::string("x") : scene.nodes[0].name) + " display_name y");
        const auto m3 = hormiga::memory::process_bytes();
        const double text = (double)part.size();
        std::printf("expand   core x%.1f, scene x%.1f, one undo frame x%.1f the state text (%zu runes)\n",
                    ((double)m1 - (double)m0) / text, ((double)m2 - (double)m1) / text,
                    ((double)m3 - (double)m2) / text, scene.nodes.size());
    }

    // the whole document, streamed to a file: what writing a v3 bundle back costs
    const fs::path out = fs::temp_directory_path() / "hormiga-store-bench.state.json";
    {
        const auto before = hormiga::memory::process_bytes();
        const auto t2 = clk::now();
        std::ofstream o(out, std::ios::binary);
        st.write_state(o);
        o.close();
        std::printf("export   %7.1f s  process +%6.1f MB   %.1f MB written\n",
                    std::chrono::duration<double>(clk::now() - t2).count(),
                    ((double)hormiga::memory::process_bytes() - (double)before) / mb,
                    (double)fs::file_size(out, ec) / mb);
    }
    fs::remove(out, ec);

    // the same database as a v4 bundle: packed, then opened the way the app opens a file
    const fs::path v4 = fs::temp_directory_path() / "hormiga-store-bench.miga";
    const fs::path assets = fs::temp_directory_path() / "hormiga-store-bench-assets";
    const fs::path reopened = fs::temp_directory_path() / "hormiga-store-bench-reopened.db";
    {
        const auto t3 = clk::now();
        const auto packed = hormiga::store::pack_v4(st, fs::temp_directory_path(), assets, {}, v4, "bench");
        std::printf("pack v4  %7.1f s  %s, %.1f MB\n", std::chrono::duration<double>(clk::now() - t3).count(),
                    packed.ok ? "ok" : packed.error.c_str(), (double)fs::file_size(v4, ec) / mb);
        const auto t4 = clk::now();
        const auto opened = hormiga::store::open_bundle(v4, reopened, fs::temp_directory_path(), assets);
        std::printf("open v4  %7.1f s  %s, %lld runes   (a v3 file of the same database: the import above)\n",
                    std::chrono::duration<double>(clk::now() - t4).count(), opened.ok ? "ok" : opened.error.c_str(),
                    opened.runes);
    }
    for (const fs::path& p : {v4, reopened}) fs::remove(p, ec);
    return 0;
}
