#ifndef TIG_TYPES_H_
#define TIG_TYPES_H_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TIG_MAX_PATH 260

typedef unsigned int tig_art_id_t;
typedef unsigned int tig_button_handle_t;
typedef unsigned int tig_color_t;
typedef intptr_t tig_font_handle_t;
typedef unsigned int tig_window_handle_t;

typedef struct TigRect TigRect;
typedef struct TigVideoBuffer TigVideoBuffer;
typedef struct TigPaletteModifyInfo TigPaletteModifyInfo;
typedef struct TigArtBlitInfo TigArtBlitInfo;
typedef struct TigMessage TigMessage;
typedef struct TigBmp TigBmp;

// Palette is rare case of non-opaque pointer. It is an array of 256 elements,
// either `uint16_t` (in 16 bpp video mode), or `uint32_t` (in 24 and 32 bpp
// video mode). Be sure to cast appropriately.
typedef void* TigPalette;

#define TIG_OK 0
#define TIG_NOT_INITIALIZED 1
#define TIG_ALREADY_INITIALIZED 2

// Indicates that subsystem has run out of free handles or used all slots
// reserved for appropriate objects.
#define TIG_ERR_OUT_OF_HANDLES 3

#define TIG_ERR_4 4

// Indicates that memory allocation have failed.
#define TIG_ERR_OUT_OF_MEMORY 5

#define TIG_ERR_6 6
#define TIG_ERR_7 7
#define TIG_ERR_10 10
#define TIG_ERR_11 11
#define TIG_ERR_INVALID_PARAM 12
#define TIG_ERR_IO 13
#define TIG_ERR_14 14
#define TIG_ERR_16 16

typedef unsigned int TigInitFlags;

// Display FPS counter in the top-left corner of the window.
//
// NOTE: The counter is rendered using GDI which severely affects
// performance.
#define TIG_INITIALIZE_FPS 0x0001u

// Use double buffering.
#define TIG_INITIALIZE_DOUBLE_BUFFER 0x0002u

// Use video memory (otherwise only system memory is used).
#define TIG_INITIALIZE_VIDEO_MEMORY 0x0004u

// Use intermediary buffer when rendering windows with color key.
#define TIG_INITIALIZE_SCRATCH_BUFFER 0x0010u

// Use windowed mode (otherwise fullscreen).
//
// NOTE: This flag is disabled in the game and enabled in the editor. So it
// might have a different meaning.
#define TIG_INITIALIZE_WINDOWED 0x0020u

// Use `TigInitInfo::message_handler`.
#define TIG_INITIALIZE_MESSAGE_HANDLER 0x0040u

// TODO: Something with window positioning, unclear.
#define TIG_INITIALIZE_0x0080 0x0080u

// Respect settings provided in `TigInitInfo::x` and `TigInitInfo::y`
// (otherwise the window is centered in the screen).
#define TIG_INITIALIZE_POSITIONED 0x0100u

#define TIG_INITIALIZE_3D_SOFTWARE_DEVICE 0x0200u
#define TIG_INITIALIZE_3D_HARDWARE_DEVICE 0x0400u
#define TIG_INITIALIZE_3D_REF_DEVICE 0x0800u

// Use `TigInitInfo::mss_redist_path` to set Miles Sound System redist path.
#define TIG_INITIALIZE_SET_MSS_REDIST_PATH 0x1000u

// Completely disables sound system.
#define TIG_INITIALIZE_NO_SOUND 0x2000u

// Use `TigInitInfo::window_name` to set window name (otherwise it's set to
// the executable name).
#define TIG_INITIALIZE_SET_WINDOW_NAME 0x4000u

#define TIG_INITIALIZE_ANY_3D (TIG_INITIALIZE_3D_SOFTWARE_DEVICE \
    | TIG_INITIALIZE_3D_HARDWARE_DEVICE \
    | TIG_INITIALIZE_3D_REF_DEVICE)

typedef int(TigArtFilePathResolver)(tig_art_id_t art_id, char* path);
typedef tig_art_id_t(TigArtIdResetFunc)(tig_art_id_t art_id);
typedef bool(TigMessageHandler)(LPMSG msg);
typedef int(TigSoundFilePathResolver)(int sound_id, char* path);

typedef struct TigInitInfo {
    /* 0000 */ TigInitFlags flags;
    /* 0004 */ int x;
    /* 0008 */ int y;
    /* 000C */ int width;
    /* 0010 */ int height;
    /* 0014 */ int bpp;
    /* 0018 */ HINSTANCE instance;
    /* 001C */ TigArtFilePathResolver* art_file_path_resolver;
    /* 0020 */ TigArtIdResetFunc* art_id_reset_func;
    /* 0024 */ WNDPROC default_window_proc;
    /* 0028 */ TigMessageHandler* message_handler;
    /* 002C */ TigSoundFilePathResolver* sound_file_path_resolver;
    /* 0030 */ const char* mss_redist_path;
    /* 0034 */ unsigned int texture_width;
    /* 0038 */ unsigned int texture_height;
    /* 003C */ const char* window_name;
} TigInitInfo;

static_assert(sizeof(TigInitInfo) == 0x40, "wrong size");

#ifdef __cplusplus
}
#endif

#endif /* TIG_TYPES_H_ */
