#!/bin/sh

# Create map table
sqlite3 test.db <<'END_SQL'
DROP TABLE IF EXISTS map;
CREATE TABLE IF NOT EXISTS map(
    x INTEGER NOT NULL, 
    y INTEGER NOT NULL, 
    tile_key INTEGER NOT NULL);
END_SQL

# Update some map tiles to different tile types
sqlite3 test.db <<'END_SQL'
INSERT INTO map (x, y, tile_key) VALUES (10,10,0);
INSERT INTO map (x, y, tile_key) VALUES (10,11,1);
INSERT INTO map (x, y, tile_key) VALUES (10,12,2);
INSERT INTO map (x, y, tile_key) VALUES (10,13,3);
INSERT INTO map (x, y, tile_key) VALUES (10,14,4);
END_SQL

sqlite3 test.db "SELECT COUNT(*) FROM map;"

# Create tile table
sqlite3 test.db <<'END_SQL'
DROP TABLE IF EXISTS tile;
CREATE TABLE IF NOT EXISTS tile(
    tile_key INTEGER PRIMARY KEY,
    walkable INTEGER DEFAULT 1 CHECK(walkable IN (0,1)),
    description STRING NON NULL,
    color INTEGER NON NULL
) WITHOUT rowid;

INSERT INTO tile (tile_key, walkable, description, color)
    VALUES (0, 0, 'White default tile', 0xFFFFFFFF), 
           (1, 1, 'Green grass tile', 0x00FF00FF),
           (2, 1, 'Brown path tile', 0x8B4513FF),
           (3, 0, 'Grey rock tile', 0x808080FF),
           (4, 0, 'Blue water tile', 0x5033FFFF); 

END_SQL
