/* vim: set et sw=4 sts=4: */

/*
 * Copyright © 2010-2011, Clément Bœsch
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
    /* Trainers are loaded while tracking maps data, so a pre-load is
       required to allow calling get_trainers() and get_maps() in any
       order. */
    self->trainers = PyList_New(0);
    self->maps     = preload_maps(self);
    return 0;
}

static PyMethodDef rom_methods[] = {
    {"get_maps",     (PyCFunction)get_maps,     METH_NOARGS,  "Game maps"},
    {"get_pokedex",  (PyCFunction)get_pokedex,  METH_NOARGS,  "Get all Pokémon"},
    {"get_trainers", (PyCFunction)get_trainers, METH_NOARGS,  "Get all trainers"},
    {"disasm",       (PyCFunction)disasm,       METH_VARARGS, "Disassemble given bank"},
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
