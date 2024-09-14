// BMP UTILS
// ---
//
// The BMP UTILS module provides a set of utility functions to convert 24-bit
// BGR surface to 8-bit palette-indexed surface.
//
// NOTES
//
// - The name is wrong. Given it's location in the binary the name should
// start with `D`.
//
// - These functions are never used in Arcanum. They are referenced inside
// screenshotting routine in VIDEO module, but the calling code never sets
// appropriate flag to activate proper code path. ToEE has not been checked.
//
// - The implementation is rather obscure. I decided not to invest time into
// deep understanding of the math behind computations.

#include "tig/bmp_utils.h"

#include <stdio.h>
#include <string.h>

static int sub_53A5D0(int r, int g, int b);
static void sub_53A6B0(int alpha, int index, int b, int g, int r);
static void sub_53A730();

// 0x636F24
static int dword_636F24[256];

// 0x637324
static int dword_637324[256][4];

// 0x638324
static uint8_t* off_638324;

// 0x638328
static int dword_638328;

// 0x63832C
static int dword_63832C;

// 0x638330
static int dword_638330[256];

// 0x638730
static int dword_638730[256];

// 0x638B30
static int dword_638B30[32];

// NOTE: Odd address.
//
// 0x739E80
static int dword_739E80;

// 0x53A2A0
void sub_53A2A0(uint8_t* pixels, int size, int a3)
{
    int index;

    off_638324 = pixels;
    dword_63832C = size;
    dword_638328 = a3;

    memset(dword_638330, 0, sizeof(dword_638330));

    for (index = 0; index < 256; index++) {
        dword_636F24[index] = 256;
    }

    for (index = 0; index < 256; index++) {
        dword_637324[index][0] = (index << 12) / 256;
        dword_637324[index][1] = (index << 12) / 256;
        dword_637324[index][2] = (index << 12) / 256;
    }
}

// 0x53A310
void sub_53A310()
{
    int index;
    int cc;

    for (index = 0; index < 256; index++) {
        for (cc = 0; cc < 3; cc++) {
            dword_637324[index][cc] >>= 4;
        }
        dword_637324[index][3] = index;
    }
}

// 0x53A390
void sub_53A390(uint8_t* palette)
{
    int index;
    int cc;

    for (index = 0; index < 256; index++) {
        for (cc = 0; cc < 3; cc++) {
            *palette++ = (uint8_t)dword_637324[index][cc];
        }
    }
}

// 0x53A3C0
void sub_53A3C0()
{
    int index;
    int tmp;
    int v1 = 0;
    int v2 = 0;
    int v3;
    int v4;
    int v5;

    for (index = 0; index < 256; index++) {
        v3 = dword_637324[index][1];
        v5 = index;
        for (v4 = index + 1; v4 < 256; v4++) {
            if (dword_637324[v4][1] < v3) {
                v5 = v4;
                v3 = dword_637324[v4][1];
            }
        }

        if (index != v5) {
            tmp = dword_637324[v5][0];
            dword_637324[v5][0] = dword_637324[index][0];
            dword_637324[index][0] = tmp;

            tmp = dword_637324[v5][1];
            dword_637324[v5][1] = dword_637324[index][1];
            dword_637324[index][1] = tmp;

            tmp = dword_637324[v5][2];
            dword_637324[v5][2] = dword_637324[index][2];
            dword_637324[index][2] = tmp;

            tmp = dword_637324[v5][3];
            dword_637324[v5][3] = dword_637324[index][3];
            dword_637324[index][3] = tmp;
        }

        if (v3 != v1) {
            dword_638730[v1] = (index + v2) >> 1;
            while (v1 + 1 < v3) {
                dword_638730[v1 + 1] = index;
                v1++;
            }
            v1 = v3;
            v2 = index;
        }
    }

    dword_638730[v1] = (v2 + 255) >> 1;

    while (v1 + 1 < 256) {
        dword_638730[v1 + 1] = 255;
        v1++;
    }
}

// 0x53A4D0
int sub_53A4D0(int b, int g, int r)
{
    int delta_g;
    int delta_b;
    int delta_r;
    int delta;
    int v1 = dword_638730[g];
    int v2 = 1000;
    int v3 = -1;
    int v4 = v1 - 1;

    while (v1 < 256 || v4 >= 0) {
        if (v1 < 256) {
            delta_g = dword_637324[v1][1] - g;
            if (delta_g < v2) {
                if (delta_g < 0) {
                    delta_g = g - dword_637324[v1][1];
                }

                delta_b = dword_637324[v1][0] - b;
                if (delta_b < 0) {
                    delta_b = b - dword_637324[v1][0];
                }

                delta = delta_b + delta_g;
                if (delta < v2) {
                    delta_r = dword_637324[v1][2] - r;
                    if (delta_r < 0) {
                        delta_r = r - dword_637324[v1][2];
                    }

                    delta += delta_r;
                    if (delta < v2) {
                        v2 = delta;
                        v3 = dword_637324[v1][3];
                    }
                }
                v1++;
            } else {
                v1 = 256;
            }
        }

        if (v4 >= 0) {
            delta_g = g - dword_637324[v4][1];
            if (delta_g < v2) {
                if (delta_g < 0) {
                    delta_g = dword_637324[v4][1] - g;
                }

                delta_b = dword_637324[v4][0] - b;
                if (delta_b < 0) {
                    delta_b = b - dword_637324[v4][0];
                }

                delta = delta_b + delta_g;
                if (delta < v2) {
                    delta_r = dword_637324[v4][2] - r;
                    if (delta_r < 0) {
                        delta_r = r - dword_637324[v4][2];
                    }

                    delta += delta_r;
                    if (delta < v2) {
                        v2 = delta;
                        v3 = dword_637324[v4][3];
                    }
                }
                v4--;
            } else {
                v4 = -1;
            }
        }
    }

    return v3;
}

