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

#define PKMN_IDS_TAB(id)	gl_stream[ROM_ADDR(0x10, 0x5024 + id)]

static u8 get_pkmn_id_from_stupid_one(u8 id) /* Red: RO10:5010 */
{
	return PKMN_IDS_TAB(id - 1);
}

static u8 get_rom_id_from_pkmn_id(u8 pkmn_id) /* Red: RO10:4FFB */
{
	u8 pos;

	for (pos = 0; PKMN_IDS_TAB(pos) != pkmn_id; pos++);
	return pos;
}

static PyObject *get_pkmn_header(u8 *pkmn_header)
{
	PyObject *dict = PyDict_New();

	PyDict_SetItemString(dict, "0x00_pokemon_id", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x01_base_HP", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x02_base_ATK", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x03_base_DEF", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x04_base_SPD", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x05_base_SPE", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x06_type1", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x07_type2", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x08_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x09_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x0a_sprite_front_dim", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x0b_sprite_front_addr", Py_BuildValue("i", *(pkmn_header + 1) << 8 | *pkmn_header)); pkmn_header += 2;
	PyDict_SetItemString(dict, "0x0d_sprite_back_addr", Py_BuildValue("i", *(pkmn_header + 1) << 8 | *pkmn_header)); pkmn_header += 2;
	PyDict_SetItemString(dict, "0x0f_initial_attack_1", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x10_initial_attack_2", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x11_initial_attack_3", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x12_initial_attack_4", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x13_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x14_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x15_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x16_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x17_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x18_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x19_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x1a_unknown", Py_BuildValue("i", *pkmn_header++));
	PyDict_SetItemString(dict, "0x1b_unknown", Py_BuildValue("i", *pkmn_header++));
	return dict;
}

static void get_pkmn_item_name(char *iname, u8 item_id, size_t max_len)
{
	int rom_addr = ROM_ADDR(0x1, GET_ADDR(0x375d + (4 - 1) * 2));
	u8 *data = &gl_stream[rom_addr];

	while (--item_id) {
		while (*data != 0x50)
			data++;
		data++;
	}
	load_string(iname, data, max_len, 0);
}

static void get_pkmn_move_name(char *mname, u8 move_id, size_t max_len)
{
	int rom_addr = ROM_ADDR(0x2C, GET_ADDR(0x375d + (2 - 1) * 2));
	u8 *data = &gl_stream[rom_addr];

	while (--move_id) {
		while (*data != 0x50)
			data++;
		data++;
	}
	load_string(mname, data, max_len, 0);
}

#define PKMN_EVENTS_ADDR(i)	ROM_ADDR(0x0E, GET_ADDR(ROM_ADDR(0x0E, 0x705c + (i - 1) * 2)))

static PyObject *get_pkmn_evolutions(u8 rom_pkmn_id)
{
	u8 *ptr = &gl_stream[PKMN_EVENTS_ADDR(rom_pkmn_id)];
	PyObject *list = PyList_New(0);

	while (*ptr) {
		PyObject *evol = PyDict_New();

		switch (*ptr++) {
		case 1:
			PyDict_SetItemString(evol, "type", Py_BuildValue("s", "level"));
			PyDict_SetItemString(evol, "level", Py_BuildValue("i", *ptr++));
			PyDict_SetItemString(evol, "pkmn-id", Py_BuildValue("i", *ptr++));
			break;
		case 2:
		{
			char iname[30];

			PyDict_SetItemString(evol, "type", Py_BuildValue("s", "stone"));
			get_pkmn_item_name(iname, *ptr++, sizeof(iname));
			PyDict_SetItemString(evol, "stone", Py_BuildValue("s", iname));
			PyDict_SetItemString(evol, "level", Py_BuildValue("i", *ptr++));
			PyDict_SetItemString(evol, "pkmn-id", Py_BuildValue("i", *ptr++));
			break;
		}
		case 3:
			PyDict_SetItemString(evol, "type", Py_BuildValue("s", "exchange"));
			PyDict_SetItemString(evol, "level", Py_BuildValue("i", *ptr++));
			PyDict_SetItemString(evol, "pkmn-id", Py_BuildValue("i", *ptr++));
			break;
		}
		PyList_Append(list, evol);
	}
	return list;
}

static PyObject *get_pkmn_type(u8 type_id)
{
	int rom_addr = ROM_ADDR(0x09, GET_ADDR(ROM_ADDR(0x09, 0x7DAE + type_id * 2)));
	char tname[20];

	load_string(tname, &gl_stream[rom_addr], sizeof(tname), 0);
	return Py_BuildValue("s", tname);
}

static PyObject *get_pkmn_types(u8 *type_ids)
{
	PyObject *list = PyList_New(0);

	PyList_Append(list, get_pkmn_type(type_ids[0]));
	PyList_Append(list, (type_ids[1] == type_ids[0]) ? Py_BuildValue("z", NULL) : get_pkmn_type(type_ids[1]));
	return list;
}

static PyObject *get_pkmn_attacks(u8 *ptr, u8 rom_pkmn_id)
{
	PyObject *list = PyList_New(0);
	int n = 0;
	char mname[20];

	/* Native attacks */
	for (ptr += 0x0f; ptr[n] && n < 4; n++) {
		get_pkmn_move_name(mname, ptr[n], sizeof(mname));
		PyList_Append(list, Py_BuildValue("is", 0, mname));
	}

	/* Levels attacks */
	ptr = &gl_stream[PKMN_EVENTS_ADDR(rom_pkmn_id)];
	while (*ptr != 0x00)
		ptr++;
	ptr++;
	while (*ptr) {
		get_pkmn_move_name(mname, ptr[1], sizeof(mname));
		PyList_Append(list, Py_BuildValue("is", ptr[0], mname));
		ptr += 2;
	}

	return list;
}

static int get_pkmn_header_address(u8 rom_pkmn_id)
{
	if (rom_pkmn_id == 0x15)	// Mew
		return ROM_ADDR(0x01, 0x425B);
	return ROM_ADDR(0x0E, 0x43de + (get_pkmn_id_from_stupid_one(rom_pkmn_id) - 1) * 0x1C);
}

static void load_pokemon_sprite(u8 *pixbuf, u8 stupid_pkmn_id, int back)
{
	int pkmn_header_addr;
	u8 b[3 * 0x188];
	u8 sprite_dim;
	u8 ff8b, ff8c, ff8d;
	u16 sprite_addr;

	pkmn_header_addr = get_pkmn_header_address(stupid_pkmn_id);

	if (back) {
		sprite_addr = GET_ADDR(pkmn_header_addr + 0x0D);
		sprite_dim = 0x44;
	} else {
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
			sprite_dim = gl_stream[pkmn_header_addr + 0x0A];
		}
	}

	ff8b = low_nibble(sprite_dim);
	ff8d = 7 * ((8 - ff8b) >> 1);
	ff8c = 8 * high_nibble(sprite_dim);
	ff8d = 8 * (ff8d + 7 - high_nibble(sprite_dim));

	uncompress_sprite(b + 0x188, ROM_ADDR(get_bank_from_stupid_pkmn_id(stupid_pkmn_id), sprite_addr));
	//printf("pkmn_id=%02X sprite_addr=%04X dim=%02X ff8b=%02X ff8c=%02X ff8d=%02X\n", real_pkmn_id, sprite_addr, sprite_dim, ff8b, ff8c, ff8d);

	memset(b, 0, 0x188);
	fill_data(b, b + 0x188, ff8b, ff8c, ff8d);
	memset(b + 0x188, 0, 0x188);
	fill_data(b + 0x188, b + 0x310, ff8b, ff8c, ff8d);
	merge_buffers(b);
	rle_sprite(pixbuf, b + 0x188);
}

