#include "tig/video.h"

#include <d3d.h>
#include <stdio.h>

#include "tig/art.h"
#include "tig/bmp_utils.h"
#include "tig/color.h"
#include "tig/core.h"
#include "tig/debug.h"
#include "tig/draw.h"
#include "tig/file.h"
#include "tig/memory.h"
#include "tig/message.h"
#include "tig/mouse.h"
#include "tig/timer.h"
#include "tig/window.h"

typedef struct TigVideoBuffer {
    /* 0000 */ unsigned int flags;
    /* 0004 */ TigRect frame;
    /* 0014 */ int texture_width;
    /* 0018 */ int texture_height;
    /* 001C */ int pitch;
    /* 0020 */ unsigned int background_color;
    /* 0024 */ unsigned int color_key;
    /* 0028 */ LPDIRECTDRAWSURFACE7 surface;
    /* 002C */ DDSURFACEDESC2 surface_desc;
    /* 00A8 */ union {
        void* pixels;
        uint8_t* p8;
        uint16_t* p16;
        uint32_t* p32;
    } surface_data;
    /* 00AC */ int lock_count;
} TigVideoBuffer;

static_assert(sizeof(TigVideoBuffer) == 0xB0, "wrong size");

typedef struct TigVideoState {
    /* 0000 */ HINSTANCE instance;
    /* 0004 */ HWND wnd;
    /* 0008 */ LPDIRECTDRAW7 ddraw;
    /* 000C */ LPDIRECTDRAWSURFACE7 primary_surface;
    /* 0010 */ LPDIRECTDRAWSURFACE7 offscreen_surface;
    /* 0014 */ LPDIRECTDRAWSURFACE7 main_surface;
    /* 0018 */ LPDIRECTDRAWPALETTE palette;
    /* 001C */ LPDIRECT3D7 d3d;
    /* 0020 */ LPDIRECT3DDEVICE7 d3d_device;
    /* 0024 */ DDSURFACEDESC2 current_surface_desc;
    /* 00A0 */ LPDIRECTDRAWGAMMACONTROL gamma_control;
    /* 00A4 */ bool have_gamma_control;
    /* 00A8 */ DDGAMMARAMP default_gamma_ramp;
    /* 06A8 */ DDGAMMARAMP current_gamma_ramp;
    /* 0CA8 */ unsigned char field_CA8[1124];
} TigVideoState;

static_assert(sizeof(TigVideoState) == 0x110C, "wrong size");

static int tig_video_get_gamma_ramp(LPDDGAMMARAMP gamma_ramp);
static int tig_video_set_gamma_ramp(LPDDGAMMARAMP gamma_ramp);
static bool tig_video_platform_window_init(TigInitInfo* init_info);
static void tig_video_platform_window_exit();
static bool tig_video_ddraw_init(TigInitInfo* init_info);
static bool tig_video_ddraw_init_windowed(TigInitInfo* init_info);
static bool tig_video_ddraw_init_fullscreen(TigInitInfo* init_info);
static bool sub_524830();
static int tig_video_ddraw_exit();
static bool tig_video_d3d_init(TigInitInfo* init_info);
static HRESULT CALLBACK tig_video_3d_check_pixel_format(LPDDPIXELFORMAT lpDDPixFmt, LPVOID lpContext);
static void tig_video_d3d_exit();
static bool tig_video_ddraw_palette_create(LPDIRECTDRAW7 ddraw);
static void tig_video_ddraw_palette_destroy();
static bool tig_video_surface_create(LPDIRECTDRAW7 ddraw, int width, int height, unsigned int caps, LPDIRECTDRAWSURFACE7* surface_ptr);
static void tig_video_surface_destroy(LPDIRECTDRAWSURFACE7* surface_ptr);
static bool tig_video_surface_lock(LPDIRECTDRAWSURFACE7 surface, LPDDSURFACEDESC2 surface_desc, void** surface_data_ptr);
static bool tig_video_surface_unlock(LPDIRECTDRAWSURFACE7 surface, LPDDSURFACEDESC2 surface_desc);
static bool tig_video_surface_fill(LPDIRECTDRAWSURFACE7 surface, const TigRect* rect, tig_color_t color);
static int tig_video_screenshot_make_internal(int key);
static unsigned int tig_video_color_to_mask(COLORREF color);
static void tig_video_print_dd_result(HRESULT hr);
static int tig_video_buffer_data_to_bmp(TigVideoBufferData* video_buffer_data, TigRect* rect, const char* file_name, bool palette_indexed);
static int tig_video_3d_set_viewport(TigVideoBuffer* video_buffer);
static void sub_526530(const TigRect* a, const TigRect* b, D3DCOLOR* alpha);
static void sub_526690(const TigRect* a, const TigRect* b, D3DCOLOR* color);

// 0x5BF3D8
static int tig_video_screenshot_key = -1;

// 0x60F250
static TigVideoState tig_video_state;

// 0x60FEF8
static float tig_video_gamma;

// 0x60FEFC
static RECT tig_video_client_rect;

// 0x60FF0C
static unsigned int tig_video_palette[256];

// 0x61030C
static int tig_video_bpp;

// 0x610310
static bool tig_video_fullscreen;

// 0x610314
static bool tig_video_double_buffered;

// 0x610318
static bool tig_video_can_use_video_memory;

// 0x61031C
static bool tig_video_3d_initialized;

// 0x610320
static bool tig_video_3d_is_hardware;

// 0x610324
static bool tig_video_3d_texture_must_be_power_of_two;

// 0x610328
static bool tig_video_3d_texture_must_be_square;

// 0x61032C
static int tig_video_3d_extra_surface_caps;

// 0x610330
static int tig_video_3d_extra_surface_caps2;

// 0x610334
static TigVideoBuffer* tig_video_3d_viewport;

// 0x610338
static DDPIXELFORMAT tig_video_3d_pixel_format;

// 0x610358
static bool tig_video_3d_scene_started;

// 0x610388
static TigRect stru_610388;

// 0x610398
static bool tig_video_main_surface_locked;

// 0x61039C
static bool tig_video_initialized;

// 0x6103A0
static bool tig_video_show_fps;

// 0x6103A4
static int dword_6103A4;

// 0x51F330
int tig_video_init(TigInitInfo* init_info)
{
    memset(&tig_video_state, 0, sizeof(tig_video_state));

    if ((init_info->flags & TIG_INITIALIZE_WINDOWED) != 0) {
        if (init_info->default_window_proc == NULL) {
            return TIG_ERR_12;
        }

        tig_message_set_default_window_proc(init_info->default_window_proc);
    }

    if (!tig_video_platform_window_init(init_info)) {
        return TIG_ERR_16;
    }

    if (!tig_video_ddraw_init(init_info)) {
        tig_video_platform_window_exit();
        return TIG_ERR_16;
    }

    tig_video_show_fps = (init_info->flags & TIG_INITIALIZE_FPS) != 0;
    tig_video_can_use_video_memory = (init_info->flags & TIG_INITIALIZE_VIDEO_MEMORY) != 0;
    tig_video_bpp = init_info->bpp;
    tig_video_main_surface_locked = false;

    ShowCursor((init_info->flags & TIG_INITIALIZE_WINDOWED) != 0 ? TRUE : FALSE);

    tig_video_screenshot_key = -1;
    dword_6103A4 = 0;

    tig_video_initialized = true;

    return TIG_OK;
}

// 0x51F3F0
void tig_video_exit()
{
    tig_video_ddraw_exit();
    tig_video_platform_window_exit();
    tig_video_initialized = false;
}

// 0x51F410
int tig_video_platform_window_get(HWND* wnd_ptr)
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    *wnd_ptr = tig_video_state.wnd;

    return TIG_OK;
}

// 0x51F430
int tig_video_instance_get(HINSTANCE* instance_ptr)
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    *instance_ptr = tig_video_state.instance;

    return TIG_OK;
}

// 0x51F450
int tig_video_main_surface_get(LPDIRECTDRAWSURFACE7* surface_ptr)
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    *surface_ptr = tig_video_state.main_surface;

    return TIG_OK;
}

// 0x51F470
void tig_video_set_client_rect(LPRECT rect)
{
    tig_video_client_rect = *rect;
}

// 0x51F4A0
void tig_video_display_fps()
{
    // 0x60F248
    static tig_timestamp_t curr;

    // 0x61035C
    static char fps_string_buffer[32];

    // 0x61037C
    static float fps;

    // 0x610380
    static tig_timestamp_t prev;

    // 0x610384
    static unsigned int counter;

    tig_duration_t elapsed;
    HDC hdc;

    if (tig_video_show_fps) {
        ++counter;
        if (counter >= 10) {
            tig_timer_now(&curr);
            elapsed = tig_timer_between(prev, curr);
            fps = (float)counter / ((float)elapsed / 1000.0f);
            sprintf(fps_string_buffer, "%10.0f", fps);
            prev = curr;
            counter = 0;
        }

        IDirectDrawSurface7_GetDC(tig_video_state.main_surface, &hdc);
        TextOutA(hdc, 0, 0, fps_string_buffer, strlen(fps_string_buffer));
        IDirectDrawSurface7_ReleaseDC(tig_video_state.main_surface, hdc);
    }
}

// 0x51F590
int tig_video_main_surface_lock(void** surface_data_ptr)
{
    if (tig_video_main_surface_locked) {
        return TIG_ERR_6;
    }

    if (!tig_video_surface_lock(tig_video_state.main_surface, &(tig_video_state.current_surface_desc), surface_data_ptr)) {
        return TIG_ERR_7;
    }

    tig_video_main_surface_locked = true;

    return TIG_OK;
}

// 0x51F5D0
int tig_video_main_surface_unlock()
{
    if (tig_video_main_surface_locked) {
        tig_video_surface_unlock(tig_video_state.main_surface, &(tig_video_state.current_surface_desc));
        tig_video_main_surface_locked = false;
    }

    return TIG_OK;
}

// 0x51F600
int tig_video_blit(TigVideoBuffer* src_video_buffer, TigRect* src_rect, TigRect* dst_rect, bool to_primary_surface)
{
    HRESULT hr;
    TigRect clamped_dst_rect;
    int rc;
    LPDIRECTDRAWSURFACE7 dst_surface;
    RECT native_src_rect;
    RECT native_dst_rect;
    DWORD flags;

    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    rc = tig_rect_intersection(dst_rect, &stru_610388, &clamped_dst_rect);
    if (rc != TIG_OK) {
        return rc;
    }

    dst_surface = to_primary_surface
        ? tig_video_state.primary_surface
        : tig_video_state.main_surface;

    native_src_rect.left = clamped_dst_rect.x - dst_rect->x + src_rect->x;
    native_src_rect.top = clamped_dst_rect.y - dst_rect->y + src_rect->y;
    native_src_rect.right = native_src_rect.left + clamped_dst_rect.width - dst_rect->width + src_rect->width;
    native_src_rect.bottom = native_src_rect.top + clamped_dst_rect.height - dst_rect->height + src_rect->height;

    if (tig_video_fullscreen) {
        flags = DDBLTFAST_WAIT;
        if ((src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) != 0) {
            flags |= DDBLTFAST_SRCCOLORKEY;
        }

        hr = IDirectDrawSurface7_BltFast(dst_surface,
            clamped_dst_rect.x,
            clamped_dst_rect.y,
            src_video_buffer->surface,
            &native_src_rect,
            flags);
        if (hr == DDERR_SURFACELOST) {
            IDirectDrawSurface7_Restore(dst_surface);
            hr = IDirectDrawSurface7_BltFast(dst_surface,
                clamped_dst_rect.x,
                clamped_dst_rect.y,
                src_video_buffer->surface,
                &native_src_rect,
                flags);
        }
    } else {
        native_dst_rect.left = tig_video_client_rect.left + clamped_dst_rect.x;
        native_dst_rect.top = tig_video_client_rect.top + clamped_dst_rect.y;
        native_dst_rect.right = native_dst_rect.left + clamped_dst_rect.width;
        native_dst_rect.bottom = native_dst_rect.top + clamped_dst_rect.height;

        flags = DDBLT_WAIT;
        if ((src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) != 0) {
            flags |= DDBLT_KEYSRC;
        }

        hr = IDirectDrawSurface7_Blt(dst_surface,
            &native_dst_rect,
            src_video_buffer->surface,
            &native_src_rect,
            flags,
            NULL);
        if (hr == DDERR_SURFACELOST) {
            IDirectDrawSurface7_Restore(dst_surface);
            hr = IDirectDrawSurface7_Blt(dst_surface,
                &native_dst_rect,
                src_video_buffer->surface,
                &native_src_rect,
                flags,
                NULL);
        }
    }

    if (FAILED(hr)) {
        tig_debug_printf("DirectX error %d in tig_video_blit()\n", hr);
        return TIG_ERR_11;
    }

    return TIG_OK;
}

// 0x51F7C0
int tig_video_fill(const TigRect* rect, tig_color_t color)
{
    int rc;
    TigRect clamped_rect;

    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    rc = tig_rect_intersection(rect != NULL ? rect : &stru_610388,
        &stru_610388,
        &clamped_rect);
    if (rc != TIG_OK) {
        return rc;
    }

    if (!tig_video_fullscreen) {
        clamped_rect.x += tig_video_client_rect.left;
        clamped_rect.y += tig_video_client_rect.top;
    }

    if (tig_video_surface_fill(tig_video_state.main_surface, &clamped_rect, color) != TIG_OK) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x51F860
int sub_51F860()
{
    HRESULT hr;

    hr = IDirectDrawSurface7_IsLost(tig_video_state.main_surface);
    if (FAILED(hr)) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x51F880
int sub_51F880()
{
    HRESULT hr;
    TigMessage message;

    if (sub_51F860() == TIG_OK) {
        return TIG_OK;
    }

    hr = IDirectDraw7_TestCooperativeLevel(tig_video_state.ddraw);
    if (FAILED(hr)) {
        return TIG_ERR_16;
    }

    hr = IDirectDraw7_RestoreAllSurfaces(tig_video_state.ddraw);
    if (FAILED(hr)) {
        tig_video_print_dd_result(hr);
        return TIG_ERR_16;
    }

    tig_mouse_cursor_refresh();
    tig_window_set_needs_display_in_rect(NULL);

    tig_timer_now(&(message.timestamp));
    message.type = TIG_MESSAGE_REDRAW;
    return tig_message_enqueue(&message);
}

// 0x51F8F0
int tig_video_flip()
{
    HRESULT hr;
    RECT src_rect;

    if (tig_video_double_buffered) {
        if (tig_video_fullscreen) {
            hr = IDirectDrawSurface7_Flip(tig_video_state.primary_surface, NULL, 1);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            src_rect.left = stru_610388.x;
            src_rect.top = stru_610388.y;
            src_rect.right = stru_610388.x + stru_610388.width;
            src_rect.bottom = stru_610388.y + stru_610388.height;

            hr = IDirectDrawSurface7_BltFast(tig_video_state.offscreen_surface,
                0,
                0,
                tig_video_state.primary_surface,
                &src_rect,
                DDBLTFAST_WAIT);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        } else {
            src_rect.left = stru_610388.x;
            src_rect.top = stru_610388.y;
            src_rect.right = stru_610388.x + stru_610388.width;
            src_rect.bottom = stru_610388.y + stru_610388.height;

            hr = IDirectDrawSurface7_Blt(tig_video_state.primary_surface,
                &tig_video_client_rect,
                tig_video_state.offscreen_surface,
                &src_rect,
                DDBLT_WAIT,
                NULL);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        }
    }

    return TIG_OK;
}

// 0x51F9E0
int tig_video_screenshot_set_settings(TigVideoScreenshotSettings* settings)
{
    int rc;

    if (tig_video_screenshot_key != -1) {
        rc = tig_message_set_key_handler(NULL, tig_video_screenshot_key);
        if (rc != TIG_OK) {
            return rc;
        }
    }

    tig_video_screenshot_key = settings->key;
    dword_6103A4 = settings->field_4;

    if (tig_video_screenshot_key != -1) {
        rc = tig_message_set_key_handler(tig_video_screenshot_make_internal, tig_video_screenshot_key);
        if (rc != TIG_OK) {
            return rc;
        }
    }

    return TIG_OK;
}

// 0x51FA30
int tig_video_screenshot_make()
{
    return tig_video_screenshot_make_internal(tig_video_screenshot_key);
}

// 0x51FA40
int sub_51FA40(TigRect* rect)
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    *rect = stru_610388;

    return TIG_OK;
}

// 0x51FA80
int tig_video_get_bpp(int* bpp)
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    *bpp = tig_video_bpp;

    return TIG_OK;
}

// 0x51FAA0
int tig_video_get_palette(unsigned int* colors)
{
    int index;

    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    if (tig_video_bpp == 8) {
        for (index = 0; index < 256; index++) {
            colors[index] = tig_video_palette[index];
        }
    } else {
        memset(colors, 0, sizeof(int) * 256);
    }

    return TIG_OK;
}

// 0x51FAF0
int tig_video_get_pitch(int* pitch)
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    *pitch = (int)tig_video_state.current_surface_desc.lPitch;

    return TIG_OK;
}

