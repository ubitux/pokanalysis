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

static int get_bank_from_rom_pkmn_id(u8 pkmn_id) /* Red: RO01:1637 */
{
    if (pkmn_id == 0x15) return 0x01; // Mew
    if (pkmn_id == 0xb6) return 0x0b;
    if (pkmn_id < 0x1f)  return 0x09;
    if (pkmn_id < 0x4a)  return 0x0a;
    if (pkmn_id < 0x74)  return 0x0b;
    if (pkmn_id < 0x99)  return 0x0c;
    return 0x0d;
}

#define PKMN_IDS_TAB(id) stream[ROM_ADDR(0x10, 0x5024 + id)]

static u8 get_pkmn_id_from_rom_one(u8 *stream, u8 id) /* Red: RO10:5010 */
{
    return PKMN_IDS_TAB(id - 1);
}

static u8 get_pkmn_rom_id_from_pkmn_id(u8 *stream, u8 pkmn_id) /* Red: RO10:4FFB */
{
    u8 pos;
    for (pos = 0; PKMN_IDS_TAB(pos) != pkmn_id; pos++);
    return pos;
}

struct pkmn_header_raw {
    u8 id;                      // 0x00
    u8 hp, atk, def, spd, spe;  // 0x01-0x05
    u8 type1, type2;            // 0x06-0x07
    u8 capture_rate;            // 0x08
    u8 base_exp_yield;          // 0x09
    u8 sprite_front_dim;        // 0x0A
    u16 sprite_front_addr;      // 0x0B
    u16 sprite_back_addr;       // 0x0C
    u8 initial_atk[4];          // 0x0F-0x12
    u8 growth_rate;             // 0x12
    u8 TMHM_flags[7];           // 0x14-0x1A
    u8 unknown;                 // 0x1B
} PACKED;

static PyObject *get_pkmn_growth_rate(struct pkmn_header_raw *h)
{
    static char *growth_str[] = {[0] = "Medium Fast", [3] = "Medium Slow", [4] = "Fast", [5] = "Slow"};
    return Py_BuildValue("s", growth_str[h->growth_rate]);
}

static PyObject *get_pkmn_stats(struct pkmn_header_raw *h)
{
    PyObject *dict = PyDict_New();
    PyDict_SetItemString(dict, "HP",  Py_BuildValue("i", h->hp));
    PyDict_SetItemString(dict, "ATK", Py_BuildValue("i", h->atk));
    PyDict_SetItemString(dict, "DEF", Py_BuildValue("i", h->def));
    PyDict_SetItemString(dict, "SPD", Py_BuildValue("i", h->spd));
    PyDict_SetItemString(dict, "SPE", Py_BuildValue("i", h->spe));
    PyDict_SetItemString(dict, "CAP", Py_BuildValue("i", h->capture_rate));
    PyDict_SetItemString(dict, "EXP", Py_BuildValue("i", h->base_exp_yield));
    return dict;
}

static PyObject *get_pkmn_HM_TM(u8 *stream, struct pkmn_header_raw *h)
{
    PyObject *list = PyList_New(0);
    u8 *flags = h->TMHM_flags;
    u8 mask = 1;
    int id;

    for (id = 1; id < 7 * 8; id++) {
        if (*flags & mask) {
            char name[30];
            char tmhm[5];

            get_item_name(stream, tmhm, id+200, sizeof tmhm);
            get_pkmn_move_name(stream, name, stream[ROM_ADDR(0x04, 0x7773 + id - 1)], sizeof(name));
            PyList_Append(list, Py_BuildValue("ss", tmhm, name));
        }
        if (mask == 1 << 7) {
            mask = 1;
            flags++;
            continue;
        }
        mask <<= 1;
    }
    return list;
}

#define PKMN_EVENTS_ADDR(i) ROM_ADDR(0x0E, GET_ADDR(ROM_ADDR(0x0E, 0x705c + (i - 1) * 2)))