PyObject *get_pixbuf(u8 pkmn_id)
{
	u8 pixbuf[(7 * 7) * (8 * 8) * 3];
	u8 pixbuf_back[(7 * 7) * (8 * 8) * 3];

	switch (pkmn_id) {
	case 193: case 196: case 199: case 202: case 210: case 213: case 216: case 219: case 253:
		return Py_BuildValue("z", NULL);
	}
	load_pokemon_sprite(pixbuf, pkmn_id, 0);
	load_pokemon_sprite(pixbuf_back, pkmn_id, 1);
	return Py_BuildValue("s#s#", pixbuf, sizeof(pixbuf), pixbuf_back, sizeof(pixbuf));
}

static PyObject *get_pkmn_name(int rom_id)
{
	int rom_addr = ROM_ADDR(0x07, 0x421E + 0x0A * (rom_id - 1));
	char name[20];

	load_string(name, &gl_stream[rom_addr], sizeof(name), 10);
	return Py_BuildValue("s", name);
}

void trad_num(u8 *b)
{
	for (; *b; b++)
		if (*b != ' ')
			*b = *b - 0xF6 + '0';
}

static void set_pkmn_texts(PyObject *dict, int rom_id)
{
	int i, rom_addr;
	char *s;
	char b[128];

	i = 0;
	rom_addr = ROM_ADDR(0x10, GET_ADDR(ROM_ADDR(0x10, 0x447E + 2 * (rom_id - 1))));
	while (gl_stream[rom_addr] != 0x50) {
		s = get_pkmn_char(gl_stream[rom_addr++], "¿?");
		strcpy(&b[i], s);
		i += strlen(s);
	}
	PyDict_SetItemString(dict, "class", Py_BuildValue("s", b));
	rom_addr++; // skip 0x50

	sprintf(b, "%d'%02d\"", gl_stream[rom_addr], gl_stream[rom_addr + 1]);
	rom_addr += 2;
	PyDict_SetItemString(dict, "height", Py_BuildValue("s", b));

	{
		u8 input[2];
		char *dest;
		char *b_trim;

		strcpy(b, "   ???lb");
		input[1] = gl_stream[rom_addr++];
		input[0] = gl_stream[rom_addr++];

		pkmn_put_nbr((u8*)b, input, 0x02, 0x05);
		trad_num((u8*)b);

		dest = b + 3;
		if (input[0] < (input[1] < 10))
			*dest = '0';
		dest++;
		*(dest + 1) = *dest;
		*dest = '.';

		strcpy(b + 6, "lb");
		for (b_trim = b; *b_trim == ' '; b_trim++);

		PyDict_SetItemString(dict, "weight", Py_BuildValue("s", b_trim));
		rom_addr++;
	}

	i = 0;
	rom_addr = ROM_ADDR(gl_stream[rom_addr + 2], GET_ADDR(rom_addr)) + 1;
	while (gl_stream[rom_addr] != 0x50) {
		s = get_pkmn_char(gl_stream[rom_addr++], "¿?");
		strcpy(&b[i], s);
		i += strlen(s);
	}
	b[i] = 0;
	PyDict_SetItemString(dict, "desc", Py_BuildValue("s", b));
}

