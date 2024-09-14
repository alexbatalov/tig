#ifndef TIG_DXINPUT_H_
#define TIG_DXINPUT_H_

#define DIRECTINPUT_VERSION 0x300
#include <dinput.h>

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initializes DXINPUT subsystem.
int tig_dxinput_init(TigInitInfo* init_info);

// Shutdowns DXINPUT subsystem.
void tig_dxinput_exit();

// Retrieves the `IDirectInput` instance.
int tig_dxinput_get_instance(LPDIRECTINPUTA* direct_input_ptr);

#ifdef __cplusplus
}
#endif

#endif /* TIG_DXINPUT_H_ */
