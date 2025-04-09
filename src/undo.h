// undo.h
#ifndef UNDO_H
#define UNDO_H

// includes
#include "database.h"
#include "draw.h"
#include "edge.h"

// structs
typedef struct TileChange {
  int x, y;             // Tile position
  int oldKey, oldStyle; // Old tile data
  int newKey, newStyle; // New tile data
  DrawType drawType;
} TileChange;

typedef struct TileChangeBatch {
  TileChange *changes;          // Array of changes
  int changeCount;              // Number of changes in the batch
  struct TileChangeBatch *next; // Pointer to the next batch
  struct TileChangeBatch *prev; // Pointer to the previous batch
  int (*visitedTiles)[2];
  int visitedCount;
} TileChangeBatch;

typedef struct UndoRedoManager {
  TileChangeBatch *head;    // Head of the stack
  TileChangeBatch *current; // Current batch (pointer for undo/redo)
} UndoRedoManager;

// functions
void createTileChangeBatch(UndoRedoManager *manager, Map *map,
                           DrawingState *drawState, int visitedTiles[][2],
                           int visitedCount);

void undo(UndoRedoManager *manager, Map *map, Tile *tileTypes, Edge *edgeTypes,
          Wall *wallTypes);

void redo(UndoRedoManager *manager, Map *map, Tile *tileTypes, Edge *edgeTypes,
          Wall *wallTypes);

#endif // UNDO_H
