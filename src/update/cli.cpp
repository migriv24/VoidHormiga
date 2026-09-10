/* update/cli.cpp — the terminal's half of the update client.
 *
 *     voidhormiga-cli update                 what is installed, and the setting
 *     voidhormiga-cli update --check         ask the feed (a network request)
 *     voidhormiga-cli update --install       check, download, verify, launch
 *     voidhormiga-cli update --startup       let the app ask the feed on boot
 *     voidhormiga-cli update --never         never offer updates again
 *     voidhormiga-cli update --ask           forget the answer; ask again
 *     voidhormiga-cli update --feed <url>    point somewhere else
 *     voidhormiga-cli update --skip <ver>    "not this one" (empty clears it)
 *
 * A BARE `update` MAKES NO NETWORK REQUEST. It prints what this installation
 * is, what it has been told to do, and where that answer is stored. `--check`
 * is a request the person made by typing it, which is the only kind this
 * application makes — the GUI's version of the same rule is the permission
 * modal in `src/ui/updates.cpp`.
 *
 * It prints the same sentences the window shows, because both call
 * `update::describe()` on the same offer. Two surfaces, one vocabulary — the
 * founding commitment applied to a place it would be easy to exempt.
 */
#include "update/cli.hpp"

#include <filesystem>
#include <iostream>

namespace hormiga::update {

int run_cli(int argc, char** argv, const Shell& shell) {
    bool want_check = false, want_install = false, set_skip = false;
    std::string set_ask, set_feed, skip;
    for (int j = 1; j < argc; ++j) {
        const std::string_view a = argv[j];
        if (a == "--check") want_check = true;
        else if (a == "--install") want_install = true;
        else if (a == "--startup" || a == "--yes") set_ask = "startup";
        else if (a == "--never") set_ask = "never";
        else if (a == "--ask") set_ask = "unasked";
        else if (a == "--feed" && j + 1 < argc) set_feed = argv[++j];
        else if (a == "--skip" && j + 1 < argc) { skip = argv[++j]; set_skip = true; }
        else {
            std::cerr << "update: unknown option " << a << "\n";
            return 2;
        }
    }

    Prefs prefs = load_prefs();
    bool dirty = false;
    if (!set_ask.empty()) { prefs.ask = ask_from_name(set_ask); dirty = true; }
    if (!set_feed.empty()) { prefs.feed_url = set_feed; dirty = true; }
    if (set_skip) { prefs.skip_version = skip; dirty = true; }

    std::cout << "Void Hormiga " << current_version() << "  (" << platform_tag()
              << ")\n  preference   " << ask_name(prefs.ask);
    if (prefs.ask == Ask::Unasked) std::cout << "   (nothing asked, nothing fetched)";
    std::cout << "\n  feed         "
              << (prefs.feed_url.empty() ? std::string(default_feed_url())
                                         : prefs.feed_url)
              << "\n  settings     " << prefs_path().string() << "\n";
    if (!prefs.skip_version.empty())
        std::cout << "  skipping     " << prefs.skip_version << "\n";
    if (!prefs.last_checked.empty())
        std::cout << "  last checked " << prefs.last_checked << "\n";

    int rc = 0;
    if (want_check || want_install) {
        const auto res = check(shell, prefs, std::filesystem::temp_directory_path());
        dirty = true;   // `last_checked` moved, whatever else happened

        if (!res.ok) {
            /* THE REMOTE'S OWN SENTENCE, unflattened. The 2026-08-20 field
             * report established that a vendor's words are routinely the entire
             * diagnosis, and "something went wrong" is what a person cannot
             * act on. */
            std::cerr << "\nupdate check failed: " << res.error << "\n";
            rc = 1;
        } else if (res.offer.skipped) {
            std::cout << "\n" << res.feed.latest
                      << " is available and you asked not to be told about it.\n"
                         "  update --skip \"\" --check   to hear about it again\n";
        } else if (!res.offer.available) {
            std::cout << "\nup to date (the feed's latest is " << res.feed.latest
                      << ").\n";
        } else {
            std::cout << "\n" << describe(res.offer);
            if (!want_install)
                std::cout << "\nupdate --install   to download, verify and run it\n";
        }

        if (rc == 0 && want_install && res.offer.available) {
            if (!res.offer.release.has_artifact) {
                std::cerr << "no " << platform_tag() << " installer is listed for "
                          << res.offer.release.version << ".\n";
                rc = 1;
            } else {
                const auto dl = download(
                    shell, res.offer.release,
                    std::filesystem::temp_directory_path() / "voidhormiga-update");
                if (!dl.ok) {
                    std::cerr << "download failed: " << dl.error << "\n";
                    rc = 1;
                } else {
                    // a non-empty `error` on a successful download is the
                    // bookkeeping note (`bytes` disagreed, the digest matched)
                    if (!dl.error.empty()) std::cerr << dl.error << "\n";
                    std::cout
                        << "verified " << dl.file.string() << "\n"
                        << "launching the installer. The copy you are running is NOT\n"
                           "touched: each version installs into its own folder, so this\n"
                           "one keeps working whatever the new one does.\n";
                    /* LAUNCHED, NOT REPLACED. Side-by-side is what makes "try
                     * the update" a reversible decision, and the one thing that
                     * must never happen here is deleting the version somebody
                     * is currently running. */
                    if (!launch_installer(dl.file)) {
                        std::cerr << "could not launch it. Run it yourself:\n  "
                                  << dl.file.string() << "\n";
                        rc = 1;
                    }
                }
            }
        }
    } else if (prefs.ask == Ask::Unasked) {
        std::cout << "\nNothing has been fetched. `update --check` asks the feed now;\n"
                     "`update --startup` lets the application ask when it opens;\n"
                     "`update --never` stops it being offered at all.\n";
    }

    if (dirty && !save_prefs(prefs))
        std::cerr << "warning: could not write " << prefs_path().string() << "\n";
    return rc;
}

} // namespace hormiga::update
