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

static const u8 code_map[] = {
     0,  1,  3,  2,  7,  6,  4,  5, 15, 14, 12, 13,  8,  9, 11, 10,
    15, 14, 12, 13,  8,  9, 11, 10,  0,  1,  3,  2,  7,  6,  4,  5,
     0,  8, 12,  4, 14,  6,  2, 10, 15,  7,  3, 11,  1,  9, 13,  5,
    15,  7,  3, 11,  1,  9, 13,  5,  0,  8, 12,  4, 14,  6,  2, 10,
};

static u8 get_code(int i, u8 prev, int flip)
{
    if (flip) return code_map[2*16 + (prev>>3 & 1) * 16 + i];
    else      return code_map[       (prev    & 1) * 16 + i];
}

static void load_data(u8 *dst, int flip, int sprite_h, int sprite_w)
{
    for (int y = 0; y != sprite_h; y++) {
        u8 *d = dst;
        u8 a, b = 0;
        for (int x = 0; x != sprite_w; x += 8) {
            a = get_code(high_nibble(d[y]), b, flip);
            b = get_code(low_nibble(d[y]),  a, flip);
            d[y] = a<<4 | b;
            d += sprite_h;
        }
    }
}

static const u8 col_flip_reorder[] = {
    0, 8, 4,12,
    2,10, 6,14,
    1, 9, 5,13,
    3,11, 7,15,
};

static void xor_buf(u8 *dst, int flip, int i, int j,
                    int sprite_w, int sprite_h)
{
    for (int x = 0; x != sprite_w; x += 8) {
        for (int y = 0; y != sprite_h; y++) {
            if (flip)
                dst[j] = col_flip_reorder[high_nibble(dst[j])]<<4 |
                         col_flip_reorder[low_nibble (dst[j])];
            dst[j++] ^= dst[i++];
        }
    }
}

enum {
    OP_UNCHANGED   = 0,
    OP_SHIFTLEFT_2 = 1,
    OP_BSWAP       = 2,
    OP_ROTATE_2    = 3,
};

static void reset_p1_p2(int b, int *p1, int *p2)
{
    if (b & 1) *p1 =     0, *p2 = 7*7*8;
    else       *p1 = 7*7*8, *p2 =     0;
}

enum {Z_NOT_YET_READY, Z_END, Z_RESET};

struct decode_ctx {
    u8 *dst;
    int tile_x, tile_y;
    int op;
    int b_flag;
    int p1, p2;
    int sprite_w, sprite_h;
    int packing;
    int flip;
};

static int unpack(struct decode_ctx *c)
{
    u8 *b = c->dst;
    int sw = c->sprite_w, sh = c->sprite_h, flip = c->flip;

    /* start processing only when having a full column… */
    if (c->tile_y+1 != sh) {
        c->tile_y++;
        c->p1++;
        return Z_NOT_YET_READY;
    }
    c->tile_y = 0;

    /* …and the op is OP_UNCHANGED… */
    if (c->op != OP_UNCHANGED) {
        c->op--;
        c->p1 = c->p2;
        return Z_NOT_YET_READY;
    }

    /* …and having all the lines */
    c->op = OP_ROTATE_2;
    c->tile_x += 8;
    if (c->tile_x != sw) {
        c->p1 = c->p2 = c->p1 + 1;
        return Z_NOT_YET_READY;
    }

    /* get everything but need to reset (XXX: why?) */
    c->tile_x = 0;
    if (!(c->b_flag & 2))
        return Z_RESET;

    /* process data */
    switch (c->packing) {
    case 2:
        reset_p1_p2(c->b_flag, &c->p1, &c->p2);
        load_data(b + c->p2, 0, sw, sh);
    case 1:
        reset_p1_p2(c->b_flag, &c->p1, &c->p2);
        load_data(b + c->p1, flip, sw, sh);
        xor_buf(b, flip, c->p1, c->p2, sw, sh);
        break;
    case 0:
        load_data(c->dst,         flip, sw, sh);
        load_data(c->dst + 7*7*8, flip, sw, sh);
        break;
    }
    return Z_END;
}

static int unpack_n_pkt(struct getbits *gb, struct decode_ctx *c)
{
    // count number of consecutive 1-bit
    int nb_ones;
    u16 ones = 1;
    for (nb_ones = 0; get_next_bit(gb); nb_ones++)
        ones = ones<<1 | 1;

    // read nb_ones bits (nb_ones = number of 1-bit previously counted)
    u16 data;
    for (data = 0x0000; nb_ones >= 0; nb_ones--)
        data = data<<1 | get_next_bit(gb);

    int r, n = ones + data;
    do {
        r = unpack(c);
        n--;
    } while (n && r == Z_NOT_YET_READY);
    return r;
}

