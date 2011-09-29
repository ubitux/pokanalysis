/* vim: set et sw=4 sts=4: */

/*
 *  This file is part of Pokanalysis.
 *
 *  Pokanalysis
 *  Copyright © 2010-2011 Clément Bœsch
 *
 *  Pokanalysis is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Pokanalysis is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Pokanalysis.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "pokerom.h"

#define ZERO_CHAR 0xF6
//#define ZERO_CHAR '0'

static u8 consts[][3] = {
    {0x0F, 0x42, 0x40},
    {0x01, 0x86, 0xA0},
    {0x00, 0x27, 0x10},
    {0x00, 0x03, 0xE8},
    {0x00, 0x00, 0x64},
};

static u8 V0, V1, V2, V3;

static void set_char(u8 *dest, u8 input_flag, u8 *C)    /* Red: ROM0:3D25 */
{
    u8 c = 0;
    u8 B1, B2; //, B3;

    while (1) {
        B1 = V1;
        if (V1 < C[0])
            break;
        V1 -= C[0];

        B2 = V2;
        if (V2 < C[1]) {
            if (V1 == 0) {
                V1 = B1;
                break;
            }
            V1--;
        }
        V2 -= C[1];

        //B3 = V3; FIXME: why?
        if (V3 < C[2]) {
            if (V2) {
                V2--;
            } else if (V1 == 0) {
                V2 = B2;
                V1 = B1;
                break;
            } else {
                V1--;
                V2 = 0xFF;
            }
        }

        // 3D69
        V3 -= C[2];
        c++;
    }

    // 3D77
    if ((V0 | c) == 0) {
        if (input_flag & (1 << 7))
            *dest = ZERO_CHAR;
        return;
    }
    V0 = *dest = ZERO_CHAR + c;
}

/* Red: ROM0:3D89 */
#define CHECK_PUTNBR_DEST_INC() do {                           \
    if ((input_flag & 1<<7) || (input_flag & 1<<6) == 0 || V0) \
        dest++;                                                \
} while (0)

void pkmn_put_nbr(u8 *dest, u8 *src, u8 input_flag, u8 precision)   /* Red: ROM0:3C5F */
{
    int c;

    V0 = 0;
    V1 = 0;
    V2 = 0;

    u8 n;

    switch (input_flag & 0x0f) {
    default:  V1 = src[0]; V2 = src[1]; V3 = src[2]; break;
    case 2:                V2 = src[0]; V3 = src[1]; break;
    case 1:                             V3 = src[0]; break;
    }

    switch (precision) {
    default: set_char(dest, input_flag, consts[0]); CHECK_PUTNBR_DEST_INC();
    case 6:  set_char(dest, input_flag, consts[1]); CHECK_PUTNBR_DEST_INC();
    case 5:  set_char(dest, input_flag, consts[2]); CHECK_PUTNBR_DEST_INC();
    case 4:  set_char(dest, input_flag, consts[3]); CHECK_PUTNBR_DEST_INC();
    case 3:  set_char(dest, input_flag, consts[4]); CHECK_PUTNBR_DEST_INC();
    case 2:  break;
    }

    c = 0;
    n = V3;

    // 3D00
    while (n >= 10) {
        n -= 10;
        c++;
    }

    // 3D09
    V0 |= c;
    if (V0) {
        *dest = ZERO_CHAR + c;
    } else {
        if (input_flag & 1<<7)
            *dest = ZERO_CHAR;
    }

    // 3D1A
    CHECK_PUTNBR_DEST_INC();
    *dest++ = ZERO_CHAR + n;
}
