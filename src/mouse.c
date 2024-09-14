#include "tig/mouse.h"

#include "tig/art.h"
#include "tig/color.h"
#include "tig/core.h"
#include "tig/dxinput.h"
#include "tig/message.h"
#include "tig/timer.h"
#include "tig/video.h"
#include "tig/window.h"

static int tig_mouse_device_init();
static int tig_mouse_hardware_init();
static void tig_mouse_device_exit();
static void tig_mouse_hardware_exit();
static bool tig_mouse_update_state();
static bool tig_mouse_device_update_state();
static void tig_mouse_cursor_fallback();
static bool tig_mouse_cursor_create_video_buffers(tig_art_id_t art_id, int dx, int dy);
static bool tig_mouse_cursor_destroy_video_buffers();
static bool tig_mouse_cursor_set_art_frame(tig_art_id_t art_id, int x, int y);
static void tig_mouse_cursor_animate();

// 0x5BE840
static int tig_mouse_state_button_down_flags[TIG_MOUSE_BUTTON_COUNT] = {
    TIG_MOUSE_STATE_LEFT_MOUSE_DOWN,
    TIG_MOUSE_STATE_RIGHT_MOUSE_DOWN,
    TIG_MOUSE_STATE_MIDDLE_MOUSE_DOWN,
};

// 0x5BE84C
static int tig_mouse_state_button_down_repeat_flags[TIG_MOUSE_BUTTON_COUNT] = {
    TIG_MOUSE_STATE_LEFT_MOUSE_DOWN_REPEAT,
    TIG_MOUSE_STATE_RIGHT_MOUSE_DOWN_REPEAT,
    TIG_MOUSE_STATE_MIDDLE_MOUSE_DOWN_REPEAT,
};

// 0x5BE858
static int tig_mouse_state_button_up_flags[TIG_MOUSE_BUTTON_COUNT] = {
    TIG_MOUSE_STATE_LEFT_MOUSE_UP,
    TIG_MOUSE_STATE_RIGHT_MOUSE_UP,
    TIG_MOUSE_STATE_MIDDLE_MOUSE_UP,
};

// 0x5BE864
static int TIG_MESSAGE_MOUSE_button_down_flags[TIG_MOUSE_BUTTON_COUNT] = {
    TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN,
    TIG_MESSAGE_MOUSE_RIGHT_BUTTON_DOWN,
    TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_DOWN,
};

// 0x5BE870
static int TIG_MESSAGE_MOUSE_button_up_flags[TIG_MOUSE_BUTTON_COUNT] = {
    TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,
    TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP,
    TIG_MESSAGE_MOUSE_MIDDLE_BUTTON_UP,
};

// 0x5BE87C
static bool tig_mouse_z_axis_enabled = true;

// 0x604640
static int tig_mouse_cursor_art_frame_height;

// 0x604644
static TigVideoBuffer* tig_mouse_cursor_opaque_video_buffer;

// 0x604648
static int tig_mouse_cursor_art_frame_width;

// 0x60464C
static int tig_mouse_max_y;

// 0x604650
static int tig_mouse_max_x;

// The interval (in milliseconds) to display each animation frame.
//
// 0x604654
static tig_duration_t tig_mouse_cursor_art_fps;

// A boolean value indicating mouse is in hardware mode.
//
// 0x604658
static bool tig_mouse_is_hardware;

// The timestamp when animation frame was last updated.
//
// 0x60465C
static tig_timestamp_t tig_mouse_cursor_art_animation_timestamp;

// 0x604660
static int tig_mouse_cursor_art_x;

// 0x604664
static int tig_mouse_cursor_art_y;

// 0x604668
static bool tig_mouse_idle_emitted;

// 0x60466C
static TigVideoBuffer* tig_mouse_cursor_trans_video_buffer;

// 0x604670
static tig_art_id_t tig_mouse_cursor_art_id;

// 0x604674
static tig_color_t tig_mouse_cursor_art_color_key;

// 0x604678
static bool tig_mouse_active;

// The timestamp of the last "mouse move" event as observed in DirectInput mode.
//
// 0x60467C
static tig_timestamp_t tig_mouse_move_timestamp;

// The timestamps of last "button down" or "button up" events used in Windows
// Messages mode.
//
// 0x604680
static tig_timestamp_t tig_mouse_hardware_button_timestamps[TIG_MOUSE_BUTTON_COUNT];

// 0x604690
static TigMouseState tig_mouse_state;

// 0x6046B8
static int tig_mouse_cursor_art_num_frames;

// Bounds of mouse cursor video buffers.
//
// The rect's origin is always (0, 0) and never changes. The size includes
// width/height of the cursor art plus extra size as specified during video
// buffers initialization in `tig_mouse_cursor_create_video_buffers`.
//
// 0x6046C0
static TigRect tig_mouse_cursor_bounds;

// The timestamp of the last mouse update in DirectInput mode.
//
// 0x6046D0
static tig_timestamp_t tig_mouse_update_timestamp;

// A boolean value indicating left and right mouse buttons are swapped at OS
// level.
//
// 0x6046D4
static bool tig_mouse_buttons_swapped;

// 0x6046D8
static TigRect tig_mouse_cursor_art_frame_bounds;

// 0x6046E8
static int dword_6046E8;

// The timestamps of the last "mouse down" or "mouse up" events used in
// DirectInput mode.
//
// 0x6046EC
static tig_timestamp_t tig_mouse_software_button_timestamps[TIG_MOUSE_BUTTON_COUNT];

