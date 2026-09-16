/* main/desktop.cpp — the desktop shell: GLFW window + OpenGL 3 + ImGui
 * backends around HormigaApp. Platform glue only; the application lives in
 * app.cpp (the app/platform split, DESIGN.md §9). */
#include "app/app.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
/* The GL context this view needs, per platform. GUARDED, because the header is
 * newer than the Void Maiz commit currently on GitHub — our CI clones the
 * sibling repos from there, and a hard include makes every macOS and Linux
 * build fail on a file that exists only in the author's working copy. The
 * fallback below does the same thing and deletes itself the day upstream
 * ships; see the note at the call site for why it is not simply duplicated. */
#if defined(__has_include)
#if __has_include("voidmaiz/glhost.hpp")
#include "voidmaiz/glhost.hpp"
#define HORMIGA_HAS_MAIZ_GLHOST 1
#endif
#endif
#include <GLFW/glfw3.h>

#include <cstdio>
#include <chrono>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_GIF
#define STBI_ONLY_BMP
#include "stb_image.h" // vendored, public domain (vendor/stb/LICENSE.md)

#include "IconsFontAwesome6.h" // vendored (vendor/fonts) — icon codepoints

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX // the toolchain may already define it
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h> // SHBrowseForFolder: the priority folder
#endif

/* Hand a file/URL to the OS default handler (the system browser is a VIEWER,
 * never part of the runtime — DESIGN.md §12). */
static void os_open(const std::string& path) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    std::string cmd = "xdg-open \"" + path + "\"";
    std::system(cmd.c_str());
#endif
}

/* ── FILE DIALOGS OFF WINDOWS (2026-09-15) ──────────────────────────────────
 *
 * The author, on a Linux machine: *"its not able to open up dialogue folder
 * things ... for things like 'save data base as' or adding a photo."* It was
 * not the machine. Both functions below had a Windows branch and nothing else,
 * so on Linux and macOS every Browse, Open database and Save database as...
 * returned "" -- which callers read as "cancelled", so nothing was said either.
 *
 * Linux has no one dialog API a program can link without a toolkit, so this asks
 * the helpers desktops already ship: zenity (GNOME and most others), kdialog
 * (KDE), yad or qarma. macOS has `osascript`. The dialog is a separate process
 * and this call waits for it, exactly as the Windows dialog blocks the frame. When
 * none is installed, `g_dialog_problem` says which one to install, and the caller
 * in main() hands it to the app to show, rather than failing in silence again. */
static std::string g_dialog_problem;

#ifndef _WIN32
static std::string shell_quoted(const std::string& s) {
    std::string q = "'";
    for (char c : s) {
        if (c == '\'') q += "'\\''";
        else q += c;
    }
    return q + "'";
}

static bool have_tool(const char* tool) {
    const std::string c = std::string("command -v ") + tool + " >/dev/null 2>&1";
    return std::system(c.c_str()) == 0;
}

