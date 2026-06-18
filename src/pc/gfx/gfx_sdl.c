#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include <SDL3/SDL.h>

#if defined(_WIN32)
#include <windows.h>
#endif

#if FOR_WINDOWS
#define GLEW_STATIC
#include <GL/glew.h>

#define GL_GLEXT_PROTOTYPES 1
#include <SDL3/SDL_opengl.h>
#else
#define GL_GLEXT_PROTOTYPES 1

#ifdef OSX_BUILD
#include <SDL3/SDL_opengl.h>
#else
#include <SDL3/SDL_opengles2.h>
#endif

#endif // End of OS-Specific GL defines

#include <stdio.h>
#include <unistd.h>

#include "gfx_window_manager_api.h"
#include "gfx_screen_config.h"
#include "../pc_main.h"
#include "../configfile.h"
#include "../cliopts.h"

#include "pc/controller/controller_keyboard.h"
#include "pc/controller/controller_sdl.h"
#include "pc/controller/controller_bind_mapping.h"
#include "pc/utils/misc.h"
#include "pc/mods/mod_import.h"
#include "pc/rom_checker.h"

#ifndef GL_MAX_SAMPLES
#define GL_MAX_SAMPLES 0x8D57
#endif

// TODO: figure out if this shit even works
#ifdef VERSION_EU
# define FRAMERATE 25
#else
# define FRAMERATE 30
#endif

static SDL_Window *wnd;
static SDL_GLContext ctx = NULL;

static kb_callback_t kb_key_down = NULL;
static kb_callback_t kb_key_up = NULL;
static void (*kb_all_keys_up)(void) = NULL;
static void (*kb_text_input)(char*) = NULL;
static void (*kb_text_editing)(char*, int) = NULL;

static void (*m_scroll)(float, float) = NULL;

#define IS_FULLSCREEN() ((SDL_GetWindowFlags(wnd) & (SDL_WINDOW_FULLSCREEN | 0x00001000)) != 0)

static inline void gfx_sdl_set_vsync(const bool enabled) {
    SDL_GL_SetSwapInterval(enabled);
}

static void gfx_sdl_set_fullscreen(void) {
    if (configWindow.reset) {
        configWindow.fullscreen = false;
    }
    if (configWindow.fullscreen == IS_FULLSCREEN()) {
        return;
    }
    if (configWindow.fullscreen) {
        SDL_SetWindowFullscreen(wnd, true);
    } else {
        SDL_SetWindowFullscreen(wnd, false);
        SDL_ShowCursor();
        configWindow.exiting_fullscreen = true;
    }
}

static void gfx_sdl_reset_dimension_and_pos(void) {
    if (configWindow.exiting_fullscreen) {
        configWindow.exiting_fullscreen = false;
        SDL_HideCursor();
    }

    if (configWindow.reset) {
        configWindow.x = WAPI_WIN_CENTERPOS;
        configWindow.y = WAPI_WIN_CENTERPOS;
        configWindow.w = DESIRED_SCREEN_WIDTH;
        configWindow.h = DESIRED_SCREEN_HEIGHT;
        configWindow.reset = false;
    } else if (!configWindow.settings_changed) {
        return;
    }

    int xpos = (configWindow.x == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.x;
    int ypos = (configWindow.y == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.y;

    SDL_SetWindowSize(wnd, configWindow.w, configWindow.h);
    SDL_SetWindowPosition(wnd, xpos, ypos);
    // in case vsync changed
    gfx_sdl_set_vsync(configWindow.vsync);
}

static void gfx_sdl_init(const char *window_title) {
#if defined(_WIN32)
    SetProcessDPIAware();
#endif

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_StartTextInput(wnd);

    if (configWindow.msaa > 0) {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, configWindow.msaa);
    } else {
        SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
    }

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

#ifdef USE_GLES
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);  // These attributes allow for hardware acceleration on RPis.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif

    int xpos = (configWindow.x == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.x;
    int ypos = (configWindow.y == WAPI_WIN_CENTERPOS) ? SDL_WINDOWPOS_CENTERED : configWindow.y;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, window_title);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, xpos);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, ypos);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, configWindow.w);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, configWindow.h);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    wnd = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    ctx = SDL_GL_CreateContext(wnd);

    gfx_sdl_set_vsync(configWindow.vsync);

    gfx_sdl_set_fullscreen();
    if (configWindow.fullscreen) {
        SDL_HideCursor();
    }

    controller_bind_init();
}

