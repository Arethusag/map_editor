#include "undo.h"
#include "database.h"
#include "draw.h"
#include "edge.h"
#include "wall.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Undo/Redo functions
void createTileChangeBatch(UndoRedoManager *manager, Map *map,
                           DrawingState *drawState, int visitedTiles[][2],
                           int visitedCount) {
  printf("Creating tile change batch with %d tiles.\n",
         drawState->drawnTilesCount);

  TileChange *changes =
      (TileChange *)malloc(drawState->drawnTilesCount * sizeof(TileChange));

  for (int i = 0; i < drawState->drawnTilesCount; i++) {
    int x = drawState->drawnTiles[i][0];
    int y = drawState->drawnTiles[i][1];
    changes[i].x = x;
    changes[i].y = y;
    changes[i].drawType = drawState->drawType;

    switch (drawState->drawType) {
    case DRAW_TILE:
      changes[i].oldKey = map->grid[x][y][0];
      changes[i].oldStyle = map->grid[x][y][1];
      changes[i].newKey = drawState->activeTileKey;
      changes[i].newStyle = drawState->drawnTiles[i][2];
      break;
    case DRAW_WALL:
      changes[i].oldKey = map->grid[x][y][2];
      changes[i].newKey = drawState->activeWallKey;
      break;
    }
    printf("Creating change %d: [%d, %d] Key=%d -> Key=%d with Type=%d\n", i,
           changes[i].x, changes[i].y, changes[i].oldKey, changes[i].newKey,
           (int)changes[i].drawType);
  }

  TileChangeBatch *batch = (TileChangeBatch *)malloc(sizeof(TileChangeBatch));

  batch->changes = changes; // Point to the changes array
  batch->changeCount = drawState->drawnTilesCount;
  batch->visitedTiles = (int (*)[2])malloc(visitedCount * sizeof(int[2]));
  memcpy(batch->visitedTiles, visitedTiles, visitedCount * sizeof(int[2]));
  batch->visitedCount = visitedCount;
  batch->next = NULL;
  batch->prev = NULL;

  printf("Stored %d visited tiles.\n", visitedCount);

  printf("TileChangeBatch created. changeCount=%d\n",
         drawState->drawnTilesCount);

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

void undo(UndoRedoManager *manager, Map *map, Tile *tileTypes, Edge *edgeTypes,
          Wall *wallTypes) {
  if (manager->current) {
    TileChangeBatch *batch = manager->current;
    printf("Undoing batch at %p with %d changes.\n", (void *)batch,
           batch->changeCount);

    for (int i = 0; i < batch->changeCount; i++) {
      TileChange *change = &batch->changes[i];

      // Revert to old tile information
      printf("Undoing change %d: [%d, %d] Key=%d -> Key=%d with Type=%d\n", i,
             change->x, change->y, change->newKey, change->oldKey,
             (int)change->drawType);
      switch (change->drawType) {
      case DRAW_TILE:
        map->grid[change->x][change->y][0] = change->oldKey;
        map->grid[change->x][change->y][1] = change->oldStyle;
        computeEdges(batch->visitedTiles, batch->visitedCount, map, tileTypes,
                     edgeTypes);
        break;
      case DRAW_WALL:
        map->grid[change->x][change->y][2] = change->oldKey;
        computeWalls(batch->visitedTiles, batch->visitedCount, map, wallTypes);
        break;
      }
    }

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

void redo(UndoRedoManager *manager, Map *map, Tile *tileTypes, Edge *edgeTypes,
          Wall *wallTypes) {
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
    printf("Redoing change %d: [%d, %d] Key=%d -> Key=%d with Type=%d\n", i,
           change->x, change->y, change->oldKey, change->newKey,
           (int)change->drawType);

    // Apply new tile information

    switch (change->drawType) {
    case DRAW_TILE:
      map->grid[change->x][change->y][0] = change->newKey;
      map->grid[change->x][change->y][1] = change->newStyle;
      computeEdges(batch->visitedTiles, batch->visitedCount, map, tileTypes,
                   edgeTypes);
      break;
    case DRAW_WALL:
      map->grid[change->x][change->y][2] = change->newKey;
      computeWalls(batch->visitedTiles, batch->visitedCount, map, wallTypes);
      break;
    }
  }

  if (manager->current->next) {
    printf("Moved to next batch at %p.\n", (void *)manager->current);
  } else {
    printf("No next batch. Reached the end of the stack.\n");
  }
}
