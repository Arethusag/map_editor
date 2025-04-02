#!/bin/bash

DB_FILE="test.db"
GRID_SIZE=16

# Define Tile Keys for clarity
TILE_DEFAULT=0
TILE_GRASS=1
TILE_WATER=2
TILE_DIRT=3
TILE_ROCK=4
TILE_PATH=5

# --- Define Wall Keys ---
WALL_DEFAULT=0
WALL_STONE_V=1
WALL_STONE_H=2
WALL_STONE_POST=3
WALL_STONE_CORNER=4
WALL_WINDOW_V=5
WALL_WINDOW_H=6
WALL_ARCH_V=7
WALL_ARCH_H=8

# Temporary file to store SQL commands
SQL_FILE="populate_map.sql"

# Start building the SQL commands
echo "BEGIN TRANSACTION;" > "$SQL_FILE"
for ((x=0; x<GRID_SIZE; x++)); do
    for ((y=0; y<GRID_SIZE; y++)); do
        echo "REPLACE INTO map (x, y, tile_key, tile_style, wall_key) VALUES ($x, $y, $TILE_DEFAULT, 0, 0);" >> "$SQL_FILE"
    done
done


# Layer 1: Water (Outer edge, width 2)
# x < 2 OR x >= GRID_SIZE - 2 OR y < 2 OR y >= GRID_SIZE - 2
for ((x=0; x<GRID_SIZE; x++)); do
    for ((y=0; y<GRID_SIZE; y++)); do
        if (( x < 2 || x >= GRID_SIZE - 2 || y < 2 || y >= GRID_SIZE - 2 )); then
            echo "UPDATE map SET tile_key = $TILE_WATER WHERE x = $x AND y = $y;" >> "$SQL_FILE"
        fi
    done
done

# Layer 2: Grass (Next inner, width 3)
# Must be *inside* water layer: x>=2 AND x<GRID_SIZE-2 AND y>=2 AND y<GRID_SIZE-2
# And *on the edge* of this inner area: x < 5 OR x >= GRID_SIZE - 5 OR y < 5 OR y >= GRID_SIZE - 5
for ((x=2; x<GRID_SIZE-2; x++)); do
    for ((y=2; y<GRID_SIZE-2; y++)); do
        if (( x < 5 || x >= GRID_SIZE - 5 || y < 5 || y >= GRID_SIZE - 5 )); then
            echo "UPDATE map SET tile_key = $TILE_GRASS WHERE x = $x AND y = $y;" >> "$SQL_FILE"
        fi
    done
done

# Layer 3: Dirt (Next inner, width 2)
# Must be *inside* grass layer: x>=5 AND x<GRID_SIZE-5 AND y>=5 AND y<GRID_SIZE-5
# And *on the edge* of this inner area: x < 7 OR x >= GRID_SIZE - 7 OR y < 7 OR y >= GRID_SIZE - 7
for ((x=5; x<GRID_SIZE-5; x++)); do
    for ((y=5; y<GRID_SIZE-5; y++)); do
        if (( x < 7 || x >= GRID_SIZE - 7 || y < 7 || y >= GRID_SIZE - 7 )); then
            echo "UPDATE map SET tile_key = $TILE_DIRT WHERE x = $x AND y = $y;" >> "$SQL_FILE"
        fi
    done
done

# Layer 4: Rock (Remaining middle)
# Must be *inside* dirt layer: x>=7 AND x<GRID_SIZE-7 AND y>=7 AND y<GRID_SIZE-7
for ((x=7; x<GRID_SIZE-7; x++)); do
    for ((y=7; y<GRID_SIZE-7; y++)); do
        echo "UPDATE map SET tile_key = $TILE_ROCK WHERE x = $x AND y = $y;" >> "$SQL_FILE"
    done
done

# Layer 5: Path (Overwrite in center bottom)
# Path starts from y=9 (just below the rock layer which ends at y=8)
# Path extends to the edge (y=15)
# Path is centered horizontally (x=7 and x=8 for a 2-wide path in the 16x16 grid)
path_x1=7
path_x2=8
path_start_y=9 # Start below the rock/dirt layer

for ((y=path_start_y; y<GRID_SIZE; y++)); do
    echo "UPDATE map SET tile_key = $TILE_PATH WHERE x = $path_x1 AND y = $y;" >> "$SQL_FILE"
    echo "UPDATE map SET tile_key = $TILE_PATH WHERE x = $path_x2 AND y = $y;" >> "$SQL_FILE"
done

# --- Add Building ---
# Small 4x3 building located at x=6-9, y=6-8
# Placed after terrain layers, overwriting wall_key where needed.

# Building Coordinates
b_x_start=6
b_y_start=6
b_x_end=9
b_y_end=8

# Top row (y=6)
echo "UPDATE map SET wall_key = $WALL_STONE_POST WHERE x = $b_x_start AND y = $b_y_start;" >> "$SQL_FILE"
echo "UPDATE map SET wall_key = $WALL_WINDOW_H WHERE x = $((b_x_start + 1)) AND y = $b_y_start;" >> "$SQL_FILE" # Arch/Door
echo "UPDATE map SET wall_key = $WALL_STONE_H WHERE x = $((b_x_start + 2)) AND y = $b_y_start;" >> "$SQL_FILE"
echo "UPDATE map SET wall_key = $WALL_STONE_H WHERE x = $b_x_end AND y = $b_y_start;" >> "$SQL_FILE"
#
# # Middle row (y=7)
echo "UPDATE map SET wall_key = $WALL_STONE_V WHERE x = $b_x_start AND y = $((b_y_start + 1));" >> "$SQL_FILE"
echo "UPDATE map SET wall_key = $WALL_WINDOW_V WHERE x = $b_x_end AND y = $((b_y_start + 1));" >> "$SQL_FILE"
#
# # Bottom row (y=8)
echo "UPDATE map SET wall_key = $WALL_STONE_V WHERE x = $b_x_start AND y = $b_y_end;" >> "$SQL_FILE"
echo "UPDATE map SET wall_key = $WALL_ARCH_H WHERE x = $((b_x_start + 1)) AND y = $b_y_end;" >> "$SQL_FILE"
echo "UPDATE map SET wall_key = $WALL_ARCH_H WHERE x = $((b_x_start + 2)) AND y = $b_y_end;" >> "$SQL_FILE"
echo "UPDATE map SET wall_key = $WALL_STONE_CORNER WHERE x = $b_x_end AND y = $b_y_end;" >> "$SQL_FILE"

# --- End Transaction ---
echo "COMMIT;" >> "$SQL_FILE"

# --- Execute the SQL commands ---
echo "Populating database from $SQL_FILE..."
sqlite3 "$DB_FILE" < "$SQL_FILE"

# --- Clean up the temporary SQL file ---
rm "$SQL_FILE"

echo "Map population complete."

