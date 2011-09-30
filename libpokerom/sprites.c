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

enum {
    OP_UNCHANGED   = 0,
    OP_SHIFTLEFT_2 = 1,
    OP_BSWAP       = 2,
    OP_ROTATE_2    = 3,
};

static u8 do_op(u8 a, int x) // 2649
{
    switch (x) {
    case OP_SHIFTLEFT_2: return a << 2;
    case OP_BSWAP:       return swap_u8(a);
    case OP_ROTATE_2:    return (a&3)<<6 | a>>2;
    default: return a;
    }
}

static void reset_p1_p2(int b, int *p1, int *p2)
{
    if (b & 1) *p1 =     0, *p2 = 7*7*8;
    else       *p1 = 7*7*8, *p2 =     0;
}

static const u8 tileid_map[] = {
     0,  1,  3,  2,  7,  6,  4,  5, 15, 14, 12, 13,  8,  9, 11, 10,
    15, 14, 12, 13,  8,  9, 11, 10,  0,  1,  3,  2,  7,  6,  4,  5,
     0,  8, 12,  4, 14,  6,  2, 10, 15,  7,  3, 11,  1,  9, 13,  5,
    15,  7,  3, 11,  1,  9, 13,  5,  0,  8, 12,  4, 14,  6,  2, 10,
};

static u8 get_tile_id(int i, u8 prev, int flip)
{
    if (flip) return tileid_map[2*16 + (prev>>3 & 1) * 16 + i];
    else      return tileid_map[       (prev    & 1) * 16 + i];
}

static void load_data(u8 *dst, int flip, int sprite_h, int sprite_w)
{
    for (int y = 0; y != sprite_h; y++) {
        u8 *d = dst;
        u8 a, b = 0;
        for (int x = 0; x != sprite_w; x += 8) {
            a = get_tile_id(high_nibble(d[y]), b, flip);
            b = get_tile_id(low_nibble(d[y]),  a, flip);
            d[y] = a<<4 | b;
            d += sprite_h;
        }
    }
}

enum {Z_RET, Z_END, Z_START};

static const u8 col_interlaced_paths[] = {0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15};

