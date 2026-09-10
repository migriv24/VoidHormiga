/* ui/updates.cpp — the update prompt, and nothing else.
 *
 * THIS FILE CONTAINS NO DECISIONS. Whether an update exists, whether the
 * person has been asked, whether the bytes that arrived are the bytes that were
 * promised: all of it is `src/update/update.cpp`, which is view-free and
 * reachable in full from `voidhormiga-cli update`. Here there is a thread, a
 * modal and a settings block.
 *
 * The rule this draws is `void.json`'s, and it has two halves:
 *
 *     no silent updates: the user is told an update exists and chooses it.
 *
 *   - **Never check without being asked to.** The first thing a fresh install
 *     sees is a question about the *check*, before any request is made. "Not
 *     now" is a real, persisted answer, not a snooze.
 *   - **Never install without being told to.** The offer modal downloads
 *     nothing until a button is pressed, and installs nothing at all — it hands
 *     a verified file to the OS. Side-by-side means the running copy survives,
 *     which is what makes trying an update a reversible decision.
 *
 * Prompt text lives in `update::describe()` so the window and the terminal say
 * the same words about the same release.
 */
#include "app/app_internal.hpp"
#include "update/update.hpp"

#include <thread>

namespace up = hormiga::update;

/* ── BOOT ───────────────────────────────────────────────────────────────────
 *
 * Three outcomes and no fourth: ask, check, or do nothing. The order matters —
 * the preferences file is read before anything else happens, so a machine that
 * answered "never" makes no request during a boot in which it was never even
 * considered. */
void HormigaApp::updates_reload_prefs() {
    const up::Prefs p = up::load_prefs();
    update_pref_ask =
        p.ask == up::Ask::Startup ? 2 : (p.ask == up::Ask::Never ? 1 : 0);
    update_pref_skip = p.skip_version;
    update_pref_last = p.last_checked;
    update_pref_feed = p.feed_url.empty() ? up::default_feed_url() : p.feed_url;
}

void HormigaApp::updates_boot() {
    const up::Prefs p = up::load_prefs();
    updates_reload_prefs();
    update_latest.clear();
    switch (p.ask) {
        case up::Ask::Unasked:
            /* THE QUESTION COMES BEFORE THE REQUEST. Asking permission to check
             * by checking would answer it in the wrong direction, and the
             * answer would already be on somebody's network. */
            update_modal = UpdateModal::AskPermission;
            break;
        case up::Ask::Startup:
            updates_check(/*by_hand=*/false);
            break;
        case up::Ask::Never:
            break;   // and that is the whole of it
    }
}

/* ── THE CHECK ──────────────────────────────────────────────────────────────
 *
 * Off the frame thread, because `curl` on the UI thread is a frozen window on a
 * bad connection and the first thing a person should learn about the update
 * system is not that it hangs the application. The worker writes into the job
 * and sets `finished`; `updates_drain()` reads it on the main thread. There is
 * no mutex and none is needed: `finished` is the release, and nothing on the
 * main thread reads a field before it. */
void HormigaApp::updates_check(bool by_hand) {
    if (update_job) return;   // one at a time; a second click is a no-op
    update_error.clear();
    update_installing = false;

    auto shell = on_shell_capture;
    if (!shell) {
        update_error = "no shell transport, so no way to reach the feed";
        if (by_hand) update_modal = UpdateModal::Failed;
        return;
    }

    update_job = std::make_unique<UpdateJob>();
    UpdateJob* j = update_job.get();
    update_by_hand = by_hand;   // read on the main thread, in `updates_drain`
    j->worker = std::thread([j, shell]() {
        up::Prefs prefs = up::load_prefs();
        const auto res = up::check(shell, prefs,
                                   std::filesystem::temp_directory_path());
        /* The check happened, so `last_checked` moved and is written back
         * whatever the outcome. A person looking at `updates.json` should be
         * able to see when this application last went to the network — that
         * inspectability is the point of the file living somewhere they can
         * open it. */
        up::save_prefs(prefs);

        j->ok = res.ok;
        j->error = res.error;
        j->latest = res.feed.latest;
        if (res.ok) {
            j->available = res.offer.available;
            j->skipped = res.offer.skipped;
            j->version = res.offer.release.version;
            j->summary = up::describe(res.offer);
            j->art_url = res.offer.release.url;
            j->art_file = res.offer.release.file;
            j->art_sha256 = res.offer.release.sha256;
            j->art_bytes = res.offer.release.bytes;
        }
        j->finished.store(true);
    });
}