// 0x51FB10
int tig_video_3d_check_initialized()
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    if (!tig_video_3d_initialized) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x51FB30
int tig_video_3d_check_hardware()
{
    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    if (!tig_video_3d_is_hardware) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x51FB50
int tig_video_3d_begin_scene()
{
    if (sub_51F860() != TIG_OK) {
        return TIG_ERR_16;
    }

    if (tig_video_3d_initialized) {
        if (FAILED(IDirect3DDevice7_BeginScene(tig_video_state.d3d_device))) {
            return TIG_ERR_16;
        }
    }

    tig_video_3d_scene_started = true;
    return TIG_OK;
}

// 0x51FBA0
int tig_video_3d_end_scene()
{
    if (!tig_video_3d_scene_started) {
        return TIG_ERR_16;
    }

    if (tig_video_3d_initialized) {
        if (FAILED(IDirect3DDevice7_EndScene(tig_video_state.d3d_device))) {
            return TIG_ERR_16;
        }
    }

    tig_video_3d_scene_started = false;
    return TIG_OK;
}

// 0x51FBF0
int tig_video_get_video_memory_status(size_t* total_memory, size_t* available_memory)
{
    DDSCAPS2 ddscaps2;
    DWORD total;
    DWORD available;
    HRESULT hr;

    if (!tig_video_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    ddscaps2.dwCaps = tig_video_3d_initialized
        ? DDSCAPS_TEXTURE
        : DDSCAPS_LOCALVIDMEM | DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    ddscaps2.dwCaps2 = 0;
    ddscaps2.dwCaps3 = 0;
    ddscaps2.dwCaps4 = 0;

    hr = IDirectDraw7_GetAvailableVidMem(tig_video_state.ddraw, &ddscaps2, &total, &available);
    if (FAILED(hr)) {
        tig_video_print_dd_result(hr);
        tig_debug_printf("Error determining approx. available surface memory.\n");
        return TIG_ERR_OUT_OF_MEMORY;
    }

    *total_memory = (size_t)total;
    *available_memory = (size_t)available;

    return TIG_OK;
}

// 0x51FC90
int tig_video_check_gamma_control()
{
    if (!tig_video_state.have_gamma_control) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x51FCA0
int tig_video_fade(int color, int steps, float duration, unsigned int flags)
{
    DDGAMMARAMP curr_gamma_ramp;
    DDGAMMARAMP temp_gamma_ramp;
    LPDDGAMMARAMP dest_gamma_ramp;

    if (!tig_video_state.have_gamma_control) {
        return TIG_ERR_16;
    }

    if (tig_video_get_gamma_ramp(&curr_gamma_ramp) != TIG_OK) {
        return TIG_ERR_16;
    }

    if ((flags & 0x1) != 0) {
        dest_gamma_ramp = &(tig_video_state.current_gamma_ramp);
    } else {
        int index;
        unsigned int red = tig_color_get_red(color) << 8;
        unsigned int green = tig_color_get_green(color) << 8;
        unsigned int blue = tig_color_get_blue(color) << 8;

        for (index = 0; index < 256; index++) {
            temp_gamma_ramp.red[index] = (WORD)red;
            temp_gamma_ramp.green[index] = (WORD)green;
            temp_gamma_ramp.blue[index] = (WORD)blue;
        }

        dest_gamma_ramp = &temp_gamma_ramp;
    }

    if (steps < 1 || duration < 1.0) {
        if (tig_video_set_gamma_ramp(dest_gamma_ramp) != TIG_OK) {
            return TIG_ERR_16;
        }
    } else {
        int duration_per_step = (int)((double)duration / (double)steps);
        int step;
        unsigned int time;
        int index;

        for (step = 0; step < steps; step++) {
            tig_timer_now(&time);

            // TODO: Unclear.
            for (index = 0; index < 256; index++) {
                curr_gamma_ramp.red[index] = (WORD)((dest_gamma_ramp->red[index] - curr_gamma_ramp.red[index]) / (steps - step));
                curr_gamma_ramp.green[index] = (WORD)((dest_gamma_ramp->green[index] - curr_gamma_ramp.green[index]) / (steps - step));
                curr_gamma_ramp.blue[index] = (WORD)((dest_gamma_ramp->blue[index] - curr_gamma_ramp.blue[index]) / (steps - step));

                if (tig_video_set_gamma_ramp(&curr_gamma_ramp) != TIG_OK) {
                    return TIG_ERR_16;
                }
            }

            while (tig_timer_elapsed(time) < duration_per_step) {
            }
        }
    }

    return TIG_OK;
}

// 0x51FEC0
int tig_video_get_gamma_ramp(LPDDGAMMARAMP gamma_ramp)
{
    int value;
    int index;

    if (!tig_video_state.have_gamma_control) {
        return TIG_ERR_16;
    }

    if (FAILED(IDirectDrawGammaControl_GetGammaRamp(tig_video_state.gamma_control, 0, gamma_ramp))) {
        return TIG_ERR_16;
    }

    value = 0;
    for (index = 0; index < 256; index++) {
        value |= gamma_ramp->red[index] | gamma_ramp->green[index] | gamma_ramp->blue[index];
    }

    if ((value & 0xFF00) == 0) {
        for (index = 0; index < 256; index++) {
            gamma_ramp->red[index] = (uint8_t)gamma_ramp->red[index] << 8;
            gamma_ramp->green[index] = (uint8_t)gamma_ramp->green[index] << 8;
            gamma_ramp->blue[index] = (uint8_t)gamma_ramp->blue[index] << 8;
        }
    }

    return TIG_OK;
}

// 0x51FF60
int tig_video_set_gamma_ramp(LPDDGAMMARAMP gamma_ramp)
{
    int index;

    if (!tig_video_state.have_gamma_control) {
        return TIG_ERR_16;
    }

    for (index = 0; index < 256; index++) {
        gamma_ramp->red[index] = (gamma_ramp->red[index] & 0xFF00) | ((gamma_ramp->red[index] & 0xFF00) >> 8);
        gamma_ramp->green[index] = (gamma_ramp->green[index] & 0xFF00) | ((gamma_ramp->green[index] & 0xFF00) >> 8);
        gamma_ramp->blue[index] = (gamma_ramp->blue[index] & 0xFF00) | ((gamma_ramp->blue[index] & 0xFF00) >> 8);
    }

    if (FAILED(IDirectDrawGammaControl_SetGammaRamp(tig_video_state.gamma_control, 0, gamma_ramp))) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x51FFE0
int tig_video_set_gamma(float gamma)
{
    int index;
    unsigned int red;
    unsigned int green;
    unsigned int blue;

    if (!tig_video_state.have_gamma_control) {
        return TIG_ERR_16;
    }

    if (gamma == tig_video_gamma) {
        return TIG_OK;
    }

    if (gamma < 0.0 || gamma >= 2.0) {
        return TIG_ERR_12;
    }

    for (index = 0; index < 256; index++) {
        red = (unsigned int)((float)tig_video_state.default_gamma_ramp.red[index] * gamma);
        tig_video_state.current_gamma_ramp.red[index] = (WORD)min(red, 0xFF00);

        green = (unsigned int)((float)tig_video_state.default_gamma_ramp.green[index] * gamma);
        tig_video_state.current_gamma_ramp.green[index] = (WORD)min(green, 0xFF00);

        blue = (unsigned int)((float)tig_video_state.default_gamma_ramp.blue[index] * gamma);
        tig_video_state.current_gamma_ramp.blue[index] = (WORD)min(blue, 0xFF00);
    }

    tig_video_gamma = gamma;
    tig_video_fade(0, 0, 0.0, 1);

    return TIG_OK;
}

// 0x5200F0
int tig_video_buffer_create(TigVideoBufferCreateInfo* vb_create_info, TigVideoBuffer** video_buffer_ptr)
{
    TigVideoBuffer* video_buffer;
    DWORD caps;
    int texture_width;
    int texture_height;

    video_buffer = (TigVideoBuffer*)MALLOC(sizeof(*video_buffer));
    memset(video_buffer, 0, sizeof(*video_buffer));
    *video_buffer_ptr = video_buffer;

    caps = DDSCAPS_OFFSCREENPLAIN;

    if (tig_video_can_use_video_memory) {
        if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_VIDEO_MEMORY) != 0) {
            caps |= DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;
        } else if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY) != 0) {
            caps |= DDSCAPS_SYSTEMMEMORY;
        }
    } else {
        caps |= DDSCAPS_SYSTEMMEMORY;
    }

    if (tig_video_3d_initialized) {
        if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_3D) != 0) {
            caps |= DDSCAPS_3DDEVICE;
        }

        if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_TEXTURE) != 0) {
            caps &= ~DDSCAPS_OFFSCREENPLAIN;
            caps |= DDSCAPS_TEXTURE;
        }
    }

    texture_width = vb_create_info->width;
    texture_height = vb_create_info->height;

    if ((caps & DDSCAPS_TEXTURE) != 0) {
        if (tig_video_3d_texture_must_be_power_of_two) {
            texture_width = 1;
            while (texture_width < vb_create_info->width) {
                texture_width *= 2;
            }

            texture_height = 1;
            while (texture_height < vb_create_info->height) {
                texture_height *= 2;
            }
        }

        if (tig_video_3d_texture_must_be_square) {
            if (texture_height < texture_width) {
                texture_height = texture_width;
            } else {
                texture_width = texture_height;
            }
        }
    }

    if (!tig_video_surface_create(tig_video_state.ddraw, texture_width, texture_height, caps, &(video_buffer->surface))) {
        if ((caps & DDSCAPS_VIDEOMEMORY) == 0) {
            tig_debug_printf("*Request to allocate surface (?] memory failed (%dx%d) [%d].\n",
                texture_width,
                texture_height,
                caps);
            FREE(video_buffer);
            return TIG_ERR_OUT_OF_MEMORY;
        }

        caps &= ~DDSCAPS_LOCALVIDMEM;
        caps |= DDSCAPS_NONLOCALVIDMEM;
        if (!tig_video_surface_create(tig_video_state.ddraw, texture_width, texture_height, caps, &(video_buffer->surface))) {
            if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_3D) != 0) {
                tig_debug_printf("tig_video_buffer_create: Error trying to create surface for 3D, flushing...\n");
                tig_art_flush();

                if (!tig_video_surface_create(tig_video_state.ddraw, texture_width, texture_height, caps, &(video_buffer->surface))) {
                    tig_debug_printf("tig_video_buffer_create: Error, flushing FAILED to fix problem!\n");

                    caps &= ~(DDSCAPS_NONLOCALVIDMEM | DDSCAPS_VIDEOMEMORY);
                    caps |= DDSCAPS_SYSTEMMEMORY;
                    tig_debug_printf("*Request to allocate surface in video memory failed.\n");
                    if (!tig_video_surface_create(tig_video_state.ddraw, texture_width, texture_height, caps, &(video_buffer->surface))) {
                        tig_debug_printf(
                            "*Request to allocate surface in SYSTEM memory failed (%dx%d) [%d].\n",
                            texture_width,
                            texture_height,
                            caps);
                        FREE(video_buffer);
                        return TIG_ERR_OUT_OF_MEMORY;
                    }
                }
            } else {
                caps &= ~(DDSCAPS_NONLOCALVIDMEM | DDSCAPS_VIDEOMEMORY);
                caps |= DDSCAPS_SYSTEMMEMORY;
                tig_debug_printf("*Request to allocate surface in video memory failed.\n");
                if (!tig_video_surface_create(tig_video_state.ddraw, texture_width, texture_height, caps, &(video_buffer->surface))) {
                    tig_debug_printf(
                        "*Request to allocate surface in SYSTEM memory failed (%dx%d) [%d].\n",
                        texture_width,
                        texture_height,
                        caps);
                    FREE(video_buffer);
                    return TIG_ERR_OUT_OF_MEMORY;
                }
            }
        }
    }

    if ((caps & DDSCAPS_VIDEOMEMORY) != 0) {
        video_buffer->flags |= TIG_VIDEO_BUFFER_VIDEO_MEMORY;
    } else if ((caps & (DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE)) == 0) {
        video_buffer->flags |= TIG_VIDEO_BUFFER_SYSTEM_MEMORY;
    }

    if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_COLOR_KEY) != 0) {
        video_buffer->flags |= TIG_VIDEO_BUFFER_COLOR_KEY;
        tig_video_buffer_set_color_key(*video_buffer_ptr, vb_create_info->color_key);
    }

    if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_3D) != 0) {
        video_buffer->flags |= TIG_VIDEO_BUFFER_3D;
    }

    if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_TEXTURE) != 0) {
        video_buffer->flags |= TIG_VIDEO_BUFFER_TEXTURE;
    }

    video_buffer->frame.x = 0;
    video_buffer->frame.y = 0;
    video_buffer->frame.width = vb_create_info->width;
    video_buffer->frame.height = vb_create_info->height;
    video_buffer->texture_width = texture_width;
    video_buffer->texture_height = texture_height;
    video_buffer->background_color = vb_create_info->background_color;

    if ((vb_create_info->flags & TIG_VIDEO_BUFFER_CREATE_TEXTURE) == 0) {
        tig_video_surface_fill(video_buffer->surface, NULL, vb_create_info->background_color);
    }

    video_buffer->pitch = 0;
    video_buffer->surface_data.pixels = NULL;
    video_buffer->lock_count = 0;

    return TIG_OK;
}

// 0x520390
int tig_video_buffer_destroy(TigVideoBuffer* video_buffer)
{
    if (video_buffer == NULL) {
        return TIG_ERR_16;
    }

    if (tig_video_3d_viewport == video_buffer) {
        tig_video_3d_viewport = NULL;
    }

    tig_video_surface_destroy(&(video_buffer->surface));
    FREE(video_buffer);

    return TIG_OK;
}

// 0x5203E0
int tig_video_buffer_data(TigVideoBuffer* video_buffer, TigVideoBufferData* video_buffer_data)
{
    if (video_buffer == NULL) {
        return TIG_ERR_16;
    }

    video_buffer_data->flags = video_buffer->flags;
    video_buffer_data->width = video_buffer->frame.width;
    video_buffer_data->height = video_buffer->frame.height;

    if ((video_buffer->flags & TIG_VIDEO_BUFFER_LOCKED) != 0) {
        video_buffer_data->pitch = video_buffer->pitch;
    } else {
        video_buffer_data->pitch = 0;
    }

    video_buffer_data->background_color = video_buffer->background_color;
    video_buffer_data->color_key = video_buffer->color_key;
    video_buffer_data->bpp = tig_video_bpp;

    if ((video_buffer->flags & TIG_VIDEO_BUFFER_LOCKED) != 0) {
        video_buffer_data->surface_data.pixels = video_buffer->surface_data.pixels;
    } else {
        video_buffer_data->surface_data.pixels = NULL;
    }

    return TIG_OK;
}

// 0x520450
int tig_video_buffer_set_color_key(TigVideoBuffer* video_buffer, int color_key)
{
    if ((video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) == 0) {
        return TIG_ERR_16;
    }

    DDCOLORKEY ddck;
    ddck.dwColorSpaceLowValue = color_key;
    ddck.dwColorSpaceHighValue = color_key;

    HRESULT hr = IDirectDrawSurface7_SetColorKey(video_buffer->surface, DDCKEY_SRCBLT, &ddck);
    if (FAILED(hr)) {
        tig_video_print_dd_result(hr);
        return TIG_ERR_16;
    }

    video_buffer->color_key = color_key;

    return TIG_OK;
}

// 0x5204B0
int tig_video_buffer_lock(TigVideoBuffer* video_buffer)
{
    if (video_buffer->lock_count == 0) {
        if (!tig_video_surface_lock(video_buffer->surface, &(video_buffer->surface_desc), &(video_buffer->surface_data.pixels))) {
            return TIG_ERR_7;
        }

        video_buffer->pitch = video_buffer->surface_desc.lPitch;
        video_buffer->flags |= TIG_VIDEO_BUFFER_LOCKED;
    }

    video_buffer->lock_count++;

    return TIG_OK;
}

// 0x520500
int tig_video_buffer_unlock(TigVideoBuffer* video_buffer)
{
    if (video_buffer->lock_count == 1) {
        tig_video_surface_unlock(video_buffer->surface, &(video_buffer->surface_desc));
        video_buffer->flags &= ~TIG_VIDEO_BUFFER_LOCKED;
    }

    video_buffer->lock_count--;

    return TIG_OK;
}

// 0x520540
int tig_video_buffer_outline(TigVideoBuffer* video_buffer, TigRect* rect, int color)
{
    TigRect line;
    int rc;

    line.x = rect->x;
    line.y = rect->y;
    line.width = rect->width;
    line.height = 1;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    line.x = rect->x;
    line.y = rect->y;
    line.width = 1;
    line.height = rect->height;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    line.x = rect->x + rect->width - 1;
    line.y = rect->y;
    line.width = 1;
    line.height = rect->height;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    line.x = rect->x;
    line.y = rect->y + rect->height - 1;
    line.width = rect->width;
    line.height = 1;
    rc = tig_video_buffer_fill(video_buffer, &line, color);
    if (rc != TIG_OK) {
        return rc;
    }

    return TIG_OK;
}

