# Map Editor
A simple map editor for creating a tiled map. 

## How it works (so far)
setup.sh writes example map and tile data to an SQLite database. The data are
loaded into a C program and rendered in a grid of tiles with size 32x32 pixels.

## Commands

command prefix is `:`

1: tile <key>: selects active tile for placement.
2: save <name>: saves current map.
2: load <name>: loads saved map.

## Roadmap

- Feature: sprite tilesets.
- Feature: z-axis.
- Feature: minimap.






