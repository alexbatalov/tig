#ifndef BINK_COMPAT_H_
#define BINK_COMPAT_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BINKCALL __stdcall

#define BINKSNDTRACK 0x4000

typedef struct BINK* HBINK;

typedef int(BINKCALL* BINKSNDOPEN)(void* BnkSnd, unsigned freq, int bits, int chans, unsigned flags, HBINK bnk);
typedef BINKSNDOPEN(BINKCALL* BINKSNDSYSOPEN)(unsigned param);

typedef struct BINK {
    unsigned Width;
    unsigned Height;
    unsigned Frames;
    unsigned FrameNum;
} BINK;

void BINKCALL BinkClose(HBINK bnk);
int BINKCALL BinkCopyToBuffer(HBINK bnk, void* dest, int destpitch, unsigned destheight, unsigned destx, unsigned desty, unsigned flags);
int BINKCALL BinkDDSurfaceType(void* lpDDS);
int BINKCALL BinkDoFrame(HBINK bnk);
void BINKCALL BinkNextFrame(HBINK bnk);
HBINK BINKCALL BinkOpen(const char* name, unsigned flags);
BINKSNDOPEN BINKCALL BinkOpenMiles(unsigned param);
int BINKCALL BinkSetSoundSystem(BINKSNDSYSOPEN open, unsigned param);
void BINKCALL BinkSetSoundTrack(unsigned track);
int BINKCALL BinkWait(HBINK bnk);

bool bink_compat_init();
void bink_compat_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* BINK_COMPAT_H_ */