static std::string run_dialog(const std::string& cmd) {
    std::string out;
    FILE* p = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    pclose(p);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

static const char* kNoDialog =
    "No file dialog is available on this computer. Install zenity (for example "
    "`sudo apt install zenity`) or kdialog, then try again.";
#endif

/* The OS open-file dialog for the "path" editor kind ("" = cancelled).
 * OFN_NOCHANGEDIR matters: the app's cwd is where the org file lives. */
static std::string os_pick_file(std::string_view /*current*/) {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Databases & documents\0*.miga;*.png;*.jpg;*.jpeg;*.gif;*.webp;*.pdf\0"
                      "Database (.miga)\0*.miga\0CSV\0*.csv\0All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return buf;
#elif defined(__APPLE__)
    return run_dialog("osascript -e 'POSIX path of (choose file)'");
#else
    if (have_tool("zenity")) return run_dialog("zenity --file-selection --title='Choose a file'");
    if (have_tool("kdialog")) return run_dialog("kdialog --getopenfilename \"$HOME\"");
    if (have_tool("yad")) return run_dialog("yad --file --title='Choose a file'");
    if (have_tool("qarma")) return run_dialog("qarma --file-selection --title='Choose a file'");
    g_dialog_problem = kNoDialog;
#endif
    return {};
}

// a FOLDER: where an organization's files live (Niche Tools, 2026-09-16)
static std::string os_pick_folder() {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    const HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    BROWSEINFOA bi = {};
    bi.lpszTitle = "Choose the folder where this database's files live";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    std::string picked;
    if (LPITEMIDLIST pidl = SHBrowseForFolderA(&bi)) {
        if (SHGetPathFromIDListA(pidl, buf)) picked = buf;
        CoTaskMemFree(pidl);
    }
    if (SUCCEEDED(co)) CoUninitialize();
    return picked;
#elif defined(__APPLE__)
    return run_dialog("osascript -e 'POSIX path of (choose folder)'");
#else
    if (have_tool("zenity"))
        return run_dialog("zenity --file-selection --directory --title='Choose a folder'");
    if (have_tool("kdialog")) return run_dialog("kdialog --getexistingdirectory \"$HOME\"");
    if (have_tool("yad")) return run_dialog("yad --file --directory --title='Choose a folder'");
    if (have_tool("qarma"))
        return run_dialog("qarma --file-selection --directory --title='Choose a folder'");
    g_dialog_problem = kNoDialog;
    return {};
#endif
}

// the SAVE dialog: pick where to write a .miga (author 2026-07-24). Suggests a
// default filename; appends .miga if the user didn't type an extension.
static std::string os_save_file(std::string_view suggested) {
#ifdef _WIN32
    char buf[MAX_PATH] = {};
    std::snprintf(buf, sizeof buf, "%.*s", (int)suggested.size(), suggested.data());
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof ofn;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Void Hormiga database (.miga)\0*.miga\0All files\0*.*\0";
    ofn.lpstrDefExt = "miga";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    if (GetSaveFileNameA(&ofn)) return buf;
#else
    const std::string name(suggested);
    std::string picked;
# if defined(__APPLE__)
    picked = run_dialog("osascript -e " +
                        shell_quoted("POSIX path of (choose file name default name \"" +
                                     name + "\")"));
# else
    if (have_tool("zenity"))
        picked = run_dialog("zenity --file-selection --save --confirm-overwrite --filename=" +
                            shell_quoted(name));
    else if (have_tool("kdialog"))
        picked = run_dialog("kdialog --getsavefilename " + shell_quoted(name));
    else if (have_tool("yad"))
        picked = run_dialog("yad --file --save --confirm-overwrite --filename=" +
                            shell_quoted(name));
    else if (have_tool("qarma"))
        picked = run_dialog("qarma --file-selection --save --confirm-overwrite --filename=" +
                            shell_quoted(name));
    else
        g_dialog_problem = kNoDialog;
# endif
    // the Windows dialog appends .miga when no extension was typed; so does this
    if (!picked.empty() && std::filesystem::path(picked).extension().empty())
        picked += ".miga";
    return picked;
#endif
    return {};
}

#ifndef GL_CLAMP_TO_EDGE // GL 1.2 constant; Windows' gl.h stops at 1.1
#define GL_CLAMP_TO_EDGE 0x812F
#endif

/* Decode with stb_image, upload as a GL texture (GL 1.1 calls only — no
 * loader needed). The app caches the result; failures return id 0. */
static HormigaApp::HostTexture gl_load_texture(const std::string& abs_path) {
    HormigaApp::HostTexture t{};
    int w = 0, h = 0, comp = 0;
    unsigned char* pixels = stbi_load(abs_path.c_str(), &w, &h, &comp, 4);
    if (!pixels) return t;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels);
    stbi_image_free(pixels);
    t.id = tex;
    t.w = w;
    t.h = h;
    return t;
}

