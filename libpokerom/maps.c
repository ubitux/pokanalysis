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
	char *key;
	char *colors[4];
} color_set[] = {
	{"default",  {"\xE8\xE8\xE8", "\x58\x58\x58", "\xA0\xA0\xA0", "\x10\x10\x10"}},
	{"warps",    {"\xE8\xC0\xC0", "\xC0\x58\x58", "\xC0\xA0\xA0", "\xC0\x10\x10"}},
	{"signs",    {"\xC0\xC0\xE8", "\x58\x58\xC0", "\xA0\xA0\xC0", "\x10\x10\xC0"}},
	{"entities", {"\xE8\xC0\xE8", "\xC0\x58\xC0", "\xC0\xA0\xC0", "\xC0\x10\xC0"}},
};

#define ENTITY_OFFSET 3

static char **get_color_set(char *color_key)
{
	size_t n;

	for (n = 0; n < sizeof(color_set) / sizeof(*color_set); n++)
		if (strcmp(color_key, color_set[n].key) == 0)
			return color_set[n].colors;
	return color_set[0].colors;
}

#define TILE_X 8
#define TILE_Y 8

/* 1 tile = 8x8 px (2 bytes -> 8 pixels) */
static void load_tile(u8 *pixbuf, int addr, char *color_key)
{
	info_t *info = get_info();
	int x, y, pixbuf_offset = 0;
	char **colors = get_color_set(color_key);

	for (y = 0; y < TILE_Y; y++) {
		u8 bit1 = info->stream[addr++];
		u8 bit2 = info->stream[addr++];
		u8 mask = 1 << 7;

		for (x = 0; x < TILE_X; x++, mask >>= 1) {
			memcpy(&pixbuf[pixbuf_offset], colors[(!!(bit1 & mask) << 1) | !!(bit2 & mask)], 3);
			pixbuf_offset += 3;
		}
	}
}

static void load_tile_from_ptr(u8 *pixbuf, u8 *src, char *color_key)
{
	int x, y, pixbuf_offset = 0;
	char **colors = get_color_set(color_key);

	for (y = 0; y < TILE_Y; y++) {
		u8 bit1 = *src++;
		u8 bit2 = *src++;
		u8 mask = 1 << 7;

		for (x = 0; x < TILE_X; x++, mask >>= 1) {
			memcpy(&pixbuf[pixbuf_offset], colors[(!!(bit1 & mask) << 1) | !!(bit2 & mask)], 3);
			pixbuf_offset += 3;
		}
	}
}

#define PIXBUF_TILE_SIZE	(TILE_Y * TILE_X * 3)
#define PIXBUF_TILE_LINE_SIZE	(TILE_X * 3)

/* 1 block = 4x4 tiles */
/* 1 sprite = 7x7 tiles */

#define SPRITE_X 7
#define SPRITE_Y 7

void rle_sprite(u8 *dst, u8 *src)
{
	int i, j, pixbuf_offset = 0;

	for (j = 0; j < SPRITE_Y; j++) {
		for (i = 0; i < SPRITE_X; i++) {
			u8 tile_pixbuf[PIXBUF_TILE_SIZE];
			int y, tile_offset = 0;

			load_tile_from_ptr(tile_pixbuf, src, "default");
			src += 0x10;
			for (y = 0; y < TILE_Y; y++) {
				memcpy(&dst[pixbuf_offset], &tile_pixbuf[tile_offset], PIXBUF_TILE_LINE_SIZE);
				tile_offset += PIXBUF_TILE_LINE_SIZE;
				pixbuf_offset += SPRITE_X * PIXBUF_TILE_LINE_SIZE;
			}
		}
		pixbuf_offset = pixbuf_offset - (SPRITE_Y * (SPRITE_X * PIXBUF_TILE_LINE_SIZE) * TILE_Y) + PIXBUF_TILE_LINE_SIZE;
	}
}

#define BOX_X 2
#define BOX_Y 2

typedef struct {
	char *color_key;
	int entity_addr;
	int flip;
} box_info_type;

