#ifndef TIG_GUID_H_
#define TIG_GUID_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// For compatibility reasons this struct matches `GUID` from <guiddef.h>
typedef struct TigGuid {
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
} TigGuid;

static_assert(sizeof(TigGuid) == 16, "wrong size");

void tig_guid_create(TigGuid* guid);
bool tig_guid_is_equal(const TigGuid* a, const TigGuid* b);

#ifdef __cplusplus
}
#endif

#endif /* TIG_GUID_H_ */