/* Run a command, capture stdout (the ImgBB holiday's transport — curl ships
 * with Windows 10+; nothing vendored, nothing linked). */
static std::string shell_capture(const std::string& cmd) {
    std::string out;
#ifdef _WIN32
    FILE* p = _popen(cmd.c_str(), "r");
#else
    FILE* p = popen(cmd.c_str(), "r");
#endif
    if (!p) return out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
#ifdef _WIN32
    _pclose(p);
#else
    pclose(p);
#endif
    return out;
}

static ImFont* g_mono_font = nullptr; // vendored JetBrains Mono (script IDE)

int main(int argc, char** argv) {
    /* WHERE THE BINARY LIVES, RESOLVED FIRST because the UI typefaces are
     * loaded from it and that happens before `HormigaApp` exists. This used to
     * sit forty lines below, under `app.ship_dir`, which is why the font block
     * could only reach for `current_path()` -- see the `vendor/fonts` comment
     * there. Empty means "argv[0] told us nothing"; every use falls back to the
     * working directory. */
    std::filesystem::path ship_dir;
    {
        std::error_code sec;
        if (argc > 0)
            ship_dir = std::filesystem::absolute(argv[0], sec).parent_path();
        if (sec) ship_dir.clear();
    }

    auto T0 = std::chrono::steady_clock::now();
    auto lap = [&](const char* what) {
        if (std::getenv("HORMIGA_BOOT_TIMING"))
            std::fprintf(stderr, "[boot] %6.0f ms  %s\n",
                         std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - T0).count(),
                         what);
    };
    glfwSetErrorCallback([](int code, const char* desc) {
        std::fprintf(stderr, "glfw error %d: %s\n", code, desc);
    });
    if (!glfwInit()) return 1;
    lap("glfwInit");
    /* ── THE GL CONTEXT IS THE LIBRARY'S ANSWER, NOT OURS (2026-09-08) ───────
     *
     * These two lines used to be `CONTEXT_VERSION_MAJOR 3` / `MINOR 0` here and
     * a matching `#version 130` two hundred lines below, copied from Void
     * Maiz's `canvas_window.cpp` the way every host copies it. Their CI found
     * what that pair does on macOS, and told us to check ours:
     *
     *   macOS ships no OpenGL 3.0. It offers legacy 2.1, or 3.2+ core profile
     *   with forward compatibility, and nothing between. So the request DOES
     *   NOT FAIL -- `glfwCreateWindow` succeeds and hands back 2.1 -- and then
     *   the `#version 130` shader will not compile against it. **The window
     *   opens and stays blank**, which reads as a rendering bug in our own draw
     *   code, in a file we did not write the interesting part of.
     *
     * `gl_context_hints()` sets the hints and RETURNS the matching GLSL version
     * string, so the two halves cannot drift apart again -- there is no longer
     * a second place to write either of them. Windows behaviour is
     * byte-identical (still 3.0 / `#version 130`); what this buys is a platform
     * we have not shipped, which is exactly when a fix like this is cheap. */
#ifdef HORMIGA_HAS_MAIZ_GLHOST
    const char* glsl_version = maiz::gl_context_hints();
#else
    /* THE SAME ANSWER, WHILE UPSTREAM'S IS UNPUBLISHED. This is a copy, and a
     * copy is the thing `gl_context_hints()` exists to abolish — so it is here
     * under `__has_include` rather than as a choice: the moment Void Maiz's
     * commit reaches GitHub this branch stops compiling into anything and the
     * library's answer wins again. Keeping the hints and the version string
     * adjacent, in one block, is what stops THIS copy from drifting the way the
     * original pair did across two hundred lines. */
    const char* glsl_version;
