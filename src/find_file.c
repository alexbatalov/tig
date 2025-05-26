#include "tig/find_file.h"

#include <stdio.h>
#include <string.h>

#include "tig/memory.h"

typedef struct TigFindFileDataPriv {
    char dir[TIG_MAX_PATH];
    char** entries;
    int count;
    int index;
} TigFindFileDataPriv;

bool tig_find_first_file(const char* pattern, TigFindFileData* find_file_data)
{
    char path[TIG_MAX_PATH];
    char fname[TIG_MAX_PATH];
    char* pch;
    int count;
    char** entries;
    TigFindFileDataPriv* priv;

    find_file_data->name[0] = '\0';
    find_file_data->handle = 0;

    pch = strrchr(pattern, '\\');
    if (pch != NULL) {
        strncpy(path, pattern, pch - pattern);
        path[pch - pattern] = '\0';

        strcpy(fname, pch + 1);
    } else {
        path[0] = '.';
        path[1] = '\0';

        strcpy(fname, pattern);
    }

    entries = SDL_GlobDirectory(path, fname, SDL_GLOB_CASEINSENSITIVE, &count);
    if (count == 0) {
        return false;
    }

    priv = (TigFindFileDataPriv*)MALLOC(sizeof(*priv));
    strcpy(priv->dir, path);
    priv->entries = entries;
    priv->count = count;
    priv->index = 0;
    find_file_data->handle = (intptr_t)priv;

    if (!tig_find_next_file(find_file_data)) {
        SDL_free(entries);
        FREE(priv);
        find_file_data->handle = 0;
        return false;
    }

    return true;
}

bool tig_find_next_file(TigFindFileData* find_file_data)
{
    TigFindFileDataPriv* priv;
    char path[TIG_MAX_PATH];

    priv = (TigFindFileDataPriv*)find_file_data->handle;
    if (priv == NULL) {
        return false;
    }

    if (priv->index >= priv->count) {
        return false;
    }

    snprintf(path, sizeof(path), "%s\\%s", priv->dir, priv->entries[priv->index]);
    if (!SDL_GetPathInfo(path, &(find_file_data->path_info))) {
        return false;
    }

    strcpy(find_file_data->name, priv->entries[priv->index]);
    priv->index++;

    return true;
}

bool tig_find_close(TigFindFileData* find_file_data)
{
    TigFindFileDataPriv* priv;

    priv = (TigFindFileDataPriv*)find_file_data->handle;
    if (priv == NULL) {
        return false;
    }

    SDL_free(priv->entries);
    FREE(priv);

    memset(find_file_data, 0, sizeof(*find_file_data));

    return true;
}
