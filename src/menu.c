// MENU
// ---
//
// The MENU subsystem provides several widgets that resemble DOS style windows.
// These widgets could have been used in early iterations of Arcanum/TIG
// development. The game does not use any of the functions from this subsystem
// besides `init` and `exit` pair.
//
// In fact there is a couple of bugs which makes it useless:
//
// - The internal implementation of modal loops is wrong. The loops in question
// only call `tig_ping` that "pumps" internal message queue with TIG messages as
// well as Windows Messages. However in order to process them ("peek" phase) a
// loop with `tig_message_dequeue` is required. The correct implementation of
// internal event can be seen in `tig_window_modal_dialog`.
//
// - The width calculation in `tig_menu_drop_down_create` is wrong resulting in
// some menu items not being rendered (due to clipping).

#include "tig/menu.h"

#include "tig/art.h"
#include "tig/button.h"
#include "tig/color.h"
#include "tig/core.h"
#include "tig/font.h"
#include "tig/kb.h"
#include "tig/memory.h"
#include "tig/rect.h"
#include "tig/window.h"

// The maximum number of menu bars.
//
// NOTE: This number should likely be the same as `MAX_WINDOWS`.
#define MAX_MENU_BARS 50

// The maximum number of drop downs in menu bar.
#define MAX_DROP_DOWNS 10

typedef struct TigMenuDropDown {
    /* 0000 */ const char** menu_items;
    /* 0004 */ int num_menu_items;
    /* 0008 */ int item_width;
    /* 000C */ int item_height;
    /* 0010 */ tig_window_handle_t window_handle;
    /* 0014 */ TigRect frame;
    /* 0024 */ unsigned int* flags;
} TigMenuDropDown;

static_assert(sizeof(TigMenuDropDown) == 0x28, "wrong size");

// Indicates that menu bar is unused.
#define TIG_MENU_BAR_UNUSED 0x01

typedef struct TigMenuBar {
    /* 0000 */ unsigned int usage;
    /* 0004 */ tig_window_handle_t window_handle;
    /* 0008 */ TigRect frame;
    /* 0018 */ int num_drop_downs;
    /* 001C */ const char** drop_downs;
    /* 0020 */ int* num_menu_items;
    /* 0024 */ const char** menu_items;
    /* 0028 */ unsigned int* flags;
    /* 002C */ tig_button_handle_t* buttons;
} TigMenuBar;

static_assert(sizeof(TigMenuBar) == 0x30, "wrong size");

static bool sub_5395E0(TigMessage* msg);
static int tig_menu_bar_destroy(int menu_bar_handle);
static void refresh_menu_items(tig_window_handle_t window_handle, int x, int y, const char** menu_items, int num_menu_items, int selected, int item_width, int item_height, unsigned int* flags);
static int calculate_drop_down_item_size(const char* title, const char** menu_items, int num_menu_items, int* width_ptr, int* height_ptr);
static int tig_menu_bar_get_free_index();
static int tig_menu_bar_find_drop_down(tig_button_handle_t button_handle, int* drop_down_index_ptr);
static int tig_menu_bar_handle_to_index(int handle);
static int tig_menu_bar_index_to_handle(int index);
static bool tig_menu_drop_down_create(const char** menu_items, int num_menu_items, int x, int y, unsigned int* flags, TigMenuDropDown* drop_down);
static void tig_menu_drop_down_destroy(TigMenuDropDown* drop_down);
static int tig_menu_drop_down_run(TigMenuDropDown* drop_down, TigRect* frame);
static bool tig_menu_drop_down_message_filter(TigMessage* msg);

// 0x636514
static tig_button_handle_t tig_menu_select_menu_item_button_handles[MAX_DROP_DOWNS];

// 0x63653C
static int dword_63653C;

// 0x636540
static tig_button_handle_t tig_menu_select_down_button_handle;

// 0x636544
static int tig_menu_select_item_height;

// 0x636548
static int tig_menu_select_num_buttons;

// 0x63654C
static int tig_menu_select_item_width;

// 0x636550
static TigMenuDropDown tig_menu_current_drop_down;