// 0x520630
int tig_video_buffer_fill(TigVideoBuffer* video_buffer, TigRect* rect, int color)
{
    if (!tig_video_surface_fill(video_buffer->surface, rect, color)) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x520660
int tig_video_buffer_line(TigVideoBuffer* video_buffer, TigLine* line, TigRect* a3, unsigned int color)
{
    (void)a3;

    int pattern = 0;
    bool reversed;
    TigDrawLineModeInfo mode_info;
    TigDrawLineStyleInfo style_info;
    int thick;
    int thickness;
    int dx;
    int dy;
    int step_x;
    int step_y;
    int error;
    int error_y;
    int error_x;
    int x1;
    int y1;
    int x2;
    int y2;
    int x;
    int y;

    if (tig_video_buffer_lock(video_buffer) != TIG_OK) {
        return TIG_ERR_7;
    }

    if (line->y2 <= line->y1) {
        if (line->y2 < line->y1) {
            reversed = true;
        } else {
            reversed = line->x1 >= line->x2;
        }
    } else {
        reversed = false;
    }

    tig_draw_get_line_mode(&mode_info);
    tig_draw_get_line_style(&style_info);

    thickness = mode_info.thickness / 2 + 1;
    dx = abs(line->x2 - line->x1);
    dy = abs(line->y2 - line->y1);

    if (reversed) {
        if (dx < dy) {
            error_y = 2 * dx;
            error = 2 * dx - dy;
            error_x = 2 * (dx - dy);
        } else {
            error_y = 2 * dy;
            error = 2 * dy - dx;
            error_x = 2 * (dy - dx);
        }

        x1 = line->x2;
        y1 = line->y2;
        x2 = line->x1;
        y2 = line->y1;

        if (line->x1 < line->x2) {
            step_x = -1;
        } else if (line->x1 > line->x2) {
            step_x = 1;
        } else {
            step_x = 0;
        }
    } else {
        if (dx < dy) {
            error_y = 2 * dx;
            error = 2 * dx - dy;
            error_x = 2 * (dx - dy);
        } else {
            error_y = 2 * dy;
            error = 2 * dy - dx;
            error_x = 2 * (dy - dx);
        }

        x1 = line->x1;
        y1 = line->y1;
        x2 = line->x2;
        y2 = line->y2;

        if (line->x1 < line->x2) {
            step_x = 1;
        } else if (line->x1 > line->x2) {
            step_x = -1;
        } else {
            step_x = 0;
        }
    }

    switch (tig_video_bpp) {
    case 8:
        if (1) {
            uint8_t* dst = (uint8_t*)video_buffer->surface_data.pixels + video_buffer->pitch * y1 + x1;
            *dst = (uint8_t)color;

            if (dx < dy) {
                y = y1;
                while (y != y2) {
                    if (error > 0) {
                        error += error_x;
                        y++;
                        dst += video_buffer->pitch + step_x;
                    } else {
                        error += error_y;
                        y++;
                        dst += video_buffer->pitch;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = (uint8_t)color;
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = (uint8_t)color;
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = (uint8_t)color;
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            } else {
                if (line->y1 == line->y2) {
                    step_y = step_x;
                } else {
                    step_y = step_x + video_buffer->pitch;
                }

                x = x1;
                while (x != x2) {
                    x += step_x;
                    if (error > 0) {
                        error += error_x;
                        dst += step_y;
                    } else {
                        error += error_y;
                        dst += step_x;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = (uint8_t)color;
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = (uint8_t)color;
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = (uint8_t)color;
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            }
        }
        break;
    case 16:
        if (1) {
            uint16_t* dst = (uint16_t*)video_buffer->surface_data.pixels + (video_buffer->pitch / 2) * y1 + x1;
            *dst = (uint16_t)color;

            // FIXME: This is the only video mode (16-bpp) where the game
            // supports line thickness, but even in this mode there is a bug -
            // the beginning of line (drawn above) is always 1 px thick. See
            // thickness loop below to get the idea of what's missing.

            if (dx < dy) {
                y = y1;
                while (y != y2) {
                    if (error > 0) {
                        error += error_x;
                        y++;
                        dst += video_buffer->pitch + step_x;
                    } else {
                        error += error_y;
                        y++;
                        dst += video_buffer->pitch;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = (uint16_t)color;
                        if (mode_info.mode == TIG_DRAW_LINE_MODE_THICK) {
                            for (thick = 1; thick < thickness; thick++) {
                                dst[-thick] = (uint16_t)color;
                                dst[thick] = (uint16_t)color;
                            }
                        }
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = (uint16_t)color;
                            if (mode_info.mode == TIG_DRAW_LINE_MODE_THICK) {
                                for (thick = 1; thick < thickness; thick++) {
                                    dst[-thick] = (uint16_t)color;
                                    dst[thick] = (uint16_t)color;
                                }
                            }
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = (uint16_t)color;
                            if (mode_info.mode == TIG_DRAW_LINE_MODE_THICK) {
                                for (thick = 1; thick < thickness; thick++) {
                                    dst[-thick] = (uint16_t)color;
                                    dst[thick] = (uint16_t)color;
                                }
                            }
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            } else {
                if (line->y1 == line->y2) {
                    step_y = step_x;
                } else {
                    step_y = step_x + video_buffer->pitch;
                }

                x = x1;
                while (x != x2) {
                    x += step_x;
                    if (error > 0) {
                        error += error_x;
                        dst += step_y;
                    } else {
                        error += error_y;
                        dst += step_x;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = (uint16_t)color;
                        if (mode_info.mode == TIG_DRAW_LINE_MODE_THICK) {
                            for (thick = 1; thick < thickness; thick++) {
                                dst[-thick * (video_buffer->pitch / 2)] = (uint16_t)color;
                                dst[thick * (video_buffer->pitch / 2)] = (uint16_t)color;
                            }
                        }
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = (uint16_t)color;
                            if (mode_info.mode == TIG_DRAW_LINE_MODE_THICK) {
                                for (thick = 1; thick < thickness; thick++) {
                                    dst[-thick * (video_buffer->pitch / 2)] = (uint16_t)color;
                                    dst[thick * (video_buffer->pitch / 2)] = (uint16_t)color;
                                }
                            }
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = (uint16_t)color;
                            if (mode_info.mode == TIG_DRAW_LINE_MODE_THICK) {
                                for (thick = 1; thick < thickness; thick++) {
                                    dst[-thick * (video_buffer->pitch / 2)] = (uint16_t)color;
                                    dst[thick * (video_buffer->pitch / 2)] = (uint16_t)color;
                                }
                            }
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            }
        }
        break;
    case 24:
        if (1) {
            uint8_t* dst = (uint8_t*)video_buffer->surface_data.pixels + (video_buffer->pitch / 3) * y1 + x1;
            dst[0] = (uint8_t)color;
            dst[1] = (uint8_t)(color >> 8);
            dst[2] = (uint8_t)(color >> 16);

            if (dx < dy) {
                y = y1;
                while (y != y2) {
                    if (error > 0) {
                        error += error_x;
                        y++;
                        dst += video_buffer->pitch / 3 + step_x;
                    } else {
                        error += error_y;
                        y++;
                        dst += video_buffer->pitch / 3;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        dst[0] = (uint8_t)color;
                        dst[1] = (uint8_t)(color >> 8);
                        dst[2] = (uint8_t)(color >> 16);
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            dst[0] = (uint8_t)color;
                            dst[1] = (uint8_t)(color >> 8);
                            dst[2] = (uint8_t)(color >> 16);
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            dst[0] = (uint8_t)color;
                            dst[1] = (uint8_t)(color >> 8);
                            dst[2] = (uint8_t)(color >> 16);
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            } else {
                if (line->y1 == line->y2) {
                    step_y = step_x;
                } else {
                    step_y = step_x + video_buffer->pitch / 3;
                }

                x = x1;
                while (x != x2) {
                    x += step_x;
                    if (error > 0) {
                        error += error_x;
                        dst += step_y;
                    } else {
                        error += error_y;
                        dst += step_x;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        dst[0] = (uint8_t)color;
                        dst[1] = (uint8_t)(color >> 8);
                        dst[2] = (uint8_t)(color >> 16);
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            dst[0] = (uint8_t)color;
                            dst[1] = (uint8_t)(color >> 8);
                            dst[2] = (uint8_t)(color >> 16);
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            dst[0] = (uint8_t)color;
                            dst[1] = (uint8_t)(color >> 8);
                            dst[2] = (uint8_t)(color >> 16);
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            }
        }
        break;
    case 32:
        if (1) {
            uint32_t* dst = (uint32_t*)video_buffer->surface_data.pixels + (video_buffer->pitch / 4) * y1 + x1;
            *dst = color;

            if (dx < dy) {
                y = y1;
                while (y != y2) {
                    if (error > 0) {
                        error += error_x;
                        y++;
                        dst += video_buffer->pitch / 4 + step_x;
                    } else {
                        error += error_y;
                        y++;
                        dst += video_buffer->pitch / 4;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = color;
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = color;
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = color;
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            } else {
                if (line->y1 == line->y2) {
                    step_y = step_x;
                } else {
                    step_y = step_x + video_buffer->pitch / 4;
                }

                x = x1;
                while (x != x2) {
                    x += step_x;
                    if (error > 0) {
                        error += error_x;
                        dst += step_y;
                    } else {
                        error += error_y;
                        dst += step_x;
                    }

                    switch (style_info.style) {
                    case TIG_LINE_STYLE_SOLID:
                        *dst = (uint32_t)color;
                        break;
                    case TIG_LINE_STYLE_DOTTED:
                        if ((pattern & 1) != 0) {
                            *dst = (uint32_t)color;
                        }
                        ++pattern;
                        break;
                    case TIG_LINE_STYLE_DASHED:
                        if (pattern < 3) {
                            *dst = (uint32_t)color;
                        }
                        ++pattern;
                        if (pattern > 5) {
                            pattern = 0;
                        }
                        break;
                    }
                }
            }
        }
        break;
    }

    if (tig_video_buffer_unlock(video_buffer) != TIG_OK) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x520FB0
int sub_520FB0(TigVideoBuffer* video_buffer, unsigned int flags)
{
    TigVideoBufferData data;

    if ((flags & ~(TIG_VIDEO_BUFFER_BLIT_FLIP_X | TIG_VIDEO_BUFFER_BLIT_FLIP_Y)) == 0) {
        return TIG_OK;
    }

    if (!tig_video_3d_initialized) {
        return TIG_ERR_16;
    }

    if (tig_video_buffer_data(video_buffer, &data) != TIG_OK) {
        return TIG_ERR_16;
    }

    if ((data.flags & TIG_VIDEO_BUFFER_3D) == 0) {
        return TIG_ERR_16;
    }

    return TIG_OK;
}

// 0x521000
int tig_video_buffer_blit(TigVideoBufferBlitInfo* blit_info)
{
    bool stretched;
    float width_ratio;
    float height_ratio;
    TigRect bounds;
    TigRect blit_src_rect;
    TigRect blit_dst_rect;
    TigRect tmp_rect;
    int rc;

    if (blit_info->src_rect->width == blit_info->dst_rect->width
        && blit_info->src_rect->height == blit_info->dst_rect->height) {
        stretched = false;

        // NOTE: Original code does not initialize these values, but we have
        // to keep compiler happy.
        width_ratio = 1;
        height_ratio = 1;
    } else {
        stretched = true;
        width_ratio = (float)blit_info->src_rect->width / (float)blit_info->dst_rect->width;
        height_ratio = (float)blit_info->src_rect->height / (float)blit_info->dst_rect->height;
    }

    bounds.x = 0;
    bounds.y = 0;
    bounds.width = blit_info->src_video_buffer->frame.width;
    bounds.height = blit_info->src_video_buffer->frame.height;

    rc = tig_rect_intersection(blit_info->src_rect,
        &bounds,
        &blit_src_rect);
    if (rc != TIG_OK) {
        return TIG_OK;
    }

    tmp_rect = *blit_info->dst_rect;

    if (stretched) {
        tmp_rect.x += (int)((float)(blit_src_rect.x - blit_info->src_rect->x) / width_ratio);
        tmp_rect.y += (int)((float)(blit_src_rect.y - blit_info->src_rect->y) / height_ratio);
        tmp_rect.width -= (int)((float)(blit_info->src_rect->width - blit_src_rect.width) / width_ratio);
        tmp_rect.height -= (int)((float)(blit_info->src_rect->height - blit_src_rect.height) / height_ratio);
    } else {
        tmp_rect.x += blit_src_rect.x - blit_info->src_rect->x;
        tmp_rect.y += blit_src_rect.y - blit_info->src_rect->y;
        tmp_rect.width -= blit_info->src_rect->width - blit_src_rect.width;
        tmp_rect.height -= blit_info->src_rect->height - blit_src_rect.height;
    }

    bounds.x = 0;
    bounds.y = 0;
    bounds.width = blit_info->dst_video_buffer->frame.width;
    bounds.height = blit_info->dst_video_buffer->frame.height;

    rc = tig_rect_intersection(&tmp_rect,
        &bounds,
        &blit_dst_rect);
    if (rc != TIG_OK) {
        return TIG_OK;
    }

    if (stretched) {
        blit_src_rect.x += (int)((float)(blit_dst_rect.x - tmp_rect.x) / width_ratio);
        blit_src_rect.y += (int)((float)(blit_dst_rect.y - tmp_rect.y) / height_ratio);
        blit_src_rect.width -= (int)((float)(tmp_rect.width - blit_dst_rect.width) / width_ratio);
        blit_src_rect.height -= (int)((float)(tmp_rect.height - blit_dst_rect.height) / height_ratio);
    } else {
        blit_src_rect.x += blit_dst_rect.x - tmp_rect.x;
        blit_src_rect.y += blit_dst_rect.y - tmp_rect.y;
        blit_src_rect.width -= tmp_rect.width - blit_dst_rect.width;
        blit_src_rect.height -= tmp_rect.height - blit_dst_rect.height;
    }

    if ((blit_info->dst_video_buffer->flags & TIG_VIDEO_BUFFER_3D) != 0
        && (blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_TEXTURE) != 0) {
        DDSURFACEDESC2 ddsd;
        HRESULT hr;
        D3DCOLOR alpha[4];
        D3DCOLOR color[4];
        RECT tex_native_rect;
        D3DTLVERTEX verts[4];

        rc = tig_video_3d_set_viewport(blit_info->dst_video_buffer);
        if (rc != TIG_OK) {
            return rc;
        }

        ddsd.dwSize = sizeof(ddsd);
        hr = IDirectDrawSurface7_GetSurfaceDesc(blit_info->src_video_buffer->surface, &ddsd);
        if (FAILED(hr)) {
            tig_video_print_dd_result(hr);
            return TIG_ERR_16;
        }

        if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_ADD) != 0) {
            alpha[3] = 0xFF000000;
            alpha[2] = 0xFF000000;
            alpha[1] = 0xFF000000;
            alpha[0] = 0xFF000000;

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_SRCBLEND,
                D3DBLEND_ONE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_DESTBLEND,
                D3DBLEND_ONE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_ALPHABLENDENABLE,
                TRUE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        } else if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_MUL) != 0) {
            alpha[3] = 0xFF000000;
            alpha[2] = 0xFF000000;
            alpha[1] = 0xFF000000;
            alpha[0] = 0xFF000000;

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_SRCBLEND,
                D3DBLEND_ZERO);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_DESTBLEND,
                D3DBLEND_SRCCOLOR);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_ALPHABLENDENABLE,
                TRUE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        } else if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST) != 0
            || (blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_LERP) != 0) {
            if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_LERP) != 0) {
                alpha[0] = blit_info->alpha[0];
                alpha[1] = blit_info->alpha[1];
                alpha[2] = blit_info->alpha[2];
                alpha[3] = blit_info->alpha[3];
                sub_526530(&(blit_info->src_video_buffer->frame), &blit_src_rect, alpha);
                alpha[0] <<= 24;
                alpha[1] <<= 24;
                alpha[2] <<= 24;
                alpha[3] <<= 24;
            } else {
                alpha[3] = blit_info->alpha[0] << 24;
                alpha[2] = blit_info->alpha[0] << 24;
                alpha[1] = blit_info->alpha[0] << 24;
                alpha[0] = blit_info->alpha[0] << 24;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_SRCBLEND,
                D3DBLEND_SRCALPHA);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_DESTBLEND,
                D3DBLEND_INVSRCALPHA);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_ALPHABLENDENABLE,
                TRUE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_COLORVERTEX,
                TRUE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        } else {
            alpha[3] = 0xFF000000;
            alpha[2] = 0xFF000000;
            alpha[1] = 0xFF000000;
            alpha[0] = 0xFF000000;

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_ALPHABLENDENABLE,
                FALSE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_COLORVERTEX,
                FALSE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        }

        if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_CONST) != 0) {
            D3DCOLOR clr = (tig_color_get_red(blit_info->field_10) << 16)
                | (tig_color_get_green(blit_info->field_10) << 8)
                | tig_color_get_blue(blit_info->field_10);
            color[3] = clr;
            color[2] = clr;
            color[1] = clr;
            color[0] = clr;

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_COLORVERTEX,
                TRUE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        } else if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP) != 0) {
            color[0] = (tig_color_red_platform_to_rgb_table[(tig_color_red_mask & blit_info->field_10) >> tig_color_red_shift])
                | (tig_color_green_platform_to_rgb_table[(tig_color_green_mask & blit_info->field_10) >> tig_color_green_shift])
                | (tig_color_blue_platform_to_rgb_table[(tig_color_blue_mask & blit_info->field_10) >> tig_color_blue_shift]);

            color[1] = (tig_color_red_platform_to_rgb_table[(tig_color_red_mask & blit_info->field_14) >> tig_color_red_shift])
                | (tig_color_green_platform_to_rgb_table[(tig_color_green_mask & blit_info->field_14) >> tig_color_green_shift])
                | (tig_color_blue_platform_to_rgb_table[(tig_color_blue_mask & blit_info->field_14) >> tig_color_blue_shift]);

            color[2] = (tig_color_red_platform_to_rgb_table[(tig_color_red_mask & blit_info->field_18) >> tig_color_red_shift])
                | (tig_color_green_platform_to_rgb_table[(tig_color_green_mask & blit_info->field_18) >> tig_color_green_shift])
                | (tig_color_blue_platform_to_rgb_table[(tig_color_blue_mask & blit_info->field_18) >> tig_color_blue_shift]);

            color[3] = (tig_color_red_platform_to_rgb_table[(tig_color_red_mask & blit_info->field_1C) >> tig_color_red_shift])
                | (tig_color_green_platform_to_rgb_table[(tig_color_green_mask & blit_info->field_1C) >> tig_color_green_shift])
                | (tig_color_blue_platform_to_rgb_table[(tig_color_blue_mask & blit_info->field_1C) >> tig_color_blue_shift]);

            if (blit_info->field_20 != NULL) {
                sub_526690(blit_info->field_20, &blit_src_rect, color);
            } else {
                sub_526690(&(blit_info->src_video_buffer->frame), &blit_src_rect, color);
            }

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_COLORVERTEX,
                TRUE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        } else {
            color[3] = 0x00FFFFFF;
            color[2] = 0x00FFFFFF;
            color[1] = 0x00FFFFFF;
            color[0] = 0x00FFFFFF;

            hr = IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device,
                D3DRENDERSTATE_COLORVERTEX,
                FALSE);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        }

        tex_native_rect.left = blit_src_rect.x;
        tex_native_rect.top = blit_src_rect.y;
        tex_native_rect.right = blit_src_rect.x + blit_src_rect.width;
        tex_native_rect.bottom = blit_src_rect.y + blit_src_rect.height;

        if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_X) != 0) {
            tex_native_rect.left = blit_info->src_video_buffer->frame.width - tex_native_rect.left - 1;
            tex_native_rect.right = blit_info->src_video_buffer->frame.width - tex_native_rect.right - 1;
        }

        if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_Y) != 0) {
            tex_native_rect.top = blit_info->src_video_buffer->frame.height - tex_native_rect.top - 1;
            tex_native_rect.bottom = blit_info->src_video_buffer->frame.height - tex_native_rect.bottom - 1;
        }

        verts[0].sx = (D3DVALUE)blit_dst_rect.x;
        verts[0].sy = (D3DVALUE)blit_dst_rect.y;
        verts[0].sz = 0.5f;
        verts[0].rhw = 1.0f;
        verts[0].color = alpha[0] | color[0];
        verts[0].tu = ((D3DVALUE)tex_native_rect.left + 0.5f) / (D3DVALUE)blit_info->src_video_buffer->texture_width;
        verts[0].tv = ((D3DVALUE)tex_native_rect.top + 0.5f) / (D3DVALUE)blit_info->src_video_buffer->texture_height;

        verts[1].sx = (D3DVALUE)(blit_dst_rect.x + blit_dst_rect.width);
        verts[1].sy = verts[0].sy;
        verts[1].sz = 0.5f;
        verts[1].rhw = 1.0f;
        verts[1].color = alpha[1] | color[1];
        verts[1].tu = ((D3DVALUE)tex_native_rect.right + 0.5f) / (D3DVALUE)blit_info->src_video_buffer->texture_width;
        verts[1].tv = verts[0].tv;

        verts[2].sx = verts[1].sx;
        verts[2].sy = (D3DVALUE)(blit_dst_rect.y + blit_dst_rect.height);
        verts[2].sz = 0.5f;
        verts[2].rhw = 1.0f;
        verts[2].color = alpha[2] | color[2];
        verts[2].tu = verts[1].tu;
        verts[2].tv = ((D3DVALUE)tex_native_rect.bottom + 0.5f) / (D3DVALUE)blit_info->src_video_buffer->texture_height;

        verts[3].sx = verts[0].sx;
        verts[3].sy = verts[2].sy;
        verts[3].sz = 0.5f;
        verts[3].rhw = 1.0f;
        verts[3].color = alpha[3] | color[3];
        verts[3].tu = verts[0].tu;
        verts[3].tv = verts[2].tv;

        hr = IDirect3DDevice7_SetTexture(tig_video_state.d3d_device, 0, blit_info->src_video_buffer->surface);
        if (FAILED(hr)) {
            tig_video_print_dd_result(hr);
            return TIG_ERR_16;
        }

        hr = IDirect3DDevice7_DrawPrimitive(tig_video_state.d3d_device,
            D3DPT_TRIANGLEFAN,
            D3DFVF_TLVERTEX,
            verts,
            4,
            0);
        if (FAILED(hr)) {
            tig_video_print_dd_result(hr);
            return TIG_ERR_16;
        }

        return TIG_OK;
    }

    if ((blit_info->dst_video_buffer->flags & TIG_VIDEO_BUFFER_3D) != 0
        && (blit_info->flags & (TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP | TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST)) != 0) {
        // NOTE: What for?
        tig_debug_printf("$");
    }

    if ((blit_info->flags & (TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP | TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST)) != 0) {
        int bytes_per_pixel;
        uint8_t* src;
        int src_pitch;
        int src_step;
        uint8_t* dst;
        int dst_skip;
        int x;
        int y;

        rc = tig_video_buffer_lock(blit_info->src_video_buffer);
        if (rc != TIG_OK) {
            return rc;
        }

        rc = tig_video_buffer_lock(blit_info->dst_video_buffer);
        if (rc != TIG_OK) {
            tig_video_buffer_unlock(blit_info->src_video_buffer);
            return rc;
        }

        switch (tig_video_bpp) {
        case 8:
            bytes_per_pixel = 1;
            break;
        case 16:
            bytes_per_pixel = 2;
            break;
        case 24:
            bytes_per_pixel = 3;
            break;
        case 32:
            bytes_per_pixel = 4;
            break;
        default:
            __assume(0);
        }

        dst = (uint8_t*)blit_info->dst_video_buffer->surface_data.pixels
            + blit_info->dst_video_buffer->pitch * blit_dst_rect.y
            + bytes_per_pixel * blit_dst_rect.x;
        dst_skip = blit_info->dst_video_buffer->pitch
            - bytes_per_pixel * blit_dst_rect.width;

        src = (uint8_t*)blit_info->src_video_buffer->surface_data.pixels;
        src_pitch = blit_info->src_video_buffer->pitch;
        src_step = bytes_per_pixel;

        switch (blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_ANY) {
        case TIG_VIDEO_BUFFER_BLIT_FLIP_X:
            src = (uint8_t*)blit_info->src_video_buffer->surface_data.pixels
                + blit_info->src_video_buffer->pitch * blit_src_rect.y
                + bytes_per_pixel * (blit_info->src_video_buffer->surface_desc.dwWidth - blit_src_rect.x - 1);
            if (!stretched) {
                src_pitch += blit_src_rect.width * bytes_per_pixel;
            }
            src_step = -bytes_per_pixel;
            break;
        case TIG_VIDEO_BUFFER_BLIT_FLIP_Y:
            src = (uint8_t*)blit_info->src_video_buffer->surface_data.pixels
                + blit_info->src_video_buffer->pitch * (blit_src_rect.height - blit_src_rect.y - 1)
                + bytes_per_pixel * blit_src_rect.x;
            src_pitch = -src_pitch;
            if (!stretched) {
                src_pitch -= blit_src_rect.width * bytes_per_pixel;
            }
            src_step = bytes_per_pixel;
            break;
        case TIG_VIDEO_BUFFER_BLIT_FLIP_X | TIG_VIDEO_BUFFER_BLIT_FLIP_Y:
            src = (uint8_t*)blit_info->src_video_buffer->surface_data.pixels
                + blit_info->src_video_buffer->pitch * (blit_src_rect.height - blit_src_rect.y - 1)
                + bytes_per_pixel * (blit_info->src_video_buffer->surface_desc.dwWidth - blit_src_rect.x - 1);
            src_pitch = -src_pitch;
            if (!stretched) {
                src_pitch += blit_src_rect.width * bytes_per_pixel;
            }
            src_step = -bytes_per_pixel;
            break;
        default:
            src = (uint8_t*)blit_info->src_video_buffer->surface_data.pixels
                + blit_info->src_video_buffer->pitch * blit_src_rect.y
                + bytes_per_pixel * blit_src_rect.x;
            if (!stretched) {
                src_pitch -= blit_src_rect.width * bytes_per_pixel;
            }
            src_step = bytes_per_pixel;
            break;
        }

        if (stretched) {
            uint8_t* src_base = src;
            double height_err = 0.5;
            double width_err;

            if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST) != 0) {
                switch (tig_video_bpp) {
                case 8:
                    break;
                case 16:
                    // TODO: Incomplete.
                    break;
                case 24:
                    // TODO: Incomplete.
                    break;
                case 32:
                    for (y = 0; y < blit_dst_rect.height; ++y) {
                        width_err = 0.5;
                        for (x = 0; x < blit_dst_rect.width; ++x) {
                            if ((blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) == 0
                                || *(uint32_t*)src != blit_info->src_video_buffer->color_key) {
                                *(uint32_t*)dst = (uint32_t)tig_color_blend_alpha(*(uint32_t*)src,
                                    *(uint32_t*)dst,
                                    blit_info->alpha[0]);
                            }

                            width_err += width_ratio;
                            src += src_step;

                            while (width_err > 1.0) {
                                width_err -= 1.0;
                                src += src_step;
                            }

                            dst += 4;
                        }

                        height_err += height_ratio;

                        while (height_err > 1.0) {
                            height_err -= 1.0;
                            src_base += src_pitch;
                        }
                        src = src_base;

                        dst += 4 * dst_skip;
                    }
                    break;
                }
            } else {
                tig_video_buffer_unlock(blit_info->dst_video_buffer);
                tig_video_buffer_unlock(blit_info->src_video_buffer);
                return TIG_ERR_12;
            }
        } else {
            if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_COLOR_LERP) != 0) {
                // 0x5221A2
                int r1 = tig_color_get_red(blit_info->field_10);
                int g1 = tig_color_get_green(blit_info->field_10);
                int b1 = tig_color_get_blue(blit_info->field_10);

                int r2 = tig_color_get_red(blit_info->field_14);
                int g2 = tig_color_get_green(blit_info->field_14);
                int b2 = tig_color_get_blue(blit_info->field_14);

                int r3 = tig_color_get_red(blit_info->field_18);
                int g3 = tig_color_get_green(blit_info->field_18);
                int b3 = tig_color_get_blue(blit_info->field_18);

                int r4 = tig_color_get_red(blit_info->field_1C);
                int g4 = tig_color_get_green(blit_info->field_1C);
                int b4 = tig_color_get_blue(blit_info->field_1C);

                float div = (float)blit_info->field_20->height;
                float mult = (float)(blit_src_rect.y - blit_info->field_20->y);

                float r_start = (float)(r4 - r1) / div * mult + (float)r1;
                float r_end = (float)(r3 - r2) / div * mult + (float)r2;

                float g_start = (float)(g4 - g1) / div * mult + (float)g1;
                float g_end = (float)(g3 - g2) / div * mult + (float)g2;

                float b_start = (float)(b4 - b1) / div * mult + (float)b1;
                float b_end = (float)(b3 - b2) / div * mult + (float)b2;

                float r_step;
                float r_value;

                float g_step;
                float g_value;

                float b_step;
                float b_value;

                switch (tig_video_bpp) {
                case 8:
                    break;
                case 16:
                    // TODO: Incomplete.
                    break;
                case 24:
                    // TODO: Incomplete.
                    break;
                case 32:
                    for (y = 0; y < blit_dst_rect.height; ++y) {
                        r_step = (r_end - r_start) / (float)blit_info->field_20->width;
                        r_value = r_step * (blit_src_rect.x - blit_info->field_20->x) + r_start;

                        g_step = (g_end - g_start) / (float)blit_info->field_20->width;
                        g_value = r_step * (blit_src_rect.x - blit_info->field_20->x) + g_start;

                        b_step = (b_end - b_start) / (float)blit_info->field_20->width;
                        b_value = b_step * (blit_src_rect.x - blit_info->field_20->x) + b_start;

                        for (x = 0; x < blit_dst_rect.width; ++x) {
                            if ((blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) == 0
                                || *(uint32_t*)src != blit_info->src_video_buffer->color_key) {
                                uint32_t tint_color = (tig_color_red_rgb_to_platform_table[(uint8_t)r_value] << tig_color_red_shift)
                                    | (tig_color_green_rgb_to_platform_table[(uint8_t)g_value] << tig_color_green_shift)
                                    | (tig_color_blue_rgb_to_platform_table[(uint8_t)b_value] << tig_color_blue_shift);

                                uint8_t tint_r = (uint8_t)((tint_color & tig_color_red_mask) >> tig_color_red_shift);
                                uint8_t tint_g = (uint8_t)((tint_color & tig_color_green_mask) >> tig_color_green_shift);
                                uint8_t tint_b = (uint8_t)((tint_color & tig_color_blue_mask) >> tig_color_blue_shift);

                                uint8_t src_r = (uint8_t)((*(uint32_t*)src & tig_color_red_mask) >> tig_color_red_shift);
                                uint8_t src_g = (uint8_t)((*(uint32_t*)src & tig_color_green_mask) >> tig_color_green_shift);
                                uint8_t src_b = (uint8_t)((*(uint32_t*)src & tig_color_blue_mask) >> tig_color_blue_mask);

                                *(uint32_t*)dst = (tig_color_red_mult_table[(tig_color_red_range + 1) * (src_r + tint_r)] << tig_color_red_shift)
                                    | (tig_color_green_mult_table[(tig_color_green_range + 1) * (src_g + tint_g)] << tig_color_green_shift)
                                    | (tig_color_blue_mult_table[(tig_color_blue_range + 1) * (src_b + tint_b)] << tig_color_blue_shift);
                            }

                            r_value += r_step;
                            g_value += g_step;
                            b_value += b_step;

                            src += src_step;
                            dst += 4;
                        }
                    }
                    break;
                }
            } else if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_BLEND_ALPHA_CONST) != 0) {
                // 0x522A40
                switch (tig_video_bpp) {
                case 8:
                    break;
                case 16:
                    // TODO: Incomplete.
                    break;
                case 24:
                    // TODO: Incomplete.
                    break;
                case 32:
                    for (y = 0; y < blit_dst_rect.height; y++) {
                        for (x = 0; x < blit_dst_rect.width; x++) {
                            if ((blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) == 0
                                || *(uint32_t*)src != blit_info->src_video_buffer->color_key) {
                                *(uint32_t*)dst = (uint32_t)tig_color_blend_alpha(*(uint32_t*)src,
                                    *(uint32_t*)dst,
                                    blit_info->alpha[0]);
                            }

                            src += src_step;
                            dst += 4;
                        }

                        src += src_pitch;
                        dst += dst_skip;
                    }
                    break;
                }
            } else {
                tig_video_buffer_unlock(blit_info->dst_video_buffer);
                tig_video_buffer_unlock(blit_info->src_video_buffer);
                return TIG_ERR_12;
            }
        }

        tig_video_buffer_unlock(blit_info->dst_video_buffer);
        tig_video_buffer_unlock(blit_info->src_video_buffer);
    } else {
        RECT src_native_rect;
        RECT dst_native_rect;
        HRESULT hr;

        if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_X) != 0) {
            blit_src_rect.x = blit_info->src_video_buffer->frame.width - blit_src_rect.width - blit_src_rect.x;
        }

        if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_Y) != 0) {
            blit_src_rect.y = blit_info->src_video_buffer->frame.height - blit_src_rect.height - blit_src_rect.y;
        }

        src_native_rect.left = blit_src_rect.x;
        src_native_rect.top = blit_src_rect.y;
        src_native_rect.right = blit_src_rect.x + blit_src_rect.width;
        src_native_rect.bottom = blit_src_rect.y + blit_src_rect.height;

        dst_native_rect.left = blit_dst_rect.x;
        dst_native_rect.top = blit_dst_rect.y;
        dst_native_rect.right = blit_dst_rect.x + blit_dst_rect.width;
        dst_native_rect.bottom = blit_dst_rect.y + blit_dst_rect.height;

        if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_ANY) == 0
            && !stretched
            && (blit_info->dst_video_buffer->flags & TIG_VIDEO_BUFFER_SYSTEM_MEMORY) == 0) {
            hr = IDirectDrawSurface7_BltFast(blit_info->dst_video_buffer->surface,
                blit_dst_rect.x,
                blit_dst_rect.y,
                blit_info->src_video_buffer->surface,
                &src_native_rect,
                (blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY)
                    ? DDBLTFAST_SRCCOLORKEY | DDBLTFAST_WAIT
                    : DDBLTFAST_WAIT);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        } else {
            LPDDBLTFX pfx;
            DDBLTFX fx;
            DWORD flags = DDBLT_WAIT;

            if ((blit_info->src_video_buffer->flags & TIG_VIDEO_BUFFER_COLOR_KEY) != 0) {
                flags |= DDBLT_KEYSRC;
            }

            if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_ANY) != 0) {
                flags |= DDBLT_DDFX;

                fx.dwSize = sizeof(fx);
                fx.dwDDFX = 0;
                pfx = &fx;

                if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_X) != 0) {
                    fx.dwDDFX |= DDBLTFX_MIRRORLEFTRIGHT;
                }

                if ((blit_info->flags & TIG_VIDEO_BUFFER_BLIT_FLIP_Y) != 0) {
                    fx.dwDDFX |= DDBLTFX_MIRRORUPDOWN;
                }
            } else {
                pfx = NULL;
            }

            hr = IDirectDrawSurface7_Blt(blit_info->dst_video_buffer->surface,
                &dst_native_rect,
                blit_info->src_video_buffer->surface,
                &src_native_rect,
                flags,
                pfx);
            if (FAILED(hr)) {
                tig_video_print_dd_result(hr);
                return TIG_ERR_16;
            }
        }
    }

    return TIG_OK;
}

