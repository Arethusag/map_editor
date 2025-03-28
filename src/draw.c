// draw.c

#include "database.h"
#include "edge.h"
#include "grid.h"
#include <stdlib.h>
#include <string.h>

int abs(int x) { return x < 0 ? -x : x; }

int getRandTileStyle(int tileKey, Tile *tileTypes) {
  int texCount = tileTypes[tileKey].texCount;
  if (texCount == 0) {
    return 0;
  } else {
    int randStyle = rand() % (texCount);
    return randStyle;
  }
}

void calculatePath(WorldCoords coords, int path[][2]) {
  int dx = coords.endX - coords.startX;
  int dy = coords.endY - coords.startY;
  int steps = abs(dx) + abs(dy) + 1;
  int count = 0;
  path[count][0] = coords.startX;
  path[count][1] = coords.startY;
  count++;

  if (abs(dx) == abs(dy)) {
    int stepX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    int stepY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
    for (int i = 0; i <= steps; i++) {
      path[i][0] = coords.startX + i / 2 * stepX;
      path[i][1] = coords.startY + i / 2 * stepY;
    }
  } else if (abs(dx) > abs(dy)) {
    int bendX = (dx != 0) ? coords.endX : coords.startX;
    int bendY = (dy != 0) ? coords.startY : coords.endY;

    // Draw X path
    for (int i = 1; i <= abs(dx); i++) {
      path[count][0] = coords.startX + (dx > 0 ? i : -i);
      path[count][1] = coords.startY;
      count++;
    }

    // Draw Y path
    for (int i = 1; i <= abs(dy); i++) {
      path[count][0] = bendX;
      path[count][1] = bendY + (dy > 0 ? i : -i);
      count++;
    }
  } else if (abs(dy) > abs(dx)) {
    int count = 1;
    int bendX = (dx != 0) ? coords.startX : coords.endX;
    int bendY = (dy != 0) ? coords.startY : coords.endY;

    // Draw X path
    for (int i = 1; i <= abs(dx); i++) {
      path[count][0] = coords.startX + (dx > 0 ? i : -i);
      path[count][1] = coords.endY;
      count++;
    }

    // Draw Y path
    for (int i = 1; i <= abs(dy); i++) {
      path[count][0] = bendX;
      path[count][1] = bendY + (dy > 0 ? i : -i);
      count++;
    }
  }
}

// Draw update functions
void drawPreview(Map *currentMap, int drawnTiles[][3], int drawnTilesCount,
                 Tile tileTypes[], Edge edgeTypes[], int activeTileKey) {

  // Make temp map from drawn tiles and current map
  Map tempMap;
  memset(tempMap.grid, 0, sizeof(tempMap.grid));
  memcpy(tempMap.grid, currentMap->grid, sizeof(currentMap->grid));

  // Draw tiles
  for (int i = 0; i < drawnTilesCount; i++) {
    int x = drawnTiles[i][0];
    int y = drawnTiles[i][1];
    int style = drawnTiles[i][2];

    // Populate temp map
    tempMap.grid[x][y][0] = activeTileKey;

    // Draw the texture
    Texture2D tileTexture = tileTypes[activeTileKey].tex[style];
    Vector2 pos = {x * TILE_SIZE, y * TILE_SIZE};
    DrawTexture(tileTexture, pos.x, pos.y, WHITE);
  }

  // Draw edges
  int edgeGrid[GRID_SIZE * GRID_SIZE][2];
  int edgeGridCount = 0;
  calculateEdgeGrid(drawnTiles, drawnTilesCount, edgeGrid, &edgeGridCount);

  for (int i = 0; i < edgeGridCount; i++) {
    int x = edgeGrid[i][0];
    int y = edgeGrid[i][1];
    Vector2 pos = {x * TILE_SIZE, y * TILE_SIZE};

    Texture2D resultTextures[12];
    int textureCount = 0; // Keeps track of populated textures

    // Compute edges for this tile
    getEdgeTextures(&tempMap, x, y, tileTypes, edgeTypes, resultTextures,
                    &textureCount);

    // If a valid edge texture is found, store it
    for (int j = 0; j < textureCount; j++) {
      if (resultTextures[j].id != 0) {
        DrawTexture(resultTextures[j], pos.x, pos.y, WHITE);
      }
    }
  }
}

