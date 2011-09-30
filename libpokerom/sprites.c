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
static u8 p_flag;           // d0a7
static u8 buffer_flag;      // d0a8
static u8 misc_flag;        // d0a9
static u8 input_flag;       // d0aa
static u16 p1;              // d0ad-d0ae
static u16 p2;              // d0af-d0b0
static u16 input_p1;        // d0b1-d0b2
static u16 input_p2;        // d0b3-d0b4

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

static int update_input_ptr(u8 *stream, u8 nibble, u16 *de) // 276D
{
    int r, in, i;

    in  = input_flag ? *de & 1<<3 : *de & 1;
    i   = (in == 0 ? input_p1 : input_p2) + (nibble>>1);
    r   = nibble&1 ? low_nibble(stream[i]) : high_nibble(stream[i]);
    *de = (*de & 0xff00) | r;
    return r;
}

static void load_data(u8 *dst, u8 *stream, u16 p) // 26D4
{
    u16 de;

    p1 = p2 = p;

    if (input_flag) input_p1 = 0x27b7, de = input_p2 = 0x27bf;
    else            input_p1 = 0x27a7, de = input_p2 = 0x27af;

    de = de & 0xff00;

    // 2704
    for (int y = 0; y != sprite_height; y++) {
        for (int x = 0; x != sprite_width; x += 8) {
            u8 z = dst[p1];

            u8 nibble = swap_u8(update_input_ptr(stream, high_nibble(z), &de));
            de = nibble<<8 | (de & 0x00ff);

            nibble = update_input_ptr(stream, low_nibble(z), &de);
            nibble |= de>>8;

            // 2729
            dst[p1] = nibble;
            p1 += sprite_height;
        }

        // 2747
        de = de & 0xff00; // e = 0

        p1 = p2 = p2+1;
    }
}

enum {Z_RET, Z_END, Z_START};

static int uncompress_data(u8 *dst, u8 *stream) // 27C7
{
    reset_p1_p2(buffer_flag, &p1, &p2);
    load_data(dst, stream, p1);
    reset_p1_p2(buffer_flag, &p1, &p2);

    int i = p1, j = p2;
    for (int x = 0; x != sprite_width; x += 8) {
        for (int y = 0; y != sprite_height; y++) {
            if (input_flag) {
                u8 v, *col_interlaced_paths = stream + 0x2867;
                v = dst[j];
                dst[j] = col_interlaced_paths[high_nibble(v)]<<4 |
                         col_interlaced_paths[low_nibble(v)];
            }
            dst[j++] ^= dst[i++];
        }
    }
    return Z_END;
}

struct tile { int x, y; };

static int f25d8(u8 *dst, u8 *stream, struct tile *tile)
{
    if (tile->y+1 != sprite_height) {
        tile->y++;
        p1++;
        return Z_RET;
    }

    // 25F6
    tile->y = 0;
    if (p_flag) {
        p_flag--;
        p1 = p2;
        return Z_RET;
    }

    // 2610
    p_flag = 3;
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

    // 2646 (-> 26BF)
    if (misc_flag == 2) {
        // 2877
        u8 input_flag_backup = input_flag;

        reset_p1_p2(buffer_flag, &p1, &p2);
        input_flag = 0;
        load_data(dst, stream, p2);
        reset_p1_p2(buffer_flag, &p1, &p2);
        input_flag = input_flag_backup;
        return uncompress_data(dst, stream);
    }

    // 26C7
    if (misc_flag)
        return uncompress_data(dst, stream);

    // 26CB
    load_data(dst, stream,     0);
    load_data(dst, stream, 7*7*8);
    return Z_END;
}

static int f2595(u8 *dst, u8 *stream, struct tile *tile, struct getbits *gb)
{
    int bitlen, p, idx;

    for (bitlen = 0; get_next_bit(gb); bitlen++);

    p = GET_ADDR(0x269f + 2*bitlen);
    for (idx = 0; bitlen >= 0; bitlen--)
        idx = idx<<1 | get_next_bit(gb);
    p += idx;

    do {
        dst[p1] |= next_a(0, p_flag);
        int r = f25d8(dst, stream, tile);
        if (r != Z_RET)
            return r;
        p--;
    } while (p);

    return Z_RET;
}

static void uncompress_sprite(u8 *stream, u8 *dst, int addr) // 251A
{
    u8 byte, b;
    int r;
    struct getbits gb = {.stream=stream+addr, .bit=1};
    struct tile tile  = {.x = 0, .y = 0};

    memset(dst, 0, 0x310);
    p_flag = 3;

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
            r = f2595(dst, stream, &tile, &gb);
            if (r == Z_START) continue;
            if (r == Z_END)   return;
        }

        do {
            b = get_next_bit(&gb)<<1 | get_next_bit(&gb);
            if (b) {
                dst[p1] |= next_a(b, p_flag);
                r = f25d8(dst, stream, &tile);
            } else {
                r = f2595(dst, stream, &tile, &gb);
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

void load_sprite(u8 *stream, u8 *pixbuf, u8 sprite_dim, int addr)
{
    u8 b[3 * 0x188];

    int width    =     low_nibble(sprite_dim);
    int height   = 8 * high_nibble(sprite_dim);
    int top_skip = 8 * (7 * ((8 - width) >> 1) + 7 - high_nibble(sprite_dim));

    uncompress_sprite(stream, b + 0x188, addr);

    memset(b,       0, 0x188); fill_column(b+top_skip,       b+0x188, width, height);
    memset(b+0x188, 0, 0x188); fill_column(b+top_skip+0x188, b+0x310, width, height);
    merge_buffers(b);
    rle_sprite(pixbuf, b + 0x188);
}