static int get_entity_addr(PyObject *item, int decal_id)
{
	info_t *info = get_info();

	if (!item)
		return 0;

	long pic_id = PyLong_AsLong(PyDict_GetItemString(item, "pic_id"));
	int entity_info_addr = ROM_ADDR(0x05, 0x7b27 + 4 * (pic_id - 1));
	int bank_id = info->stream[entity_info_addr + 3];
	return ROM_ADDR(bank_id, GET_ADDR(entity_info_addr) + decal_id);
}

static box_info_type get_box_info(PyObject *od, int x, int y)
{
	unsigned int n;
	box_info_type bi;

	bi.color_key = "default";
	bi.entity_addr = 0;
	bi.flip = 0;
	for (n = 1; n < sizeof(color_set) / sizeof(*color_set); n++) {
		PyObject *list = PyDict_GetItemString(od, color_set[n].key);
		Py_ssize_t i, len = PyList_Size(list);

		for (i = 0; i < len; i++) {
			PyObject *item = PyList_GetItem(list, i);

			if (PyLong_AsLong(PyDict_GetItemString(item, "x")) == x && PyLong_AsLong(PyDict_GetItemString(item, "y")) == y) {
				bi.color_key = color_set[n].key;
				if (strcmp(bi.color_key, "entities") == 0) {
					u8 orientation = PyLong_AsLong(PyDict_GetItemString(item, "mvt_2")) & 0xF;
					int decal_id = 0;

					if (orientation == 1) {		// North
						decal_id = 1 * 64;
					} else if (orientation == 2) {	// West
						decal_id = 2 * 64;
					} else if (orientation == 3) {	// East
						decal_id = 2 * 64;
						bi.flip = 1;
					}
					bi.entity_addr = get_entity_addr(item, decal_id);
				}
				break;
			}
		}
	}
	return bi;
}

#define PIXBUF_BOX_SIZE		(PIXBUF_TILE_SIZE * 2 * 2)
#define PIXBUF_BOX_LINE_SIZE	(PIXBUF_TILE_LINE_SIZE * 2)

#define BLOCK_X 4
#define BLOCK_Y 4

static void merge_tiles(u8 *dst, u8 *src, char *alpha)
{
	int i;

	for (i = 0; i < PIXBUF_TILE_SIZE; i += 3) {
		if (memcmp(&src[i], alpha, 3) != 0) {
			memcpy(&dst[i], &src[i], 3);
		}
	}
}

static void flip_tile(u8 *tile)
{
	u8 old_tile[PIXBUF_TILE_SIZE];
	int x, y;

	memcpy(old_tile, tile, sizeof(old_tile));
	for (y = 0; y < TILE_Y; y++) {
		for (x = 0; x < TILE_X; x++) {
			memcpy(&tile[3 * (y * TILE_X + x)], &old_tile[3 * (y * TILE_X + (TILE_X - x - 1))], 3);
		}
	}
}

/* 1 block = 4x4 tiles */
static void load_block_from_tiles_addr(u8 *pixbuf, int *tiles_addr, PyObject *od, int bx, int by)
{
	int i, j, pixbuf_offset = 0;

	for (j = 0; j < BLOCK_Y; j++) {
		for (i = 0; i < BLOCK_X; i++) {
			u8 tile_pixbuf[PIXBUF_TILE_SIZE];
			int y, tile_offset = 0;
			box_info_type bi = get_box_info(od, bx * 2 + i / 2, by * 2 + j / 2);

			if (bi.entity_addr) {
				int n = (j % 2) * 2 + (i + bi.flip) % 2;
				u8 entity_pixbuf[PIXBUF_TILE_SIZE];

				load_tile(entity_pixbuf, bi.entity_addr + n * 16, bi.color_key);
				if (bi.flip)
					flip_tile(entity_pixbuf);
				load_tile(tile_pixbuf, tiles_addr[j * BLOCK_X + i], "default");
				merge_tiles(tile_pixbuf, entity_pixbuf, color_set[ENTITY_OFFSET].colors[0]);
			} else {
				load_tile(tile_pixbuf, tiles_addr[j * BLOCK_X + i], bi.color_key);
			}

			for (y = 0; y < TILE_Y; y++) {
				memcpy(&pixbuf[pixbuf_offset], &tile_pixbuf[tile_offset], PIXBUF_TILE_LINE_SIZE);
				tile_offset += PIXBUF_TILE_LINE_SIZE;
				pixbuf_offset += BLOCK_X * PIXBUF_TILE_LINE_SIZE;
			}
			pixbuf_offset = pixbuf_offset - TILE_Y * (BLOCK_X * PIXBUF_TILE_LINE_SIZE) + PIXBUF_TILE_LINE_SIZE;
		}
		pixbuf_offset += (TILE_Y - 1) * (BLOCK_X * PIXBUF_TILE_LINE_SIZE);
	}
}

