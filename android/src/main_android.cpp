/* android/src/main_android.cpp — Void Hormiga on Android: the shell.
 *
 * NativeActivity glue + EGL/GLES3 + ImGui's android backend around HormigaApp,
 * in the phone front-end (src/phone/, okf/concepts/sections/mobile.md). Platform
 * glue only: the application is the same HormigaApp the desktop runs. The shape
 * is Interaction Combinators' android shell, which proved it on real phones.
 *
 * What differs from that template, and why:
 *   - THE PHONE FRONT-END. enable_phone() hands the app the platform keyboard
 *     (org.voidmaiz.MaizActivity) and the screen's density; the phone drives
 *     the keyboard itself each frame, so this shell must NOT also do it.
 *   - FONTS COME FROM THE APK. The desktop reads vendor/fonts beside its binary;
 *     an APK has no such folder, so build_apk.ps1 packs the faces as assets and
 *     they load from memory here, rasterized at the screen's density (the icons
 *     on the navigation bar and half the buttons are Font Awesome codepoints:
 *     without the merge they draw as blanks).
 *   - DATA lives in the app's private folder (internalDataPath).
 *   - NO HOST HOOKS for what a phone does not have here yet: no file dialogs,
 *     no shell, no opening URLs. The app treats an absent hook as "not on this
 *     device". */
#include "app/app.hpp"
#include "IconsFontAwesome6.h" // vendor/fonts: the icon codepoints

#include "imgui.h"
#include "imgui_impl_android.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"

#include "platform/device_paths.hpp" // where this device keeps its files
#include "voidmaiz/documents.hpp" // the system picker and save dialog
#include "voidmaiz/mobile.hpp"        // the safe area
#include "voidmaiz/textinputview.hpp" // maiz::android_text_input

#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <cstdlib> // setenv
#include <filesystem>
#include <memory>

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "voidhormiga", __VA_ARGS__)

