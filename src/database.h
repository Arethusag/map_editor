// database.h
#ifndef DATABASE_H
#define DATABASE_H

#define MAX_TILE_VARIANTS 20
#define MAX_WALL_VARIANTS 4
#define TILE_SIZE 32
#define GRID_SIZE 16

#include <raylib.h>
#include <sqlite3.h>

typedef struct { // ground tile edges
  int tileKey;
  Texture2D edges[12];
} Edge;

typedef struct { // ground tiles
  int tileKey;
  int walkable;
  Texture2D tex[MAX_TILE_VARIANTS];
  int texCount;
  int edgePriority;
  int edgeIndicator;
} Tile;

typedef struct {
  const char *name;
  // x, y, 1: tileKey, 2: tileStyle 3: wallKey
  int grid[GRID_SIZE][GRID_SIZE][3];
  // 12 possible types of ground edges
  Texture2D edges[GRID_SIZE][GRID_SIZE][12];
  // 3 possible types of wall edges
  Texture2D walls[GRID_SIZE][GRID_SIZE][3];
  int edgeCount[GRID_SIZE][GRID_SIZE];
  int wallCount[GRID_SIZE][GRID_SIZE];
  int maxTileKey;
  int maxWallKey;
  int countEdges;
} Map;

typedef struct {
  Texture2D tex;
  int wall_quadrant_key;
  int quadrant_key;
  int primary_wall_quadrant_indicator;
} WallTexture;

typedef struct {
  int wallKey;
  WallTexture wallTex[4];
  int wallGroupKey;
  int wallTypeKey;
  int orientationKey;
} Wall;

typedef struct Entry {
  int sourceWallKey;
  int orientationKey;
  int targetWallKey;
  struct Entry *next;
} Entry;

typedef struct {
  int capacity;
  int size;
  Entry **buckets;
} WallOrientMap;

// Function prototypes
sqlite3 *connectDatabase(void);

Edge *loadEdges(sqlite3 *db, Map *map);

Tile *loadTiles(sqlite3 *db, Map *map);

Wall *loadWalls(sqlite3 *db, Map *map);

void loadMap(sqlite3 *db, char *table, Map *map);

void saveMap(sqlite3 *db, char *table, Map *map);

WallOrientMap *loadWallOrientationsMap(sqlite3 *db);

#endif // DATABASE_H
