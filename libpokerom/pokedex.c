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

struct pkmn_header_raw {
	u8 id;				// 0x00
	u8 hp, atk, def, spd, spe;	// 0x01-0x05
	u8 type1, type2;		// 0x06-0x07
	u8 capture_rate;		// 0x08
	u8 base_exp_yield;		// 0x09
	u8 sprite_front_dim;		// 0x0A
	u16 sprite_front_addr;		// 0x0B
	u16 sprite_back_addr;		// 0x0C
	u8 initial_atk[4];		// 0x0F-0x12
	u8 growth_rate;			// 0x12
	u8 TMHM_flags[7];		// 0x14-0x1A
	u8 unknown;			// 0x1B
} PACKED;

static PyObject *get_pkmn_growth_rate(struct pkmn_header_raw *h)
{
	static char *growth_str[] = {[0] = "Medium Fast", [3] = "Medium Slow", [4] = "Fast", [5] = "Slow"};
	return Py_BuildValue("s", growth_str[h->growth_rate]);
}

static PyObject *get_pkmn_stats(struct pkmn_header_raw *h)
{
	PyObject *dict = PyDict_New();

	PyDict_SetItemString(dict, "HP", Py_BuildValue("i", h->hp));
	PyDict_SetItemString(dict, "ATK", Py_BuildValue("i", h->atk));
	PyDict_SetItemString(dict, "DEF", Py_BuildValue("i", h->def));
	PyDict_SetItemString(dict, "SPD", Py_BuildValue("i", h->spd));
	PyDict_SetItemString(dict, "SPE", Py_BuildValue("i", h->spe));
	PyDict_SetItemString(dict, "CAP", Py_BuildValue("i", h->capture_rate));
	PyDict_SetItemString(dict, "EXP", Py_BuildValue("i", h->base_exp_yield));
	return dict;
}

static void load_packed_text_string(u8 *data, char *dst, u8 id, size_t max_len)
{
	while (--id) {
		while (*data != 0x50)
			data++;
		data++;
	}
	load_string(dst, data, max_len, 0);
}

#define PACKED_TEXT_BASE_ADDR(b, i)	&gl_stream[ROM_ADDR((b), GET_ADDR(0x375d + ((i) - 1) * 2))]

static void get_pkmn_item_name(char *iname, u8 item_id, size_t max_len)
{
	load_packed_text_string(PACKED_TEXT_BASE_ADDR(0x01, 4), iname, item_id, max_len);
}

static void get_pkmn_move_name(char *mname, u8 move_id, size_t max_len)
{
	load_packed_text_string(PACKED_TEXT_BASE_ADDR(0x2c, 2), mname, move_id, max_len);
}