/* ── THE DOWNLOAD ───────────────────────────────────────────────────────────
 *
 * Also off-thread — an installer is twenty-odd megabytes. `up::download`
 * verifies the digest and DELETES a file that does not match, so anything that
 * lands in `update_file` is a file whose bytes are the bytes the feed promised.
 * It is still not run: that is a separate button. */
void HormigaApp::updates_install() {
    if (update_job || update_version.empty()) return;
    update_error.clear();
    auto shell = on_shell_capture;
    if (!shell) {
        update_error = "no shell transport, so no way to fetch the installer";
        return;
    }
    /* Rebuild the artifact half of a `Release` from what the app carried
     * across. The app holds strings so `update/update.hpp` stays out of
     * `app.hpp`, which every translation unit includes; this is the one place
     * that has to undo that, and it is five assignments. */
    up::Release rel;
    rel.version = update_version;
    rel.url = update_art_url;
    rel.file = update_art_file;
    rel.sha256 = update_art_sha256;
    rel.bytes = update_art_bytes;
    rel.has_artifact = !rel.url.empty() && !rel.file.empty();
    update_installing = true;
    update_job = std::make_unique<UpdateJob>();
    UpdateJob* j = update_job.get();
    j->worker = std::thread([j, shell, rel]() {
        const auto dl = up::download(
            shell, rel, std::filesystem::temp_directory_path() / "voidhormiga-update");
        j->ok = dl.ok;
        j->error = dl.error;
        if (dl.ok) j->downloaded = dl.file.string();
        j->finished.store(true);
    });
}

/* ── THE MAIN-THREAD SIDE ───────────────────────────────────────────────────
 *
 * Called once a frame. Joins a finished worker (in `~UpdateJob`) and copies its
 * answer into the fields the modal draws. Nothing here reaches the network and
 * nothing here decides anything. */
void HormigaApp::updates_drain() {
    if (!update_job || !update_job->finished.load()) return;
    const bool was_install = update_installing;
    UpdateJob j2;
    std::swap(j2.ok, update_job->ok);
    std::swap(j2.available, update_job->available);
    std::swap(j2.skipped, update_job->skipped);
    std::swap(j2.error, update_job->error);
    std::swap(j2.summary, update_job->summary);
    std::swap(j2.version, update_job->version);
    std::swap(j2.latest, update_job->latest);
    std::swap(j2.art_url, update_job->art_url);
    std::swap(j2.art_file, update_job->art_file);
    std::swap(j2.art_sha256, update_job->art_sha256);
    std::swap(j2.art_bytes, update_job->art_bytes);
    std::swap(j2.downloaded, update_job->downloaded);
    update_job.reset();          // joins the worker
    update_installing = false;
    updates_reload_prefs();      // the worker moved `last_checked`

    if (was_install) {
        if (!j2.ok) {
            update_error = j2.error;
            update_modal = UpdateModal::Failed;
            return;
        }
        // a non-empty `error` on a successful download is the bookkeeping note
        // (`bytes` disagreed, the checksum matched) -- worth showing, not fatal
        if (!j2.error.empty()) log.push_back({"warn", "update", j2.error});
        update_file = j2.downloaded;
        toast("installer verified - ready to run");
        return;
    }

    update_latest = j2.latest;
    if (!j2.ok) {
        update_error = j2.error;
        /* A FAILED CHECK IS ONLY SHOWN WHEN SOMEBODY ASKED FOR ONE. A startup
         * check that could not reach GitHub is not news; putting a modal in
         * front of an organization because their café Wi-Fi is captive would
         * train them to dismiss the one that matters. It goes in the log, and
         * Settings says so. */
        if (update_by_hand) update_modal = UpdateModal::Failed;
        else log.push_back({"warn", "update", j2.error});
        return;
    }
    update_have_offer = j2.available;
    update_version = j2.available ? j2.version : std::string();
    update_summary = j2.summary;
    update_art_url = j2.art_url;
    update_art_file = j2.art_file;
    update_art_sha256 = j2.art_sha256;
    update_art_bytes = j2.art_bytes;
    if (j2.available) update_modal = UpdateModal::Offer;
    else if (update_by_hand)
        toast(j2.skipped ? "there is a newer version you asked not to hear about"
                         : "Hormiga is up to date");
}

