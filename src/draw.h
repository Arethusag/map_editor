// draw.h
#ifndef DRAW_H
#define DRAW_H

// Headers
#include "database.h"
#include "grid.h"
#include "window.h"
#include <raylib.h>

// Structures
typedef enum {
  DRAW_TILE, // drawing tiles
  DRAW_WALL  // drawing walls
} drawType;

typedef enum {
  MODE_PAINTER, // single tile painting
  MODE_PATHING, // path drawing mode
  MODE_BOX      // box (or area) drawing mode
} drawMode;

// The drawing state structure that groups all drawing variables
typedef struct {
  drawType drawType; // tile or wall drawing
  drawMode mode;     // painter, pathing, or box mode
  int activeTileKey; // currently selected tile key (for tile drawing)
  int activeWallKey; // currently selected wall key (for wall drawing)
  Vector2 startPos;  // starting mouse position for drawing (in world coords)
  Vector2 mousePos;  // current mouse position (in world coords)
  bool isDrawing;    // flag to signal an active drawing operation

  // Include a preview buffer for drawn tiles (or walls)
  // Using a 2D array where each entry holds {x, y, style}
  int drawnTiles[GRID_SIZE * GRID_SIZE][3];
  int drawnTilesCount;
} drawingState;

// functions
int abs(int x);

int getRandTileStyle(int tileKey, Tile *tileTypes);

void calculatePath(WorldCoords coords, int path[][2]);

void drawPreview(Map *currentMap, drawingState *drawState, Tile tileTypes[],
                 Edge edgeTypes[], Wall wallTypes[], WindowState windowState,
                 Camera2D camera);

void applyTiles(Map *map, int placedTiles[][3], int placedCount,
                int activeTileKey);

void drawExistingMap(Map *map, Tile tileTypes[], Wall wallTypes[],
                     Camera2D camera, int screenWidth, int screenHeight);

void updateDrawnTiles(int coordArray[][2], int coordArraySize,
                      int drawnTiles[][3], int *drawnTilesCount,
                      int activeTileKey, Tile *tileTypes);

#endif // DRAW_H
