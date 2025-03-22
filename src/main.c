#include "database.h"
#include <raylib.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Definitions
#define CAMERA_SPEED 300.0f

// Structs
typedef struct NeighborInfo {
  int tileKey;
  int priority;
} NeighborInfo;

typedef struct EdgeTextureInfo {
  Texture2D texture;
  int priority;
} EdgeTextureInfo;

typedef struct Wall { // wall tiles
  int wallKey;
  int orientation;
  Texture2D tex[4];
} Wall;

typedef struct WorldCoords {
  int startX;
  int startY;
  int endX;
  int endY;
} WorldCoords;

typedef struct CameraState {
  bool isPanning;
  Vector2 lastMousePosition;
} CameraState;

typedef struct WindowState {
  int width;
  int height;
} WindowState;

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

// Variables
char command[256] = {0};
unsigned int commandIndex = 0;
bool inCommandMode = false;
int activeTileKey = 0;
int countEdges;
int maxTileKey;
Map currentMap;
sqlite3 *db;
Camera2D camera = {0};

// Math utils
int abs(int x) { return x < 0 ? -x : x; }

// Grid update functions
void UpdateCameraOffset(Camera2D *camera, int newWidth, int newHeight) {
  camera->offset = (Vector2){newWidth / 2.0f, newHeight / 2.0f};
}

void HandleWindowResize(WindowState *windowState, Camera2D *camera) {
  // Get current window dimensions
  int newWidth = GetScreenWidth();
  int newHeight = GetScreenHeight();

  // Only update if dimensions actually changed
  if (newWidth != windowState->width || newHeight != windowState->height) {
    windowState->width = newWidth;
    windowState->height = newHeight;
    UpdateCameraOffset(camera, newWidth, newHeight);
  }
}

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

// Edge functions
void processCorner(NeighborInfo *edgeNumbers, NeighborInfo *neighbors,
                   bool *visitedTiles, int index, int adjacent1,
                   int adjacent2) {
  int opposite1 = (adjacent1 + 2) % 4; // Calculate opposites
  int opposite2 = (adjacent2 + 2) % 4;

  if (neighbors[adjacent1].tileKey == neighbors[adjacent2].tileKey &&
      neighbors[adjacent1].tileKey != 0 &&
      neighbors[adjacent1].tileKey != neighbors[opposite1].tileKey &&
      neighbors[adjacent1].tileKey != neighbors[opposite2].tileKey) {

    edgeNumbers[index] = neighbors[adjacent1];
    visitedTiles[adjacent1] = visitedTiles[adjacent2] = true;
  }
}

void processDiagonal(NeighborInfo *edgeNumbers, NeighborInfo *neighbors,
                     bool *visitedTiles, int index, int adjacent1,
                     int adjacent2) {
  if (neighbors[index].tileKey != 0 &&
      (!visitedTiles[adjacent1] ||
       neighbors[index].priority > neighbors[adjacent1].priority) &&
      (!visitedTiles[adjacent2] ||
       neighbors[index].priority > neighbors[adjacent2].priority)) {
    edgeNumbers[index] = neighbors[index];
  }
}

Texture2D getEdge(Edge *edgeTypes, int countEdges, int tileKey,
                  int edgeNumber) {
  for (int i = 0; i < countEdges; i++) {
    if (edgeTypes[i].tileKey == tileKey) {
      return edgeTypes[i].edges[edgeNumber];
    }
  }
  Texture2D defaultTexture = {0}; // Default texture initialization
  return defaultTexture;
}

int compareEdgeTextures(const void *a, const void *b) {
  const EdgeTextureInfo *edgeA = (const EdgeTextureInfo *)a;
  const EdgeTextureInfo *edgeB = (const EdgeTextureInfo *)b;
  return edgeA->priority - edgeB->priority; // Ascending order
}