// 0x636578
static TigMenuBar tig_menu_bars[MAX_MENU_BARS];

// 0x636ED8
static tig_button_handle_t tig_menu_select_up_button_handle;

// 0x636EDC
static tig_button_handle_t tig_menu_select_cancel_button_handle;

// 0x636EE0
static int tig_menu_select_num_menu_items;

// 0x636EE4
static TigMenuDropDown* dword_636EE4;

// 0x636EE8
static const char** tig_menu_select_menu_items;

// 0x636EEC
static tig_window_handle_t tig_menu_select_window_handle;

// 0x636EF0
static int tig_menu_current_drop_down_index;

// 0x636EF4
static bool dword_636EF4;

// 0x636EF8
static int tig_menu_select_selected_menu_item_index;

// 0x636EFC
static int tig_menu_select_top_menu_item_index;

// 0x636F00
static TigMenuColors tig_menu_colors;

// 0x636510
static tig_button_handle_t tig_menu_current_drop_down_button_handle;

// 0x636F14
static int dword_636F14;

// 0x636F18
static TigRect* dword_636F18;

// 0x636F1C
static int tig_menu_current_menu_bar_index;

// A boolean flag indicating we're currently showing drop down menu items.
//
// 0x636F20
static bool tig_menu_presenting_drop_down;

// 0x538F60
int tig_menu_init(TigInitInfo* init_info)
{
    TigMenuColors colors;
    int menu_bar_index;

    (void)init_info;

    for (menu_bar_index = 0; menu_bar_index < MAX_MENU_BARS; menu_bar_index++) {
        tig_menu_bars[menu_bar_index].usage = TIG_MENU_BAR_UNUSED;
    }

    colors.text_color = tig_color_make(255, 255, 255);
    colors.background_color = tig_color_make(64, 64, 64);
    colors.selected_text_color = tig_color_make(255, 0, 0);
    colors.disabled_text_color = tig_color_make(128, 128, 128);
    colors.selected_menu_item_outline_color = tig_color_make(255, 255, 255);
    tig_menu_set_colors(&colors);

    return TIG_OK;
}

// 0x5390C0
void tig_menu_exit()
{
    int menu_bar_index;
    int menu_bar_handle;

    for (menu_bar_index = 0; menu_bar_index < MAX_MENU_BARS; menu_bar_index++) {
        if ((tig_menu_bars[menu_bar_index].usage & TIG_MENU_BAR_UNUSED) == 0) {
            menu_bar_handle = tig_menu_bar_index_to_handle(menu_bar_index);
            tig_menu_bar_destroy(menu_bar_handle);
        }
    }
}

// 0x5390F0
int tig_menu_set_colors(TigMenuColors* colors)
{
    tig_menu_colors = *colors;
    return TIG_OK;
}

