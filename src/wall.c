#include "wall.h"
#include "database.h"
#include <raylib.h>
#include <stdio.h>

void computeWalls(int wallGrid[][2], int wallGridCount, Map *map,
                  Wall wallTypes[]) {

  for (int i = 0; i < wallGridCount; i++) {
    int x = wallGrid[i][0];
    int y = wallGrid[i][1];

    int currentWallKey = map->grid[x][y][2];

    if (currentWallKey != 0) {

      int wallQuadrant[3][2] = {
          {x - 1, y - 1}, // NW
          {x, y - 1},     // NE
          {x - 1, y}      // SW
      };

      for (int j = 0; j < 3; j++) {
        Texture2D tex = wallTypes[currentWallKey].wallTex[j].tex;
        int _x = wallQuadrant[j][0];
        int _y = wallQuadrant[j][1];
        int wallCount = map->wallCount[_x][_y];
        if (_x >= 0 && _x < GRID_SIZE && _y >= 0 && _y < GRID_SIZE) {
          if (wallCount == 0) {
            map->walls[_x][_y][0] = tex;
            map->wallCount[_x][_y] = 1;
          } else if (wallCount == 1) {
            map->wallCount[_x][_y] = 2;
            map->walls[_x][_y][1] = tex;
          } else if (wallCount == 2) {
            map->wallCount[_x][_y] = 3;
            map->walls[_x][_y][1] = tex;
          } else {
            printf("Error: Too many wall tiles!");
          }
        } else {
          printf("Warning: Wall quadrant out of bounds (x=%d, y=%d)\n", _x, _y);
        }
      }
    }
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
