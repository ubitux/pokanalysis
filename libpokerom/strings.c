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

static struct {
	u8 c;
	char *s;
} alphabet[] = {
	{0x49, "\n"},	// pokédex desc sentence separator
	{0x4E, "\n"},	// pokédex desc normal \n
	{0x4F, "\n"},
	{0x51, "\n\n"},
	{0x54, "POKé"},
	{0x55, "\n"},	// stop with arrow indicator
	{0x57, ""},	// ending stop without arrow
	{0x5F, ""},	// pokémon cry?
	{0x7F, " "}, {0x80, "A"}, {0x81, "B"}, {0x82, "C"}, {0x83, "D"}, {0x84, "E"},
	{0x85, "F"}, {0x86, "G"}, {0x87, "H"}, {0x88, "I"}, {0x89, "J"}, {0x8A, "K"},
	{0x8B, "L"}, {0x8C, "M"}, {0x8D, "N"}, {0x8E, "O"}, {0x8F, "P"}, {0x90, "Q"},
	{0x91, "R"}, {0x92, "S"}, {0x93, "T"}, {0x94, "U"}, {0x95, "V"}, {0x96, "W"},
	{0x97, "X"}, {0x98, "Y"}, {0x99, "Z"}, {0x9A, "("}, {0x9B, ")"}, {0x9C, ":"},
	{0x9D, ";"}, {0x9E, "]"}, {0x9F, "["}, {0xA0, "a"}, {0xA1, "b"}, {0xA2, "c"},
	{0xA3, "d"}, {0xA4, "e"}, {0xA5, "f"}, {0xA6, "g"}, {0xA7, "h"}, {0xA8, "i"},
	{0xA9, "j"}, {0xAA, "k"}, {0xAB, "l"}, {0xAC, "m"}, {0xAD, "n"}, {0xAE, "o"},
	{0xAF, "p"}, {0xB0, "q"}, {0xB1, "r"}, {0xB2, "s"}, {0xB3, "t"}, {0xB4, "u"},
	{0xB5, "v"}, {0xB6, "w"}, {0xB7, "x"}, {0xB8, "y"}, {0xB9, "z"}, {0xBA, "é"},
	{0xBB, "'d"}, {0xBC, "'l"}, {0xBD, "'s"}, {0xBE, "'t"}, {0xBF, "'v"},
	{0xE0, "'"}, {0xE1, "PK"}, {0xE2, "MN"}, {0xE3, "-"}, {0xE4, "'r"},
	{0xE5, "'m"}, {0xE6, "?"}, {0xE7, "!"}, {0xE8, "."}, {0xEF, "♂"},
	{0xF0, "$"}, {0xF1, "x"}, {0xF2, "."}, {0xF3, "/"}, {0xF4, ","}, {0xF5, "♀"},
	{0xF6, "0"}, {0xF7, "1"}, {0xF8, "2"}, {0xF9, "3"}, {0xFA, "4"}, {0xFB, "5"},
	{0xFC, "6"}, {0xFD, "7"}, {0xFE, "8"}, {0xFF, "9"}
};

char *get_pkmn_char(u8 c, char *def_ret)
{
	size_t i;

	for (i = 0; i < sizeof(alphabet) / sizeof(*alphabet); i++)
		if (alphabet[i].c == c)
			return alphabet[i].s;
	return def_ret;
}

void load_string(char *dest, u8 *src, size_t max_len, int fixed_str_len)
{
	int i, len;

	for (i = 0;; i += len) {
		char *insert = get_pkmn_char(*src++, "");
		if (!*insert)
			goto end;
		len = strlen(insert);
		if ((fixed_str_len && i >= fixed_str_len) || (i + len >= (int)max_len))
			goto end;
		strcpy(&dest[i], insert);
	}
end:
	dest[i] = 0;
}

PyObject *str_getbin(PyObject *self, PyObject *args)
{
	int i, j = 0;
	char *s;
	char b[64];

	for (PyArg_ParseTuple(args, "s", &s); *s; s++) {
		for (i = 0; i < (int)(sizeof(alphabet) / sizeof(*alphabet)) && j < (int)sizeof(b); i++) {
			if (*alphabet[i].s && strncmp(s, alphabet[i].s, strlen(alphabet[i].s)) == 0) {
				b[j++] = alphabet[i].c;
				break;
			}
		}
	}
	b[j] = 0;
	return Py_BuildValue("s", b);
}

PyObject *str_getascii(PyObject *self, PyObject *args)
{
	int i, j = 0;
	char *s;
	char b[64];

	for (PyArg_ParseTuple(args, "s", &s); *s; s++) {
		for (i = 0; i < (int)(sizeof(alphabet) / sizeof(*alphabet)) && j < (int)sizeof(b); i++) {
			if ((u8)*s == alphabet[i].c) {
				strcpy(&b[j], alphabet[i].s);
				j += strlen(alphabet[i].s);
				break;
			}
		}
	}
	b[j] = 0;
	return Py_BuildValue("s", b);
}