void getEdgeTextures(Map *map, int x, int y, Tile tileTypes[], Edge edgeTypes[],
                     Texture2D resultTextures[], int *textureCount) {

  const int neighborCoords[8][2] = {
      {x, y - 1},     {x + 1, y},     {x, y + 1},     {x - 1, y},    // Cardinal
      {x - 1, y - 1}, {x + 1, y - 1}, {x - 1, y + 1}, {x + 1, y + 1} // Diagonal
  };

  NeighborInfo neighbors[8] = {0};
  NeighborInfo edgeNumbers[12] = {0};
  bool visitedTiles[4] = {false};

  int currentTileKey = map->grid[x][y][0];

  // Populate neighbors
  for (int i = 0; i < 8; i++) {
    int nx = neighborCoords[i][0];
    int ny = neighborCoords[i][1];

    if (nx >= 0 && nx < GRID_SIZE && ny >= 0 && ny < GRID_SIZE) {
      int neighborKey = map->grid[nx][ny][0];
      if (tileTypes[neighborKey].edgeIndicator == 1 &&
          tileTypes[neighborKey].edgePriority >
              tileTypes[currentTileKey].edgePriority) {
        neighbors[i].tileKey = neighborKey;
        neighbors[i].priority = tileTypes[neighborKey].edgePriority;
      }
    }
  }

  // Process corners
  processCorner(edgeNumbers, neighbors, visitedTiles, 8, 0, 3);  // Northwest
  processCorner(edgeNumbers, neighbors, visitedTiles, 9, 0, 1);  // Northeast
  processCorner(edgeNumbers, neighbors, visitedTiles, 10, 2, 3); // Southwest
  processCorner(edgeNumbers, neighbors, visitedTiles, 11, 2, 1); // Southeast

  // Process cardinal edges
  for (int i = 0; i < 4; i++) {
    if (neighbors[i].tileKey != 0 && !visitedTiles[i]) {
      edgeNumbers[i] = neighbors[i];
      visitedTiles[i] = true;
    }
  }

  // Process diagonals
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 4, 0, 3); // Northwest
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 5, 0, 1); // Northeast
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 6, 2, 3); // Southwest
  processDiagonal(edgeNumbers, neighbors, visitedTiles, 7, 2, 1); // Southeast

  // Populate result textures
  EdgeTextureInfo edgeTextureInfoArray[12];
  int actualCount = 0;

  for (int i = 0; i < 12; i++) {
    if (edgeNumbers[i].tileKey != 0) {

      Texture2D edgeTexture =
          getEdge(edgeTypes, countEdges, edgeNumbers[i].tileKey, i);
      edgeTextureInfoArray[actualCount].texture = edgeTexture;
      edgeTextureInfoArray[actualCount].priority = edgeNumbers[i].priority;
      actualCount++;
    }
  }

  // Sort edgeTextureInfoArray by priority in descending order
  qsort(edgeTextureInfoArray, actualCount, sizeof(EdgeTextureInfo),
        compareEdgeTextures);

  // Extract sorted textures back into resultTextures
  for (int i = 0; i < actualCount; i++) {
    resultTextures[i] = edgeTextureInfoArray[i].texture;
  }

  // Update textureCount
  *textureCount = actualCount;
}

void computeEdges(int edgeGrid[][2], int edgeGridCount, Map *map,
                  Tile tileTypes[], Edge edgeTypes[]) {

  for (int i = 0; i < edgeGridCount; i++) {
    int x = edgeGrid[i][0];
    int y = edgeGrid[i][1];

    // Initialize to empty texture
    for (int j = 0; j < 12; j++) {
      map->edges[x][y][j] = (Texture2D){0};
    }

    Texture2D resultTextures[12];
    int textureCount = 0; // Keeps track of populated textures

    // Compute edges for this tile
    getEdgeTextures(map, x, y, tileTypes, edgeTypes, resultTextures,
                    &textureCount);

    // If a valid edge texture is found, store it
    for (int j = 0; j < textureCount; j++) {
      if (resultTextures[j].id != 0) {
        map->edges[x][y][j] = resultTextures[j];
      }
    }
    map->edgeCount[x][y] = textureCount;
  }
}

void computeMapEdges(Tile tileTypes[], Edge edgeTypes[]) {
  // Compute edges
  int edgeGrid[GRID_SIZE * GRID_SIZE][2] = {0};
  int edgeGridCount = 0;
  for (int x = 0; x < GRID_SIZE; x++) {
    for (int y = 0; y < GRID_SIZE; y++) {
      edgeGrid[edgeGridCount][0] = x;
      edgeGrid[edgeGridCount][1] = y;
      edgeGridCount++;
    }
  }
  computeEdges(edgeGrid, edgeGridCount, &currentMap, tileTypes, edgeTypes);
}

