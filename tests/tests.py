#!/usr/bin/env python

import os, sys, binascii
sys.path += [os.path.join(os.path.dirname(os.path.realpath(__file__)), '../'),]
import pokerom

crc32 = lambda x: binascii.crc32(x) & 0xffffffff

rom = pokerom.ROM(sys.argv[1])
dex = rom.get_pokedex()
for pkmn in dex:
    print 'PKMN %-12s %08X %08X' % (pkmn['name'], crc32(pkmn['pic'][0]), crc32(pkmn['pic'][1]))

trainers = rom.get_trainers()
for trainer in trainers:
    if 'name' in trainer:
        print 'TRAINER %-12s %08X' % (trainer['name'], crc32(trainer['pic']))

maps = rom.get_maps()
for m in maps:
    print 'MAP%03d: %08X' % (m['id'], crc32(m['map_pic']))
