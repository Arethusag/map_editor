#include <raylib.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Definitions
#define TILE_SIZE 32
#define GRID_SIZE 256

// Structs
typedef struct Tile {
    int tileKey;
    int walkable;
    Color color;
} Tile;

typedef struct Map {
    const char *name;
    int grid[GRID_SIZE][GRID_SIZE];
    int maxX;
    int maxY;
    int minX;
    int minY;
} Map;

// Variables
char command[256] = {0};
unsigned int commandIndex = 0;
bool inCommandMode = false;
int activeTileKey = 0;
int maxTileKey;
Map currentMap;
sqlite3 *db;

// Functions
void loadMap(sqlite3 *db, const char *table) {

    // Database Initialization
    if (sqlite3_open("test.db", &db) == SQLITE_OK) {
        printf("Database opened successfully.\n");
    } else {
        printf("Error opening database: %s\n", sqlite3_errmsg(db));
    };

    //Buffers to hold queries
    char dimsQuery[256];
    char mapQuery[256];

    // Format Queries
    snprintf(dimsQuery, sizeof(dimsQuery), "SELECT MAX(x), MAX(y), MIN(x), MIN(y) FROM %s;", table);
    snprintf(mapQuery, sizeof(mapQuery), "SELECT x, y, tile_key FROM %s;", table);

    sqlite3_stmt* dimsStmt;
    sqlite3_stmt* mapStmt;
    
    if (sqlite3_prepare_v2(db, dimsQuery, -1, &dimsStmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(dimsStmt) == SQLITE_ROW) {
            currentMap.maxX = sqlite3_column_int(dimsStmt, 0);
            currentMap.maxY = sqlite3_column_int(dimsStmt, 1);
            currentMap.minX = sqlite3_column_int(dimsStmt, 2);
            currentMap.minY = sqlite3_column_int(dimsStmt, 3);
            printf("Range x-coordinate: (%d,%d)\n", currentMap.minX, currentMap.maxX);
            printf("Range y-coordinates: (%d,%d)\n", currentMap.minY, currentMap.maxY);
        }
    } else {
        printf("Error preparing SQL query: %s\n", sqlite3_errmsg(db));
        return;
    }
    sqlite3_finalize(dimsStmt);

    // Create map grid
    if (sqlite3_prepare_v2(db, mapQuery, -1, &mapStmt, NULL) == SQLITE_OK) {
        memset(currentMap.grid, 0, sizeof(currentMap.grid));
        while (sqlite3_step(mapStmt) == SQLITE_ROW) {
            int x = sqlite3_column_int(mapStmt, 0);
            int y = sqlite3_column_int(mapStmt, 1);
            int tileKey = sqlite3_column_int(mapStmt, 2);

            if (x >= 0 && y >=0 && x < GRID_SIZE && y < GRID_SIZE) {
                currentMap.grid[x][y] = tileKey;
            } else {
                printf("Warning: x or y out of bounds (%d,%d)\n", x, y);
                return;
            }    
        }
    } else {
        fprintf(stderr, "Error preparing SQL query: %s\n", sqlite3_errmsg(db));
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
        for (int x = currentMap.minX; x <= currentMap.maxX; x++) {
            for (int y = currentMap.minY; y <= currentMap.maxY; y++) {
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

    // Window Initialization
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Map Editor");
    SetTargetFPS(60);
    bool isDrawing = false;

    while (!WindowShouldClose()) {

        // Initialize render loop
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Active tile
        int mouseX = GetMouseX()/32;
        int mouseY = GetMouseY()/32;
        Color activeColor = tileTypes[activeTileKey].color;
        
        // Mouse press event
        int startX;
        int startY;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (mouseX >= 0 && mouseY >= 0 && mouseX < GRID_SIZE && mouseY < GRID_SIZE) {
                if (mouseX > currentMap.maxX) {
                    currentMap.maxX = mouseX;
                }
                if (mouseX < currentMap.minX) {
                    currentMap.minX = mouseX;
                }
                if (mouseY > currentMap.maxY) {
                    currentMap.maxY = mouseY;
                }
                if (mouseY < currentMap.minY) {
                    currentMap.minY = mouseY;
                }
                startX = mouseX;
                startY = mouseY;
                isDrawing = true;
            }
        }

        int endX;
        int endY;

        // Command mode
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
            if (IsKeyPressed(KEY_SEMICOLON)) {
                printf("Entering command mode\n");
                inCommandMode = true;
                commandIndex = 0;
                memset(command, 0, sizeof(command));
            }
        }

        // Command processing
        if (inCommandMode) {
            int key = GetCharPressed();
            if (key >= 32 && key <= 126 && commandIndex < sizeof(command) -1) {
                command[commandIndex++] = (char)key;
            } else if (IsKeyPressed(KEY_BACKSPACE) && commandIndex > 0) {
                command[--commandIndex] ='\0';
            } else if (IsKeyPressed(KEY_ENTER)) {
                printf("Command entered: %s\n", command);
                parseCommand(tileTypes, db);
                inCommandMode = false;
            } else if (IsKeyPressed(KEY_ESCAPE)) {
                inCommandMode = false;
            }
            DrawRectangle(0, screenHeight - 30, screenWidth, 30, DARKGRAY);
            DrawText(command, 10, screenHeight -25, 20, RAYWHITE);
        }
        
        // Draw grid
        for (int x = currentMap.minX; x <= currentMap.maxX; x++) {
            for (int y = currentMap.minY; y <= currentMap.maxY; y++) {
                if (currentMap.grid[x][y] != 0) {
                    int tileKey = currentMap.grid[x][y]; 
                    Color tileColor = tileTypes[tileKey].color;
                    DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, tileColor); 
                }
            }
        }

        if (isDrawing) {

            endX = mouseX;
            endY = mouseY;
            
            if (endX > GRID_SIZE) {
                endX = GRID_SIZE;
            } else if (endX < 0) {
                endX = 0;
            } else {
                endX = mouseX;
            }

            if (endY > GRID_SIZE) {
                endY = GRID_SIZE;
            } else if (endY < 0) {
                endY = 0;
            } else {
                endY = mouseY;
            }

            if (startX <= endX && startY <= endY ) {
                for (int x = startX; x <= endX; x++) {
                    for (int y = startY; y <= endY; y++) {
                        DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, activeColor);
                        DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);
                    }
                }
            } else if (startX >= endX && startY >= endY) {
                for (int x = endX; x <= startX; x++) {
                    for (int y = endY; y <= startY; y++) {
                        DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, activeColor);
                        DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);
                    }
                }
            } else if (startX >= endX && startY <= endY) {
                for (int x = endX; x <= startX; x++) {
                    for (int y = startY; y <= endY; y++) {
                        DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, activeColor);
                        DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);
                    }
                }
            } else if (startX <= endX && startY >= endY) {
                for (int x = startX; x <= endX; x++) {
                    for (int y = endY; y <= startY; y++) {
                        DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, activeColor);
                        DrawRectangleLines(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);
                    }
                }
            }
        } else {
            DrawRectangle(mouseX * TILE_SIZE, mouseY * TILE_SIZE, TILE_SIZE, TILE_SIZE, activeColor);
            DrawRectangleLines(mouseX * TILE_SIZE, mouseY * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);
        }

        if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            endX = mouseX;
            endY = mouseY;
            
            if (endX > GRID_SIZE) {
                endX = GRID_SIZE;
            } else if (endX < 0) {
                endX = 0;
            } else {
                endX = mouseX;
            }

            if (endY > GRID_SIZE) {
                endY = GRID_SIZE;
            } else if (endY < 0) {
                endY = 0;
            } else {
                endY = mouseY;
            }


            if (endX > currentMap.maxX) {
                currentMap.maxX = endX;
            }
            if (endX < currentMap.minX) {
                currentMap.minX = endX;
            }
            if (endY > currentMap.maxY) {
                currentMap.maxY = endY;
            }
            if (endY < currentMap.minY) {
                currentMap.minY = endY;
            }
            printf("Drawing released with range ((%d,%d),(%d,%d))\n", startX, endX, startY, endY);

            if (startX <= endX && startY <= endY ) {
                for (int x = startX; x <= endX; x++) {
                    for (int y = startY; y <= endY; y++) {
                        currentMap.grid[x][y] = activeTileKey;
                    }
                }
            } else if (startX >= endX && startY >= endY) {
                for (int x = endX; x <= startX; x++) {
                    for (int y = endY; y <= startY; y++) {
                        currentMap.grid[x][y] = activeTileKey;
                    }
                }
            } else if (startX >= endX && startY <= endY) {
                for (int x = endX; x <= startX; x++) {
                    for (int y = startY; y <= endY; y++) {
                        currentMap.grid[x][y] = activeTileKey;
                    }
                }
            } else if (startX <= endX && startY >= endY) {
                for (int x = startX; x <= endX; x++) {
                    for (int y = endY; y <= startY; y++) {
                        currentMap.grid[x][y] = activeTileKey;
                    }
                }
            }
            isDrawing = false;
        }

        EndDrawing();
    }
    CloseWindow();
   
    return 0;
}
