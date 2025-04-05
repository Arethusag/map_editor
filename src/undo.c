#include "undo.h"
#include "database.h"
#include "edge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Undo/Redo functions
void createTileChangeBatch(UndoRedoManager *manager, Map *map,
                           int drawnTiles[][3], int drawnTilesCount,
                           int activeTileKey, int visitedTiles[][2],
                           int visitedCount) {
  printf("Creating tile change batch with %d tiles.\n", drawnTilesCount);

  TileChange *changes =
      (TileChange *)malloc(drawnTilesCount * sizeof(TileChange));

  for (int i = 0; i < drawnTilesCount; i++) {
    int x = drawnTiles[i][0];
    int y = drawnTiles[i][1];
    int newStyle = drawnTiles[i][2];
    int newKey = activeTileKey;

    // Get old tile information from the map
    int oldKey = map->grid[x][y][0];
    int oldStyle = map->grid[x][y][1];

    // Populate the TileChange structure
    changes[i].x = x;
    changes[i].y = y;
    changes[i].oldKey = oldKey;
    changes[i].oldStyle = oldStyle;
    changes[i].newKey = newKey;
    changes[i].newStyle = newStyle;

    printf("TileChange %d: [%d, %d] OldKey=%d OldStyle=%d NewKey=%d "
           "NewStyle=%d\n",
           i, x, y, oldKey, oldStyle, newKey, newStyle);
  }

  TileChangeBatch *batch = (TileChangeBatch *)malloc(sizeof(TileChangeBatch));

  batch->changes = changes; // Point to the changes array
  batch->changeCount = drawnTilesCount;
  batch->visitedTiles = (int (*)[2])malloc(visitedCount * sizeof(int[2]));
  memcpy(batch->visitedTiles, visitedTiles, visitedCount * sizeof(int[2]));
  batch->visitedCount = visitedCount;
  batch->next = NULL;
  batch->prev = NULL;

  printf("Stored %d visited tiles.\n", visitedCount);

  printf("TileChangeBatch created. changeCount=%d\n", drawnTilesCount);

  // If we're in the middle of the stack, truncate the "dead branches"
  if (manager->current && manager->current->next) {
    printf("Truncating redo stack.\n");
    TileChangeBatch *toDelete = manager->current->next;
    while (toDelete) {
      printf("Deleting TileChangeBatch at %p\n", (void *)toDelete);
      TileChangeBatch *next = toDelete->next;
      free(toDelete->changes);
      free(toDelete);
      toDelete = next;
    }
    manager->current->next = NULL;
  }

  // Add the new batch to the list
  if (manager->current) {
    printf("Appending batch to the existing list.\n");
    manager->current->next = batch;
    batch->prev = manager->current;
  } else {
    printf("Starting a new list with this batch.\n");
    manager->head = batch;
  }
  manager->current = batch;

  printf("Batch added. Current batch is at %p\n", (void *)manager->current);
}

void undo(UndoRedoManager *manager, Map *map, Tile *tileTypes,
          Edge *edgeTypes) {
  if (manager->current) {
    TileChangeBatch *batch = manager->current;
    printf("Undoing batch at %p with %d changes.\n", (void *)batch,
           batch->changeCount);

    for (int i = 0; i < batch->changeCount; i++) {
      TileChange *change = &batch->changes[i];
      printf("Undoing change %d: [%d, %d] Key=%d Style=%d -> Key=%d "
             "Style=%d\n",
             i, change->x, change->y, change->newKey, change->newStyle,
             change->oldKey, change->oldStyle);

      // Revert to old tile information
      map->grid[change->x][change->y][0] = change->oldKey;
      map->grid[change->x][change->y][1] = change->oldStyle;
    }

    computeEdges(batch->visitedTiles, batch->visitedCount, map, tileTypes,
                 edgeTypes);
    manager->current = manager->current->prev;
    if (manager->current) {
      printf("Moved to previous batch at %p.\n", (void *)manager->current);
    } else {
      printf("No previous batch. Reached the beginning of the stack.\n");
    }
  } else {
    printf("Nothing to undo. Current is NULL.\n");
  }
}

void redo(UndoRedoManager *manager, Map *map, Tile *tileTypes,
          Edge *edgeTypes) {
  TileChangeBatch *batch;

  if (manager->current && manager->current->next) {
    printf("Redoing next batch. Current batch: %p, Next batch: %p.\n",
           (void *)manager->current, (void *)manager->current->next);

    manager->current = manager->current->next;
    batch = manager->current;
  } else if (!manager->current && manager->head) {
    printf("Redoing from the beginning. Starting with head batch at %p.\n",
           (void *)manager->head);

    manager->current = manager->head;
    batch = manager->current;
  } else {
    printf("Nothing to redo.\n");
    return;
  }

  printf("Redoing batch at %p with %d changes.\n", (void *)batch,
         batch->changeCount);

  for (int i = 0; i < batch->changeCount; i++) {
    TileChange *change = &batch->changes[i];
    printf("Redoing change %d: [%d, %d] Key=%d Style=%d -> Key=%d Style=%d\n",
           i, change->x, change->y, change->oldKey, change->oldStyle,
           change->newKey, change->newStyle);

    // Apply new tile information
    map->grid[change->x][change->y][0] = change->newKey;
    map->grid[change->x][change->y][1] = change->newStyle;
  }

  if (manager->current->next) {
    printf("Moved to next batch at %p.\n", (void *)manager->current);
  } else {
    printf("No next batch. Reached the end of the stack.\n");
  }
  computeEdges(batch->visitedTiles, batch->visitedCount, map, tileTypes,
               edgeTypes);
}
