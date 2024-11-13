#include <raylib.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definitions
#define TILE_SIZE 32
#define GRID_SIZE 256
#define CAMERA_SPEED 300.0f

// Structs
typedef struct Tile {
    int tileKey;
    int walkable;
    Color color;
} Tile;

typedef struct Map {
    const char *name;
    int grid[GRID_SIZE][GRID_SIZE];
} Map;

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

// Variables
char command[256] = {0};
unsigned int commandIndex = 0;
bool inCommandMode = false;
int activeTileKey = 0;
int maxTileKey;
Map currentMap;
sqlite3 *db;
Camera2D camera = {0};

// Functions
void loadMap(sqlite3 *db, const char *table) {

    // Database Initialization
    if (sqlite3_open("test.db", &db) == SQLITE_OK) {
        printf("Database opened successfully.\n");
    } else {
        printf("Error opening database: %s\n", sqlite3_errmsg(db));
    };

    //Buffers to hold queries
    char mapQuery[256];

    // Format Map Query
    snprintf(mapQuery, sizeof(mapQuery), "SELECT x, y, tile_key FROM %s;", table);
    sqlite3_stmt* mapStmt;

    // Create map grid
    if (sqlite3_prepare_v2(db, mapQuery, -1, &mapStmt, NULL) == SQLITE_OK) {
        memset(currentMap.grid, 0, sizeof(currentMap.grid));
        while (sqlite3_step(mapStmt) == SQLITE_ROW) {
            int x = sqlite3_column_int(mapStmt, 0);
            int y = sqlite3_column_int(mapStmt, 1);
            int tileKey = sqlite3_column_int(mapStmt, 2);
            currentMap.grid[x][y] = tileKey;
        }
    } else {
        printf("Error preparing SQL query: %s\n", sqlite3_errmsg(db));
    }
    sqlite3_finalize(mapStmt);
    sqlite3_close(db);
    currentMap.name = table;
}

void saveMap(sqlite3 *db, char *table) {
    
    // Database Initialization
    if (sqlite3_open("test.db", &db) == SQLITE_OK) {
        printf("Database opened successfully.\n");
    } else {
        printf("Error opening database: %s\n", sqlite3_errmsg(db));
    };

    //Buffer to hold query
    char dropQuery[256];
    char createQuery[256];
    char insertQuery[256];

    sqlite3_stmt* dropStmt;
    sqlite3_stmt* createStmt;
    sqlite3_stmt* insertStmt;
    
    snprintf(dropQuery, sizeof(dropQuery), "DROP TABLE IF EXISTS %s;", table);
    snprintf(createQuery, sizeof(createQuery),
            "CREATE TABLE IF NOT EXISTS %s("
            "x INTEGER NOT NULL,"
            "y INTEGER NOT NULL,"
            "tile_key INTEGER NOT NULL);", table);
    snprintf(insertQuery, sizeof(insertQuery),
            "INSERT INTO %s (x, y, tile_key) VALUES (?, ?, ?);", table);

    // Execute DROP TABLE
    if (sqlite3_prepare_v2(db, dropQuery, -1, &dropStmt, NULL) == SQLITE_OK) {
        sqlite3_step(dropStmt);
        sqlite3_finalize(dropStmt);
    } else {
        return;
    }

    // Execute CREATE TABLE
    if (sqlite3_prepare_v2(db, createQuery, -1, &createStmt, NULL) == SQLITE_OK) {
        sqlite3_step(createStmt);
        sqlite3_finalize(createStmt);
    } else {
        return;
    }

    // Insert map grid
    if (sqlite3_prepare_v2(db, insertQuery, -1, &insertStmt, NULL) == SQLITE_OK) {
        for (int x = 0; x < GRID_SIZE; x++) {
            for (int y = 0; y < GRID_SIZE; y++) {
                if (currentMap.grid[x][y] != 0) {
                    int tileKey = currentMap.grid[x][y]; 
                    sqlite3_bind_int(insertStmt, 1, x);         // Bind x
                    sqlite3_bind_int(insertStmt, 2, y);         // Bind y
                    sqlite3_bind_int(insertStmt, 3, tileKey);   // Bind tile_key
                    sqlite3_step(insertStmt);
                    sqlite3_reset(insertStmt); 
                    sqlite3_clear_bindings(insertStmt);
                }
            }
        }
    } else {
        return;
    }
    sqlite3_finalize(insertStmt);
    sqlite3_close(db);
} 