bool visitedCheck(int visitedTiles[][2], int visitedCount, int x, int y) {
  if (visitedCount == 0) {
    return false;
  } else {
    for (int i = 0; i < visitedCount; i++) {
      int vx = visitedTiles[i][0];
      int vy = visitedTiles[i][1];

      if (vx == x && vy == y) {
        return true;
      }
    }
    return false;
  }
}

void calculateEdgeGrid(int placedTiles[][3], int placedCount,
                       int visitedTiles[][2], int *visitedCount) {

  for (int i = 0; i < placedCount; i++) {
    int x = placedTiles[i][0];
    int y = placedTiles[i][1];

    int directions[8][2] = {
        {x, y - 1},     {x + 1, y},
        {x, y + 1},     {x - 1, y}, // Cardinal
        {x - 1, y - 1}, {x + 1, y - 1},
        {x - 1, y + 1}, {x + 1, y + 1} // Diagonal
    };

    // populate visited tiles first with placedTiles
    if (!visitedCheck(visitedTiles, *visitedCount, x, y)) {

      visitedTiles[*visitedCount][0] = x;
      visitedTiles[*visitedCount][1] = y;
      (*visitedCount)++;
    }

    for (int j = 0; j < 8; j++) {
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
  batch->visitedTiles = (int(*)[2])malloc(visitedCount * sizeof(int[2]));
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

// Command mode functions
void parseCommand(Tile tileTypes[], Edge edgeTypes[], sqlite3 *db) {
  if (strncmp(command, ":tile ", 6) == 0) {
    char *tileKeyStr = command + 6;
    char *endptr;
    long tileKey = strtol(tileKeyStr, &endptr, 10);
    if (*endptr == '\0') {
      if (tileKey >= 0 && tileKey <= maxTileKey &&
          tileTypes[tileKey].tileKey == tileKey) {
        activeTileKey = (int)tileKey;
        printf("Active tile set to %d\n", activeTileKey);
      } else {
        printf("Tile key out of range\n");
      }
    } else {
      printf("Invalid tile key\n");
    }
  } else if (strncmp(command, ":load ", 6) == 0) {
    char *table = &command[6];
    loadMap(db, table);
    computeMapEdges(tileTypes, edgeTypes);
    printf("Map loaded: %s\n", table);
  } else if (strncmp(command, ":save ", 6) == 0) {
    char *table = &command[6];
    saveMap(db, table);
    printf("Map saved: %s\n", table);
  } else {
    printf("Command not recognized\n");
  }
}

void handleCommandMode(char *command, unsigned int *commandIndex,
                       bool *inCommandMode, int screenHeight, int screenWidth,
                       Tile tileTypes[], Edge edgeTypes[], sqlite3 *db) {

  // Command mode entry
  if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
    if (IsKeyPressed(KEY_SEMICOLON)) {
      printf("Entering command mode\n");
      *inCommandMode = true;
      *commandIndex = 0;
      memset(command, 0, 256);
    }
  }

  if (*inCommandMode) {
    // Handle character input
    int key = GetCharPressed();
    while (key > 0) { // Process all queued characters
      if (key >= 32 && key <= 126 && *commandIndex < 255) {
        command[*commandIndex] = (char)key;
        (*commandIndex)++;
        command[*commandIndex] = '\0';
      }
      key = GetCharPressed(); // Get next character in queue
    }

    // Handle backspace
    if (IsKeyPressed(KEY_BACKSPACE) && *commandIndex > 0) {
      (*commandIndex)--;
      command[*commandIndex] = '\0';
    }

    // Handle command execution or exit
    if (IsKeyPressed(KEY_ENTER)) {
      printf("Command entered: %s\n", command);
      parseCommand(tileTypes, edgeTypes, db);
      *inCommandMode = false;
    } else if (IsKeyPressed(KEY_ESCAPE)) {
      *inCommandMode = false;
    }

    DrawRectangle(0, screenHeight - 30, screenWidth, 30, DARKGRAY);
    DrawText(command, 10, screenHeight - 25, 20, RAYWHITE);
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

// Entry point
int main() {
  loadMap(db, "map");

  // Set window dimensions
  int windowWidth = 800;
  int windowHeight = 600;

  // Initialize window
  InitWindow(windowWidth, windowHeight, "Map Editor");
  SetTargetFPS(60);
  SetExitKey(KEY_NULL);

  // Load textures
  Tile *tileTypes = loadTiles(db);
  Edge *edgeTypes = loadEdges(db);
  computeMapEdges(tileTypes, edgeTypes);

  // Initialize Undo/Redo manager
  UndoRedoManager *manager = (UndoRedoManager *)malloc(sizeof(UndoRedoManager));
  manager->head = NULL;
  manager->current = NULL;

  // Window state
  SetWindowState(FLAG_WINDOW_RESIZABLE); // Enable window resizing
  WindowState windowState;
  windowState.width = windowWidth;
  windowState.height = windowHeight;

  // Initialize camera
  camera.zoom = 1.0f;
  camera.offset =
      (Vector2){windowState.width / 2.0f, windowState.height / 2.0f};
  camera.target =
      (Vector2){GRID_SIZE / 2 * TILE_SIZE, GRID_SIZE / 2 * TILE_SIZE};
  camera.rotation = 0.0f;

  // Initialize camera state
  CameraState cameraState;
  cameraState.isPanning = false;
  cameraState.lastMousePosition = (Vector2){0, 0};

  // Drawing state
  bool isShiftDrawing = false; // Box mode
  bool isCtrlDrawing = false;  // Pathing mode
  bool isDrawing = false;      // Painter mode

  int drawnTilesCount = 0;
  int drawnTiles[GRID_SIZE * GRID_SIZE][3]; // 0=x; 1=y; 2=style
  Vector2 startPos = {0};
  Vector2 mousePos;

  // Event loop
  while (!WindowShouldClose()) {
    // Handle window resizing
    HandleWindowResize(&windowState, &camera);

    // Camera movement
    float deltaTime = GetFrameTime();
    if (IsKeyDown(KEY_RIGHT))
      camera.target.x += CAMERA_SPEED * deltaTime;
    if (IsKeyDown(KEY_LEFT))
      camera.target.x -= CAMERA_SPEED * deltaTime;
    if (IsKeyDown(KEY_DOWN))
      camera.target.y += CAMERA_SPEED * deltaTime;
    if (IsKeyDown(KEY_UP))
      camera.target.y -= CAMERA_SPEED * deltaTime;

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
      cameraState.isPanning = true;
      cameraState.lastMousePosition = mousePos;
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
      cameraState.isPanning = false;
    }

    // Update camera position while panning
    if (cameraState.isPanning) {
      Vector2 mouseDelta = {mousePos.x - cameraState.lastMousePosition.x,
                            mousePos.y - cameraState.lastMousePosition.y};

      // Move camera opposite to mouse movement, adjusted for zoom
      camera.target.x -= mouseDelta.x / camera.zoom;
      camera.target.y -= mouseDelta.y / camera.zoom;

      cameraState.lastMousePosition = mousePos;
    }

    // Get mouse position in world coordinates
    mousePos = GetMousePosition();

    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
      // Get mouse position before zoom for zoom targeting
      Vector2 mouseWorldPos = getWorldCoordinates(GetMousePosition(), camera);

      // Apply zoom
      camera.zoom += wheel * 0.1f;
      if (camera.zoom < 0.2f)
        camera.zoom = 0.1f;

      // Adjust camera target to zoom towards mouse position
      Vector2 newMouseWorldPos =
          getWorldCoordinates(GetMousePosition(), camera);
      camera.target.x += mouseWorldPos.x - newMouseWorldPos.x;
      camera.target.y += mouseWorldPos.y - newMouseWorldPos.y;
    }

    // Check for Ctrl-Z (Undo)
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
      if (IsKeyPressed(KEY_Z)) {
        undo(manager, &currentMap, tileTypes, edgeTypes);
      }

      // Check for Ctrl-Y (Redo)
      if (IsKeyPressed(KEY_Y)) {
        redo(manager, &currentMap, tileTypes, edgeTypes);
      }
    }

    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode2D(camera);

    // Draw existing map
    drawExistingMap(&currentMap, tileTypes, camera, windowState.width,
                    windowState.height);

    // Handle mouse input and drawing
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && IsKeyDown(KEY_LEFT_SHIFT)) {
      // Drawing mode: box
      drawnTilesCount = 0;
      startPos = mousePos;
      isShiftDrawing = true;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
               IsKeyDown(KEY_LEFT_CONTROL)) {
      // Drawing mode: pathing
      drawnTilesCount = 0;
      startPos = mousePos;
      isCtrlDrawing = true;
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
               !IsKeyDown(KEY_LEFT_SHIFT && !IsKeyDown(KEY_LEFT_CONTROL))) {
      // Drawing mode: painter
      drawnTilesCount = 0;
      startPos = mousePos;
      isDrawing = true;
    }

    if (isShiftDrawing) {
      WorldCoords coords = getWorldGridCoords(startPos, mousePos, camera);

      // Calculate the size of the array based on the bounding box
      int coordArraySize = getBoundingBoxSize(coords);
      int coordArray[coordArraySize][2];
      coordsToArray(coords, coordArray);

      updateDrawnTiles(coordArray, coordArraySize, drawnTiles, &drawnTilesCount,
                       activeTileKey, tileTypes);
      drawPreview(&currentMap, drawnTiles, drawnTilesCount, tileTypes,
                  edgeTypes, activeTileKey);
    } else if (isCtrlDrawing) {
      WorldCoords coords = getWorldGridCoords(startPos, mousePos, camera);

      /* int euclideanDistance = (abs(coords.endX - coords.startX) +
       * (abs(coords.endY - coords.startY))); */
      /* int coordArraySize = (euclideanDistance == 0) ? 1 :
       * euclideanDistance + 1; */
      int coordArraySize = (abs(coords.endX - coords.startX) +
                            (abs(coords.endY - coords.startY)) + 1);
      int coordArray[coordArraySize][2];

      calculatePath(coords, coordArray);
      updateDrawnTiles(coordArray, coordArraySize, drawnTiles, &drawnTilesCount,
                       activeTileKey, tileTypes);
      drawPreview(&currentMap, drawnTiles, drawnTilesCount, tileTypes,
                  edgeTypes, activeTileKey);

    } else if (isDrawing) {

      WorldCoords coords = getWorldGridCoords(mousePos, mousePos, camera);
      int x = coords.endX;
      int y = coords.endY;

      int alreadyVisited = 0;
      for (int j = 0; j < drawnTilesCount; j++) {
        if (drawnTiles[j][0] == x && drawnTiles[j][1] == y) {
          alreadyVisited = 1;
          break;
        }
      }

      if (!alreadyVisited) {
        int style = getRandTileStyle(activeTileKey, tileTypes);
        drawnTiles[drawnTilesCount][0] = x;
        drawnTiles[drawnTilesCount][1] = y;
        drawnTiles[drawnTilesCount][2] = style;
        drawnTilesCount++;
      }

      drawPreview(&currentMap, drawnTiles, drawnTilesCount, tileTypes,
                  edgeTypes, activeTileKey);

    } else {
      // Draw cursor preview
      WorldCoords coords = getWorldGridCoords(mousePos, mousePos, camera);
      Texture2D tileTexture = tileTypes[activeTileKey].tex[0];
      Vector2 tilePos = {coords.startX * TILE_SIZE, coords.startY * TILE_SIZE};
      DrawTexture(tileTexture, tilePos.x, tilePos.y, WHITE);
      DrawRectangleLines(tilePos.x, tilePos.y, TILE_SIZE, TILE_SIZE, RED);
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {

      // Get neighbors to placement
      int visitedTiles[GRID_SIZE * GRID_SIZE][2];
      int visitedCount = 0;
      calculateEdgeGrid(drawnTiles, drawnTilesCount, visitedTiles,
                        &visitedCount);

      // Add drawn tiles to undo/redo stack
      createTileChangeBatch(manager, &currentMap, drawnTiles, drawnTilesCount,
                            activeTileKey, visitedTiles, visitedCount);

      // Texture updates
      applyTiles(&currentMap, drawnTiles, drawnTilesCount, activeTileKey);
      computeEdges(visitedTiles, visitedCount, &currentMap, tileTypes,
                   edgeTypes);

      memset(drawnTiles, 0, sizeof(drawnTiles));

      isShiftDrawing = false;
      isCtrlDrawing = false;
      isDrawing = false;
    }

    EndMode2D();

    // Handle command mode
    handleCommandMode(command, &commandIndex, &inCommandMode,
                      windowState.height, windowState.width, tileTypes,
                      edgeTypes, db);

    EndDrawing();
  }

  // free Undo/Redo manager and all batches/changes from session
  TileChangeBatch *batch = manager->head;
  while (batch) {
    TileChangeBatch *nextBatch = batch->next;
    free(batch->changes);
    free(batch);
    batch = nextBatch;
  }
  free(manager);

  free(tileTypes);
  free(edgeTypes);
  CloseWindow();
  return 0;
}
