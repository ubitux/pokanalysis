/*
 *  This file is part of Pokanalysis.
 *
 *  Pokanalysis
 *  Copyright © 2011 Clément Bœsch
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

static PyObject *get_trainer(u8 *stream, int id, int set_id)
{
	char tname[20];
	u8 pixbuf[(7 * 7) * (8 * 8) * 3];

	int i, lvl, addr, trainer_id = id - 0xc9;
	int class_addr = ROM_ADDR(0xE, 0x5d3b + trainer_id*2);
	int hdr_addr   = ROM_ADDR(0xE, 0x5914 + trainer_id*5);

	PyObject *trainer = PyDict_New();
	PyObject *team    = PyList_New(0);

	// locate trainer pkmn set
	addr = ROM_ADDR(0xE, GET_ADDR(class_addr));
	for (i = 1; i < set_id; i++)
		while (stream[addr++]);

	// parse trainer team
	// TODO: handle pkmn with special moves
	get_trainer_name(stream, tname, trainer_id+1, sizeof tname);
	lvl = stream[addr++];
	if (lvl != 0xffU) {
		for (i = addr; stream[i]; i++)
			PyList_Append(team, Py_BuildValue("ii", stream[i], lvl));
	} else {
		for (i = addr; stream[i]; i += 2)
			PyList_Append(team, Py_BuildValue("ii", stream[i + 1], stream[i]));
	}

	// extract trainer pic
	addr = ROM_ADDR(0x13, GET_ADDR(hdr_addr)); // bank #4 when is for link, otherwise #13
	load_sprite(stream, pixbuf, 0x77, addr);

	PyDict_SetItemString(trainer, "team", team);
	PyDict_SetItemString(trainer, "name", Py_BuildValue("s", tname));
	PyDict_SetItemString(trainer, "pic",  Py_BuildValue("s#", pixbuf, sizeof(pixbuf)));

	return trainer;
}

/* legendary pokemons, or voltorb/electrode at the power plant are considered
   special trainers */
static PyObject *get_pokemon(u8 *stream, int pkmn_id, int level)
{
	(void)stream;
	PyObject *pkmn = PyDict_New();
	PyDict_SetItemString(pkmn, "pkmn_id", Py_BuildValue("i", pkmn_id));
	PyDict_SetItemString(pkmn, "level",   Py_BuildValue("i", level));
	return pkmn;
}

void add_trainer(struct rom *rom, int map_id, int x, int y, int extra1, int extra2)
{
	PyObject *(*f[])(u8*, int, int) = {get_pokemon, get_trainer};
	PyObject *trainer = f[extra1 > 0xc8](rom->stream, extra1, extra2);

	PyDict_SetItemString(trainer, "map_id", Py_BuildValue("i", map_id));
	PyDict_SetItemString(trainer, "x",      Py_BuildValue("i", x));
	PyDict_SetItemString(trainer, "y",      Py_BuildValue("i", y));
	PyList_Append(rom->trainers, trainer);
}

PyObject *get_trainers(struct rom *self)
{
	return self->trainers;
}