static PyObject *get_pkmn_evolutions(u8 *stream, u8 rom_pkmn_id)
{
    u8 *ptr = &stream[PKMN_EVENTS_ADDR(rom_pkmn_id)];
    PyObject *list = PyList_New(0);

    while (*ptr) {
        PyObject *evol = PyDict_New();

        switch (*ptr++) {
        case 1:
            PyDict_SetItemString(evol, "type",    Py_BuildValue("s", "level"));
            PyDict_SetItemString(evol, "level",   Py_BuildValue("i", *ptr++));
            PyDict_SetItemString(evol, "pkmn-id", Py_BuildValue("i", *ptr++));
            break;
        case 2: {
            char iname[30];
            get_item_name(stream, iname, *ptr++, sizeof(iname));
            PyDict_SetItemString(evol, "type",    Py_BuildValue("s", "stone"));
            PyDict_SetItemString(evol, "stone",   Py_BuildValue("s", iname));
            PyDict_SetItemString(evol, "level",   Py_BuildValue("i", *ptr++));
            PyDict_SetItemString(evol, "pkmn-id", Py_BuildValue("i", *ptr++));
            break;
        }
        case 3:
            PyDict_SetItemString(evol, "type",    Py_BuildValue("s", "exchange"));
            PyDict_SetItemString(evol, "level",   Py_BuildValue("i", *ptr++));
            PyDict_SetItemString(evol, "pkmn-id", Py_BuildValue("i", *ptr++));
            break;
        }
        PyList_Append(list, evol);
    }
    return list;
}

static PyObject *get_pkmn_type(u8 *stream, u8 type_id)
{
    int rom_addr = ROM_ADDR(0x09, GET_ADDR(ROM_ADDR(0x09, 0x7DAE + type_id * 2)));
    char tname[20];

    load_string(tname, &stream[rom_addr], sizeof(tname), 0);
    return Py_BuildValue("s", tname);
}

static PyObject *get_pkmn_types(u8 *stream, struct pkmn_header_raw *h)
{
    PyObject *list = PyList_New(0);

    PyList_Append(list, get_pkmn_type(stream, h->type1));
    PyList_Append(list, h->type1 == h->type2 ? Py_BuildValue("z", NULL) : get_pkmn_type(stream, h->type2));
    return list;
}

static PyObject *get_pkmn_attacks(u8 *stream, struct pkmn_header_raw *h, u8 rom_pkmn_id)
{
    PyObject *list = PyList_New(0);
    int n = 0;
    char mname[20];
    u8 *ptr;

    /* Native attacks */
    for (ptr = h->initial_atk; ptr[n] && n < 4; n++) {
        get_pkmn_move_name(stream, mname, ptr[n], sizeof(mname));
        PyList_Append(list, Py_BuildValue("is", 0, mname));
    }

    /* Levels attacks */
    ptr = &stream[PKMN_EVENTS_ADDR(rom_pkmn_id)];
    while (*ptr != 0x00)
        ptr++;
    ptr++;
    while (*ptr) {
        get_pkmn_move_name(stream, mname, ptr[1], sizeof(mname));
        PyList_Append(list, Py_BuildValue("is", ptr[0], mname));
        ptr += 2;
    }

    return list;
}

static int get_pkmn_header_address(u8 *stream, u8 rom_pkmn_id)
{
    if (rom_pkmn_id == 0x15)    // Mew
        return ROM_ADDR(0x01, 0x425B);
    return ROM_ADDR(0x0E, 0x43de + (get_pkmn_id_from_rom_one(stream, rom_pkmn_id) - 1) * 0x1C);
}

static void load_pokemon_sprite(u8 *stream, struct pkmn_header_raw *h, u8 *pixbuf, u8 rom_pkmn_id, int back)
{
    int addr;
    u16 sprite_addr;

    if (back) {
        sprite_addr = h->sprite_back_addr;
    } else {
        switch (rom_pkmn_id) {
        case 0xb6: sprite_addr = 0x79E8; break;
        case 0xb7: sprite_addr = 0x6536; break;
        case 0xb8: sprite_addr = 0x66b5; break;
        default:
            sprite_addr = h->sprite_front_addr;
        }
    }
    addr = ROM_ADDR(get_bank_from_rom_pkmn_id(rom_pkmn_id), sprite_addr);
    load_sprite(pixbuf, stream+addr, 0);
}

static PyObject *get_pixbuf(u8 *stream, struct pkmn_header_raw *h, u8 pkmn_id)
{
    u8 pixbuf[7*7 * 8*8 * 3];
    u8 pixbuf_back[7*7 * 8*8 * 3];

    load_pokemon_sprite(stream, h, pixbuf,      pkmn_id, 0);
    load_pokemon_sprite(stream, h, pixbuf_back, pkmn_id, 1);
    return Py_BuildValue("s#s#", pixbuf, sizeof(pixbuf), pixbuf_back, sizeof(pixbuf));
}

