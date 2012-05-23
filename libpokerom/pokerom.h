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

#ifndef POKEROM_H
# define POKEMON_H

# include <Python.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <fcntl.h>

# define BPP 3 /* Byte per pixel for output pixbuf */

enum {
    DEFAULT_COLORS_OFFSET,
    WARPS_COLORS_OFFSET,
    SIGNS_COLORS_OFFSET,
    ENTITIES_COLORS_OFFSET,
};

# define GET_ADDR(offset)       (*(u16*)&stream[offset])
# define ROM_ADDR(bank, addr)   (((addr) > 0x3fff) ? (bank) * 0x4000 + (addr) - 0x4000 : (addr))
# define REL_ADDR(addr)         (((addr) > 0x3fff) ? (addr) % 0x4000 + 0x4000 : (addr))

typedef uint8_t u8;
typedef uint16_t u16;

# define PACKED __attribute__((__packed__))

struct rom {
    PyObject_HEAD;
    char *fname;
    int fd;
    struct stat st;
    u8 *stream;
    PyObject *maps;
    PyObject *trainers;
};

PyObject *disasm(struct rom *, PyObject *);
PyObject *preload_maps(struct rom *);
PyObject *get_maps(struct rom *);
PyObject *get_pokedex(struct rom *);
PyObject *get_special_items(u8 *stream, int map_id);
PyObject *get_trainers(struct rom *);
PyObject *str_getascii(struct rom *, PyObject *);
PyObject *str_getbin(struct rom *, PyObject *);

char *get_pkmn_char(u8, char *);
void add_trainer(struct rom *rom, int map_id, int x, int y, int extra1, int extra2);
void apply_filter(u8 *stream, u8 *pixbuf, int map_id, int w, int h);

void get_pkmn_name(     u8 *stream, char *pname, u8 pkmn_id,    size_t max_len);
void get_item_name(     u8 *stream, char *iname, u8 item_id,    size_t max_len);
void get_pkmn_move_name(u8 *stream, char *mname, u8 move_id,    size_t max_len);
void get_trainer_name(  u8 *stream, char *tname, u8 trainer_id, size_t max_len);

void load_tile     (u8 *dst, const u8 *src, int color_key);
void load_flip_tile(u8 *dst, const u8 *src, int color_key);
void merge_tiles   (u8 *dst, const u8 *src, int color_key);

void load_sprite(u8 *pixbuf, const u8 *src, int flip);
void load_string(char *dest, u8 *src, size_t max_len, int fixed_str_len);
void pkmn_put_nbr(u8 *dest, u8 *src, u8 input_flag, u8 precision);

#endif
