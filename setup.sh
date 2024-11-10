#!/bin/sh

# Create 255x255 map
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

sqlite3 test.db "SELECT COUNT(*) FROM map;"

# Create tile types
sqlite3 test.db <<'END_SQL'
DROP TABLE IF EXISTS tile;
CREATE TABLE IF NOT EXISTS tile(
    tile_key INTEGER PRIMARY KEY,
    walkable INTEGER DEFAULT 1 CHECK(walkable IN (0,1)),
    description STRING NON NULL,
    color STRING NON NULL
) WITHOUT rowid;

INSERT INTO tile (tile_key, walkable, description, color)
    VALUES (0, 0, 'White default tile', '#FFFFFF'), 
           (1, 1, 'Green grass tile', '#00FF00'),
           (2, 1, 'Brown path tile', '#8B4513'),
           (3, 0, 'Grey rock tile', '#808080'),
           (4, 0, 'Blue water tile', '#0000FF'); 

SELECT COUNT(*) FROM tile;
END_SQL