// Parse command input
void parseCommand(Tile tileTypes[], sqlite3 *db) {
    if (strncmp(command, ":tile ", 6) == 0) {
        char *tileKeyStr = command +6;
        char *endptr;
        long tileKey = strtol(tileKeyStr, &endptr, 10);
        if (*endptr == '\0') {
            if (tileKey >= 0 && tileKey <= maxTileKey && tileTypes[tileKey].tileKey == tileKey) {
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
        printf("Map loaded: %s\n", table);
    } else if (strncmp(command, ":save ", 6) == 0) {
        char *table = &command[6];
        saveMap(db, table);
        printf("Map saved: %s\n", table);
    } else {
        printf("Command not recognized\n");
    }
}

// Function to update camera offset based on new window dimensions
void UpdateCameraOffset(Camera2D *camera, int newWidth, int newHeight) {
    camera->offset = (Vector2){ newWidth/2.0f, newHeight/2.0f };
}

// Function to handle window resize
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
    if (*coord > max) *coord = max;
    else if (*coord < min) *coord = 0;
    else return;
}

Vector2 GetWorldCoordinates(Vector2 screenPos, Camera2D camera) {
    Vector2 worldPos;
    worldPos.x = (screenPos.x - camera.offset.x) / camera.zoom + camera.target.x;
    worldPos.y = (screenPos.y - camera.offset.y) / camera.zoom + camera.target.y;
    return worldPos;
}

WorldCoords GetVisibleGridBounds(Camera2D camera, int screenWidth, int screenHeight) {
    WorldCoords bounds;

    // Get the bounds of the visible area in world coordinates
    Vector2 startPos = {0, 0};
    Vector2 endPos = {screenWidth, screenHeight};
    Vector2 topLeft = GetWorldCoordinates(startPos, camera);
    Vector2 bottomRight = GetWorldCoordinates(endPos, camera);
    
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

WorldCoords GetWorldGridCoords(Vector2 startPos, Vector2 endPos, Camera2D camera) {
    WorldCoords coords;
    
    // Convert start and end positions to world coordinates
    Vector2 worldStartPos = GetWorldCoordinates(startPos, camera);
    Vector2 worldEndPos = GetWorldCoordinates(endPos, camera);
    
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


void handleCommandMode(char *command, unsigned int *commandIndex, bool *inCommandMode, 
                      int screenHeight, int screenWidth, Tile tileTypes[], sqlite3 *db) {

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
            parseCommand(tileTypes, db);
            *inCommandMode = false;
        } else if (IsKeyPressed(KEY_ESCAPE)) {
            *inCommandMode = false;
        }

        DrawRectangle(0, screenHeight - 30, screenWidth, 30, DARKGRAY);
        DrawText(command, 10, screenHeight - 25, 20, RAYWHITE);
    }
}


void drawPreview(WorldCoords coords, Color activeColor) {
    int minX = (coords.startX <= coords.endX) ? coords.startX : coords.endX;
    int maxX = (coords.startX <= coords.endX) ? coords.endX : coords.startX;
    int minY = (coords.startY <= coords.endY) ? coords.startY : coords.endY;
    int maxY = (coords.startY <= coords.endY) ? coords.endY : coords.startY;

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            Vector2 pos = { x * TILE_SIZE, y * TILE_SIZE };
            DrawRectangle(pos.x, pos.y, TILE_SIZE, TILE_SIZE, activeColor);
            DrawRectangleLines(pos.x, pos.y, TILE_SIZE, TILE_SIZE, RED);
        }
    }
}