// 0x6046F8
static DIMOUSESTATE tig_mouse_device_state;

// 0x604708
static LPDIRECTINPUTDEVICEA tig_mouse_device;

// 0x60470C
static bool tig_mouse_initialized;

// 0x4FF020
int tig_mouse_init(TigInitInfo* init_info)
{
    if (tig_mouse_initialized) {
        return TIG_ALREADY_INITIALIZED;
    }

    tig_mouse_max_x = init_info->width - 1;
    tig_mouse_max_y = init_info->height - 1;

    tig_mouse_state.frame.x = init_info->width / 2;
    tig_mouse_state.frame.y = init_info->height / 2;
    tig_mouse_state.frame.width = 0;
    tig_mouse_state.frame.height = 0;
    tig_mouse_state.x = init_info->width / 2;
    tig_mouse_state.y = init_info->height / 2;
    tig_mouse_state.flags = 0;

    if ((init_info->flags & TIG_INITIALIZE_WINDOWED) != 0) {
        return tig_mouse_hardware_init();
    } else {
        return tig_mouse_device_init();
    }
}

// 0x4FF0B0
int tig_mouse_device_init()
{
    LPDIRECTINPUTA direct_input;
    HWND wnd;
    HRESULT hr;
    tig_art_id_t art_id;
    int rc;

    if (tig_dxinput_get_instance(&direct_input) != TIG_OK) {
        return TIG_ERR_7;
    }

    hr = IDirectInput_CreateDevice(direct_input, &GUID_SysMouse, &tig_mouse_device, NULL);
    if (FAILED(hr)) {
        return TIG_ERR_7;
    }

    hr = IDirectInputDevice_SetDataFormat(tig_mouse_device, &c_dfDIMouse);
    if (FAILED(hr)) {
        tig_mouse_device_exit();
        return TIG_ERR_7;
    }

    if (tig_video_platform_window_get(&wnd) != TIG_OK) {
        tig_mouse_device_exit();
        return TIG_ERR_7;
    }

    hr = IDirectInputDevice_SetCooperativeLevel(tig_mouse_device, wnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    if (FAILED(hr)) {
        tig_mouse_device_exit();
        return TIG_ERR_7;
    }

    rc = tig_art_misc_id_create(TIG_ART_SYSTEM_MOUSE, 0, &art_id);
    if (rc != TIG_OK) {
        return rc;
    }

    if (!tig_mouse_cursor_create_video_buffers(art_id, 0, 0)) {
        tig_mouse_device_exit();
        return TIG_ERR_16;
    }

    if (!tig_mouse_cursor_set_art_frame(art_id, 0, 0)) {
        tig_mouse_cursor_destroy_video_buffers();
        tig_mouse_device_exit();
        return TIG_ERR_16;
    }

    dword_6046E8 = 0;
    tig_mouse_initialized = true;
    tig_mouse_is_hardware = false;

    tig_mouse_buttons_swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;
    if (tig_mouse_buttons_swapped) {
        int temp;

        temp = tig_mouse_state_button_down_flags[TIG_MOUSE_BUTTON_RIGHT];
        tig_mouse_state_button_down_flags[TIG_MOUSE_BUTTON_RIGHT] = tig_mouse_state_button_down_flags[TIG_MOUSE_BUTTON_LEFT];
        tig_mouse_state_button_down_flags[TIG_MOUSE_BUTTON_LEFT] = temp;

        temp = tig_mouse_state_button_down_repeat_flags[TIG_MOUSE_BUTTON_RIGHT];
        tig_mouse_state_button_down_repeat_flags[TIG_MOUSE_BUTTON_RIGHT] = tig_mouse_state_button_down_repeat_flags[TIG_MOUSE_BUTTON_LEFT];
        tig_mouse_state_button_down_repeat_flags[TIG_MOUSE_BUTTON_LEFT] = temp;

        temp = tig_mouse_state_button_up_flags[TIG_MOUSE_BUTTON_RIGHT];
        tig_mouse_state_button_up_flags[TIG_MOUSE_BUTTON_RIGHT] = tig_mouse_state_button_up_flags[TIG_MOUSE_BUTTON_LEFT];
        tig_mouse_state_button_up_flags[TIG_MOUSE_BUTTON_LEFT] = temp;

        temp = TIG_MESSAGE_MOUSE_button_down_flags[TIG_MOUSE_BUTTON_RIGHT];
        TIG_MESSAGE_MOUSE_button_down_flags[TIG_MOUSE_BUTTON_RIGHT] = TIG_MESSAGE_MOUSE_button_down_flags[TIG_MOUSE_BUTTON_LEFT];
        TIG_MESSAGE_MOUSE_button_down_flags[TIG_MOUSE_BUTTON_LEFT] = temp;

        temp = TIG_MESSAGE_MOUSE_button_up_flags[TIG_MOUSE_BUTTON_RIGHT];
        TIG_MESSAGE_MOUSE_button_up_flags[TIG_MOUSE_BUTTON_RIGHT] = TIG_MESSAGE_MOUSE_button_up_flags[TIG_MOUSE_BUTTON_LEFT];
        TIG_MESSAGE_MOUSE_button_up_flags[TIG_MOUSE_BUTTON_LEFT] = temp;
    }

    return TIG_OK;
}

// 0x4FF2A0
int tig_mouse_hardware_init()
{
    dword_6046E8 = 0;
    tig_mouse_state.frame.height = 0;
    tig_mouse_state.frame.width = 0;
    tig_mouse_initialized = true;
    tig_mouse_is_hardware = true;

    return TIG_OK;
}

// 0x4FF2D0
void tig_mouse_exit()
{
    if (tig_mouse_initialized) {
        if (tig_mouse_is_hardware) {
            tig_mouse_hardware_exit();
        } else {
            tig_mouse_device_exit();
            tig_mouse_cursor_destroy_video_buffers();
        }
        tig_mouse_initialized = false;
    }
}

// 0x4FF310
void tig_mouse_device_exit()
{
    if (tig_mouse_device != NULL) {
        tig_mouse_set_active(false);
        IDirectInputDevice_Release(tig_mouse_device);
        tig_mouse_device = NULL;
    }
}

// 0x4FF340
void tig_mouse_hardware_exit()
{
}

// 0x4FF350
void tig_mouse_set_active(bool is_active)
{
    if (tig_mouse_device != NULL) {
        if (is_active) {
            IDirectInputDevice_Acquire(tig_mouse_device);
            tig_mouse_active = is_active;
        } else {
            IDirectInputDevice_Unacquire(tig_mouse_device);
            tig_mouse_active = is_active;
        }
    } else {
        tig_mouse_active = is_active;
    }
}

// NOTE: Rare case among `ping` functions - returns `bool`. Other functions are
// usually `void`. This value is never used by the callers.
//
// 0x4FF390
bool tig_mouse_ping()
{
    if (!tig_mouse_active) {
        return false;
    }

    if (tig_mouse_is_hardware) {
        return true;
    }

    return tig_mouse_update_state();
}

// 0x4FF3B0
void tig_mouse_set_position(int x, int y, int z)
{
    TigMessage message;

    // NOTE: This is meaningless since this value is only used in DirectInput
    // mode.
    tig_mouse_idle_emitted = false;

    tig_mouse_state.x = x;
    tig_mouse_state.y = y;

    tig_timer_now(&(message.timestamp));
    message.type = TIG_MESSAGE_MOUSE;
    message.data.mouse.x = x;
    message.data.mouse.y = y;
    message.data.mouse.z = z;
    message.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
    message.data.mouse.repeat = false;
    tig_message_enqueue(&message);
}

// 0x4FF410
void tig_mouse_set_button(int button, bool pressed)
{
    TigMessage message;
    unsigned int flags;

    flags = tig_mouse_state.flags;
    tig_mouse_state.flags = 0;

    message.type = TIG_MESSAGE_MOUSE;

    if (pressed) {
        // Mouse button is currently pressed. Check previous state to see if its
        // a "long press".
        if (((tig_mouse_state_button_down_repeat_flags[button] | tig_mouse_state_button_down_flags[button]) & flags) != 0) {
            tig_mouse_state.flags |= tig_mouse_state_button_down_repeat_flags[button];

            // Check if we're keeping it pressed long enough to emit "button
            // down" event again.
            if (tig_timer_between(tig_mouse_hardware_button_timestamps[button], tig_ping_timestamp) > 250) {
                tig_mouse_hardware_button_timestamps[button] = tig_ping_timestamp;

                tig_mouse_state.flags |= tig_mouse_state_button_down_flags[button];

                // Emit "button down" event.
                message.timestamp = tig_ping_timestamp;
                message.data.mouse.x = tig_mouse_state.x;
                message.data.mouse.y = tig_mouse_state.y;
                message.data.mouse.z = 0;
                message.data.mouse.event = TIG_MESSAGE_MOUSE_button_down_flags[button];
                message.data.mouse.repeat = true;
                tig_message_enqueue(&message);
            }
        } else {
            tig_mouse_hardware_button_timestamps[button] = tig_ping_timestamp;

            tig_mouse_state.flags |= tig_mouse_state_button_down_flags[button];

            // Emit "button down" event.
            message.timestamp = tig_ping_timestamp;
            message.data.mouse.x = tig_mouse_state.x;
            message.data.mouse.y = tig_mouse_state.y;
            message.data.mouse.z = 0;
            message.data.mouse.event = TIG_MESSAGE_MOUSE_button_down_flags[button];
            message.data.mouse.repeat = false;
            tig_message_enqueue(&message);
        }
    } else {
        // Mouse button is not currently pressed. Check previous state to see if
        // "button up" event is needed.
        if (((tig_mouse_state_button_down_flags[button] | tig_mouse_state_button_down_repeat_flags[button]) & flags) != 0) {
            tig_mouse_hardware_button_timestamps[button] = tig_ping_timestamp;

            tig_mouse_state.flags |= tig_mouse_state_button_up_flags[button];

            // Emit "button up" event.
            message.timestamp = tig_ping_timestamp;
            message.data.mouse.x = tig_mouse_state.x;
            message.data.mouse.y = tig_mouse_state.y;
            message.data.mouse.z = 0;
            message.data.mouse.event = TIG_MESSAGE_MOUSE_button_up_flags[button];
            message.data.mouse.repeat = false;
            tig_message_enqueue(&message);
        }
    }

    if ((flags & TIG_MOUSE_STATE_HIDDEN) != 0) {
        tig_mouse_state.flags |= TIG_MOUSE_STATE_HIDDEN;
    }
}

// 0x4FF5A0
bool tig_mouse_update_state()
{
    TigMessage message;
    unsigned int flags;
    int button;

    message.type = TIG_MESSAGE_MOUSE;
    message.timestamp = tig_ping_timestamp;

    if (tig_mouse_cursor_art_num_frames > 1) {
        if (tig_timer_between(tig_mouse_cursor_art_animation_timestamp, tig_ping_timestamp) >= tig_mouse_cursor_art_fps) {
            tig_mouse_cursor_animate();
            tig_mouse_cursor_art_animation_timestamp = tig_ping_timestamp;
        }
    }

    if (tig_timer_between(tig_mouse_update_timestamp, tig_ping_timestamp) < 0) {
        return false;
    }

    tig_mouse_update_timestamp = tig_ping_timestamp;

    if (!tig_mouse_device_update_state()) {
        return false;
    }

    // Update cursor rect.
    if (tig_mouse_device_state.lX != 0 || tig_mouse_device_state.lY != 0) {
        // Mark previous cursor rect as dirty.
        tig_window_set_needs_display_in_rect(&(tig_mouse_state.frame));

        if (tig_mouse_device_state.lX != 0) {
            tig_mouse_state.frame.x += tig_mouse_device_state.lX;

            if (tig_mouse_state.frame.x < -tig_mouse_state.offset_x) {
                tig_mouse_state.frame.x = -tig_mouse_state.offset_x;
            } else if (tig_mouse_state.frame.x > tig_mouse_max_x - tig_mouse_state.offset_x) {
                tig_mouse_state.frame.x = tig_mouse_max_x - tig_mouse_state.offset_x;
            }

            tig_mouse_state.x = tig_mouse_state.frame.x + tig_mouse_state.offset_x;
        }

        if (tig_mouse_device_state.lY != 0) {
            tig_mouse_state.frame.y += tig_mouse_device_state.lY;

            if (tig_mouse_state.frame.y < -tig_mouse_state.offset_y) {
                tig_mouse_state.frame.y = -tig_mouse_state.offset_y;
            } else if (tig_mouse_state.frame.y > tig_mouse_max_y - tig_mouse_state.offset_y) {
                tig_mouse_state.frame.y = tig_mouse_max_y - tig_mouse_state.offset_y;
            }

            tig_mouse_state.y = tig_mouse_state.frame.y + tig_mouse_state.offset_y;
        }
    }

    // Process mouse movement.
    if (tig_mouse_device_state.lX != 0 || tig_mouse_device_state.lY != 0) {
        tig_mouse_move_timestamp = tig_ping_timestamp;
        tig_mouse_idle_emitted = false;

        // Emit "move" event.
        message.data.mouse.x = tig_mouse_state.x;
        message.data.mouse.y = tig_mouse_state.y;
        message.data.mouse.z = tig_mouse_device_state.lZ;
        message.data.mouse.event = TIG_MESSAGE_MOUSE_MOVE;
        tig_message_enqueue(&message);

        // Mark current cursor as dirty.
        tig_window_set_needs_display_in_rect(&(tig_mouse_state.frame));
    } else {
        if (!tig_mouse_idle_emitted && tig_timer_between(tig_mouse_move_timestamp, tig_ping_timestamp) > 35) {
            // Mark this move sequence as ended so we don't spam "idle" events.
            tig_mouse_idle_emitted = true;

            // Emit "idle" event.
            message.data.mouse.x = tig_mouse_state.x;
            message.data.mouse.y = tig_mouse_state.y;
            message.data.mouse.z = tig_mouse_device_state.lZ;
            message.data.mouse.event = TIG_MESSAGE_MOUSE_IDLE;
            tig_message_enqueue(&message);

            // Mark current cursor as dirty.
            tig_window_set_needs_display_in_rect(&(tig_mouse_state.frame));
        }
    }

    if (tig_mouse_z_axis_enabled) {
        if (tig_mouse_device_state.lZ != 0) {
            // Emit "wheel" event.
            message.data.mouse.x = tig_mouse_state.x;
            message.data.mouse.y = tig_mouse_state.y;
            message.data.mouse.z = tig_mouse_device_state.lZ;
            message.data.mouse.event = TIG_MESSAGE_MOUSE_WHEEL;
            tig_message_enqueue(&message);
        }
    } else {
        if (tig_mouse_device_state.lZ != tig_mouse_state.z) {
            // Emit "wheel" event.
            message.data.mouse.x = tig_mouse_state.x;
            message.data.mouse.y = tig_mouse_state.y;
            message.data.mouse.z = tig_mouse_device_state.lZ - tig_mouse_state.z;
            message.data.mouse.event = TIG_MESSAGE_MOUSE_WHEEL;
            tig_message_enqueue(&message);
            tig_mouse_state.z = tig_mouse_device_state.lZ;
        }
    }

    // Process buttons state.
    flags = tig_mouse_state.flags;
    tig_mouse_state.flags = 0;

    // NOTE: This loop is very similar to `tig_mouse_set_button` implementation,
    // but it uses different timestamp arrays.
    for (button = 0; button < TIG_MOUSE_BUTTON_COUNT; button++) {
        if ((tig_mouse_device_state.rgbButtons[button] & 0x80) != 0) {
            // Mouse button is currently pressed. Check previous state to see
            // if it a "long press".
            if (((tig_mouse_state_button_down_flags[button] | tig_mouse_state_button_down_repeat_flags[button]) & flags) != 0) {
                tig_mouse_state.flags |= tig_mouse_state_button_down_repeat_flags[button];

                // Check if we're keeping it pressed long enough to emit "button
                // down" event again.
                if (tig_timer_between(tig_mouse_software_button_timestamps[button], tig_ping_timestamp) > 250) {
                    tig_mouse_software_button_timestamps[button] = tig_ping_timestamp;

                    tig_mouse_state.flags |= tig_mouse_state_button_down_flags[button];

                    // Emit "button down" event.
                    message.timestamp = tig_ping_timestamp;
                    message.data.mouse.x = tig_mouse_state.x;
                    message.data.mouse.y = tig_mouse_state.y;
                    message.data.mouse.z = tig_mouse_device_state.lZ;
                    message.data.mouse.event = TIG_MESSAGE_MOUSE_button_down_flags[button];
                    message.data.mouse.repeat = true;
                    tig_message_enqueue(&message);
                }
            } else {
                tig_mouse_software_button_timestamps[button] = tig_ping_timestamp;

                tig_mouse_state.flags |= tig_mouse_state_button_down_flags[button];

                // Emit "button down" event.
                message.timestamp = tig_ping_timestamp;
                message.data.mouse.x = tig_mouse_state.x;
                message.data.mouse.y = tig_mouse_state.y;
                message.data.mouse.z = tig_mouse_device_state.lZ;
                message.data.mouse.event = TIG_MESSAGE_MOUSE_button_down_flags[button];
                message.data.mouse.repeat = false;
                tig_message_enqueue(&message);
            }
        } else {
            // Mouse button is not currently pressed. Check previous state to
            // see if "button up" event is needed.
            if (((tig_mouse_state_button_down_flags[button] | tig_mouse_state_button_down_repeat_flags[button]) & flags) != 0) {
                tig_mouse_software_button_timestamps[button] = tig_ping_timestamp;

                tig_mouse_state.flags |= tig_mouse_state_button_up_flags[button];

                // Emit "button up" event.
                message.timestamp = tig_ping_timestamp;
                message.data.mouse.x = tig_mouse_state.x;
                message.data.mouse.y = tig_mouse_state.y;
                message.data.mouse.z = tig_mouse_device_state.lZ;
                message.data.mouse.event = TIG_MESSAGE_MOUSE_button_up_flags[button];
                message.data.mouse.repeat = false;
                tig_message_enqueue(&message);
            }
        }
    }

    if ((flags & TIG_MOUSE_STATE_HIDDEN) != 0) {
        tig_mouse_state.flags |= TIG_MOUSE_STATE_HIDDEN;
    }

    return true;
}

// 0x4FF9B0
bool tig_mouse_device_update_state()
{
    HRESULT hr;

    if (tig_mouse_device == NULL) {
        return false;
    }

    tig_mouse_set_active(true);

    hr = IDirectInputDevice_GetDeviceState(tig_mouse_device,
        sizeof(tig_mouse_device_state),
        &tig_mouse_device_state);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

// 0x4FF9F0
int tig_mouse_get_state(TigMouseState* mouse_state)
{
    if (!tig_mouse_initialized) {
        return TIG_NOT_INITIALIZED;
    }

    tig_mouse_ping();

    memcpy(mouse_state, &tig_mouse_state, sizeof(TigMouseState));
    mouse_state->flags = tig_mouse_state.flags & TIG_MOUSE_STATE_HIDDEN;

    return TIG_OK;
}

// 0x4FFA30
int tig_mouse_hide()
{
    if ((tig_mouse_state.flags & TIG_MOUSE_STATE_HIDDEN) == 0) {
        tig_mouse_state.flags |= TIG_MOUSE_STATE_HIDDEN;
        if (!tig_mouse_is_hardware) {
            tig_window_set_needs_display_in_rect(&(tig_mouse_state.frame));
        }
    }

    return TIG_OK;
}

// 0x4FFA70
int tig_mouse_show()
{
    if ((tig_mouse_state.flags & TIG_MOUSE_STATE_HIDDEN) != 0) {
        tig_mouse_state.flags &= ~TIG_MOUSE_STATE_HIDDEN;
        if (!tig_mouse_is_hardware) {
            tig_window_set_needs_display_in_rect(&(tig_mouse_state.frame));
        }
    }

    return TIG_OK;
}

// 0x4FFAB0
void tig_mouse_display()
{
    TigVideoBufferBlitInfo vb_blit_info;

    if (tig_mouse_is_hardware) {
        return;
    }

    if ((tig_mouse_state.flags & TIG_MOUSE_STATE_HIDDEN) != 0) {
        return;
    }

    // Copy area under cursor.
    sub_51D050(&(tig_mouse_state.frame),
        NULL,
        tig_mouse_cursor_opaque_video_buffer,
        0,
        0,
        TIG_WINDOW_TOP);

    // Blit cursor over background.
    vb_blit_info.flags = 0;
    vb_blit_info.src_video_buffer = tig_mouse_cursor_trans_video_buffer;
    vb_blit_info.src_rect = &tig_mouse_cursor_art_frame_bounds;
    vb_blit_info.dst_video_buffer = tig_mouse_cursor_opaque_video_buffer;
    vb_blit_info.dst_rect = &tig_mouse_cursor_art_frame_bounds;
    tig_video_buffer_blit(&vb_blit_info);

    // Blit composed cursor/background to screen.
    tig_video_blit(tig_mouse_cursor_opaque_video_buffer,
        &tig_mouse_cursor_art_frame_bounds,
        &(tig_mouse_state.frame),
        false);
}

// 0x4FFB40
void tig_mouse_cursor_refresh()
{
    TigRect src_rect;
    TigRect dst_rect;
    TigArtBlitInfo blit_info;

    if (tig_mouse_is_hardware) {
        return;
    }

    // Clear surface.
    tig_video_buffer_fill(tig_mouse_cursor_trans_video_buffer,
        &tig_mouse_cursor_art_frame_bounds,
        tig_mouse_cursor_art_color_key);

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = tig_mouse_cursor_art_frame_width;
    src_rect.height = tig_mouse_cursor_art_frame_height;

    dst_rect.x = tig_mouse_cursor_art_x;
    dst_rect.y = tig_mouse_cursor_art_y;
    dst_rect.width = tig_mouse_cursor_art_frame_width;
    dst_rect.height = tig_mouse_cursor_art_frame_height;

    blit_info.flags = 0;
    blit_info.art_id = tig_mouse_cursor_art_id;
    blit_info.src_rect = &src_rect;
    blit_info.dst_video_buffer = tig_mouse_cursor_trans_video_buffer;
    blit_info.dst_rect = &dst_rect;

    if (tig_art_blit(&blit_info) != TIG_OK) {
        // There was an error during cursor blitting which should not normally
        // happen. In this case a special fallback mouse cursor is used - 1px
        // white dot.
        tig_mouse_cursor_fallback();
    }
}

// 0x4FFBF0
void tig_mouse_cursor_fallback()
{
    if (tig_video_buffer_lock(tig_mouse_cursor_trans_video_buffer) == TIG_OK) {
        TigVideoBufferData video_buffer_data;
        if (tig_video_buffer_data(tig_mouse_cursor_trans_video_buffer, &video_buffer_data) == TIG_OK) {
            switch (video_buffer_data.bpp) {
            case 8:
                *video_buffer_data.surface_data.p8 = (uint8_t)tig_color_make(255, 255, 255);
                break;
            case 16:
                *video_buffer_data.surface_data.p16 = (uint16_t)tig_color_make(255, 255, 255);
                break;
            case 24:
            case 32:
                *video_buffer_data.surface_data.p32 = (uint32_t)tig_color_make(255, 255, 255);
                break;
            }
        }
        tig_video_buffer_unlock(tig_mouse_cursor_trans_video_buffer);
    }
}

// 0x4FFD60
bool tig_mouse_cursor_create_video_buffers(tig_art_id_t art_id, int dx, int dy)
{
    TigArtAnimData art_anim_data;
    int width;
    int height;
    TigVideoBufferCreateInfo vb_create_info;

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        return false;
    }

    if (tig_art_size(art_id, &width, &height) != TIG_OK) {
        return false;
    }

    width += dx;
    height += dy;

    vb_create_info.width = width;
    vb_create_info.height = height;
    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_VIDEO_MEMORY;
    vb_create_info.background_color = tig_color_make(0, 0, 0);
    if (tig_video_buffer_create(&vb_create_info, &tig_mouse_cursor_opaque_video_buffer) != TIG_OK) {
        return false;
    }

    vb_create_info.width = width;
    vb_create_info.height = height;
    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_VIDEO_MEMORY | TIG_VIDEO_BUFFER_CREATE_COLOR_KEY;
    vb_create_info.background_color = art_anim_data.color_key;
    vb_create_info.color_key = art_anim_data.color_key;
    if (tig_video_buffer_create(&vb_create_info, &tig_mouse_cursor_trans_video_buffer) != TIG_OK) {
        tig_video_buffer_destroy(tig_mouse_cursor_opaque_video_buffer);
        return false;
    }

    tig_mouse_cursor_bounds.x = 0;
    tig_mouse_cursor_bounds.y = 0;
    tig_mouse_cursor_bounds.width = width;
    tig_mouse_cursor_bounds.height = height;

    return true;
}

// 0x4FFEA0
bool tig_mouse_cursor_destroy_video_buffers()
{
    tig_video_buffer_destroy(tig_mouse_cursor_opaque_video_buffer);
    tig_video_buffer_destroy(tig_mouse_cursor_trans_video_buffer);
    return true;
}

// 0x4FFEC0
bool tig_mouse_cursor_set_art_frame(tig_art_id_t art_id, int x, int y)
{
    TigArtFrameData art_frame_data;
    TigArtAnimData art_anim_data;

    if (tig_art_frame_data(art_id, &art_frame_data) != TIG_OK) {
        return false;
    }

    if (tig_art_anim_data(art_id, &art_anim_data) != TIG_OK) {
        return false;
    }

    tig_mouse_cursor_art_id = art_id;
    tig_mouse_cursor_art_x = x;
    tig_mouse_cursor_art_y = y;
    tig_mouse_cursor_art_frame_width = art_frame_data.width;
    tig_mouse_cursor_art_frame_height = art_frame_data.height;
    tig_mouse_cursor_art_num_frames = art_anim_data.num_frames;
    tig_mouse_cursor_art_fps = 1000 / art_anim_data.fps;
    tig_mouse_cursor_art_color_key = art_anim_data.color_key;

    tig_mouse_state.offset_x = x + art_frame_data.hot_x;
    tig_mouse_state.offset_y = y + art_frame_data.hot_y;
    tig_mouse_state.frame.x = tig_mouse_state.x - tig_mouse_state.offset_x;
    tig_mouse_state.frame.y = tig_mouse_state.y - tig_mouse_state.offset_y;
    tig_mouse_state.frame.width = x + art_frame_data.width;
    tig_mouse_state.frame.height = y + art_frame_data.height;

    tig_mouse_cursor_art_frame_bounds.x = 0;
    tig_mouse_cursor_art_frame_bounds.y = 0;
    tig_mouse_cursor_art_frame_bounds.width = x + art_frame_data.width;
    tig_mouse_cursor_art_frame_bounds.height = y + art_frame_data.height;

    tig_video_buffer_set_color_key(tig_mouse_cursor_trans_video_buffer,
        tig_mouse_cursor_art_color_key);

    tig_mouse_cursor_refresh();

    return true;
}

// 0x4FFFE0
int tig_mouse_cursor_set_art_id(tig_art_id_t art_id)
{
    if (!tig_mouse_is_hardware) {
        int width;
        int height;
        int rc = tig_art_size(art_id, &width, &height);
        if (rc != TIG_OK) {
            return rc;
        }

        bool hidden = (tig_mouse_state.flags & TIG_MOUSE_STATE_HIDDEN) != 0;
        if (!hidden) {
            tig_mouse_hide();
        }

        if (width > tig_mouse_cursor_bounds.width || height > tig_mouse_cursor_bounds.height) {
            TigVideoBuffer* old_opaque_video_buffer = tig_mouse_cursor_opaque_video_buffer;
            TigVideoBuffer* old_trans_video_buffer = tig_mouse_cursor_trans_video_buffer;

            if (!tig_mouse_cursor_create_video_buffers(art_id, 0, 0)) {
                tig_mouse_cursor_opaque_video_buffer = old_opaque_video_buffer;
                tig_mouse_cursor_trans_video_buffer = old_trans_video_buffer;

                if (!hidden) {
                    tig_mouse_show();
                }

                return TIG_ERR_16;
            }

            if (!tig_mouse_cursor_set_art_frame(art_id, 0, 0)) {
                tig_mouse_cursor_destroy_video_buffers();

                tig_mouse_cursor_opaque_video_buffer = old_opaque_video_buffer;
                tig_mouse_cursor_trans_video_buffer = old_trans_video_buffer;

                if (!hidden) {
                    tig_mouse_show();
                }

                return TIG_ERR_16;
            }

            tig_video_buffer_destroy(old_opaque_video_buffer);
            tig_video_buffer_destroy(old_trans_video_buffer);
        } else {
            if (!tig_mouse_cursor_set_art_frame(art_id, 0, 0)) {
                if (!hidden) {
                    tig_mouse_show();
                }

                return TIG_ERR_16;
            }
        }

        if (!hidden) {
            tig_mouse_show();
        }
    }

    return TIG_OK;
}

// 0x5000F0
void tig_mouse_cursor_set_offset(int x, int y)
{
    bool hidden;

    if (tig_mouse_state.offset_x != x || tig_mouse_state.offset_y != y) {
        hidden = (tig_mouse_state.flags & TIG_MOUSE_STATE_HIDDEN) != 0;
        if (!hidden) {
            tig_mouse_hide();
        }

        tig_mouse_state.offset_x = x;
        tig_mouse_state.offset_y = y;
        tig_mouse_state.frame.x = tig_mouse_state.x - x;
        tig_mouse_state.frame.y = tig_mouse_state.y - y;

        if (!hidden) {
            tig_mouse_show();
        }
    }
}

// 0x500150
tig_art_id_t tig_mouse_cursor_get_art_id()
{
    return tig_mouse_cursor_art_id;
}

// 0x500160
int tig_mouse_cursor_overlay(tig_art_id_t art_id, int x, int y)
{
    bool hidden;
    int rc;
    TigArtFrameData art_frame_data;
    TigRect src_rect;
    TigRect dst_rect;
    TigArtAnimData art_anim_data;
    TigArtBlitInfo blit_info;
    int dy;
    int dx;
    int height;
    int width;

    hidden = (tig_mouse_state.flags & TIG_MOUSE_STATE_HIDDEN) != 0;
    if (!hidden) {
        tig_mouse_hide();
    }

    rc = tig_art_frame_data(art_id, &art_frame_data);
    if (rc != TIG_OK) {
        // FIXME: Visibility is not restored.
        return rc;
    }

    src_rect.x = x;
    src_rect.y = y;
    src_rect.width = art_frame_data.width;
    src_rect.height = art_frame_data.height;

    rc = tig_rect_intersection(&src_rect, &tig_mouse_cursor_bounds, &dst_rect);
    if (rc == TIG_OK
        && dst_rect.x == x
        && dst_rect.y == y
        && dst_rect.width == art_frame_data.width
        && dst_rect.height == art_frame_data.height) {
        // Overlay perfectly fits inside already available video buffer.
        dy = 0;
        dx = 0;
        width = dst_rect.x + dst_rect.width - tig_mouse_cursor_art_frame_bounds.width;
        if (width < 0) {
            width = 0;
        }
        height = dst_rect.y + dst_rect.height - tig_mouse_cursor_art_frame_bounds.height;
        if (height < 0) {
            height = 0;
        }
        if (width > 0 && height > 0) {
            rc = tig_art_anim_data(tig_mouse_cursor_art_id, &art_anim_data);
            if (rc != TIG_OK) {
                if (!hidden) {
                    tig_mouse_show();
                }
                return TIG_ERR_16;
            }

            if (width != 0) {
                dst_rect.x = tig_mouse_cursor_art_frame_bounds.width;
                dst_rect.y = 0;
                dst_rect.width = width;
                dst_rect.height = width + tig_mouse_cursor_art_frame_bounds.height;
                tig_video_buffer_fill(tig_mouse_cursor_trans_video_buffer,
                    &dst_rect,
                    art_anim_data.color_key);
            }

            if (height != 0) {
                dst_rect.x = 0;
                dst_rect.y = tig_mouse_cursor_art_frame_bounds.height;
                dst_rect.width = width + tig_mouse_cursor_art_frame_bounds.width;
                dst_rect.height = height;
                tig_video_buffer_fill(tig_mouse_cursor_trans_video_buffer,
                    &dst_rect,
                    art_anim_data.color_key);
            }
        }
    } else {
        TigVideoBuffer* old_opaque_video_buffer = tig_mouse_cursor_opaque_video_buffer;
        TigVideoBuffer* old_trans_video_buffer = tig_mouse_cursor_trans_video_buffer;
        int extra_width;
        int extra_height;

        if (rc == TIG_OK) {
            if (src_rect.x + src_rect.width > 0) {
                dx = 0;
            } else {
                dx = -src_rect.x;
            }

            if (src_rect.y + src_rect.height > 0) {
                dy = 0;
            } else {
                dy = -src_rect.y;
            }

            if (src_rect.x + src_rect.width < tig_mouse_cursor_bounds.width) {
                extra_width = dx;
            } else {
                extra_width = src_rect.x + src_rect.width + dx - tig_mouse_cursor_bounds.width;
            }

            if (src_rect.y + src_rect.height < tig_mouse_cursor_bounds.height) {
                extra_height = dy;
            } else {
                extra_height = src_rect.y + src_rect.height + dy - tig_mouse_cursor_bounds.height;
            }
        } else {
            dx = dst_rect.x - x;
            dy = dst_rect.y - y;
            extra_width = art_frame_data.width + dst_rect.x - x - dst_rect.width;
            extra_height = art_frame_data.height + dst_rect.y - y - dst_rect.height;
        }

        rc = tig_art_frame_data(tig_mouse_cursor_art_id, &art_frame_data);
        if (rc != TIG_OK) {
            // FIXME: Visibility is not restored.
            return rc;
        }

        width = tig_mouse_cursor_bounds.width - art_frame_data.width + extra_width;
        height = tig_mouse_cursor_bounds.height - art_frame_data.height + extra_height;

        if (!tig_mouse_cursor_create_video_buffers(tig_mouse_cursor_art_id, width, height)) {
            tig_mouse_cursor_opaque_video_buffer = old_opaque_video_buffer;
            tig_mouse_cursor_trans_video_buffer = old_trans_video_buffer;

            if (!hidden) {
                tig_mouse_show();
            }
            return TIG_ERR_16;
        }

        if (!tig_mouse_cursor_set_art_frame(tig_mouse_cursor_art_id, dx, dy)) {
            tig_mouse_cursor_destroy_video_buffers();

            tig_mouse_cursor_opaque_video_buffer = old_opaque_video_buffer;
            tig_mouse_cursor_trans_video_buffer = old_trans_video_buffer;

            if (!hidden) {
                tig_mouse_show();
            }
            return TIG_ERR_16;
        }

        tig_video_buffer_destroy(old_opaque_video_buffer);
        tig_video_buffer_destroy(old_trans_video_buffer);
    }

    dst_rect.x = dx + src_rect.x;
    dst_rect.y = dy + src_rect.y;
    dst_rect.width = src_rect.width;
    dst_rect.height = src_rect.height;

    src_rect.x = 0;
    src_rect.y = 0;

    blit_info.flags = 0;
    blit_info.art_id = art_id;
    blit_info.src_rect = &src_rect;
    blit_info.dst_rect = &dst_rect;
    blit_info.dst_video_buffer = tig_mouse_cursor_trans_video_buffer;

    rc = tig_art_blit(&blit_info);
    if (rc == TIG_OK) {
        width -= dx;
        height -= dy;

        tig_mouse_cursor_art_frame_bounds.width += width;
        tig_mouse_cursor_art_frame_bounds.height += height;

        tig_mouse_state.frame.width += width;
        tig_mouse_state.frame.height += height;
    }

    if (!hidden) {
        tig_mouse_show();
    }

    return rc;
}

// 0x500520
void tig_mouse_cursor_animate()
{
    if (!tig_mouse_is_hardware) {
        tig_window_set_needs_display_in_rect(&(tig_mouse_state.frame));
        tig_mouse_cursor_art_id = tig_art_id_frame_inc(tig_mouse_cursor_art_id);
        tig_mouse_cursor_set_art_frame(tig_mouse_cursor_art_id, 0, 0);
        tig_window_set_needs_display_in_rect(&(tig_mouse_state.frame));
    }
}

// 0x500560
int sub_500560()
{
    return 0;
}

// 0x500570
void sub_500570()
{
}

// 0x500580
void tig_mouse_set_z_axis_enabled(bool enabled)
{
    tig_mouse_z_axis_enabled = enabled;
}