#define PIXBUF_BLOCK_SIZE	(PIXBUF_BOX_SIZE * 2 * 2)
#define PIXBUF_BLOCK_LINE_SIZE	(PIXBUF_BOX_LINE_SIZE * 2)

#define TILES_PER_ROW 4
#define TILES_PER_COL 4

/* Map blocks */
typedef struct {
	int b[4 * 4];
} blocks_type;

/* 4x4 tiles (1 tile = 8x8px) */
static blocks_type *get_blocks(int n, int addr, int tiles_addr)
{
	blocks_type *blocks;
	int i, j;
	info_t *info = get_info();

	if (addr > info->rom_stat.st_size) {
		return NULL;
	}
	if ((blocks = calloc(n, sizeof(*blocks))) == NULL)
		return NULL;
	for (i = 0; i < n; i++) {
		for (j = 0; j < 16; j++) {
			blocks[i].b[j] = (info->stream[addr++] << 4) + tiles_addr;
		}
	}
	return blocks;
}

#define PIXEL_LINES_PER_BLOCK 32

static u8 *get_map_pic_raw(int r_map_pointer, u8 map_w, u8 map_h, int blockdata_addr, int tiles_addr, PyObject *od)
{
	u8 *pixbuf = malloc(map_w * map_h * PIXBUF_BLOCK_SIZE);
	info_t *info = get_info();
	int i, j, pixbuf_offset = 0;

	blocks_type *blocks = get_blocks(256, blockdata_addr, tiles_addr);
	if (!blocks)
		return NULL;

	for (j = 0; j < map_h; j++) {
		for (i = 0; i < map_w; i++) {
			u8 block_pixbuf[PIXBUF_BLOCK_SIZE];
			int y, block_offset = 0;
			int *block = blocks[info->stream[r_map_pointer + j * map_w + i]].b;

			if (!block)
				return NULL;

			load_block_from_tiles_addr(block_pixbuf, block, od, i, j);
			for (y = 0; y < PIXEL_LINES_PER_BLOCK; y++) {
				memcpy(&pixbuf[pixbuf_offset], &block_pixbuf[block_offset], PIXBUF_BLOCK_LINE_SIZE);
				block_offset += PIXBUF_BLOCK_LINE_SIZE;
				pixbuf_offset += map_w * PIXBUF_BLOCK_LINE_SIZE;
			}
			pixbuf_offset = pixbuf_offset - PIXEL_LINES_PER_BLOCK * (map_w * PIXBUF_BLOCK_LINE_SIZE) + PIXBUF_BLOCK_LINE_SIZE;
		}
		pixbuf_offset += (PIXEL_LINES_PER_BLOCK - 1) * (map_w * PIXBUF_BLOCK_LINE_SIZE);
	}
	return pixbuf;
}

#define TILESET_HEADERS ROM_ADDR(3, 0x47BE)

static int get_blockdata_addr(u8 tileset_id)
{
	info_t *info = get_info();
	int header_offset = TILESET_HEADERS + tileset_id * 12;
	int bank_id = info->stream[header_offset];

	return ROM_ADDR(bank_id, GET_ADDR(header_offset + 1));
}

