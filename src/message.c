#include "tig/message.h"

#include "tig/button.h"
#include "tig/core.h"
#include "tig/kb.h"
#include "tig/memory.h"
#include "tig/mouse.h"
#include "tig/movie.h"
#include "tig/net.h"
#include "tig/timer.h"
#include "tig/video.h"
#include "tig/window.h"

// Maximum number of key handler.
#define MAX_KEY_HANDLERS 16

typedef struct TigMessageListNode {
    /* 0000 */ TigMessage message;
    /* 001C */ struct TigMessageListNode* next;
} TigMessageListNode;

static_assert(sizeof(TigMessageListNode) == 0x20, "wrong size");

typedef struct TigMessageKeyboardHandler {
    /* 0000 */ int key;
    /* 0004 */ TigMessageKeyboardCallback* callback;
} TigMessageKeyboardHandler;

static_assert(sizeof(TigMessageKeyboardHandler) == 0x8, "wrong size");

static bool sub_52BF90(TigMessage* message);
static TigMessageListNode* tig_message_node_acquire();
static void tig_message_node_reserve();
static void tig_message_node_release(TigMessageListNode* node);

// 0x62B1F0
static TigMessageKeyboardHandler tig_message_key_handlers[MAX_KEY_HANDLERS];

// 0x62B1E8
static TigMessageHandler* tig_message_handler;

// 0x62B1EC
static int dword_62B1EC;

// 0x62B278
static CRITICAL_SECTION tig_message_critical_section;

// 0x62B270
static WNDPROC tig_message_default_window_proc;

// 0x62B274
static bool tig_message_should_process_mouse_events;

// 0x62B290
static TigMessageListNode* tig_message_empty_node_head;

// 0x62B294
static TigMessageListNode* tig_message_queue_head;

// 0x62B298
static int tig_message_key_handlers_count;

// 0x52B6D0
int tig_message_init(TigInitInfo* init_info)
{
    InitializeCriticalSection(&tig_message_critical_section);

    tig_message_empty_node_head = NULL;
    tig_message_queue_head = NULL;
    dword_62B1EC = 1;
    tig_message_key_handlers_count = 0;
    tig_message_should_process_mouse_events = (init_info->flags & TIG_INITIALIZE_WINDOWED) != 0;

    if (!tig_message_should_process_mouse_events) {
        tig_message_default_window_proc = NULL;
        tig_message_handler = NULL;
        return TIG_OK;
    }

    if (tig_message_default_window_proc == NULL) {
        return TIG_ERR_16;
    }

    if ((init_info->flags & TIG_INITIALIZE_MESSAGE_HANDLER) == 0) {
        tig_message_handler = NULL;
        return TIG_OK;
    }

    if (init_info->message_handler == NULL) {
        return TIG_ERR_INVALID_PARAM;
    }

    tig_message_handler = init_info->message_handler;

    return TIG_OK;
}

// 0x52B750
void tig_message_exit()
{
    TigMessageListNode* next;

    while (tig_message_queue_head != NULL) {
        next = tig_message_queue_head->next;
        FREE(tig_message_queue_head);
        tig_message_queue_head = next;
    }

    while (tig_message_empty_node_head != NULL) {
        next = tig_message_empty_node_head->next;
        FREE(tig_message_empty_node_head);
        tig_message_empty_node_head = next;
    }

    DeleteCriticalSection(&tig_message_critical_section);
}

// 0x52B7B0
void tig_message_ping()
{
    TigMessage message;
    MSG msg;
    int index;

    message.type = TIG_MESSAGE_PING;
    message.timestamp = tig_ping_timestamp;
    tig_message_enqueue(&message);

    for (index = 0; index < tig_message_key_handlers_count; index++) {
        if (tig_kb_is_key_pressed(tig_message_key_handlers[index].key)) {
            if (!dword_62B1A8[index]) {
                dword_62B1A8[index] = true;
                tig_message_key_handlers[index].callback(tig_message_key_handlers[index].key);
            }
        } else {
            dword_62B1A8[index] = false;
        }
    }

    if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            tig_message_post_quit(msg.wParam);
        } else {
            if (tig_message_handler == NULL || tig_message_handler(&msg)) {
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
        }
    }
}

