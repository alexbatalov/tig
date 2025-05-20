#include "mss_compat.h"

#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#endif

typedef signed int S32;
typedef unsigned int U32;

typedef void(AILCALL* AIL_CLOSE_STREAM)(HSTREAM stream);
typedef int(AILCALL* AIL_DIGITAL_HANDLE_REACQUIRE)(HDIGDRIVER drvr);
typedef int(AILCALL* AIL_DIGITAL_HANDLE_RELEASE)(HDIGDRIVER drvr);
typedef HSTREAM(AILCALL* AIL_OPEN_STREAM)(HDIGDRIVER dig, const char* filename, int stream_mem);
typedef void(AILCALL* AIL_QUICK_HANDLES)(HDIGDRIVER* pdig, HMDIDRIVER* pmdi, HDLSDEVICE* pdls);
typedef HAUDIO(AILCALL* AIL_QUICK_LOAD_MEM)(void const* mem, unsigned size);
typedef int(AILCALL* AIL_QUICK_PLAY)(HAUDIO audio, unsigned loop_count);
typedef void(AILCALL* AIL_QUICK_SET_VOLUME)(HAUDIO audio, int volume, int extravol);
typedef void(AILCALL* AIL_QUICK_SHUTDOWN)(void);
typedef int(AILCALL* AIL_QUICK_STARTUP)(S32 use_digital, S32 use_MIDI, U32 output_rate, S32 output_bits, S32 output_channels);
typedef int(AILCALL* AIL_QUICK_STATUS)(HAUDIO audio);
typedef void(AILCALL* AIL_QUICK_UNLOAD)(HAUDIO audio);
typedef char*(AILCALL* AIL_SET_REDIST_DIRECTORY)(const char* dir);
typedef void(AILCALL* AIL_SET_STREAM_LOOP_COUNT)(HSTREAM stream, int count);
typedef void(AILCALL* AIL_SET_STREAM_VOLUME)(HSTREAM stream, int volume);
typedef void(AILCALL* AIL_START_STREAM)(HSTREAM stream);
typedef int(AILCALL* AIL_STREAM_STATUS)(HSTREAM stream);

#ifdef _WIN32
static HMODULE mss32;
#endif

static AIL_CLOSE_STREAM mss32_AIL_close_stream;
static AIL_DIGITAL_HANDLE_REACQUIRE mss32_AIL_digital_handle_reacquire;
static AIL_DIGITAL_HANDLE_RELEASE mss32_AIL_digital_handle_release;
static AIL_OPEN_STREAM mss32_AIL_open_stream;
static AIL_QUICK_HANDLES mss32_AIL_quick_handles;
static AIL_QUICK_LOAD_MEM mss32_AIL_quick_load_mem;
static AIL_QUICK_PLAY mss32_AIL_quick_play;
static AIL_QUICK_SET_VOLUME mss32_AIL_quick_set_volume;
static AIL_QUICK_SHUTDOWN mss32_AIL_quick_shutdown;
static AIL_QUICK_STARTUP mss32_AIL_quick_startup;
static AIL_QUICK_STATUS mss32_AIL_quick_status;
static AIL_QUICK_UNLOAD mss32_AIL_quick_unload;
static AIL_SET_REDIST_DIRECTORY mss32_AIL_set_redist_directory;
static AIL_SET_STREAM_LOOP_COUNT mss32_AIL_set_stream_loop_count;
static AIL_SET_STREAM_VOLUME mss32_AIL_set_stream_volume;
static AIL_START_STREAM mss32_AIL_start_stream;
static AIL_STREAM_STATUS mss32_AIL_stream_status;

void AILCALL AIL_close_stream(HSTREAM stream)
{
    if (mss32_AIL_close_stream != NULL) {
        mss32_AIL_close_stream(stream);
    }
}

int AILCALL AIL_digital_handle_reacquire(HDIGDRIVER drvr)
{
    if (mss32_AIL_digital_handle_reacquire != NULL) {
        return mss32_AIL_digital_handle_reacquire(drvr);
    } else {
        return -1;
    }
}

int AILCALL AIL_digital_handle_release(HDIGDRIVER drvr)
{
    if (mss32_AIL_digital_handle_release != NULL) {
        return mss32_AIL_digital_handle_release(drvr);
    } else {
        return -1;
    }
}

