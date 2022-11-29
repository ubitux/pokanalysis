# ![#pokeball#](img/pokeball.png) Pokanalysis ![#pokeball#](img/pokeball.png)

Pokanalysis is a tool to analyse the first generation Pokémon ROMS… At least
Red and Blue US. It's only a viewer (no editing feature).

**Note**: this repository is old and and its dependencies depreciated a long
time ago. It's likely challenging to make it run again.

## Screenshots

<a href="img/pokedex-sandshrew.png"><img src="img/small_pokedex-sandshrew.png" alt="Pokédex (Sandshrew)" /></a>
<a href="img/area-cerulean.png"><img src="img/small_area-cerulean.png" alt="Area (Cerulean city)" /></a>
<a href="img/viridian-city.png"><img src="img/small_viridian-city.png" alt="Viridian city" /></a>
<a href="img/safari-wild-pkmn.png"><img src="img/small_safari-wild-pkmn.png" alt="Wild Pokémon in Safari" /></a>
<a href="img/trainers-erika.png"><img src="img/small_trainers-erika.png" alt="Erika trainer" /></a>
<a href="img/disasm-pallet-script.png"><img src="img/small_disasm-pallet-script.png" alt="Disassembler (Pallet town script)" /></a>

## Current features

- Complete Pokédex
- Basic disassembler
- Main game area (Kanto) with merged or split maps
- Interior maps (houses, caves, etc.)
- Entering maps by clicking on warps/doors
- Wild Pokémon with their names and sprites
- Text of most of the signs
- Special items (hidden objects for example)
- Entities (people, trainers, items) and their default orientation
- Trainers list and teams

## Dependencies

- Python ~= 2.6
- PyGTK (GTK+ ~= 2.16)
- GCC (build only)

## Build

```shell
% make
% ./pokanalysis.py /path/to/your/pokemon/rom.gb
```

## TODO

- Map names
- Special items text and type
- Support more text (entities, special signs, …)
- More ROMS support (better addresses support)
- Collisions
- Search engine (wild Pokémon, items, …)
- Better maps GUI (previous/next, area position…)
- Editor?

## License

[ISC](https://www.isc.org/licenses/)
