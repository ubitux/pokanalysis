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

PyObject *read_addr(PyObject *self, PyObject *args)
{
	int offset = 0;
	int addr, rom_addr;

	PyArg_ParseTuple(args, "i", &offset);
	addr = GET_ADDR(offset);
	rom_addr = ROM_ADDR(offset / 0x4000, addr);
	return Py_BuildValue("ii", addr, rom_addr);
}

static PyObject *read_data(PyObject *self, PyObject *args)
{
	u8 *s;
	int offset;
	PyObject *list = PyList_New(0);

	PyArg_ParseTuple(args, "is", &offset, &s);
	for (; *s; s++) {
		if (*s == 'B') { /* 8-bit */
			PyList_Append(list, Py_BuildValue("i", gl_stream[offset]));
			offset++;
		} else if (*s == 'W') { /* 16-bit */
			PyList_Append(list, read_addr(NULL, Py_BuildValue("(i)", offset, NULL)));
			offset += 2;
		}
	}
	return list;
}

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
		{"get_pokedex", (PyCFunction)get_pokedex, METH_NOARGS, "Get all pokémons"},
		{"disasm", disasm, METH_VARARGS, "Disassemble given bank"},

		/* Utils */
		{"str_getbin", str_getbin, METH_VARARGS, "Convert binary text to ascii"},
		{"str_getascii", str_getascii, METH_VARARGS, "Convert ascii text to binary"},
		{"read_addr", read_addr, METH_VARARGS, "Get 24-bits ROM address from 16-bit address read at the given offset"},
		{"read_data", read_data, METH_VARARGS, "Return list of 8-bit char and 16-bit address (same return as read_addr)"},
		{NULL, NULL, 0, NULL}
	};
	Py_InitModule("pokerom", m);
}
