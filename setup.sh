#!/bin/sh

# Create map table
sqlite3 test.db <<'END_SQL'
DROP TABLE IF EXISTS map;
CREATE TABLE IF NOT EXISTS map(
    x INTEGER NOT NULL, 
    y INTEGER NOT NULL, 
    tile_key INTEGER NOT NULL);
END_SQL

for x in $(seq 0 10); do
    for y in $(seq 0 14); do
        sqlite3 test.db "INSERT INTO map (x, y, tile_key) VALUES ($x, $y, 0);"
    done
done

# Update some map tiles to different tile types
sqlite3 test.db <<'END_SQL'
UPDATE map SET tile_key = 1 WHERE x = 5 AND y = 8;
UPDATE map SET tile_key = 2 WHERE x = 6 AND y = 9;
UPDATE map SET tile_key = 3 WHERE x = 7 AND y = 10;
UPDATE map SET tile_key = 4 WHERE x = 8 AND y = 11;
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
           (4, 0, 'Blue water tile', 0x0000FFFF); 

SELECT COUNT(*) FROM tile;
END_SQL