static int get_tiles_addr(u8 tileset_id)
{
	info_t *info = get_info();
	int header_offset = TILESET_HEADERS + tileset_id * 12;
	int bank_id = info->stream[header_offset];

	return ROM_ADDR(bank_id, GET_ADDR(header_offset + 2 + 1));
}

#define DICT_ADD_BYTE(dict, key) PyDict_SetItemString(dict, key, Py_BuildValue("i", info->stream[addr++]))
#define DICT_ADD_WORD(dict, key) PyDict_SetItemString(dict, key, read_addr(NULL, Py_BuildValue("(i)", addr, NULL))); addr += 2

static struct {
	u8 k;
	char c;
} cons[] = {
	{1 << 3, 'N'},
	{1 << 2, 'S'},
	{1 << 1, 'W'},
	{1 << 0, 'E'}
};

# define NB_MAPS 248

static int loaded_maps[NB_MAPS] = {0};

static int is_loaded(int addr)
{
	int i;

	for (i = 0; loaded_maps[i]; i++)
		if (loaded_maps[i] == addr)
			return 1;
	return 0;
}

static void add_loaded_map(int addr)
{
	static int n = 0;
	loaded_maps[n++] = addr;
}

typedef struct {
	u8 index;
	u16 connected_map;
	u16 current_map;
	u8 bigness;
	u8 map_width;
	u8 y_align;
	u8 x_align;
	u16 window;
} __attribute__((__packed__)) connection_type;

static int get_map_addr(int i)
{
	info_t *info = get_info();
	return info->stream[ROM_ADDR(3, 0x423D) + i] * 0x4000 + GET_ADDR(0x01AE + i * 2) % 0x4000;
}

typedef struct {
	int x;
	int y;
} coords_type;

typedef struct submap_s {
	coords_type coords;
	u8 *pixbuf;
	int map_w;
	int map_h;
	struct submap_s *next;
	PyObject *info;
	int id;
	PyObject *objects;
} submap_type;

typedef struct {
	int w;
	int h;
	u8 *pixbuf;
	int id;
	PyObject *info_list;
	PyObject *objects;
} map_type;

static PyObject *get_wild_pokemons(int id)
{
	PyObject *list = PyList_New(0);
	info_t *info = get_info();
	int addr = ROM_ADDR(0x03, GET_ADDR(ROM_ADDR(0x03, 0x4EEB + id * 2))) + 1;

	while (info->stream[addr]) {
		PyList_Append(list, Py_BuildValue("ii", info->stream[addr], info->stream[addr + 1]));
		addr += 2;
	}
	return list;
}