// 0x539130
int tig_menu_do_select(const char* title, const char** menu_items, int num_menu_items, int x, int y)
{
    TigArtFrameData up_art_frame_data;
    TigArtFrameData down_art_frame_data;
    TigArtFrameData cancel_art_frame_data;
    TigButtonData button_data;
    TigWindowData window_data;
    TigRect title_rect;
    TigRect box_rect;
    TigFont font_data;
    tig_font_handle_t font;
    tig_art_id_t up_art_id;
    tig_art_id_t down_art_id;
    tig_art_id_t cancel_art_id;
    int item_width;
    int item_height;
    int width;
    int height;
    int index;
    int num_buttons;

    if (num_menu_items < 1) {
        return -1;
    }

    if (calculate_drop_down_item_size(title, menu_items, num_menu_items, &item_width, &item_height) == -1) {
        return -1;
    }

    tig_art_misc_id_create(TIG_ART_SYSTEM_UP, 0, &up_art_id);
    tig_art_misc_id_create(TIG_ART_SYSTEM_DOWN, 0, &down_art_id);
    tig_art_misc_id_create(TIG_ART_SYSTEM_CANCEL, 0, &cancel_art_id);

    if (tig_art_frame_data(up_art_id, &up_art_frame_data) != TIG_OK) {
        return -1;
    }

    if (tig_art_frame_data(down_art_id, &down_art_frame_data) != TIG_OK) {
        return -1;
    }

    if (tig_art_frame_data(cancel_art_id, &cancel_art_frame_data) != TIG_OK) {
        return -1;
    }

    if (up_art_frame_data.width > down_art_frame_data.width) {
        width = up_art_frame_data.width + item_width;
    } else {
        width = down_art_frame_data.width + item_width;
    }

    width += 15;

    if (width < 100) {
        width = 100;
    }

    height = cancel_art_frame_data.height + 11 * item_height + 20;

    window_data.rect.x = x;
    window_data.rect.y = y;
    window_data.rect.width = width;
    window_data.rect.height = height;
    window_data.background_color = tig_menu_colors.background_color;
    window_data.message_filter = sub_5395E0;

    if (tig_window_create(&window_data, &tig_menu_select_window_handle) != TIG_OK) {
        return -1;
    }

    title_rect.x = 5;
    title_rect.y = 5;

    font_data.str = title;
    font_data.width = 0;
    tig_font_measure(&font_data);

    title_rect.width = font_data.width;
    title_rect.height = font_data.height;
    font_data.color = tig_menu_colors.text_color;

    tig_font_create(&font_data, &font);
    tig_font_push(font);

    if (title != NULL) {
        tig_window_text_write(tig_menu_select_window_handle, title, &title_rect);
    }

    button_data.window_handle = tig_menu_select_window_handle;
    button_data.flags = TIG_BUTTON_FLAG_0x01;
    button_data.x = width - up_art_frame_data.width - 5;
    button_data.y = item_height + 10;
    button_data.art_id = up_art_id;
    button_data.mouse_down_snd_id = -1;
    button_data.mouse_up_snd_id = -1;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;
    tig_button_create(&button_data, &tig_menu_select_up_button_handle);

    button_data.window_handle = tig_menu_select_window_handle;
    button_data.flags = TIG_BUTTON_FLAG_0x01;
    button_data.x = width - down_art_frame_data.width - 5;
    button_data.y = 11 * item_height - down_art_frame_data.height + 10;
    button_data.art_id = down_art_id;
    button_data.mouse_down_snd_id = -1;
    button_data.mouse_up_snd_id = -1;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;
    tig_button_create(&button_data, &tig_menu_select_down_button_handle);

    button_data.window_handle = tig_menu_select_window_handle;
    button_data.flags = TIG_BUTTON_FLAG_0x01;
    button_data.x = 5;
    button_data.y = height - cancel_art_frame_data.height - 5;
    button_data.art_id = cancel_art_id;
    button_data.mouse_down_snd_id = -1;
    button_data.mouse_up_snd_id = -1;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;
    tig_button_create(&button_data, &tig_menu_select_cancel_button_handle);

    box_rect.x = 4;
    box_rect.y = item_height + 9;
    box_rect.width = item_width + 2;
    box_rect.height = 10 * item_height + 2;
    tig_window_box(tig_menu_select_window_handle, &box_rect, tig_menu_colors.selected_menu_item_outline_color);

    num_buttons = num_menu_items > 10 ? 10 : num_menu_items;

    for (index = 0; index < num_buttons; index++) {
        button_data.window_handle = tig_menu_select_window_handle;
        button_data.x = 5;
        button_data.y = item_height * (index + 1) + 10;
        button_data.art_id = TIG_ART_ID_INVALID;
        button_data.width = item_width;
        button_data.height = item_height;
        button_data.mouse_down_snd_id = -1;
        button_data.mouse_up_snd_id = -1;
        button_data.mouse_enter_snd_id = -1;
        button_data.mouse_exit_snd_id = -1;
        tig_button_create(&button_data, &(tig_menu_select_menu_item_button_handles[index]));
    }

    refresh_menu_items(tig_menu_select_window_handle,
        5,
        item_height + 10,
        menu_items,
        num_buttons,
        -1,
        item_width,
        item_height,
        NULL);

    tig_menu_select_menu_items = menu_items;
    tig_menu_select_top_menu_item_index = 0;
    tig_menu_select_selected_menu_item_index = -2;
    tig_menu_select_item_width = item_width;
    tig_menu_select_item_height = item_height;
    tig_menu_select_num_buttons = num_buttons;
    tig_menu_select_num_menu_items = num_menu_items;

    while (!tig_kb_is_key_pressed(SDL_SCANCODE_ESCAPE) && tig_menu_select_selected_menu_item_index == -2) {
        tig_ping();

        // FIXME: Missing message handling loop.
    }

    tig_font_pop();
    tig_font_destroy(font);
    tig_window_destroy(tig_menu_select_window_handle);

    return tig_menu_select_selected_menu_item_index;
}

