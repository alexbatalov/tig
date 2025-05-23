#ifndef TIG_FIND_FILE_H_
#define TIG_FIND_FILE_H_

#include <io.h>

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TigFindFileData {
    struct _finddata_t find_data;
    intptr_t handle;
} TigFindFileData;

bool tig_find_first_file(const char* pattern, TigFindFileData* find_file_data);
bool tig_find_next_file(TigFindFileData* find_file_data);
bool tig_find_close(TigFindFileData* find_file_data);

#ifdef __cplusplus
}
#endif

#endif /* TIG_FIND_FILE_H_ */