static submap_type *get_submap(int id, int addr, int x_init, int y_init)
{
	info_t *info = get_info();
	PyObject *dict = PyDict_New();
	PyObject *list, *warp_list, *sign_list, *entity_list;
	int blockdata_addr, tiles_addr, rom_addr;
	u8 connect_byte, i;
	u8 map_h, map_w;
	int connection_addr;
	submap_type *current_map, *last, *tmp, *to_add;
	int text_pointers;

	if (is_loaded(addr)) {
		//printf("0x%06x -> previously loaded\n", addr);
		return NULL;
	}

	add_loaded_map(addr);

	//printf("0x%06x\n", addr);
	if (addr > info->rom_stat.st_size) {
		return NULL;
	}

	current_map = malloc(sizeof(*current_map));

	current_map->info = dict;
	current_map->next = NULL;
	current_map->id = id;
	current_map->objects = PyDict_New();

	PyDict_SetItemString(dict, "id", Py_BuildValue("i", id));
	PyDict_SetItemString(dict, "rom-addr", Py_BuildValue("i", addr));
	PyDict_SetItemString(dict, "wild-pkmn", get_wild_pokemons(id));
	PyDict_SetItemString(dict, "special-items", get_special_items(id));

	/* Map Header */
	{
		int word_addr;

		blockdata_addr = get_blockdata_addr(info->stream[addr]);
		tiles_addr = get_tiles_addr(info->stream[addr]);

		DICT_ADD_BYTE(dict, "tileset");
		map_h = info->stream[addr];
		DICT_ADD_BYTE(dict, "map_h");
		map_w = info->stream[addr];
		DICT_ADD_BYTE(dict, "map_w");
		DICT_ADD_WORD(dict, "map_pointer");

		current_map->map_w = map_w;
		current_map->map_h = map_h;

		PyArg_ParseTuple(PyDict_GetItemString(dict, "map_pointer"), "ii", &word_addr, &rom_addr);

		text_pointers = GET_ADDR(addr);
		DICT_ADD_WORD(dict, "map_text_pointers");
		DICT_ADD_WORD(dict, "map_scripts");
		connect_byte = info->stream[addr];
		DICT_ADD_BYTE(dict, "connect_byte");
	}

	/* Connections */

	connection_addr = addr;

	list = PyList_New(0);
	for (i = 0; i < (u8)(sizeof(cons) / sizeof(*cons)); i++) {
		if (!(connect_byte & cons[i].k))
			continue ;
		PyObject *con_dict = PyDict_New();

		PyDict_SetItemString(con_dict, "key", Py_BuildValue("c", cons[i].c));
		DICT_ADD_BYTE(con_dict, "index");
		DICT_ADD_WORD(con_dict, "connected_map");
		DICT_ADD_WORD(con_dict, "current_map");
		DICT_ADD_BYTE(con_dict, "bigness");
		DICT_ADD_BYTE(con_dict, "map_width");
		DICT_ADD_BYTE(con_dict, "y_align");
		DICT_ADD_BYTE(con_dict, "x_align");
		DICT_ADD_WORD(con_dict, "window");
		PyList_Append(list, con_dict);
	}
	PyDict_SetItemString(dict, "connections", list);

	/* Object Data */
	{
		int word_addr;
		u8 nb;

		DICT_ADD_WORD(dict, "object_data");

		/* Seek to the object data address */
		PyArg_ParseTuple(PyDict_GetItemString(dict, "object_data"), "ii", &word_addr, &addr);

		DICT_ADD_BYTE(dict, "maps_border_tile");

		/* Warps */
		warp_list = PyList_New(0);
		nb = info->stream[addr++];
		for (i = 0; i < nb; i++) {
			PyObject *warp_dict = PyDict_New();

			DICT_ADD_BYTE(warp_dict, "y");
			DICT_ADD_BYTE(warp_dict, "x");
			DICT_ADD_BYTE(warp_dict, "to_point");
			DICT_ADD_BYTE(warp_dict, "to_map");
			PyList_Append(warp_list, warp_dict);
		}
		PyDict_SetItemString(dict, "warps", warp_list);

		/* Signs */
		sign_list = PyList_New(0);
		nb = info->stream[addr++];
		for (i = 0; i < nb; i++) {
			PyObject *sign_dict = PyDict_New();

			DICT_ADD_BYTE(sign_dict, "y");
			DICT_ADD_BYTE(sign_dict, "x");

			{
				u8 tid = info->stream[addr];
				int base_addr = (addr / 0x4000) * 0x4000;
				int text_pointer = GET_ADDR(base_addr + (text_pointers + ((tid - 1) << 1)) % 0x4000);
				int rom_text_pointer = ((text_pointer < 0x4000) ? 0 : base_addr) + text_pointer % 0x4000;
				int rom_text_addr = ROM_ADDR(info->stream[rom_text_pointer + 3], GET_ADDR(rom_text_pointer + 1)) + 1;
				char buffer[512] = {0};
				u8 c;
				unsigned int d = 0;

				if (info->stream[rom_text_pointer] == 0x17) {
					while ((c = info->stream[rom_text_addr])) {
						char *append = get_pkmn_char(c, "¿?");

						memcpy(buffer + d, append, strlen(append));
						d += strlen(append);
						if (d >= sizeof(buffer))
							break;
						rom_text_addr++;
					}
					PyDict_SetItemString(sign_dict, "text", Py_BuildValue("s", buffer));
				}
			}

			DICT_ADD_BYTE(sign_dict, "text_id");
			PyList_Append(sign_list, sign_dict);
		}
		PyDict_SetItemString(dict, "signs", sign_list);

		/* Entities (normal people, trainers, items) */
		entity_list = PyList_New(0);
		nb = info->stream[addr++];
		for (i = 0; i < nb; i++) {
			u8 tid;
			PyObject *entities_dict = PyDict_New();

			DICT_ADD_BYTE(entities_dict, "pic_id");
			PyDict_SetItemString(entities_dict, "y", Py_BuildValue("i", info->stream[addr++] - 4));	// -4 for some reason. Don't ask me why.
			PyDict_SetItemString(entities_dict, "x", Py_BuildValue("i", info->stream[addr++] - 4));
			DICT_ADD_BYTE(entities_dict, "mvt_1");
			DICT_ADD_BYTE(entities_dict, "mvt_2");
			tid = info->stream[addr];
			DICT_ADD_BYTE(entities_dict, "text_id");

			if (tid & (1 << 6)) {
				DICT_ADD_BYTE(entities_dict, "trainer_type");
				DICT_ADD_BYTE(entities_dict, "pkmn_set");
			} else if (tid & (1 << 7)) {
				DICT_ADD_BYTE(entities_dict, "item_id");
			}

			PyList_Append(entity_list, entities_dict);
		}
		PyDict_SetItemString(dict, "entities", entity_list);
	}

	current_map->pixbuf = get_map_pic_raw(rom_addr, map_w, map_h, blockdata_addr, tiles_addr, dict);
	apply_filter(current_map->pixbuf, id, map_w * 2);

	{
		PyObject *map_pic;

		map_pic = Py_BuildValue("s#", current_map->pixbuf, PIXBUF_BLOCK_SIZE * map_w * map_h);
		PyDict_SetItemString(dict, "map_pic", map_pic);
	}

	map_w *= 2;
	map_h *= 2;

	current_map->coords.x = x_init;
	current_map->coords.y = y_init;

	for (i = 0; i < (u8)(sizeof(cons) / sizeof(*cons)); i++) {
		connection_type con;
		int nx = 0, ny = 0;

		if (!(connect_byte & cons[i].k))
			continue;
		memcpy(&con, &info->stream[connection_addr], sizeof(con));
		connection_addr += sizeof(con);

		switch (cons[i].c) {
			// FIXME: I'm sure there is something wrong here...
			case 'N':
				nx = x_init - (char)(con.x_align);
				ny = y_init - (u8)(con.y_align) - 1;
				break;
			case 'S':
				nx = x_init - (char)(con.x_align);
				ny = y_init - (char)(con.y_align) + map_h;
				break;
			case 'W':
				nx = x_init - (char)(con.x_align) - 1;
				ny = y_init - (char)(con.y_align);
				break;
			case 'E':
				nx = x_init - (char)(con.x_align) + map_w;
				ny = y_init - (char)(con.y_align);
				break;
		}

		to_add = get_submap(con.index, get_map_addr(con.index), nx, ny);
		if (to_add) {
			last = current_map;
			for (tmp = current_map; tmp; tmp = tmp->next)
				last = tmp;
			last->next = to_add;
		}
	}

	return current_map;
}

