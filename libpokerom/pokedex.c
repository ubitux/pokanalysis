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

#define PKMN_IDS_TAB(id)	info->stream[ROM_ADDR(0x10, 0x5024 + id)]

static u8 get_pkmn_id_from_stupid_one(u8 id) /* Red: RO10:5010 */
{
	info_t *info = get_info();
	return PKMN_IDS_TAB(id - 1);
}

static u8 get_rom_id_from_pkmn_id(u8 pkmn_id) /* Red: RO10:4FFB */
{
	info_t *info = get_info();
	u8 pos;

	for (pos = 0; PKMN_IDS_TAB(pos) != pkmn_id; pos++);
	return pos;
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
		pkmn_header_addr = ROM_ADDR(pkmn_header_bank_id, 0x43de + (real_pkmn_id - 1) * 0x1C);
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

	uncompress_sprite(b + 0x188, ROM_ADDR(get_bank_from_stupid_pkmn_id(stupid_pkmn_id), sprite_addr), info->stream);
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

	switch (pkmn_id) {
		case 193: case 196: case 199: case 202: case 210: case 213: case 216: case 219: case 253:
			return Py_BuildValue("z", NULL);
	}
	load_pokemon_sprite(pixbuf, pkmn_id);
	return Py_BuildValue("s#", pixbuf, sizeof(pixbuf));
}


static PyObject *get_pkmn_name(int rom_id)
{
	info_t *info = get_info();
	int i = 0, rom_addr = ROM_ADDR(0x07, 0x421E + 0x0A * (rom_id - 1));
	char *s;
	char name[10];

	while (*(s = get_pkmn_char(info->stream[rom_addr++], "")) && i < 10) {
		strcpy(&name[i], s);
		i += strlen(s);
	}
	name[i] = 0;
	return Py_BuildValue("s", name);
}

PyObject* get_pokedex(PyObject* self, PyObject* args)
{
	(void)self;
	(void)args;
	//info_t *info = get_info();
	int real_pkmn_id;
	PyObject *list = PyList_New(0);

	for (real_pkmn_id = 1; real_pkmn_id <= 151; real_pkmn_id++) {
		PyObject *pkmn = PyDict_New();
		u8 rom_id = get_rom_id_from_pkmn_id(real_pkmn_id) + 1;

		PyDict_SetItemString(pkmn, "pic", get_pixbuf(rom_id));
		PyDict_SetItemString(pkmn, "name", get_pkmn_name(rom_id));
		PyDict_SetItemString(pkmn, "rom_id", Py_BuildValue("i", rom_id));
		PyList_Append(list, pkmn);
	}
	return list;
}
