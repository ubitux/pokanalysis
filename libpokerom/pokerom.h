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

#ifndef POKEROM_H
# define POKEMON_H

# include <Python.h>
# include <sys/stat.h>
# include <sys/mman.h>
# include <fcntl.h>

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
void apply_filter(u8 *stream, u8 *pixbuf, int map_id, int w);
void get_pkmn_item_name(u8 *stream, char *iname, u8 item_id, size_t max_len);
void get_pkmn_move_name(u8 *stream, char *mname, u8 move_id, size_t max_len);
void get_trainer_name(u8 *stream, char *tname, u8 trainer_id, size_t max_len);
void load_sprite(u8 *pixbuf, const u8 *src);
void load_string(char *dest, u8 *src, size_t max_len, int fixed_str_len);
void pkmn_put_nbr(u8 *dest, u8 *src, u8 input_flag, u8 precision);
void rle_sprite(u8 *dst, u8 *src);

#endif