static coords_type process_submap(submap_type *map)
{
	int xmin = 0, ymin = 0, xmax = 0, ymax = 0;
	coords_type s;
	submap_type *map_start = map;

	for (; map; map = map->next) {
		int x1 = map->coords.x;
		int y1 = map->coords.y;
		int x2 = x1 + map->map_w * 2;
		int y2 = y1 + map->map_h * 2;

		if (x1 < xmin)
			xmin = x1;
		if (y1 < ymin)
			ymin = y1;
		if (x2 > xmax)
			xmax = x2;
		if (y2 > ymax)
			ymax = y2;
	}

	// Shift coords so they start at (0, 0)
	for (map = map_start; map; map = map->next) {
		map->coords.x -= xmin;
		map->coords.y -= ymin;
	}

	s.x = -xmin + xmax;
	s.y = -ymin + ymax;
	return s;
}

#define NB_PIXEL_PER_BLOCK_LINE 32
#define PIXBUF_SINGLE_LINE_SIZE(w) ((w) * NB_PIXEL_PER_BLOCK_LINE * 3)

static char *items_keys[] = {"warps", "signs", "entities"};

static void insert_objects(PyObject *objects, PyObject *items, int x0, int y0)
{
	unsigned int i;

	for (i = 0; i < sizeof(items_keys) / sizeof(*items_keys); i++) {
		int j;
		PyObject *data = PyDict_GetItemString(items, items_keys[i]);

		for (j = 0; j < PyList_Size(data); j++) {
			PyObject *item = PyList_GetItem(data, j);
			long x = PyLong_AsLong(PyDict_GetItemString(item, "x")) + x0;
			long y = PyLong_AsLong(PyDict_GetItemString(item, "y")) + y0;

			PyDict_SetItem(objects, Py_BuildValue("ii", x, y), item);
		}
	}
}