PyObject *get_pokedex(PyObject *self)
{
	int real_pkmn_id;
	PyObject *list = PyList_New(0);

	for (real_pkmn_id = 1; real_pkmn_id <= 151; real_pkmn_id++) {
		PyObject *pkmn = PyDict_New();
		u8 rom_id = get_rom_id_from_pkmn_id(real_pkmn_id) + 1;
		int header_addr = get_pkmn_header_address(rom_id);

		PyDict_SetItemString(pkmn, "pic", get_pixbuf(rom_id));
		PyDict_SetItemString(pkmn, "name", get_pkmn_name(rom_id));

		set_pkmn_texts(pkmn, rom_id);

		PyDict_SetItemString(pkmn, "attacks", get_pkmn_attacks(&gl_stream[header_addr], rom_id));
		PyDict_SetItemString(pkmn, "evolutions", get_pkmn_evolutions(rom_id));
		PyDict_SetItemString(pkmn, "types", get_pkmn_types(&gl_stream[header_addr + 0x06]));
		PyDict_SetItemString(pkmn, "rom_header_addr", Py_BuildValue("i", header_addr));
		PyDict_SetItemString(pkmn, "header_values", get_pkmn_header(&gl_stream[header_addr]));
		PyDict_SetItemString(pkmn, "id", Py_BuildValue("i", real_pkmn_id));
		PyDict_SetItemString(pkmn, "rom_id", Py_BuildValue("i", rom_id));

		PyList_Append(list, pkmn);
	}
	return list;
}