// 0x522F30
int tig_video_buffer_get_pixel_color(TigVideoBuffer* video_buffer, int x, int y, unsigned int* color)
{
    int index;
    int rc;

    if (x < video_buffer->frame.x
        || y < video_buffer->frame.y
        || x >= video_buffer->frame.x + video_buffer->frame.width
        || y >= video_buffer->frame.y + video_buffer->frame.height) {
        return TIG_ERR_12;
    }

    rc = tig_video_buffer_lock(video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    switch (tig_video_bpp) {
    case 8:
        index = y * video_buffer->pitch + x;
        *color = video_buffer->surface_data.p8[index];
        break;
    case 16:
        index = y * video_buffer->pitch + x;
        *color = video_buffer->surface_data.p16[index];
        break;
    case 24:
        index = (y * video_buffer->pitch / 3 + x) * 3;
        *color = video_buffer->surface_data.p8[index] | (video_buffer->surface_data.p8[index + 1] << 8) | (video_buffer->surface_data.p8[index + 2] << 16);
        break;
    default:
        index = y * video_buffer->pitch + x;
        *color = video_buffer->surface_data.p32[index];
        break;
    }

    tig_video_buffer_unlock(video_buffer);

    return TIG_OK;
}

// 0x523050
int sub_523050(TigVideoBuffer* video_buffer, int x, int y, HWND wnd, TigRect* rect)
{
    HDC src;
    HDC dst;

    IDirectDrawSurface7_GetDC(video_buffer->surface, &src);
    dst = GetDC(wnd);
    BitBlt(dst,
        rect->x,
        rect->y,
        rect->width,
        rect->height,
        src,
        x,
        y,
        SRCCOPY);
    ReleaseDC(wnd, dst);
    IDirectDrawSurface7_ReleaseDC(video_buffer->surface, src);

    return TIG_OK;
}

// 0x5230C0
int sub_5230C0(TigVideoBuffer* video_buffer, int x, int y, HDC dst, TigRect* rect)
{
    HDC src;

    IDirectDrawSurface7_GetDC(video_buffer->surface, &src);
    BitBlt(dst,
        rect->x,
        rect->y,
        rect->width,
        rect->height,
        src,
        x,
        y,
        SRCCOPY);
    IDirectDrawSurface7_ReleaseDC(video_buffer->surface, src);

    return TIG_OK;
}

// 0x523120
int tig_video_buffer_tint(TigVideoBuffer* video_buffer, TigRect* rect, tig_color_t tint_color, TigVideoBufferTintMode mode)
{
    int rc;
    TigRect frame;
    int x;
    int y;

    if (mode >= TIG_VIDEO_BUFFER_TINT_MODE_COUNT) {
        return TIG_ERR_12;
    }

    if (tint_color == tig_color_make(0, 0, 0)
        && mode != TIG_VIDEO_BUFFER_TINT_MODE_GRAYSCALE) {
        return TIG_OK;
    }

    rc = tig_rect_intersection(rect, &(video_buffer->frame), &frame);
    if (rc != TIG_OK) {
        return rc;
    }

    rc = tig_video_buffer_lock(video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    for (y = 0; y < frame.height; ++y) {
        switch (tig_video_bpp) {
        case 8:
            break;
        case 16:
            if (1) {
                uint16_t* dst = (uint16_t*)video_buffer->surface_data.pixels + (video_buffer->pitch / 2) * (y + frame.y) + frame.x;

                switch (mode) {
                case TIG_VIDEO_BUFFER_TINT_MODE_ADD:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = (uint16_t)tig_color_add(*dst, (uint16_t)tint_color);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_SUB:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = (uint16_t)tig_color_sub(*dst, (uint16_t)tint_color);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_MUL:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = (uint16_t)tig_color_mul(*dst, (uint16_t)tint_color);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_GRAYSCALE:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = (uint16_t)tig_color_rgb_to_grayscale(*dst);
                    }
                    break;
                }
            }
            break;
        case 24:
            if (1) {
                // TODO: Check.
                uint8_t* dst = (uint8_t*)video_buffer->surface_data.pixels + (video_buffer->pitch / 3) * (y + frame.y) + frame.x;
                uint32_t src_color;
                uint32_t dst_color;

                switch (mode) {
                case TIG_VIDEO_BUFFER_TINT_MODE_ADD:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = dst[0] | (dst[1] << 8) | (dst[2] << 16);
                        dst_color = tig_color_add(src_color, tint_color);

                        dst[0] = (uint8_t)dst_color;
                        dst[1] = (uint8_t)(dst_color >> 8);
                        dst[2] = (uint8_t)(dst_color >> 16);
                        dst += 3;
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_SUB:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = dst[0] | (dst[1] << 8) | (dst[2] << 16);
                        dst_color = tig_color_sub(src_color, tint_color);

                        dst[0] = (uint8_t)dst_color;
                        dst[1] = (uint8_t)(dst_color >> 8);
                        dst[2] = (uint8_t)(dst_color >> 16);
                        dst += 3;
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_MUL:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = dst[0] | (dst[1] << 8) | (dst[2] << 16);
                        dst_color = tig_color_mul(src_color, tint_color);

                        dst[0] = (uint8_t)dst_color;
                        dst[1] = (uint8_t)(dst_color >> 8);
                        dst[2] = (uint8_t)(dst_color >> 16);
                        dst += 3;
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_GRAYSCALE:
                    for (x = 0; x < frame.width; ++x) {
                        src_color = dst[0] | (dst[1] << 8) | (dst[2] << 16);
                        dst_color = tig_color_rgb_to_grayscale(src_color);

                        dst[0] = (uint8_t)dst_color;
                        dst[1] = (uint8_t)(dst_color >> 8);
                        dst[2] = (uint8_t)(dst_color >> 16);
                        dst += 3;
                    }
                    break;
                }
            }
            break;
        case 32:
            if (1) {
                uint32_t* dst = (uint32_t*)video_buffer->surface_data.pixels + (video_buffer->pitch / 4) * (y + frame.y) + frame.x;

                switch (mode) {
                case TIG_VIDEO_BUFFER_TINT_MODE_ADD:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = tig_color_add(tint_color, *dst);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_SUB:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = tig_color_sub(tint_color, *dst);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_MUL:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = tig_color_mul(tint_color, *dst);
                    }
                    break;
                case TIG_VIDEO_BUFFER_TINT_MODE_GRAYSCALE:
                    for (x = 0; x < frame.width; ++x) {
                        *dst++ = tig_color_rgb_to_grayscale(*dst);
                    }
                    break;
                }
            }
            break;
        }
    }

    tig_video_buffer_unlock(video_buffer);
    return TIG_OK;
}

