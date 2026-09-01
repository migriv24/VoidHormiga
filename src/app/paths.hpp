/* paths.hpp — resolving an organization's folders, without needing the app.
 *
 * `HormigaApp::data_dir()` is the everyday door and answers from a cache. This
 * free function is the same arithmetic reachable by anything holding a Core —
 * which is what the headless effects have. `effect pack-database` needs to know
 * where THIS database's assets really are, and building a whole HormigaApp just
 * to ask (and reaching into its private core to load one) would be two wrongs
 * to avoid one duplicated ten-line resolve. There is one resolve, here, and
 * both callers go through it.
 */
#pragma once

#include "voidmaiz/embed.hpp"

#include <filesystem>
#include <string>

namespace hormiga {

/* `base / name`, unless `config paths.<name>` says otherwise: an absolute
 * override is used as given, a relative one is relative to `base` — never to
 * the process's working directory, or the setting would mean different folders
 * depending on where the binary was launched from. */
std::filesystem::path resolve_data_dir(maiz::Core& core,
                                       const std::filesystem::path& base,
                                       const std::string& name);

} // namespace hormiga
