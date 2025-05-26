#include "tig/compat.h"

#ifdef _WIN32
#include <stdlib.h>
#else
#include <dirent.h>
#endif

void compat_splitpath(const char* path, char* drive, char* dir, char* fname, char* ext)
{
#ifdef _WIN32
    _splitpath(path, drive, dir, fname, ext);
#else
    const char* driveStart = path;
    if (path[0] == '/' && path[1] == '/') {
        path += 2;
        while (*path != '\0' && *path != '/' && *path != '.') {
            path++;
        }
    }

    if (drive != NULL) {
        size_t driveSize = path - driveStart;
        if (driveSize > COMPAT_MAX_DRIVE - 1) {
            driveSize = COMPAT_MAX_DRIVE - 1;
        }
        strncpy(drive, path, driveSize);
        drive[driveSize] = '\0';
    }

    const char* dir_start = path;
    const char* fname_start = path;
    const char* ext_start = NULL;

    const char* end = path;
    while (*end != '\0') {
        if (*end == '/') {
            fname_start = end + 1;
        } else if (*end == '.') {
            ext_start = end;
        }
        end++;
    }

    if (ext_start == NULL) {
        ext_start = end;
    }

    if (dir != NULL) {
        size_t dirSize = fname_start - dir_start;
        if (dirSize > COMPAT_MAX_DIR - 1) {
            dirSize = COMPAT_MAX_DIR - 1;
        }
        strncpy(dir, path, dirSize);
        dir[dirSize] = '\0';
    }

    if (fname != NULL) {
        size_t fname_length = ext_start - fname_start;
        if (fname_length > COMPAT_MAX_FNAME - 1) {
            fname_length = COMPAT_MAX_FNAME - 1;
        }
        strncpy(fname, fname_start, fname_length);
        fname[fname_length] = '\0';
    }

    if (ext != NULL) {
        size_t ext_length = end - ext_start;
        if (ext_length > COMPAT_MAX_EXT - 1) {
            ext_length = COMPAT_MAX_EXT - 1;
        }
        strncpy(ext, ext_start, ext_length);
        ext[ext_length] = '\0';
    }
#endif
}

void compat_makepath(char* path, const char* drive, const char* dir, const char* fname, const char* ext)
{
#ifdef _WIN32
    _makepath(path, drive, dir, fname, ext);
#else
    path[0] = '\0';

    if (drive != NULL) {
        if (*drive != '\0') {
            strcpy(path, drive);
            path = strchr(path, '\0');

            if (path[-1] == '/') {
                path--;
            } else {
                *path = '/';
            }
        }
    }

    if (dir != NULL) {
        if (*dir != '\0') {
            if (*dir != '/' && *path == '/') {
                path++;
            }

            strcpy(path, dir);
            path = strchr(path, '\0');

            if (path[-1] == '/') {
                path--;
            } else {
                *path = '/';
            }
        }
    }

    if (fname != NULL && *fname != '\0') {
        if (*fname != '/' && *path == '/') {
            path++;
        }

        strcpy(path, fname);
        path = strchr(path, '\0');
    } else {
        if (*path == '/') {
            path++;
        }
    }

    if (ext != NULL) {
        if (*ext != '\0') {
            if (*ext != '.') {
                *path++ = '.';
            }

            strcpy(path, ext);
            path = strchr(path, '\0');
        }
    }

    *path = '\0';
#endif
}
