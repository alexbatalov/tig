#include "tig/core.h"

#include "tig/art.h"
#include "tig/button.h"
#include "tig/color.h"
#include "tig/debug.h"
#include "tig/draw.h"
#include "tig/dxinput.h"
#include "tig/file.h"
#include "tig/file_cache.h"
#include "tig/font.h"
#include "tig/kb.h"
#include "tig/memory.h"
#include "tig/menu.h"
#include "tig/message.h"
#include "tig/mouse.h"
#include "tig/movie.h"
#include "tig/net.h"
#include "tig/palette.h"
#include "tig/rect.h"
#include "tig/sound.h"
#include "tig/str_parse.h"
#include "tig/timer.h"
#include "tig/video.h"
#include "tig/window.h"

typedef int(TigInitFunc)(TigInitInfo* init_info);
typedef void(TigExitFunc)();

static void tig_init_executable();

// 0x5BF2D4
static TigInitFunc* init_funcs[] = {
    tig_memory_init,
    tig_debug_init,
    tig_rect_init,
    tig_file_init,
    tig_color_init,
    tig_video_init,
    tig_palette_init,
    tig_window_init,
    tig_timer_init,
    tig_dxinput_init,
    tig_kb_init,
    tig_art_init,
    tig_mouse_init,
    tig_message_init,
    tig_button_init,
    tig_menu_init,
    tig_font_init,
    tig_draw_init,
    tig_str_parse_init,
    tig_net_init,
    tig_file_cache_init,
    tig_sound_init,
    tig_movie_init,
};

#define NUM_INIT_FUNCS (sizeof(init_funcs) / sizeof(init_funcs[0]))

// 0x5BF330
static TigExitFunc* exit_funcs[] = {
    tig_memory_exit,
    tig_debug_exit,
    tig_rect_exit,
    tig_file_exit,
    tig_color_exit,
    tig_video_exit,
    tig_palette_exit,
    tig_window_exit,
    tig_timer_exit,
    tig_dxinput_exit,
    tig_kb_exit,
    tig_art_exit,
    tig_mouse_exit,
    tig_message_exit,
    tig_button_exit,
    tig_menu_exit,
    tig_font_exit,
    tig_draw_exit,
    tig_str_parse_exit,
    tig_net_exit,
    tig_file_cache_exit,
    tig_sound_exit,
    tig_movie_exit,
};

#define NUM_EXIT_FUNCS (sizeof(exit_funcs) / sizeof(exit_funcs[0]))

static_assert(NUM_INIT_FUNCS == NUM_EXIT_FUNCS, "number of init and exit funcs does not match");

// 0x60F134
static char tig_executable_path[TIG_MAX_PATH];

// 0x60F238
static char* tig_executable_file_name;

// 0x60F23C
static bool tig_initialized;

// 0x60F240
static bool in_tig_exit;

// 0x60F244
static bool tig_active;

// 0x739F34
tig_timestamp_t tig_ping_timestamp;

// 0x51F130
int tig_init(TigInitInfo* init_info)
{
    int index;
    int rc;

    if (tig_initialized) {
        return TIG_ALREADY_INITIALIZED;
    }

    for (index = 0; index < NUM_INIT_FUNCS; ++index) {
        // NOTE: For unknown reason skipping sound is done this way instead
        // of checking for appropriate flag in `tig_sound_init`.
        if ((init_info->flags & TIG_INITIALIZE_NO_SOUND) != 0
            && init_funcs[index] == tig_sound_init) {
            continue;
        }

        rc = init_funcs[index](init_info);
        if (rc != TIG_OK) {
            tig_debug_printf("Error initializing TIG init_func %d", index);

            // Unwind initialized modules.
            for (; index >= 0; --index) {
                exit_funcs[index]();
            }
            return rc;
        }
    }

    atexit(tig_exit);
    tig_init_executable();

    tig_initialized = true;

    return TIG_OK;
}

// 0x51F1D0
void tig_exit(void)
{
    int index;

    if (!tig_initialized) {
        return;
    }

    if (!in_tig_exit) {
        in_tig_exit = true;

        for (index = NUM_EXIT_FUNCS - 1; index >= 0; index--) {
            exit_funcs[index]();
        }

        tig_initialized = false;

        in_tig_exit = false;
    }
}

// 0x51F220
void tig_ping()
{
    tig_timer_now(&tig_ping_timestamp);

    tig_mouse_ping();
    tig_kb_ping();
    tig_message_ping();
    tig_net_ping();
    tig_sound_ping();
    tig_art_ping();
}

// NOTE: Purpose is unclear, both this function and `tig_ping` are public.
//
// 0x51F250
void sub_51F250()
{
    tig_ping();
}

// 0x51F260
const char* tig_get_executable(bool file_name_only)
{
    if (!tig_initialized) {
        tig_init_executable();
    }

    if (file_name_only) {
        return tig_executable_file_name;
    }

    return tig_executable_path;
}

// 0x51F290
void tig_init_executable()
{
    char* pch;

    if (GetModuleFileNameA(NULL, tig_executable_path, sizeof(tig_executable_path)) == 0) {
        tig_debug_println("GetModuleFileName failed.");
        strcpy(tig_executable_path, "<unknown>");
    }

    pch = strrchr(tig_executable_path, '\\');
    if (pch != NULL) {
        tig_executable_file_name = pch + 1;
    } else {
        tig_executable_file_name = tig_executable_path;
    }
}

// 0x51F300
void tig_set_active(bool is_active)
{
    tig_active = is_active;
    tig_mouse_set_active(is_active);
    tig_sound_set_active(is_active);
}

// 0x51F320
bool tig_get_active()
{
    return tig_active;
}
