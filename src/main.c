// main.c
#include "command.h"
#include "database.h"
#include "draw.h"
#include "edge.h"
#include "grid.h"
#include "math.h"
#include "undo.h"
#include "wall.h"
#include "window.h"
#include <raylib.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Entry point
int main() {

  // Initialize database
  sqlite3 *db = connectDatabase();

  // Initialize map
  Map currentMap = {0};
  loadMap(db, "map", &currentMap);

  // Set window dimensions
  int windowWidth = 800;
  int windowHeight = 600;

  // Initialize window
  InitWindow(windowWidth, windowHeight, "Map Editor");
  SetTargetFPS(60);
  SetExitKey(KEY_NULL);

  // Load textures
  Tile *tileTypes = loadTiles(db, &currentMap);
  Edge *edgeTypes = loadEdges(db, &currentMap);
  Wall *wallTypes = loadWalls(db, &currentMap);
  computeMapEdges(tileTypes, edgeTypes, &currentMap);
  computeMapWalls(wallTypes, &currentMap);

  // Load Hash Tables
  WallOrientMap *wallOrientationMap = loadWallOrientationsMap(db);

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
  Camera2D camera = {0};
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
  DrawingState drawState = {0};
  drawState.drawType = DRAW_TILE;             // Start in tile drawing mode
  drawState.drawMode = MODE_PAINTER;          // Default mode is painter
  drawState.pathMode = PATH_DIAGONAL;         // Default path is diagonal
  drawState.previousPathMode = PATH_DIAGONAL; // Default path is diagonal
  drawState.activeTileKey = 0;                // Initialize to a default tile
  drawState.activeWallKey = 0;                // Initialize to a default wall
  drawState.isDrawing = false;
  drawState.drawnTilesCount = 0;
  drawState.initialDragDirection = (Vector2){0, 0}; // Initialize drag direction
  drawState.hasCapturedDragDirection = false;       // Initialize capture flag

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
        undo(manager, &currentMap, tileTypes, edgeTypes, wallTypes);
      }

      // Check for Ctrl-Y (Redo)
      if (IsKeyPressed(KEY_Y)) {
        redo(manager, &currentMap, tileTypes, edgeTypes, wallTypes);
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
        drawState.drawMode = MODE_BOX;
      } else if (IsKeyDown(KEY_LEFT_CONTROL)) {
        drawState.drawMode = MODE_PATHING;
      } else {
        drawState.drawMode = MODE_PAINTER;
      }
      // Store the position where drawing started
      drawState.startPos = drawState.mousePos;
      drawState.isDrawing = true;
      drawState.drawnTilesCount = 0; // Clear previous preview data
    }

    if (drawState.isDrawing) {

      // Box (Shift) mode drawing
      switch (drawState.drawMode) {
      case MODE_BOX: {
        switch (drawState.drawType) {
        case DRAW_TILE: {

          WorldCoords coords = getWorldGridCoords(drawState.startPos,
                                                  drawState.mousePos, camera);

          // Calculate the size of the array based on the bounding box
          int coordArraySize = getBoundingBoxSize(coords);
          int coordArray[coordArraySize][2];

          coordsToArray(coords, coordArray);

          Array2DPtr coordData = {.arrayLength = coordArraySize,
                                  .array = coordArray};

          updateDrawnTiles(coordData, &drawState, tileTypes);

          drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                      windowState, camera);
          break;
        }
        case DRAW_WALL: {
          WorldCoords coords = getWorldGridCoords(drawState.startPos,
                                                  drawState.mousePos, camera);

          // Calculate the size of the array based on the bounding box
          int coordArraySize = getBoundingPerimeterSize(coords);
          int coordArray[coordArraySize][2];

          coordsToPerimeterArray(coords, coordArray);

          Array2DPtr coordData = {.arrayLength = coordArraySize,
                                  .array = coordArray};

          updateDrawnTiles(coordData, &drawState, tileTypes);

          calculateWallOrientations(&drawState, wallOrientationMap);

          drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                      windowState, camera);
          break;
        }
        }
        break;
      }
      case MODE_PATHING: {
        // Check if we're still within the starting tile (32x32 pixels)
        float deltaX = drawState.mousePos.x - drawState.startPos.x;
        float deltaY = drawState.mousePos.y - drawState.startPos.y;
        bool withinStartingTile =
            (fabsf(deltaX) < TILE_SIZE && fabsf(deltaY) < TILE_SIZE);

        // If we moved back to starting tile, reset capture to allow
        // recalculation
        if (withinStartingTile && drawState.hasCapturedDragDirection) {
          drawState.hasCapturedDragDirection = false;
          drawState.initialDragDirection = (Vector2){0, 0};
        }

        // Capture initial drag direction if we haven't yet and we're outside
        // starting tile
        if (!drawState.hasCapturedDragDirection && !withinStartingTile) {
          float dragDistance = sqrt(deltaX * deltaX + deltaY * deltaY);

          // Only capture direction after mouse moves outside the starting tile
          if (dragDistance >
              5.0f) { // Small buffer to avoid jitter at tile edge
            drawState.initialDragDirection = (Vector2){deltaX, deltaY};
            drawState.hasCapturedDragDirection = true;
          }
        }
        switch (drawState.drawType) {
        case DRAW_TILE: {

          WorldCoords coords = getWorldGridCoords(drawState.startPos,
                                                  drawState.mousePos, camera);

          int coordArraySize = (abs(coords.endX - coords.startX) +
                                (abs(coords.endY - coords.startY)) + 1);

          int coordArray[coordArraySize][2];

          Array2DPtr pathData = {.arrayLength = coordArraySize,
                                 .array = coordArray};

          calculatePath(coords, pathData, &drawState);

          updateDrawnTiles(pathData, &drawState, tileTypes);

          drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                      windowState, camera);

          break;
        }
        case DRAW_WALL: {

          WorldCoords coords = getWorldGridCoords(drawState.startPos,
                                                  drawState.mousePos, camera);

          int coordArraySize = (abs(coords.endX - coords.startX) +
                                (abs(coords.endY - coords.startY)) + 1);

          int coordArray[coordArraySize][2];

          Array2DPtr pathData = {.arrayLength = coordArraySize,
                                 .array = coordArray};

          calculatePath(coords, pathData, &drawState);

          updateDrawnTiles(pathData, &drawState, tileTypes);

          calculateWallOrientations(&drawState, wallOrientationMap);

          drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                      windowState, camera);

          break;
        }
        }
        break;
      }
      case MODE_PAINTER: {
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

        switch (drawState.drawType) {
          int style;
        case DRAW_TILE:

          if (!alreadyVisited) {
            style = getRandTileStyle(drawState.activeTileKey, tileTypes);
            drawState.drawnTiles[drawState.drawnTilesCount][0] = x;
            drawState.drawnTiles[drawState.drawnTilesCount][1] = y;
            drawState.drawnTiles[drawState.drawnTilesCount][2] = style;
            drawState.drawnTilesCount++;
          }

          drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                      windowState, camera);
          break;

        case DRAW_WALL:

          if (!alreadyVisited) {
            drawState.drawnTiles[drawState.drawnTilesCount][0] = x;
            drawState.drawnTiles[drawState.drawnTilesCount][1] = y;
            drawState.drawnTilesCount++;
          }

          drawPreview(&currentMap, &drawState, tileTypes, edgeTypes, wallTypes,
                      windowState, camera);

          break;
        }
      }
      }
    } else {

      // Fallback: Draw a simple cursor preview if no drawing mode is active
      WorldCoords coords =
          getWorldGridCoords(drawState.mousePos, drawState.mousePos, camera);

      switch (drawState.drawType) {
        Vector2 previewPos;
        Texture2D previewTex;
      case DRAW_TILE:
        previewPos =
            (Vector2){coords.startX * TILE_SIZE, coords.startY * TILE_SIZE};
        previewTex = tileTypes[drawState.activeTileKey].tex[0];
        DrawTexture(previewTex, previewPos.x, previewPos.y, WHITE);
        DrawRectangleLines(previewPos.x, previewPos.y, TILE_SIZE, TILE_SIZE,
                           RED);
        break;

      case DRAW_WALL: {
        Texture2D previewTex[4] = {
            wallTypes[drawState.activeWallKey].wallTex[0].tex,
            wallTypes[drawState.activeWallKey].wallTex[1].tex,
            wallTypes[drawState.activeWallKey].wallTex[2].tex,
            wallTypes[drawState.activeWallKey].wallTex[3].tex};
        Vector2 previewPos[4] = {
            {(coords.startX - 1) * TILE_SIZE, (coords.startY - 1) * TILE_SIZE},
            {(coords.startX) * TILE_SIZE, (coords.startY - 1) * TILE_SIZE},
            {(coords.startX - 1) * TILE_SIZE, (coords.startY) * TILE_SIZE},
            {(coords.startX) * TILE_SIZE, (coords.startY) * TILE_SIZE},
        };
        for (int i = 0; i < 4; i++) {
          DrawTexture(previewTex[i], previewPos[i].x, previewPos[i].y, WHITE);
        }
        DrawRectangleLines(previewPos[3].x - TILE_SIZE,
                           previewPos[3].y - TILE_SIZE, TILE_SIZE * 2,
                           TILE_SIZE * 2, RED);
        break;
      }
      };
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
      // Get neighbors to placement
      int visitedTiles[GRID_SIZE * GRID_SIZE][2];
      int visitedCount = 0;
      switch (drawState.drawType) {
      case DRAW_TILE:

        calculateEdgeGrid(&drawState, visitedTiles, &visitedCount);

        // Add drawn tiles to undo/redo stack
        createTileChangeBatch(manager, &currentMap, &drawState, visitedTiles,
                              visitedCount);

        // Texture updates
        applyTiles(&currentMap, &drawState);
        computeEdges(visitedTiles, visitedCount, &currentMap, tileTypes,
                     edgeTypes);

        memset(drawState.drawnTiles, 0, sizeof(drawState.drawnTiles));

        drawState.isDrawing = false;
        break;
      case DRAW_WALL:
        calculateWallGrid(&drawState, visitedTiles, &visitedCount);
        if (drawState.drawMode == MODE_BOX) {
          calculateWallOrientations(&drawState, wallOrientationMap);
        }
        createTileChangeBatch(manager, &currentMap, &drawState, visitedTiles,
                              visitedCount);
        applyTiles(&currentMap, &drawState);
        computeWalls(visitedTiles, visitedCount, &currentMap, wallTypes);
        memset(drawState.drawnTiles, 0, sizeof(drawState.drawnTiles));
        drawState.isDrawing = false;
        break;
      }
      drawState.hasCapturedDragDirection = false;
    }

    EndMode2D();

    // Handle command mode
    handleCommandMode(&commandState, windowState.height, windowState.width,
                      tileTypes, edgeTypes, wallTypes, db, &drawState,
                      &currentMap);

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

  // free wall orientation
  for (int i = 0; i < wallOrientationMap->capacity; ++i) {
    Entry *current = wallOrientationMap->buckets[i];
    while (current != NULL) {
      Entry *temp = current;
      current = current->next;
      free(temp);
    }
  }

  free(wallOrientationMap->buckets);
  free(wallOrientationMap);
  free(manager);
  free(tileTypes);
  free(edgeTypes);
  sqlite3_close(db);
  CloseWindow();
  return 0;
}
