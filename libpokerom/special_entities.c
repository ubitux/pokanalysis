/* vim: set et sw=4 sts=4: */

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

#define MAP_IDS_ADDR ROM_ADDR(0x11, 0x6a40)

PyObject *get_special_items(u8 *stream, int map_id)
{
    u8 *data;
    int idx = 0;
    PyObject *list = PyList_New(0);

    for (data = &stream[MAP_IDS_ADDR]; *data != 0xff; data++, idx += 2) {
        if (*data != map_id)
            continue;
        u8 *item_data = &stream[ROM_ADDR(0x11, GET_ADDR(ROM_ADDR(0x11, 0x6a96 + idx)))];
        while (item_data[0] != 0xff) {
            u16 addr = *(u16*)&item_data[4];
            if (item_data[3] == 0x1d && (addr == 0x6688 || addr == 0x6689)) {
                char iname[30];

                get_item_name(stream, iname, item_data[2], sizeof(iname));
                PyList_Append(list, Py_BuildValue("iisii", item_data[0], item_data[1], iname, item_data[3], addr));
            } else {
                PyList_Append(list, Py_BuildValue("iiiii", item_data[0], item_data[1], item_data[2], item_data[3], addr));
            }
            item_data += 6;
        }
    }
    return list;
}

void apply_filter(u8 *stream, u8 *pixbuf, int map_id, int w, int h)
{
    int map_id_addr;
    int idx = 0;

    if (!pixbuf)
        return;

    for (map_id_addr = MAP_IDS_ADDR; stream[map_id_addr] != 0xff; map_id_addr++, idx += 2) {
        if (stream[map_id_addr] != map_id)
            continue;

        int item_ptr_addr = ROM_ADDR(0x11, 0x6a96 + idx);
        int item_addr = ROM_ADDR(0x11, GET_ADDR(item_ptr_addr));
        while (stream[item_addr] != 0xff) {
            int y = stream[item_addr];
            int x = stream[item_addr + 1];
            int offset = (y*16*w*16 + x*16) * 3;
            char *c = stream[item_addr + 3] == 0x1d ? "\xff\x00\x00" : "\x00\xff\x00";
            int i;

            if (x >= w || y >= h) {
                fprintf(stderr, "super hidden entity spotted on map %d: x=%d y=%d\n", map_id, x, y);
                item_addr += 6;
                continue;
            }

            for (i = 0; i < 16; i++) memcpy(&pixbuf[offset + i*3             ], c, 3);
            for (i = 0; i < 16; i++) memcpy(&pixbuf[offset + i*w*16*3        ], c, 3);
            for (i = 0; i < 16; i++) memcpy(&pixbuf[offset + i*w*16*3  + 15*3], c, 3);
            for (i = 0; i < 16; i++) memcpy(&pixbuf[offset + w*16*3*15 + i*3 ], c, 3);
            item_addr += 6;
        }
    }
}
