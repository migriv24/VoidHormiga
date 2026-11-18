/* app/shell_ui.hpp — what the desktop holds about the application as data
 * (app/shell_app.cpp; domain/shell.hpp; okf/concepts/platform/shell.md). */
#pragma once

#include "domain/shell.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

struct ShellUi {
    std::vector<hormiga::shell::Tab> tabs;  // this database's, effective (every built-in present)
    std::set<std::string> applied;          // shell.ids whose default this DEVICE has applied (imgui.ini)
    std::string candidate_id, candidate_layout; // the open database's default, waiting for the first frame
    std::string pending_layout;             // to load before the next frame begins
    bool candidate_has_tabs = false;        // the open database names its tabs (their order applies once)
    bool reseed = false;                    // dock the tabs again, in `tabs` order, this frame
    std::map<std::string, bool> screen_open; // a screen tab's open flag, by its document mantle
    // SCREENS (ui/screens.cpp)
    unsigned dock_id = 0;                          // the main dock area, where a new screen tab lands
    std::map<std::string, std::vector<std::string>> selected; // a screen's selected runes, by its document mantle
    std::map<std::string, std::string> buffers;    // an input's or a form field's text, by component
};
