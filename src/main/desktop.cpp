/* main_desktop.cpp — the desktop shell: GLFW window + OpenGL 3 + ImGui
 * backends around HormigaApp. Platform glue only; the application lives in
 * app.cpp (the app/platform split, DESIGN.md §9). */
#include "app/app.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
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
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
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
#endif
    return {};
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
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
        auto vf = std::filesystem::current_path() / "vendor/fonts";
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
    ImGui_ImplOpenGL3_Init("#version 130");
    lap("imgui init");

    HormigaApp app;
    app.mono_font = g_mono_font; // the script editor's monospace face
    app.base_dir = std::filesystem::current_path();
    {   // where the BINARY lives: shipped files (webfonts) resolve from here,
        // never from the data folder. Falls back to the working directory.
        std::error_code sec;
        if (argc > 0)
            app.ship_dir = std::filesystem::absolute(argv[0], sec).parent_path();
    }
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
    app.on_pick_file = os_pick_file;
    app.on_save_file = os_save_file;
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
