#ifndef TIG_BUTTON_H_
#define TIG_BUTTON_H_

#include "tig/message.h"
#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TIG_BUTTON_HANDLE_INVALID ((tig_button_handle_t)-1)

typedef enum TigButtonState {
    TIG_BUTTON_STATE_PRESSED,
    TIG_BUTTON_STATE_RELEASED,
    TIG_BUTTON_STATE_MOUSE_INSIDE,
    TIG_BUTTON_STATE_MOUSE_OUTSIDE,
} TigButtonState;

typedef enum TigButtonFlags {
    TIG_BUTTON_FLAG_0x01 = 1 << 0,
    TIG_BUTTON_FLAG_0x02 = 1 << 1,
    TIG_BUTTON_FLAG_0x04 = 1 << 2,
    TIG_BUTTON_FLAG_HIDDEN = 1 << 3,
    TIG_BUTTON_FLAG_0x10 = 1 << 4,
    TIG_BUTTON_MENU_BAR = 1 << 12,
} TigButtonFlags;

typedef struct TigButtonData {
    /* 0000 */ unsigned int flags; // See `TigButtonFlags`.
    /* 0004 */ tig_window_handle_t window_handle;
    /* 0008 */ int x; // In window coordinate system
    /* 000C */ int y; // In window coordinate system
    /* 0010 */ tig_art_id_t art_id;
    /* 0014 */ int width;
    /* 0018 */ int height;
    /* 001C */ int mouse_down_snd_id;
    /* 0020 */ int mouse_up_snd_id;
    /* 0024 */ int mouse_enter_snd_id;
    /* 0028 */ int mouse_exit_snd_id;
} TigButtonData;

static_assert(sizeof(TigButtonData) == 0x2C, "wrong size");

int tig_button_init(TigInitInfo* init_info);
void tig_button_exit();
int tig_button_create(TigButtonData* button_data, tig_button_handle_t* button_handle);
int tig_button_destroy(tig_button_handle_t button_handle);
int tig_button_data(tig_button_handle_t button_handle, TigButtonData* button_data);
int tig_button_refresh_rect(int window_handle, TigRect* rect);
void tig_button_state_change(tig_button_handle_t button_handle, int state);
int tig_button_state_get(tig_button_handle_t button_handle, int* state);
tig_button_handle_t tig_button_get_at_position(int x, int y);
bool tig_button_process_mouse_msg(TigMouseMessageData* mouse);
int tig_button_radio_group_create(int count, tig_button_handle_t* button_handles, int selected);
tig_button_handle_t sub_538730(tig_button_handle_t button_handle);
int tig_button_show(tig_button_handle_t button_handle);
int tig_button_hide(tig_button_handle_t button_handle);
int tig_button_is_hidden(tig_button_handle_t button_handle, bool* hidden);
int tig_button_show_force(tig_button_handle_t button_handle);
int tig_button_hide_force(tig_button_handle_t button_handle);
void tig_button_set_art(tig_button_handle_t button_handle, tig_art_id_t art_id);

#ifdef __cplusplus
}
#endif

#endif /* TIG_BUTTON_H_ */