// 0x5395E0
bool sub_5395E0(TigMessage* msg)
{
    int index;

    switch (msg->type) {
    case TIG_MESSAGE_BUTTON:
        if (msg->data.button.button_handle == tig_menu_select_cancel_button_handle) {
            if (msg->data.button.state == TIG_BUTTON_STATE_RELEASED) {
                tig_menu_select_selected_menu_item_index = -1;
            }
        } else if (msg->data.button.button_handle == tig_menu_select_up_button_handle) {
            if (msg->data.button.state == TIG_BUTTON_STATE_PRESSED) {
                if (tig_menu_select_top_menu_item_index > 0) {
                    --tig_menu_select_top_menu_item_index;
                    refresh_menu_items(tig_menu_select_window_handle,
                        5,
                        tig_menu_select_item_height + 10,
                        tig_menu_select_menu_items + 4 * tig_menu_select_top_menu_item_index,
                        tig_menu_select_num_buttons,
                        -1,
                        tig_menu_select_item_width,
                        tig_menu_select_item_height,
                        NULL);
                }
            }
        } else if (msg->data.button.button_handle == tig_menu_select_down_button_handle) {
            if (msg->data.button.state == TIG_BUTTON_STATE_PRESSED) {
                if (tig_menu_select_top_menu_item_index < tig_menu_select_num_menu_items - 10) {
                    ++tig_menu_select_top_menu_item_index;
                    refresh_menu_items(tig_menu_select_window_handle,
                        5,
                        tig_menu_select_item_height + 10,
                        tig_menu_select_menu_items + 4 * tig_menu_select_top_menu_item_index,
                        tig_menu_select_num_buttons,
                        -1,
                        tig_menu_select_item_width,
                        tig_menu_select_item_height,
                        NULL);
                }
            }
        } else {
            for (index = 0; index < tig_menu_select_num_buttons; index++) {
                if (msg->data.button.button_handle == tig_menu_select_menu_item_button_handles[index]) {
                    switch (msg->data.button.state) {
                    case TIG_BUTTON_STATE_RELEASED:
                        tig_menu_select_selected_menu_item_index = index + tig_menu_select_top_menu_item_index;
                        break;
                    case TIG_BUTTON_STATE_MOUSE_INSIDE:
                        refresh_menu_items(tig_menu_select_window_handle,
                            5,
                            tig_menu_select_item_height + 10,
                            tig_menu_select_menu_items + 4 * tig_menu_select_top_menu_item_index,
                            tig_menu_select_num_buttons,
                            index,
                            tig_menu_select_item_width,
                            tig_menu_select_item_height,
                            NULL);
                        break;
                    case TIG_BUTTON_STATE_MOUSE_OUTSIDE:
                        refresh_menu_items(tig_menu_select_window_handle,
                            5,
                            tig_menu_select_item_height + 10,
                            tig_menu_select_menu_items + 4 * tig_menu_select_top_menu_item_index,
                            tig_menu_select_num_buttons,
                            -1,
                            tig_menu_select_item_width,
                            tig_menu_select_item_height,
                            NULL);
                        break;
                    }
                    break;
                }
            }
        }
        break;
    }

    return true;
}

