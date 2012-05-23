/* vim: set et sw=4 sts=4: */

/*
 * Copyright © 2010-2011, Clément Bœsch
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
            memcpy(dst, colors[(byte1>>x & 1) << 1 | (byte2>>x & 1)], BPP);
            dst += BPP;
        }
    }
}

void load_flip_tile(u8 *dst, const u8 *src, int color_key)
{
    char **colors = color_set[color_key];

    for (int y = 0; y < 8; y++) {
        u8 byte1 = *src++;
        u8 byte2 = *src++;

        for (int x = 0; x < 8; x++) {
            memcpy(dst, colors[(byte1>>x & 1) << 1 | (byte2>>x & 1)], BPP);
            dst += BPP;
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
