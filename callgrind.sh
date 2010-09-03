#!/bin/sh
[ ! -f "$1" ] && echo "Unable to open '$1'" && exit 1
valgrind --tool=callgrind python -c "import pokerom;pokerom.load_rom('$1');pokerom.get_maps();pokerom.get_pokedex();pokerom.disasm(1)"
