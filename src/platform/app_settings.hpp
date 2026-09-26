/* platform/app_settings.hpp — the APPLICATION's settings, on this device.
 *
 * The author, 2026-09-25: there are a database's preferences (its name, where
 * its files are: they belong to the database) and there are "a user's real
 * settings" for the application itself: updates, and "default database", which
 * as a default will be "empty database" but a user could assign a specific
 * database as the one that pops up when they open the application. This is the
 * second kind, kept in the device's own folder (platform/device_paths.hpp),
 * never in a database, and never shared with members.
 *
 * Also the recent databases File > Recent lists. */
#pragma once

#include <string>
#include <vector>

namespace hormiga::app_settings {

struct Settings {
    /* "" = start with an EMPTY database; otherwise a .miga opened at start.
     * A database named on the command line always wins over this. */
    std::string default_database;
    std::vector<std::string> recent; // .miga paths, most recent first
    bool migrated = false;           // the first start with this file was handled
};

inline constexpr std::size_t kRecentMax = 10;

Settings load();
bool save(const Settings& s);

/* Put `path` at the top of the recent list (and drop missing files from it). */
void note_recent(const std::string& path);

} // namespace hormiga::app_settings
