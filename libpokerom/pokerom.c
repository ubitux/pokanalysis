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

static int rom_init(struct rom *self, PyObject *args, PyObject *kwds)
{
	(void)kwds;
	PyArg_ParseTuple(args, "s", &self->fname);
	self->fd = open(self->fname, O_RDONLY);
	if (self->fd < 0)
		return -1;
	if (fstat(self->fd, &self->st))
		return -1;
	self->stream = mmap(0, self->st.st_size, PROT_READ, MAP_PRIVATE, self->fd, 0);
	if (self->stream == MAP_FAILED)
		return -1;
	return 0;
}

static PyMethodDef rom_methods[] = {
	{"get_maps",    (PyCFunction)get_maps,    METH_NOARGS,  "Game maps"},
	{"get_pokedex", (PyCFunction)get_pokedex, METH_NOARGS,  "Get all Pokémon"},
	{"disasm",      (PyCFunction)disasm,      METH_VARARGS, "Disassemble given bank"},
	{NULL, NULL, 0, NULL}
};

static PyTypeObject rom_type = {
	PyObject_HEAD_INIT(NULL)
	.tp_name      = "pokerom.ROM",
	.tp_basicsize = sizeof(struct rom),
	.tp_flags     = Py_TPFLAGS_DEFAULT,
	.tp_doc       = "ROM cartridge file",
	.tp_methods   = rom_methods,
	.tp_init      = (initproc)rom_init,
	.tp_new       = PyType_GenericNew,
};

PyMODINIT_FUNC initpokerom(void)
{
	static PyMethodDef module_methods[] = {
		{"str_getbin",   (PyCFunction)str_getbin,   METH_VARARGS, "Convert binary text to ascii"},
		{"str_getascii", (PyCFunction)str_getascii, METH_VARARGS, "Convert ascii text to binary"},
		{NULL, NULL, 0, NULL}
	};
	PyObject *m = Py_InitModule("pokerom", module_methods);

	if (PyType_Ready(&rom_type) < 0)
		return;
	PyModule_AddObject(m, "ROM", (PyObject *)&rom_type);
}