bool gfx_sdl_check_opengl_compatibility(void) {
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            return false;
        }
    }

    // hidden window
    SDL_Window *window = SDL_CreateWindow("", 1, 1, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);

    if (!window) {
        return false;
    }

    SDL_GLContext ctx = SDL_GL_CreateContext(window);

    if (!ctx) {
        SDL_DestroyWindow(window);
        return false;
    }

    SDL_GL_MakeCurrent(window, ctx);
    bool validVersion = gfx_opengl_check_compatibility();

    SDL_GL_DestroyContext(ctx);
    SDL_DestroyWindow(window);

    return validVersion;
}

static void gfx_sdl_main_loop(void (*run_one_game_iter)(void)) {
    run_one_game_iter();
}

static void gfx_sdl_get_dimensions(uint32_t *width, uint32_t *height) {
    int w, h;
    SDL_GetWindowSize(wnd, &w, &h);
    if (width) *width = w;
    if (height) *height = h;
}

static void gfx_sdl_onkeydown(int scancode) {
    const bool *state = SDL_GetKeyboardState(NULL);

    if ((state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT]) && state[SDL_SCANCODE_RETURN]) {
        configWindow.fullscreen = !configWindow.fullscreen;
        configWindow.settings_changed = true;
        return;
    }

    if (kb_key_down) {
        kb_key_down(translate_sdl_scancode(scancode));
    }
}

static void gfx_sdl_onkeyup(int scancode) {
    if (kb_key_up) {
        kb_key_up(translate_sdl_scancode(scancode));
    }
}

static void gfx_sdl_onscroll(float x, float y) {
    if (m_scroll) {
        m_scroll(x, y);
    }
}

static void gfx_sdl_ondropfile(char* path) {
#ifdef _WIN32
    char portable_path[SYS_MAX_PATH];
    if (sys_windows_short_path_from_mbs(portable_path, SYS_MAX_PATH, path)) {
        if (!gRomIsValid) {
            rom_on_drop_file(portable_path);
        } else if (gGameInited) {
            mod_import_file(portable_path);
        }
    }
#else
    if (!gRomIsValid) {
        rom_on_drop_file(path);
    } else if (gGameInited) {
        mod_import_file(path);
    }
#endif
}

static void gfx_sdl_handle_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_TEXT_INPUT:
                kb_text_input((char *)event.text.text);
                break;
            case SDL_EVENT_TEXT_EDITING: //IME composition
                kb_text_editing((char *)event.edit.text, event.edit.start);
                break;
            case SDL_EVENT_KEY_DOWN:
                gfx_sdl_onkeydown(event.key.scancode);
                break;
            case SDL_EVENT_KEY_UP:
                gfx_sdl_onkeyup(event.key.scancode);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                gfx_sdl_onscroll(event.wheel.x, event.wheel.y);
                break;
            case SDL_EVENT_WINDOW_MOVED:
                if (!IS_FULLSCREEN() && !configWindow.exiting_fullscreen) {
                    if (event.window.data1 >= 0) { configWindow.x = event.window.data1; }
                    if (event.window.data2 >= 0) { configWindow.y = event.window.data2; }
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                if (!IS_FULLSCREEN()) {
                    configWindow.w = event.window.data1;
                    configWindow.h = event.window.data2;
                }
                break;
            case SDL_EVENT_DROP_FILE:
                gfx_sdl_ondropfile((char *)event.drop.data);
                break;
            case SDL_EVENT_QUIT:
                game_exit();
                break;
        }
    }

    if (configWindow.settings_changed) {
        gfx_sdl_set_fullscreen();
        gfx_sdl_reset_dimension_and_pos();
        configWindow.settings_changed = false;
    }
}

