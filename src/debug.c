#include "tig/debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#endif

#include "tig/memory.h"

#define VGA_PTR ((unsigned char*)0xB0000)
#define VGA_WIDTH 160
#define VGA_HEIGHT 25
#define VGA_SIZE (VGA_WIDTH * VGA_HEIGHT)

#define MAX_DEBUG_FUNCS 8

typedef void(DebugOutputFunc)(const char*);
typedef void(DebugInitFunc)();

typedef struct DebugBackend {
    const char* name;
    DebugOutputFunc* func;
} DebugBackend;

static void tig_debug_init_backends();
static void tig_debug_init_backends_from_registry();
static bool tig_debug_add_func(DebugOutputFunc* func);
static bool tig_debug_remove_func(DebugOutputFunc* func);
static void tig_debug_console_init();
static void tig_debug_console_impl(const char* string);
static void tig_debug_debugger_impl(const char* string);
static void tig_debug_file_init();
static void tig_debug_file_impl(const char* string);
static void tig_debug_mono_init();
static void tig_debug_mono_impl(const char* string);

// 0x5BE7A0
static DebugBackend tig_debug_backends[] = {
    { "console", tig_debug_console_impl },
    { "debugger", tig_debug_debugger_impl },
    { "file", tig_debug_file_impl },
    { "mono", tig_debug_mono_impl },
};

#define MAX_BACKENDS (sizeof(tig_debug_backends) / sizeof(tig_debug_backends[0]))

// 0x5BE7C0
static DebugInitFunc* tig_debug_console_init_func = tig_debug_console_init;

// 0x5BE7C4
static DebugInitFunc* tig_debug_file_init_func = tig_debug_file_init;

// 0x5BE7C8
static DebugInitFunc* tig_debug_mono_init_func = tig_debug_mono_init;

// 0x60420C
static FILE* tig_debug_file_handle;

// 0x604210
static bool tig_debug_mono_initialized;

// 0x604614
static bool have_debug_funcs;

// 0x604618
static DebugOutputFunc* tig_debug_funcs[MAX_DEBUG_FUNCS];

// 0x604638
static unsigned char* mono_dest;

// 0x4FEB10
int tig_debug_init(TigInitInfo* init_info)
{
    time_t now;

    (void)init_info;

    tig_debug_init_backends();
    tig_memory_set_output_func(tig_debug_println);
    tig_debug_println("\n");

    time(&now);
    tig_debug_println(asctime(localtime(&now)));

    return 0;
}

// 0x4FEB60
void tig_debug_exit()
{
}

// 0x4FEB70
void tig_debug_printf(const char* format, ...)
{
    // 0x604214
    static char buffer[1024];

    int index;
    DebugOutputFunc* func;

    va_list args;
    va_start(args, format);

    if (have_debug_funcs) {
        vsnprintf(buffer, sizeof(buffer), format, args);

        for (index = 0; index < MAX_DEBUG_FUNCS; index++) {
            func = tig_debug_funcs[index];
            if (func != NULL) {
                func(buffer);
            }
        }
    }

    va_end(args);
}

// 0x4FEBC0
void tig_debug_println(const char* string)
{
    int index;
    DebugOutputFunc* func;

    if (!have_debug_funcs) {
        return;
    }

    for (index = 0; index < MAX_DEBUG_FUNCS; index++) {
        func = tig_debug_funcs[index];
        if (func != NULL) {
            func(string);
            func("\n");
        }
    }
}

// 0x4FEC00
void tig_debug_init_backends()
{
#ifdef _WIN32
    const char* command_line;
    const char* pch;
    bool enabled;
    int index;

    tig_debug_init_backends_from_registry();

    command_line = GetCommandLineA();
    pch = strstr(command_line, "tigdebug");
    while (pch != NULL) {
        enabled = true;
        if (pch > command_line && pch[-1] == '-') {
            enabled = false;
        }

        // Consume "tigdebug".
        pch += 8;

        if (*pch == '=') {
            // Consume "="
            pch++;

            for (index = 0; index < MAX_BACKENDS; index++) {
                if (strnicmp(pch, tig_debug_backends[index].name, strlen(tig_debug_backends[index].name)) == 0) {
                    if (enabled) {
                        tig_debug_add_func(tig_debug_backends[index].func);
                    } else {
                        tig_debug_remove_func(tig_debug_backends[index].func);
                    }
                }
            }
        }

        pch = strstr(pch, "tigdebug");
    }
#endif
}

