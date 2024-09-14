#include "tig/find_file.h"

#include <string.h>

// 0x533CD0
bool tig_find_first_file(const char* pattern, TigFindFileData* find_file_data)
{
    find_file_data->handle = _findfirst(pattern, &(find_file_data->find_data));
    return find_file_data->handle != -1;
}

// 0x533D00
bool tig_find_next_file(TigFindFileData* find_file_data)
{
    return _findnext(find_file_data->handle, &(find_file_data->find_data)) != -1;
}

// 0x533D20
bool tig_find_close(TigFindFileData* find_file_data)
{
    int rc = _findclose(find_file_data->handle);
    memset(find_file_data, 0, sizeof(*find_file_data));
    return rc != -1;
}