// 0x523930
int tig_video_buffer_save_to_bmp(TigVideoBufferSaveToBmpInfo* save_info)
{
    int rc;
    TigVideoBufferData video_buffer_data;
    TigRect rect;

    rc = tig_video_buffer_lock(save_info->video_buffer);
    if (rc != TIG_OK) {
        return rc;
    }

    rc = tig_video_buffer_data(save_info->video_buffer, &video_buffer_data);
    if (rc != TIG_OK) {
        return rc;
    }

    if (save_info->rect != NULL) {
        rect = *save_info->rect;
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.width = video_buffer_data.width;
        rect.height = video_buffer_data.height;
    }

    rc = tig_video_buffer_data_to_bmp(&video_buffer_data,
        &rect,
        save_info->path,
        (save_info->flags & 1) != 0);

    tig_video_buffer_unlock(save_info->video_buffer);

    return rc;
}

// 0x5239D0
int tig_video_buffer_load_from_bmp(const char* filename, TigVideoBuffer** video_buffer_ptr, unsigned int flags)
{
    TigFile* stream;
    BITMAPFILEHEADER file_hdr;
    BITMAPINFOHEADER info_hdr;
    RGBQUAD quads[256];
    TigVideoBufferCreateInfo vb_create_info;
    TigVideoBufferData vb_data;
    int rc;
    int v1 = 1;
    int padding;
    int x;
    int y;
    void* dst;
    void* dst_base;
    int dst_pitch;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t clr;

    if (tig_video_bpp == 8) {
        return TIG_ERR_16;
    }

    stream = tig_file_fopen(filename, "rb");
    if (stream == NULL) {
        return TIG_ERR_13;
    }

    if (tig_file_fread(&file_hdr, sizeof(file_hdr), 1, stream) != 1) {
        tig_file_fclose(stream);
        return TIG_ERR_13;
    }

    if (tig_file_fread(&info_hdr, sizeof(info_hdr), 1, stream) != 1) {
        tig_file_fclose(stream);
        return TIG_ERR_13;
    }

    if (info_hdr.biCompression != 0
        || (info_hdr.biBitCount != 8 && info_hdr.biBitCount != 24)) {
        // FIXME: Leaking `stream`.
        return TIG_ERR_13;
    }

    if (info_hdr.biHeight < 0) {
        info_hdr.biHeight = -info_hdr.biHeight;
        v1 = -1;
    }

    if ((flags & 0x1) != 0) {
        vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
        vb_create_info.width = info_hdr.biWidth;
        vb_create_info.height = info_hdr.biHeight;
        vb_create_info.color_key = 0;
        vb_create_info.background_color = 0;

        rc = tig_video_buffer_create(&vb_create_info, video_buffer_ptr);
        if (rc != TIG_OK) {
            tig_file_fclose(stream);
            return rc;
        }
    }

    rc = tig_video_buffer_lock(*video_buffer_ptr);
    if (rc != TIG_OK) {
        tig_video_buffer_unlock(*video_buffer_ptr);

        if ((flags & 0x1) != 0) {
            tig_video_buffer_destroy(*video_buffer_ptr);
        }

        tig_file_fclose(stream);
        return rc;
    }

    rc = tig_video_buffer_data(*video_buffer_ptr, &vb_data);
    if (rc != TIG_OK) {
        tig_video_buffer_unlock(*video_buffer_ptr);

        if ((flags & 0x1) != 0) {
            tig_video_buffer_destroy(*video_buffer_ptr);
        }

        tig_file_fclose(stream);
        return rc;
    }

    if (vb_data.width < info_hdr.biWidth || vb_data.height < info_hdr.biHeight) {
        tig_video_buffer_unlock(*video_buffer_ptr);

        if ((flags & 0x1) != 0) {
            tig_video_buffer_destroy(*video_buffer_ptr);
        }

        tig_file_fclose(stream);
        return TIG_ERR_12;
    }

    if (info_hdr.biBitCount == 8) {
        if (info_hdr.biClrUsed == 0) {
            info_hdr.biClrUsed = 256;
        }

        if (tig_file_fread(quads, sizeof(RGBQUAD), info_hdr.biClrUsed, stream) != (int)info_hdr.biClrUsed) {
            tig_video_buffer_unlock(*video_buffer_ptr);

            if ((flags & 0x1) != 0) {
                tig_video_buffer_destroy(*video_buffer_ptr);
            }

            tig_file_fclose(stream);
            return TIG_ERR_13;
        }
        padding = info_hdr.biWidth % 4;
    } else {
        padding = 3 * info_hdr.biWidth % 4;
    }

    if (padding != 0) {
        padding = 4 - padding;
    }

    if (v1 == -1) {
        dst = vb_data.surface_data.pixels;
        dst_pitch = vb_data.pitch;
    } else {
        dst = (uint8_t*)vb_data.surface_data.pixels + vb_data.pitch * ((vb_data.height - info_hdr.biHeight) / 2 + info_hdr.biHeight - 1);
        dst_pitch = -vb_data.pitch;
    }

    for (y = 0; y < info_hdr.biHeight; y++) {
        dst_base = dst;

        for (x = 0; x < info_hdr.biWidth; x++) {
            switch (info_hdr.biBitCount) {
            case 8:
                if (tig_file_fread(&clr, sizeof(clr), 1, stream) != 1) {
                    tig_video_buffer_unlock(*video_buffer_ptr);

                    if ((flags & 0x1) != 0) {
                        tig_video_buffer_destroy(*video_buffer_ptr);
                    }

                    // FIXME: Leaking `stream`.
                    return TIG_ERR_13;
                }

                r = quads[clr].rgbRed;
                g = quads[clr].rgbGreen;
                b = quads[clr].rgbBlue;
                break;
            case 16:
                // NOTE: Unreachable (see validation in the beginning).
                tig_video_buffer_unlock(*video_buffer_ptr);

                if ((flags & 0x1) != 0) {
                    tig_video_buffer_destroy(*video_buffer_ptr);
                }

                // FIXME: Leaking `stream`.
                return TIG_ERR_13;
            case 24:
                if (tig_file_fread(&b, sizeof(b), 1, stream) != 1
                    || tig_file_fread(&g, sizeof(g), 1, stream) != 1
                    || tig_file_fread(&r, sizeof(r), 1, stream) != 1) {
                    tig_video_buffer_unlock(*video_buffer_ptr);

                    if ((flags & 0x1) != 0) {
                        tig_video_buffer_destroy(*video_buffer_ptr);
                    }

                    // FIXME: Leaking `stream`.
                    return TIG_ERR_13;
                }
                break;
            default:
                __assume(0);
            }

            switch (tig_video_bpp) {
            case 16:
                *(uint16_t*)dst = (uint16_t)tig_color_make(r, g, b);
                dst = (uint8_t*)dst + 2;
                break;
            case 24:
                ((uint8_t*)dst)[0] = r;
                ((uint8_t*)dst)[1] = g;
                ((uint8_t*)dst)[2] = b;
                dst = (uint8_t*)dst + 3;
                break;
            case 32:
                *(uint32_t*)dst = (uint32_t)tig_color_make(r, g, b);
                dst = (uint8_t*)dst + 4;
                break;
            }
        }

        for (x = 0; x < padding; x++) {
            if (tig_file_fread(&b, 1, 1, stream) != 1) {
                tig_video_buffer_unlock(*video_buffer_ptr);

                if ((flags & 0x1) != 0) {
                    tig_video_buffer_destroy(*video_buffer_ptr);
                }

                // FIXME: Leaking `stream`.
                return TIG_ERR_13;
            }
        }

        dst = (uint8_t*)dst_base + dst_pitch;
    }

    tig_video_buffer_unlock(*video_buffer_ptr);

    tig_file_fclose(stream);
    return TIG_OK;
}

// 0x524080
bool tig_video_platform_window_init(TigInitInfo* init_info)
{
    WNDCLASSA window_class;
    RECT window_rect;
    const char* name;
    int screen_width;
    int screen_height;
    HMENU menu;
    DWORD style;
    DWORD ex_style;

    tig_video_state.instance = init_info->instance;

    window_class.style = CS_DBLCLKS;
    window_class.lpfnWndProc = tig_message_wnd_proc;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = init_info->instance;
    window_class.hIcon = LoadIconA(init_info->instance, "TIGIcon");
    window_class.hCursor = LoadCursorA(NULL, MAKEINTRESOURCEA(0x7F00));
    window_class.hbrBackground = (HBRUSH)GetStockObject(4);
    window_class.lpszMenuName = NULL;
    window_class.lpszClassName = "TIGClass";

    if (RegisterClassA(&window_class) == 0) {
        return false;
    }

    screen_width = GetSystemMetrics(SM_CXFULLSCREEN);
    screen_height = GetSystemMetrics(SM_CYFULLSCREEN);

    if ((init_info->flags & TIG_INITIALIZE_WINDOWED) != 0) {
        if ((init_info->flags & TIG_INITIALIZE_POSITIONED) != 0) {
            window_rect.left = init_info->x;
            window_rect.top = init_info->y;
            window_rect.right = window_rect.left + init_info->width;
            window_rect.bottom = window_rect.top + init_info->height;
        } else {
            window_rect.left = (screen_width - init_info->width) / 2;
            window_rect.top = (screen_height - init_info->height) / 2;
            window_rect.right = window_rect.left + init_info->width;
            window_rect.bottom = window_rect.top + init_info->height;
        }

        menu = LoadMenuA(init_info->instance, "TIGMenu");
        style = WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_GROUP;
        ex_style = 0;

        if ((init_info->flags & TIG_INITIALIZE_0x0080) == 0) {
            LONG offset_x = (window_rect.right - window_rect.left - init_info->width) / 2;
            LONG offset_y = (window_rect.bottom - window_rect.top - init_info->height) / 2;

            AdjustWindowRectEx(&window_rect,
                style,
                menu != NULL,
                ex_style);

            window_rect.left += offset_x;
            window_rect.top += offset_y;
            window_rect.right += offset_x;
            window_rect.bottom += offset_y;
        }
    } else {
        window_rect.left = 0;
        window_rect.top = 0;
        window_rect.right = screen_width;
        window_rect.bottom = screen_height;
        menu = NULL;
        style = WS_POPUP;
        ex_style = WS_EX_APPWINDOW | WS_EX_TOPMOST;
        tig_video_client_rect = window_rect;
    }

    style |= WS_VISIBLE;

    stru_610388.x = 0;
    stru_610388.y = 0;
    stru_610388.width = init_info->width;
    stru_610388.height = init_info->height;

    name = (init_info->flags & TIG_INITIALIZE_SET_WINDOW_NAME) != 0
        ? init_info->window_name
        : tig_get_executable(true);

    tig_video_state.wnd = CreateWindowExA(
        ex_style,
        "TIGClass",
        name,
        style,
        window_rect.left,
        window_rect.top,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        NULL,
        menu,
        init_info->instance,
        NULL);
    if (tig_video_state.wnd == NULL) {
        return false;
    }

    return true;
}

// 0x5242F0
void tig_video_platform_window_exit()
{
    // NOTE: For unknown reasons there is no `DestroyWindow` call in the
    // original code. It makes integration testing somewhat difficult.
    DestroyWindow(tig_video_state.wnd);

    tig_video_state.wnd = NULL;
}

// 0x524300
bool tig_video_ddraw_init(TigInitInfo* init_info)
{
    HRESULT hr = DirectDrawCreateEx(NULL, (LPVOID*)&(tig_video_state.ddraw), &IID_IDirectDraw7, NULL);
    if (FAILED(hr)) {
        return false;
    }

    DDCAPS ddcaps = { 0 };
    ddcaps.dwSize = sizeof(ddcaps);

    if ((init_info->flags & TIG_INITIALIZE_ANY_3D) != 0) {
        tig_debug_printf("3D: Checking DirectDraw object caps for 3D support...");

        IDirectDraw7_GetCaps(tig_video_state.ddraw, &ddcaps, NULL);

        if (ddcaps.dwCaps != 0) {
            tig_debug_printf("supported.\n");
        } else {
            init_info->flags &= ~TIG_INITIALIZE_3D_HARDWARE_DEVICE;
            tig_debug_printf("unsupported.\n");
        }
    }

    if ((init_info->flags & TIG_INITIALIZE_WINDOWED) != 0) {
        if (!tig_video_ddraw_init_windowed(init_info)) {
            IDirectDraw7_Release(tig_video_state.ddraw);
            return false;
        }
    } else {
        if (!tig_video_ddraw_init_fullscreen(init_info)) {
            IDirectDraw7_Release(tig_video_state.ddraw);
            return false;
        }
    }

    if (!sub_524830()) {
        IDirectDraw7_Release(tig_video_state.ddraw);
        return false;
    }

    if ((init_info->flags & TIG_INITIALIZE_ANY_3D) != 0) {
        if (!tig_video_d3d_init(init_info)) {
            tig_debug_printf("Error initializing Direct3D.  3D support disabled.\n");
            init_info->flags &= ~TIG_INITIALIZE_ANY_3D;
        }
    }

    if (init_info->bpp == 8) {
        if (!tig_video_ddraw_palette_create(tig_video_state.ddraw)) {
            IDirectDraw7_Release(tig_video_state.ddraw);
            return false;
        }

        tig_video_surface_fill(tig_video_state.primary_surface, NULL, 0);

        if (tig_video_double_buffered) {
            tig_video_surface_fill(tig_video_state.offscreen_surface, NULL, 0);
        }
    }

    return true;
}

