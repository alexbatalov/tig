#ifndef TIG_BMP_UTILS_H_
#define TIG_BMP_UTILS_H_

#include "tig/types.h"

#ifdef __cplusplus
extern "C" {
#endif

void sub_53A2A0(uint8_t* pixels, int size, int a3);
void sub_53A310();
void sub_53A390(uint8_t* palette);
void sub_53A3C0();
int sub_53A4D0(int b, int g, int r);
void sub_53A8B0();

#ifdef __cplusplus
}
#endif

#endif /* TIG_BMP_UTILS_H_ */
