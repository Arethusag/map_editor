#!/bin/bash
DB_FILE="test.db"
GRID_SIZE=16

# Create map table
sqlite3 "$DB_FILE" <<'END_SQL'
DROP TABLE IF EXISTS map;
CREATE TABLE IF NOT EXISTS map(
    x INTEGER NOT NULL, 
    y INTEGER NOT NULL, 
    tile_key INTEGER NOT NULL,
    tile_style INTEGER NOT NULL,
    wall_key INTEGER NOT NULL,
    FOREIGN KEY (tile_key) REFERENCES tile(tile_key),
    FOREIGN KEY (wall_key) REFERENCES wall(wall_key),
    PRIMARY KEY (x, y)
);

END_SQL

# Create tile table and insert records
sqlite3 "$DB_FILE" <<'END_SQL'
DROP TABLE IF EXISTS tile;
CREATE TABLE IF NOT EXISTS tile(
    tile_key INTEGER PRIMARY KEY,
    walkable INTEGER DEFAULT 1 CHECK(walkable IN (0,1)),
    description STRING NOT NULL,
    edge_indicator INTEGER NOT NULL CHECK(edge_indicator IN (0, 1)),
    edge_priority INTEGER NOT NULL CHECK(edge_priority IN (1, 2, 3, 4))
) WITHOUT rowid;
INSERT INTO tile (tile_key, walkable, description, edge_indicator, edge_priority)
    VALUES (0, 0, 'default'    , 0, 1),
           (1, 0, 'grass'      , 1, 4),
           (2, 0, 'water'      , 0, 1),
           (3, 0, 'dirt'       , 1, 3),
           (4, 0, 'rock'       , 0, 1),
           (5, 0, 'stone'      , 1, 2)
; 
END_SQL

# Create wall table and insert records
sqlite3 "$DB_FILE" <<'END_SQL'
DROP TABLE IF EXISTS wall;
CREATE TABLE IF NOT EXISTS wall(
    wall_key INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    orientation NOT NULL
) WITHOUT rowid;
INSERT INTO wall (wall_key, name, orientation)
    VALUES (0, 'default', 'N/A')
END_SQL

# Create wall type table and insert records
sqlite3 "$DB_FILE" <<'END_SQL'
DROP TABLE IF EXISTS wall_quadrant;
CREATE TABLE IF NOT EXISTS wall_quadrant(
    wall_quadrant_key INTEGER PRIMARY KEY,
    wall_key INTEGER NOT NULL,
    quadrant_key INTEGER NOT NULL,
    quadrant_description TEXT NOT NULL,
    primary_quadrant_indicator INT CHECK(primary_quadrant_indicator IN (0,1)),
    FOREIGN KEY (wall_key) REFERENCES wall_type(wall_key)
) WITHOUT rowid;
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
    tile_key INTEGER,
    wall_quadrant_key INTEGER,
    data BLOB,
    FOREIGN KEY (tile_key) REFERENCES tile(tile_key),
    FOREIGN KEY (wall_quadrant_key) REFERENCES wall_quadrant(wall_quadrant_key)
);
END_SQL

# Function to generate a single RGBA color value
generate_color() {
    # Generate (R,G,B,A) and combine them
    R=$(printf "%02X" $((255))) 
    G=$(printf "%02X" $((0)))
    B=$(printf "%02X" $((255)))
    A="00"  # Fixed alpha value of 0 (fully transparent)
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
sqlite3 "$DB_FILE" "INSERT INTO texture (texture_key, type, name, style, source, tile_key, wall_quadrant_key, data) 
    VALUES (0, 'tile', 'default', 0, 'setup.sh', 0, 0, readfile('$FILE')) ;"

sqlite3 "$DB_FILE" <<'END_SQL'
SELECT texture_key, type, name, style, source, length(data) as data_length,
    hex(substr(data, 1, 16)) as first_16_bytes_hex FROM texture;
END_SQL
rm -f $FILE

# Process remaining tiles
bash utils/extract_sprites.sh

# Generate tables
bash utils/generate_tables.sh

# Insert sample map
bash utils/insert_sample_map.sh