/* ── THE MODALS ─────────────────────────────────────────────────────────────*/
void HormigaApp::draw_update_modal() {
    if (update_modal == UpdateModal::None) return;

    if (update_modal == UpdateModal::AskPermission) {
        ImGui::OpenPopup("Check for updates?");
        ImGui::SetNextWindowSize(ImVec2(430, 0), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Check for updates?", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped(
                "Hormiga can ask %s whether a newer version exists, each time "
                "it opens.",
                "github.com");
            ImGui::Spacing();
            ImGui::TextWrapped(
                "It has not asked yet, and it will not unless you say so. "
                "Nothing is ever installed without you choosing it, and "
                "installing a new version leaves this one on the machine and "
                "working.");
            ImGui::Spacing();
            ImGui::TextDisabled("You can change this any time in Settings.");
            ImGui::Spacing();
            if (ImGui::Button("Yes, check on startup", ImVec2(190, 0))) {
                up::Prefs p = up::load_prefs();
                p.ask = up::Ask::Startup;
                up::save_prefs(p);
                updates_reload_prefs();
                update_modal = UpdateModal::None;
                updates_check(/*by_hand=*/false);
            }
            ImGui::SameLine();
            if (ImGui::Button("No, don't ask again", ImVec2(170, 0))) {
                /* AND IT IS REAL. `Never` is persisted beside the install, not
                 * in the org's config, so it is not undone by opening a
                 * different database or by a merge from another device. */
                up::Prefs p = up::load_prefs();
                p.ask = up::Ask::Never;
                up::save_prefs(p);
                updates_reload_prefs();
                update_modal = UpdateModal::None;
            }
            ImGui::EndPopup();
        }
        return;
    }

    if (update_modal == UpdateModal::Failed) {
        ImGui::OpenPopup("Update check failed");
        ImGui::SetNextWindowSize(ImVec2(430, 0), ImGuiCond_Always);
        if (ImGui::BeginPopupModal("Update check failed", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            /* THE VENDOR'S OWN SENTENCE, unflattened. The 2026-08-20 field
             * report established that the remote's words are routinely the
             * entire diagnosis, and "something went wrong" is what a person
             * cannot act on. */
            ImGui::TextWrapped("%s", update_error.c_str());
            ImGui::Spacing();
            if (ImGui::Button("Close", ImVec2(120, 0)))
                update_modal = UpdateModal::None;
            ImGui::EndPopup();
        }
        return;
    }

    // ── the offer ───────────────────────────────────────────────────────────
    ImGui::OpenPopup("A new version of Hormiga");
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal("A new version of Hormiga", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize))
        return;

    /* `describe()` is the whole prompt, and it is the same text the CLI prints.
     * Its shape comes from Mago's `release` block: a headline, what was added,
     * and -- the part a version number cannot express and a resolver cannot see
     * -- what behaves differently and who that affects. Without those, an
     * update prompt can only say "a new version is available", which is the
     * difference between a notification people read and one they dismiss. */
    ImGui::PushTextWrapPos(500);
    ImGui::TextUnformatted(update_summary.c_str());
    ImGui::PopTextWrapPos();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (update_job) {
        ImGui::TextUnformatted("downloading...");
        ImGui::ProgressBar(-0.4f * (float)ImGui::GetTime(), ImVec2(-1, 0), "");
        ImGui::TextDisabled("the checksum is verified before anything is run.");
        ImGui::EndPopup();
        return;
    }

    if (!update_error.empty()) {
        ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.3f, 1), "%s", update_error.c_str());
        ImGui::Spacing();
    }

    if (!update_file.empty()) {
        ImGui::TextWrapped("Downloaded and verified. Running the installer will "
                           "NOT touch this copy: each version installs into its "
                           "own folder, so if the new one is wrong you still "
                           "have this one.");
        ImGui::Spacing();
        if (ImGui::Button("Run the installer", ImVec2(160, 0))) {
            if (!up::launch_installer(update_file))
                update_error = "could not launch it. It is at " + update_file;
            else
                update_modal = UpdateModal::None;
        }
        ImGui::SameLine();
        if (ImGui::Button("Later", ImVec2(90, 0))) update_modal = UpdateModal::None;
        ImGui::SameLine();
        ImGui::TextDisabled("%s", update_file.c_str());
        ImGui::EndPopup();
        return;
    }

    if (ImGui::Button("Download it", ImVec2(140, 0))) updates_install();
    ImGui::SameLine();
    if (ImGui::Button("Not now", ImVec2(100, 0))) update_modal = UpdateModal::None;
    ImGui::SameLine();
    if (ImGui::Button("Skip this version", ImVec2(150, 0))) {
        /* "NOT THIS ONE", NOT "NEVER AGAIN". The skip expires the moment
         * something newer than it appears, so it cannot become an off-switch
         * nobody remembers flipping. */
        up::Prefs p = up::load_prefs();
        p.skip_version = update_version;
        up::save_prefs(p);
        updates_reload_prefs();
        update_modal = UpdateModal::None;
        toast("skipping " + update_version + " - you'll hear about the next one");
    }
    ImGui::EndPopup();
}

