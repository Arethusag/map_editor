#include "database.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Variables
Color transparencyKey = {255, 0, 255, 255};

// Helper functions
int executeScalarQuery(sqlite3 *db, const char *query) {
  sqlite3_stmt *stmt;
  int scalar = 0;
  if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) == SQLITE_OK) {
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      scalar = sqlite3_column_int(stmt, 0);
    }
  } else {
    printf("Error preparing query: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
  }
  sqlite3_finalize(stmt);
  return scalar;
}

Texture2D loadTextureFromBlob(const unsigned char *blobData, int blobSize) {

  // Initialize texture
  Texture2D texture = {0};

  // Verify blob size
  if (blobSize != TILE_SIZE * TILE_SIZE * 4) {
    printf("Error: Invalid blob size %d (expected %d)\n", blobSize,
           TILE_SIZE * TILE_SIZE * 4);
    return texture; // Return an empty texture on error
  }

  // Create color array for the texture
  Color pixelData[TILE_SIZE * TILE_SIZE];

  // Parse blob data into Color array
  for (int i = 0; i < TILE_SIZE * TILE_SIZE; i++) {
    // Each pixel is 4 bytes (RGBA)
    int offset = i * 4;
    pixelData[i] = (Color){
        blobData[offset],     // R
        blobData[offset + 1], // G
        blobData[offset + 2], // B
        blobData[offset + 3]  // A
    };

    // Apply transparency key
    if (pixelData[i].r == transparencyKey.r &&
        pixelData[i].g == transparencyKey.g &&
        pixelData[i].b == transparencyKey.b &&
        pixelData[i].a == transparencyKey.a) {
      pixelData[i].a = 0; // Set alpha to 0
    }
  }

  // Initialize the Image for the tile
  Image img = {.data = pixelData, // Directly assign the pixel data
               .width = TILE_SIZE,
               .height = TILE_SIZE,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};

  texture = LoadTextureFromImage(img);
  return texture;
}

sqlite3 *connectDatabase() {
  sqlite3 *db;
  if (sqlite3_open("test.db", &db) != SQLITE_OK) {
    printf("Error opening database: %s\n", sqlite3_errmsg(db));
    return NULL;
  };
  return db;
}

// Database functions
Edge *loadEdges(sqlite3 *db, Map *map) {

  // Get number of tile types
  const char *countQuery =
      "SELECT COUNT(DISTINCT tile_key) FROM texture WHERE type = 'edge';";

  int countEdges = executeScalarQuery(db, countQuery);
  if (countEdges == -1) {
    sqlite3_close(db);
    return NULL;
  } else {
    map->countEdges = countEdges;
  }

  // Allocate memory for Edge structs
  Edge *edgeTypes = (Edge *)malloc(countEdges * sizeof(Edge));
  if (edgeTypes == NULL) {
    printf("Memory allocation failed\n");
    sqlite3_close(db);
    return NULL;
  }

  // Load tile edges from database
  const char *edgeQuery =
      "SELECT tile_key, data FROM texture WHERE type = 'edge';";
  sqlite3_stmt *edgeStmt;
  if (sqlite3_prepare_v2(db, edgeQuery, -1, &edgeStmt, NULL) == SQLITE_OK) {
    int currentIndex = -1;
    int textureIndex = 1;
    while (sqlite3_step(edgeStmt) == SQLITE_ROW) {
      int tileKey = sqlite3_column_int(edgeStmt, 0);
      const unsigned char *blobData = sqlite3_column_blob(edgeStmt, 1);
      int blobSize = sqlite3_column_bytes(edgeStmt, 1);

      // Detect a new tile key
      if (currentIndex == -1 || edgeTypes[currentIndex].tileKey != tileKey) {
        currentIndex++;
        textureIndex = 0;

        // Initialize a new Edge struct
        edgeTypes[currentIndex].tileKey = tileKey;
        memset(edgeTypes[currentIndex].edges, 0,
               sizeof(edgeTypes[currentIndex].edges));
      }

      if (textureIndex >= 12) {
        printf("Error: Too many textures for tile %d\n", tileKey);
        continue;
      }

      // Populate edge with texture
      edgeTypes[currentIndex].edges[textureIndex] =
          loadTextureFromBlob(blobData, blobSize);
      textureIndex++;
    }

  } else {
    printf("Error preparing tile query: %s\n", sqlite3_errmsg(db));
  }
  sqlite3_finalize(edgeStmt);
  return edgeTypes;
}

