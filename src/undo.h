// undo.h
#ifndef UNDO_H
#define UNDO_H

// includes
#include "database.h"
#include "edge.h"

// structs
typedef struct TileChange {
  int x, y;             // Tile position
  int oldKey, oldStyle; // Old tile data
  int newKey, newStyle; // New tile data
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
                           int drawnTiles[][3], int drawnTilesCount,
                           int activeTileKey, int visitedTiles[][2],
                           int visitedCount);
void undo(UndoRedoManager *manager, Map *map, Tile *tileTypes, Edge *edgeTypes);
void redo(UndoRedoManager *manager, Map *map, Tile *tileTypes, Edge *edgeTypes);

#endif // UNDO_H
