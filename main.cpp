/* SPDX-License-Identifier: MIT
 *
 * SPDX-FileCopyrightText: Copyright (c) Copyright (c) 2014-2024 Omar Cornut
 * SPDX-FileCopyrightText: Copyright (c) 2024 Ian Hangartner <icrashstuff at outlook dot com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "util/console.h"
#include "util/convar.h"
#include "util/imgui-1.91.1/backends/imgui_impl_opengl3.h"
#include "util/imgui-1.91.1/backends/imgui_impl_sdl2.h"
#include "util/imgui.h"
#include "util/physfs/physfs.h"

#include "util/cli_parser.h"
#include "util/gui_registrar.h"
#include "util/misc.h"
#include "util/overlay_loading.h"
#include "util/overlay_performance.h"
#include "util/styles.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL2/SDL_opengl.h>
#endif

#define log_gl_attribute(attribute)                \
    do                                             \
    {                                              \
        int __buffer;                              \
        SDL_GL_GetAttribute(attribute, &__buffer); \
        dc_log(#attribute " %d", __buffer);        \
    } while (0)

SDL_Window* window = NULL;

static convar_int_t dev("dev", 0, 0, 1, "Enables developer focused features", CONVAR_FLAG_HIDDEN | CONVAR_FLAG_INT_IS_BOOL);

static convar_int_t cl_resizable("cl_resizable", 1, 0, 1, "Enable/Disable window resizing", CONVAR_FLAG_HIDDEN | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cl_win_w("cl_win_w", 1280, 0, SDL_MAX_SINT32 - 1, "Force window width", CONVAR_FLAG_HIDDEN);
static convar_int_t cl_win_h("cl_win_h", 720, 0, SDL_MAX_SINT32 - 1, "Force window height", CONVAR_FLAG_HIDDEN);
static convar_int_t cl_grab_mouse(
    "cl_grab_mouse", 0, 0, 1, "Enable/Disable mouse grabbing (dev_console::shown overrides this)", CONVAR_FLAG_HIDDEN | CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cl_fullscreen("cl_fullscreen", 0, 0, 1, "Enable/Disable fullscreen window", CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cl_fullscreen_mode("cl_fullscreen_mode", 0, 0, 1, "Fullscreen mode [0: Fullscreen Windowed, 1: Fullscreen]");

static convar_int_t cl_fps_limiter("cl_fps_limiter", 300, 0, SDL_MAX_SINT32 - 1, "Max FPS, 0 to disable");
static convar_int_t cl_vsync("cl_vsync", 1, 0, 1, "Enable/Disable vsync", CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t cl_adapative_vsync("cl_adapative_vsync", 1, 0, 1, "Enable disable adaptive vsync", CONVAR_FLAG_INT_IS_BOOL);

static convar_int_t cl_wait_for_events(
    "cl_wait_for_events", 0, 0, 3, "Wait for events instead of polling for them [0: Auto (Off), 1: Auto(On), 2: Force (Off), 3: Force(On)]");

static convar_int_t cl_show_menu("cl_show_main_menu", 1, 0, 1, "Enable/Disable main menu", CONVAR_FLAG_INT_IS_BOOL);
static convar_int_t dev_show_demo_window_complex("dev_show_demo_window_complex", 0, 0, 1, "Show Dear ImGui demo window", CONVAR_FLAG_INT_IS_BOOL);

static convar_string_t rom_release("rom_release", "", "Force specific Release ROM", CONVAR_FLAG_HIDDEN);
static convar_string_t rom_first_hunt("rom_first_hunt", "", "Force specific First Hunt ROM", CONVAR_FLAG_HIDDEN);

void process_event(SDL_Event& event, bool* done, int* win_width, int* win_height)
{
    if (!cl_grab_mouse.get() || dev_console::shown)
        ImGui_ImplSDL2_ProcessEvent(&event);

    if (event.type == SDL_QUIT)
        *done = true;
    if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(window))
        switch (event.window.event)
        {
        case SDL_WINDOWEVENT_CLOSE:
            *done = true;
            break;
        case SDL_WINDOWEVENT_RESIZED:
            SDL_GetWindowSize(window, win_width, win_height);
            break;
        default:
            break;
        }
    if (event.type == SDL_KEYDOWN)
        switch (event.key.keysym.scancode)
        {
        case SDL_SCANCODE_F11:
            cl_fullscreen.set(!cl_fullscreen.get());
            break;
        case SDL_SCANCODE_END:
            *done = 1;
            break;
        case SDL_SCANCODE_GRAVE:
            if (event.key.repeat == 0)
                dev_console::show_hide();
            break;
        default:
            break;
        }

    if (!cl_grab_mouse.get() || dev_console::shown)
        return;

    /* Game bind logic here */
}

