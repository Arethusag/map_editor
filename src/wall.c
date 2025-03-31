#include "wall.h"
#include "database.h"
#include <raylib.h>

void computeWalls(int wallGrid[][2], int wallGridCount, Map *map,
                  Wall wallTypes[]) {

  for (int i = 0; i < wallGridCount; i++) {
    int x = wallGrid[i][0];
    int y = wallGrid[i][0];

    int currentWallKey = map->grid[x][y][2];

    if (currentWallKey == 1) {

      int wallQuadrant[3][2] = {
          {y - 1, x - 1}, // NW
          {y, x - 1},     // SW
          {y - 1, x}      // NE
      };

      for (int j = 0; j < 3; j++) {
        Texture2D tex = wallTypes[currentWallKey].wallTex[j].tex;
        int _x = wallQuadrant[j][0];
        int _y = wallQuadrant[j][1];
        map->walls[_x][_y] = tex;
        map->wallCount[_x][_y] = 1;
      }
    }
  }
}

void computeMapWalls(Wall wallTypes[]) {
  int wallGrid[GRID_SIZE * GRID_SIZE][2] = {0};
  int wallGridCount = 0;

  for (int x = 0; x < GRID_SIZE; x++) {
    for (int y = 0; y < GRID_SIZE; y++) {
      wallGrid[wallGridCount][0] = x;
      wallGrid[wallGridCount][1] = y;
      wallGridCount++;
    }
  }

  computeWalls(wallGrid, wallGridCount, &currentMap, wallTypes);
}
