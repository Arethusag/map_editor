// draw.h
#ifndef DRAW_H
#define DRAW_H

// Headers
#include "database.h"
#include "grid.h"
#include "window.h"
#include <raylib.h>

// Structures
typedef struct {
  int arrayLength;
  int (*array)[2]; // Pointer to array[2] (e.g., coordinates)
} Array2DPtr;

typedef struct {
  int arrayLength;
  int (*array)[3]; // Pointer to array[3] (e.g., x, y, style)
} Array3DPtr;

typedef enum {
  DRAW_TILE, // drawing tiles
  DRAW_WALL  // drawing walls
} DrawType;

typedef enum {
  MODE_PAINTER, // single tile painting
  MODE_PATHING, // path drawing mode
  MODE_BOX      // box (or area) drawing mode
} DrawMode;

// The drawing state structure that groups all drawing variables
typedef struct {
  DrawType drawType; // tile or wall drawing
  DrawMode mode;     // painter, pathing, or box mode
  int activeTileKey; // currently selected tile key (for tile drawing)
  int activeWallKey; // currently selected wall key (for wall drawing)
  Vector2 startPos;  // starting mouse position for drawing (in world coords)
  Vector2 mousePos;  // current mouse position (in world coords)
  bool isDrawing;    // flag to signal an active drawing operation

  // Include a preview buffer for drawn tiles (or walls)
  // Using a 2D array where each entry holds {x, y, style}
  int drawnTiles[GRID_SIZE * GRID_SIZE][3];
  int drawnTilesCount;
} DrawingState;

// functions
int abs(int x);

int getRandTileStyle(int tileKey, Tile *tileTypes);

void calculatePath(WorldCoords coords, Array2DPtr pathData);

void drawPreview(Map *currentMap, DrawingState *drawState, Tile tileTypes[],
                 Edge edgeTypes[], Wall wallTypes[], WindowState windowState,
                 Camera2D camera);

void applyTiles(Map *map, DrawingState *drawState);

void drawExistingMap(Map *map, Tile tileTypes[], Wall wallTypes[],
                     Camera2D camera, int screenWidth, int screenHeight);

void updateDrawnTiles(Array2DPtr coordArrayData, DrawingState *drawState,
                      Tile *tileTypes);

#endif // DRAW_H