// 0x524480
bool tig_video_ddraw_init_windowed(TigInitInfo* init_info)
{
    HRESULT hr;

    hr = IDirectDraw7_SetCooperativeLevel(tig_video_state.ddraw, tig_video_state.wnd, DDSCL_NORMAL);
    if (FAILED(hr)) {
        return false;
    }

    DDSURFACEDESC2 ddsd;
    ddsd.dwSize = sizeof(ddsd);
    hr = IDirectDraw7_GetDisplayMode(tig_video_state.ddraw, &ddsd);
    if (FAILED(hr)) {
        return false;
    }

    if ((ddsd.ddpfPixelFormat.dwFlags & DDPF_RGB) == 0
        || (ddsd.ddpfPixelFormat.dwRGBBitCount != 16
            && ddsd.ddpfPixelFormat.dwRGBBitCount != 24
            && ddsd.ddpfPixelFormat.dwRGBBitCount != 32)) {
        MessageBoxA(tig_video_state.wnd,
            "Sorry, this program requires the Windows Desktop to be in a 16 or 24 or 32-bit RGB mode.",
            "Invalid Desktop Settings",
            MB_ICONHAND);
    }

    init_info->bpp = ddsd.ddpfPixelFormat.dwRGBBitCount;

    unsigned int caps = DDSCAPS_PRIMARYSURFACE;
    if ((init_info->flags & TIG_INITIALIZE_ANY_3D) != 0) {
        caps |= DDSCAPS_3DDEVICE;
    }

    if (!tig_video_surface_create(tig_video_state.ddraw, init_info->width, init_info->height, caps, &(tig_video_state.primary_surface))) {
        return false;
    }

    tig_video_double_buffered = (init_info->flags & TIG_INITIALIZE_DOUBLE_BUFFER) != 0;
    if (tig_video_double_buffered) {
        if (!tig_video_surface_create(tig_video_state.ddraw, init_info->width, init_info->height, DDSCAPS_OFFSCREENPLAIN, &(tig_video_state.offscreen_surface))) {
            IDirectDrawSurface7_Release(tig_video_state.primary_surface);
            return false;
        }

        tig_video_state.main_surface = tig_video_state.offscreen_surface;
    } else {
        tig_video_state.main_surface = tig_video_state.primary_surface;
    }

    LPDIRECTDRAWCLIPPER clipper;
    hr = DirectDrawCreateClipper(0, &clipper, NULL);
    if (FAILED(hr)) {
        IDirectDrawSurface7_Release(tig_video_state.offscreen_surface);
        IDirectDrawSurface7_Release(tig_video_state.primary_surface);
        return false;
    }

    hr = IDirectDrawClipper_SetHWnd(clipper, 0, tig_video_state.wnd);
    if (FAILED(hr)) {
        IDirectDrawClipper_Release(clipper);
        IDirectDrawSurface7_Release(tig_video_state.offscreen_surface);
        IDirectDrawSurface7_Release(tig_video_state.primary_surface);
        return false;
    }

    hr = IDirectDrawSurface7_SetClipper(tig_video_state.primary_surface, clipper);
    if (FAILED(hr)) {
        IDirectDrawClipper_Release(clipper);
        IDirectDrawSurface7_Release(tig_video_state.offscreen_surface);
        IDirectDrawSurface7_Release(tig_video_state.primary_surface);
        return false;
    }

    IDirectDrawClipper_Release(clipper);

    tig_video_fullscreen = false;

    return true;
}

