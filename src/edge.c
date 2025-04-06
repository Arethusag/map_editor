#include "edge.h"
#include "database.h"
#include "draw.h"
#include <stdbool.h>
#include <stdlib.h>

void processCorner(NeighborInfo *edgeNumbers, NeighborInfo *neighbors,
                   bool *visitedTiles, int index, int adjacent1,
                   int adjacent2) {
  int opposite1 = (adjacent1 + 2) % 4; // Calculate opposites
  int opposite2 = (adjacent2 + 2) % 4;

  if (neighbors[adjacent1].tileKey == neighbors[adjacent2].tileKey &&
      neighbors[adjacent1].tileKey != 0 &&
      neighbors[adjacent1].tileKey != neighbors[opposite1].tileKey &&
      neighbors[adjacent1].tileKey != neighbors[opposite2].tileKey) {

    edgeNumbers[index] = neighbors[adjacent1];
    visitedTiles[adjacent1] = visitedTiles[adjacent2] = true;
  }
}

void processDiagonal(NeighborInfo *edgeNumbers, NeighborInfo *neighbors,
                     bool *visitedTiles, int index, int adjacent1,
                     int adjacent2) {
  if (neighbors[index].tileKey != 0 &&
      (!visitedTiles[adjacent1] ||
       neighbors[index].priority > neighbors[adjacent1].priority) &&
      (!visitedTiles[adjacent2] ||
       neighbors[index].priority > neighbors[adjacent2].priority)) {
    edgeNumbers[index] = neighbors[index];
  }
}

Texture2D getEdge(Edge *edgeTypes, int countEdges, int tileKey,
                  int edgeNumber) {
  for (int i = 0; i < countEdges; i++) {
    if (edgeTypes[i].tileKey == tileKey) {
      return edgeTypes[i].edges[edgeNumber];
    }
  }
  Texture2D defaultTexture = {0}; // Default texture initialization
  return defaultTexture;
}

int compareEdgeTextures(const void *a, const void *b) {
  const EdgeTextureInfo *edgeA = (const EdgeTextureInfo *)a;
  const EdgeTextureInfo *edgeB = (const EdgeTextureInfo *)b;
  return edgeA->priority - edgeB->priority; // Ascending order
}

void getEdgeTextures(Map *map, int x, int y, Tile tileTypes[], Edge edgeTypes[],
                     Texture2D resultTextures[], int *textureCount) {

  const int neighborCoords[8][2] = {
      {x, y - 1},     {x + 1, y},     {x, y + 1},     {x - 1, y},    // Cardinal
      {x - 1, y - 1}, {x + 1, y - 1}, {x - 1, y + 1}, {x + 1, y + 1} // Diagonal
  };

  NeighborInfo neighbors[8] = {0};
  NeighborInfo edgeNumbers[12] = {0};
  bool visitedTiles[4] = {false};

  int currentTileKey = map->grid[x][y][0];

  // Populate neighbors
  for (int i = 0; i < 8; i++) {
    int nx = neighborCoords[i][0];
    int ny = neighborCoords[i][1];

    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
      int neighborKey = map->grid[nx][ny][0];
      if (tileTypes[neighborKey].edgeIndicator == 1 &&
          tileTypes[neighborKey].edgePriority >
              tileTypes[currentTileKey].edgePriority) {
        neighbors[i].tileKey = neighborKey;
        neighbors[i].priority = tileTypes[neighborKey].edgePriority;
      }
    }
  }

  // Process corners
  processCorner(edgeNumbers, neighbors, visitedTiles, 8, 0, 3);  // Northwest
  processCorner(edgeNumbers, neighbors, visitedTiles, 9, 0, 1);  // Northeast
  processCorner(edgeNumbers, neighbors, visitedTiles, 10, 2, 3); // Southwest
  processCorner(edgeNumbers, neighbors, visitedTiles, 11, 2, 1); // Southeast

  // Process cardinal edges
  for (int i = 0; i < 4; i++) {
    if (neighbors[i].tileKey != 0 && !visitedTiles[i]) {
      edgeNumbers[i] = neighbors[i];
      visitedTiles[i] = true;
    }
  }

  // Process diagonals
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 4, 0, 3); // Northwest
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 5, 0, 1); // Northeast
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 6, 2, 3); // Southwest
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 7, 2, 1); // Southeast

  // Populate result textures
  EdgeTextureInfo edgeTextureInfoArray[12];
  int actualCount = 0;

  for (int i = 0; i < 12; i++) {
    if (edgeNumbers[i].tileKey != 0) {

      Texture2D edgeTexture =
          getEdge(edgeTypes, map->countEdges, edgeNumbers[i].tileKey, i);
      edgeTextureInfoArray[actualCount].texture = edgeTexture;
      edgeTextureInfoArray[actualCount].priority = edgeNumbers[i].priority;
      actualCount++;
    }
  }

  // Sort edgeTextureInfoArray by priority in descending order
  qsort(edgeTextureInfoArray, actualCount, sizeof(EdgeTextureInfo),
        compareEdgeTextures);

  // Extract sorted textures back into resultTextures
  for (int i = 0; i < actualCount; i++) {
    resultTextures[i] = edgeTextureInfoArray[i].texture;
  }

  // Update textureCount
  *textureCount = actualCount;
}