HSTREAM AILCALL AIL_open_stream(HDIGDRIVER dig, const char* filename, int stream_mem)
{
    if (mss32_AIL_open_stream != NULL) {
        return mss32_AIL_open_stream(dig, filename, stream_mem);
    } else {
        return NULL;
    }
}

void AILCALL AIL_quick_handles(HDIGDRIVER* pdig, HMDIDRIVER* pmdi, HDLSDEVICE* pdls)
{
    if (mss32_AIL_quick_handles != NULL) {
        mss32_AIL_quick_handles(pdig, pmdi, pdls);
    } else {
        if (pdig != NULL) {
            *pdig = NULL;
        }

        if (pmdi != NULL) {
            *pmdi = NULL;
        }

        if (pdls != NULL) {
            *pdls = NULL;
        }
    }
}

HAUDIO AILCALL AIL_quick_load_mem(void const* mem, unsigned size)
{
    if (mss32_AIL_quick_load_mem != NULL) {
        return mss32_AIL_quick_load_mem(mem, size);
    } else {
        return NULL;
    }
}

int AILCALL AIL_quick_play(HAUDIO audio, unsigned loop_count)
{
    if (mss32_AIL_quick_play != NULL) {
        return mss32_AIL_quick_play(audio, loop_count);
    } else {
        return -1;
    }
}

void AILCALL AIL_quick_set_volume(HAUDIO audio, int volume, int extravol)
{
    if (mss32_AIL_quick_set_volume != NULL) {
        mss32_AIL_quick_set_volume(audio, volume, extravol);
    }
}

void AILCALL AIL_quick_shutdown(void)
{
    if (mss32_AIL_quick_shutdown != NULL) {
        mss32_AIL_quick_shutdown();
    }
}

int AILCALL AIL_quick_startup(int use_digital, int use_MIDI, unsigned output_rate, int output_bits, int output_channels)
{
    if (mss32_AIL_quick_startup != NULL) {
        return mss32_AIL_quick_startup(use_digital, use_MIDI, output_rate, output_bits, output_channels);
    } else {
        return -1;
    }
}

int AILCALL AIL_quick_status(HAUDIO audio)
{
    if (mss32_AIL_quick_status != NULL) {
        return mss32_AIL_quick_status(audio);
    } else {
        return 0;
    }
}

void AILCALL AIL_quick_unload(HAUDIO audio)
{
    if (mss32_AIL_quick_unload != NULL) {
        mss32_AIL_quick_unload(audio);
    }
}

char* AILCALL AIL_set_redist_directory(const char* dir)
{
    if (mss32_AIL_set_redist_directory != NULL) {
        return mss32_AIL_set_redist_directory(dir);
    } else {
        return NULL;
    }
}

void AILCALL AIL_set_stream_loop_count(HSTREAM stream, int count)
{
    if (mss32_AIL_set_stream_loop_count != NULL) {
        mss32_AIL_set_stream_loop_count(stream, count);
    }
}

void AILCALL AIL_set_stream_volume(HSTREAM stream, int volume)
{
    if (mss32_AIL_set_stream_volume != NULL) {
        mss32_AIL_set_stream_volume(stream, volume);
    }
}

void AILCALL AIL_start_stream(HSTREAM stream)
{
    if (mss32_AIL_start_stream != NULL) {
        mss32_AIL_start_stream(stream);
    }
}

int AILCALL AIL_stream_status(HSTREAM stream)
{
    if (mss32_AIL_stream_status != NULL) {
        return mss32_AIL_stream_status(stream);
    } else {
        return 0;
    }
}

