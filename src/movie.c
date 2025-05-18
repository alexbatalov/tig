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

// 0x62B2BC
static HBINK tig_movie_bink;

static TigVideoBuffer* tig_movie_video_buffer;
static int tig_movie_screen_width;
static int tig_movie_screen_height;

// 0x5314F0
int tig_movie_init(TigInitInfo* init_info)
{
    HDIGDRIVER drvr;

    // COMPAT: Load `binkw32.dll`.
    bink_compat_init();

    AIL_quick_handles(&drvr, NULL, NULL);
    BinkSetSoundSystem(BinkOpenMiles, (unsigned)drvr);

    tig_movie_screen_width = init_info->width;
    tig_movie_screen_height = init_info->height;

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
    TigVideoBufferCreateInfo vb_create_info;

    if (path == NULL) {
        tig_video_fade(0, 0, 0.0f, 1);
        return TIG_ERR_INVALID_PARAM;
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

    vb_create_info.flags = TIG_VIDEO_BUFFER_CREATE_SYSTEM_MEMORY;
    vb_create_info.background_color = 0;
    vb_create_info.color_key = 0;
    vb_create_info.width = tig_movie_bink->Width;
    vb_create_info.height = tig_movie_bink->Height;

    if (tig_video_buffer_create(&vb_create_info, &tig_movie_video_buffer) != TIG_OK) {
        BinkClose(tig_movie_bink);
        return TIG_ERR_GENERIC;
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

    tig_video_buffer_destroy(tig_movie_video_buffer);
    tig_movie_video_buffer = NULL;

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
    TigVideoBufferData video_buffer_data;
    TigRect src_rect;
    TigRect dst_rect;

    if (!BinkWait(tig_movie_bink)) {
        BinkDoFrame(tig_movie_bink);

        if (tig_video_buffer_lock(tig_movie_video_buffer) != TIG_OK) {
            return false;
        }

        if (tig_video_buffer_data(tig_movie_video_buffer, &video_buffer_data) != TIG_OK) {
            return false;
        }

        src_rect.x = 0;
        src_rect.y = 0;
        src_rect.width = video_buffer_data.width;
        src_rect.height = video_buffer_data.height;

        dst_rect.x = (tig_movie_screen_width - video_buffer_data.width) / 2;
        dst_rect.y = (tig_movie_screen_height - video_buffer_data.height) / 2;
        dst_rect.width = video_buffer_data.width;
        dst_rect.height = video_buffer_data.height;

        // Copy movie pixels to the video buffer.
        BinkCopyToBuffer(tig_movie_bink,
            video_buffer_data.surface_data.pixels,
            video_buffer_data.pitch,
            video_buffer_data.height,
            0,
            0,
            3);

        tig_video_buffer_unlock(tig_movie_video_buffer);

        // Blit buffer to the center of the screen.
        tig_video_blit(tig_movie_video_buffer, &src_rect, &dst_rect);

        if (tig_movie_bink->FrameNum == tig_movie_bink->Frames) {
            return false;
        }

        BinkNextFrame(tig_movie_bink);
    }

    return true;
}
