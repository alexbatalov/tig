#include "tig/kb.h"

#include "tig/core.h"
#include "tig/debug.h"
#include "tig/dxinput.h"
#include "tig/message.h"
#include "tig/video.h"

#define KEYBOARD_DEVICE_DATA_CAPACITY 64

static bool tig_kb_device_init();
static void tig_kb_device_exit();

// 0x62AC90
static bool tig_kb_caps_lock_state;

// 0x62AC94
static bool tig_kb_scroll_lock_state;

// 0x62AC98
static DIDEVICEOBJECTDATA tig_kb_keyboard_buffer[KEYBOARD_DEVICE_DATA_CAPACITY];

// 0x62B098
static bool tig_kb_num_lock_state;

// 0x62B09C
static unsigned char tig_kb_state[256];

// 0x62B19C
static LPDIRECTINPUTDEVICEA tig_kb_device;

// 0x62B1A0
static bool tig_kb_initialized;

// 0x62B1A8
int dword_62B1A8[16];

// 0x52B2F0
int tig_kb_init(TigInitInfo* init_info)
{
    (void)init_info;

    if (tig_kb_initialized) {
        return TIG_ALREADY_INITIALIZED;
    }

    if (!tig_kb_device_init()) {
        return TIG_ERR_7;
    }

    tig_kb_initialized = true;

    return TIG_OK;
}

// 0x52B320
void tig_kb_exit()
{
    if (tig_kb_initialized) {
        tig_kb_device_exit();
        tig_kb_initialized = false;
    }
}

// 0x52B340
bool tig_kb_is_key_pressed(int key)
{
    return tig_kb_state[key] & 0x80;
}

// 0x52B350
bool tig_kb_get_modifier(int key)
{
    switch (key) {
    case DIK_CAPITAL:
        return tig_kb_caps_lock_state;
    case DIK_SCROLL:
        return tig_kb_scroll_lock_state;
    case DIK_NUMLOCK:
        return tig_kb_num_lock_state;
    }

    return false;
}

// 0x52B380
void tig_kb_ping()
{
    TigMessage message;
    DWORD count;
    DWORD index;
    HRESULT hr;

    if (!tig_kb_initialized) {
        return;
    }

    if (!tig_get_active()) {
        return;
    }

    message.timestamp = tig_ping_timestamp;
    message.type = TIG_MESSAGE_KEYBOARD;

    count = KEYBOARD_DEVICE_DATA_CAPACITY;
    hr = IDirectInputDevice_GetDeviceData(tig_kb_device, sizeof(*tig_kb_keyboard_buffer), tig_kb_keyboard_buffer, &count, 0);
    if (FAILED(hr)) {
        unsigned char state[256];
        unsigned int key;

        do {
            hr = IDirectInputDevice_Acquire(tig_kb_device);
            if (FAILED(hr)) {
                return;
            }

            hr = IDirectInputDevice_GetDeviceState(tig_kb_device, 256, state);
            if (FAILED(hr)) {
                tig_debug_println("Error getting keyboard device state tig_kb_ping()\n");
                return;
            }

            // Keyboard state is out of sync, rewrite it to current state,
            // simulating "key up" events in the process.
            for (key = 0; key < 256; key++) {
                if (state[key] != tig_kb_state[key]
                    && (tig_kb_state[key] & 0x80) != 0) {
                    tig_kb_state[key] = state[key];
                    message.data.keyboard.key = key;
                    message.data.keyboard.pressed = 0;
                    tig_message_enqueue(&message);
                }
            }

            count = KEYBOARD_DEVICE_DATA_CAPACITY;
            hr = IDirectInputDevice_GetDeviceData(tig_kb_device, sizeof(*tig_kb_keyboard_buffer), tig_kb_keyboard_buffer, &count, 0);
        } while (FAILED(hr));
    }

    for (index = 0; index < count; index++) {
        unsigned int key = tig_kb_keyboard_buffer[index].dwOfs;
        unsigned char flags = (unsigned char)tig_kb_keyboard_buffer[index].dwData;
        if (tig_kb_state[key] != flags) {
            tig_kb_state[key] = flags;

            if ((key == DIK_LCONTROL || key == DIK_RCONTROL || (tig_kb_state[DIK_LCONTROL] == 0 && tig_kb_state[DIK_RCONTROL] == 0))
                && (key == DIK_LMENU || key == DIK_RMENU || (tig_kb_state[DIK_LMENU] == 0 && tig_kb_state[DIK_RMENU] == 0))) {
                message.data.keyboard.key = key;
                message.data.keyboard.pressed = flags != 0;
                tig_message_enqueue(&message);
            }

            // FIXED: Original code checks for `message.data.keyboard.pressed`,
            // but this is wrong if previous condition fails to set it (in this
            // case previous or even uninitialized value would be used).

            if (flags != 0) {
                // NOTE: Original code is slightly different, uses the
                // following pattern:
                //
                // ```c
                // state = 1 - state
                // ```
                switch (key) {
                case DIK_CAPITAL:
                    tig_kb_caps_lock_state = !tig_kb_caps_lock_state;
                    break;
                case DIK_SCROLL:
                    tig_kb_scroll_lock_state = !tig_kb_scroll_lock_state;
                    break;
                case DIK_NUMLOCK:
                    tig_kb_num_lock_state = !tig_kb_num_lock_state;
                    break;
                }
            }
        }
    }
}

// 0x52B580
bool tig_kb_device_init()
{
    LPDIRECTINPUTA direct_input;
    DIPROPDWORD dipdw;
    HWND wnd;
    HRESULT hr;

    tig_kb_caps_lock_state = (GetKeyState(VK_CAPITAL) & 1) != 0;
    tig_kb_scroll_lock_state = (GetKeyState(VK_SCROLL) & 1) != 0;
    tig_kb_num_lock_state = (GetKeyState(VK_NUMLOCK) & 1) != 0;

    if (tig_dxinput_get_instance(&direct_input) != TIG_OK) {
        return false;
    }

    hr = IDirectInput_CreateDevice(direct_input, &GUID_SysKeyboard, &tig_kb_device, NULL);
    if (FAILED(hr)) {
        return false;
    }

    hr = IDirectInputDevice_SetDataFormat(tig_kb_device, &c_dfDIKeyboard);
    if (FAILED(hr)) {
        tig_kb_device_exit();
        return false;
    }

    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = KEYBOARD_DEVICE_DATA_CAPACITY;

    hr = IDirectInputDevice_SetProperty(tig_kb_device, DIPROP_BUFFERSIZE, &(dipdw.diph));
    if (FAILED(hr)) {
        tig_kb_device_exit();
        return false;
    }

    if (tig_video_platform_window_get(&wnd) != TIG_OK) {
        tig_kb_device_exit();
        return false;
    }

    hr = IDirectInputDevice_SetCooperativeLevel(tig_kb_device, wnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
    if (FAILED(hr)) {
        tig_kb_device_exit();
        return false;
    }

    return true;
}

// 0x52B6A0
void tig_kb_device_exit()
{
    if (tig_kb_device != NULL) {
        IDirectInputDevice_Unacquire(tig_kb_device);
        IDirectInputDevice_Release(tig_kb_device);
        tig_kb_device = NULL;
    }
}
