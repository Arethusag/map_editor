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
      if (drawState->pathMode == PATH_DIAGONAL) {
        int x0, y0, sx, sy;
        switch (drawState->pathQuadrant) {
        case QUADRANT_SOUTHEAST:
          x0 = minX;
          y0 = minY;
          sx = +1;
          sy = +1;
          break;
        case QUADRANT_NORTHEAST:
          x0 = minX;
          y0 = maxY;
          sx = +1;
          sy = -1;
          break;
        case QUADRANT_SOUTHWEST:
          x0 = maxX;
          y0 = minY;
          sx = -1;
          sy = +1;
          break;
        case QUADRANT_NORTHWEST:
          x0 = maxX;
          y0 = maxY;
          sx = -1;
          sy = -1;
          break;
        default:
          x0 = minX;
          y0 = minY;
          sx = +1;
          sy = +1;
          break;
        }

        int dx = sx * (x - x0);
        int dy = sy * (y - y0);
        int len = (maxX - minX < maxY - minY) ? (maxX - minX) : (maxY - minY);

        bool onDiag = (dx == dy && dx >= 0 && dx <= len);
        bool isStart = (dx == 0 && dy == 0);
        bool isEnd = (dx == len && dy == len);

        switch (drawState->pathQuadrant) {
        case QUADRANT_SOUTHEAST:
          if (isStart) {
            orient = WALL_STYLE_POST;
          } else if (isEnd) {
            orient = (drawState->diagonalPriority == PRIORITY_X)
                         ? WALL_STYLE_VERTICAL
                         : WALL_STYLE_HORIZONTAL;
          } else if (onDiag) {
            orient = (drawState->diagonalPriority == PRIORITY_X)
                         ? WALL_STYLE_VERTICAL
                         : WALL_STYLE_HORIZONTAL;
          } else {
            orient = (drawState->diagonalPriority == PRIORITY_X)
                         ? WALL_STYLE_HORIZONTAL
                         : WALL_STYLE_VERTICAL;
          }
          break;
        case QUADRANT_NORTHEAST:
          if (isStart) {
            orient = (drawState->diagonalPriority == PRIORITY_X)
                         ? WALL_STYLE_POST
                         : WALL_STYLE_VERTICAL;
          } else if (isEnd) {
            orient = (drawState->diagonalPriority == PRIORITY_X)
                         ? WALL_STYLE_POST
                         : WALL_STYLE_HORIZONTAL;
          } else if (onDiag) {
            orient = (drawState->diagonalPriority == PRIORITY_X)
                         ? WALL_STYLE_POST
                         : WALL_STYLE_CORNER;
          } else {
            orient = (drawState->diagonalPriority == PRIORITY_X)
                         ? WALL_STYLE_CORNER
                         : WALL_STYLE_POST;
          }
          break;
        case QUADRANT_SOUTHWEST:
          if (isStart) {
            orient = (drawState->diagonalPriority == PRIORITY_Y)
                         ? WALL_STYLE_POST
                         : WALL_STYLE_HORIZONTAL;
          } else if (isEnd) {
            orient = (drawState->diagonalPriority == PRIORITY_Y)
                         ? WALL_STYLE_POST
                         : WALL_STYLE_VERTICAL;
          } else if (onDiag) {
            orient = (drawState->diagonalPriority == PRIORITY_Y)
                         ? WALL_STYLE_POST
                         : WALL_STYLE_CORNER;
          } else {
            orient = (drawState->diagonalPriority == PRIORITY_Y)
                         ? WALL_STYLE_CORNER
                         : WALL_STYLE_POST;
          }
          break;
        case QUADRANT_NORTHWEST:
          if (isStart) {
            orient = (drawState->diagonalPriority == PRIORITY_Y)
                         ? WALL_STYLE_VERTICAL
                         : WALL_STYLE_HORIZONTAL;
          } else if (isEnd) {
            orient = WALL_STYLE_POST;
          } else if (onDiag) {
            orient = (drawState->diagonalPriority == PRIORITY_Y)
                         ? WALL_STYLE_VERTICAL
                         : WALL_STYLE_HORIZONTAL;
          } else {
            orient = (drawState->diagonalPriority == PRIORITY_Y)
                         ? WALL_STYLE_HORIZONTAL
                         : WALL_STYLE_VERTICAL;
          }
          break;
        default:
          orient = WALL_STYLE_POST;
          break;
        }

      } else if (x == maxX && y == maxY) {
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
