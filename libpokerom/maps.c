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

enum {DEFAULT_COLORS_OFFSET, WARPS_COLORS_OFFSET, SIGNS_COLORS_OFFSET, ENTITIES_COLORS_OFFSET};
static char *color_set[][4] = {
	[DEFAULT_COLORS_OFFSET]  = {"\xE8\xE8\xE8", "\x58\x58\x58", "\xA0\xA0\xA0", "\x10\x10\x10"},
	[WARPS_COLORS_OFFSET]    = {"\xE8\xC0\xC0", "\xC0\x58\x58", "\xC0\xA0\xA0", "\xC0\x10\x10"},
	[SIGNS_COLORS_OFFSET]    = {"\xC0\xC0\xE8", "\x58\x58\xC0", "\xA0\xA0\xC0", "\x10\x10\xC0"},
	[ENTITIES_COLORS_OFFSET] = {"\xE8\xC0\xE8", "\xC0\x58\xC0", "\xC0\xA0\xC0", "\xC0\x10\xC0"},
};

#define TILE_X 8
#define TILE_Y 8

static void load_tile_from_ptr(u8 *__restrict__ pixbuf, u8 *__restrict__ src, int color_key)
{
	int x, y;
	char **colors = color_set[color_key];

	for (y = 0; y < TILE_Y; y++) {
		u8 byte1 = *src++;
		u8 byte2 = *src++;
		u8 mask = 1 << 7;

		for (x = 0; x < TILE_X; x++, mask >>= 1) {
			memcpy(pixbuf, colors[(!!(byte1 & mask) << 1) | !!(byte2 & mask)], 4);
			pixbuf += 3;
		}
	}
}

