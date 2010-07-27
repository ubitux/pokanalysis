cmake_minimum_required(VERSION 2.8)
project(libpokerom)

include(FindPythonLibs)
include_directories(${PYTHON_INCLUDE_DIR})

set(sources
	libpokerom/disasm.c
	libpokerom/maps.c
	libpokerom/pokedex.c
	libpokerom/pokerom.c
	libpokerom/special_entities.c
	libpokerom/sprites.c
)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

add_definitions(-W -Wall -Os)

add_library(pokerom SHARED ${sources})
set_target_properties(pokerom PROPERTIES PREFIX "")
