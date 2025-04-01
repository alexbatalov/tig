#include "tig/movie.h"

#include <bink_compat.h>
#include <mss_compat.h>

#include "tig/color.h"
#include "tig/core.h"
#include "tig/kb.h"
#include "tig/message.h"
#include "tig/video.h"
#include "tig/window.h"

static bool tig_movie_do_frame();

// 0x62B2B4
static LPDIRECTDRAWSURFACE7 tig_movie_surface;

// 0x62B2B8
static unsigned int tig_movie_flags;

// 0x62B2BC
static HBINK tig_movie_bink;

// 0x5314F0
int tig_movie_init(TigInitInfo* init_info)
{
    HDIGDRIVER drvr;

    (void)init_info;

    // COMPAT: Load `binkw32.dll`.
    bink_compat_init();

    AIL_quick_handles(&drvr, NULL, NULL);
    BinkSetSoundSystem(BinkOpenMiles, (unsigned)drvr);

    return TIG_OK;
}

// 0x531520
void tig_movie_exit()
{
    // COMPAT: Free binkw32.dll
    bink_compat_exit();
}

// 0x531530
int tig_movie_play(const char* path, unsigned int movie_flags, int sound_track)
{
    unsigned int bink_open_flags = 0;
    TigMessage message;
    bool stop;
    int key = -1;

    if (path == NULL) {
        tig_video_fade(0, 0, 0.0f, 1);
        return TIG_ERR_INVALID_PARAM;
    }

    tig_video_main_surface_get(&tig_movie_surface);
    tig_movie_flags = BinkDDSurfaceType(tig_movie_surface);
    if (tig_movie_flags == -1 || tig_movie_flags == 0) {
        tig_video_fade(0, 0, 0.0f, 1);
        return TIG_ERR_GENERIC;
    }

    if (sound_track != 0) {
        BinkSetSoundTrack(sound_track);
        bink_open_flags |= BINKSNDTRACK;
    }

    tig_movie_bink = BinkOpen(path, bink_open_flags);
    if (tig_movie_bink == NULL) {
        tig_video_fade(0, 0, 0.0f, 1);
        return TIG_ERR_IO;
    }

    if ((movie_flags & TIG_MOVIE_FADE_IN) != 0) {
        tig_video_fade(0, 64, 3.0f, 0);
    }

    // Reset to black.
    tig_video_fill(NULL, tig_color_make(0, 0, 0));
    tig_video_flip();

    tig_video_fade(0, 0, 0.0f, 1);

    // Clear message queue.
    while (tig_message_dequeue(&message) == TIG_OK) {
    }

    stop = false;
    while (!stop && tig_movie_do_frame()) {
        tig_video_flip();
        tig_ping();

        while (tig_message_dequeue(&message) == TIG_OK) {
            if ((movie_flags & TIG_MOVIE_IGNORE_KEYBOARD) == 0
                && message.type == TIG_MESSAGE_KEYBOARD
                && message.data.keyboard.pressed == 1) {
                key = message.data.keyboard.key;
                stop = true;
            }
        }
    }

    if (key != -1) {
        // Wait until the key is released.
        while (tig_kb_is_key_pressed(key)) {
            tig_ping();
        }
    }

    // Clear message queue.
    while (tig_message_dequeue(&message) == TIG_OK) {
    }

    BinkClose(tig_movie_bink);
    tig_movie_bink = NULL;

    tig_window_set_needs_display_in_rect(NULL);

    if ((movie_flags & TIG_MOVIE_FADE_OUT) != 0) {
        tig_video_fade(0, 0, 0.0f, 0);
    }

    if ((movie_flags & TIG_MOVIE_BLACK_OUT) != 0) {
        tig_video_fill(NULL, tig_color_make(0, 0, 0));
        tig_video_flip();
    }

    return TIG_OK;
}

// NOTE: Original code probably provides access to the underlying BINK pointer.
// However this function is only used to check if movie is currently playing
// during message handling, so there is no point to expose it.
//
// 0x5317C0
bool tig_movie_is_playing()
{
    return tig_movie_bink != NULL;
}

// 0x5317D0
bool tig_movie_do_frame()
{
    DDSURFACEDESC2 ddsd;
    HRESULT hr;

    if (!BinkWait(tig_movie_bink)) {
        BinkDoFrame(tig_movie_bink);

        ddsd.dwSize = sizeof(ddsd);

        do {
            // NOTE: Looks odd, not sure how to make it a little bit more
            // readable.
            hr = IDirectDrawSurface_Lock(tig_movie_surface, NULL, &ddsd, DDLOCK_WAIT, NULL);
            if (hr == DDERR_SURFACELOST) {
                hr = IDirectDrawSurface_Restore(tig_movie_surface);
                if (hr != DD_OK) {
                    // Failed to restore surface, skip blitting.
                    break;
                } else {
                    // Attempt to lock once again.
                    continue;
                }
            }

            // Copy movie pixels to the center of the screen.
            BinkCopyToBuffer(tig_movie_bink,
                ddsd.lpSurface,
                ddsd.lPitch,
                tig_movie_bink->Height,
                (ddsd.dwWidth - tig_movie_bink->Width) / 2,
                (ddsd.dwHeight - tig_movie_bink->Height) / 2,
                tig_movie_flags);

            // FIXME: Should be rect, not surface.
            IDirectDrawSurface_Unlock(tig_movie_surface, (LPRECT)ddsd.lpSurface);
        } while (0);

        if (tig_movie_bink->FrameNum == tig_movie_bink->Frames) {
            return false;
        }

        BinkNextFrame(tig_movie_bink);
    }

    return true;
}
