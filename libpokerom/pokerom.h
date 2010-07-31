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

# define GET_ADDR(offset)	(info->stream[(offset) + 1] << 8 | info->stream[(offset)])
# define ROM_ADDR(bank, addr)	((bank) * 0x4000 + (addr) % 0x4000)
# define ROM_ADDR2(bank, addr)	(((addr) > 0x3fff) ? (bank) * 0x4000 + (addr) % 0x4000 : (addr))
# define REL_ADDR(addr)		(((addr) > 0x3fff) ? (addr) % 0x4000 + 0x4000 : (addr))

typedef uint8_t u8;
typedef uint16_t u16;

# define high_nibble(c)		((c) >> 4)
# define low_nibble(c)		((c) & 0x0f)
# define swap_u8(c)		(((c) << 4) | ((c) >> 4))

typedef struct {
	int fd;
	struct stat rom_stat;
	u8* stream;
} info_t;

PyObject *disasm(PyObject *self, PyObject *args);
PyObject *get_map_pic(int r_map_pointer, u8 map_w, u8 map_h, int blockdata_addr, int tiles_addr, PyObject *);
PyObject *get_maps(PyObject *, PyObject *);
PyObject *get_pokedex(PyObject *, PyObject *);
PyObject *get_special_items(int map_id);
PyObject *grab_tile(PyObject *, PyObject *);
PyObject *read_addr(PyObject *, PyObject *);
char *get_pkmn_char(u8, char *);
info_t *get_info();
void apply_filter(u8 *pixbuf, int map_id, int w);
void rle_sprite(u8 *dst, u8 *src);
void uncompress_sprite(u8 *dest, int addr, u8 *rom_data);
void pkmn_put_nbr(u8 *dest, u8 *src, u8 input_flag, u8 precision);

#endif
