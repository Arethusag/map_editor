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

// Variables
char command[256] = {0};
unsigned int commandIndex = 0;
bool inCommandMode = false;
int activeTileKey = 0;
int grid[GRID_SIZE][GRID_SIZE] = {0};
int maxTileKey;

// Functions
void parseCommand(Tile tileTypes[]) {
    if (strncmp(command, ";TILE ", 6) == 0) {
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
    } else {
        printf("Command not recognized\n");
    }
}

int main() {
    // Database Initialization
    sqlite3 *db;
    if (sqlite3_open("test.db", &db) == SQLITE_OK) {
        printf("Database opened successfully.\n");
    } else {
        printf("Error opening database: %s\n", sqlite3_errmsg(db));
    };

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

    // Get map dimensions
    const char* dimsQuery = "SELECT MAX(x), MAX(y), MIN(x), MIN(y) FROM map;";
    sqlite3_stmt* dimsStmt;
    int maxX;
    int maxY;
    int minX;
    int minY;
    if (sqlite3_prepare_v2(db, dimsQuery, -1, &dimsStmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(dimsStmt) == SQLITE_ROW) {
            maxX = sqlite3_column_int(dimsStmt, 0);
            maxY = sqlite3_column_int(dimsStmt, 1);
            minX = sqlite3_column_int(dimsStmt, 2);
            minY = sqlite3_column_int(dimsStmt, 3);
        }
    }
    sqlite3_finalize(dimsStmt);
    printf("Range x-coordinate: (%d,%d)\n", minX, maxX);
    printf("Range y-coordinates: (%d,%d)\n", minY, maxY);

    // Create map grid
    const char* mapQuery = "SELECT x, y, tile_key FROM map;";
    sqlite3_stmt* mapStmt;
    if (sqlite3_prepare_v2(db, mapQuery, -1, &mapStmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(mapStmt) == SQLITE_ROW) {
            int x = sqlite3_column_int(mapStmt, 0);
            int y = sqlite3_column_int(mapStmt, 1);
            int tileKey = sqlite3_column_int(mapStmt, 2);
            grid[x][y] = tileKey;
        }
    }
    sqlite3_finalize(mapStmt);
    sqlite3_close(db);

    // Window Initialization
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Map Editor");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {

        // Initialize render loop
        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Active tile
        int mouseX = GetMouseX()/32;
        int mouseY = GetMouseY()/32;
        Color activeColor = tileTypes[activeTileKey].color;
        DrawRectangle(mouseX * TILE_SIZE, mouseY * TILE_SIZE, TILE_SIZE, TILE_SIZE, activeColor);
        DrawRectangleLines(mouseX * TILE_SIZE, mouseY * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (mouseX >= 0 && mouseY >= 0 && mouseX < GRID_SIZE && mouseY < GRID_SIZE) {
                if (mouseX > maxX) {
                    maxX = mouseX;
                } else if (mouseX < minX) {
                    minX = mouseX;
                } else if (mouseY > maxY) {
                    maxY = mouseY;
                } else if (mouseY < minY) {
                    minY = mouseY;
                }
                grid[mouseX][mouseY] = activeTileKey;
                printf("Coordinate (%d,%d) assigned tile key: %d\n", mouseX, mouseY, activeTileKey);
            }
        }

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
            int key = GetKeyPressed();
            if (key >= 32 && key <= 126 && commandIndex < sizeof(command) -1) {
                command[commandIndex++] = (char)key;
            } else if (key == KEY_BACKSPACE && commandIndex > 0) {
                command[--commandIndex] ='\0';
            } else if (key == KEY_ENTER) {
                printf("Command entered: %s\n", command);
                parseCommand(tileTypes);
                inCommandMode = false;
            } else if (key == KEY_ESCAPE) {
                inCommandMode = false;
            }
            DrawRectangle(0, screenHeight - 30, screenWidth, 30, DARKGRAY);
            DrawText(command, 10, screenHeight -25, 20, RAYWHITE);
        }
        
        // Draw grid
        for (int x = minX; x <= maxX; x++) {
            for (int y = minY; y <= maxY; y++) {
                if (grid[x][y] != 0) {
                    int tileKey = grid[x][y]; 
                    Color tileColor = tileTypes[tileKey].color;
                    DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, tileColor); 
                }
            }
        }
        EndDrawing();
    }
    CloseWindow();
   
    return 0;
}
