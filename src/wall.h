// wall.h
#ifndef WALL_H
#define WALL_H

// includes
#include "database.h"
#include "draw.h"
#include <raylib.h>

#define WALL_STYLE_VERTICAL 1
#define WALL_STYLE_HORIZONTAL 2
#define WALL_STYLE_POST 3
#define WALL_STYLE_CORNER 4
#define WALL_STYLE_NONE 0

// functions
void computeMapWalls(Wall wallTypes[], Map *map);

void computeWalls(int wallGrid[][2], int wallGridCount, Map *map,
                  Wall wallTypes[]);

void calculateWallGrid(DrawingState *drawState, int visitedTiles[][2],
                       int *visitedCount);

void calculateWallOrientations(DrawingState *drawState, WallOrientMap *map);

#endif // WALL_H
