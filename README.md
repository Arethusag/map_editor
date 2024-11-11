# Map Editor
A simple map editor for creating a tiled map. 

## How it works (so far)
setup.sh writes example map and tile data to an SQLite database. The data are
loaded into a C program and rendered in a grid of tiles with size 32x32 pixels.

## Commands

command prefix is `SHIFT + ;`

1: TILE <tileKey>: selects active tile for placement.

## Roadmap

- Core: saving and loading of map databases.
- Core: click and drag tile placement.
- Core: map scrolling and panning.


- Feature: sprite tilesets.
- Feature: z-axis.
- Feature: minimap.