#if defined(__APPLE__)
    /* macOS has no OpenGL 3.0: legacy 2.1, or 3.2+ core with forward
     * compatibility, and nothing between. The three hints are a set — a core
     * profile without FORWARD_COMPAT is rejected outright. */
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glsl_version = "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glsl_version = "#version 130";
#endif
#endif
    GLFWwindow* window = glfwCreateWindow(1360, 800, "Hormiga", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    lap("window + GL context");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    { // the UI typeface (vendored OFL Lato) — a real face instead of the
      // pixelated ImGui default. It also sharpens the MAP PNG exports, which
      // rasterize labels/watermark straight from this same atlas. Font Awesome
      // (icon glyphs) is merged on top for the map-marker icons.
        ImGuiIO& fio = ImGui::GetIO();
        /* ---- THE FACES SHIP BESIDE THE BINARY, NOT BESIDE THE CALLER -----
         *
         * This was `current_path() / "vendor/fonts"`, and an installed copy of
         * Hormiga has no `vendor/` under the folder it was launched from --
         * nobody launches an installed application from a checkout. Every
         * `if (exists)` below would have fallen through to `AddFontDefault()`,
         * so the first device this was ever tested on would have opened a
         * window in a pixelated bitmap face with NO ICONS AT ALL: the section
         * tabs, the map markers and half the buttons are Font Awesome
         * codepoints, and a merge that never happened draws them as blanks.
         *
         * Exactly the failure `render_site` had with the webfonts in August,
         * in the other front-end, found the same way -- by asking what the
         * path means on a machine that is not this one. CMake stages
         * `vendor/fonts` beside both binaries; this is the half that reads it.
         * The working directory stays as a last resort so running the built
         * binary from the repo root still works. */
        std::filesystem::path vf = ship_dir.empty()
                                       ? std::filesystem::path()
                                       : ship_dir / "vendor" / "fonts";
        if (vf.empty() || !std::filesystem::exists(vf))
            vf = std::filesystem::current_path() / "vendor" / "fonts";
        auto lato = vf / "Lato-Regular.ttf";
        ImFontConfig base;
        base.OversampleH = 2; // crisper at small sizes than the default 1
        base.OversampleV = 2;
        base.PixelSnapH = true;
        if (std::filesystem::exists(lato))
            fio.Fonts->AddFontFromFileTTF(lato.string().c_str(), 16.0f, &base);
        else
            fio.Fonts->AddFontDefault(); // graceful fallback if the vendor drop is absent
        static const ImWchar fa_range[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
        ImFontConfig cfg;
        cfg.MergeMode = true;
        cfg.PixelSnapH = true;
        cfg.GlyphMinAdvanceX = 15.0f;
        auto fa = vf / "fa-solid-900.ttf";
        if (std::filesystem::exists(fa))
            fio.Fonts->AddFontFromFileTTF(fa.string().c_str(), 15.0f, &cfg,
                                          fa_range);
        // a MONOSPACE face for the Allomone Script IDE (vendored OFL JetBrains
        // Mono) — code wants a fixed-width font; also lets us grid-align
        // syntax-highlighting draws.
        auto mono = vf / "JetBrainsMono.ttf";
        ImFontConfig mc;
        mc.OversampleH = 2;
        mc.OversampleV = 2;
        if (std::filesystem::exists(mono))
            g_mono_font = fio.Fonts->AddFontFromFileTTF(mono.string().c_str(),
                                                        15.0f, &mc);
    }
    // FL-Studio-style movable panels (Void Maiz enabled ImGui docking, but its
    // enable_docking()/begin_dockspace() are dead — an always-false
    // `#ifdef ImGuiConfigFlags_DockingEnable` (an enum, not a macro); reported
    // upstream). Set the flag directly against the correct `IMGUI_HAS_DOCK`.
#ifdef IMGUI_HAS_DOCK
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version); // the half `gl_context_hints` returned
    lap("imgui init");

    HormigaApp app;
    app.mono_font = g_mono_font; // the script editor's monospace face
    app.base_dir = std::filesystem::current_path();
    // where the BINARY lives: shipped files (the fonts, the guide, the OKF
    // bundle) resolve from here, never from the data folder. Resolved at the
    // top of main because the typefaces above need it too.
    app.ship_dir = ship_dir;
    // A PERSON IS LOOKING AT THIS ONE, so it may offer an update. See
    // `offer_updates` in app.hpp for why this is a flag the shell sets
    // rather than something init() infers.
    app.offer_updates = true;
    /* ── `--state <path>`, ON THIS BINARY TOO (2026-08-21) ───────────────────
     *
     * The CLI has honoured `--state` since it existed; the desktop shell did
     * not, so which database the GUI opened depended entirely on the folder it
     * was launched from. Launched from the source tree it opened — and then
     * SAVED — a `demo-org.json` there, while the organization's real database
     * sat somewhere else. Two days were spent editing two copies of one
     * database because of it.
     *
     * A bare path works too, so "Open with" and a dropped file behave the way
     * anyone would expect. Both forms set `base_dir` to the document's OWN
     * folder, which is the whole point: `site/`, `assets/`, `exports/`,
     * `fonts/` and a relative `token_file` are all resolved against it, so
     * pinning the document pins everything that hangs off it.
     *
     * A path that does not exist is NOT refused — a first run has to start
     * somewhere — but it is said out loud, because a typo'd path is otherwise
     * indistinguishable from a new organization. */
    {
        std::string given;
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            if (a == "--state" && i + 1 < argc) { given = argv[++i]; continue; }
            if (a.rfind("--state=", 0) == 0) { given = a.substr(8); continue; }
            if (given.empty() && !a.empty() && a[0] != '-') given = a;
        }
        if (!given.empty()) {
            std::error_code sec;
            std::filesystem::path p = std::filesystem::absolute(given, sec);
            if (!sec) {
                if (std::filesystem::is_directory(p, sec)) {
                    // a FOLDER is a legitimate thing to point at: use its
                    // default document rather than treating it as a filename
                    app.base_dir = p;
                } else {
                    app.base_dir = p.parent_path();
                    app.state_name = p.filename().string();
                }
                const std::filesystem::path doc = app.base_dir / app.state_name;
                if (!std::filesystem::exists(doc, sec)) {
                    std::fprintf(stderr, "note: %s does not exist; starting an "
                                         "EMPTY document.\n", doc.string().c_str());
                    std::fprintf(stderr, "      If you meant an existing "
                                         "database, check the path.\n");
                }
            }
        }
    }
    app.on_title = [&](const std::string& t) { glfwSetWindowTitle(window, t.c_str()); };
    app.on_quit = [&] { glfwSetWindowShouldClose(window, 1); };
    app.on_open = os_open;
    // a dialog that could not open says why, instead of looking like "cancelled"
    app.on_pick_file = [&app](std::string_view cur) {
        std::string r = os_pick_file(cur);
        if (r.empty() && !g_dialog_problem.empty()) {
            app.host_notice(g_dialog_problem);
            g_dialog_problem.clear();
        }
        return r;
    };
    app.on_pick_folder = [&app]() {
        std::string r = os_pick_folder();
        if (r.empty() && !g_dialog_problem.empty()) {
            app.host_notice(g_dialog_problem);
            g_dialog_problem.clear();
        }
        return r;
    };
    app.on_save_file = [&app](std::string_view suggested) {
        std::string r = os_save_file(suggested);
        if (r.empty() && !g_dialog_problem.empty()) {
            app.host_notice(g_dialog_problem);
            g_dialog_problem.clear();
        }
        return r;
    };
    app.on_load_texture = gl_load_texture;
    app.on_shell_capture = shell_capture;
    app.init();
    lap("app.init");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) { glfwWaitEvents(); continue; }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.frame();

        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        if (app.light_mode) glClearColor(0.93f, 0.93f, 0.94f, 1.0f);
        else glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        static bool first = true;
        if (first) { lap("first frame drawn"); first = false; }
    }

    app.shutdown(); // save on close — no silent data loss

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
