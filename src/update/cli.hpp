/* update/cli.hpp — `voidhormiga-cli update`, as a function of an argv.
 *
 * WHY IT IS HERE AND NOT IN `main/headless.cpp`. Everything in `src/update/`
 * has to be buildable and runnable without a session, a window, or Void Core,
 * because the machine that needs an updater is often the machine where those
 * things are what is wrong. A verb whose only home was the front-end would
 * inherit the front-end's dependencies, and the folder's whole claim
 * (`tools/check_layering.py`) would become a claim about a file nobody can
 * reach.
 *
 * The front-end's side is two lines: see the `update` block in
 * `src/main/headless.cpp`, and the argument there for why this runs BEFORE any
 * session exists.
 */
#pragma once

#include "update/update.hpp"

namespace hormiga::update {

/* `argv[0]` is the word `update` itself; `argc` counts from there. Returns a
 * process exit code: 0 fine, 1 something failed with a sentence on stderr,
 * 2 the caller typed something we do not have.
 *
 * `shell` is the transport — passed in rather than taken, which is what lets a
 * test drive every branch of this with a lambda that asserts it was never
 * called. */
int run_cli(int argc, char** argv, const Shell& shell);

} // namespace hormiga::update