static int uncompress_data(u8 *dst, int flip, int b_flag, int *p1, int *p2,
                           int sprite_w, int sprite_h)
{
    reset_p1_p2(b_flag, p1, p2);
    load_data(dst + *p1, flip, sprite_w, sprite_h);

    int i = *p1, j = *p2;
    for (int x = 0; x != sprite_w; x += 8) {
        for (int y = 0; y != sprite_h; y++) {
            if (flip) {
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

static int f25d8(u8 *dst, struct tile *tile, int *op, int b_flag,
                 int *p1, int *p2, int sprite_w, int sprite_h, int misc_flag)
{
    if (tile->y+1 != sprite_h) {
        tile->y++;
        (*p1)++;
        return Z_RET;
    }

    // 25F6
    tile->y = 0;
    if (*op) {
        (*op)--;
        *p1 = *p2;
        return Z_RET;
    }

    // 2610
    *op = OP_ROTATE_2;
    tile->x += 8;
    if (tile->x != sprite_w) {
        *p1 = *p2 = *p1 + 1;
        return Z_RET;
    }

    /* Break context here: calling function must end after processing */

    tile->x = 0;
    if (!(b_flag & 2))
        return Z_START;

    int flip = 0; // XXX: 0xd0aa; does not seem to work with ≠ 0

    // 2646 (-> 26BF)
    if (misc_flag == 2) {
        reset_p1_p2(b_flag, p1, p2);
        load_data(dst + *p2, 0, sprite_w, sprite_h);
        return uncompress_data(dst, flip, b_flag, p1, p2,
                               sprite_w, sprite_h);
    }

    // 26C7
    if (misc_flag)
        return uncompress_data(dst, flip, b_flag, p1, p2,
                               sprite_w, sprite_h);

    // 26CB
    load_data(dst        , flip, sprite_w, sprite_h);
    load_data(dst + 7*7*8, flip, sprite_w, sprite_h);
    return Z_END;
}

static int f2595(u8 *dst, struct tile *tile, struct getbits *gb, int *op,
                 int b_flag, int *p1, int *p2, int sprite_w, int sprite_h,
                 int misc_flag)
{
    int bitlen, p = 1, idx;

    for (bitlen = 0; get_next_bit(gb); bitlen++)
        p = p<<1 | 1;

    for (idx = 0; bitlen >= 0; bitlen--)
        idx = idx<<1 | get_next_bit(gb);
    p += idx;

    do {
        dst[*p1] |= do_op(0, *op);
        int r = f25d8(dst, tile, op, b_flag, p1, p2,
                      sprite_w, sprite_h, misc_flag);
        if (r != Z_RET)
            return r;
        p--;
    } while (p);

    return Z_RET;
}

static u8 uncompress_sprite(u8 *dst, const u8 *src) // 251A
{
    u8 dim;
    int r = -1, misc_flag = 0, op = OP_ROTATE_2;
    struct getbits gb = {.stream=src, .bit=1};
    struct tile tile  = {.x = 0, .y = 0};

    memset(dst, 0, 0x310);

    /* Header */
    dim = *gb.stream++;
    int sprite_w    = high_nibble(dim) * 8;
    int sprite_h    = low_nibble(dim)  * 8;
    int buffer_flag = get_next_bit(&gb);

    /* Decompression */
    do {
        if (r == Z_START && !(buffer_flag & 2))
            buffer_flag = 2 | (buffer_flag^1);

        if (buffer_flag & 2)
            misc_flag = get_next_bit(&gb) ? get_next_bit(&gb)+1 : 0;

        int p1 = (buffer_flag & 1) * 7*7*8;
        int p2 = p1;

        // 257A
        if (!get_next_bit(&gb)) {
            r = f2595(dst, &tile, &gb, &op, buffer_flag, &p1, &p2,
                      sprite_w, sprite_h, misc_flag);
            if (r == Z_START) continue;
            if (r == Z_END)   break;
        }

        do {
            u8 b = get_next_bit(&gb)<<1 | get_next_bit(&gb);
            if (b) {
                dst[p1] |= do_op(b, op);
                r = f25d8(dst, &tile, &op, buffer_flag, &p1, &p2,
                          sprite_w, sprite_h, misc_flag);
            } else {
                r = f2595(dst, &tile, &gb, &op, buffer_flag, &p1, &p2,
                          sprite_w, sprite_h, misc_flag);
            }
        } while (r == Z_RET);
    } while (r == Z_START);
    return dim;
}

static void runlength_dec_sprite(u8 *dst, const u8 *src)
{
    int i, j, pixbuf_offset = 0;

    for (j = 0; j < 7; j++) {
        for (i = 0; i < 7; i++) {
            u8 tile_pixbuf[8*8 * BPP];
            int tile_offset = 0;

            load_tile(tile_pixbuf, src, 0);
            src += 0x10;
            for (int y = 0; y < 8; y++) {
                memcpy(dst+pixbuf_offset, tile_pixbuf+tile_offset, 8*BPP);
                tile_offset   +=     8*BPP;
                pixbuf_offset += 7 * 8*BPP;
            }
        }
        pixbuf_offset = pixbuf_offset - 7*7*8*BPP*8 + BPP*8;
    }
}


/*          src2       src1       dest
 *  n=0   AAAAAAAAAA BBBBBBBBBB ..........
 *  n=1   AAAAAAAAAA BBBBBBBBB# .........B
 *  n=2   AAAAAAAAA# BBBBBBBBBB ........AB
 *  n=3   AAAAAAAAAA BBBBBBBB#B .......BAB
 *  n=4   AAAAAAAA#A BBBBBBBBBB ......ABAB
 *                [...]
 *  ...   AAAAAAAAAA ABABABABAB ABABABABAB
 *                   =====================
 */
static void interlace_merge(u8 *buffer)
{
    u8 *dest = buffer + 0x188*3 - 1;
    u8 *src1 = buffer + 0x188*2 - 1;
    u8 *src2 = buffer + 0x188   - 1;

    for (int n = 0; n < 0x188; n++) {
        *dest-- = *src1--;
        *dest-- = *src2--;
    }
}

static void place_pic(u8 *dst, int w, int h, int start_skip)
{
    u8 *src = dst + 0x188;

    memset(dst, 0, 0x188);
    dst += start_skip;
    for (int x = 0; x < w; x++, dst += 7*8)
        for (int y = 0; y < h; y++)
            dst[y] = *src++;
}

/* 1 sprite = 7x7 tiles (max) */
void load_sprite(u8 *pixbuf, const u8 *src)
{
    u8 b[3 * 0x188];
    u8 sprite_dim = uncompress_sprite(b+0x188, src);

    int width      =     low_nibble(sprite_dim);
    int height     = 8 * high_nibble(sprite_dim);
    int start_skip = 8 * (7 * ((8 - width) >> 1) + 7 - high_nibble(sprite_dim));

    place_pic(b,       width, height, start_skip);
    place_pic(b+0x188, width, height, start_skip);
    interlace_merge(b);
    runlength_dec_sprite(pixbuf, b + 0x188);
}
