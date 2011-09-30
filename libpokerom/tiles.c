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

static char *color_set[][4] = {
    [DEFAULT_COLORS_OFFSET]  = {"\xE8\xE8\xE8", "\x58\x58\x58", "\xA0\xA0\xA0", "\x10\x10\x10"},
    [WARPS_COLORS_OFFSET]    = {"\xE8\xC0\xC0", "\xC0\x58\x58", "\xC0\xA0\xA0", "\xC0\x10\x10"},
    [SIGNS_COLORS_OFFSET]    = {"\xC0\xC0\xE8", "\x58\x58\xC0", "\xA0\xA0\xC0", "\x10\x10\xC0"},
    [ENTITIES_COLORS_OFFSET] = {"\xE8\xC0\xE8", "\xC0\x58\xC0", "\xC0\xA0\xC0", "\xC0\x10\xC0"},
};

void load_tile(u8 *dst, const u8 *src, int color_key)
{
    char **colors = color_set[color_key];

    for (int y = 0; y < 8; y++) {
        u8 byte1 = *src++;
        u8 byte2 = *src++;

        for (int x = 7; x >= 0; x--) {
            memcpy(dst, colors[(byte1>>x & 1) << 1 | (byte2>>x & 1)], 3);
            dst += 3;
        }
    }
}

void merge_tiles(u8 *dst, const u8 *src, int color_key)
{
    const char *alpha = color_set[color_key][0];
    for (int i = 0; i < 8*8 * BPP; i += BPP)
        if (memcmp(&src[i], alpha, BPP) != 0)
            memcpy(&dst[i], &src[i], BPP);
}

// TODO: make a load_flip_tile()
void flip_tile(u8 *tile)
{
    u8 old_tile[8*8 * BPP];

    memcpy(old_tile, tile, sizeof(old_tile));
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++)
            memcpy(&tile[BPP * (y*8 + x)], &old_tile[BPP * (y*8 + 8-x-1)], BPP);
}