void applyTiles(Map *map, int placedTiles[][3], int placedCount,
                int activeTileKey) {

  for (int i = 0; i < placedCount; i++) {
    int x = placedTiles[i][0];
    int y = placedTiles[i][1];
    int style = placedTiles[i][2];

    map->grid[x][y][0] = activeTileKey;
    map->grid[x][y][1] = style;
  }
}

void drawExistingMap(Map *map, Tile tileTypes[], Camera2D camera,
                     int screenWidth, int screenHeight) {

  // Get the visible bounds of the grid
  WorldCoords bounds = GetVisibleGridBounds(camera, screenWidth, screenHeight);

  // Only draw tiles within the visible bounds
  for (int x = bounds.startX; x <= bounds.endX; x++) {
    for (int y = bounds.startY; y <= bounds.endY; y++) {
      int tileKey = map->grid[x][y][0];
      int tileStyle = map->grid[x][y][1];

      // Draw the ground tile for each grid cell
      Texture2D tileTexture = tileTypes[tileKey].tex[tileStyle];
      Vector2 pos = {x * TILE_SIZE, y * TILE_SIZE};
      DrawTexture(tileTexture, pos.x, pos.y, WHITE);

      // Draw the edge for each grid cell
      int edgeCount = map->edgeCount[x][y];
      for (int i = 0; i < edgeCount; i++) {
        Texture2D edgeTexture = map->edges[x][y][i];
        DrawTexture(edgeTexture, pos.x, pos.y, WHITE);
      }

      // // Draw the wall for each grid cell
      // Texture2D wallTexture = map->edgeCount[x][y];
      // DrawTexture(wallTexture, pos.x, pos.y, WHITE);
    }
  }
}

void updateDrawnTiles(int coordArray[][2], int coordArraySize,
                      int drawnTiles[][3], int *drawnTilesCount,
                      int activeTileKey, Tile *tileTypes) {

  // Update drawn with all new tiles in array
  for (int i = 0; i < coordArraySize; i++) {
    int x = coordArray[i][0];
    int y = coordArray[i][1];

    int alreadyVisited = 0;
    for (int j = 0; j < *drawnTilesCount; j++) {
      if (drawnTiles[j][0] == x && drawnTiles[j][1] == y) {
        alreadyVisited = 1;
        break;
      }
    }

    if (!alreadyVisited) {
      int style = getRandTileStyle(activeTileKey, tileTypes);
      drawnTiles[*drawnTilesCount][0] = x;
      drawnTiles[*drawnTilesCount][1] = y;
      drawnTiles[*drawnTilesCount][2] = style;
      (*drawnTilesCount)++;
    }
  }

  // Remove old tiles from drawn if no longer in array
  int newCount = 0;
  for (int i = 0; i < *drawnTilesCount; i++) {
    int x = drawnTiles[i][0];
    int y = drawnTiles[i][1];
    int style = drawnTiles[i][2];
    int stillInArray = 0;

    for (int j = 0; j < coordArraySize; j++) {
      if (x == coordArray[j][0] && y == coordArray[j][1]) {
        stillInArray = 1;
        break;
      }
    }

    if (stillInArray) {
      // Keep the tile in drawnTiles
      drawnTiles[newCount][0] = x;
      drawnTiles[newCount][1] = y;
      drawnTiles[newCount][2] = style;
      newCount++;
    }
  }

  // Update the drawnTilesCount to reflect the new size
  *drawnTilesCount = newCount;
}