// 0x5397C0
int tig_menu_do_context_menu(const char** menu_items, int num_menu_items, int x, int y, unsigned int* flags)
{
    TigMenuDropDown drop_down;
    int selected;

    if (!tig_menu_drop_down_create(menu_items, num_menu_items, x, y, flags, &drop_down)) {
        return -1;
    }

    selected = tig_menu_drop_down_run(&drop_down, NULL);

    tig_menu_drop_down_destroy(&drop_down);

    return selected;
}

// 0x539820
int tig_menu_bar_create(TigMenuBarCreateInfo* create_info)
{
    TigWindowData window_data;
    TigButtonData button_data;
    TigFont font_data;
    tig_font_handle_t font;
    TigRect text_rect;
    TigMenuBar* bar;
    int index;
    int x;
    int height;
    int rc;

    // Obtain parent window data.
    rc = tig_window_data(create_info->window_handle, &window_data);
    if (rc != TIG_OK) {
        return rc;
    }

    // Obtain free slot.
    index = tig_menu_bar_get_free_index();
    if (index == -1) {
        // FIXME: Should be `TIG_ERR_OUT_OF_HANDLES`.
        return TIG_ERR_OUT_OF_MEMORY;
    }

    bar = &(tig_menu_bars[index]);

    // Reserve space for drop down buttons.
    bar->buttons = (tig_button_handle_t*)MALLOC(sizeof(tig_button_handle_t) * create_info->num_drop_downs);
    if (bar->buttons == NULL) {
        return TIG_ERR_OUT_OF_MEMORY;
    }

    // Setup menu bar.
    bar->usage &= ~TIG_MENU_BAR_UNUSED;
    bar->window_handle = create_info->window_handle;
    bar->frame.x = create_info->x;
    bar->frame.y = create_info->y;
    bar->num_drop_downs = create_info->num_drop_downs;
    bar->drop_downs = create_info->drop_downs;
    bar->num_menu_items = create_info->num_menu_items;
    bar->menu_items = create_info->menu_items;
    bar->flags = create_info->flags;

    // Calculate menu bar height.
    font_data.str = bar->drop_downs[0];
    font_data.width = 0;
    tig_font_measure(&font_data);
    height = font_data.height;

    // Prepare drop down buttons template.
    button_data.flags = TIG_BUTTON_MENU_BAR;
    button_data.window_handle = bar->window_handle;
    button_data.art_id = TIG_ART_ID_INVALID;
    button_data.mouse_down_snd_id = -1;
    button_data.mouse_up_snd_id = -1;
    button_data.mouse_enter_snd_id = -1;
    button_data.mouse_exit_snd_id = -1;

    // Prepare drop button button title font.
    font_data.str = NULL;
    font_data.width = 0;
    tig_font_measure(&font_data);

    font_data.color = tig_menu_colors.text_color;
    tig_font_create(&font_data, &font);
    tig_font_push(font);

    // Loop thru menu items, write drop down button title and create clickable
    // button.
    x = create_info->x;
    for (index = 0; index < bar->num_drop_downs; index++) {
        font_data.str = bar->drop_downs[index];
        font_data.width = 0;
        tig_font_measure(&font_data);

        text_rect.x = x;
        text_rect.y = create_info->y;
        text_rect.width = font_data.width;
        text_rect.height = font_data.height;
        tig_window_text_write(bar->window_handle, bar->drop_downs[index], &text_rect);

        button_data.x = x;
        button_data.y = create_info->y;
        button_data.width = font_data.width;
        button_data.height = font_data.height;
        tig_button_create(&button_data, &(bar->buttons[index]));

        x += button_data.width + 10;
    }

    // Cleanup.
    tig_font_pop();
    tig_font_destroy(font);

    bar->frame.height = height;
    bar->frame.width = x - create_info->x - 10;

    return TIG_OK;
}

// 0x539A40
int tig_menu_bar_destroy(int menu_bar_handle)
{
    int menu_bar_index;

    menu_bar_index = tig_menu_bar_handle_to_index(menu_bar_handle);
    FREE(tig_menu_bars[menu_bar_index].buttons);
    tig_menu_bars[menu_bar_index].usage |= TIG_MENU_BAR_UNUSED;

    return TIG_OK;
}