bool mss_compat_init()
{
#ifdef _WIN32
    mss32 = LoadLibraryA("mss32.dll");
    if (mss32 == NULL) {
        return false;
    }

    mss32_AIL_close_stream = (AIL_CLOSE_STREAM)GetProcAddress(mss32, "_AIL_close_stream@4");
    mss32_AIL_digital_handle_reacquire = (AIL_DIGITAL_HANDLE_REACQUIRE)GetProcAddress(mss32, "_AIL_digital_handle_reacquire@4");
    mss32_AIL_digital_handle_release = (AIL_DIGITAL_HANDLE_RELEASE)GetProcAddress(mss32, "_AIL_digital_handle_release@4");
    mss32_AIL_open_stream = (AIL_OPEN_STREAM)GetProcAddress(mss32, "_AIL_open_stream@12");
    mss32_AIL_quick_handles = (AIL_QUICK_HANDLES)GetProcAddress(mss32, "_AIL_quick_handles@12");
    mss32_AIL_quick_load_mem = (AIL_QUICK_LOAD_MEM)GetProcAddress(mss32, "_AIL_quick_load_mem@8");
    mss32_AIL_quick_play = (AIL_QUICK_PLAY)GetProcAddress(mss32, "_AIL_quick_play@8");
    mss32_AIL_quick_set_volume = (AIL_QUICK_SET_VOLUME)GetProcAddress(mss32, "_AIL_quick_set_volume@12");
    mss32_AIL_quick_shutdown = (AIL_QUICK_SHUTDOWN)GetProcAddress(mss32, "_AIL_quick_shutdown@0");
    mss32_AIL_quick_startup = (AIL_QUICK_STARTUP)GetProcAddress(mss32, "_AIL_quick_startup@20");
    mss32_AIL_quick_status = (AIL_QUICK_STATUS)GetProcAddress(mss32, "_AIL_quick_status@4");
    mss32_AIL_quick_unload = (AIL_QUICK_UNLOAD)GetProcAddress(mss32, "_AIL_quick_unload@4");
    mss32_AIL_set_redist_directory = (AIL_SET_REDIST_DIRECTORY)GetProcAddress(mss32, "_AIL_set_redist_directory@4");
    mss32_AIL_set_stream_loop_count = (AIL_SET_STREAM_LOOP_COUNT)GetProcAddress(mss32, "_AIL_set_stream_loop_count@8");
    mss32_AIL_set_stream_volume = (AIL_SET_STREAM_VOLUME)GetProcAddress(mss32, "_AIL_set_stream_volume@8");
    mss32_AIL_start_stream = (AIL_START_STREAM)GetProcAddress(mss32, "_AIL_start_stream@4");
    mss32_AIL_stream_status = (AIL_STREAM_STATUS)GetProcAddress(mss32, "_AIL_stream_status@4");

    if (mss32_AIL_close_stream == NULL
        || mss32_AIL_digital_handle_reacquire == NULL
        || mss32_AIL_digital_handle_release == NULL
        || mss32_AIL_open_stream == NULL
        || mss32_AIL_quick_handles == NULL
        || mss32_AIL_quick_load_mem == NULL
        || mss32_AIL_quick_play == NULL
        || mss32_AIL_quick_set_volume == NULL
        || mss32_AIL_quick_shutdown == NULL
        || mss32_AIL_quick_startup == NULL
        || mss32_AIL_quick_status == NULL
        || mss32_AIL_quick_unload == NULL
        || mss32_AIL_set_redist_directory == NULL
        || mss32_AIL_set_stream_loop_count == NULL
        || mss32_AIL_set_stream_volume == NULL
        || mss32_AIL_start_stream == NULL
        || mss32_AIL_stream_status == NULL) {
        mss_compat_exit();
        return false;
    }
#endif

    return true;
}

void mss_compat_exit(void)
{
#ifdef _WIN32
    if (mss32 != NULL) {
        FreeLibrary(mss32);
        mss32 = NULL;

        mss32_AIL_close_stream = NULL;
        mss32_AIL_digital_handle_reacquire = NULL;
        mss32_AIL_digital_handle_release = NULL;
        mss32_AIL_open_stream = NULL;
        mss32_AIL_quick_handles = NULL;
        mss32_AIL_quick_load_mem = NULL;
        mss32_AIL_quick_play = NULL;
        mss32_AIL_quick_set_volume = NULL;
        mss32_AIL_quick_shutdown = NULL;
        mss32_AIL_quick_startup = NULL;
        mss32_AIL_quick_status = NULL;
        mss32_AIL_quick_unload = NULL;
        mss32_AIL_set_redist_directory = NULL;
        mss32_AIL_set_stream_loop_count = NULL;
        mss32_AIL_set_stream_volume = NULL;
        mss32_AIL_start_stream = NULL;
        mss32_AIL_stream_status = NULL;
    }
#endif
}
