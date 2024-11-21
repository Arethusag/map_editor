#!/bin/bash

DB_FILE="test.db"

# Create map table
sqlite3 "$DB_FILE" <<'END_SQL'
DROP TABLE IF EXISTS map;
CREATE TABLE IF NOT EXISTS map(
    x INTEGER NOT NULL, 
    y INTEGER NOT NULL, 
    tile_key INTEGER NOT NULL,
    FOREIGN KEY (tile_key) REFERENCES tile(tile_key)
);

END_SQL


GRID_SIZE=16

# Insert default tile 0 into all map grid cells
for ((x=0; x<GRID_SIZE; x++)); do
    for ((y=0; y<GRID_SIZE; y++)); do
        sqlite3 "$DB_FILE" "INSERT INTO map (x, y, tile_key) VALUES ($x,$y,$((0)));"
    done
done

sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM map;"

# Create tile table and insert records
sqlite3 "$DB_FILE" <<'END_SQL'
DROP TABLE IF EXISTS tile;
CREATE TABLE IF NOT EXISTS tile(
    tile_key INTEGER PRIMARY KEY,
    walkable INTEGER DEFAULT 1 CHECK(walkable IN (0,1)),
    description STRING NOT NULL,
    texture_key INTEGER NOT NULL,
    FOREIGN KEY (texture_key) REFERENCES texture(texture_key)
) WITHOUT rowid;
INSERT INTO tile (tile_key, walkable, description, texture_key)
    VALUES (0, 0, 'default', 0),
           (1, 0, 'grass'  , 1),
           (2, 0, 'water'  , 41),
           (3, 0, 'dirt'   , 28),
           (4, 0, 'rock'   , 21),
           (5, 0, 'stone'  , 36)
; 

END_SQL

# Create texture table
sqlite3 "$DB_FILE" <<'END_SQL'
DROP TABLE IF EXISTS texture;
CREATE TABLE IF NOT EXISTS texture(
    texture_key INTEGER PRIMARY KEY,
    type TEXT NOT NULL,
    name TEXT NOT NULL,
    style INTEGER NOT NULL,
    source TEXT NOT NULL,
    data BLOB
);
END_SQL

# Function to generate a single RGBA color value
generate_color() {
    # Generate (R,G,B,A) and combine them
    R=$(printf "%02X" $((255))) 
    G=$(printf "%02X" $((0)))
    B=$(printf "%02X" $((255)))
    A="FF"  # Fixed alpha value of 255 (fully opaque)
    echo "0x${R}${G}${B}${A}"
}

# Calculate total number of pixels
TEXTURE_SIZE=32
TOTAL_PIXELS=$((TEXTURE_SIZE * TEXTURE_SIZE))

# Generate files and insert into SQLite
FILE="texture.bin"

color=$(generate_color)

# Generate pixels and convert to binary
for ((i=0; i<TOTAL_PIXELS; i++)); do
    printf "%b" "\x${color:2:2}\x${color:4:2}\x${color:6:2}\x${color:8:2}" >> "$FILE"
done

echo "Generated $TOTAL_PIXELS pixels in $FILE"
echo "File size: $(wc -c < "$FILE") bytes"

# Insert the binary data into SQLite
sqlite3 "$DB_FILE" "INSERT INTO texture (texture_key, type, name, style, source, data) 
    VALUES (0, 'tile', 'default', 0, 'setup.sh', readfile('$FILE')) ;"

sqlite3 "$DB_FILE" <<'END_SQL'
SELECT texture_key, type, name, style, source, length(data) as data_length,
    hex(substr(data, 1, 16)) as first_16_bytes_hex FROM texture;
END_SQL
rm -f $FILE

# Process remaining tiles
bash utils/extract_sprites.sh
