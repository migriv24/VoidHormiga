/* report/report.cpp — see report.hpp. Every value printed here is a compile-time
 * fact about this binary; if a line ever needs a runtime value, it needs a
 * reason in okf/concepts/contributing/intake.md first. */
#include "report/report.hpp"

#include <cstdio>
#include <iostream>

#ifndef HORMIGA_VERSION
#define HORMIGA_VERSION "0.0.0-unversioned"
#endif
#ifndef HORMIGA_PLATFORM
#define HORMIGA_PLATFORM "unconfigured-platform"
#endif
/* From void.json's `repository`, read by CMake, so the tracker moves with the
 * manifest rather than with whoever compiled this. */
#ifndef HORMIGA_REPOSITORY
#define HORMIGA_REPOSITORY "https://github.com/migriv24/VoidHormiga"
#endif

namespace hormiga::report {

namespace {
constexpr bool kHaveNet =
#ifdef HORMIGA_HAVE_NET
    true;
#else
    false;
#endif
}

std::string url_encode(std::string_view s) {
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            char buf[4];
            std::snprintf(buf, sizeof buf, "%%%02X", c);
            out += buf;
        }
    }
    return out;
}

std::string facts() {
    std::string s;
    s += "- Void Hormiga: " HORMIGA_VERSION "\n";
    s += "- Platform: " HORMIGA_PLATFORM "\n";
    s += std::string("- Networking built in: ") + (kHaveNet ? "yes" : "no") + "\n";
    return s;
}

std::string issue_url(Kind kind) {
    std::string u = HORMIGA_REPOSITORY "/issues/new?template=";
    u += kind == Kind::Bug ? "bug.yml" : "feature.yml";
    u += "&version=" + url_encode(HORMIGA_VERSION);
    u += "&platform=" + url_encode(HORMIGA_PLATFORM);
    return u;
}

int run_cli(int argc, char** argv) {
    Kind kind = Kind::Bug;
    for (int j = 1; j < argc; ++j) {
        const std::string_view a = argv[j];
        if (a == "--feature") kind = Kind::Feature;
        else if (a == "--bug") kind = Kind::Bug;
        else {
            std::cerr << "report: unknown option '" << a << "'\n"
                      << "usage: voidhormiga-cli report [--bug|--feature]\n";
            return 2;
        }
    }
    std::cout << text(kind);
    return 0;
}

std::string text(Kind kind) {
    return "This build:\n\n" + facts() +
           "\nOpen this link to file it (nothing is sent until you submit the form):\n\n  " +
           issue_url(kind) +
           "\n\nBefore you submit, keep these OUT of a public issue: an organization's\n"
           "database or .miga, its logs, names, emails, phone numbers, addresses,\n"
           "screenshots showing real people, and paths on your computer. Show the\n"
           "problem with the Cat Colony, the made-up database Hormiga opens on its\n"
           "first run, if you can.\n"
           "A security problem goes to the private advisory form instead: " HORMIGA_REPOSITORY
           "/security/advisories/new\n";
}

} // namespace hormiga::report
