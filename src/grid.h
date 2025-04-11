// grid.h
#ifndef GRID_H
#define GRID_H

// structs
typedef struct WorldCoords {
  int startX;
  int startY;
  int endX;
  int endY;
} WorldCoords;

// includes
#include <raylib.h>

// functions
Vector2 getWorldCoordinates(Vector2 screenPos, Camera2D camera);

WorldCoords getWorldGridCoords(Vector2 startPos, Vector2 endPos,
                               Camera2D camera);

void clampCoordinate(int *coord, int min, int max);

WorldCoords GetVisibleGridBounds(Camera2D camera, int screenWidth,
                                 int screenHeight);

void coordsToArray(WorldCoords coords, int coordArray[][2]);

void coordsToPerimeterArray(WorldCoords coords, int coordArray[][2]);

int getBoundingBoxSize(WorldCoords coords);

int getBoundingPerimeterSize(WorldCoords coords);

#endif // GRID_H
