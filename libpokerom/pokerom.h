/*
 *  This file is part of Pokanalysis.
 *
 *  Pokanalysis
 *  Copyright © 2010 Clément Bœsch
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

# define GET_ADDR(offset) (info->stream[(offset) + 1] << 8 | info->stream[(offset)])

typedef uint8_t u8;
typedef uint16_t u16;

#define high_nibble(c)		((c) >> 4)
#define low_nibble(c)		((c) & 0x0f)
#define swap_u8(c)		(((c) << 4) | ((c) >> 4))

typedef struct {
	int fd;
	struct stat rom_stat;
	unsigned char* stream;
} info_t;

PyObject* get_maps(PyObject*, PyObject*);
PyObject* get_pokedex(PyObject*, PyObject*);
PyObject* grab_tile(PyObject*, PyObject*);
PyObject* read_addr(PyObject*, PyObject*);
char* get_pkmn_char(unsigned char, char*);
info_t* get_info();
PyObject* get_map_pic(int r_map_pointer, unsigned char map_w, unsigned char map_h, int blockdata_addr, int tiles_addr, PyObject*);
void apply_filter(unsigned char *pixbuf, int map_id, int w);
PyObject *get_special_items(int map_id);
void uncompress_sprite(unsigned char *dest, int addr, unsigned char *rom_data);
void rle_sprite(unsigned char *dst, unsigned char *src);

#endif