void applyTiles(Map *map, WorldCoords coords, int activeTileKey) {
    int minX = (coords.startX <= coords.endX) ? coords.startX : coords.endX;
    int maxX = (coords.startX <= coords.endX) ? coords.endX : coords.startX;
    int minY = (coords.startY <= coords.endY) ? coords.startY : coords.endY;
    int maxY = (coords.startY <= coords.endY) ? coords.endY : coords.startY;

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            map->grid[x][y] = activeTileKey;
        }
    }
}

void drawExistingMap(Map *map, Tile tileTypes[], Camera2D camera, int screenWidth, int screenHeight) {
    // Get the visible bounds of the grid
    WorldCoords bounds = GetVisibleGridBounds(camera, screenWidth, screenHeight);
    
    // Only draw tiles within the visible bounds
    for (int x = bounds.startX; x <= bounds.endX; x++) {
        for (int y = bounds.startY; y <= bounds.endY; y++) {
            int tileKey = map->grid[x][y];
            Color tileColor = tileTypes[tileKey].color;
            Vector2 pos = { x * TILE_SIZE, y * TILE_SIZE };
            DrawRectangle(pos.x, pos.y, TILE_SIZE, TILE_SIZE, tileColor);
        }
    }
    
    // Draw grid lines only for visible area
    /* for (int x = bounds.startX; x <= bounds.endX; x++) { */
    /*     for (int y = bounds.startY; y <= bounds.endY; y++) { */
    /*         Vector2 pos = { x * TILE_SIZE, y * TILE_SIZE }; */
    /*         DrawRectangleLines(pos.x, pos.y, TILE_SIZE, TILE_SIZE, LIGHTGRAY); */
    /*     } */
    /* } */
}

