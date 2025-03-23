// grid.c
#include "grid.h"
#include "database.h"
#include <raylib.h>

void clampCoordinate(int *coord, int min, int max) {
  if (*coord > max)
    *coord = max;
  else if (*coord < min)
    *coord = 0;
  else
    return;
}

Vector2 getWorldCoordinates(Vector2 screenPos, Camera2D camera) {
  Vector2 worldPos;
  worldPos.x = (screenPos.x - camera.offset.x) / camera.zoom + camera.target.x;
  worldPos.y = (screenPos.y - camera.offset.y) / camera.zoom + camera.target.y;
  return worldPos;
}

WorldCoords GetVisibleGridBounds(Camera2D camera, int screenWidth,
                                 int screenHeight) {
  WorldCoords bounds;

  // Get the bounds of the visible area in world coordinates
  Vector2 startPos = {0, 0};
  Vector2 endPos = {screenWidth, screenHeight};
  Vector2 topLeft = getWorldCoordinates(startPos, camera);
  Vector2 bottomRight = getWorldCoordinates(endPos, camera);

  // Convert to grid coordinates and add a 1-tile buffer for smooth scrolling
  bounds.startX = (int)(topLeft.x / TILE_SIZE) - 1;
  bounds.startY = (int)(topLeft.y / TILE_SIZE) - 1;
  bounds.endX = (int)(bottomRight.x / TILE_SIZE) + 1;
  bounds.endY = (int)(bottomRight.y / TILE_SIZE) + 1;

  // Clamp to grid boundaries
  clampCoordinate(&bounds.startX, 0, GRID_SIZE - 1);
  clampCoordinate(&bounds.startY, 0, GRID_SIZE - 1);
  clampCoordinate(&bounds.endX, 0, GRID_SIZE - 1);
  clampCoordinate(&bounds.endY, 0, GRID_SIZE - 1);

  return bounds;
}

WorldCoords getWorldGridCoords(Vector2 startPos, Vector2 endPos,
                               Camera2D camera) {
  WorldCoords coords;

  // Convert start and end positions to world coordinates
  Vector2 worldStartPos = getWorldCoordinates(startPos, camera);
  Vector2 worldEndPos = getWorldCoordinates(endPos, camera);

  // Convert to grid coordinates
  coords.startX = (int)(worldStartPos.x / TILE_SIZE);
  coords.startY = (int)(worldStartPos.y / TILE_SIZE);
  coords.endX = (int)(worldEndPos.x / TILE_SIZE);
  coords.endY = (int)(worldEndPos.y / TILE_SIZE);

  // Clamp coordinates to grid bounds
  clampCoordinate(&coords.startX, 0, GRID_SIZE - 1);
  clampCoordinate(&coords.startY, 0, GRID_SIZE - 1);
  clampCoordinate(&coords.endX, 0, GRID_SIZE - 1);
  clampCoordinate(&coords.endY, 0, GRID_SIZE - 1);

  return coords;
}

void coordsToArray(WorldCoords coords, int coordArray[][2]) {
  int minX = (coords.startX <= coords.endX) ? coords.startX : coords.endX;
  int maxX = (coords.startX <= coords.endX) ? coords.endX : coords.startX;
  int minY = (coords.startY <= coords.endY) ? coords.startY : coords.endY;
  int maxY = (coords.startY <= coords.endY) ? coords.endY : coords.startY;

  int coordArrayCount = 0;
  for (int x = minX; x <= maxX; x++) {
    for (int y = minY; y <= maxY; y++) {
      coordArray[coordArrayCount][0] = x;
      coordArray[coordArrayCount][1] = y;
      coordArrayCount++;
    }
  }
}

int getBoundingBoxSize(WorldCoords coords) {
  int sizeX = (coords.startX <= coords.endX ? coords.endX - coords.startX
                                            : coords.startX - coords.endX) +
              1;
  int sizeY = (coords.startY <= coords.endY ? coords.endY - coords.startY
                                            : coords.startY - coords.endY) +
              1;
  return sizeX * sizeY;
}
