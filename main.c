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

    const char* countQuery = "SELECT COUNT(*) FROM tile;";
    printf(countQuery);

    // Load tile types from database
    Tile tileTypes[5];
    const char* tileQuery = "SELECT tile_key, walkable, color FROM tile;";
    sqlite3_stmt* tileStmt;
    int rc = sqlite3_prepare_v2(db, tileQuery, -1, &tileStmt, NULL);
    if (rc == SQLITE_OK) {
        while (sqlite3_step(tileStmt) == SQLITE_ROW) {
            int tile_key = sqlite3_column_int(tileStmt, 0);
            int walkable = sqlite3_column_int(tileStmt, 1);
            const unsigned char *colorHexStr = sqlite3_column_text(tileStmt, 2);
            unsigned int colorHex = strtol((const char *)colorHexStr, NULL, 16);

            Color color = GetColor(colorHex);

            printf("Tile Key: %d\n", tile_key);

            tileTypes[tile_key] = (Tile){ tile_key, walkable, color };
        }
    }
    sqlite3_finalize(tileStmt);

    sqlite3_close(db);

    // Window Initialization
    const int screen_width = 800;
    const int screen_height = 400;
    InitWindow(screen_width, screen_height, "Map Editor");
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);
        EndDrawing();
    }
    CloseWindow();
   
    return 0;
}
