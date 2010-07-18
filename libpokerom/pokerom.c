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

#include "pokerom.h"

static struct {
	unsigned char c;
	char *s;
} g_alphabet[] = {
	{0x4F, "\n"},
	{0x51, "\n\n"},
	{0x54, "POKé"},
	{0x55, "\n"},	// stop with arrow indicator
	{0x57, ""},	// ending stop without arrow
	{0x7F, " "}, {0x80, "A"}, {0x81, "B"}, {0x82, "C"}, {0x83, "D"}, {0x84, "E"},
	{0x85, "F"}, {0x86, "G"}, {0x87, "H"}, {0x88, "I"}, {0x89, "J"}, {0x8A, "K"},
	{0x8B, "L"}, {0x8C, "M"}, {0x8D, "N"}, {0x8E, "O"}, {0x8F, "P"}, {0x90, "Q"},
	{0x91, "R"}, {0x92, "S"}, {0x93, "T"}, {0x94, "U"}, {0x95, "V"}, {0x96, "W"},
	{0x97, "X"}, {0x98, "Y"}, {0x99, "Z"}, {0x9A, "("}, {0x9B, ")"}, {0x9C, ":"},
	{0x9D, ";"}, {0x9E, "]"}, {0x9F, "["}, {0xA0, "a"}, {0xA1, "b"}, {0xA2, "c"},
	{0xA3, "d"}, {0xA4, "e"}, {0xA5, "f"}, {0xA6, "g"}, {0xA7, "h"}, {0xA8, "i"},
	{0xA9, "j"}, {0xAA, "k"}, {0xAB, "l"}, {0xAC, "m"}, {0xAD, "n"}, {0xAE, "o"},
	{0xAF, "p"}, {0xB0, "q"}, {0xB1, "r"}, {0xB2, "s"}, {0xB3, "t"}, {0xB4, "u"},
	{0xB5, "v"}, {0xB6, "w"}, {0xB7, "x"}, {0xB8, "y"}, {0xB9, "z"}, {0xBA, "é"},
	{0xBB, "'d"}, {0xBC, "'l"}, {0xBD, "'s"}, {0xBE, "'t"}, {0xBF, "'v"},
	{0xE0, "'"}, {0xE1, "PK"}, {0xE2, "MN"}, {0xE3, "-"}, {0xE4, "'r"},
	{0xE5, "'m"}, {0xE6, "?"}, {0xE7, "!"}, {0xE8, "."}, {0xEF, "♂"},
	{0xF0, "$"}, {0xF1, "x"}, {0xF2, "."}, {0xF3, "/"}, {0xF4, ","}, {0xF5, "♀"},
	{0xF6, "0"}, {0xF7, "1"}, {0xF8, "2"}, {0xF9, "3"}, {0xFA, "4"}, {0xFB, "5"},
	{0xFC, "6"}, {0xFD, "7"}, {0xFE, "8"}, {0xFF, "9"}
};

info_t *get_info()
{
	static info_t i;
	return &i;
}

char *get_pkmn_char(unsigned char c, char *def_ret)
{
	size_t i;

	for (i = 0; i < sizeof(g_alphabet) / sizeof(*g_alphabet); i++)
		if (g_alphabet[i].c == c)
			return g_alphabet[i].s;
	return def_ret;
}

PyObject *read_addr(PyObject *self, PyObject *args)
{
	(void)self;
	int offset = 0;
	int addr, rom_addr;
	int bank_id;
	info_t *info = get_info();

	PyArg_ParseTuple(args, "i", &offset);
	bank_id = offset / 0x4000;
	addr = GET_ADDR(offset);
	rom_addr = bank_id * 0x4000 + addr % 0x4000;
	return Py_BuildValue("ii", addr, rom_addr);
}

static PyObject *read_data(PyObject *self, PyObject *args)
{
	(void)self;
	unsigned char *s;
	int offset;
	info_t *info = get_info();
	PyObject *list = PyList_New(0);

	PyArg_ParseTuple(args, "is", &offset, &s);
	for (; *s; s++) {
		if (*s == 'B') { /* 8-bit */
			PyList_Append(list, Py_BuildValue("i", info->stream[offset]));
			offset++;
		} else if (*s == 'W') { /* 16-bit */
			PyList_Append(list, read_addr(NULL, Py_BuildValue("(i)", offset, NULL)));
			offset += 2;
		}
	}
	return list;
}

static PyObject *load_rom(PyObject *self, PyObject *args)
{
	(void)self;
	char *fname;
	info_t *info = get_info();

	PyArg_ParseTuple(args, "s", &fname);
	if ((info->fd = open(fname, O_RDONLY)) < 0
			|| fstat(info->fd, &info->rom_stat)
			|| !(info->stream = mmap(0, info->rom_stat.st_size, PROT_READ, MAP_PRIVATE, info->fd, 0))) {
		return PyErr_SetFromErrnoWithFilename(PyExc_IOError, fname);
	}
	return Py_BuildValue("z", NULL);
}

static void merge_buffers(unsigned char *buffer)
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

static void fill_data(u8 *dst, u8 *src, u8 ff8b, u8 ff8c, u8 ff8d)
{
	int a, c;

	dst += ff8d;
	for (a = 0; a < ff8b; a++, dst += 0x38)
		for (c = 0; c < ff8c; c++)
			dst[c] = *src++;
}

static int get_bank_from_stupid_pkmn_id(u8 pkmn_id) /* Red: RO01:1637 */
{
	if (pkmn_id == 0x15)	// Mew
		return 0x01;
	if (pkmn_id == 0xb6)
		return 0x0b;
	if (pkmn_id < 0x1f)
		return 0x09;
	if (pkmn_id < 0x4a)
		return 0x0a;
	if (pkmn_id < 0x74)
		return 0x0b;
	if (pkmn_id < 0x99)
		return 0x0c;
	return 0x0d;
}

