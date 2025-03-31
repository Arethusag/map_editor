// draw.h
#ifndef DRAW_H
#define DRAW_H

// includes
#include "database.h"
#include "grid.h"

// functions
int abs(int x);

int getRandTileStyle(int tileKey, Tile *tileTypes);

void calculatePath(WorldCoords coords, int path[][2]);

void drawPreview(Map *currentMap, int drawnTiles[][3], int drawnTilesCount,
                 Tile tileTypes[], Edge edgeTypes[], int activeTileKey);

void applyTiles(Map *map, int placedTiles[][3], int placedCount,
                int activeTileKey);

void drawExistingMap(Map *map, Tile tileTypes[], Wall wallTypes[],
                     Camera2D camera, int screenWidth, int screenHeight);

void updateDrawnTiles(int coordArray[][2], int coordArraySize,
                      int drawnTiles[][3], int *drawnTilesCount,
                      int activeTileKey, Tile *tileTypes);

#endif // DRAW_H