// 0x524640
bool tig_video_ddraw_init_fullscreen(TigInitInfo* init_info)
{
    DDCAPS ddcaps;
    HRESULT hr;

    hr = IDirectDraw7_SetCooperativeLevel(tig_video_state.ddraw, tig_video_state.wnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
    if (FAILED(hr)) {
        return false;
    }

    hr = IDirectDraw7_SetDisplayMode(tig_video_state.ddraw, init_info->width, init_info->height, init_info->bpp, 0, 0);
    if (FAILED(hr)) {
        return false;
    }

    tig_video_double_buffered = (init_info->flags & TIG_INITIALIZE_DOUBLE_BUFFER) != 0;

    unsigned int caps = DDSCAPS_PRIMARYSURFACE;
    if (tig_video_double_buffered) {
        caps |= DDSCAPS_FLIP | DDSCAPS_COMPLEX;
    }
    if ((init_info->flags & TIG_INITIALIZE_ANY_3D) != 0) {
        caps |= DDSCAPS_3DDEVICE;
    }

    if (!tig_video_surface_create(tig_video_state.ddraw, init_info->width, init_info->height, caps, &(tig_video_state.primary_surface))) {
        return false;
    }

    if (tig_video_double_buffered) {
        tig_video_state.main_surface = tig_video_state.offscreen_surface;
    } else {
        tig_video_state.main_surface = tig_video_state.primary_surface;
    }

    tig_video_fullscreen = true;

    memset(&ddcaps, 0, sizeof(ddcaps));
    ddcaps.dwSize = sizeof(ddcaps);

    hr = IDirectDraw7_GetCaps(tig_video_state.ddraw, &ddcaps, NULL);
    if (FAILED(hr)) {
        tig_debug_printf("Error retrieving caps from DirectDraw object.\n");
        return false;
    }

    tig_video_state.have_gamma_control = false;

    if ((ddcaps.dwCaps2 & DDCAPS2_PRIMARYGAMMA) != 0) {
        tig_debug_printf("Query for IID_IDirectDrawGammaControl ");

        if (SUCCEEDED(IDirectDrawSurface7_QueryInterface(tig_video_state.primary_surface, &IID_IDirectDrawGammaControl, (LPVOID*)&(tig_video_state.gamma_control)))) {
            tig_debug_printf("succeeded.\n");
            tig_video_state.have_gamma_control = true;
        } else {
            tig_debug_printf("failed.\n");
        }
    }

    tig_debug_printf("Gamma control ");
    if (!tig_video_state.have_gamma_control) {
        tig_debug_printf("not ");
    }
    tig_debug_printf("supported.\n");

    if (tig_video_state.have_gamma_control) {
        if (SUCCEEDED(IDirectDrawGammaControl_GetGammaRamp(tig_video_state.gamma_control, 0, &(tig_video_state.default_gamma_ramp)))) {
            memcpy(&(tig_video_state.current_gamma_ramp), &(tig_video_state.default_gamma_ramp), sizeof(DDGAMMARAMP));
            tig_video_gamma = 1.0;
        }
    }

    return true;
}

// 0x524830
bool sub_524830()
{
    DDPIXELFORMAT ddpf;
    unsigned int red_mask;
    unsigned int green_mask;
    unsigned int blue_mask;
    int attempt;

    for (attempt = 0; attempt < 50; attempt++) {
        // NOTE: This code causes screen to flicker once on init and exit.
        // red_mask = tig_video_color_to_mask(RGB(255, 0, 0));
        // green_mask = tig_video_color_to_mask(RGB(0, 255, 0));
        // blue_mask = tig_video_color_to_mask(RGB(0, 0, 255));

        // // TODO: What the hell is this?
        // if ((red_mask | green_mask | blue_mask) != 0) {
        //     if (((red_mask & green_mask) | (blue_mask & (red_mask | green_mask))) == 0) {
        //         tig_color_set_rgb_settings(red_mask, green_mask, blue_mask);
        //         return true;
        //     }
        // }

        ddpf.dwSize = sizeof(ddpf);
        if (SUCCEEDED(IDirectDrawSurface7_GetPixelFormat(tig_video_state.primary_surface, &ddpf))) {
            if ((ddpf.dwFlags & DDPF_RGB) != 0) {
                red_mask = ddpf.dwRBitMask;
                green_mask = ddpf.dwGBitMask;
                blue_mask = ddpf.dwBBitMask;

                if ((red_mask | green_mask | blue_mask) != 0) {
                    if (((red_mask & green_mask) | (blue_mask & (red_mask | green_mask))) == 0) {
                        tig_color_set_rgb_settings(red_mask, green_mask, blue_mask);
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

// 0x5248F0
int tig_video_ddraw_exit()
{
    tig_video_d3d_exit();

    if (tig_video_bpp == 8) {
        tig_video_ddraw_palette_destroy();
    }

    IDirectDraw7_Release(tig_video_state.ddraw);
    tig_video_state.ddraw = NULL;
    tig_video_state.primary_surface = NULL;
    tig_video_state.offscreen_surface = NULL;
    tig_video_state.main_surface = NULL;
    tig_video_state.palette = NULL;

    return TIG_OK;
}

// 0x524930
bool tig_video_d3d_init(TigInitInfo* init_info)
{
    D3DDEVICEDESC7 device_desc;

    tig_video_3d_initialized = false;
    tig_video_3d_is_hardware = false;
    tig_video_3d_texture_must_be_power_of_two = false;
    tig_video_3d_texture_must_be_square = false;
    tig_video_3d_extra_surface_caps = 0;
    tig_video_3d_extra_surface_caps2 = 0;
    tig_video_3d_viewport = NULL;

    tig_debug_printf("3D: Query for IID_IDirect3D7 interface ");

    if (FAILED(IDirectDraw7_QueryInterface(tig_video_state.ddraw, &IID_IDirect3D7, (LPVOID*)&(tig_video_state.d3d)))) {
        tig_debug_printf("failed. 3D support disabled.\n");
        return false;
    }

    tig_debug_printf("succeeded.\n");

    if ((init_info->flags & TIG_INITIALIZE_3D_REF_DEVICE) != 0) {
        tig_debug_printf("3D: Checking for reference device...");

        if (FAILED(IDirect3D7_CreateDevice(tig_video_state.d3d, &IID_IDirect3DRefDevice, tig_video_state.main_surface, &(tig_video_state.d3d_device)))) {
            tig_debug_printf("FAILED.\n");
            return false;
        }

        tig_debug_printf("found IID_IDirect3DRefDevice\n");
    } else {
        if ((init_info->flags & TIG_INITIALIZE_3D_HARDWARE_DEVICE) != 0) {
            tig_debug_printf("3D: Checking for hardware devices...");

            if (SUCCEEDED(IDirect3D7_CreateDevice(tig_video_state.d3d, &IID_IDirect3DTnLHalDevice, tig_video_state.main_surface, &(tig_video_state.d3d_device)))) {
                tig_video_3d_is_hardware = true;
                tig_debug_printf("found IID_IDirect3DTnLHalDevice\n");
            } else if (SUCCEEDED(IDirect3D7_CreateDevice(tig_video_state.d3d, &IID_IDirect3DHALDevice, tig_video_state.main_surface, &(tig_video_state.d3d_device)))) {
                tig_video_3d_is_hardware = true;
                tig_debug_printf("found IID_IDirect3DHALDevice\n");
            } else {
                init_info->flags &= ~TIG_INITIALIZE_3D_HARDWARE_DEVICE;
                tig_debug_printf("none found.\n");
            }
        }

        if (!tig_video_3d_is_hardware) {
            if ((init_info->flags & TIG_INITIALIZE_3D_SOFTWARE_DEVICE) == 0) {
                return false;
            }

            tig_debug_printf("3D: Checking for RGB software devices...");

            if (SUCCEEDED(IDirect3D7_CreateDevice(tig_video_state.d3d, &IID_IDirect3DMMXDevice, tig_video_state.main_surface, &(tig_video_state.d3d_device)))) {
                tig_debug_printf("found IID_IDirect3DMMXDevice.\n");
            } else if (SUCCEEDED(IDirect3D7_CreateDevice(tig_video_state.d3d, &IID_IDirect3DRGBDevice, tig_video_state.main_surface, &(tig_video_state.d3d_device)))) {
                tig_debug_printf("found IID_IDirect3DRGBDevice.\n");
            } else {
                init_info->flags &= ~TIG_INITIALIZE_3D_SOFTWARE_DEVICE;
                tig_debug_printf("none found.\n");

                return false;
            }
        }
    }

    tig_debug_printf("3D: Retrieving caps from device...\n");

    if (FAILED(IDirect3DDevice7_GetCaps(tig_video_state.d3d_device, &device_desc))) {
        tig_debug_printf("3D: Error retrieving caps from device.\n");
        return false;
    }

    if (tig_video_3d_is_hardware) {
        tig_video_3d_extra_surface_caps = DDSCAPS_TEXTURE;
        tig_video_3d_extra_surface_caps2 = DDSCAPS2_TEXTUREMANAGE;
    } else {
        tig_video_3d_extra_surface_caps = DDSCAPS_SYSTEMMEMORY;
        tig_video_3d_extra_surface_caps2 = 0;
    }

    if (init_info->texture_width > device_desc.dwMaxTextureWidth || init_info->texture_height > device_desc.dwMaxTextureHeight) {
        tig_debug_printf(
            "3D: Card cannot support %dx%d sized textures - max is %dx%d\n",
            init_info->texture_width,
            init_info->texture_height,
            device_desc.dwMaxTextureWidth,
            device_desc.dwMaxTextureHeight);
        return false;
    }

    if ((device_desc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_POW2) != 0) {
        tig_video_3d_texture_must_be_power_of_two = true;
        tig_debug_printf("3D: Textures must be power of 2.\n");
    }

    if ((device_desc.dpcTriCaps.dwTextureCaps & D3DPTEXTURECAPS_SQUAREONLY) != 0) {
        tig_video_3d_texture_must_be_square = true;
        tig_debug_printf("3D: Textures must square.\n");
    }

    tig_debug_printf("3D: Setting default rendering states...\n");

    if (SUCCEEDED(IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device, D3DRENDERSTATE_TEXTUREPERSPECTIVE, 0))) {
        tig_debug_printf("3D: Perspective correction disabled.\n");
    } else {
        tig_debug_printf("3D: Error disabling perspective correction.\n");
    }

    if (SUCCEEDED(IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device, D3DRENDERSTATE_CULLMODE, 1))) {
        tig_debug_printf("3D: culling disabled.\n");
    } else {
        tig_debug_printf("3D: Error disabling culling.\n");
    }

    if (SUCCEEDED(IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device, D3DRENDERSTATE_LIGHTING, 0))) {
        tig_debug_printf("3D: Lighting disabled.\n");
    } else {
        tig_debug_printf("3D: Error disabling lighting.\n");
    }

    if (FAILED(IDirect3DDevice7_SetTextureStageState(tig_video_state.d3d_device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE))) {
        tig_debug_printf("3D: Error setting alpha arg 1.\n");
    }

    if (FAILED(IDirect3DDevice7_SetTextureStageState(tig_video_state.d3d_device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE))) {
        tig_debug_printf("3D: Error setting alpha arg 2.\n");
    }

    if (FAILED(IDirect3DDevice7_SetTextureStageState(tig_video_state.d3d_device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE))) {
        tig_debug_printf("3D: Error setting alpha op.\n");
    }

    if (FAILED(IDirect3DDevice7_SetTextureStageState(tig_video_state.d3d_device, 0, D3DTSS_MINFILTER, D3DTFN_POINT))) {
        tig_debug_printf("3D: Error setting texture filtering.\n");
    }

    if (FAILED(IDirect3DDevice7_SetTextureStageState(tig_video_state.d3d_device, 0, D3DTSS_MAGFILTER, D3DTFN_POINT))) {
        tig_debug_printf("3D: Error setting texture filtering.\n");
    }

    memset(&tig_video_3d_pixel_format, 0, sizeof(tig_video_3d_pixel_format));

    if (FAILED(IDirect3DDevice7_EnumTextureFormats(tig_video_state.d3d_device, tig_video_3d_check_pixel_format, (LPVOID)init_info->bpp))) {
        tig_debug_printf("3D: Error enumerating texture formats.\n");
        return false;
    }

    if (tig_video_3d_pixel_format.dwRGBBitCount == 0) {
        tig_debug_printf("3D: Error getting texture pixel format.\n");
        return false;
    }

    if (SUCCEEDED(IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device, D3DRENDERSTATE_ALPHATESTENABLE, TRUE))) {
        tig_debug_printf("3D: alpha test enabled.\n");
    } else {
        tig_debug_printf("3D: Error setting alpha test render state.\n");
        return false;
    }

    if (SUCCEEDED(IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device, D3DRENDERSTATE_ALPHAREF, 1))) {
        tig_debug_printf("3D: alpha reference set.\n");
    } else {
        tig_debug_printf("3D: Error setting alpha reference.\n");
        return false;
    }

    if (SUCCEEDED(IDirect3DDevice7_SetRenderState(tig_video_state.d3d_device, D3DRENDERSTATE_ALPHAFUNC, D3DCMP_GREATEREQUAL))) {
        tig_debug_printf("3D: alpha func set.\n");
    } else {
        tig_debug_printf("3D: Error setting alpha func.\n");
        return false;
    }

    tig_color_set_rgba_settings(tig_video_3d_pixel_format.dwRBitMask,
        tig_video_3d_pixel_format.dwGBitMask,
        tig_video_3d_pixel_format.dwBBitMask,
        tig_video_3d_pixel_format.dwRGBAlphaBitMask);

    tig_debug_printf("3D: Initialization successful.\n");
    tig_video_3d_initialized = true;

    return true;
}

// 0x524E90
HRESULT CALLBACK tig_video_3d_check_pixel_format(LPDDPIXELFORMAT lpDDPixFmt, LPVOID lpContext)
{
    if (lpDDPixFmt == NULL) {
        return S_FALSE;
    }

    if ((DWORD)lpContext == 0) {
        return S_FALSE;
    }

    if ((lpDDPixFmt->dwFlags & (DDPF_LUMINANCE | DDPF_BUMPLUMINANCE | DDPF_BUMPDUDV)) != 0) {
        return S_FALSE;
    }

    if (lpDDPixFmt->dwRGBBitCount < 16) {
        return S_FALSE;
    }

    if (lpDDPixFmt->dwFourCC != 0) {
        return S_FALSE;
    }

    if (lpDDPixFmt->dwRGBAlphaBitMask == 0xF000) {
        return S_FALSE;
    }

    if ((lpDDPixFmt->dwFlags & DDPF_ALPHAPIXELS) == 0) {
        return S_FALSE;
    }

    if (lpDDPixFmt->dwRGBBitCount != (DWORD)lpContext) {
        return S_FALSE;
    }

    memcpy(&tig_video_3d_pixel_format, lpDDPixFmt, sizeof(DDPIXELFORMAT));

    return S_OK;
}

// 0x524EF0
void tig_video_d3d_exit()
{
}

// 0x524F00
bool tig_video_ddraw_palette_create(LPDIRECTDRAW7 ddraw)
{
    PALETTEENTRY* entries;
    int index;
    int red;
    int green;
    int blue;
    HRESULT hr;

    tig_video_state.palette = NULL;

    // NOTE: Why use heap?
    entries = (PALETTEENTRY*)MALLOC(sizeof(*entries) * 256);
    for (index = 0; index < 256; index++) {
        red = tig_color_get_red(index);
        green = tig_color_get_green(index);
        blue = tig_color_get_blue(index);

        entries[index].peRed = (uint8_t)red;
        entries[index].peGreen = (uint8_t)green;
        entries[index].peBlue = (uint8_t)blue;
        entries[index].peFlags = 0;

        tig_video_palette[index] = tig_color_to_24_bpp(red, green, blue);
    }

    hr = IDirectDraw7_CreatePalette(ddraw,
        DDPCAPS_8BIT | DDPCAPS_ALLOW256,
        entries,
        &(tig_video_state.palette),
        NULL);
    if (FAILED(hr)) {
        FREE(entries);
        return false;
    }

    hr = IDirectDrawSurface7_SetPalette(tig_video_state.primary_surface,
        tig_video_state.palette);
    if (FAILED(hr)) {
        FREE(entries);
        return false;
    }

    FREE(entries);
    return true;
}

// 0x524FC0
void tig_video_ddraw_palette_destroy()
{
    if (tig_video_state.palette != NULL) {
        IDirectDrawPalette_Release(tig_video_state.palette);
    }
}

// 0x524FD0
bool tig_video_surface_create(LPDIRECTDRAW7 ddraw, int width, int height, unsigned int caps, LPDIRECTDRAWSURFACE7* surface_ptr)
{
    HRESULT hr;
    DDSURFACEDESC2 ddsd;

    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);

    if ((caps & DDSCAPS_PRIMARYSURFACE) != 0) {
        ddsd.dwFlags |= DDSD_CAPS;
        ddsd.ddsCaps.dwCaps = caps;

        if ((caps & DDSCAPS_FLIP) != 0) {
            ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
            ddsd.dwBackBufferCount = 1;
        }
    } else {
        ddsd.dwFlags |= DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        ddsd.dwWidth = width;
        ddsd.dwHeight = height;
        ddsd.ddsCaps.dwCaps = caps;

        if ((caps & DDSCAPS_TEXTURE) != 0) {
            ddsd.dwFlags |= DDSD_PIXELFORMAT;
            memcpy(&(ddsd.ddpfPixelFormat), &tig_video_3d_pixel_format, sizeof(DDPIXELFORMAT));
            ddsd.ddsCaps.dwCaps |= tig_video_3d_extra_surface_caps;
            ddsd.ddsCaps.dwCaps2 |= tig_video_3d_extra_surface_caps2;
        }
    }

    hr = IDirectDraw7_CreateSurface(ddraw, &ddsd, surface_ptr, NULL);
    if (FAILED(hr)) {
        tig_video_print_dd_result(hr);
        return false;
    }

    if ((caps & DDSCAPS_FLIP) != 0) {
        DDSCAPS2 ddscaps2;
        ddscaps2.dwCaps = DDSCAPS_BACKBUFFER;
        ddscaps2.dwCaps2 = 0;
        ddscaps2.dwCaps3 = 0;
        ddscaps2.dwCaps3 = 0;

        hr = IDirectDrawSurface7_GetAttachedSurface(*surface_ptr, &ddscaps2, &(tig_video_state.offscreen_surface));
        if (FAILED(hr)) {
            tig_video_print_dd_result(hr);
            tig_video_surface_destroy(surface_ptr);
            return false;
        }
    }

    return true;
}

// 0x525120
void tig_video_surface_destroy(LPDIRECTDRAWSURFACE7* surface_ptr)
{
    if (surface_ptr != NULL) {
        IDirectDrawSurface7_Release(*surface_ptr);
        *surface_ptr = NULL;
    }
}

// 0x525140
bool tig_video_surface_lock(LPDIRECTDRAWSURFACE7 surface, LPDDSURFACEDESC2 surface_desc, void** surface_data_ptr)
{
    HRESULT hr;

    memset(surface_desc, 0, sizeof(*surface_desc));
    surface_desc->dwSize = sizeof(*surface_desc);

    hr = IDirectDrawSurface7_Lock(surface, NULL, surface_desc, DDLOCK_OKTOSWAP | DDLOCK_WAIT, NULL);
    if (FAILED(hr)) {
        tig_video_print_dd_result(hr);
        return false;
    }

    *surface_data_ptr = surface_desc->lpSurface;

    return true;
}

// 0x525190
bool tig_video_surface_unlock(LPDIRECTDRAWSURFACE7 surface, LPDDSURFACEDESC2 surface_desc)
{
    HRESULT hr;

    hr = IDirectDrawSurface7_Unlock(surface, surface_desc->lpSurface);
    if (FAILED(hr)) {
        tig_video_print_dd_result(hr);
        return false;
    }

    return true;
}

// 0x5251C0
bool tig_video_surface_fill(LPDIRECTDRAWSURFACE7 surface, const TigRect* rect, tig_color_t color)
{
    DDBLTFX fx;
    RECT native_rect;
    HRESULT hr;

    fx.dwSize = sizeof(fx);
    fx.dwFillColor = color;

    if (rect != NULL) {
        native_rect.left = rect->x;
        native_rect.top = rect->y;
        native_rect.right = native_rect.left + rect->width;
        native_rect.bottom = native_rect.top + rect->height;
    }

    hr = IDirectDrawSurface_Blt(surface,
        rect != NULL ? &native_rect : NULL,
        0,
        0,
        DDBLT_WAIT | DDBLT_COLORFILL,
        &fx);
    if (FAILED(hr)) {
        tig_video_print_dd_result(hr);
        return false;
    }

    return true;
}

// 0x525250
int tig_video_screenshot_make_internal(int key)
{
    int rc;
    int index;
    char path[TIG_MAX_PATH];
    void* surface_data;
    TigVideoBufferData video_buffer_data;
    TigRect rect;

    if (tig_video_screenshot_key != key) {
        return TIG_ERR_16;
    }

    for (index = 0; index < INT_MAX; index++) {
        sprintf(path, "screen%04d.bmp", index);
        if (!tig_file_exists(path, NULL)) {
            break;
        }
    }

    if (index == INT_MAX) {
        return TIG_ERR_13;
    }

    rc = tig_video_main_surface_lock(&surface_data);
    if (rc != TIG_OK) {
        return rc;
    }

    video_buffer_data.pitch = tig_video_state.current_surface_desc.lPitch;
    video_buffer_data.width = tig_video_state.current_surface_desc.dwWidth;
    video_buffer_data.height = tig_video_state.current_surface_desc.dwHeight;
    video_buffer_data.surface_data.pixels = surface_data;
    video_buffer_data.flags = TIG_VIDEO_BUFFER_LOCKED | TIG_VIDEO_BUFFER_VIDEO_MEMORY;
    video_buffer_data.background_color = 0;
    video_buffer_data.color_key = 0;

    rect.x = 0;
    rect.y = 0;
    rect.width = tig_video_state.current_surface_desc.dwWidth;
    rect.height = tig_video_state.current_surface_desc.dwHeight;

    rc = tig_video_buffer_data_to_bmp(&video_buffer_data, &rect, path, false);

    tig_video_main_surface_unlock();

    return rc;
}

// 0x525350
unsigned int tig_video_color_to_mask(COLORREF color)
{
    HDC hdc;
    COLORREF saved_color = 0;
    unsigned int mask = 0;
    DDSURFACEDESC2 ddsd;

    if (SUCCEEDED(IDirectDrawSurface7_GetDC(tig_video_state.primary_surface, &hdc))) {
        saved_color = GetPixel(hdc, 0, 0);
        SetPixel(hdc, 0, 0, color);
        IDirectDrawSurface7_ReleaseDC(tig_video_state.primary_surface, hdc);
    }

    ddsd.dwSize = sizeof(ddsd);

    if (SUCCEEDED(IDirectDrawSurface7_Lock(tig_video_state.primary_surface, NULL, &ddsd, DDLOCK_WAIT, NULL))) {
        mask = *((unsigned int*)ddsd.lpSurface);
        if (ddsd.ddpfPixelFormat.dwRGBBitCount < 32) {
            mask &= (1 << ddsd.ddpfPixelFormat.dwRGBBitCount) - 1;
        }
        IDirectDrawSurface7_Unlock(tig_video_state.primary_surface, NULL);
    }

    if (SUCCEEDED(IDirectDrawSurface7_GetDC(tig_video_state.primary_surface, &hdc))) {
        SetPixel(hdc, 0, 0, saved_color);
        IDirectDrawSurface7_ReleaseDC(tig_video_state.primary_surface, hdc);
    }

    return mask;
}

// 0x525430
void tig_video_print_dd_result(HRESULT hr)
{
    switch (hr) {
    case DDERR_ALREADYINITIALIZED:
        tig_debug_println("DDERR_ALREADYINITIALIZED");
        break;
    case DDERR_BLTFASTCANTCLIP:
        tig_debug_println("DDERR_BLTFASTCANTCLIP");
        break;
    case DDERR_CANNOTATTACHSURFACE:
        tig_debug_println("DDERR_CANNOTATTACHSURFACE");
        break;
    case DDERR_CANNOTDETACHSURFACE:
        tig_debug_println("DDERR_CANNOTDETACHSURFACE");
        break;
    case DDERR_CANTCREATEDC:
        tig_debug_println("DDERR_CANTCREATEDC");
        break;
    case DDERR_CANTDUPLICATE:
        tig_debug_println("DDERR_CANTDUPLICATE");
        break;
    case DDERR_CANTLOCKSURFACE:
        tig_debug_println("DDERR_CANTLOCKSURFACE");
        break;
    case DDERR_CANTPAGELOCK:
        tig_debug_println("DDERR_CANTPAGELOCK");
        break;
    case DDERR_CANTPAGEUNLOCK:
        tig_debug_println("DDERR_CANTPAGEUNLOCK");
        break;
    case DDERR_CLIPPERISUSINGHWND:
        tig_debug_println("DDERR_CLIPPERISUSINGHWND");
        break;
    case DDERR_COLORKEYNOTSET:
        tig_debug_println("DDERR_COLORKEYNOTSET");
        break;
    case DDERR_CURRENTLYNOTAVAIL:
        tig_debug_println("DDERR_CURRENTLYNOTAVAIL");
        break;
    case DDERR_DCALREADYCREATED:
        tig_debug_println("DDERR_DCALREADYCREATED");
        break;
    case DDERR_DEVICEDOESNTOWNSURFACE:
        tig_debug_println("DDERR_DEVICEDOESNTOWNSURFACE");
        break;
    case DDERR_DIRECTDRAWALREADYCREATED:
        tig_debug_println("DDERR_DIRECTDRAWALREADYCREATED");
        break;
    case DDERR_EXCEPTION:
        tig_debug_println("DDERR_EXCEPTION");
        break;
    case DDERR_EXCLUSIVEMODEALREADYSET:
        tig_debug_println("DDERR_EXCLUSIVEMODEALREADYSET");
        break;
    case DDERR_GENERIC:
        tig_debug_println("DDERR_GENERIC");
        break;
    case DDERR_HEIGHTALIGN:
        tig_debug_println("DDERR_HEIGHTALIGN");
        break;
    case DDERR_HWNDALREADYSET:
        tig_debug_println("DDERR_HWNDALREADYSET");
        break;
    case DDERR_HWNDSUBCLASSED:
        tig_debug_println("DDERR_HWNDSUBCLASSED");
        break;
    case DDERR_IMPLICITLYCREATED:
        tig_debug_println("DDERR_IMPLICITLYCREATED");
        break;
    case DDERR_INCOMPATIBLEPRIMARY:
        tig_debug_println("DDERR_INCOMPATIBLEPRIMARY");
        break;
    case DDERR_INVALIDCAPS:
        tig_debug_println("DDERR_INVALIDCAPS");
        break;
    case DDERR_INVALIDCLIPLIST:
        tig_debug_println("DDERR_INVALIDCLIPLIST");
        break;
    case DDERR_INVALIDDIRECTDRAWGUID:
        tig_debug_println("DDERR_INVALIDDIRECTDRAWGUID");
        break;
    case DDERR_INVALIDMODE:
        tig_debug_println("DDERR_INVALIDMODE");
        break;
    case DDERR_INVALIDOBJECT:
        tig_debug_println("DDERR_INVALIDOBJECT");
        break;
    case DDERR_INVALIDPARAMS:
        tig_debug_println("DDERR_INVALIDPARAMS");
        break;
    case DDERR_INVALIDPIXELFORMAT:
        tig_debug_println("DDERR_INVALIDPIXELFORMAT");
        break;
    case DDERR_INVALIDPOSITION:
        tig_debug_println("DDERR_INVALIDPOSITION");
        break;
    case DDERR_INVALIDRECT:
        tig_debug_println("DDERR_INVALIDRECT");
        break;
    case DDERR_INVALIDSURFACETYPE:
        tig_debug_println("DDERR_INVALIDSURFACETYPE");
        break;
    case DDERR_LOCKEDSURFACES:
        tig_debug_println("DDERR_LOCKEDSURFACES");
        break;
    case DDERR_MOREDATA:
        tig_debug_println("DDERR_MOREDATA");
        break;
    case DDERR_NO3D:
        tig_debug_println("DDERR_NO3D");
        break;
    case DDERR_NOALPHAHW:
        tig_debug_println("DDERR_NOALPHAHW");
        break;
    case DDERR_NOBLTHW:
        tig_debug_println("DDERR_NOBLTHW");
        break;
    case DDERR_NOCLIPLIST:
        tig_debug_println("DDERR_NOCLIPLIST");
        break;
    case DDERR_NOCLIPPERATTACHED:
        tig_debug_println("DDERR_NOCLIPPERATTACHED");
        break;
    case DDERR_NOCOLORCONVHW:
        tig_debug_println("DDERR_NOCOLORCONVHW");
        break;
    case DDERR_NOCOLORKEY:
        tig_debug_println("DDERR_NOCOLORKEY");
        break;
    case DDERR_NOCOLORKEYHW:
        tig_debug_println("DDERR_NOCOLORKEYHW");
        break;
    case DDERR_NOCOOPERATIVELEVELSET:
        tig_debug_println("DDERR_NOCOOPERATIVELEVELSET");
        break;
    case DDERR_NODC:
        tig_debug_println("DDERR_NODC");
        break;
    case DDERR_NODDROPSHW:
        tig_debug_println("DDERR_NODDROPSHW");
        break;
    case DDERR_NODIRECTDRAWHW:
        tig_debug_println("DDERR_NODIRECTDRAWHW");
        break;
    case DDERR_NODIRECTDRAWSUPPORT:
        tig_debug_println("DDERR_NODIRECTDRAWSUPPORT");
        break;
    case DDERR_NOEMULATION:
        tig_debug_println("DDERR_NOEMULATION");
        break;
    case DDERR_NOEXCLUSIVEMODE:
        tig_debug_println("DDERR_NOEXCLUSIVEMODE");
        break;
    case DDERR_NOFLIPHW:
        tig_debug_println("DDERR_NOFLIPHW");
        break;
    case DDERR_NOFOCUSWINDOW:
        tig_debug_println("DDERR_NOFOCUSWINDOW");
        break;
    case DDERR_NOGDI:
        tig_debug_println("DDERR_NOGDI");
        break;
    case DDERR_NOHWND:
        tig_debug_println("DDERR_NOHWND");
        break;
    case DDERR_NOMIPMAPHW:
        tig_debug_println("DDERR_NOMIPMAPHW");
        break;
    case DDERR_NOMIRRORHW:
        tig_debug_println("DDERR_NOMIRRORHW");
        break;
    case DDERR_NONONLOCALVIDMEM:
        tig_debug_println("DDERR_NONONLOCALVIDMEM");
        break;
    case DDERR_NOOPTIMIZEHW:
        tig_debug_println("DDERR_NOOPTIMIZEHW");
        break;
    case DDERR_NOOVERLAYDEST:
        tig_debug_println("DDERR_NOOVERLAYDEST");
        break;
    case DDERR_NOOVERLAYHW:
        tig_debug_println("DDERR_NOOVERLAYHW");
        break;
    case DDERR_NOPALETTEATTACHED:
        tig_debug_println("DDERR_NOPALETTEATTACHED");
        break;
    case DDERR_NOPALETTEHW:
        tig_debug_println("DDERR_NOPALETTEHW");
        break;
    case DDERR_NORASTEROPHW:
        tig_debug_println("DDERR_NORASTEROPHW");
        break;
    case DDERR_NOROTATIONHW:
        tig_debug_println("DDERR_NOROTATIONHW");
        break;
    case DDERR_NOSTRETCHHW:
        tig_debug_println("DDERR_NOSTRETCHHW");
        break;
    case DDERR_NOT4BITCOLOR:
        tig_debug_println("DDERR_NOT4BITCOLOR");
        break;
    case DDERR_NOT4BITCOLORINDEX:
        tig_debug_println("DDERR_NOT4BITCOLORINDEX");
        break;
    case DDERR_NOT8BITCOLOR:
        tig_debug_println("DDERR_NOT8BITCOLOR");
        break;
    case DDERR_NOTAOVERLAYSURFACE:
        tig_debug_println("DDERR_NOTAOVERLAYSURFACE");
        break;
    case DDERR_NOTEXTUREHW:
        tig_debug_println("DDERR_NOTEXTUREHW");
        break;
    case DDERR_NOTFLIPPABLE:
        tig_debug_println("DDERR_NOTFLIPPABLE");
        break;
    case DDERR_NOTFOUND:
        tig_debug_println("DDERR_NOTFOUND");
        break;
    case DDERR_NOTINITIALIZED:
        tig_debug_println("DDERR_NOTINITIALIZED");
        break;
    case DDERR_NOTLOADED:
        tig_debug_println("DDERR_NOTLOADED");
        break;
    case DDERR_NOTLOCKED:
        tig_debug_println("DDERR_NOTLOCKED");
        break;
    case DDERR_NOTPAGELOCKED:
        tig_debug_println("DDERR_NOTPAGELOCKED");
        break;
    case DDERR_NOTPALETTIZED:
        tig_debug_println("DDERR_NOTPALETTIZED");
        break;
    case DDERR_NOVSYNCHW:
        tig_debug_println("DDERR_NOVSYNCHW");
        break;
    case DDERR_NOZBUFFERHW:
        tig_debug_println("DDERR_NOZBUFFERHW");
        break;
    case DDERR_NOZOVERLAYHW:
        tig_debug_println("DDERR_NOZOVERLAYHW");
        break;
    case DDERR_OUTOFCAPS:
        tig_debug_println("DDERR_OUTOFCAPS");
        break;
    case DDERR_OUTOFMEMORY:
        tig_debug_println("DDERR_OUTOFMEMORY");
        break;
    case DDERR_OUTOFVIDEOMEMORY:
        tig_debug_println("DDERR_OUTOFVIDEOMEMORY");
        break;
    case DDERR_OVERLAYCANTCLIP:
        tig_debug_println("DDERR_OVERLAYCANTCLIP");
        break;
    case DDERR_OVERLAYCOLORKEYONLYONEACTIVE:
        tig_debug_println("DDERR_OVERLAYCOLORKEYONLYONEACTIVE");
        break;
    case DDERR_OVERLAYNOTVISIBLE:
        tig_debug_println("DDERR_OVERLAYNOTVISIBLE");
        break;
    case DDERR_PALETTEBUSY:
        tig_debug_println("DDERR_PALETTEBUSY");
        break;
    case DDERR_PRIMARYSURFACEALREADYEXISTS:
        tig_debug_println("DDERR_PRIMARYSURFACEALREADYEXISTS");
        break;
    case DDERR_REGIONTOOSMALL:
        tig_debug_println("DDERR_REGIONTOOSMALL");
        break;
    case DDERR_SURFACEALREADYATTACHED:
        tig_debug_println("DDERR_SURFACEALREADYATTACHED");
        break;
    case DDERR_SURFACEALREADYDEPENDENT:
        tig_debug_println("DDERR_SURFACEALREADYDEPENDENT");
        break;
    case DDERR_SURFACEBUSY:
        tig_debug_println("DDERR_SURFACEBUSY");
        break;
    case DDERR_SURFACEISOBSCURED:
        tig_debug_println("DDERR_SURFACEISOBSCURED");
        break;
    case DDERR_SURFACELOST:
        tig_debug_println("DDERR_SURFACELOST");
        break;
    case DDERR_SURFACENOTATTACHED:
        tig_debug_println("DDERR_SURFACENOTATTACHED");
        break;
    case DDERR_TOOBIGHEIGHT:
        tig_debug_println("DDERR_TOOBIGHEIGHT");
        break;
    case DDERR_TOOBIGSIZE:
        tig_debug_println("DDERR_TOOBIGSIZE");
        break;
    case DDERR_TOOBIGWIDTH:
        tig_debug_println("DDERR_TOOBIGWIDTH");
        break;
    case DDERR_UNSUPPORTED:
        tig_debug_println("DDERR_UNSUPPORTED");
        break;
    case DDERR_UNSUPPORTEDFORMAT:
        tig_debug_println("DDERR_UNSUPPORTEDFORMAT");
        break;
    case DDERR_UNSUPPORTEDMASK:
        tig_debug_println("DDERR_UNSUPPORTEDMASK");
        break;
    case DDERR_UNSUPPORTEDMODE:
        tig_debug_println("DDERR_UNSUPPORTEDMODE");
        break;
    case DDERR_VERTICALBLANKINPROGRESS:
        tig_debug_println("DDERR_VERTICALBLANKINPROGRESS");
        break;
    case DDERR_VIDEONOTACTIVE:
        tig_debug_println("DDERR_VIDEONOTACTIVE");
        break;
    case DDERR_WASSTILLDRAWING:
        tig_debug_println("DDERR_WASSTILLDRAWING");
        break;
    case DDERR_WRONGMODE:
        tig_debug_println("DDERR_WRONGMODE");
        break;
    case DDERR_XALIGN:
        tig_debug_println("DDERR_XALIGN");
        break;
    default:
        tig_debug_println("Unknown HRESULT from DDRAW error\n");
        break;
    }
}

// 0x525ED0
int tig_video_buffer_data_to_bmp(TigVideoBufferData* video_buffer_data, TigRect* rect, const char* file_name, bool palette_indexed)
{
    BITMAPFILEHEADER file_hdr;
    BITMAPINFOHEADER info_hdr;
    int size;
    int padding;
    TigFile* stream;
    void* src;
    uint16_t* src16;
    uint8_t* src24;
    uint32_t* src32;
    uint32_t clr;
    uint8_t b;
    int x;
    int y;

    if (palette_indexed) {
        padding = rect->width % 4;
        if (padding != 0) {
            padding = 4 - padding;
        }

        size = rect->height * (rect->width + padding) + sizeof(RGBQUAD) * 256;
        file_hdr.bfOffBits = sizeof(file_hdr) + sizeof(info_hdr) + sizeof(RGBQUAD) * 256;
    } else {
        padding = 3 * rect->width % 4;
        if (padding != 0) {
            padding = 4 - padding;
        }
        size = rect->height * (padding + 3 * rect->width);
        file_hdr.bfOffBits = sizeof(file_hdr) + sizeof(info_hdr);
    }

    file_hdr.bfSize = size + sizeof(file_hdr) + sizeof(info_hdr);
    file_hdr.bfType = 0x4D42;
    file_hdr.bfReserved1 = 0;
    file_hdr.bfReserved2 = 0;

    info_hdr.biWidth = rect->width;
    info_hdr.biHeight = rect->height;
    info_hdr.biPlanes = 1;
    info_hdr.biCompression = 0;
    info_hdr.biSizeImage = 0;
    info_hdr.biXPelsPerMeter = 2880;
    info_hdr.biYPelsPerMeter = 2880;
    info_hdr.biClrImportant = 0;
    info_hdr.biClrUsed = 0;
    info_hdr.biBitCount = palette_indexed ? 8 : 24;

    stream = tig_file_fopen(file_name, "wb");
    if (stream == NULL) {
        return TIG_ERR_13;
    }

    if (tig_file_fwrite(&file_hdr, sizeof(file_hdr), 1, stream) != 1) {
        tig_file_fclose(stream);
        tig_file_remove(file_name);
        return TIG_ERR_13;
    }

    if (tig_file_fwrite(&info_hdr, sizeof(info_hdr), 1, stream) != 1) {
        tig_file_fclose(stream);
        tig_file_remove(file_name);
        return TIG_ERR_13;
    }

    src = (uint8_t*)video_buffer_data->surface_data.pixels
        + video_buffer_data->pitch * (rect->y + rect->height - 1)
        + rect->x * (tig_video_bpp / 8);
    if (palette_indexed) {
        int pixels_size = 3 * rect->width * rect->height;
        uint8_t* pixels = (uint8_t*)MALLOC(pixels_size);
        uint8_t* curr = pixels;
        uint8_t palette[768];
        int index;
        RGBQUAD quad;

        for (y = 0; y < rect->height; y++) {
            for (x = 0; x < rect->height; x++) {
                // NOTE: Rather ugly, but I'm almost 100% sure it was written like
                // this in the first place (i.e. not a result of compiler
                // optimization).
                src16 = (uint16_t*)src;
                src24 = (uint8_t*)src;
                src32 = (uint32_t*)src;

                switch (tig_video_bpp) {
                case 16:
                    clr = *src16;
                    break;
                case 24:
                    clr = src24[0] | (src24[1] << 8) | (src24[1] << 16);
                    break;
                default:
                    clr = *src32;
                    break;
                }

                *curr++ = (uint8_t)tig_color_get_blue(clr);
                *curr++ = (uint8_t)tig_color_get_green(clr);
                *curr++ = (uint8_t)tig_color_get_red(clr);

                src16++;
                src24 += 3;
                src32++;
            }

            src = (uint8_t*)src - video_buffer_data->pitch;
        }

        sub_53A2A0(pixels, pixels_size, 1);
        sub_53A8B0();
        sub_53A310();
        sub_53A390(palette);

        quad.rgbReserved = 0;

        for (index = 0; index < 256; index++) {
            quad.rgbBlue = palette[index * 3];
            quad.rgbGreen = palette[index * 3 + 1];
            quad.rgbRed = palette[index * 3 + 2];
            if (tig_file_fwrite(&quad, sizeof(quad), 1, stream) != 1) {
                // FIXME: Leaking `stream`.
                free(pixels);
                return TIG_ERR_13;
            }
        }

        sub_53A3C0();

        curr = pixels;
        for (y = 0; y < rect->height; y++) {
            for (x = 0; x < rect->width; x++) {
                b = (uint8_t)sub_53A4D0(curr[0], curr[1], curr[2]);
                if (tig_file_fwrite(&b, sizeof(b), 1, stream) != 1) {
                    // FIXME: Leaks `stream`.
                    free(pixels);
                    return TIG_ERR_13;
                }
                curr += 3;
            }

            b = 0;
            for (x = 0; x < padding; x++) {
                if (tig_file_fwrite(&b, sizeof(b), 1, stream) != 1) {
                    // FIXME: Leaks `stream`.
                    free(pixels);
                    return TIG_ERR_13;
                }
            }
        }

        free(pixels);
    } else {
        for (y = 0; y < rect->height; y++) {
            // NOTE: Rather ugly, but I'm almost 100% sure it was written like
            // this in the first place (i.e. not a result of compiler
            // optimization).
            src16 = (uint16_t*)src;
            src24 = (uint8_t*)src;
            src32 = (uint32_t*)src;

            for (x = 0; x < rect->width; x++) {
                switch (tig_video_bpp) {
                case 16:
                    clr = *src16;
                    break;
                case 24:
                    clr = src24[0] | (src24[1] << 8) | (src24[2] << 16);
                    break;
                default:
                    clr = *src32;
                    break;
                }

                b = (uint8_t)tig_color_get_blue(clr);
                if (tig_file_fwrite(&b, sizeof(b), 1, stream) != 1) {
                    // FIXME: Leaks `stream`.
                    return TIG_ERR_16;
                }

                b = (uint8_t)tig_color_get_green(clr);
                if (tig_file_fwrite(&b, sizeof(b), 1, stream) != 1) {
                    // FIXME: Leaks `stream`.
                    return TIG_ERR_16;
                }

                b = (uint8_t)tig_color_get_red(clr);
                if (tig_file_fwrite(&b, sizeof(b), 1, stream) != 1) {
                    // FIXME: Leaks `stream`.
                    return TIG_ERR_16;
                }

                src16++;
                src24 += 3;
                src32++;
            }

            b = 0;
            for (x = 0; x < padding; x++) {
                if (tig_file_fwrite(&b, sizeof(b), 1, stream) != 1) {
                    // FIXME: Leaks `stream`.
                    return TIG_ERR_16;
                }
            }

            src = (uint8_t*)src - video_buffer_data->pitch;
        }
    }

    tig_file_fclose(stream);
    return TIG_OK;
}

// 0x526450
int tig_video_3d_set_viewport(TigVideoBuffer* video_buffer)
{
    HRESULT hr;
    D3DVIEWPORT7 viewport;

    if (!tig_video_3d_initialized) {
        return TIG_ERR_16;
    }

    if (tig_video_3d_viewport == video_buffer) {
        return TIG_OK;
    }

    hr = IDirect3DDevice7_SetRenderTarget(tig_video_state.d3d_device, video_buffer->surface, 0);
    if (FAILED(hr)) {
        tig_debug_printf("3D: Error setting rendering target.\n");
        tig_video_print_dd_result(hr);
        return TIG_ERR_16;
    }

    tig_debug_printf("3D: Render target successfully set.\n");

    viewport.dwX = 0;
    viewport.dwY = 0;
    viewport.dwWidth = video_buffer->surface_desc.dwWidth;
    viewport.dwHeight = video_buffer->surface_desc.dwHeight;
    viewport.dvMinZ = 0.0;
    viewport.dvMaxZ = 1.0;

    hr = IDirect3DDevice7_SetViewport(tig_video_state.d3d_device, &viewport);
    if (FAILED(hr)) {
        tig_debug_printf("3D: Error setting viewport.\n");
        tig_video_print_dd_result(hr);
        return TIG_ERR_16;
    }

    tig_debug_printf("3D: Viewport successfully set.\n");

    return TIG_OK;
}

// 0x526530
void sub_526530(const TigRect* a1, const TigRect* a2, D3DCOLOR* alpha)
{
    float v1 = (float)(a2->x - a1->x);
    float v2 = (float)(a2->y - a1->y);
    float v3 = (float)(a2->width + v1 - 1);
    float v4 = v2 * ((float)(alpha[3] - alpha[0]) / (float)a1->height) + (float)alpha[0];
    float v5 = v2 * ((float)(alpha[2] - alpha[1]) / (float)a1->height) + (float)alpha[1];
    float v6 = (v5 - v4) / (float)a1->width;

    alpha[3] = (uint8_t)(v1 * v6 + v4);
    alpha[2] = (uint8_t)(v3 * v6 + v4);
    alpha[1] = (uint8_t)(v3 * v6 + v4);
    alpha[0] = (uint8_t)(v1 * v6 + v4);
}

// 0x526690
void sub_526690(const TigRect* a1, const TigRect* a2, D3DCOLOR* color)
{
    int index;
    uint8_t r[4];
    uint8_t g[4];
    uint8_t b[4];

    int dy = a2->y - a1->y;
    int dx = a2->x - a1->x;
    int v1 = a2->width + dx - 1;
    int v2 = a2->height + dy - 1;

    for (index = 0; index < 4; index++) {
        r[index] = RGBA_GETRED(color[index]);
        g[index] = RGBA_GETGREEN(color[index]);
        b[index] = RGBA_GETBLUE(color[index]);
    }

    float v3 = (float)(r[3] - r[0]) / (float)a1->height;
    float v4 = (float)(g[3] - g[0]) / (float)a1->height;
    float v5 = (float)(b[3] - b[0]) / (float)a1->height;
    float v6 = (float)(r[2] - r[1]) / (float)a1->height;
    float v7 = (float)(g[2] - g[1]) / (float)a1->height;
    float v8 = (float)(b[2] - b[1]) / (float)a1->height;

    float v9 = dy * v3 + r[0];
    float v10 = dy * v4 + g[0];
    float v11 = dy * v5 + b[0];

    float v12 = (dy * v6 + r[1] - v9) / (float)a1->width;
    float v13 = (dy * v7 + g[1] - v10) / (float)a1->width;
    float v14 = (dy * v8 + b[1] - v11) / (float)a1->width;

    float v15 = dx * v12 + v9;
    float v16 = dx * v13 + v10;
    float v17 = dx * v14 + v11;

    float v18 = v1 * v12 + v9;
    float v19 = v1 * v13 + v10;
    float v20 = v1 * v14 + v11;

    float v21 = v2 * v3 + r[0];
    float v22 = v2 * v4 + g[0];
    float v23 = v2 * v5 + b[0];

    float v25 = (v2 * v6 + r[1] - v21) / (float)a1->width;
    float v26 = (v2 * v7 + g[1] - v22) / (float)a1->width;
    float v27 = (v2 * v8 + b[1] - v23) / (float)a1->width;

    r[3] = (uint8_t)(dx * v25 + v21);
    g[3] = (uint8_t)(dx * v26 + v22);
    b[3] = (uint8_t)(dx * v27 + v23);

    r[2] = (uint8_t)(v1 * v25 + v21);
    g[2] = (uint8_t)(v1 * v26 + v22);
    b[2] = (uint8_t)(v1 * v27 + v23);

    r[1] = (uint8_t)v18;
    g[1] = (uint8_t)v19;
    b[1] = (uint8_t)v20;

    r[0] = (uint8_t)v15;
    g[0] = (uint8_t)v16;
    b[0] = (uint8_t)v17;

    for (index = 0; index < 4; index++) {
        color[index] = D3DRGBA((float)r[index] / 255,
            (float)g[index] / 255,
            (float)b[index] / 255,
            0);
    }
}
