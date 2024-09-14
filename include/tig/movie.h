#ifndef TIG_MOVIE_H_
#define TIG_MOVIE_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum TigMovieFlags {
    TIG_MOVIE_IGNORE_KEYBOARD = 0x01,
    TIG_MOVIE_FADE_IN = 0x02,
    TIG_MOVIE_FADE_OUT = 0x04,
    TIG_MOVIE_BLACK_OUT = 0x08,
} TigMovieFlags;

// Initializes movie system.
int tig_movie_init(TigInitInfo* init_info);

// Shutdowns movie system.
void tig_movie_exit();

// Play given movie file.
int tig_movie_play(const char* path, unsigned int movie_flags, int sound_track);

// Returns `true` if movie is currently being played.
bool tig_movie_is_playing();

#ifdef __cplusplus
}
#endif

#endif /* TIG_MOVIE_H_ */
