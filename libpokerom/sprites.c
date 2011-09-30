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

static inline u8 high_nibble(u8 c) { return c >> 4;      }
static inline u8 low_nibble (u8 c) { return c & 0x0f;    }
static inline u8 swap_u8    (u8 c) { return c<<4 | c>>4; }

static u8 sprite_width;     // d0a3
static u8 sprite_height;    // d0a4
static u8 buffer_flag;      // d0a8
static u8 misc_flag;        // d0a9
static u16 p1;              // d0ad-d0ae
static u16 p2;              // d0af-d0b0

struct getbits {
    const u8 *stream;
    u8 byte;
    int bit;
};

static u8 get_next_bit(struct getbits *gb)
{
    if (--gb->bit == 0) {
        gb->byte = *gb->stream++;
        gb->bit  = 8;
    }
    gb->byte = gb->byte<<1 | gb->byte>>7;
    return gb->byte & 1;
}

static u8 next_a(u8 a, int x) // 2649
{
    switch (x) {
    case 1:  return a << 2;
    case 2:  return swap_u8(a);
    case 3:  return (a&3)<<6 | a>>2;
    default: return a;
    }
}

static void reset_p1_p2(int b, u16 *ptr1, u16 *ptr2) // 2841
{
    if (b & 1) *ptr1 =     0, *ptr2 = 7*7*8;
    else       *ptr1 = 7*7*8, *ptr2 =     0;
}

static const u8 tileid_map[] = {
     0,  1,  3,  2,  7,  6,  4,  5, 15, 14, 12, 13,  8,  9, 11, 10,
    15, 14, 12, 13,  8,  9, 11, 10,  0,  1,  3,  2,  7,  6,  4,  5,
     0,  8, 12,  4, 14,  6,  2, 10, 15,  7,  3, 11,  1,  9, 13,  5,
    15,  7,  3, 11,  1,  9, 13,  5,  0,  8, 12,  4, 14,  6,  2, 10,
};

static u8 get_tile_id(int i, u16 cache, int flag)
{
    if (flag) return tileid_map[2*16 + (cache>>3 & 1) * 16 + i];
    else      return tileid_map[       (cache    & 1) * 16 + i];
}

static void load_data(u8 *dst, int flag)
{
    u16 cache = 0;

    for (int y = 0; y != sprite_height; y++) {
        u8 *d = dst;
        for (int x = 0; x != sprite_width; x += 8) {
            u8 nibble;

            nibble = get_tile_id(high_nibble(d[y]), cache,  flag);
            cache  = swap_u8(nibble)<<8 | nibble;
            nibble = get_tile_id(low_nibble(d[y]),  nibble, flag);
            cache  = (cache & 0xff00) | nibble;

            d[y] = nibble | cache>>8;
            d += sprite_height;
        }
        cache &= 0xff00;
    }
}

enum {Z_RET, Z_END, Z_START};

static const u8 col_interlaced_paths[] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};

static int uncompress_data(u8 *dst, int flag)
{
    reset_p1_p2(buffer_flag, &p1, &p2);
    load_data(dst + p1, flag);

    int i = p1, j = p2;
    for (int x = 0; x != sprite_width; x += 8) {
        for (int y = 0; y != sprite_height; y++) {
            if (flag) {
                u8 v = dst[j];
                dst[j] = col_interlaced_paths[high_nibble(v)+1]<<4 |
                         col_interlaced_paths[low_nibble(v)];
            }
            dst[j++] ^= dst[i++];
        }
    }
    return Z_END;
}

struct tile { int x, y; };

