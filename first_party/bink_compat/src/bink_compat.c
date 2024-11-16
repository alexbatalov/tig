#include "bink_compat.h"

#include <windows.h>

typedef void(BINKCALL* BINKCLOSE)(HBINK);
typedef int(BINKCALL* BINKCOPYTOBUFFER)(HBINK, void*, int, unsigned, unsigned, unsigned, unsigned);
typedef int(BINKCALL* BINKDDSURFACETYPE)(void*);
typedef int(BINKCALL* BINKDOFRAME)(HBINK);
typedef void(BINKCALL* BINKNEXTFRAME)(HBINK);
typedef HBINK(BINKCALL* BINKOPEN)(const char*, unsigned);
typedef BINKSNDOPEN(BINKCALL* BINKOPENMILES)(unsigned);
typedef int(BINKCALL* BINKSETSOUNDSYSTEM)(BINKSNDSYSOPEN, unsigned);
typedef void(BINKCALL* BINKSETSOUNDTRACK)(unsigned);
typedef int(BINKCALL* BINKWAIT)(HBINK);

static HMODULE binkw32;
static BINKCLOSE _BinkClose;
static BINKCOPYTOBUFFER _BinkCopyToBuffer;
static BINKDDSURFACETYPE _BinkDDSurfaceType;
static BINKDOFRAME _BinkDoFrame;
static BINKNEXTFRAME _BinkNextFrame;
static BINKOPEN _BinkOpen;
static BINKOPENMILES _BinkOpenMiles;
static BINKSETSOUNDSYSTEM _BinkSetSoundSystem;
static BINKSETSOUNDTRACK _BinkSetSoundTrack;
static BINKWAIT _BinkWait;

void BINKCALL BinkClose(HBINK bnk)
{
    if (_BinkClose != NULL) {
        _BinkClose(bnk);
    }
}

int BINKCALL BinkCopyToBuffer(HBINK bnk, void* dest, int destpitch, unsigned destheight, unsigned destx, unsigned desty, unsigned flags)
{
    if (_BinkCopyToBuffer != NULL) {
        return _BinkCopyToBuffer(bnk, dest, destpitch, destheight, destx, desty, flags);
    } else {
        return -1;
    }
}

int BINKCALL BinkDDSurfaceType(void* lpDDS)
{
    if (_BinkDDSurfaceType != NULL) {
        return _BinkDDSurfaceType(lpDDS);
    } else {
        return -1;
    }
}

int BINKCALL BinkDoFrame(HBINK bnk)
{
    if (_BinkDoFrame != NULL) {
        return _BinkDoFrame(bnk);
    } else {
        return -1;
    }
}

void BINKCALL BinkNextFrame(HBINK bnk)
{
    if (_BinkNextFrame != NULL) {
        _BinkNextFrame(bnk);
    }
}

HBINK BINKCALL BinkOpen(const char* name, unsigned flags)
{
    if (_BinkOpen != NULL) {
        return _BinkOpen(name, flags);
    } else {
        return NULL;
    }
}

BINKSNDOPEN BINKCALL BinkOpenMiles(unsigned param)
{
    if (_BinkOpenMiles != NULL) {
        return _BinkOpenMiles(param);
    } else {
        return NULL;
    }
}

int BINKCALL BinkSetSoundSystem(BINKSNDSYSOPEN open, unsigned param)
{
    if (_BinkSetSoundSystem != NULL) {
        return _BinkSetSoundSystem(open, param);
    } else {
        return -1;
    }
}

void BINKCALL BinkSetSoundTrack(unsigned track)
{
    if (_BinkSetSoundTrack != NULL) {
        _BinkSetSoundTrack(track);
    }
}

int BINKCALL BinkWait(HBINK bnk)
{
    if (_BinkWait != NULL) {
        return _BinkWait(bnk);
    } else {
        return -1;
    }
}

bool bink_compat_init()
{
    binkw32 = LoadLibraryA("binkw32.dll");
    if (binkw32 == NULL) {
        return false;
    }

    _BinkClose = (BINKCLOSE)GetProcAddress(binkw32, "_BinkClose@4");
    _BinkCopyToBuffer = (BINKCOPYTOBUFFER)GetProcAddress(binkw32, "_BinkCopyToBuffer@28");
    _BinkDDSurfaceType = (BINKDDSURFACETYPE)GetProcAddress(binkw32, "_BinkDDSurfaceType@4");
    _BinkDoFrame = (BINKDOFRAME)GetProcAddress(binkw32, "_BinkDoFrame@4");
    _BinkNextFrame = (BINKNEXTFRAME)GetProcAddress(binkw32, "_BinkNextFrame@4");
    _BinkOpen = (BINKOPEN)GetProcAddress(binkw32, "_BinkOpen@8");
    _BinkOpenMiles = (BINKOPENMILES)GetProcAddress(binkw32, "_BinkOpenMiles@4");
    _BinkSetSoundSystem = (BINKSETSOUNDSYSTEM)GetProcAddress(binkw32, "_BinkSetSoundSystem@8");
    _BinkSetSoundTrack = (BINKSETSOUNDTRACK)GetProcAddress(binkw32, "_BinkSetSoundTrack@4");
    _BinkWait = (BINKWAIT)GetProcAddress(binkw32, "_BinkWait@4");

    if (_BinkClose == NULL
        || _BinkCopyToBuffer == NULL
        || _BinkDDSurfaceType == NULL
        || _BinkDoFrame == NULL
        || _BinkNextFrame == NULL
        || _BinkOpen == NULL
        || _BinkOpenMiles == NULL
        || _BinkSetSoundSystem == NULL
        || _BinkSetSoundTrack == NULL
        || _BinkWait == NULL) {
        bink_compat_exit();
        return false;
    }

    return true;
}

void bink_compat_exit(void)
{
    if (binkw32 != NULL) {
        FreeLibrary(binkw32);
        binkw32 = NULL;

        _BinkClose = NULL;
        _BinkCopyToBuffer = NULL;
        _BinkDDSurfaceType = NULL;
        _BinkDoFrame = NULL;
        _BinkNextFrame = NULL;
        _BinkOpen = NULL;
        _BinkOpenMiles = NULL;
        _BinkSetSoundSystem = NULL;
        _BinkSetSoundTrack = NULL;
        _BinkWait = NULL;
    }
}