Tile *loadTiles(sqlite3 *db, Map *map) {

  // Get number of tile types
  const char *countQuery = "SELECT COUNT(DISTINCT tile_key) FROM tile;";
  const char *maxQuery = "SELECT MAX(tile_key) FROM tile;";
  int countTiles = executeScalarQuery(db, countQuery);
  if (countTiles == -1) {
    sqlite3_close(db);
    return NULL;
  }
  int maxTileKey = executeScalarQuery(db, maxQuery);
  if (maxTileKey == -1) {
    sqlite3_close(db);
    return NULL;
  } else {
    map->maxTileKey = maxTileKey;
  }

  // Allocate memory for the tiles
  Tile *tileTypes = (Tile *)malloc(countTiles * sizeof(Tile));
  if (tileTypes == NULL) {
    printf("Memory allocation failed\n");
    sqlite3_close(db);
    return NULL;
  }

  // Load tile types from database
  const char *tileQuery =
      "SELECT tile_key, walkable, edge_priority, edge_indicator FROM tile;";
  sqlite3_stmt *tileStmt;
  if (sqlite3_prepare_v2(db, tileQuery, -1, &tileStmt, NULL) == SQLITE_OK) {
    while (sqlite3_step(tileStmt) == SQLITE_ROW) {
      int tileKey = sqlite3_column_int(tileStmt, 0);
      int walkable = sqlite3_column_int(tileStmt, 1);
      int edgePriority = sqlite3_column_int(tileStmt, 2);
      int edgeIndicator = sqlite3_column_int(tileStmt, 3);

      // Populate tile with color array
      Texture2D tex[MAX_TILE_VARIANTS] = {0};
      int texCount = 0;
      char tileKeyStr[20];
      snprintf(tileKeyStr, sizeof(tileKeyStr), "%d", tileKey);
      const char *texQuery = "SELECT data FROM texture WHERE tile_key = "
                             "? AND type = 'tile';";
      sqlite3_stmt *texStmt;
      if (sqlite3_prepare_v2(db, texQuery, -1, &texStmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(texStmt, 1, tileKeyStr, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(texStmt) == SQLITE_ROW) {
          const unsigned char *blobData = sqlite3_column_blob(texStmt, 0);
          int blobSize = sqlite3_column_bytes(texStmt, 0);

          Texture2D loadedTex = loadTextureFromBlob(blobData, blobSize);
          tex[texCount] = loadedTex;
          texCount++;
        }
      } else {
        printf("Error preparing texture query: %s\n", sqlite3_errmsg(db));
      }
      sqlite3_finalize(texStmt);

      tileTypes[tileKey] = (Tile){.tileKey = tileKey,
                                  .walkable = walkable,
                                  .tex = {{0}}, // Initialize the array to zero
                                  .texCount = texCount,
                                  .edgePriority = edgePriority,
                                  .edgeIndicator = edgeIndicator};

      // Explicitly copy the textures
      memcpy(tileTypes[tileKey].tex, tex, sizeof(Texture2D) * texCount);
      printf("Tile %d created\n", tileKey);
    }
  } else {
    printf("Error preparing tile query: %s\n", sqlite3_errmsg(db));
  }
  sqlite3_finalize(tileStmt);
  return tileTypes;
}

Wall *loadWalls(sqlite3 *db, Map *map) {

  // Initialize variables
  Wall *wallTypes;

  // Get number of wall types
  const char *maxQuery = "SELECT MAX(wall_key) FROM wall;";

  int maxWalls = executeScalarQuery(db, maxQuery);
  if (maxWalls == -1) {
    sqlite3_close(db);
    return NULL;
  } else {
    map->maxWallKey = maxWalls;
  }

  // Allocate memory for the walls
  wallTypes = (Wall *)malloc((maxWalls + 1) * sizeof(Wall));
  if (wallTypes == NULL) {
    printf("Memory allocation failed\n");
    return NULL;
  }

  // Load wall textures from database
  const char *wallQuery = "SELECT wall_key FROM wall;";
  sqlite3_stmt *wallStmt;
  if (sqlite3_prepare_v2(db, wallQuery, -1, &wallStmt, NULL) == SQLITE_OK) {
    while (sqlite3_step(wallStmt) == SQLITE_ROW) {
      int wallKey = sqlite3_column_int(wallStmt, 0);

      WallTexture wallTex[4];
      char wallKeyStr[20];
      snprintf(wallKeyStr, sizeof(wallKeyStr), "%d", wallKey);
      const char *texQuery =
          "SELECT data, wall_quadrant.wall_quadrant_key, quadrant_key, "
          "primary_wall_quadrant_indicator "
          "FROM wall_quadrant LEFT JOIN texture "
          "ON wall_quadrant.wall_quadrant_key = texture.wall_quadrant_key "
          "WHERE wall_key = ?;";
      sqlite3_stmt *texStmt;

      if (sqlite3_prepare_v2(db, texQuery, -1, &texStmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(texStmt, 1, wallKeyStr, -1, SQLITE_TRANSIENT);
        while (sqlite3_step(texStmt) == SQLITE_ROW) {
          const unsigned char *blobData = sqlite3_column_blob(texStmt, 0);
          int blobSize = sqlite3_column_bytes(texStmt, 0);
          int wallQuadrantKey = sqlite3_column_int(texStmt, 1);
          int quadrantKey = sqlite3_column_int(texStmt, 2);
          int primaryWallQuadrantIndicator = sqlite3_column_int(texStmt, 3);

          Texture2D loadedTex = loadTextureFromBlob(blobData, blobSize);

          wallTex[quadrantKey - 1] = (WallTexture){
              .tex = loadedTex,
              .wall_quadrant_key = wallQuadrantKey,
              .quadrant_key = quadrantKey,
              .primary_wall_quadrant_indicator = primaryWallQuadrantIndicator};
        }
      } else {
        printf("Error preparing wall texture query: %s\n", sqlite3_errmsg(db));
      }
      sqlite3_finalize(texStmt);

      wallTypes[wallKey] = (Wall){.wallKey = wallKey, .wallTex = {{{0}}}};

      // Explicitly copy the textures
      memcpy(wallTypes[wallKey].wallTex, wallTex, 4 * sizeof(WallTexture));
      printf("Wall %d created\n", wallKey);
    }
  } else {
    printf("Error preparing wall query: %s\n", sqlite3_errmsg(db));
  }
  sqlite3_finalize(wallStmt);
  return wallTypes;
}

void loadMap(sqlite3 *db, char *table, Map *map) {
  // Buffers to hold queries
  char mapQuery[256];

  // Format Map Query
  snprintf(mapQuery, sizeof(mapQuery),
           "SELECT x, y, tile_key, tile_style, wall_key FROM %s;", table);
  sqlite3_stmt *mapStmt;

  // Create map grid
  if (sqlite3_prepare_v2(db, mapQuery, -1, &mapStmt, NULL) == SQLITE_OK) {
    memset(map->grid, 0, sizeof(map->grid));
    memset(map->edges, 0, sizeof(map->edges));
    memset(map->walls, 0, sizeof(map->walls));
    memset(map->wallCount, 0, sizeof(map->wallCount));
    memset(map->edgeCount, 0, sizeof(map->edgeCount));

    while (sqlite3_step(mapStmt) == SQLITE_ROW) {
      int x = sqlite3_column_int(mapStmt, 0);
      int y = sqlite3_column_int(mapStmt, 1);
      int tileKey = sqlite3_column_int(mapStmt, 2);
      int tileStyle = sqlite3_column_int(mapStmt, 3);
      int wallKey = sqlite3_column_int(mapStmt, 4);

      if (x >= 0 && x < GRID_SIZE && y >= 0 && y < GRID_SIZE) {
        map->grid[x][y][0] = tileKey;   // Store tile_key
        map->grid[x][y][1] = tileStyle; // Store tile_style
        map->grid[x][y][2] = wallKey;   // Store wall_key
      } else {
        printf("Warning: Map coordinate (%d, %d) out of bounds.\n", x, y);
      }
    }
  } else {
    printf("Error preparing SQL query: %s\n", sqlite3_errmsg(db));
  }
  sqlite3_finalize(mapStmt);
  map->name = (const char *)table;
  printf("Map table \"%s\" successfully loaded\n", map->name);
}

void saveMap(sqlite3 *db, char *table, Map *map) {

  // Buffer to hold query
  char dropQuery[256];
  char createQuery[256];
  char insertQuery[256];

  sqlite3_stmt *dropStmt;
  sqlite3_stmt *createStmt;
  sqlite3_stmt *insertStmt;

  snprintf(dropQuery, sizeof(dropQuery), "DROP TABLE IF EXISTS %s;", table);
  snprintf(createQuery, sizeof(createQuery),
           "CREATE TABLE IF NOT EXISTS %s("
           "x INTEGER NOT NULL,"
           "y INTEGER NOT NULL,"
           "tile_key INTEGER NOT NULL,"
           "tile_style INTEGER NOT NULL,"
           "wall_key INTEGER NOT NULL);",
           table);
  snprintf(insertQuery, sizeof(insertQuery),
           "INSERT INTO %s (x, y, tile_key, tile_style, wall_key) VALUES (?, "
           "?, ?, ?, ?);",
           table);

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

    // Begin transaction for faster inserts
    sqlite3_exec(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    for (int x = 0; x < GRID_SIZE; x++) {
      for (int y = 0; y < GRID_SIZE; y++) {
        // Save non-empty cells (tileKey != 0 or potentially wallKey != 0)
        if (map->grid[x][y][0] != 0 || map->grid[x][y][2] != 0) {
          int tileKey = map->grid[x][y][0];
          int tileStyle = map->grid[x][y][1];
          int wallKey = map->grid[x][y][2];
          sqlite3_bind_int(insertStmt, 1, x);         // Bind x
          sqlite3_bind_int(insertStmt, 2, y);         // Bind y
          sqlite3_bind_int(insertStmt, 3, tileKey);   // Bind tile_key
          sqlite3_bind_int(insertStmt, 4, tileStyle); // Bind tile_style
          sqlite3_bind_int(insertStmt, 5, wallKey);   // Bind wall_key

          if (sqlite3_step(insertStmt) != SQLITE_DONE) {
            printf("Error inserting map data at (%d, %d): %s\n", x, y,
                   sqlite3_errmsg(db));
          }
          sqlite3_reset(insertStmt); // Reset for next iteration

          // Bindings automatically cleared by reset in recent versions, but
          // explicit clear is safe.
          sqlite3_clear_bindings(insertStmt);
        }
      }
    }
    // End transaction
    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    printf("Map table \"%s\" successfully saved.\n", table);
  } else {
    return;
  }
  sqlite3_finalize(insertStmt); // Finalize insert statement if it was prepared
}