static int f25d8(u8 *dst, struct tile *tile, int *p_flag)
{
    if (tile->y+1 != sprite_height) {
        tile->y++;
        p1++;
        return Z_RET;
    }

    // 25F6
    tile->y = 0;
    if (*p_flag) {
        (*p_flag)--;
        p1 = p2;
        return Z_RET;
    }

    // 2610
    *p_flag = 3;
    tile->x += 8;
    if (tile->x != sprite_width) {
        p1 = p2 = p1 + 1;
        return Z_RET;
    }

    /* Break context here: calling function must end after processing */

    // 2630
    tile->x = 0;
    if (!(buffer_flag & 2)) {
        buffer_flag = (buffer_flag^1) | 2;
        return Z_START;
    }

    int input_flag = 0; // XXX: 0xd0aa; does not seem to work with ≠ 0

    // 2646 (-> 26BF)
    if (misc_flag == 2) {
        reset_p1_p2(buffer_flag, &p1, &p2);
        load_data(dst + p2, 0);
        return uncompress_data(dst, input_flag);
    }

    // 26C7
    if (misc_flag)
        return uncompress_data(dst, input_flag);

    // 26CB
    load_data(dst        , input_flag);
    load_data(dst + 7*7*8, input_flag);
    return Z_END;
}

static int f2595(u8 *dst, struct tile *tile, struct getbits *gb, int *p_flag)
{
    int bitlen, p = 1, idx;

    for (bitlen = 0; get_next_bit(gb); bitlen++)
        p = p<<1 | 1;

    for (idx = 0; bitlen >= 0; bitlen--)
        idx = idx<<1 | get_next_bit(gb);
    p += idx;

    do {
        dst[p1] |= next_a(0, *p_flag);
        int r = f25d8(dst, tile, p_flag);
        if (r != Z_RET)
            return r;
        p--;
    } while (p);

    return Z_RET;
}

static void uncompress_sprite(u8 *dst, const u8 *src) // 251A
{
    u8 byte, b;
    int r, p_flag = 3;
    struct getbits gb = {.stream=src, .bit=1};
    struct tile tile  = {.x = 0, .y = 0};

    memset(dst, 0, 0x310);

    byte = *gb.stream++;
    sprite_height = low_nibble(byte) * 8;
    sprite_width = high_nibble(byte) * 8;
    buffer_flag = get_next_bit(&gb);

    do {
        // 2556
        p1 = p2 = (buffer_flag & 1) * 7*7*8;
        if (buffer_flag & 2)
            misc_flag = get_next_bit(&gb) ? get_next_bit(&gb)+1 : 0;

        // 257A
        if (!get_next_bit(&gb)) {
            r = f2595(dst, &tile, &gb, &p_flag);
            if (r == Z_START) continue;
            if (r == Z_END)   return;
        }

        do {
            b = get_next_bit(&gb)<<1 | get_next_bit(&gb);
            if (b) {
                dst[p1] |= next_a(b, p_flag);
                r = f25d8(dst, &tile,      &p_flag);
            } else {
                r = f2595(dst, &tile, &gb, &p_flag);
            }
        } while (r == Z_RET);
    } while (r == Z_START);
}

static void merge_buffers(u8 *buffer)
{
    u8 *dest = buffer + 0x0497;
    u8 *src1 = buffer + 0x030F;
    u8 *src2 = buffer + 0x0187;
    int n;

    for (n = 0; n < 0xC4; n++) {
        *dest-- = *src1--;
        *dest-- = *src2--;
        *dest-- = *src1--;
        *dest-- = *src2--;
    }
}

static void fill_column(u8 *dst, u8 *src, int w, int h)
{
    for (int x = 0; x < w; x++, dst += 7*8)
        for (int y = 0; y < h; y++)
            dst[y] = *src++;
}

void load_sprite(u8 *pixbuf, const u8 *src, u8 sprite_dim)
{
    u8 b[3 * 0x188];

    int width    =     low_nibble(sprite_dim);
    int height   = 8 * high_nibble(sprite_dim);
    int top_skip = 8 * (7 * ((8 - width) >> 1) + 7 - high_nibble(sprite_dim));

    uncompress_sprite(b+0x188, src);

    memset(b,       0, 0x188); fill_column(b+top_skip,       b+0x188, width, height);
    memset(b+0x188, 0, 0x188); fill_column(b+top_skip+0x188, b+0x310, width, height);
    merge_buffers(b);
    rle_sprite(pixbuf, b + 0x188);
}
