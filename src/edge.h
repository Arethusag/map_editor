// edge.h
#ifndef EDGE_H
#define EDGE_H

#include "database.h"
#include "draw.h"
#include <raylib.h>

typedef struct {
  int tileKey;
  int priority;
} NeighborInfo;

typedef struct {
  Texture2D texture;
  int priority;
} EdgeTextureInfo;

void processCorner(NeighborInfo *edgeNumbers, NeighborInfo *neighbors,
                   bool *visitedTiles, int index, int adjacent1, int adjacent2);

void processDiagonal(NeighborInfo *edgeNumbers, NeighborInfo *neighbors,
                     bool *visitedTiles, int index, int adjacent1,
                     int adjacent2);

Texture2D getEdge(Edge *edgeTypes, int countEdges, int tileKey, int edgeNumber);

int compareEdgeTextures(const void *a, const void *b);

void getEdgeTextures(Map *map, int x, int y, Tile tileTypes[], Edge edgeTypes[],
                     Texture2D resultTextures[], int *textureCount);

void computeEdges(int edgeGrid[][2], int edgeGridCount, Map *map,
                  Tile tileTypes[], Edge edgeTypes[]);

void computeMapEdges(Tile tileTypes[], Edge edgeTypes[], Map *map);

bool visitedCheck(int visitedTiles[][2], int visitedCount, int x, int y);

void calculateEdgeGrid(DrawingState *drawState, int visitedTiles[][2],
                       int *visitedCount);

#endif // EDGE_H
