/* domain/workspaces.hpp — what a new database can start as
 * (okf/concepts/platform/workspaces.md).
 *
 * The author, 2026-10-06: Hormiga "has been optimized for normal
 * organizations. However we should also build for things like: grocery
 * stores, smart homes (just keeping a layout of the home), volunteer
 * organizations, DND campaigns, etc."
 *
 * A WORKSPACE IS A STARTING POINT, not a mode: a seed transcript of dispatcher
 * commands run once when a database is made, plus `config workspace.kind` so
 * the application knows what it started as. Every workspace is the same data
 * model; anything one sets up can be changed afterwards in any database. The
 * demo data is synthetic and public, as the Cat Colony is.
 *
 * ImGui-free: the desktop, the phone and the CLI offer the same list. */
#pragma once

#include "domain/demos.hpp" // the Whiskerwood, Whisker Mart, the House of Cats

#include <cstdio>
#include <string>
#include <vector>

namespace hormiga::workspaces {

struct Workspace {
    const char* key;   // config workspace.kind
    const char* title; // what a person picks
    const char* line;  // one line on what it sets up
    bool built;        // false: listed so the breadth is seen, not yet pickable
};

inline const Workspace kAll[] = {
    {"organization", "Organization", "An outreach organization: people, places and events on a map of the area.",
     true},
    {"grocery", "Grocery store", "Whisker Mart: a cat grocery's floor, products, vendors, customers and sales.", true},
    {"home", "Smart home", "The House of Cats: a floor plan, its rooms, smart devices, supplies that expire, chores.",
     true},
    {"volunteer", "Volunteer organization", "Volunteers, shifts and stations, on the area and on the venue's plan.",
     false},
    {"campaign", "D&D campaign", "The Whiskerwood: a party of cat adventurers, creatures, quests and sessions, on a map in feet.",
     true},
};
inline constexpr int kCount = (int)(sizeof kAll / sizeof kAll[0]);

inline const Workspace* find(const std::string& key) {
    for (const auto& w : kAll)
        if (key == w.key) return &w;
    return nullptr;
}

/* The commands that set a workspace up in a database new_database() just made:
 * its kinds (run in the `kinds` mantle, then registered), then its data. The
 * organization's is what New database always made. The demos are the proof of
 * the kinds release, made only of what any database can make (domain/demos.hpp). */
inline demos::Transcript transcript(const std::string& key) {
    if (key == "grocery") return demos::mart();
    if (key == "home") return demos::home();
    if (key == "campaign") return demos::campaign();
    if (key == "organization") return {{}, {"config set workspace.kind \"organization\""}};
    return {};
}

} // namespace hormiga::workspaces