// 0x52B890
int tig_message_enqueue(TigMessage* message)
{
    EnterCriticalSection(&tig_message_critical_section);

    if (sub_52BF90(message)) {
        TigMessageListNode* node = tig_message_node_acquire();
        node->message = *message;
        node->next = NULL;

        if (message->type == TIG_MESSAGE_REDRAW || tig_message_queue_head == NULL) {
            // Redraw is always placed on top of the message queue.
            node->next = tig_message_queue_head;
            tig_message_queue_head = node;
        } else {
            // Append new node to the end of the message queue.
            TigMessageListNode* curr = tig_message_queue_head;
            while (curr->next != NULL) {
                curr = curr->next;
            }

            curr->next = node;
        }
    }

    LeaveCriticalSection(&tig_message_critical_section);

    return TIG_OK;
}

// 0x52B920
int tig_message_set_key_handler(TigMessageKeyboardCallback* callback, int key)
{
    int index;

    if (key < 0) {
        // Bad key code.
        return TIG_ERR_INVALID_PARAM;
    }

    if (callback == NULL) {
        // Find handler to remove.
        for (index = 0; index < MAX_KEY_HANDLERS; index++) {
            if (tig_message_key_handlers[index].key == key) {
                break;
            }
        }

        if (index >= MAX_KEY_HANDLERS) {
            // Not found.
            return TIG_ERR_INVALID_PARAM;
        }

        // Reorder subsequent slots.
        while (index + 1 < tig_message_key_handlers_count) {
            dword_62B1A8[index] = dword_62B1A8[index + 1];
            tig_message_key_handlers[index] = tig_message_key_handlers[index + 1];
        }

        tig_message_key_handlers_count--;

        return TIG_OK;
    }

    if (tig_message_key_handlers_count >= MAX_KEY_HANDLERS) {
        return TIG_ERR_OUT_OF_HANDLES;
    }

    tig_message_key_handlers[tig_message_key_handlers_count].key = key;
    tig_message_key_handlers[tig_message_key_handlers_count].callback = callback;
    dword_62B1A8[tig_message_key_handlers_count] = false;
    tig_message_key_handlers_count++;

    return TIG_OK;
}

// 0x52B9D0
LRESULT CALLBACK tig_message_wnd_proc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT client_rect;
    TigRect rect;
    TigMessage message;

    switch (msg) {
    case WM_CREATE:
        GetClientRect(hWnd, &client_rect);
        // NOTE: Odd formatting by clang-format.
        ClientToScreen(hWnd, (LPPOINT) & (client_rect.left));
        ClientToScreen(hWnd, (LPPOINT) & (client_rect.right));
        tig_video_set_client_rect(&client_rect);
        break;
    case WM_MOVE:
        GetClientRect(hWnd, &client_rect);
        // NOTE: Odd formatting by clang-format.
        ClientToScreen(hWnd, (LPPOINT) & (client_rect.left));
        ClientToScreen(hWnd, (LPPOINT) & (client_rect.right));
        tig_video_set_client_rect(&client_rect);
        sub_51FA40(&rect);
        tig_window_set_needs_display_in_rect(&rect);
        break;
    case WM_PAINT:
        if (GetUpdateRect(hWnd, &client_rect, FALSE)) {
            rect.x = client_rect.left;
            rect.y = client_rect.top;
            rect.width = client_rect.right - client_rect.left;
            rect.height = client_rect.bottom - client_rect.top;
            tig_window_set_needs_display_in_rect(&rect);
            tig_window_display();
        }
        break;
    case WM_CLOSE:
        tig_message_post_quit(1);
        return FALSE;
    case WM_ERASEBKGND:
        return FALSE;
    case WM_ACTIVATEAPP:
        if (wParam == 1 || wParam == 2) {
            tig_set_active(true);
        } else {
            tig_set_active(false);
        }
        break;
    case WM_CHAR:
        tig_timer_now(&(message.timestamp));
        message.type = TIG_MESSAGE_CHAR;
        message.data.character.ch = wParam & 0xFF;
        tig_message_enqueue(&message);
        break;
    case WM_SYSCOMMAND:
        if (wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER) {
            return FALSE;
        }
        break;
    case WM_MOUSEMOVE:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_position(lParam & 0xFFFF, (lParam >> 16) & 0xFFFF, 0);
        }
        break;
    case WM_LBUTTONDOWN:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT, true);
        }
        break;
    case WM_LBUTTONUP:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT, false);
        }
        break;
    case WM_RBUTTONDOWN:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_button(TIG_MOUSE_BUTTON_RIGHT, true);
        }
        break;
    case WM_RBUTTONUP:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_button(TIG_MOUSE_BUTTON_RIGHT, false);
        }
        break;
    case WM_MBUTTONDOWN:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_button(TIG_MOUSE_BUTTON_MIDDLE, true);
        }
        break;
    case WM_MBUTTONUP:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_button(TIG_MOUSE_BUTTON_MIDDLE, false);
        }
        break;
    case WM_MOUSEWHEEL:
        if (tig_message_should_process_mouse_events) {
            tig_mouse_set_position(LOWORD(lParam), HIWORD(lParam), HIWORD(wParam));
        }
        break;
    case TIG_NET_BCAST_ASYNC_SELECT:
        tig_net_bcast_async_select(hWnd, wParam, lParam);
        return TRUE;
    case TIG_NET_LISTEN_ASYNC_SELECT:
        tig_net_listen_async_select(hWnd, wParam, lParam);
        return TRUE;
    }

    if (tig_message_default_window_proc != NULL) {
        return tig_message_default_window_proc(hWnd, msg, wParam, lParam);
    } else {
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
}