static void gfx_sdl_set_keyboard_callbacks(kb_callback_t on_key_down, kb_callback_t on_key_up,
void (*on_all_keys_up)(void), void (*on_text_input)(char*), void (*on_text_editing)(char*, int))
{
    kb_key_down = on_key_down;
    kb_key_up = on_key_up;
    kb_all_keys_up = on_all_keys_up;
    kb_text_input = on_text_input;
    kb_text_editing = on_text_editing;
}

static void gfx_sdl_set_scroll_callback(void (*on_scroll)(float, float)) {
    m_scroll = on_scroll;
}

static bool gfx_sdl_start_frame(void) {
    return true;
}

static void gfx_sdl_swap_buffers_begin(void) {
    SDL_GL_SwapWindow(wnd);
}

static void gfx_sdl_swap_buffers_end(void) {
}

static double gfx_sdl_get_time(void) {
    return 0.0;
}

static void gfx_sdl_delay(u32 ms) {
    SDL_Delay(ms);
}

static int gfx_sdl_get_max_msaa(void) {
    int maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (maxSamples > 16) { maxSamples = 16; }
    return maxSamples;
}

static void gfx_sdl_set_window_title(const char* title) {
    SDL_SetWindowTitle(wnd, title);
}

static void gfx_sdl_reset_window_title(void) {
    SDL_SetWindowTitle(wnd, TITLE);
}

SDL_Window *gfx_sdl_get_window(void) {
    return wnd;
}

static void gfx_sdl_shutdown(void) {
    if (SDL_WasInit(0)) {
        if (ctx) { SDL_GL_DestroyContext(ctx); ctx = NULL; }
        if (wnd) { SDL_DestroyWindow(wnd); wnd = NULL; }
        SDL_Quit();
    }
}

static bool gfx_sdl_has_focus(void) {
    return (SDL_GetWindowFlags(wnd) & SDL_WINDOW_INPUT_FOCUS);
}

static void gfx_sdl_start_text_input(void) { SDL_StartTextInput(wnd); }
static void gfx_sdl_stop_text_input(void) { SDL_StopTextInput(wnd); }

static char* gfx_sdl_get_clipboard_text(void) {
    static char clipboard_buf[WAPI_CLIPBOARD_BUFSIZ];

    char* text = SDL_GetClipboardText();
    strncpy(clipboard_buf, text, WAPI_CLIPBOARD_BUFSIZ - 1);
    SDL_free(text);

    clipboard_buf[WAPI_CLIPBOARD_BUFSIZ - 1] = '\0';
    return clipboard_buf;
}

static void gfx_sdl_set_clipboard_text(const char* text) { SDL_SetClipboardText(text); }
static void gfx_sdl_set_cursor_visible(bool visible) { visible ? SDL_ShowCursor() : SDL_HideCursor(); }

struct GfxWindowManagerAPI gfx_sdl = {
    gfx_sdl_init,
    gfx_sdl_set_keyboard_callbacks,
    gfx_sdl_set_scroll_callback,
    gfx_sdl_main_loop,
    gfx_sdl_get_dimensions,
    gfx_sdl_handle_events,
    gfx_sdl_start_frame,
    gfx_sdl_swap_buffers_begin,
    gfx_sdl_swap_buffers_end,
    gfx_sdl_get_time,
    gfx_sdl_shutdown,
    gfx_sdl_start_text_input,
    gfx_sdl_stop_text_input,
    gfx_sdl_get_clipboard_text,
    gfx_sdl_set_clipboard_text,
    gfx_sdl_set_cursor_visible,
    gfx_sdl_delay,
    gfx_sdl_get_max_msaa,
    gfx_sdl_set_window_title,
    gfx_sdl_reset_window_title,
    gfx_sdl_has_focus
};
