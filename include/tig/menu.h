#ifndef TIG_MENU_H_
#define TIG_MENU_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TigMenuBarCreateInfo {
    /* 0000 */ tig_window_handle_t window_handle;
    /* 0004 */ int x;
    /* 0008 */ int y;
    /* 000C */ int num_drop_downs;
    /* 0010 */ const char** drop_downs;
    /* 0014 */ int* num_menu_items;
    /* 0018 */ const char** menu_items;
    /* 001C */ unsigned int* flags;
} TigMenuBarCreateInfo;

static_assert(sizeof(TigMenuBarCreateInfo) == 0x20, "wrong size");

typedef struct TigMenuColors {
    /* 0000 */ tig_color_t text_color;
    /* 0004 */ tig_color_t background_color;
    /* 0008 */ tig_color_t selected_text_color;
    /* 000C */ tig_color_t disabled_text_color;
    /* 0010 */ tig_color_t selected_menu_item_outline_color;
} TigMenuColors;

static_assert(sizeof(TigMenuColors) == 0x14, "wrong size");

// Initializes MENU subsystem.
int tig_menu_init(TigInitInfo* init_info);

// Shutdowns MENU subsystem.
void tig_menu_exit();

// Sets menu color scheme.
int tig_menu_set_colors(TigMenuColors* colors);

// Presents modal dialog to select item from a list.
//
// The select UI consists of title, a scrollable list of up to 10 visible items,
// scroll buttons (up and down), and a cancel button.
//
// Returns index of the selected menu item, or -1 if dialog was canceled.
//
// NOTE: Do not use, the original code (and this implementation) is bugged and
// can result in infinite loop.
int tig_menu_do_select(const char* title, const char** menu_items, int num_menu_items, int x, int y);

// Updates menu item flags in the specified menu bar.
int tig_menu_bar_set_flags(int menu_bar_handle, unsigned int* flags);

// Handles "click" event on button which belongs to MENU subsystem.
void tig_menu_bar_on_click(tig_button_handle_t button_handle);

// Presents modal context menu.
//
// Returns index of the selected menu item, or -1 if dialog was canceled.
//
// NOTE: Do not use, the original code (and this implementation) is bugged and
// can result in infinite loop.
int tig_menu_do_context_menu(const char** menu_items, int num_menu_items, int x, int y, unsigned int* flags);

// Creates a main menu in the specified window.
//
// NOTE: Do not use, the original code (and this implementation) is bugged and
// can result in infinite loop.
int tig_menu_bar_create(TigMenuBarCreateInfo* create_info);

#ifdef __cplusplus
}
#endif

#endif /* TIG_MENU_H_ */
