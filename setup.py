#!/usr/bin/env python
# -*- coding: utf-8 -*-

import glob
from distutils.core import setup, Extension

setup(
    name='pokerom',
    version='0.6',
    description='Pokémon ROM Red & Blue (US) analysis module',
    author='Clément Bœsch',
    author_email='ubitux@gmail.com',
    url='http://pokanalysis.ubitux.fr',
    license='GPLv3',
    ext_modules=[Extension(
        'pokerom',
        sources=glob.glob('libpokerom/*.c'),
        extra_compile_args=['-Wall', '-Wextra', '-Wshadow', '-fstack-protector', '-O3', '-std=c99'],
    )],
)
