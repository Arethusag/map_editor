#include "wall.h"
#include "database.h"
#include "edge.h"
#include <raylib.h>

void calculateWallGrid(DrawingState *drawState, int visitedTiles[][2],
                       int *visitedCount) {

  for (int i = 0; i < drawState->drawnTilesCount; i++) {
    int x = drawState->drawnTiles[i][0];
    int y = drawState->drawnTiles[i][1];

    int directions[3][2] = {{x, y - 1}, {x - 1, y}, {x - 1, y - 1}};

    // populate visited tiles first with placedTiles
    if (!visitedCheck(visitedTiles, *visitedCount, x, y)) {

      visitedTiles[*visitedCount][0] = x;
      visitedTiles[*visitedCount][1] = y;
      (*visitedCount)++;
    }

    for (int j = 0; j < 3; j++) {
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

void getWallTextures(Map *map, int x, int y, Wall wallTypes[],
                     Texture2D resultTextures[], int *textureCount) {

  // Order matters for draw position
  const int neighborCoords[3][2] = {
      {x + 1, y + 1}, // SE
      {x, y + 1},     // S
      {x + 1, y}      // E
  };

  // Populate neighbors
  for (int i = 2; i >= 0; i--) {
    int nx = neighborCoords[i][0];
    int ny = neighborCoords[i][1];

    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
      int neighborKey = map->grid[nx][ny][2];
      if (neighborKey != 0) {
        resultTextures[*textureCount] = wallTypes[neighborKey].wallTex[i].tex;
        (*textureCount)++;
      }
    }
  }
}

void computeWalls(int wallGrid[][2], int wallGridCount, Map *map,
                  Wall wallTypes[]) {

  for (int i = 0; i < wallGridCount; i++) {
    int x = wallGrid[i][0];
    int y = wallGrid[i][1];

    // Initialize to empty texture
    for (int j = 0; j < 3; j++) {
      map->walls[x][y][j] = (Texture2D){0};
    }

    Texture2D resultTextures[3];
    int textureCount = 0; // Keeps track of populated textures

    // Compute walls for this tile
    getWallTextures(map, x, y, wallTypes, resultTextures, &textureCount);

    for (int j = 0; j < textureCount; j++) {
      if (resultTextures[j].id != 0) {
        map->walls[x][y][j] = resultTextures[j];
      }
    }
    map->wallCount[x][y] = textureCount;
  }
}

void computeMapWalls(Wall wallTypes[], Map *map) {
  int wallGrid[GRID_SIZE * GRID_SIZE][2] = {0};
  int wallGridCount = 0;

  for (int x = 0; x < GRID_SIZE; x++) {
    for (int y = 0; y < GRID_SIZE; y++) {
      wallGrid[wallGridCount][0] = x;
      wallGrid[wallGridCount][1] = y;
      wallGridCount++;
    }
  }

  computeWalls(wallGrid, wallGridCount, map, wallTypes);
}
