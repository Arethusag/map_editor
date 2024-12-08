# Map Editor
A simple map editor for creating a tiled map. 

## How it works
setup.sh writes example map and tile data to an SQLite database. The data are
loaded into a C program and rendered in a grid of tiles with size 32x32 pixels.

SQLite is the primary interface for IO and data operations.

## Build

Build on and run on linux with `make run`.

Dependencies:
 - gcc
 - make
 - raylib
 - sqlite3

## Commands

command prefix is `:`

1: tile <key>: selects active tile for placement.
2: save <name>: saves current map.
2: load <name>: loads saved map.

## Utils

A number of bash scripts are included in the utils folder. These scripts are
never executed at runtime. They are used in the build stage or as development
tools.

`extract_tiles.sh` is used in the build stage to parse .png assets into binary
RGBA and insert the blobs into the SQLite database. The parsing intructions are
declared in the `assets/sprites.conf` file.

`create_grid_reference.sh` is a development tool that takes as argument a .png
file and produces a copy with a 32x32 pixel grid overlay with cell numbering.

## Roadmap

- Feature: implement random style variations for ground tiles.
- Feature: undo/redo





