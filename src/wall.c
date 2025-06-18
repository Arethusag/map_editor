#include "wall.h"
#include "database.h"
#include "draw.h"
#include "edge.h"
#include <limits.h>
#include <raylib.h>
#include <sqlite3.h>
#include <stdio.h>

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

void calculateWallOrientations(DrawingState *drawState, WallOrientMap *map) {
  int srcKey = drawState->activeWallKey;
  int n = drawState->drawnTilesCount;

  fprintf(stderr, "CALC: count=%d, activeWallKey=%d\n", n, srcKey);

  if (n <= 0)
    return;

  // Initialize bounds to the first tile
  int x0 = drawState->drawnTiles[0][0];
  int y0 = drawState->drawnTiles[0][1];
  int minX = x0, maxX = x0;
  int minY = y0, maxY = y0;

  // Compute true min/max
  for (int i = 1; i < n; i++) {
    int x = drawState->drawnTiles[i][0];
    int y = drawState->drawnTiles[i][1];
    if (x < minX)
      minX = x;
    if (x > maxX)
      maxX = x;
    if (y < minY)
      minY = y;
    if (y > maxY)
      maxY = y;
  }

  fprintf(stderr, "  bounds: min=(%d,%d) max=(%d,%d)\n", minX, minY, maxX,
          maxY);

  for (int i = 0; i < n; i++) {
    int x = drawState->drawnTiles[i][0];
    int y = drawState->drawnTiles[i][1];
    int orient = WALL_STYLE_NONE;

    // corner/post/edge logic
    switch (drawState->drawMode) {
    case MODE_BOX:
      if (x == maxX && y == maxY)
        orient = WALL_STYLE_CORNER;
      else if (x == minX && y == minY)
        orient = WALL_STYLE_POST;
      else if (x == minX && y == maxY)
        orient = WALL_STYLE_VERTICAL;
      else if (x == maxX && y == minY)
        orient = WALL_STYLE_HORIZONTAL;
      else if (y == minY || y == maxY)
        orient = WALL_STYLE_HORIZONTAL;
      else if (x == minX || x == maxX)
        orient = WALL_STYLE_VERTICAL;
      break;
    case MODE_PATHING:
      if (x == maxX && y == maxY) {
        if (drawState->pathQuadrant == QUADRANT_SOUTHEAST &&
            drawState->pathMode == PATH_STEEP) {
          orient = WALL_STYLE_HORIZONTAL;
        } else if (drawState->pathQuadrant == QUADRANT_SOUTHEAST &&
                   drawState->pathMode == PATH_SHALLOW) {
          orient = WALL_STYLE_VERTICAL;
        } else if (drawState->pathQuadrant == QUADRANT_NORTHWEST &&
                   drawState->pathMode == PATH_SHALLOW) {
          orient = WALL_STYLE_HORIZONTAL;
        } else if (drawState->pathQuadrant == QUADRANT_NORTHWEST &&
                   drawState->pathMode == PATH_STEEP) {
          orient = WALL_STYLE_VERTICAL;
        } else if (drawState->pathQuadrant == QUADRANT_NORTH ||
                   drawState->pathQuadrant == QUADRANT_SOUTH) {
          orient = WALL_STYLE_VERTICAL;
        } else if (drawState->pathQuadrant == QUADRANT_WEST ||
                   drawState->pathQuadrant == QUADRANT_EAST) {
          orient = WALL_STYLE_HORIZONTAL;
        } else {
          // Default case for this corner
          orient = WALL_STYLE_CORNER;
        }
      } else if (x == minX && y == minY) {
        if (drawState->pathQuadrant == QUADRANT_NORTHWEST &&
            drawState->pathMode == PATH_STEEP) {
          orient = WALL_STYLE_HORIZONTAL;
        } else if (drawState->pathQuadrant == QUADRANT_NORTHWEST &&
                   drawState->pathMode == PATH_SHALLOW) {
          orient = WALL_STYLE_VERTICAL;
        } else {
          // Default case for this corner
          orient = WALL_STYLE_POST;
        }
      } else if (x == minX && y == maxY) {
        if (drawState->pathQuadrant == QUADRANT_SOUTHWEST &&
            drawState->pathMode == PATH_STEEP) {
          orient = WALL_STYLE_HORIZONTAL;
        } else if (drawState->pathQuadrant == QUADRANT_SOUTHWEST &&
                   drawState->pathMode == PATH_SHALLOW) {
          orient = WALL_STYLE_VERTICAL;
        } else if (drawState->pathQuadrant == QUADRANT_NORTHEAST &&
                   drawState->pathMode == PATH_SHALLOW) {
          orient = WALL_STYLE_POST;
        } else {
          // Default case for this corner
          orient = WALL_STYLE_VERTICAL;
        }
      } else if (x == maxX && y == minY) {
        if (drawState->pathQuadrant == QUADRANT_NORTHEAST &&
            drawState->pathMode == PATH_STEEP) {
          orient = WALL_STYLE_HORIZONTAL;
        } else if (drawState->pathQuadrant == QUADRANT_NORTHEAST &&
                   drawState->pathMode == PATH_SHALLOW) {
          orient = WALL_STYLE_VERTICAL;
        } else if (drawState->pathQuadrant == QUADRANT_SOUTHWEST &&
                   drawState->pathMode == PATH_STEEP) {
          orient = WALL_STYLE_POST;
        } else {
          // Default case for this corner
          orient = WALL_STYLE_HORIZONTAL;
        }
      } else if (y == minY || y == maxY) {
        orient = WALL_STYLE_HORIZONTAL;
      } else if (x == minX || x == maxX) {
        orient = WALL_STYLE_VERTICAL;
      }
    case MODE_PAINTER:
      break;
    }

    // debug
    fprintf(stderr, "  tile %2d: (%2d,%2d) => orient=%d ", i, x, y, orient);

    // hash lookup
    unsigned idx = ((unsigned)srcKey ^ ((unsigned)orient << 1)) % map->capacity;
    Entry *e = map->buckets[idx];

    int target = srcKey;
    while (e) {
      if (e->sourceWallKey == srcKey && e->orientationKey == orient) {
        target = e->targetWallKey;
        break;
      }
      e = e->next;
    }

    fprintf(stderr, "-> idx=%u, target=%d\n", idx, target);

    drawState->drawnTiles[i][2] = target;
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
