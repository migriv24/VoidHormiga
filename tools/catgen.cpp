/* catgen.cpp — export the Cat Colony as a standalone snapshot.
 *
 * The colony is now Hormiga's SHIPPED DEFAULT: it's defined once as
 * `hormiga::seed_cat_transcript()` in seed.hpp and replayed on the app's fresh
 * boot (okf/concepts/projects/cat-dataset.md). This tool replays that SAME transcript
 * headless and writes the resulting `export_state()` to a JSON snapshot — handy
 * for producing a portable copy, diffing the dataset, or packing a `.miga`.
 * One source of truth: the transcript. No divergence.
 *
 * Photos are the committed demo-assets/cat-NN.jpg (cataas.com cats). Output:
 * argv[1] (default databases/catworld/demo-org.json).
 */
#include "domain/seed.hpp"

#include "voidmaiz/embed.hpp"
#include "voidmaiz/project.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    fs::path out = argc > 1 ? fs::path(argv[1])
                            : fs::path("databases") / "catworld" / "demo-org.json";
    fs::create_directories(out.parent_path());

    maiz::Core core;
    hormiga::register_glyphs(core);
    core.dispatch("config set actor human:catgen");
    for (const auto& cmd : hormiga::seed_cat_transcript()) core.dispatch(cmd);
    for (const auto& cmd : hormiga::seed_cat_issue_transcript()) core.dispatch(cmd);
    // The legacy example library is no longer seeded (2026-08-11) — its tab is
    // unshipped and the eight `allo-script` examples replace it.
    // for (const auto& cmd : hormiga::seed_cat_scripts_transcript()) core.dispatch(cmd);
    for (const auto& cmd : hormiga::seed_allomone_scripts_transcript()) core.dispatch(cmd);
    core.dispatch("use demo-org");

    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    f << core.export_state();
    f.close();

    maiz::Scene sc = maiz::project_scene(core);
    std::printf("catgen: wrote %s\n  %zu runes in the data mantle\n",
                out.string().c_str(), sc.nodes.size());
    return 0;
}
