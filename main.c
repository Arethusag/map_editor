#include <raylib.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

#define TILE_SIZE 32

typedef struct Tile {
    int tile_key;
    int walkable;
    Color color;
} Tile;

int main() {
    // Database Initialization
    sqlite3 *db;
    if (sqlite3_open("test.db", &db) == SQLITE_OK) {
        printf("Database opened successfully.\n");
    } else {
        printf("Error opening database: %s\n", sqlite3_errmsg(db));
    };

    // Get number of tile types
    const char* countQuery = "SELECT COUNT(*) FROM tile;";
    sqlite3_stmt* countStmt;
    int count;
    if (sqlite3_prepare_v2(db, countQuery, -1, &countStmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(countStmt) == SQLITE_ROW) {
            count = sqlite3_column_int(countStmt, 0);
        }
    }
    sqlite3_finalize(countStmt);
    printf("Number of tile types to be loaded: %d\n", count);

    // Load tile types from database
    Tile tileTypes[count];
    const char* tileQuery = "SELECT tile_key, walkable, color FROM tile;";
    sqlite3_stmt* tileStmt;
    if (sqlite3_prepare_v2(db, tileQuery, -1, &tileStmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(tileStmt) == SQLITE_ROW) {
            int tile_key = sqlite3_column_int(tileStmt, 0);
            int walkable = sqlite3_column_int(tileStmt, 1);
            const unsigned int colorHex = sqlite3_column_int(tileStmt, 2);
            Color color = GetColor(colorHex);
            /* printf("Tile Key: %d\n", tile_key); */
            tileTypes[tile_key] = (Tile){ tile_key, walkable, color };
        }
    }
    sqlite3_finalize(tileStmt);

    // Get map dimensions
    const char* dimsQuery = "SELECT COUNT(DISTINCT x), COUNT(DISTINCT y) FROM map;";
    sqlite3_stmt* dimsStmt;
    int dims_x;
    int dims_y;
    if (sqlite3_prepare_v2(db, dimsQuery, -1, &dimsStmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(dimsStmt) == SQLITE_ROW) {
            dims_x = sqlite3_column_int(dimsStmt, 0);
            dims_y = sqlite3_column_int(dimsStmt, 1);
        }
    }
    sqlite3_finalize(dimsStmt);
    printf("Distinct number of x-coordinates: %d\n", dims_x);
    printf("Distinct number of y-coordinates: %d\n", dims_y);

    // Create map grid
    int grid[dims_x][dims_y];
    const char* mapQuery = "SELECT x, y, tile_key FROM map;";
    sqlite3_stmt* mapStmt;
    if (sqlite3_prepare_v2(db, mapQuery, -1, &mapStmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(mapStmt) == SQLITE_ROW) {
            int x = sqlite3_column_int(mapStmt, 0);
            int y = sqlite3_column_int(mapStmt, 1);
            int tile_key = sqlite3_column_int(mapStmt, 2);
            /* printf("Grid coord(%d,%d) assigned tile type %d\n", x, y, tile_key); */
            grid[x][y] = tile_key;
        }
    }
    sqlite3_finalize(mapStmt);

    sqlite3_close(db);

    // Window Initialization
    const int screen_width = 800;
    const int screen_height = 600;
    InitWindow(screen_width, screen_height, "Map Editor");
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        int mouse_x = GetMouseX()/32;
        int mouse_y = GetMouseY()/32;
        /* printf("Mouse Pos(%d,%d)\n", mouse_x/32, mouse_y/32); */

        // Initialize render loop
        BeginDrawing();
        ClearBackground(BLACK);
        
        // Draw grid
        for (int x = 0; x < dims_x; x++) {
            for (int y = 0; y < dims_y; y++) {
                /* printf("Drawing tile(%d,%d)\n", x, y); */
                int tile_key = grid[x][y]; 
                /* printf("Tile Key: %d\n", tile_key); */
                Color color = tileTypes[tile_key].color;
                DrawRectangle(x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, color); 
            }
        }

        // Red highlight on mouse square
        DrawRectangleLines(mouse_x * TILE_SIZE, mouse_y * TILE_SIZE, TILE_SIZE, TILE_SIZE, RED);


        EndDrawing();
    }
    CloseWindow();
   
    return 0;
}