static u8 do_op(u8 a, int x)
{
    switch (x) {
    case OP_SHIFTLEFT_2: return a << 2;
    case OP_BSWAP:       return swap_u8(a);
    case OP_ROTATE_2:    return (a&3)<<6 | a>>2;
    default:             return a;
    }
}

static u8 uncompress_sprite(u8 *dst, const u8 *src, int flip)
{
    u8 dim;
    int r = -1;
    struct getbits gb = {.stream=src, .bit=1};
    struct decode_ctx c = {
        .dst  = dst,
        .op   = OP_ROTATE_2,
        .flip = flip
    };

    memset(dst, 0, 0x310);

    /* Header */
    dim = *gb.stream++;
    c.sprite_w  = high_nibble(dim) * 8;
    c.sprite_h  = low_nibble(dim)  * 8;
    c.b_flag    = get_next_bit(&gb);

    /* Decompression */
    do {
        if (r == Z_RESET && !(c.b_flag & 2))
            c.b_flag = 2 | (c.b_flag^1);

        /* packing:
         *    0 X → 0
         *    1 0 → 1
         *    1 1 → 2
         */
        if (c.b_flag & 2)
            c.packing = get_next_bit(&gb) ? get_next_bit(&gb)+1 : 0;

        c.p1 = c.p2 = (c.b_flag & 1) * 7*7*8;

        if (!get_next_bit(&gb)) {
            r = unpack_n_pkt(&gb, &c);
            if (r == Z_RESET) continue;
            if (r == Z_END)   break;
        }

        do {
            u8 b = get_next_bit(&gb)<<1 | get_next_bit(&gb);
            if (b) {
                c.dst[c.p1] |= do_op(b, c.op);
                r = unpack(&c);
            } else {
                r = unpack_n_pkt(&gb, &c);
            }
        } while (r == Z_NOT_YET_READY);
    } while (r == Z_RESET);
    return dim;
}

/*
 * Get a pixbuf in BPP bytes per pixel of 7x7 tiles.
 *
 * Tiles are stored in column: first 7 tiles are the first column, second 7
 * tiles are the second column, …
 * If flip flag is set, the columns are inversed (first 7 tiles are the last
 * column).
 * */
static void runlength_dec_sprite(u8 *dst, const u8 *src, int flip)
{
    int i, j, pixbuf_offset;

    pixbuf_offset = flip ? 6 * 8*BPP : 0; // start on first or last column
    for (j = 0; j < 7; j++) {
        for (i = 0; i < 7; i++) {
            u8 tile_pixbuf[8*8 * BPP];
            int tile_offset = 0;

            // load next tile
            load_tile(tile_pixbuf, src, 0);
            src += 0x10;

            // copy tile lines
            for (int y = 0; y < 8; y++) {
                memcpy(dst+pixbuf_offset, tile_pixbuf+tile_offset, 8*BPP);
                tile_offset   +=     8*BPP;
                pixbuf_offset += 7 * 8*BPP; // skip sprite width
            }
        }
        // rollback to the top of the column and select previous/next column
        pixbuf_offset = pixbuf_offset - 7*7*8*BPP*8 + BPP*8 * (flip ? -1 : 1);
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
static void interlace_merge(u8 *buffer, int flip)
{
    u8 *dest = buffer + 0x188*3 - 1;
    u8 *src1 = buffer + 0x188*2 - 1;
    u8 *src2 = buffer + 0x188   - 1;

    for (int n = 0; n < 0x188; n++) {
        *dest-- = *src1--;
        *dest-- = *src2--;
    }

    if (flip) {
        u8 *p = buffer + 0x188;
        for (int i = 0; i < 0x188*2; i++)
            p[i] = swap_u8(p[i]);
    }
}

static void place_pic(u8 *dst, int w, int h)
{
    u8 *src = dst + 0x188;

    memset(dst, 0, 0x188);
    dst += 8 * (7 * ((8 - w) >> 1) + 7 - h); // align
    for (int x = 0; x < w; x++, dst += 7*8)
        for (int y = 0; y < h*8; y++)
            dst[y] = *src++;
}

/* 1 sprite = 7x7 tiles (max) */
void load_sprite(u8 *pixbuf, const u8 *src, int flip)
{
    u8 b[3 * 0x188];
    u8 sprite_dim = uncompress_sprite(b+0x188, src, flip);

    int width  = low_nibble(sprite_dim);
    int height = high_nibble(sprite_dim);

    place_pic(b,       width, height);
    place_pic(b+0x188, width, height);
    interlace_merge(b, flip);
    runlength_dec_sprite(pixbuf, b + 0x188, flip);
}
