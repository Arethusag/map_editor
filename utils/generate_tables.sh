#!/bin/bash
config_file="./assets/wall_sprites.conf"
DB_FILE="test.db"

while IFS= read -r line || [[ -n "$line" ]]; do
    [[ -z "$line" || "$line" == \#* ]] && continue
    
    line=$(echo "$line" | tr -d ' ')

    if [[ "$line" == @* ]]; then
        header="${line#@}"
    fi
    
    if [[ $header == "assets/otsp_walls_01.png" && "${line#@}" != $header ]]; then
        IFS=: read -r range orientation_key orientation_description wall_group_key wall_group_description wall_type_key wall_type_description wall_key <<< "$line"

        echo "Inserting into wall table: wall_key: $wall_key orientation_key: $orientation_key wall_group_key: $wall_group_key wall_type_key: $wall_type_key"
        sqlite3 "$DB_FILE" "INSERT INTO wall (wall_key, orientation_key, wall_group_key, wall_type_key)
                           VALUES ($wall_key, $orientation_key, $wall_group_key, $wall_type_key);"

        sqlite3 "$DB_FILE" "INSERT OR IGNORE INTO orientation (orientation_key, description)
                           VALUES ($orientation_key, '$orientation_description');"

        sqlite3 "$DB_FILE" "INSERT OR IGNORE INTO wall_type (wall_type_key, description)
                           VALUES ($wall_type_key, '$wall_type_description');"
        
        sqlite3 "$DB_FILE" "INSERT OR IGNORE INTO wall_group (wall_group_key, description)
                           VALUES ($wall_group_key, '$wall_type_description');"

        IFS=, 
        quadrant_key=0
        quadrant_description_mapping=("NW", "NE", "SW", "SE")
        primary_wall_quadrant_indicator_mapping=(0 0 0 1)

        for wall_quadrant_key in $range; do
            quadrant_description=${quadrant_description_mapping[(($quadrant_key))]}
            primary_wall_quadrant_indicator=${primary_wall_quadrant_indicator_mapping[(($quadrant_key))]}
            ((quadrant_key++))
            echo "Inserting into wall_quadrant_table: wall_quadrant_key: $wall_quadrant_key wall_key: $wall_key quadrant_key: $quadrant_key primary_wall_quadrant_indicator: $primary_wall_quadrant_indicator"
            sqlite3 "$DB_FILE" "INSERT INTO wall_quadrant (
                wall_quadrant_key,
                wall_key,
                quadrant_key,
                primary_wall_quadrant_indicator
            ) VALUES (
                $wall_quadrant_key, 
                $wall_key, 
                $quadrant_key, 
                $primary_wall_quadrant_indicator
            );"

            sqlite3 "$DB_FILE" "INSERT OR IGNORE INTO quadrant (quadrant_key, description)
                               VALUES ($quadrant_key, '$quadrant_description');"
        done
    fi 

done < "$config_file"
