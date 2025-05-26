#ifndef TIG_COMPAT_H_
#define TIG_COMPAT_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COMPAT_MAX_DRIVE 3
#define COMPAT_MAX_DIR 256
#define COMPAT_MAX_FNAME 256
#define COMPAT_MAX_EXT 256

void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext);
void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext);

#ifdef __cplusplus
}
#endif

#endif /* TIG_COMPAT_H_ */