static u8 get_pkmn_id_from_stupid_one(u8 id) /* Red: RO10:5010 */
{
	info_t *info = get_info();
	return info->stream[0x10 * 0x4000 + (0x5024 + id - 1) % 0x4000];
}

static void load_pokemon_sprite(u8 *pixbuf, u8 stupid_pkmn_id)
{
	info_t *info = get_info();
	int pkmn_header_addr, pkmn_header_bank_id;
	u8 b[3 * 0x188];
	u8 real_pkmn_id, sprite_dim;
	u8 ff8b, ff8c, ff8d;
	u16 sprite_addr;

	real_pkmn_id = get_pkmn_id_from_stupid_one(stupid_pkmn_id);

	if (stupid_pkmn_id == 0x15) {	// Mew
		pkmn_header_bank_id = 0x3A;
		pkmn_header_addr = 0x425B;
	} else {
		pkmn_header_bank_id = 0x0E;
		pkmn_header_addr = pkmn_header_bank_id * 0x4000 + (0x43de + (real_pkmn_id - 1) * 0x1C) % 0x4000;
	}

	switch (stupid_pkmn_id) {
		case 0xb6:
			sprite_addr = 0x79E8;
			sprite_dim = 0x66;
			break;

		case 0xb7:
			sprite_addr = 0x6536;
			sprite_dim = 0x77;
			break;

		case 0xb8:
			sprite_addr = 0x66b5;
			sprite_dim = 0x66;
			break;

		default:
			sprite_addr = GET_ADDR(pkmn_header_addr + 0x0B);
			sprite_dim = info->stream[pkmn_header_addr + 0x0A];
	}

	ff8b = low_nibble(sprite_dim);
	ff8d = 7 * ((8 - ff8b) >> 1);
	ff8c = 8 * high_nibble(sprite_dim);
	ff8d = 8 * (ff8d + 7 - high_nibble(sprite_dim));

	uncompress_sprite(b + 0x188, get_bank_from_stupid_pkmn_id(stupid_pkmn_id) * 0x4000 + sprite_addr % 0x4000, info->stream);
	//printf("pkmn_id=%02X sprite_addr=%04X dim=%02X ff8b=%02X ff8c=%02X ff8d=%02X\n", real_pkmn_id, sprite_addr, sprite_dim, ff8b, ff8c, ff8d);

	memset(b, 0, 0x188);
	fill_data(b, b + 0x188, ff8b, ff8c, ff8d);
	memset(b + 0x188, 0, 0x188);
	fill_data(b + 0x188, b + 0x310, ff8b, ff8c, ff8d);
	merge_buffers(b);
	rle_sprite(pixbuf, b + 0x188);
}

static PyObject *get_pokemons_info(PyObject *self, PyObject *args)
{
	(void)self;
	(void)args;
	int i;
	PyObject *list = PyList_New(0);

	for (i = 0; i <= 0xFF; i++) {
		u8 pixbuf[(7 * 7) * (8 * 8) * 3];

		switch (i) {
			case 193: case 196: case 199: case 202: case 210: case 213: case 216: case 219: case 253:
				PyList_Append(list, Py_BuildValue("z", NULL));
				continue;
		}
		load_pokemon_sprite(pixbuf, i);
		PyList_Append(list, Py_BuildValue("s#", pixbuf, sizeof(pixbuf)));
	}
	return list;
}

static PyObject *str_getbin(PyObject *self, PyObject *args)
{
	(void)self;
	int i, j = 0;
	char *s;
	char b[64];

	for (PyArg_ParseTuple(args, "s", &s); *s; s++)
		for (i = 0; i < (int)(sizeof(g_alphabet) / sizeof(*g_alphabet)) && j < (int)sizeof(b); i++)
			if (*g_alphabet[i].s && strncmp(s, g_alphabet[i].s, strlen(g_alphabet[i].s)) == 0)
				b[j++] = g_alphabet[i].c;
	b[j] = 0;
	return Py_BuildValue("s", b);
}

static PyObject *str_getascii(PyObject *self, PyObject *args)
{
	(void)self;
	int i, j = 0;
	char *s;
	char b[64];

	for (PyArg_ParseTuple(args, "s", &s); *s; s++) {
		for (i = 0; i < (int)(sizeof(g_alphabet) / sizeof(*g_alphabet)) && j < (int)sizeof(b); i++) {
			if ((unsigned char)*s == g_alphabet[i].c) {
				strcpy(&b[j], g_alphabet[i].s);
				j += strlen(g_alphabet[i].s);
			}
		}
	}
	b[j] = 0;
	return Py_BuildValue("s", b);
}

PyMODINIT_FUNC initpokerom(void)
{
	static PyMethodDef m[] = {
		{"load_rom", load_rom, METH_VARARGS, "Load ROM"},
		{"read_addr", read_addr, METH_VARARGS, "Get 24-bits ROM address from 16-bit address read at the given offset"},
		{"read_data", read_data, METH_VARARGS, "Return list of 8-bit char and 16-bit address (same return as read_addr)"},

		{"get_maps", get_maps, METH_VARARGS, "Game maps"},
		{"get_pokemons_info", get_pokemons_info, METH_VARARGS, "Get all pokémons"},

		/* Utils */
		{"str_getbin", str_getbin, METH_VARARGS, "Convert binary text to ascii"},
		{"str_getascii", str_getascii, METH_VARARGS, "Convert ascii text to binary"},
		{NULL, NULL, 0, NULL}
	};
	Py_InitModule("pokerom", m);
}
