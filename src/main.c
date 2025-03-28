// main.c

// project headers
#include "command.h"
#include "database.h"
#include "draw.h"
#include "edge.h"
#include "grid.h"
#include "undo.h"
#include "window.h"

// system headers
#include <raylib.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

  // Initialize mouse vectors
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