// 0x539A80
int tig_menu_bar_set_flags(int menu_bar_handle, unsigned int* flags)
{
    int menu_bar_index;

    menu_bar_index = tig_menu_bar_handle_to_index(menu_bar_handle);
    tig_menu_bars[menu_bar_index].flags = flags;

    return TIG_OK;
}

// 0x539AA0
void tig_menu_bar_on_click(tig_button_handle_t button_handle)
{
    TigButtonData button_data;
    TigWindowData window_data;
    TigMessage msg;
    int index;
    int drop_down_index;
    int menu_item_index;

    if (tig_menu_presenting_drop_down && button_handle == tig_menu_current_drop_down_button_handle) {
        return;
    }

    tig_menu_current_menu_bar_index = tig_menu_bar_find_drop_down(button_handle, &tig_menu_current_drop_down_index);

    if (tig_button_data(button_handle, &button_data) != TIG_OK) {
        return;
    }

    if (tig_window_data(tig_menu_bars[tig_menu_current_menu_bar_index].window_handle, &window_data) != TIG_OK) {
        return;
    }

    drop_down_index = 0;
    menu_item_index = 0;
    if (tig_menu_current_drop_down_index > 0) {
        drop_down_index = tig_menu_current_drop_down_index;
        for (index = 0; index < tig_menu_current_drop_down_index; index++) {
            menu_item_index += tig_menu_bars[tig_menu_current_menu_bar_index].num_menu_items[index];
        }
    }

    if (tig_menu_presenting_drop_down) {
        tig_menu_drop_down_destroy(&tig_menu_current_drop_down);
    }

    if (tig_menu_drop_down_create(&(tig_menu_bars[tig_menu_current_menu_bar_index].menu_items[menu_item_index]), tig_menu_bars[tig_menu_current_menu_bar_index].num_menu_items[drop_down_index], button_data.x + window_data.rect.x, button_data.y + button_data.height, &(tig_menu_bars[tig_menu_current_menu_bar_index].flags[menu_item_index]), &tig_menu_current_drop_down)) {
        tig_menu_current_drop_down_button_handle = button_handle;

        if (!tig_menu_presenting_drop_down) {
            int selected;

            tig_menu_presenting_drop_down = true;

            selected = tig_menu_drop_down_run(&tig_menu_current_drop_down, &(tig_menu_bars[tig_menu_current_menu_bar_index].frame));
            tig_menu_drop_down_destroy(&tig_menu_current_drop_down);

            if (selected != -1) {
                msg.type = TIG_MESSAGE_MENU;
                msg.data.menu.menu_bar_handle = tig_menu_bar_index_to_handle(tig_menu_current_menu_bar_index);
                msg.data.menu.drop_down_index = tig_menu_current_drop_down_index;
                msg.data.menu.menu_item_index = selected;
                tig_message_enqueue(&msg);
            }

            tig_menu_presenting_drop_down = false;
        }
    }
}

// 0x539C50
void refresh_menu_items(tig_window_handle_t window_handle, int x, int y, const char** menu_items, int num_menu_items, int selected, int item_width, int item_height, unsigned int* flags)
{
    TigFont font_data;
    tig_font_handle_t font;
    TigRect rect;
    char knob[2];
    int index;
    tig_color_t color;

    knob[0] = '\xB7';
    knob[1] = '\0';

    rect.x = x;
    rect.y = y;
    rect.width = item_width;
    rect.height = num_menu_items * item_height;
    tig_window_fill(window_handle, &rect, tig_menu_colors.background_color);

    rect.width = item_width;
    rect.height = item_height;

    for (index = 0; index < num_menu_items; index++) {
        if (flags != NULL && (flags[index] & 0x2) != 0) {
            color = tig_menu_colors.disabled_text_color;
        } else {
            color = index == selected ? tig_menu_colors.selected_text_color : tig_menu_colors.text_color;
        }

        font_data.str = NULL;
        font_data.width = 0;
        tig_font_measure(&font_data);
        font_data.color = color;

        tig_font_create(&font_data, &font);
        tig_font_push(font);

        rect.y = y;
        if (flags != NULL && (flags[index] & 0x4) != 0) {
            rect.x = x - 5;
            tig_window_text_write(window_handle, knob, &rect);
        }

        rect.x = x;
        tig_window_text_write(window_handle, menu_items[index], &rect);

        tig_font_pop();
        tig_font_destroy(font);

        y += item_height;
    }

    tig_window_display();
}

