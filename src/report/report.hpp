/* report/report.hpp — `voidhormiga-cli report`: what a person pastes into an issue.
 *
 *     voidhormiga-cli report             a bug report: this build, and the link
 *     voidhormiga-cli report --feature   the same, for asking for something new
 *
 * The intake end of okf/concepts/contributing/index.md. Somebody who is not a
 * developer and whose Hormiga misbehaved needs two things: the facts a
 * maintainer will ask for first, and a door to put them through. This prints
 * both and does nothing else.
 *
 * WHAT IT IS ALLOWED TO KNOW is the whole design, and the list is short: what
 * this BINARY is (version, platform, whether networking was built in) and
 * where the project takes issues. It never reads a database, the working
 * folder, a path, the user's name or an environment variable, and it makes no
 * network request — so nothing it prints can carry an organization's data or
 * a machine's identity into a public tracker. The person adds the rest
 * themselves, under a template that tells them what must stay out
 * (okf/concepts/foundation/what-may-be-public.md).
 *
 * Like `update`, it is a verb of the process rather than of a session: an
 * installation that will not open its database is the most likely moment to
 * want it.
 */
#pragma once

#include <string>
#include <string_view>

namespace hormiga::report {

enum class Kind { Bug, Feature };

// "%XX" for everything outside the URL-safe set, so a version like
// `0.2.2-rc 1` cannot break the query string.
std::string url_encode(std::string_view s);

// The facts block, as Markdown, ready to paste.
std::string facts();

// The new-issue link for this kind, with the build's facts pre-filled into the
// issue form's fields (GitHub fills a form field whose id matches a query key).
std::string issue_url(Kind kind);

/* The whole report as one block a person can SEND: the facts, the link, and
 * what to leave out. The organizer's help is usually not us — it is their
 * group's technical person, or whoever runs a fork for them (Void Mago
 * okf/concepts/audiences.md) — so the app offers this to copy or share as well
 * as the form, and it must read on its own in a chat or an email. */
std::string text(Kind kind);

int run_cli(int argc, char** argv);

} // namespace hormiga::report