// Main code
int main(const int argc, const char** argv)
{
    convar_t::atexit_init();
    atexit(convar_t::atexit_callback);

    PHYSFS_init(argv[0]);
    PHYSFS_setSaneConfig("icrashstuff", "mph_tetra", NULL, 0, 0);

    /* Parse command line */
    cli_parser::parse(argc, argv);

    /* Set dev before any other variables in case their callbacks require dev */
    if (cli_parser::get_value(dev.get_name()))
        dev.set(1);
    dev.set_pre_callback([=](int, int) -> bool { return false; });

    if (dev.get())
    {
        /* KDevelop fully buffers the output and will not display anything */
        setvbuf(stdout, NULL, _IONBF, 0);
        setvbuf(stderr, NULL, _IONBF, 0);
        fflush_unlocked(stdout);
        fflush_unlocked(stderr);
        dc_log("Developer convar set");
    }

    /* Set convars from command line */
    cli_parser::apply();

    const PHYSFS_ArchiveInfo** supported_archives = PHYSFS_supportedArchiveTypes();

    for (int i = 0; supported_archives[i] != NULL; i++)
        dc_log("Supported archive: [%s]", supported_archives[i]->extension);

    overlay::loading::push();

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    const char* glsl_version = "#version 150";
#if defined(__APPLE__)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    Uint32 window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;

    window = SDL_CreateWindow("MPH Tetra", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, cl_win_w.get(), cl_win_h.get(), window_flags);
    if (window == nullptr)
        util::die("Error: SDL_CreateWindow(): %s\n", SDL_GetError());

    cl_resizable.set_post_callback([=]() { SDL_SetWindowResizable(window, (SDL_bool)cl_resizable.get()); }, true);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    SDL_GL_MakeCurrent(window, gl_context);
    cl_vsync.set_post_callback(
        [=]() {
            bool vsync_enable = cl_vsync.get();
            bool adapative_vsync_enable = cl_adapative_vsync.get();
            if (vsync_enable && adapative_vsync_enable && SDL_GL_SetSwapInterval(-1) == 0)
                return;
            SDL_GL_SetSwapInterval(vsync_enable);
        },
        true);
    cl_fullscreen.set_pre_callback(
        [=](int _old, int _new) -> bool {
            Uint32 mode = cl_fullscreen_mode.get() ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
            return !SDL_SetWindowFullscreen(window, _new ? mode : 0);
        },
        true);
    cl_fullscreen_mode.set_pre_callback(
        [=](int _old, int _new) -> bool {
            Uint32 mode = _new ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_FULLSCREEN_DESKTOP;
            return !SDL_SetWindowFullscreen(window, cl_fullscreen.get() ? mode : 0);
        },
        true);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls

    style_colors_rotate_hue(0, 160, 1, 1);

    // Setup Platform/Renderer backends
    if (!ImGui_ImplSDL2_InitForOpenGL(window, gl_context))
        util::die("Failed to initialize Dear Imgui SDL2 backend\n");
    if (!ImGui_ImplOpenGL3_Init(glsl_version))
        util::die("Failed to initialize Dear Imgui OpenGL3 backend\n");

    log_gl_attribute(SDL_GL_RED_SIZE);
    log_gl_attribute(SDL_GL_GREEN_SIZE);
    log_gl_attribute(SDL_GL_BLUE_SIZE);
    log_gl_attribute(SDL_GL_ALPHA_SIZE);
    log_gl_attribute(SDL_GL_DEPTH_SIZE);
    log_gl_attribute(SDL_GL_STENCIL_SIZE);
    log_gl_attribute(SDL_GL_CONTEXT_PROFILE_MASK);
    log_gl_attribute(SDL_GL_CONTEXT_MAJOR_VERSION);
    log_gl_attribute(SDL_GL_CONTEXT_MINOR_VERSION);

    SDL_GL_MakeCurrent(window, gl_context);

    ImVec4 clear_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    char** search_paths = PHYSFS_getSearchPath();

    for (size_t i = 0; search_paths[i] != NULL; i++)
        dc_log("Search path [%zu]: [%s]", i, search_paths[i]);

    PHYSFS_freeList(search_paths);

    dev_console::shown = !true;
    bool done = false;
    int win_height, win_width;
    SDL_GetWindowSize(window, &win_width, &win_height);
    if (!done)
        dc_log("Beginning main loop\n");
    Uint64 last_loop_time;
    bool first_loop = true;
    while (!done)
    {
        Uint64 loop_start_time = SDL_GetPerformanceCounter();
        overlay::performance::calculate(((float)(last_loop_time * 10000 / SDL_GetPerformanceFrequency())) / 10.0f);
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard
        // data. Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        if (!first_loop && (cl_wait_for_events.get() == 1 || cl_wait_for_events.get() == 3))
        {
            SDL_WaitEventTimeout(&event, 250);
            do
                process_event(event, &done, &win_width, &win_height);
            while (SDL_PollEvent(&event));
        }
        else
        {
            first_loop = false;
            while (SDL_PollEvent(&event))
                process_event(event, &done, &win_width, &win_height);
        }

        // The requirement that dev_console not be shown is to ensure that the mouse won't get trapped
        SDL_SetWindowMouseGrab(window, (SDL_bool)(cl_grab_mouse.get() && !dev_console::shown));
        SDL_SetRelativeMouseMode((SDL_bool)(cl_grab_mouse.get() && !dev_console::shown));

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        bool open = dev_show_demo_window_complex.get();
        if (open)
        {
            ImGui::ShowDemoWindow(&open);
            if (open != dev_show_demo_window_complex.get())
                dev_show_demo_window_complex.set(open);
        }

        if (cl_show_menu.get())
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGuiWindowFlags win_flags
                = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
            if (ImGui::BeginCVR("Main Menu", &cl_show_menu, win_flags))
            {
                if (ImGui::Button("Hello"))
                {
                }
                ImGui::End();
            }
        }

        gui_registrar::render_menus();
        gui_registrar::render_overlays();

        dev_console::render();

        // Actual Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        last_loop_time = SDL_GetPerformanceCounter() - loop_start_time;
        SDL_GL_SwapWindow(window);

        Uint64 now = SDL_GetTicks64();
        static Uint64 reference_time = 0;
        static Uint64 frames_since_reference = 0;
        if (cl_fps_limiter.get())
        {
            Uint64 elasped_time_ideal = frames_since_reference * 1000 / cl_fps_limiter.get();
            Sint64 delay = reference_time + elasped_time_ideal - now;
            // Reset when difference between the real and ideal worlds gets problematic
            if (delay < -100 || 100 < delay)
            {
                reference_time = now;
                frames_since_reference = 0;
            }
            else if (delay > 0)
                SDL_Delay(delay);
        }
        frames_since_reference += 1;
    }

    convar_t::atexit_callback();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    assert(PHYSFS_deinit());

    return 0;
}
