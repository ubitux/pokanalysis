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

#define MAP_IDS_ADDR	ROM_ADDR(0x11, 0x6a40)

PyObject *get_special_items(int map_id)
{
	info_t *info = get_info();
	int map_id_addr;
	int index = 0;
	PyObject *list = PyList_New(0);

	for (map_id_addr = MAP_IDS_ADDR; info->stream[map_id_addr] != 0xff; map_id_addr++, index += 2) {
		if (info->stream[map_id_addr] != map_id)
			continue;

		int item_ptr_addr = ROM_ADDR(0x11, 0x6a96 + index);
		int item_addr = ROM_ADDR(0x11, GET_ADDR(item_ptr_addr));

		// [MAP #%03d] Y=%03d X=%03d item-id=%03d type=0x%02x unknown-address=0x%04x,
		PyList_Append(list, Py_BuildValue("iiiii",
					info->stream[item_addr],
					info->stream[item_addr + 1],
					info->stream[item_addr + 2],
					info->stream[item_addr + 3],
					GET_ADDR(info->stream[item_addr + 4])));
	}
	return list;
}

void apply_filter(u8 *pixbuf, int map_id, int w)
{
	info_t *info = get_info();
	int map_id_addr;
	int index = 0;

	if (!pixbuf)
		return;

	for (map_id_addr = MAP_IDS_ADDR; info->stream[map_id_addr] != 0xff; map_id_addr++, index += 2) {
		if (info->stream[map_id_addr] != map_id)
			continue;

		int item_ptr_addr = ROM_ADDR(0x11, 0x6a96 + index);
		int item_addr = ROM_ADDR(0x11, GET_ADDR(item_ptr_addr));
		int y = info->stream[item_addr];
		int x = info->stream[item_addr + 1];
		int offset = (y * 16 * (w * 16) + x * 16) * 3;
		int i;

		if (x == 0xff || y == 0xff)
			return;

		for (i = 0; i < 16; i++)
			memcpy(&pixbuf[offset + i * 3], "\xFF\x00\x00", 3);

		for (i = 0; i < 16; i++)
			memcpy(&pixbuf[offset + (w * 16 * 3) * i], "\xFF\x00\x00", 3);

		for (i = 0; i < 16; i++)
			memcpy(&pixbuf[offset + (w * 16 * 3) * i + 15 * 3], "\xFF\x00\x00", 3);

		offset += (w * 16 * 3) * 15;
		for (i = 0; i < 16; i++)
			memcpy(&pixbuf[offset + i * 3], "\xFF\x00\x00", 3);
	}
}
