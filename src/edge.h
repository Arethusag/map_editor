// edge.h
#ifndef EDGE_H
#define EDGE_H

// includes
#include "database.h"
#include <raylib.h>

// structs
typedef struct NeighborInfo {
  int tileKey;
  int priority;
} NeighborInfo;

typedef struct EdgeTextureInfo {
  Texture2D texture;
  int priority;
} EdgeTextureInfo;

// functions
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

void computeMapEdges(Tile tileTypes[], Edge edgeTypes[]);

bool visitedCheck(int visitedTiles[][2], int visitedCount, int x, int y);

void calculateEdgeGrid(int placedTiles[][3], int placedCount,
                       int visitedTiles[][2], int *visitedCount);

#endif // EDGE_H
