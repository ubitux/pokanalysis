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

u8 *gl_stream;
struct stat gl_rom_stat;

static PyObject *load_rom(PyObject *self, PyObject *args)
{
	char *fname;
	int fd;

	PyArg_ParseTuple(args, "s", &fname);
	if ((fd = open(fname, O_RDONLY)) < 0
			|| fstat(fd, &gl_rom_stat)
			|| (gl_stream = mmap(0, gl_rom_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		return PyErr_SetFromErrnoWithFilename(PyExc_IOError, fname);
	}
	return Py_BuildValue("z", NULL);
}

PyMODINIT_FUNC initpokerom(void)
{
	static PyMethodDef m[] = {
		{"load_rom", load_rom, METH_VARARGS, "Load ROM"},

		{"get_maps", (PyCFunction)get_maps, METH_NOARGS, "Game maps"},
		{"get_pokedex", (PyCFunction)get_pokedex, METH_NOARGS, "Get all Pokémon"},
		{"disasm", disasm, METH_VARARGS, "Disassemble given bank"},

		/* Utils */
		{"str_getbin", str_getbin, METH_VARARGS, "Convert binary text to ascii"},
		{"str_getascii", str_getascii, METH_VARARGS, "Convert ascii text to binary"},
		{NULL, NULL, 0, NULL}
	};
	Py_InitModule("pokerom", m);
}