void computeEdges(int edgeGrid[][2], int edgeGridCount, Map *map,
                  Tile tileTypes[], Edge edgeTypes[]) {

  for (int i = 0; i < edgeGridCount; i++) {
    int x = edgeGrid[i][0];
    int y = edgeGrid[i][1];

    // Initialize to empty texture
    for (int j = 0; j < 12; j++) {
      map->edges[x][y][j] = (Texture2D){0};
    }

    Texture2D resultTextures[12];
    int textureCount = 0; // Keeps track of populated textures

    // Compute edges for this tile
    getEdgeTextures(map, x, y, tileTypes, edgeTypes, resultTextures,
                    &textureCount);

    // If a valid edge texture is found, store it
    for (int j = 0; j < textureCount; j++) {
      if (resultTextures[j].id != 0) {
        map->edges[x][y][j] = resultTextures[j];
      }
    }
    map->edgeCount[x][y] = textureCount;
  }
}

void computeMapEdges(Tile tileTypes[], Edge edgeTypes[], Map *map) {
  // Compute edges
  int edgeGrid[GRID_SIZE * GRID_SIZE][2] = {0};
  int edgeGridCount = 0;
  for (int x = 0; x < GRID_SIZE; x++) {
    for (int y = 0; y < GRID_SIZE; y++) {
      edgeGrid[edgeGridCount][0] = x;
      edgeGrid[edgeGridCount][1] = y;
      edgeGridCount++;
    }
  }
  computeEdges(edgeGrid, edgeGridCount, map, tileTypes, edgeTypes);
}

bool visitedCheck(int visitedTiles[][2], int visitedCount, int x, int y) {
  if (visitedCount == 0) {
    return false;
  } else {
    for (int i = 0; i < visitedCount; i++) {
      int vx = visitedTiles[i][0];
      int vy = visitedTiles[i][1];

      if (vx == x && vy == y) {
        return true;
      }
    }
    return false;
  }
}

void calculateEdgeGrid(DrawingState *drawState, int visitedTiles[][2],
                       int *visitedCount) {

  for (int i = 0; i < drawState->drawnTilesCount; i++) {
    int x = drawState->drawnTiles[i][0];
    int y = drawState->drawnTiles[i][1];

    int directions[8][2] = {
        {x, y - 1},     {x + 1, y},
        {x, y + 1},     {x - 1, y}, // Cardinal
        {x - 1, y - 1}, {x + 1, y - 1},
        {x - 1, y + 1}, {x + 1, y + 1} // Diagonal
    };

    // populate visited tiles first with placedTiles
    if (!visitedCheck(visitedTiles, *visitedCount, x, y)) {

      visitedTiles[*visitedCount][0] = x;
      visitedTiles[*visitedCount][1] = y;
      (*visitedCount)++;
    }

    for (int j = 0; j < 8; j++) {
      int nx = directions[j][0];
      int ny = directions[j][1];

      bool visited = visitedCheck(visitedTiles, *visitedCount, nx, ny);

      if (!visited && nx >= 0 && ny >= 0 && ny < GRID_SIZE && nx < GRID_SIZE) {

        visitedTiles[*visitedCount][0] = nx;
        visitedTiles[*visitedCount][1] = ny;
        (*visitedCount)++;
      }
    }
  }
}
