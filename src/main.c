// main.c

// project headers
#include "command.h"
#include "database.h"
#include "draw.h"
#include "edge.h"
#include "grid.h"
#include "undo.h"
#include "wall.h"
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
int countEdges;
int maxTileKey;
Map currentMap = {0};
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
  Wall *wallTypes = loadWalls(db);
  computeMapEdges(tileTypes, edgeTypes);
  computeMapWalls(wallTypes);

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

  // Initialize drawing state
  drawingState drawState = {0};
  drawState.drawType = DRAW_TILE; // Start in tile drawing mode
  drawState.mode = MODE_PAINTER;  // Default mode is painter
  drawState.activeTileKey = 0;    // Initialize to a default tile
  drawState.activeWallKey = 0;    // Initialize to a default wall
  drawState.isDrawing = false;
  drawState.drawnTilesCount = 0;

  // Initialize command state
  CommandState commandState = {0};
  commandState.commandIndex = 0;
  commandState.inCommandMode = false;

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
      cameraState.lastMousePosition = drawState.mousePos;
    } else if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
      cameraState.isPanning = false;
    }

    // Update camera position while panning
    if (cameraState.isPanning) {
      Vector2 mouseDelta = {
          drawState.mousePos.x - cameraState.lastMousePosition.x,
          drawState.mousePos.y - cameraState.lastMousePosition.y};

      // Move camera opposite to mouse movement, adjusted for zoom
      camera.target.x -= mouseDelta.x / camera.zoom;
      camera.target.y -= mouseDelta.y / camera.zoom;

      cameraState.lastMousePosition = drawState.mousePos;
    }

    // Get mouse position in world coordinates
    Vector2 screenMousePos = GetMousePosition();
    drawState.mousePos = screenMousePos;

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
    if (!drawState.isDrawing) {
      drawExistingMap(&currentMap, tileTypes, wallTypes, camera,
                      windowState.width, windowState.height);
    }

    // Check for starting a drawing action
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      // Decide the drawing mode based on modifier keys
      if (IsKeyDown(KEY_LEFT_SHIFT)) {
        drawState.mode = MODE_BOX;
      } else if (IsKeyDown(KEY_LEFT_CONTROL)) {
        drawState.mode = MODE_PATHING;
      } else {
        drawState.mode = MODE_PAINTER;
      }
      // Store the position where drawing started
      drawState.startPos = drawState.mousePos;
      drawState.isDrawing = true;
      drawState.drawnTilesCount = 0; // Clear previous preview data
    }

    if (drawState.isDrawing) {
      // Box (Shift) mode drawing
      if (drawState.mode == MODE_BOX) {
        WorldCoords coords =
            getWorldGridCoords(drawState.startPos, drawState.mousePos, camera);

        // Calculate the size of the array based on the bounding box
        int coordArraySize = getBoundingBoxSize(coords);
        int coordArray[coordArraySize][2];
        coordsToArray(coords, coordArray);

        updateDrawnTiles(coordArray, coordArraySize, drawState.drawnTiles,
                         &drawState.drawnTilesCount, drawState.activeTileKey,
                         tileTypes);
        drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                    windowState, camera);

        // Pathing (Ctrl) mode drawing
      } else if (drawState.mode == MODE_PATHING) {
        WorldCoords coords =
            getWorldGridCoords(drawState.startPos, drawState.mousePos, camera);

        int coordArraySize = (abs(coords.endX - coords.startX) +
                              (abs(coords.endY - coords.startY)) + 1);
        int coordArray[coordArraySize][2];

        calculatePath(coords, coordArray);
        updateDrawnTiles(coordArray, coordArraySize, drawState.drawnTiles,
                         &drawState.drawnTilesCount, drawState.activeTileKey,
                         tileTypes);
        drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                    windowState, camera);

        // Painter mode drawing
      } else if (drawState.mode == MODE_PAINTER) {

        WorldCoords coords =
            getWorldGridCoords(drawState.mousePos, drawState.mousePos, camera);
        int x = coords.endX;
        int y = coords.endY;

        int alreadyVisited = 0;
        for (int j = 0; j < drawState.drawnTilesCount; j++) {
          if (drawState.drawnTiles[j][0] == x &&
              drawState.drawnTiles[j][1] == y) {
            alreadyVisited = 1;
            break;
          }
        }

        if (!alreadyVisited) {
          int style = getRandTileStyle(drawState.activeTileKey, tileTypes);
          drawState.drawnTiles[drawState.drawnTilesCount][0] = x;
          drawState.drawnTiles[drawState.drawnTilesCount][1] = y;
          drawState.drawnTiles[drawState.drawnTilesCount][2] = style;
          drawState.drawnTilesCount++;
        }

        drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                    windowState, camera);
      }
    } else {

      // Fallback: Draw a simple cursor preview if no drawing mode is active
      WorldCoords coords =
          getWorldGridCoords(drawState.mousePos, drawState.mousePos, camera);
      Texture2D tileTexture = tileTypes[drawState.activeTileKey].tex[0];
      Vector2 tilePos = {coords.startX * TILE_SIZE, coords.startY * TILE_SIZE};
      DrawTexture(tileTexture, tilePos.x, tilePos.y, WHITE);
      DrawRectangleLines(tilePos.x, tilePos.y, TILE_SIZE, TILE_SIZE, RED);
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {

      // Get neighbors to placement
      int visitedTiles[GRID_SIZE * GRID_SIZE][2];
      int visitedCount = 0;
      calculateEdgeGrid(drawState.drawnTiles, drawState.drawnTilesCount,
                        visitedTiles, &visitedCount);

      // Add drawn tiles to undo/redo stack
      createTileChangeBatch(manager, &currentMap, drawState.drawnTiles,
                            drawState.drawnTilesCount, drawState.activeTileKey,
                            visitedTiles, visitedCount);

      // Texture updates
      applyTiles(&currentMap, drawState.drawnTiles, drawState.drawnTilesCount,
                 drawState.activeTileKey);
      computeEdges(visitedTiles, visitedCount, &currentMap, tileTypes,
                   edgeTypes);

      memset(drawState.drawnTiles, 0, sizeof(drawState.drawnTiles));

      drawState.isDrawing = false;
    }

    EndMode2D();

    // Handle command mode
    handleCommandMode(&commandState, windowState.height, windowState.width,
                      tileTypes, edgeTypes, db, &drawState);

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
