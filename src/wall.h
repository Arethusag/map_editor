// wall.h
#ifndef WALL_H
#define WALL_H

// includes
#include "database.h"
#include "draw.h"
#include <raylib.h>

// functions
void computeMapWalls(Wall wallTypes[], Map *map);

void computeWalls(int wallGrid[][2], int wallGridCount, Map *map,
                  Wall wallTypes[]);

void calculateWallGrid(DrawingState *drawState, int visitedTiles[][2],
                       int *visitedCount);

#endif // WALL_H
