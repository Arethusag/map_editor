#!/bin/bash

# Create map table
sqlite3 test.db <<'END_SQL'
DROP TABLE IF EXISTS map;
CREATE TABLE IF NOT EXISTS map(
    x INTEGER NOT NULL, 
    y INTEGER NOT NULL, 
    tile_key INTEGER NOT NULL,
    FOREIGN KEY (tile_key) REFERENCES tile(tile_key)
);
END_SQL

GRID_SIZE=16
NUM_TILES=5

# Update some map tiles to different tile types
for ((x=0; x<GRID_SIZE; x++)); do
    for ((y=0; y<GRID_SIZE; y++)); do
        sqlite3 test.db "INSERT INTO map (x, y, tile_key) VALUES ($x,$y,$((RANDOM % $NUM_TILES + 1)));"
    done
done



sqlite3 test.db "SELECT COUNT(*) FROM map;"

# Create tile table and insert records
sqlite3 test.db <<'END_SQL'

DROP TABLE IF EXISTS tile;

CREATE TABLE IF NOT EXISTS tile(
    tile_key INTEGER PRIMARY KEY,
    walkable INTEGER DEFAULT 1 CHECK(walkable IN (0,1)),
    description STRING NOT NULL,
    texture_key INTEGER NOT NULL,
    FOREIGN KEY (texture_key) REFERENCES texture(texture_key)
) WITHOUT rowid;

INSERT INTO tile (tile_key, walkable, description, texture_key)
    VALUES (0, 0, 'Random color tile', 0), 
           (1, 0, 'Random color tile', 1), 
           (2, 0, 'Random color tile', 2),
           (3, 0, 'Random color tile', 3),
           (4, 0, 'Random color tile', 4),
           (5, 0, 'Random color tile', 5); 

END_SQL

# Create texture table
sqlite3 test.db <<'END_SQL'
DROP TABLE IF EXISTS texture;
CREATE TABLE IF NOT EXISTS texture(
    texture_key INTEGER PRIMARY KEY,
    data BLOB NOT NULL
);
END_SQL

# Function to generate a single random RGBA color value
generate_color() {
    # Generate 4 random bytes (R,G,B,A) and combine them
    R=$(printf "%02X" $((RANDOM % 256)))
    G=$(printf "%02X" $((RANDOM % 256)))
    B=$(printf "%02X" $((RANDOM % 256)))
    A="FF"  # Fixed alpha value of 255 (fully opaque)
    echo "0x${R}${G}${B}${A}"
}

# Calculate total number of pixels
TEXTURE_SIZE=32
TOTAL_PIXELS=$((TEXTURE_SIZE * TEXTURE_SIZE))

# Number of textures to generate
NUM_TEXTURES=5

# Loop through index to generate files and insert into SQLite
for INDEX in $(seq 0 $NUM_TEXTURES); do
    FILE="texture_${INDEX}.bin"
    rm -f "$FILE"
    color=$(generate_color)

    # Generate pixels and convert to binary
    for ((i=0; i<TOTAL_PIXELS; i++)); do
        printf "%b" "\x${color:2:2}\x${color:4:2}\x${color:6:2}\x${color:8:2}" >> "$FILE"
    done

    echo "Generated $TOTAL_PIXELS pixels in $FILE"
    echo "File size: $(wc -c < "$FILE") bytes"

    # Insert the binary data into SQLite, using the index as the primary key
    sqlite3 test.db "INSERT INTO texture (texture_key, data) VALUES ($INDEX, readfile('$FILE'));"
    rm -f "$FILE"
done

# Verify the insertion
echo "Verifying data in SQLite:"
sqlite3 test.db "SELECT texture_key, length(data) AS size FROM texture;"