static void trad_num(u8 *b)
{
    for (; *b; b++)
        if (*b != ' ')
            *b = *b - 0xF6 + '0';
}

static void set_pkmn_texts(u8 *stream, PyObject *dict, int pkmn_rom_id)
{
    int i;
    char *s;
    char b[128];
    u8 *data = &stream[ROM_ADDR(0x10, GET_ADDR(ROM_ADDR(0x10, 0x447E + 2 * (pkmn_rom_id - 1))))];

    i = 0;
    while (*data != 0x50) {
        s = get_pkmn_char(*data++, "¿?");
        strcpy(&b[i], s);
        i += strlen(s);
    }
    PyDict_SetItemString(dict, "class", Py_BuildValue("s", b));
    data++; // skip 0x50

    sprintf(b, "%d'%02d\"", data[0], data[1]);
    data += 2;
    PyDict_SetItemString(dict, "height", Py_BuildValue("s", b));

    {
        u8 input[2];
        char *dest;
        char *b_trim;

        strcpy(b, "   ???lb");
        input[1] = *data++;
        input[0] = *data++;

        pkmn_put_nbr((u8*)b, input, 0x02, 0x05);
        trad_num((u8*)b);

        dest = b + 3;
        if (input[0] < (input[1] < 10))
            *dest = '0';
        dest++;
        *(dest + 1) = *dest;
        *dest = '.';

        strcpy(b + 6, "lb");
        for (b_trim = b; *b_trim == ' '; b_trim++);

        PyDict_SetItemString(dict, "weight", Py_BuildValue("s", b_trim));
        data++;
    }

    i = 0;
    data = &stream[ROM_ADDR(data[2], *(u16*)data)] + 1;
    while (*data != 0x50) {
        s = get_pkmn_char(*data++, "¿?");
        strcpy(&b[i], s);
        i += strlen(s);
    }
    b[i] = 0;
    PyDict_SetItemString(dict, "desc", Py_BuildValue("s", b));
}

PyObject *get_pokedex(struct rom *self)
{
    int real_pkmn_id;
    PyObject *list = PyList_New(0);

    for (real_pkmn_id = 1; real_pkmn_id <= 151; real_pkmn_id++) {
        PyObject *pkmn = PyDict_New();
        u8 pkmn_rom_id = get_pkmn_rom_id_from_pkmn_id(self->stream, real_pkmn_id) + 1;
        int header_addr = get_pkmn_header_address(self->stream, pkmn_rom_id);
        struct pkmn_header_raw *pkmn_header = (void *)&self->stream[header_addr];
        char pname[30];

        get_pkmn_name(self->stream, pname, pkmn_rom_id, sizeof pname);

        PyDict_SetItemString(pkmn, "pic",  get_pixbuf(self->stream, pkmn_header, pkmn_rom_id));
        PyDict_SetItemString(pkmn, "name", Py_BuildValue("s", pname));

        set_pkmn_texts(self->stream, pkmn, pkmn_rom_id);

        PyDict_SetItemString(pkmn, "stats",           get_pkmn_stats(pkmn_header));
        PyDict_SetItemString(pkmn, "attacks",         get_pkmn_attacks(self->stream, pkmn_header, pkmn_rom_id));
        PyDict_SetItemString(pkmn, "evolutions",      get_pkmn_evolutions(self->stream, pkmn_rom_id));
        PyDict_SetItemString(pkmn, "types",           get_pkmn_types(self->stream, pkmn_header));
        PyDict_SetItemString(pkmn, "HM_TM",           get_pkmn_HM_TM(self->stream, pkmn_header));
        PyDict_SetItemString(pkmn, "growth_rate",     get_pkmn_growth_rate(pkmn_header));
        PyDict_SetItemString(pkmn, "rom_header_addr", Py_BuildValue("i", header_addr));
        PyDict_SetItemString(pkmn, "id",              Py_BuildValue("i", real_pkmn_id));
        PyDict_SetItemString(pkmn, "rom_id",          Py_BuildValue("i", pkmn_rom_id));

        PyList_Append(list, pkmn);
    }
    return list;
}