// 0x4FEC10
void tig_debug_init_backends_from_registry()
{
#ifdef _WIN32
    LSTATUS rc;
    HKEY key;
    DWORD type;
    DWORD data;
    DWORD cbData;
    int index;

    rc = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "Software\\Troika\\TIG\\Debug Output",
        0,
        KEY_QUERY_VALUE,
        &key);
    if (rc != ERROR_SUCCESS) {
        return;
    }

    for (index = 0; index < MAX_BACKENDS; index++) {
        rc = RegQueryValueExA(key,
            tig_debug_backends[index].name,
            0,
            &type,
            (LPBYTE)&data,
            &cbData);
        if (rc == ERROR_SUCCESS && type == REG_DWORD) {
            if (data != 0) {
                tig_debug_add_func(tig_debug_backends[index].func);
            } else {
                tig_debug_remove_func(tig_debug_backends[index].func);
            }
        }
    }

    RegCloseKey(key);
#endif
}

// 0x4FED60
bool tig_debug_add_func(DebugOutputFunc* func)
{
    int index;

    for (index = 0; index < MAX_DEBUG_FUNCS; index++) {
        if (tig_debug_funcs[index] == func) {
            // Already exists.
            return false;
        }

        if (tig_debug_funcs[index] == NULL) {
            tig_debug_funcs[index] = func;
            have_debug_funcs = true;

            // Added.
            return true;
        }
    }

    // No room.
    return false;
}

// 0x4FEDA0
bool tig_debug_remove_func(DebugOutputFunc* func)
{
    int funcs_present = 0;
    int index;

    for (index = 0; index < MAX_DEBUG_FUNCS; index++) {
        if (tig_debug_funcs[index] != NULL) {
            funcs_present++;
            if (tig_debug_funcs[index] == func) {
                tig_debug_funcs[index] = NULL;

                // FIXME: Looks like a bug, if the function to remove is the
                // first, it thinks there are no other functions.
                if (funcs_present == 1) {
                    have_debug_funcs = false;
                }

                // Removed.
                return true;
            }
        }
    }

    // Not found.
    return false;
}

// 0x4FEDF0
void tig_debug_console_init()
{
#ifdef _WIN32
    if (AllocConsole()) {
        SetConsoleTitleA("Debug Output");
    }
#endif

    tig_debug_console_init_func = NULL;
}

// 0x4FEE10
void tig_debug_console_impl(const char* str)
{
#ifdef _WIN32
    DWORD chars_written;

    if (tig_debug_console_init_func != NULL) {
        tig_debug_console_init_func();
    }

    WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE),
        str,
        strlen(str) + 1,
        &chars_written,
        NULL);
#else
    (void)str;
#endif
}

// 0x4FEE50
void tig_debug_debugger_impl(const char* str)
{
#ifdef _WIN32
    OutputDebugStringA(str);
#else
    (void)str;
#endif
}

// 0x4FEE60
void tig_debug_file_init()
{
    if (tig_debug_file_handle == NULL) {
        tig_debug_file_handle = fopen("debug.txt", "w");
    }

    tig_debug_file_init_func = NULL;
}

// 0x4FEE90
void tig_debug_file_impl(const char* str)
{
    if (tig_debug_file_init_func != NULL) {
        tig_debug_file_init_func();
    }

    if (tig_debug_file_handle != NULL) {
        fputs(str, tig_debug_file_handle);
        fflush(tig_debug_file_handle);
    }
}

// 0x4FEEC0
void tig_debug_mono_init()
{
    if (!tig_debug_mono_initialized) {
        memset(VGA_PTR, 0, VGA_SIZE);
        mono_dest = VGA_PTR;
        tig_debug_mono_initialized = true;
    }

    tig_debug_mono_init_func = NULL;
}

// 0x4FEF00
void tig_debug_mono_impl(const char* str)
{
    if (tig_debug_mono_init_func != NULL) {
        tig_debug_mono_init_func();
    }

    if (!tig_debug_mono_initialized) {
        return;
    }

    while (*str != '\0') {
        if (*str == '\f') {
            mono_dest = VGA_PTR;
            memset(mono_dest, 0, VGA_SIZE);
        } else {
            switch (*str) {
            case '\n':
                mono_dest += VGA_WIDTH - (mono_dest - VGA_PTR) % VGA_WIDTH;
                break;
            case '\r':
                mono_dest -= (mono_dest - VGA_PTR) % VGA_WIDTH;
                break;
            case '\t':
                mono_dest += 8;
                break;
            case '\v':
                mono_dest += VGA_WIDTH;
                break;
            default:
                *mono_dest++ = *str;
                *mono_dest++ = 7;
                break;
            }
        }

        if (mono_dest >= VGA_PTR + VGA_SIZE) {
            memcpy(VGA_PTR, VGA_PTR + VGA_WIDTH, VGA_SIZE - VGA_WIDTH);
            memset(VGA_PTR + VGA_SIZE - VGA_WIDTH, 0, VGA_WIDTH);
            mono_dest = (mono_dest - VGA_PTR) % VGA_HEIGHT + VGA_PTR + VGA_SIZE - VGA_WIDTH;
        }
        str++;
    }
}