static map_type get_final_map(submap_type *map)
{
	coords_type map_info = process_submap(map);
	map_type final_map;

	final_map.w = map_info.x / 2;
	final_map.h = map_info.y / 2;
	final_map.id = 0;
	final_map.info_list = PyList_New(0);
	if (!map || !map->pixbuf) {
		final_map.pixbuf = NULL;
		return final_map;
	}

	final_map.pixbuf = calloc(1, PIXBUF_BLOCK_SIZE * final_map.w * final_map.h);
	final_map.objects = PyDict_New();

	for (; map; map = map->next) {
		int x, y, line, final_pixbuf_pos, pixbuf_pos = 0, pad;

		x = map->coords.x;
		y = map->coords.y;

		insert_objects(map->objects, map->info, 0, 0);
		insert_objects(final_map.objects, map->info, x, y);

		if (final_map.id == 0 || (final_map.id != 0 && map->id < final_map.id))
			final_map.id = map->id;

		PyDict_SetItemString(map->info, "objects", map->objects);
		PyList_Append(final_map.info_list, map->info);

		pad = PIXBUF_SINGLE_LINE_SIZE(map->map_w);

		final_pixbuf_pos = y * (final_map.w * 2) * PIXBUF_BOX_SIZE + PIXBUF_SINGLE_LINE_SIZE(x) / 2;

		for (line = 0; line < map->map_h * NB_PIXEL_PER_BLOCK_LINE; line++) {
			memcpy(&final_map.pixbuf[final_pixbuf_pos], &map->pixbuf[pixbuf_pos], pad);
			final_pixbuf_pos += PIXBUF_SINGLE_LINE_SIZE(final_map.w);
			pixbuf_pos += pad;
		}

		free(map->pixbuf);
		free(map);
	}

	return final_map;
}

static PyObject *get_py_map(submap_type *map)
{
	PyObject *py_map = PyDict_New();
	PyObject *map_pic;
	map_type final_map;

	if (!map)
		return NULL;

	final_map = get_final_map(map);

	if (!final_map.pixbuf)
		return NULL;

	map_pic = Py_BuildValue("s#", final_map.pixbuf, PIXBUF_BLOCK_SIZE * final_map.w * final_map.h);

	PyDict_SetItemString(py_map, "id", Py_BuildValue("i", final_map.id));
	PyDict_SetItemString(py_map, "map_w", Py_BuildValue("i", final_map.w));
	PyDict_SetItemString(py_map, "map_h", Py_BuildValue("i", final_map.h));
	PyDict_SetItemString(py_map, "map_pic", map_pic);
	PyDict_SetItemString(py_map, "info", final_map.info_list);
	PyDict_SetItemString(py_map, "objects", final_map.objects);

	return py_map;
}

PyObject *get_maps(PyObject *self, PyObject *args)
{
	(void)self;
	(void)args;
	int i, addr;
	PyObject *map, *list = PyList_New(0);

	for (i = 0; i < NB_MAPS; i++) {
		addr = get_map_addr(i);
		if (is_loaded(addr))
			continue;
		if (!(map = get_py_map(get_submap(i, addr, 0, 0))))
			continue;
		PyList_Append(list, map);
	}
	return list;
}