/* ── SETTINGS ───────────────────────────────────────────────────────────────
 *
 * Drawn inside the Settings window, and deliberately NOT beside the `ui.*`
 * knobs above it: everything else there is `config set`, which rides the saved
 * org and travels to other devices on a merge. This is a fact about THIS
 * installation, so it says where it is stored. */
void HormigaApp::draw_update_settings() {
    ImGui::SeparatorText("Updates");

    ImGui::Text("This is Hormiga %s (%s)", up::current_version(), up::platform_tag());
    if (!update_latest.empty() && !update_have_offer)
        ImGui::TextDisabled("the feed's latest is %s", update_latest.c_str());

    /* Drawn from the CACHE (`updates_reload_prefs`), never from the file: this
     * window is open by default, and reading and parsing a file on disk once
     * per frame is not a thing to do to somebody's laptop. Writes go straight
     * to the file and refresh the cache, so the file stays authoritative. */
    int mode = update_pref_ask == 2 ? 0 : (update_pref_ask == 1 ? 1 : 2);
    const int was = mode;
    if (ImGui::RadioButton("Check when Hormiga opens", mode == 0)) mode = 0;
    if (ImGui::RadioButton("Never check", mode == 1)) mode = 1;
    if (mode == 2) {
        ImGui::SameLine();
        ImGui::TextDisabled("(not answered yet)");
    }
    if (mode != was) {
        up::Prefs p = up::load_prefs();
        p.ask = mode == 0 ? up::Ask::Startup : up::Ask::Never;
        up::save_prefs(p);
        updates_reload_prefs();
    }

    if (ImGui::Button("Check now")) updates_check(/*by_hand=*/true);
    if (update_job) {
        ImGui::SameLine();
        ImGui::TextDisabled("checking...");
    } else if (update_have_offer && !update_version.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton(("Hormiga " + update_version + " is available").c_str()))
            update_modal = UpdateModal::Offer;
    }

    if (!update_pref_skip.empty()) {
        ImGui::TextDisabled("skipping %s", update_pref_skip.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("stop skipping")) {
            up::Prefs p = up::load_prefs();
            p.skip_version.clear();
            up::save_prefs(p);
            updates_reload_prefs();
        }
    }
    if (!update_pref_last.empty())
        ImGui::TextDisabled("last checked %s", update_pref_last.c_str());
    if (!update_error.empty())
        ImGui::TextDisabled("last error: %s", update_error.c_str());

    /* WHERE THE ANSWER LIVES, said out loud. It is outside the org folder and
     * outside the version folder, so it survives both switching databases and
     * installing an update -- and somebody who wants to change it without the
     * application running can. */
    if (ImGui::TreeNode("Where this is stored")) {
        ImGui::TextWrapped("%s", up::prefs_path().string().c_str());
        ImGui::TextDisabled("Not part of your database: it is a fact about this "
                            "installation, so it does not travel to another "
                            "device on a sync.");
        ImGui::TextWrapped("feed: %s", update_pref_feed.c_str());
        ImGui::TreePop();
    }
}
