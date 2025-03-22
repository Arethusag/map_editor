// database.h
#ifndef DATABASE_H
#define DATABASE_H

// variables
#define MAX_TILE_VARIANTS 20
#define TILE_SIZE 32
#define GRID_SIZE 16

// includes
#include <raylib.h>
#include <sqlite3.h>

// structs
typedef struct Edge {
  int tileKey;
  Texture2D edges[12];
} Edge;

typedef struct Tile { // ground tiles
  int tileKey;
  int walkable;
  Texture2D tex[MAX_TILE_VARIANTS];
  int texCount;
  int edgePriority;
  int edgeIndicator;
} Tile;

typedef struct Map {
  const char *name;
  // x, y, 1: tileKey, 2: tileStyle 3: wallKey
  int grid[GRID_SIZE][GRID_SIZE][3];
  // 12 possible types of ground edges
  Texture2D edges[GRID_SIZE][GRID_SIZE][12];
  // 2 possible types of wall edges
  Texture2D wallEdges[GRID_SIZE][GRID_SIZE][2];
  int edgeCount[GRID_SIZE][GRID_SIZE];
} Map;

// globals
extern int countEdges;
extern int maxTileKey;
extern Map currentMap;

// Function prototypes
Edge *loadEdges(sqlite3 *db);
Tile *loadTiles(sqlite3 *db);
void loadMap(sqlite3 *db, const char *table);
void saveMap(sqlite3 *db, char *table);

#endif // DATABASE_H