/* 1 tile = 8x8 px (2 bytes -> 8 pixels) */
static void load_tile(u8 *pixbuf, int addr, int color_key)
{
	if (addr > gl_rom_stat.st_size) {
		memset(pixbuf, 0, TILE_X * TILE_Y * 3);
		return;
	}
	load_tile_from_ptr(pixbuf, &gl_stream[addr], color_key);
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

			load_tile_from_ptr(tile_pixbuf, src, DEFAULT_COLORS_OFFSET);
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

struct box_info {
	int color_key;
	int entity_addr;
	int flip;
};

struct warp_raw { u8 y, x, to_point, to_map; } PACKED;
struct warp_item {
	struct warp_raw data;
	struct warp_item *next;
};

struct sign_raw { u8 y, x, tid; } PACKED;
struct sign_item {
	struct sign_raw data;
	PyObject *py_text_string;
	struct sign_item *next;
};

struct entity_raw { u8 pic_id, y, x, mvt_1, mvt_2, tid, extra_1, extra_2; } PACKED;
struct entity_item {
	struct entity_raw data;
	char *type;
	struct entity_item *next;
};

struct map_things {
	struct warp_item *warps;
	struct sign_item *signs;
	struct entity_item *entities;
};

static struct box_info get_box_info(struct map_things *mt, int x, int y)
{
	struct box_info bi = {.color_key = DEFAULT_COLORS_OFFSET, .entity_addr = 0, .flip = 0};

	struct warp_item *warp;
	for (warp = mt->warps; warp; warp = warp->next) {
		if (warp->data.x == x && warp->data.y == y) {
			bi.color_key = WARPS_COLORS_OFFSET;
			return bi;
		}
	}

	struct sign_item *sign;
	for (sign = mt->signs; sign; sign = sign->next) {
		if (sign->data.x == x && sign->data.y == y) {
			bi.color_key = SIGNS_COLORS_OFFSET;
			return bi;
		}
	}

	struct entity_item *entity;
	for (entity = mt->entities; entity; entity = entity->next) {
		if (entity->data.x == x && entity->data.y == y) {
			int entity_info_addr = ROM_ADDR(0x05, 0x7b27 + 4 * (entity->data.pic_id - 1));
			int bank_id = gl_stream[entity_info_addr + 3];
			u8 orientation = entity->data.mvt_2 & 0xF;
			int decal_id = 0;

			if (orientation == 1) {		// North
				decal_id = 1 * 64;
			} else if (orientation == 2) {	// West
				decal_id = 2 * 64;
			} else if (orientation == 3) {	// East
				decal_id = 2 * 64;
				bi.flip = 1;
			}
			bi.color_key = ENTITIES_COLORS_OFFSET;
			bi.entity_addr = ROM_ADDR(bank_id, GET_ADDR(entity_info_addr) + decal_id);
			return bi;
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

	for (i = 0; i < PIXBUF_TILE_SIZE; i += 3)
		if (memcmp(&src[i], alpha, 3) != 0)
			memcpy(&dst[i], &src[i], 3);
}

static void flip_tile(u8 *tile)
{
	u8 old_tile[PIXBUF_TILE_SIZE];
	int x, y;

	memcpy(old_tile, tile, sizeof(old_tile));
	for (y = 0; y < TILE_Y; y++)
		for (x = 0; x < TILE_X; x++)
			memcpy(&tile[3 * (y * TILE_X + x)], &old_tile[3 * (y * TILE_X + (TILE_X - x - 1))], 4);
}

/* 1 block = 4x4 tiles */
static void load_block_from_tiles_addr(u8 * __restrict__ pixbuf, int *__restrict__ tiles_addr, void *mt, int bx, int by)
{
	int i, j, pixbuf_offset = 0;

	for (j = 0; j < BLOCK_Y; j++) {
		for (i = 0; i < BLOCK_X; i++) {
			u8 tile_pixbuf[PIXBUF_TILE_SIZE];
			int y, tile_offset = 0;
			struct box_info bi = get_box_info(mt, bx * 2 + i / 2, by * 2 + j / 2);

			if (bi.entity_addr) {
				int n = (j % 2) * 2 + (i + bi.flip) % 2;
				u8 entity_pixbuf[PIXBUF_TILE_SIZE];

				load_tile(entity_pixbuf, bi.entity_addr + n * 16, bi.color_key);
				if (bi.flip)
					flip_tile(entity_pixbuf);
				load_tile(tile_pixbuf, tiles_addr[j * BLOCK_X + i], DEFAULT_COLORS_OFFSET);
				merge_tiles(tile_pixbuf, entity_pixbuf, color_set[ENTITIES_COLORS_OFFSET][0]);
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
struct blocks {
	int b[4 * 4];
};

/* 4x4 tiles (1 tile = 8x8px) */
static struct blocks *get_blocks(int n, int addr, int tiles_addr)
{
	u8 *data;
	struct blocks *blocks;
	int i, j;

	if (addr > gl_rom_stat.st_size)
		return NULL;
	if ((blocks = calloc(n, sizeof(*blocks))) == NULL)
		return NULL;
	data = &gl_stream[addr];
	for (i = 0; i < n; i++)
		for (j = 0; j < 16; j++)
			blocks[i].b[j] = (*data++ << 4) + tiles_addr;
	return blocks;
}

#define PIXEL_LINES_PER_BLOCK 32

static u8 *get_map_pic_raw(int r_map_pointer, u8 map_w, u8 map_h, int blockdata_addr, int tiles_addr, void *mt)
{
	u8 *pixbuf = malloc(map_w * map_h * PIXBUF_BLOCK_SIZE);
	int i, j, pixbuf_offset = 0;

	struct blocks *blocks = get_blocks(256, blockdata_addr, tiles_addr);
	if (!blocks)
		return NULL;

	for (j = 0; j < map_h; j++) {
		for (i = 0; i < map_w; i++) {
			u8 block_pixbuf[PIXBUF_BLOCK_SIZE];
			int y, block_offset = 0;
			int *block = blocks[gl_stream[r_map_pointer + j * map_w + i]].b;

			if (!block)
				return NULL;

			load_block_from_tiles_addr(block_pixbuf, block, mt, i, j);
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
	int header_offset = TILESET_HEADERS + tileset_id * 12;
	int bank_id = gl_stream[header_offset];

	return ROM_ADDR(bank_id, GET_ADDR(header_offset + 1));
}

static int get_tiles_addr(u8 tileset_id)
{
	int header_offset = TILESET_HEADERS + tileset_id * 12;
	int bank_id = gl_stream[header_offset];

	return ROM_ADDR(bank_id, GET_ADDR(header_offset + 2 + 1));
}

#define DICT_ADD_BYTE(dict, key) PyDict_SetItemString(dict, key, Py_BuildValue("i", gl_stream[addr++]))
#define DICT_ADD_ADDR(dict, key) do {\
	PyDict_SetItemString(dict, key, Py_BuildValue("i", GET_ADDR(addr)));\
	addr += 2;\
} while (0)

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

struct connection {
	u8 index;
	u16 connected_map;
	u16 current_map;
	u8 bigness;
	u8 map_width;
	u8 y_align;
	u8 x_align;
	u16 window;
} PACKED;

static int get_map_addr(int i)
{
	return ROM_ADDR(gl_stream[ROM_ADDR(3, 0x423D) + i], GET_ADDR(0x01AE + i * 2));
}

struct coords {
	int x;
	int y;
};

struct submap {
	struct coords coords;
	u8 *pixbuf;
	int map_w;
	int map_h;
	struct submap *next;
	PyObject *info;
	int id;
	PyObject *objects;
};

struct map {
	int w;
	int h;
	u8 *pixbuf;
	int id;
	PyObject *info_list;
	PyObject *objects;
};

static PyObject *get_wild_pokemon_at_addr(u8 *data)
{
	int i;
	PyObject *list = PyList_New(0);

	for (i = 0; i < 10; i++) {
		if (!data[0])
			break;
		PyList_Append(list, Py_BuildValue("ii", data[0], data[1]));
		data += 2;
	}
	return list;
}

static PyObject *get_wild_pokemon(int map_id)
{
	PyObject *dict = PyDict_New();
	u8 *data = &gl_stream[ROM_ADDR(0x03, GET_ADDR(ROM_ADDR(0x03, 0x4EEB + map_id * 2)))];
	u8 rate;

	rate = *data++;
	if (rate) {
		PyDict_SetItemString(dict, "grass-rate", Py_BuildValue("i", rate));
		PyDict_SetItemString(dict, "grass", get_wild_pokemon_at_addr(data));
		data += 20;
	}

	rate = *data++;
	if (rate) {
		PyDict_SetItemString(dict, "water-rate", Py_BuildValue("i", rate));
		PyDict_SetItemString(dict, "water", get_wild_pokemon_at_addr(data));
	}

	return dict;
}

static void set_map_things_in_python_dict(PyObject *dict, struct map_things *things)
{
	struct entity_item *entity = things->entities;
	struct sign_item *sign     = things->signs;
	struct warp_item *warp     = things->warps;
	PyObject *list;

	/* Warps */
	list = PyList_New(0);
	while (warp) {
		struct warp_raw *data = &warp->data;
		PyObject *py_warp = PyDict_New();

		PyDict_SetItemString(py_warp, "y", Py_BuildValue("i", data->y));
		PyDict_SetItemString(py_warp, "x", Py_BuildValue("i", data->x));
		PyDict_SetItemString(py_warp, "to_point", Py_BuildValue("i", data->to_point));
		PyDict_SetItemString(py_warp, "to_map", Py_BuildValue("i", data->to_map));
		PyList_Append(list, py_warp);
		warp = warp->next;
	}
	PyDict_SetItemString(dict, "warps", list);

	/* Signs */
	list = PyList_New(0);
	while (sign) {
		struct sign_raw *data = &sign->data;
		PyObject *py_sign = PyDict_New();

		PyDict_SetItemString(py_sign, "y", Py_BuildValue("i", data->y));
		PyDict_SetItemString(py_sign, "x", Py_BuildValue("i", data->x));
		PyDict_SetItemString(py_sign, "text_id", Py_BuildValue("i", data->tid));
		if (sign->py_text_string)
			PyDict_SetItemString(py_sign, "text", sign->py_text_string);
		PyList_Append(list, py_sign);
		sign = sign->next;
	}
	PyDict_SetItemString(dict, "signs", list);

	/* Entities */
	list = PyList_New(0);
	while (entity) {
		struct entity_raw *data = &entity->data;
		PyObject *py_entity = PyDict_New();

		PyDict_SetItemString(py_entity, "pic_id", Py_BuildValue("i", data->pic_id));
		PyDict_SetItemString(py_entity, "y", Py_BuildValue("i", data->y));
		PyDict_SetItemString(py_entity, "x", Py_BuildValue("i", data->x));
		PyDict_SetItemString(py_entity, "mvt_1", Py_BuildValue("i", data->mvt_1));
		PyDict_SetItemString(py_entity, "mvt_2", Py_BuildValue("i", data->mvt_2));
		PyDict_SetItemString(py_entity, "text_id", Py_BuildValue("i", data->tid));
		if (strcmp(entity->type, "trainer") == 0) {
			PyDict_SetItemString(py_entity, "trainer_type", Py_BuildValue("i", data->extra_1));
			PyDict_SetItemString(py_entity, "pkmn_set", Py_BuildValue("i", data->extra_2));
		} else if (strcmp(entity->type, "item") == 0) {
			char iname[30];

			get_pkmn_item_name(iname, data->extra_1, sizeof(iname));
			PyDict_SetItemString(py_entity, "item_id", Py_BuildValue("i", data->extra_1));
			PyDict_SetItemString(py_entity, "item_name", Py_BuildValue("s", iname));
		}
		PyList_Append(list, py_entity);
		entity = entity->next;
	}
	PyDict_SetItemString(dict, "entities", list);
}

#define FREE_THINGS_LIST(list) do {\
	while (list) {\
		void *last = list;\
		list = (list)->next;\
		free(last);\
	}\
} while(0)

static void free_map_things(struct map_things *things) {
	struct entity_item *entity = things->entities;
	struct sign_item *sign     = things->signs;
	struct warp_item *warp     = things->warps;

	FREE_THINGS_LIST(entity);
	FREE_THINGS_LIST(sign);
	FREE_THINGS_LIST(warp);
}

#define ADD_ITEM_IN_LIST(list) do {\
	item->next = NULL;\
	if (!last) {\
		list = last = item;\
	} else {\
		last->next = item;\
		last = item;\
	}\
} while (0)

static struct submap *get_submap(int id, int addr, int x_init, int y_init)
{
	PyObject *dict = PyDict_New(), *list;
	struct map_things map_things = {.warps = NULL, .signs = NULL, .entities = NULL};
	int blockdata_addr, tiles_addr, rom_addr;
	u8 connect_byte, i;
	u8 map_h, map_w;
	int connection_addr;
	struct submap *current_map, *last, *tmp, *to_add;
	int text_pointers;
	u8 bank_id;

	if (is_loaded(addr)) {
		return NULL;
	}

	add_loaded_map(addr);

	if (addr > gl_rom_stat.st_size)
		return NULL;

	current_map = malloc(sizeof(*current_map));

	current_map->info = dict;
	current_map->next = NULL;
	current_map->id = id;
	current_map->objects = PyDict_New();

	bank_id = addr / 0x4000;

	PyDict_SetItemString(dict, "id", Py_BuildValue("i", id));
	PyDict_SetItemString(dict, "bank-id", Py_BuildValue("i", bank_id));
	PyDict_SetItemString(dict, "addr", Py_BuildValue("i", REL_ADDR(addr)));
	PyDict_SetItemString(dict, "wild-pkmn", get_wild_pokemon(id));
	PyDict_SetItemString(dict, "special-items", get_special_items(id));

	/* Map Header */
	{
		int word_addr;

		blockdata_addr = get_blockdata_addr(gl_stream[addr]);
		tiles_addr = get_tiles_addr(gl_stream[addr]);

		DICT_ADD_BYTE(dict, "tileset");
		map_h = gl_stream[addr];
		DICT_ADD_BYTE(dict, "map_h");
		map_w = gl_stream[addr];
		DICT_ADD_BYTE(dict, "map_w");

		word_addr = GET_ADDR(addr);
		rom_addr = ROM_ADDR(bank_id, word_addr);
		PyDict_SetItemString(dict, "map-pointer", Py_BuildValue("i", word_addr));
		addr += 2;

		current_map->map_w = map_w;
		current_map->map_h = map_h;

		text_pointers = GET_ADDR(addr);

		DICT_ADD_ADDR(dict, "map-text-pointer");
		DICT_ADD_ADDR(dict, "map-script-pointer");

		connect_byte = gl_stream[addr];
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
		DICT_ADD_ADDR(con_dict, "connected-map");
		DICT_ADD_ADDR(con_dict, "current-map");
		DICT_ADD_BYTE(con_dict, "bigness");
		DICT_ADD_BYTE(con_dict, "map_width");
		DICT_ADD_BYTE(con_dict, "y_align");
		DICT_ADD_BYTE(con_dict, "x_align");
		DICT_ADD_ADDR(con_dict, "window");
		PyList_Append(list, con_dict);
	}
	PyDict_SetItemString(dict, "connections", list);

	/* Object Data */
	{
		int word_addr = GET_ADDR(addr);
		u8 nb;

		DICT_ADD_ADDR(dict, "object-data");

		/* Seek to the object data address */
		addr = ROM_ADDR(bank_id, word_addr);

		DICT_ADD_BYTE(dict, "maps_border_tile");

		/* Warps */
		nb = gl_stream[addr++];
		{
			struct warp_item *last = NULL;

			for (i = 0; i < nb; i++) {
				struct warp_item *item = malloc(sizeof(*item));
				struct warp_raw *data = &item->data;

				memcpy(data, &gl_stream[addr], sizeof(*data));
				addr += sizeof(*data);
				ADD_ITEM_IN_LIST(map_things.warps);
			}
		}

		/* Signs */
		nb = gl_stream[addr++];
		{
			struct sign_item *last = NULL;

			for (i = 0; i < nb; i++) {
				struct sign_item *item = malloc(sizeof(*item));
				struct sign_raw *data = &item->data;

				memcpy(data, &gl_stream[addr], sizeof(*data));
				addr += sizeof(*data);
				item->py_text_string = NULL;
				{
					int base_addr = (addr / 0x4000) * 0x4000;
					int text_pointer = GET_ADDR(base_addr + (text_pointers + ((data->tid - 1) << 1)) % 0x4000);
					int rom_text_pointer = ((text_pointer < 0x4000) ? 0 : base_addr) + text_pointer % 0x4000;
					int rom_text_addr = ROM_ADDR(gl_stream[rom_text_pointer + 3], GET_ADDR(rom_text_pointer + 1)) + 1;
					char buffer[512] = {0};
					u8 c;
					unsigned int d = 0;

					if (gl_stream[rom_text_pointer] == 0x17) {
						while ((c = gl_stream[rom_text_addr])) {
							char *append = get_pkmn_char(c, "¿?");

							memcpy(buffer + d, append, strlen(append));
							d += strlen(append);
							if (d >= sizeof(buffer))
								break;
							rom_text_addr++;
						}
						item->py_text_string = Py_BuildValue("s", buffer);
					}
				}
				ADD_ITEM_IN_LIST(map_things.signs);
			}
		}

		/* Entities (normal people, trainers, items) */
		nb = gl_stream[addr++];
		{
			struct entity_item *last = NULL;

			for (i = 0; i < nb; i++) {
				struct entity_item *item = malloc(sizeof(*item));
				struct entity_raw *data = &item->data;

				memcpy(data, &gl_stream[addr], sizeof(*data));
				addr += sizeof(*data) - 2;
				data->y -= 4;
				data->x -= 4;

				if (data->tid & (1 << 6)) {
					item->type = "trainer";
					addr += 2;
				} else if (data->tid & (1 << 7)) {
					item->type = "item";
					addr += 1;
				} else {
					item->type = "people";
				}
				ADD_ITEM_IN_LIST(map_things.entities);
			}
		}
	}

	set_map_things_in_python_dict(dict, &map_things);
	current_map->pixbuf = get_map_pic_raw(rom_addr, map_w, map_h, blockdata_addr, tiles_addr, &map_things);
	free_map_things(&map_things);
	apply_filter(current_map->pixbuf, id, map_w * 2);

	PyDict_SetItemString(dict, "map_pic", Py_BuildValue("s#", current_map->pixbuf, PIXBUF_BLOCK_SIZE * map_w * map_h));

	map_w *= 2;
	map_h *= 2;

	current_map->coords.x = x_init;
	current_map->coords.y = y_init;

	for (i = 0; i < (u8)(sizeof(cons) / sizeof(*cons)); i++) {
		struct connection con;
		int nx = 0, ny = 0;

		if (!(connect_byte & cons[i].k))
			continue;
		memcpy(&con, &gl_stream[connection_addr], sizeof(con));
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

static struct coords process_submap(struct submap *map)
{
	int xmin = 0, ymin = 0, xmax = 0, ymax = 0;
	struct coords s;
	struct submap *map_start = map;

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

static struct map get_final_map(struct submap *map)
{
	struct coords map_info = process_submap(map);
	struct map final_map;

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

	while (map) {
		int x, y, line, final_pixbuf_pos, pixbuf_pos = 0, pad;
		struct submap *last_map;

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

		last_map = map;
		map = map->next;
		free(last_map->pixbuf);
		free(last_map);
	}

	return final_map;
}

static PyObject *get_py_map(struct submap *map)
{
	PyObject *py_map = PyDict_New();
	PyObject *map_pic;
	struct map final_map;

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

struct map_header {
	u8 tileset_id;
	u8 map_h, map_w;
	u16 map_ptr, text_ptr, script_ptr;
	u8 connect_byte;
} PACKED;

struct maps {
	int addr;
	int bank;
	struct map_header *header;
	u8 *cons;
	u8 *obj_data;
	PyObject *info;
};

static void track_maps(struct maps *maps, int map_id)
{
	struct maps *map = &maps[map_id];

	if (map->addr || map_id == 0xed || map_id == 0xff) // 0xed is the elevator special warp destination
		return;

	map->addr   = get_map_addr(map_id);
	map->bank   = map->addr / 0x4000;
	map->header = (void *)&gl_stream[map->addr];
	map->cons   = (void *)(map->header + 1);

	u8 cb = map->header->connect_byte;
	int i, n_con = ((cb&8)>>3) + ((cb&4)>>2) + ((cb&2)>>1) + (cb&1);
	for (i = 0; i < n_con; i++) {
		struct connection *con = (void *)(map->cons + sizeof(*con) * i);
		track_maps(maps, con->index);
	}
	u16 objaddr = *(u16 *)(map->cons + sizeof(struct connection) * n_con);
	map->obj_data = &gl_stream[ROM_ADDR(map->bank, objaddr)];
	int n_warps = *(map->obj_data + 1);
	struct warp_raw *warps = (void *)(map->obj_data + 2);
	for (i = 0; i < n_warps; i++)
		track_maps(maps, warps[i].to_map);
}

PyObject *get_maps(PyObject *self)
{
	int i;
	PyObject *list = PyList_New(0);
	struct maps trackme[0x100];

	memset(trackme, 0, sizeof(trackme));
	track_maps(trackme, 0);
	for (i = 0; i < (int)(sizeof(trackme) / sizeof(*trackme)); i++) {
		if (!trackme[i].addr || trackme[i].info)
			continue;
		trackme[i].info = get_py_map(get_submap(i, trackme[i].addr, 0, 0));
		if (!trackme[i].info)
			continue;
		PyList_Append(list, trackme[i].info);
	}
	return list;
}