// 0x52BD90
void tig_message_set_default_window_proc(WNDPROC fn)
{
    tig_message_default_window_proc = fn;
}

// 0x52BDA0
int tig_message_dequeue(TigMessage* message)
{
    TigMessageListNode* next;
    TigMessage temp_message;

    if (tig_message_queue_head == NULL) {
        return TIG_ERR_10;
    }

    EnterCriticalSection(&tig_message_critical_section);

    while (tig_message_queue_head != NULL) {
        next = tig_message_queue_head->next;
        temp_message = tig_message_queue_head->message;
        tig_message_node_release(tig_message_queue_head);
        tig_message_queue_head = next;

        if (tig_movie_is_playing()
            || (temp_message.type != TIG_MESSAGE_MOUSE || !tig_button_process_mouse_msg(&(temp_message.data.mouse)))
                && (!tig_window_filter_message(&temp_message) || temp_message.type == TIG_MESSAGE_REDRAW)) {
            *message = temp_message;

            LeaveCriticalSection(&tig_message_critical_section);

            return TIG_OK;
        }
    }

    // FIXME: Missing `LeaveCriticalSection`.
    return TIG_ERR_10;
}

// 0x52BE60
int tig_message_post_quit(int exit_code)
{
    TigMessage message;
    tig_timer_now(&(message.timestamp));
    message.type = TIG_MESSAGE_QUIT;
    message.data.quit.exit_code = exit_code;
    return tig_message_enqueue(&message);
}

// 0x52BE90
int sub_52BE90()
{
    dword_62B1EC--;
    return TIG_OK;
}

// 0x52BEA0
int sub_52BEA0()
{
    dword_62B1EC++;
    return TIG_OK;
}

// 0x52BEB0
int sub_52BEB0(bool a1)
{
    dword_62B1EC = a1 ? 1 : 0;
    return TIG_OK;
}

// 0x52BED0
TigMessageListNode* tig_message_node_acquire()
{
    TigMessageListNode* node;

    EnterCriticalSection(&tig_message_critical_section);

    tig_message_node_reserve();

    node = tig_message_empty_node_head;
    tig_message_empty_node_head = node->next;
    node->next = NULL;

    LeaveCriticalSection(&tig_message_critical_section);

    return node;
}

// 0x52BF10
void tig_message_node_reserve()
{
    int index;
    TigMessageListNode* node;

    if (tig_message_empty_node_head == NULL) {
        EnterCriticalSection(&tig_message_critical_section);

        for (index = 0; index < 32; index++) {
            node = (TigMessageListNode*)MALLOC(sizeof(*node));
            node->next = tig_message_empty_node_head;
            tig_message_empty_node_head = node;
        }

        LeaveCriticalSection(&tig_message_critical_section);
    }
}

// 0x52BF60
void tig_message_node_release(TigMessageListNode* node)
{
    EnterCriticalSection(&tig_message_critical_section);

    node->next = tig_message_empty_node_head;
    tig_message_empty_node_head = node;

    LeaveCriticalSection(&tig_message_critical_section);
}

// 0x52BF90
bool sub_52BF90(TigMessage* message)
{
    if (dword_62B1EC == 0) {
        switch (message->type) {
        case TIG_MESSAGE_MOUSE:
            if (message->data.mouse.event != TIG_MESSAGE_MOUSE_MOVE) {
                return false;
            }
            break;
        case TIG_MESSAGE_KEYBOARD:
            return false;
        }
    }

    return true;
}