// 0x53A5D0
int sub_53A5D0(int b, int g, int r)
{
    int index;
    int delta_b;
    int delta_g;
    int delta_r;
    int delta;
    int v1 = 0x7FFFFFFF;
    int v2 = 0x7FFFFFFF;
    int v3 = -1;
    int v4 = -1;
    int v5;
    int v6;
    int v7;

    for (index = 0; index < 256; index++) {
        delta_b = dword_637324[index][0] - b;
        if (delta_b < 0) {
            delta_b = b - dword_637324[index][0];
        }

        delta_g = dword_637324[index][1] - g;
        if (delta_g < 0) {
            delta_g = g - dword_637324[index][1];
        }

        delta_r = dword_637324[index][2] - r;
        if (delta_r < 0) {
            delta_r = r - dword_637324[index][2];
        }

        delta = delta_b + delta_g + delta_r;
        if (delta < v2) {
            v2 = delta;
            v3 = index;
        }

        v5 = dword_638330[index];
        v6 = delta - (v5 >> 12);
        if (v6 < v1) {
            v1 = v6;
            v4 = index;
        }

        v7 = dword_636F24[index];
        dword_636F24[index] = v7 - (v7 >> 10);

        dword_638330[index] = v5 + ((v7 >> 10) << 10);
    }

    dword_636F24[v3] += 64;
    dword_638330[v3] -= 64 << 10;
    return v4;
}

// 0x53A6B0
void sub_53A6B0(int alpha, int index, int b, int g, int r)
{
    dword_637324[index][0] -= alpha * (dword_637324[index][0] - b) / 1024;
    dword_637324[index][1] -= alpha * (dword_637324[index][1] - g) / 1024;
    dword_637324[index][2] -= alpha * (dword_637324[index][2] - r) / 1024;
}

// 0x53A730
void sub_53A730(int radius, int a2, int b, int g, int r)
{
    int v1;
    int v2;
    int v3;
    int v4;
    int* v5;

    v1 = a2 - radius;
    if (v1 < -1) {
        v1 = -1;
    }

    v2 = a2 + radius;
    if (v2 > 256) {
        v2 = 256;
    }

    v3 = a2 - 1;
    v4 = a2 + 1;
    v5 = dword_638B30;

    while (v4 < v2 || v3 > v1) {
        v5++;

        if (v4 < v2) {
            dword_637324[v4][0] -= (*v5 * (dword_637324[v4][0] - b)) / 0x40000;
            dword_637324[v4][1] -= (*v5 * (dword_637324[v4][1] - g)) / 0x40000;
            dword_637324[v4][2] -= (*v5 * (dword_637324[v4][2] - r)) / 0x40000;
            v4++;
        }

        if (v3 > v1) {
            dword_637324[v3][0] -= (*v5 * (dword_637324[v3][0] - b)) / 0x40000;
            dword_637324[v3][1] -= (*v5 * (dword_637324[v3][1] - g)) / 0x40000;
            dword_637324[v3][2] -= (*v5 * (dword_637324[v3][2] - r)) / 0x40000;
            v3--;
        }
    }
}

// 0x53A8B0
void sub_53A8B0()
{
    uint8_t* src;
    uint8_t* end;
    int alpha;
    int radius;
    int index;
    int step;
    int b;
    int g;
    int r;
    int v1;
    int v2;
    int v3;
    int v4;
    int v5;

    src = off_638324;
    end = src + dword_63832C;
    dword_739E80 = (dword_638328 - 1) / 3 + 30;
    alpha = 1024;
    v1 = 2048;
    radius = 32;
    v2 = dword_63832C / (3 * dword_638328);
    v3 = v2 / 100;

    for (index = 0; index < radius; index++) {
        dword_638B30[index] = (((1024 - index * index) << 8) / 1024) << 10;
    }

    fprintf(stderr, "beginning 1D learning: initial radius=%d\n", radius);

    if (dword_63832C % 499 != 0) {
        step = 1497;
    } else if (dword_63832C % 491 != 0) {
        step = 1473;
    } else if (dword_63832C % 487 != 0) {
        step = 1461;
    } else {
        step = 1509;
    }

    index = 0;
    while (index < v2) {
        b = src[0] << 4;
        g = src[1] << 4;
        r = src[2] << 4;
        v5 = sub_53A5D0(b, g, r);
        sub_53A6B0(alpha, v5, b, g, r);
        if (radius != 0) {
            sub_53A730(radius, v5, b, g, r);
        }

        src += step;
        if (src >= end) {
            src -= dword_63832C;
        }

        index++;
        if (index % v3 == 0) {
            alpha -= alpha / dword_739E80;
            v1 -= v1 / 30;
            radius = v1 >> 6;
            if (radius <= 1) {
                radius = 0;
            }
            for (v4 = 0; v4 < radius; v4++) {
                dword_638B30[v4] = alpha * (((radius * radius - v4 * v4) << 8) / (radius * radius));
            }
        }
    }

    fprintf(stderr, "finished 1D learning: final alpha=%f !\n", alpha / 1024.0f);
}
