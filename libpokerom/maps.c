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

static void load_tile(u8 *__restrict__ pixbuf, u8 *__restrict__ src, int color_key)
{
	char **colors = color_set[color_key];

	for (int y = 0; y < TILE_Y; y++) {
		u8 byte1 = *src++;
		u8 byte2 = *src++;

		for (int x = TILE_X - 1; x >= 0; x--) {
			memcpy(pixbuf, colors[(byte1>>x & 1) << 1 | (byte2>>x & 1)], 3);
			pixbuf += 3;
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
	int pixbuf_offset = 0;

	for (int j = 0; j < SPRITE_Y; j++) {
		for (int i = 0; i < SPRITE_X; i++) {
			u8 tile_pixbuf[PIXBUF_TILE_SIZE];
			int tile_offset = 0;

			load_tile(tile_pixbuf, src, DEFAULT_COLORS_OFFSET);
			src += 0x10;
			for (int y = 0; y < TILE_Y; y++) {
				memcpy(&dst[pixbuf_offset], &tile_pixbuf[tile_offset], PIXBUF_TILE_LINE_SIZE);
				tile_offset   += PIXBUF_TILE_LINE_SIZE;
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

struct warp_raw   { u8 y, x, to_point, to_map; } PACKED;
struct sign_raw   { u8 y, x, tid; } PACKED;
struct entity_raw { u8 pic_id, y, x, mvt_1, mvt_2, tid, extra_1, extra_2; } PACKED;

struct coords {
	int x;
	int y;
};

struct map_header {
	u8 tileset_id;
	u8 map_h, map_w;
	u16 map_ptr, text_ptr, script_ptr;
	u8 connect_byte;
} PACKED;

struct submap {
	int id;
	int addr;
	int bank;
	struct map_header *header;
	int n_cons;
	u8 *cons;
	u16 obj_addr;
	u8 *obj_data;
	int n_warps, n_signs, n_entities;
	struct warp_raw *warps;
	struct sign_raw *signs;
	struct entity_raw *entities;
	PyObject *info;
	int loaded;
	struct coords coords;
	u8 *pixbuf;
	PyObject *objects;
	struct submap *next;
};

static struct box_info get_box_info(u8 *stream, struct submap *map, int x, int y)
{
	struct box_info bi = {.color_key = DEFAULT_COLORS_OFFSET, .entity_addr = 0, .flip = 0};

	for (int i = 0; i < map->n_warps; i++) {
		if (map->warps[i].x == x && map->warps[i].y == y) {
			bi.color_key = WARPS_COLORS_OFFSET;
			return bi;
		}
	}

	for (int i = 0; i < map->n_signs; i++) {
		if (map->signs[i].x == x && map->signs[i].y == y) {
			bi.color_key = SIGNS_COLORS_OFFSET;
			return bi;
		}
	}

	u8 *entities = (void *)map->entities;
	for (int i = 0; i < map->n_entities; i++) {
		struct entity_raw *entity = (void *)entities;

		if (entity->x - 4 == x && entity->y - 4 == y) {
			int entity_info_addr = ROM_ADDR(0x05, 0x7b27 + 4 * (entity->pic_id - 1));
			int bank_id = stream[entity_info_addr + 3];
			u8 orientation = entity->mvt_2 & 0xF;
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

		if (entity->tid & 1<<6)
			entities += 2;
		else if (entity->tid & 1<<7)
			entities += 1;
		entities += sizeof(*entity) - 2;
	}

	return bi;
}

#define PIXBUF_BOX_SIZE		(PIXBUF_TILE_SIZE * 2 * 2)
#define PIXBUF_BOX_LINE_SIZE	(PIXBUF_TILE_LINE_SIZE * 2)

static void merge_tiles(u8 *dst, u8 *src, char *alpha)
{
	for (int i = 0; i < PIXBUF_TILE_SIZE; i += 3)
		if (memcmp(&src[i], alpha, 3) != 0)
			memcpy(&dst[i], &src[i], 3);
}

static void flip_tile(u8 *tile)
{
	u8 old_tile[PIXBUF_TILE_SIZE];

	memcpy(old_tile, tile, sizeof(old_tile));
	for (int y = 0; y < TILE_Y; y++)
		for (int x = 0; x < TILE_X; x++)
			memcpy(&tile[3 * (y * TILE_X + x)], &old_tile[3 * (y * TILE_X + (TILE_X - x - 1))], 3);
}

#define BLOCK_X 4
#define BLOCK_Y 4

/* 1 block = 4x4 tiles */
static void load_block(u8 *stream, struct submap *map, u8 *pixbuf, u8 *blocks, u8 *tiles, int bx, int by)
{
	int pixbuf_offset = 0;

	for (int j = 0; j < BLOCK_Y; j++) {
		for (int i = 0; i < BLOCK_X; i++) {
			u8 tile_pixbuf[PIXBUF_TILE_SIZE];
			int tile_offset = 0;
			struct box_info bi = get_box_info(stream, map, bx * 2 + i / 2, by * 2 + j / 2);

			if (bi.entity_addr) {
				int n = (j % 2) * 2 + (i + bi.flip) % 2;
				u8 entity_pixbuf[PIXBUF_TILE_SIZE];

				load_tile(entity_pixbuf, &stream[bi.entity_addr + n * 16], bi.color_key);
				if (bi.flip)
					flip_tile(entity_pixbuf);
				load_tile(tile_pixbuf, &tiles[*blocks++ * 16], DEFAULT_COLORS_OFFSET);
				merge_tiles(tile_pixbuf, entity_pixbuf, color_set[ENTITIES_COLORS_OFFSET][0]);
			} else {
				load_tile(tile_pixbuf, &tiles[*blocks++ * 16], bi.color_key);
			}

			for (int y = 0; y < TILE_Y; y++) {
				memcpy(&pixbuf[pixbuf_offset], &tile_pixbuf[tile_offset], PIXBUF_TILE_LINE_SIZE);
				tile_offset += PIXBUF_TILE_LINE_SIZE;
				pixbuf_offset += BLOCK_X * PIXBUF_TILE_LINE_SIZE;
			}
			pixbuf_offset = pixbuf_offset - TILE_Y * (BLOCK_X * PIXBUF_TILE_LINE_SIZE) + PIXBUF_TILE_LINE_SIZE;
		}
		pixbuf_offset += (TILE_Y - 1) * (BLOCK_X * PIXBUF_TILE_LINE_SIZE);
	}
}

#define TILESET_HEADERS ROM_ADDR(3, 0x47BE)

static int get_blockdata_addr(u8 *stream, u8 tileset_id)
{
	int header_offset = TILESET_HEADERS + tileset_id * 12;
	int bank_id = stream[header_offset];

	return ROM_ADDR(bank_id, GET_ADDR(header_offset + 1));
}

static int get_tiles_addr(u8 *stream, u8 tileset_id)
{
	int header_offset = TILESET_HEADERS + tileset_id * 12;
	int bank_id = stream[header_offset];

	return ROM_ADDR(bank_id, GET_ADDR(header_offset + 2 + 1));
}

#define NB_PIXEL_PER_BLOCK_LINE	32
#define PIXBUF_BLOCK_SIZE	(PIXBUF_BOX_SIZE * 2 * 2)
#define PIXBUF_BLOCK_LINE_SIZE	(PIXBUF_BOX_LINE_SIZE * 2)

static u8 *get_map_pic_raw(u8 *stream, struct submap *map)
{
	u8 map_h = map->header->map_h;
	u8 map_w = map->header->map_w;
	u8 *pixbuf = malloc(map_w * map_h * PIXBUF_BLOCK_SIZE);
	int pixbuf_offset = 0;

	u8 *datablocks = &stream[get_blockdata_addr(stream, map->header->tileset_id)];
	u8 *tiles      = &stream[get_tiles_addr(stream,     map->header->tileset_id)];
	u8 *mapblocks  = &stream[ROM_ADDR(map->bank,        map->header->map_ptr)];

	for (int j = 0; j < map_h; j++) {
		for (int i = 0; i < map_w; i++) {
			u8 block_pixbuf[PIXBUF_BLOCK_SIZE];
			int y, block_offset = 0;

			load_block(stream, map, block_pixbuf, &datablocks[*mapblocks++ * 16], tiles, i, j);
			for (y = 0; y < NB_PIXEL_PER_BLOCK_LINE; y++) {
				memcpy(&pixbuf[pixbuf_offset], &block_pixbuf[block_offset], PIXBUF_BLOCK_LINE_SIZE);
				block_offset += PIXBUF_BLOCK_LINE_SIZE;
				pixbuf_offset += map_w * PIXBUF_BLOCK_LINE_SIZE;
			}
			pixbuf_offset = pixbuf_offset - NB_PIXEL_PER_BLOCK_LINE * (map_w * PIXBUF_BLOCK_LINE_SIZE) + PIXBUF_BLOCK_LINE_SIZE;
		}
		pixbuf_offset += (NB_PIXEL_PER_BLOCK_LINE - 1) * (map_w * PIXBUF_BLOCK_LINE_SIZE);
	}
	return pixbuf;
}

static struct {
	u8 k;
	char c;
} cons[] = {
	{1 << 3, 'N'},
	{1 << 2, 'S'},
	{1 << 1, 'W'},
	{1 << 0, 'E'}
};

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
	PyObject *list = PyList_New(0);

	for (int i = 0; i < 10; i++) {
		if (!data[0])
			break;
		PyList_Append(list, Py_BuildValue("ii", data[0], data[1]));
		data += 2;
	}
	return list;
}

static PyObject *get_wild_pokemon(u8 *stream, int map_id)
{
	PyObject *dict = PyDict_New();
	u8 *data = &stream[ROM_ADDR(0x03, GET_ADDR(ROM_ADDR(0x03, 0x4EEB + map_id * 2)))];
	u8 rate;

	rate = *data++;
	if (rate) {
		PyDict_SetItemString(dict, "grass-rate", Py_BuildValue("i", rate));
		PyDict_SetItemString(dict, "grass",      get_wild_pokemon_at_addr(data));
		data += 20;
	}

	rate = *data++;
	if (rate) {
		PyDict_SetItemString(dict, "water-rate", Py_BuildValue("i", rate));
		PyDict_SetItemString(dict, "water",      get_wild_pokemon_at_addr(data));
	}

	return dict;
}

static struct submap *get_submap(struct rom *rom, struct submap *maps, int id, int x_init, int y_init)
{
	u8 *stream = rom->stream;
	struct submap *current_map = &maps[id], *last, *tmp, *to_add;

	if (current_map->loaded)
		return NULL;

	PyObject *dict = PyDict_New();
	struct map_header *header = current_map->header;
	u8 connect_byte = header->connect_byte;
	u8 map_h = header->map_h, map_w = header->map_w;
	int addr = current_map->addr;

	current_map->loaded = 1;

	current_map->info = dict;
	current_map->objects = PyDict_New();

	PyDict_SetItemString(dict, "id",                 Py_BuildValue("i", id));
	PyDict_SetItemString(dict, "bank-id",            Py_BuildValue("i", current_map->bank));
	PyDict_SetItemString(dict, "addr",               Py_BuildValue("i", REL_ADDR(addr)));
	PyDict_SetItemString(dict, "wild-pkmn",          get_wild_pokemon(stream, id));
	PyDict_SetItemString(dict, "special-items",      get_special_items(stream, id));

	/* Map Header */
	PyDict_SetItemString(dict, "tileset",            Py_BuildValue("i", header->tileset_id));
	PyDict_SetItemString(dict, "map_h",              Py_BuildValue("i", map_h));
	PyDict_SetItemString(dict, "map_w",              Py_BuildValue("i", map_w));
	PyDict_SetItemString(dict, "map-pointer",        Py_BuildValue("i", header->map_ptr));
	PyDict_SetItemString(dict, "map-text-pointer",   Py_BuildValue("i", header->text_ptr));
	PyDict_SetItemString(dict, "map-script-pointer", Py_BuildValue("i", header->script_ptr));
	PyDict_SetItemString(dict, "connect_byte",       Py_BuildValue("i", connect_byte));

	/* Object Data */
	PyDict_SetItemString(dict, "object-data",        Py_BuildValue("i", current_map->obj_addr));

	/* Seek to the object data address */
	addr = current_map->obj_data - stream;
	PyDict_SetItemString(dict, "maps_border_tile",   Py_BuildValue("i", stream[addr++]));

	u8 nb;
	PyObject *list;

	/* Warps */
	nb = stream[addr++];
	list = PyList_New(0);
	struct warp_raw *warp = (void *)&stream[addr];
	current_map->n_warps = nb;
	current_map->warps   = warp;
	for (int i = 0; i < nb; i++) {
		PyObject *warp_dict = PyDict_New();

		PyDict_SetItemString(warp_dict, "y",        Py_BuildValue("i", warp->y));
		PyDict_SetItemString(warp_dict, "x",        Py_BuildValue("i", warp->x));
		PyDict_SetItemString(warp_dict, "to_point", Py_BuildValue("i", warp->to_point));
		PyDict_SetItemString(warp_dict, "to_map",   Py_BuildValue("i", warp->to_map));
		PyList_Append(list, warp_dict);
		warp++;
		addr += sizeof(*warp);
	}
	PyDict_SetItemString(dict, "warps", list);

	/* Signs */
	nb = stream[addr++];
	list = PyList_New(0);
	struct sign_raw *sign = (void *)&stream[addr];
	current_map->n_signs = nb;
	current_map->signs   = sign;
	for (int i = 0; i < nb; i++) {
		PyObject *sign_dict = PyDict_New();

		PyDict_SetItemString(sign_dict, "y",       Py_BuildValue("i", sign->y));
		PyDict_SetItemString(sign_dict, "x",       Py_BuildValue("i", sign->x));
		PyDict_SetItemString(sign_dict, "text_id", Py_BuildValue("i", sign->tid));

		int base_addr = (addr / 0x4000) * 0x4000;
		int text_pointer = GET_ADDR(base_addr + (header->text_ptr + ((sign->tid - 1) << 1)) % 0x4000);
		int rom_text_pointer = ((text_pointer < 0x4000) ? 0 : base_addr) + text_pointer % 0x4000;
		int rom_text_addr = ROM_ADDR(stream[rom_text_pointer + 3], GET_ADDR(rom_text_pointer + 1)) + 1;
		char buffer[512] = {0};
		u8 c;
		unsigned int d = 0;

		if (stream[rom_text_pointer] == 0x17) {
			while ((c = stream[rom_text_addr])) {
				char *append = get_pkmn_char(c, "¿?");

				memcpy(buffer + d, append, strlen(append));
				d += strlen(append);
				if (d >= sizeof(buffer))
					break;
				rom_text_addr++;
			}
			PyDict_SetItemString(sign_dict, "text", Py_BuildValue("s", buffer));
		}

		PyList_Append(list, sign_dict);
		sign++;
		addr += sizeof(*sign);
	}
	PyDict_SetItemString(dict, "signs", list);

	/* Entities (normal people, trainers, items) */
	nb = stream[addr++];
	list = PyList_New(0);
	struct entity_raw *entity = (void *)&stream[addr];
	current_map->n_entities = nb;
	current_map->entities   = entity;
	for (int i = 0; i < nb; i++) {
		PyObject *entity_dict = PyDict_New();

		PyDict_SetItemString(entity_dict, "pic_id",  Py_BuildValue("i", entity->pic_id));
		PyDict_SetItemString(entity_dict, "y",       Py_BuildValue("i", entity->y - 4));
		PyDict_SetItemString(entity_dict, "x",       Py_BuildValue("i", entity->x - 4));
		PyDict_SetItemString(entity_dict, "mvt_1",   Py_BuildValue("i", entity->mvt_1));
		PyDict_SetItemString(entity_dict, "mvt_2",   Py_BuildValue("i", entity->mvt_2));
		PyDict_SetItemString(entity_dict, "text_id", Py_BuildValue("i", entity->tid));

		if (entity->tid & 1<<6) {
			PyDict_SetItemString(entity_dict, "trainer_type", Py_BuildValue("i", entity->extra_1));
			PyDict_SetItemString(entity_dict, "pkmn_set",     Py_BuildValue("i", entity->extra_2));
			addr += 2;
			add_trainer(rom, id, entity->x-4, entity->y-4, entity->extra_1, entity->extra_2);
		} else if (entity->tid & 1<<7) {
			char iname[30];

			get_pkmn_item_name(stream, iname, entity->extra_1, sizeof(iname));
			PyDict_SetItemString(entity_dict, "item_id",   Py_BuildValue("i", entity->extra_1));
			PyDict_SetItemString(entity_dict, "item_name", Py_BuildValue("s", iname));
			addr += 1;
		}

		PyList_Append(list, entity_dict);
		addr += sizeof(*entity) - 2;
		entity = (void *)&stream[addr];
	}
	PyDict_SetItemString(dict, "entities", list);

	current_map->pixbuf = get_map_pic_raw(stream, current_map);
	apply_filter(stream, current_map->pixbuf, id, map_w * 2);

	PyDict_SetItemString(dict, "map_pic", Py_BuildValue("s#", current_map->pixbuf, PIXBUF_BLOCK_SIZE * map_w * map_h));

	current_map->coords.x = x_init;
	current_map->coords.y = y_init;

	list = PyList_New(0);
	struct connection *con = (void *)current_map->cons;
	for (int i = 0; i < (u8)(sizeof(cons) / sizeof(*cons)); i++) {
		int nx = 0, ny = 0;

		if (!(connect_byte & cons[i].k))
			continue;

		PyObject *con_dict = PyDict_New();
		PyDict_SetItemString(con_dict, "key",           Py_BuildValue("c", cons[i].c));
		PyDict_SetItemString(con_dict, "index",         Py_BuildValue("i", con->index));
		PyDict_SetItemString(con_dict, "connected-map", Py_BuildValue("i", con->connected_map));
		PyDict_SetItemString(con_dict, "current-map",   Py_BuildValue("i", con->current_map));
		PyDict_SetItemString(con_dict, "bigness",       Py_BuildValue("i", con->bigness));
		PyDict_SetItemString(con_dict, "map_width",     Py_BuildValue("i", con->map_width));
		PyDict_SetItemString(con_dict, "y_align",       Py_BuildValue("i", con->y_align));
		PyDict_SetItemString(con_dict, "x_align",       Py_BuildValue("i", con->x_align));
		PyDict_SetItemString(con_dict, "window",        Py_BuildValue("i", con->window));
		PyList_Append(list, con_dict);

		switch (cons[i].c) {
		// FIXME: I'm sure there is something wrong here...
		case 'N':
			nx = x_init - (char)(con->x_align);
			ny = y_init - (u8)(con->y_align) - 1;
			break;
		case 'S':
			nx = x_init - (char)(con->x_align);
			ny = y_init - (char)(con->y_align) + map_h * 2;
			break;
		case 'W':
			nx = x_init - (char)(con->x_align) - 1;
			ny = y_init - (char)(con->y_align);
			break;
		case 'E':
			nx = x_init - (char)(con->x_align) + map_w * 2;
			ny = y_init - (char)(con->y_align);
			break;
		}

		to_add = get_submap(rom, maps, con->index, nx, ny);
		if (to_add) {
			last = current_map;
			for (tmp = current_map; tmp; tmp = tmp->next)
				last = tmp;
			last->next = to_add;
		}

		con += 1;
	}
	PyDict_SetItemString(dict, "connections", list);

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
		int x2 = x1 + map->header->map_w * 2;
		int y2 = y1 + map->header->map_h * 2;

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

#define PIXBUF_SINGLE_LINE_SIZE(w) ((w) * NB_PIXEL_PER_BLOCK_LINE * 3)

static char *items_keys[] = {"warps", "signs", "entities"};

static void insert_objects(PyObject *objects, PyObject *items, int x0, int y0)
{
	for (int i = 0; i < (int)(sizeof(items_keys) / sizeof(*items_keys)); i++) {
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
		struct submap *last_map;

		int x = map->coords.x;
		int y = map->coords.y;

		insert_objects(map->objects, map->info, 0, 0);
		insert_objects(final_map.objects, map->info, x, y);

		if (final_map.id == 0 || (final_map.id != 0 && map->id < final_map.id))
			final_map.id = map->id;

		PyDict_SetItemString(map->info, "objects", map->objects);
		PyList_Append(final_map.info_list, map->info);

		int pad = PIXBUF_SINGLE_LINE_SIZE(map->header->map_w);

		int final_pixbuf_pos = y * (final_map.w * 2) * PIXBUF_BOX_SIZE + PIXBUF_SINGLE_LINE_SIZE(x) / 2;

		int pixbuf_pos = 0;
		for (int line = 0; line < map->header->map_h * NB_PIXEL_PER_BLOCK_LINE; line++) {
			memcpy(&final_map.pixbuf[final_pixbuf_pos], &map->pixbuf[pixbuf_pos], pad);
			final_pixbuf_pos += PIXBUF_SINGLE_LINE_SIZE(final_map.w);
			pixbuf_pos       += pad;
		}

		last_map = map;
		map = map->next;
		free(last_map->pixbuf);
	}

	return final_map;
}

static PyObject *get_py_map(struct submap *map)
{
	PyObject *py_map = PyDict_New();
	PyObject *map_pic;

	if (!map)
		return NULL;

	struct map final_map = get_final_map(map);

	if (!final_map.pixbuf)
		return NULL;

	map_pic = Py_BuildValue("s#", final_map.pixbuf, PIXBUF_BLOCK_SIZE * final_map.w * final_map.h);

	PyDict_SetItemString(py_map, "id",      Py_BuildValue("i", final_map.id));
	PyDict_SetItemString(py_map, "map_w",   Py_BuildValue("i", final_map.w));
	PyDict_SetItemString(py_map, "map_h",   Py_BuildValue("i", final_map.h));
	PyDict_SetItemString(py_map, "map_pic", map_pic);
	PyDict_SetItemString(py_map, "info",    final_map.info_list);
	PyDict_SetItemString(py_map, "objects", final_map.objects);

	return py_map;
}

static void track_maps(u8 *stream, struct submap *maps, int map_id)
{
	struct submap *map = &maps[map_id];

	if (map->addr || map_id == 0xed || map_id == 0xff) // 0xed is the elevator special warp destination
		return;

	map->id     = map_id;
	map->addr   = ROM_ADDR(stream[ROM_ADDR(3, 0x423D) + map_id], GET_ADDR(0x01AE + map_id * 2));
	map->bank   = map->addr / 0x4000;
	map->header = (void *)&stream[map->addr];
	map->cons   = (void *)(map->header + 1);

	u8 cb = map->header->connect_byte;
	map->n_cons = ((cb&8)>>3) + ((cb&4)>>2) + ((cb&2)>>1) + (cb&1);
	for (int i = 0; i < map->n_cons; i++) {
		struct connection *con = (void *)(map->cons + sizeof(*con) * i);
		track_maps(stream, maps, con->index);
	}
	map->obj_addr = *(u16 *)(map->cons + sizeof(struct connection) * map->n_cons);
	map->obj_data = &stream[ROM_ADDR(map->bank, map->obj_addr)];
	int n_warps = *(map->obj_data + 1);
	struct warp_raw *warps = (void *)(map->obj_data + 2);
	for (int i = 0; i < n_warps; i++)
		track_maps(stream, maps, warps[i].to_map);
}

PyObject *preload_maps(struct rom *self)
{
	PyObject *list = PyList_New(0);
	struct submap maps[0x100];

	memset(maps, 0, sizeof(maps));
	track_maps(self->stream, maps, 0);
	for (int i = 0; i < (int)(sizeof(maps) / sizeof(*maps)); i++) {
		if (!maps[i].addr || maps[i].info)
			continue;
		maps[i].info = get_py_map(get_submap(self, maps, i, 0, 0));
		if (!maps[i].info)
			continue;
		PyList_Append(list, maps[i].info);
	}
	return list;
}

PyObject *get_maps(struct rom *self)
{
	return self->maps;
}
