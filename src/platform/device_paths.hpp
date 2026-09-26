/* platform/device_paths.hpp — where THIS device keeps its files, in one place.
 *
 * Found on the first phone (2026-09-25): every part of Hormiga that touched a
 * file guessed its folder from the desktop's environment. The profile read
 * APPDATA or HOME, and "where a joined database goes" read USERPROFILE or HOME.
 * Android sets neither, so the profile landed in "/.config/..." and a join wrote
 * "test/test.miga.part" relative to "/": "did not join. cannot write
 * test/test.miga.part". The author: "i think something is off with the way the
 * application handles files and the database in general."
 *
 * So the shell says where the device's files are, ONCE, before anything reads
 * them, and everything asks here. On a desktop the defaults are exactly the
 * folders Hormiga has always used, so nothing moves. On a phone the shell roots
 * all of it in the app's private folder.
 *
 * This is also what the Antfarm's device nodes read ("which device am I", "where
 * does this device keep things"): the same answer, not a second guess. */
#pragma once

#include <filesystem>
#include <string>

namespace hormiga::device {

enum class Kind { Desktop, Phone };

struct Paths {
    Kind kind = Kind::Desktop;
    std::string platform;             // HORMIGA_PLATFORM: "windows-x64", "android-arm64", ...
    std::filesystem::path data;       // this device's own folder: everything below lives in it on a phone
    std::filesystem::path profile;    // the profile: who this device's person is
    std::filesystem::path databases;  // where databases are kept, and where a joined one goes
};

/* The desktop's folders, from its environment, as Hormiga has always found
 * them: the profile in the platform's per-user config folder, databases in
 * Documents/HormigaFiles. HORMIGA_PROFILE_DIR still overrides the profile. */
Paths desktop_defaults();

/* A phone's folders, all inside `app_dir` (Android's internalDataPath). */
Paths phone(const std::filesystem::path& app_dir);

/* Set once by the shell, before HormigaApp::init. Unset, the desktop defaults. */
void set(const Paths& p);
const Paths& get();

const char* kind_name(Kind k);

} // namespace hormiga::device
