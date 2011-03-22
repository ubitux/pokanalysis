#!/bin/sh
[ ! -f "$1" ] && echo "Unable to open '$1'" && exit 1
valgrind --tool=callgrind python -c "import pokerom;rom=pokerom.ROM('$1');rom.get_maps();rom.get_pokedex();rom.disasm(1)"