// 0x539DC0
int calculate_drop_down_item_size(const char* title, const char** menu_items, int num_menu_items, int* width_ptr, int* height_ptr)
{
    TigFont font_desc;
    int index;
    int width;

    // NOTE: Initialize to make compiler happy.
    font_desc.height = 0;

    if (title != NULL) {
        font_desc.str = title;
        font_desc.width = 0;
        tig_font_measure(&font_desc);
        width = font_desc.width;
    } else {
        width = 0;
    }

    for (index = 0; index < num_menu_items; index++) {
        font_desc.str = menu_items[index];
        font_desc.width = 0;
        tig_font_measure(&font_desc);
        if (width < font_desc.width) {
            width = font_desc.width;
        }
    }

    *width_ptr = width;
    *height_ptr = font_desc.height;

    return TIG_OK;
}

// 0x539E50
int tig_menu_bar_get_free_index()
{
    int index = 0;

    for (index = 0; index < MAX_MENU_BARS; index++) {
        if ((tig_menu_bars[index].usage & TIG_MENU_BAR_UNUSED) != 0) {
            return index;
        }
    }

    return -1;
}

// 0x539E70
int tig_menu_bar_find_drop_down(tig_button_handle_t button_handle, int* drop_down_index_ptr)
{
    int menu_bar_index;
    int drop_down_index;

    for (menu_bar_index = 0; menu_bar_index < MAX_MENU_BARS; menu_bar_index++) {
        if ((tig_menu_bars[menu_bar_index].usage & TIG_MENU_BAR_UNUSED) == 0) {
            for (drop_down_index = 0; drop_down_index < tig_menu_bars[menu_bar_index].num_drop_downs; drop_down_index++) {
                if (tig_menu_bars[menu_bar_index].buttons[drop_down_index] == button_handle) {
                    *drop_down_index_ptr = drop_down_index;
                    return menu_bar_index;
                }
            }
        }
    }

    return -1;
}

// 0x539EC0
int tig_menu_bar_handle_to_index(int handle)
{
    return handle;
}

// 0x539ED0
int tig_menu_bar_index_to_handle(int index)
{
    return index;
}

// 0x539EE0
bool tig_menu_drop_down_create(const char** menu_items, int num_menu_items, int x, int y, unsigned int* flags, TigMenuDropDown* drop_down)
{
    TigWindowData window_data;
    TigRect box_rect;
    tig_window_handle_t window_handle;
    int item_width;
    int item_height;

    drop_down->menu_items = menu_items;
    drop_down->num_menu_items = num_menu_items;

    if (calculate_drop_down_item_size(NULL, menu_items, num_menu_items, &item_width, &item_height) == -1) {
        return false;
    }

    // FIXME: 5 is not enough because below we're subtracting 10 from it.
    drop_down->item_width = item_width + 5;
    drop_down->item_height = item_height;
    drop_down->flags = flags;

    window_data.rect.x = x;
    window_data.rect.y = y;
    window_data.rect.width = drop_down->item_width + 2;
    window_data.rect.height = num_menu_items * item_height + 2;
    window_data.flags = TIG_WINDOW_MESSAGE_FILTER | TIG_WINDOW_MODAL;
    window_data.background_color = tig_menu_colors.background_color;
    window_data.message_filter = tig_menu_drop_down_message_filter;

    if (tig_window_create(&window_data, &window_handle) != TIG_OK) {
        return false;
    }

    drop_down->window_handle = window_handle;
    drop_down->frame = window_data.rect;

    box_rect.x = 0;
    box_rect.y = 0;
    box_rect.width = window_data.rect.width;
    box_rect.height = window_data.rect.height;
    tig_window_box(window_handle, &box_rect, tig_menu_colors.selected_menu_item_outline_color);

    refresh_menu_items(window_handle,
        11,
        1,
        menu_items,
        num_menu_items,
        -1,
        drop_down->item_width - 10,
        drop_down->item_height,
        flags);

    return true;
}