int main() {
    // Database Initialization
    if (sqlite3_open("test.db", &db) == SQLITE_OK) {
        printf("Database opened successfully.\n");
    } else {
        printf("Error opening database: %s\n", sqlite3_errmsg(db));
    };
    loadMap(db, "map");

    // Get number of tile types
    const char* countQuery = "SELECT MAX(tile_key) +1 FROM tile;";
    sqlite3_stmt* countStmt;
    if (sqlite3_prepare_v2(db, countQuery, -1, &countStmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(countStmt) == SQLITE_ROW) {
            maxTileKey = sqlite3_column_int(countStmt, 0);
        }
    }
    sqlite3_finalize(countStmt);
    printf("Number of tile types to be loaded: %d\n", maxTileKey);

    // Load tile types from database
    Tile tileTypes[maxTileKey];
    const char* tileQuery = "SELECT tile_key, walkable, color FROM tile;";
    sqlite3_stmt* tileStmt;
    if (sqlite3_prepare_v2(db, tileQuery, -1, &tileStmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(tileStmt) == SQLITE_ROW) {
            int tileKey = sqlite3_column_int(tileStmt, 0);
            int walkable = sqlite3_column_int(tileStmt, 1);
            const unsigned int colorHex = sqlite3_column_int(tileStmt, 2);
            Color color = GetColor(colorHex);
            tileTypes[tileKey] = (Tile){ tileKey, walkable, color };
        }
    }
    sqlite3_finalize(tileStmt);

    // Get the current monitor dimensions
    /* int monitorWidth = GetMonitorWidth(GetCurrentMonitor()); */
    /* int monitorHeight = GetMonitorHeight(GetCurrentMonitor()); */
    int windowWidth = 800;
    int windowHeight = 600;

    // Initialize window with monitor dimensions
    InitWindow(windowWidth, windowHeight, "Map Editor");
    SetWindowState(FLAG_WINDOW_RESIZABLE);  // Enable window resizing
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    WindowState windowState;
    windowState.width = windowWidth;
    windowState.height = windowHeight;

    // Initialize camera
    camera.zoom = 1.0f;
    camera.offset = (Vector2){windowState.width/2.0f, windowState.width/2.0f};
    camera.target = (Vector2){GRID_SIZE/2 * TILE_SIZE, GRID_SIZE/2 * TILE_SIZE};
    camera.rotation = 0.0f;

    // Initialize camera state
    CameraState cameraState;
    cameraState.isPanning = false;
    cameraState.lastMousePosition = (Vector2){0, 0};

    // Drawing state
    bool isDrawing = false;
    Vector2 startPos = {0};
    Vector2 mousePos;

    while (!WindowShouldClose()) {
        // Handle window resizing
        HandleWindowResize(&windowState, &camera);

        // Camera movement
        float deltaTime = GetFrameTime();
        if (IsKeyDown(KEY_RIGHT)) camera.target.x += CAMERA_SPEED * deltaTime;
        if (IsKeyDown(KEY_LEFT)) camera.target.x -= CAMERA_SPEED * deltaTime;
        if (IsKeyDown(KEY_DOWN)) camera.target.y += CAMERA_SPEED * deltaTime;
        if (IsKeyDown(KEY_UP)) camera.target.y -= CAMERA_SPEED * deltaTime;

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            cameraState.isPanning = true;
            cameraState.lastMousePosition = mousePos;
        } else if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) {
            cameraState.isPanning = false;
        }

        // Update camera position while panning
        if (cameraState.isPanning) {
            Vector2 mouseDelta = {
                mousePos.x - cameraState.lastMousePosition.x,
                mousePos.y - cameraState.lastMousePosition.y
            };
            
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
            Vector2 mouseWorldPos = GetWorldCoordinates(GetMousePosition(), camera);
            
            // Apply zoom
            camera.zoom += wheel * 0.1f;
            if (camera.zoom < 0.2f) camera.zoom = 0.1f;
            
            // Adjust camera target to zoom towards mouse position
            Vector2 newMouseWorldPos = GetWorldCoordinates(GetMousePosition(), camera);
            camera.target.x += mouseWorldPos.x - newMouseWorldPos.x;
            camera.target.y += mouseWorldPos.y - newMouseWorldPos.y;
        }

        BeginDrawing();
        ClearBackground(BLACK);

        BeginMode2D(camera);

        // Update mouse position
        Color activeColor = tileTypes[activeTileKey].color;

        // Draw existing map
        drawExistingMap(&currentMap, tileTypes, camera, windowState.width, windowState.height);

        // Handle mouse input and drawing
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            startPos = mousePos;
            isDrawing = true;
        }

        if (isDrawing) {
            WorldCoords coords = GetWorldGridCoords(startPos, mousePos, camera);
            drawPreview(coords, activeColor);
        } else {
            // Draw cursor preview
            WorldCoords coords = GetWorldGridCoords(mousePos, mousePos, camera);
            Vector2 tilePos = { coords.startX * TILE_SIZE, coords.startY * TILE_SIZE };
            DrawRectangle(tilePos.x, tilePos.y, TILE_SIZE, TILE_SIZE, activeColor);
            DrawRectangleLines(tilePos.x, tilePos.y, TILE_SIZE, TILE_SIZE, RED);
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && isDrawing) {
            WorldCoords coords = GetWorldGridCoords(startPos, mousePos, camera);
            applyTiles(&currentMap, coords, activeTileKey);
            printf("Drawing released with range ((%d,%d),(%d,%d))\n", 
                   coords.startX, coords.endX, coords.startY, coords.endY);
            isDrawing = false;
        }

        EndMode2D();

        // Handle command mode
        handleCommandMode(command, &commandIndex, &inCommandMode, windowState.height, 
                         windowState.width, tileTypes, db);

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