static PyObject *get_pkmn_HM_TM(struct pkmn_header_raw *h)
{
	PyObject *list = PyList_New(0);
	u8 *flags = h->TMHM_flags;
	u8 mask = 1 << 7;
	int id;

	for (id = 1; id < 7 * 8; id++) {
		if (*flags & mask) {
			char name[30];
			char tmhm[5];

			if (id <= 50) {
				snprintf(tmhm, 5, "TM%02d", id);
			} else {
				snprintf(tmhm, 5, "HM%02d", id - 50);
			}
			get_pkmn_move_name(name, gl_stream[ROM_ADDR(0x04, 0x7773 + id - 1)], sizeof(name));
			PyList_Append(list, Py_BuildValue("ss", tmhm, name));
		}
		if (mask == 1) {
			mask = 1 << 7;
			flags++;
			continue;
		}
		mask >>= 1;
	}
	return list;
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

static PyObject *get_pkmn_types(struct pkmn_header_raw *h)
{
	PyObject *list = PyList_New(0);

	PyList_Append(list, get_pkmn_type(h->type1));
	PyList_Append(list, (h->type1 == h->type2) ? Py_BuildValue("z", NULL) : get_pkmn_type(h->type2));
	return list;
}

static PyObject *get_pkmn_attacks(struct pkmn_header_raw *h, u8 rom_pkmn_id)
{
	PyObject *list = PyList_New(0);
	int n = 0;
	char mname[20];
	u8 *ptr;

	/* Native attacks */
	for (ptr = h->initial_atk; ptr[n] && n < 4; n++) {
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

static void load_pokemon_sprite(struct pkmn_header_raw *h, u8 *pixbuf, u8 stupid_pkmn_id, int back)
{
	u8 b[3 * 0x188];
	u8 sprite_dim;
	u8 ff8b, ff8c, ff8d;
	u16 sprite_addr;

	if (back) {
		sprite_addr = h->sprite_back_addr;
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
			sprite_addr = h->sprite_front_addr;
			sprite_dim = h->sprite_front_dim;
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

static PyObject *get_pixbuf(struct pkmn_header_raw *h, u8 pkmn_id)
{
	u8 pixbuf[(7 * 7) * (8 * 8) * 3];
	u8 pixbuf_back[(7 * 7) * (8 * 8) * 3];

	switch (pkmn_id) {
	case 193: case 196: case 199: case 202: case 210: case 213: case 216: case 219: case 253:
		return Py_BuildValue("z", NULL);
	}
	load_pokemon_sprite(h, pixbuf, pkmn_id, 0);
	load_pokemon_sprite(h, pixbuf_back, pkmn_id, 1);
	return Py_BuildValue("s#s#", pixbuf, sizeof(pixbuf), pixbuf_back, sizeof(pixbuf));
}

static PyObject *get_pkmn_name(int rom_id)
{
	int rom_addr = ROM_ADDR(0x07, 0x421E + 0x0A * (rom_id - 1));
	char name[20];

	load_string(name, &gl_stream[rom_addr], sizeof(name), 10);
	return Py_BuildValue("s", name);
}

static void trad_num(u8 *b)
{
	for (; *b; b++)
		if (*b != ' ')
			*b = *b - 0xF6 + '0';
}

static void set_pkmn_texts(PyObject *dict, int rom_id)
{
	int i;
	char *s;
	char b[128];
	u8 *data = &gl_stream[ROM_ADDR(0x10, GET_ADDR(ROM_ADDR(0x10, 0x447E + 2 * (rom_id - 1))))];

	i = 0;
	while (*data != 0x50) {
		s = get_pkmn_char(*data++, "¿?");
		strcpy(&b[i], s);
		i += strlen(s);
	}
	PyDict_SetItemString(dict, "class", Py_BuildValue("s", b));
	data++; // skip 0x50

	sprintf(b, "%d'%02d\"", data[0], data[1]);
	data += 2;
	PyDict_SetItemString(dict, "height", Py_BuildValue("s", b));

	{
		u8 input[2];
		char *dest;
		char *b_trim;

		strcpy(b, "   ???lb");
		input[1] = *data++;
		input[0] = *data++;

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
		data++;
	}

	i = 0;
	data = &gl_stream[ROM_ADDR(data[2], *(u16*)data)] + 1;
	while (*data != 0x50) {
		s = get_pkmn_char(*data++, "¿?");
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
		struct pkmn_header_raw *pkmn_header = (void *)&gl_stream[header_addr];

		PyDict_SetItemString(pkmn, "pic", get_pixbuf(pkmn_header, rom_id));
		PyDict_SetItemString(pkmn, "name", get_pkmn_name(rom_id));

		set_pkmn_texts(pkmn, rom_id);

		PyDict_SetItemString(pkmn, "stats", get_pkmn_stats(pkmn_header));
		PyDict_SetItemString(pkmn, "attacks", get_pkmn_attacks(pkmn_header, rom_id));
		PyDict_SetItemString(pkmn, "evolutions", get_pkmn_evolutions(rom_id));
		PyDict_SetItemString(pkmn, "types", get_pkmn_types(pkmn_header));
		PyDict_SetItemString(pkmn, "HM_TM", get_pkmn_HM_TM(pkmn_header));
		PyDict_SetItemString(pkmn, "growth_rate", get_pkmn_growth_rate(pkmn_header));
		PyDict_SetItemString(pkmn, "rom_header_addr", Py_BuildValue("i", header_addr));
		PyDict_SetItemString(pkmn, "id", Py_BuildValue("i", real_pkmn_id));
		PyDict_SetItemString(pkmn, "rom_id", Py_BuildValue("i", rom_id));

		PyList_Append(list, pkmn);
	}
	return list;
}