// 0x53A020
void tig_menu_drop_down_destroy(TigMenuDropDown* drop_down)
{
    tig_window_destroy(drop_down->window_handle);
}

// 0x53A030
int tig_menu_drop_down_run(TigMenuDropDown* drop_down, TigRect* frame)
{
    dword_636EE4 = drop_down;
    dword_636F18 = frame;
    dword_63653C = -1;
    dword_636F14 = 0;
    dword_636EF4 = false;
    while (!dword_636EF4) {
        tig_ping();

        // FIXME: Missing message handling loop.
    }
    return dword_63653C;
}

// 0x53A080
bool tig_menu_drop_down_message_filter(TigMessage* msg)
{
    tig_button_handle_t button_handle;

    switch (msg->type) {
    case TIG_MESSAGE_MOUSE:
        switch (msg->data.mouse.event) {
        case TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP:
            if (dword_636F18 == NULL
                || msg->data.mouse.x < dword_636F18->x
                || msg->data.mouse.y < dword_636F18->y
                || msg->data.mouse.x >= dword_636F18->x + dword_636F18->width
                || msg->data.mouse.y >= dword_636F18->y + dword_636F18->height) {
                dword_636EF4 = true;
            }
            break;
        case TIG_MESSAGE_MOUSE_MOVE:
            if (msg->data.mouse.x >= dword_636EE4->frame.x
                && msg->data.mouse.y >= dword_636EE4->frame.y
                && msg->data.mouse.x < dword_636EE4->frame.x + dword_636EE4->frame.width
                && msg->data.mouse.y < dword_636EE4->frame.y + dword_636EE4->frame.height) {
                int index = (msg->data.mouse.y - dword_636EE4->frame.y) / ((dword_636EE4->frame.height - 2) / dword_636EE4->num_menu_items);
                if (index >= 0
                    && index < dword_636EE4->num_menu_items
                    && (dword_636EE4->flags == NULL
                        || (dword_636EE4->flags[index] & 0x2) == 0)) {
                    if (index != dword_63653C) {
                        refresh_menu_items(dword_636EE4->window_handle,
                            11,
                            1,
                            dword_636EE4->menu_items,
                            dword_636EE4->num_menu_items,
                            index,
                            dword_636EE4->item_width - 10,
                            dword_636EE4->item_height,
                            dword_636EE4->flags);
                        dword_63653C = index;
                        dword_636F14 = true;
                    }
                } else {
                    refresh_menu_items(dword_636EE4->window_handle,
                        11,
                        1,
                        dword_636EE4->menu_items,
                        dword_636EE4->num_menu_items,
                        -1,
                        dword_636EE4->item_width - 10,
                        dword_636EE4->item_height,
                        dword_636EE4->flags);
                    dword_63653C = -1;
                }
            } else {
                if (dword_636F14) {
                    refresh_menu_items(dword_636EE4->window_handle,
                        11,
                        1,
                        dword_636EE4->menu_items,
                        dword_636EE4->num_menu_items,
                        -1,
                        dword_636EE4->item_width - 10,
                        dword_636EE4->item_height,
                        dword_636EE4->flags);
                    dword_636F14 = false;
                    dword_63653C = -1;
                }

                if (dword_636F18 != NULL
                    && msg->data.mouse.x >= dword_636F18->x
                    && msg->data.mouse.y >= dword_636F18->y
                    && msg->data.mouse.x < dword_636F18->x + dword_636F18->width
                    && msg->data.mouse.y < dword_636F18->y + dword_636F18->height) {
                    button_handle = tig_button_get_at_position(msg->data.mouse.x, msg->data.mouse.y);
                    if (button_handle != TIG_BUTTON_HANDLE_INVALID) {
                        tig_menu_bar_on_click(button_handle);
                    }
                }
            }
            break;
        }
        break;
    }

    return true;
}