namespace {

struct Shell {
    maiz::TouchScrollState scroll;
    float density = 1.0f; // px per dp, for the scroll's slop
    android_app* aapp = nullptr;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool backends_ready = false; // window + EGL + imgui backends live
    bool app_ready = false;      // HormigaApp::init ran (once per process)
    HormigaApp app;
    /* THE SAFE AREA: the status bar, the cutout, the gesture strip. The surface
     * runs under all of them; 0.1.7 put its navigation bar there, where a
     * gesture phone takes every touch as "go home". Re-read twice a second:
     * rotation and a change of navigation mode move it. */
    maiz::SafeArea safe;
    int safe_age = 1 << 20;
};

bool egl_init(Shell& s) {
    s.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (s.display == EGL_NO_DISPLAY || !eglInitialize(s.display, nullptr, nullptr)) return false;
    const EGLint attribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                              EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint num = 0;
    if (!eglChooseConfig(s.display, attribs, &config, 1, &num) || num < 1) return false;
    EGLint format;
    eglGetConfigAttrib(s.display, config, EGL_NATIVE_VISUAL_ID, &format);
    ANativeWindow_setBuffersGeometry(s.aapp->window, 0, 0, format);
    s.surface = eglCreateWindowSurface(s.display, config, s.aapp->window, nullptr);
    const EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    s.context = eglCreateContext(s.display, config, EGL_NO_CONTEXT, ctx_attribs);
    if (s.surface == EGL_NO_SURFACE || s.context == EGL_NO_CONTEXT) return false;
    return eglMakeCurrent(s.display, s.surface, s.surface, s.context) == EGL_TRUE;
}

void egl_term(Shell& s) {
    if (s.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(s.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (s.context != EGL_NO_CONTEXT) eglDestroyContext(s.display, s.context);
        if (s.surface != EGL_NO_SURFACE) eglDestroySurface(s.display, s.surface);
        eglTerminate(s.display);
    }
    s.display = EGL_NO_DISPLAY;
    s.surface = EGL_NO_SURFACE;
    s.context = EGL_NO_CONTEXT;
}

/* One asset from the APK, in memory ImGui may own (it frees with IM_FREE). */
void* asset_bytes(AAssetManager* am, const char* name, int* size) {
    AAsset* a = AAssetManager_open(am, name, AASSET_MODE_BUFFER);
    if (!a) return nullptr;
    const off_t n = AAsset_getLength(a);
    void* buf = IM_ALLOC((size_t)n);
    const int got = AAsset_read(a, buf, (size_t)n);
    AAsset_close(a);
    if (got != n) {
        IM_FREE(buf);
        return nullptr;
    }
    *size = (int)n;
    return buf;
}

/* The desktop's faces (desktop.cpp), from the APK and at the screen's density. */
void load_fonts(AAssetManager* am, float density, HormigaApp& app) {
    ImGuiIO& io = ImGui::GetIO();
    int n = 0;
    ImFontConfig base;
    base.OversampleH = 2;
    base.OversampleV = 2;
    base.PixelSnapH = true;
    if (void* lato = asset_bytes(am, "fonts/Lato-Regular.ttf", &n))
        io.Fonts->AddFontFromMemoryTTF(lato, n, 16.0f * density, &base);
    else {
        LOGE("fonts/Lato-Regular.ttf missing from the APK: the default face, and NO icons");
        ImFontConfig fallback;
        fallback.SizePixels = 13.0f * density; // at least at the screen's size, not 13 px
        io.Fonts->AddFontDefault(&fallback);
        return;
    }
    static const ImWchar fa_range[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
    ImFontConfig icons;
    icons.MergeMode = true;
    icons.PixelSnapH = true;
    icons.GlyphMinAdvanceX = 15.0f * density;
    if (void* fa = asset_bytes(am, "fonts/fa-solid-900.ttf", &n))
        io.Fonts->AddFontFromMemoryTTF(fa, n, 15.0f * density, &icons, fa_range);
    else
        LOGE("fonts/fa-solid-900.ttf missing from the APK: icons will draw blank");
    ImFontConfig mono;
    mono.OversampleH = 2;
    mono.OversampleV = 2;
    if (void* jb = asset_bytes(am, "fonts/JetBrainsMono.ttf", &n))
        app.mono_font = io.Fonts->AddFontFromMemoryTTF(jb, n, 15.0f * density, &mono);
}

/* A picture from the data folder, as a GLES3 texture (desktop.cpp's, for GLES). */
HormigaApp::HostTexture gl_load_texture(const std::string& abs_path) {
    HormigaApp::HostTexture t;
    int w = 0, h = 0, comp = 0;
    unsigned char* px = stbi_load(abs_path.c_str(), &w, &h, &comp, 4);
    if (!px) return t;
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    stbi_image_free(px);
    t.id = (unsigned long long)id;
    t.w = w;
    t.h = h;
    return t;
}

void backends_up(Shell& s) {
    if (s.backends_ready || !s.aapp->window) return;
    if (!egl_init(s)) {
        LOGE("EGL init failed");
        return;
    }
    ImGui_ImplAndroid_Init(s.aapp->window);
    ImGui_ImplOpenGL3_Init("#version 300 es");
    if (!s.app_ready) {
        // 160 dpi is 1x; IC measured that density alone still reads small on
        // glass, and the phone front-end's own sizes are design pixels
        const float dpi = (float)AConfiguration_getDensity(s.aapp->config);
        const float density = std::clamp(dpi > 0 ? dpi / 160.0f : 2.0f, 1.0f, 5.0f);
        load_fonts(s.aapp->activity->assetManager, density, s.app);
        s.density = density;
        HormigaApp& app = s.app;
        app.base_dir = s.aapp->activity->internalDataPath ? std::filesystem::path(s.aapp->activity->internalDataPath)
                                                          : std::filesystem::path("/data/local/tmp");
        app.ship_dir = app.base_dir; // nothing ships beside a .so: the guide and OKF are desktop-only
        // where this device keeps its files, said ONCE, before anything reads it:
        // the profile and joined databases inside the app's folder, never HOME
        // (a phone has none: 0.1.7 joined into "test/" under "/", 2026-09-25)
        hormiga::device::set(hormiga::device::phone(app.base_dir));
        // Android has no /tmp, and std::filesystem::temp_directory_path falls
        // back to it: the share bundle could not be written. TMPDIR is what it
        // reads first, so every temp file lands inside the app's own folder.
        {
            std::error_code ec;
            const auto tmp = app.base_dir / "tmp";
            std::filesystem::create_directories(tmp, ec);
            setenv("TMPDIR", tmp.c_str(), 1);
        }
        android_app* aapp = s.aapp;
        // the system's picker and save dialog (voidmaiz/documents.hpp): asked
        // here, answered in render_frame, a frame or a minute later
        const std::string incoming = (app.base_dir / "incoming").string();
        app.on_pick_document = [aapp, incoming](int request, const std::string& mime) {
            return maiz::android_pick_document(aapp->activity, request, mime, incoming);
        };
        app.on_save_document = [aapp](int request, const std::string& src, const std::string& name) {
            return maiz::android_save_document(aapp->activity, request, src, name, "application/octet-stream");
        };
        app.on_quit = [aapp] { ANativeActivity_finish(aapp->activity); };
        app.on_load_texture = gl_load_texture;
        app.enable_phone(true, maiz::android_text_input(s.aapp->activity), density);
        const bool first_run = !std::filesystem::exists(app.base_dir / app.state_name); // before init makes it
        app.init();
        // a phone reopens the database it had; a new install starts EMPTY, not the demo
        app.open_default_database(true, first_run);
        s.app_ready = true;
        // started by "Open with Void Hormiga" on a .miga: it arrives like a pick
        maiz::android_opened_document(s.aapp->activity, (app.base_dir / "incoming").string());
    }
    s.backends_ready = true;
}

void backends_down(Shell& s) {
    if (!s.backends_ready) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    egl_term(s);
    // the context is gone, and every picture's texture id with it: they reload
    // on return (the photo picker sends the app here every time)
    if (s.app_ready) s.app.gl_context_lost();
    s.backends_ready = false; // the app (and ImGui context) survive backgrounding
}

void on_cmd(android_app* a, int32_t cmd) {
    Shell& s = *(Shell*)a->userData;
    switch (cmd) {
    case APP_CMD_INIT_WINDOW: backends_up(s); break;
    case APP_CMD_TERM_WINDOW: backends_down(s); break;
    // leaving the foreground: save now, since Android may end the process
    // without another word (the desktop saves on close; a phone never closes).
    // shutdown() also releases the Human floor claim, which an agent would wait
    // on; no agent runs on a phone, so the floor has nobody to hold it from.
    case APP_CMD_RESUME: // "Open with" on a .miga while Hormiga was already running
        if (s.app_ready) maiz::android_opened_document(a->activity, (s.app.base_dir / "incoming").string());
        break;
    case APP_CMD_PAUSE:
    case APP_CMD_SAVE_STATE:
        if (s.app_ready) s.app.shutdown();
        break;
    default: break;
    }
}

int32_t on_input(android_app* a, AInputEvent* ev) {
    Shell& s = *(Shell*)a->userData;
    if (!s.backends_ready) return 0;
    return ImGui_ImplAndroid_HandleInputEvent(ev);
}

void render_frame(Shell& s) {
    // what the system's picker or save dialog answered, between frames (never
    // mid-frame: an arriving .miga replaces the whole database)
    for (maiz::DocumentResult r; maiz::android_take_document(s.aapp->activity, r);)
        s.app.document_arrived(r.request, r.status, r.path, r.name);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame();
    ImGui::NewFrame();
    if (++s.safe_age > 30) {
        s.safe = maiz::android_safe_area(s.aapp->activity);
        s.safe_age = 0;
    }
    maiz::reserve_safe_area(s.safe); // before the phone's own bars, which then sit inside it
    maiz::touch_scroll(s.scroll, s.density); // a finger has no wheel
    s.app.frame(); // the phone front-end: it runs the platform keyboard itself
    ImGui::Render();
    EGLint w = 0, h = 0;
    eglQuerySurface(s.display, s.surface, EGL_WIDTH, &w);
    eglQuerySurface(s.display, s.surface, EGL_HEIGHT, &h);
    glViewport(0, 0, w, h);
    const ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    glClearColor(bg.x, bg.y, bg.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    eglSwapBuffers(s.display, s.surface);
}

} // namespace

void android_main(android_app* a) {
    Shell s;
    s.aapp = a;
    a->userData = &s;
    a->onAppCmd = on_cmd;
    a->onInputEvent = on_input;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr; // the phone's layout is its own, not imgui's

    while (true) {
        int events;
        android_poll_source* source;
        // render-driven: poll without blocking while we can draw
        while (ALooper_pollOnce(s.backends_ready ? 0 : -1, nullptr, &events, (void**)&source) >= 0) {
            if (source) source->process(a, source);
            if (a->destroyRequested) {
                if (s.app_ready) s.app.shutdown();
                backends_down(s);
                ImGui::DestroyContext();
                return;
            }
            if (s.backends_ready) break; // drain later; keep frames coming
        }
        if (s.backends_ready) render_frame(s);
    }
}
