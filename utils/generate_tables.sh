#!/bin/bash
config_file="./assets/sprites.conf"
DB_FILE="test.db"

while IFS= read -r line || [[ -n "$line" ]]; do
    [[ -z "$line" || "$line" == \#* ]] && continue
    
    line=$(echo "$line" | tr -d ' ')

    if [[ "$line" == @* ]]; then
        header="${line#@}"
    fi
    
    if [[ $header == "assets/otsp_walls_01.png" && "${line#@}" != $header ]]; then
        IFS=: read -r range orientation name wall_key <<< "$line"
        echo "Inserting into wall table: " $wall_key $name $orientation
        sqlite3 "$DB_FILE" "INSERT INTO wall (wall_key, name, orientation) VALUES ($wall_key, '$name', '$orientation');"
        IFS=, 
        quadrant_key=0
        quadrant_description_mapping=("NW", "NE", "SW", "SE")
        primary_quadrant_indicator_mapping=(0 0 0 1)

        for wall_quadrant_key in $range; do
            quadrant_description=${quadrant_description_mapping[(($quadrant_key))]}
            primary_quadrant_indicator=${primary_quadrant_indicator_mapping[(($quadrant_key))]}
            ((quadrant_key++))
            echo "Inserting into wall_quadrant_table: " $wall_quadrant_key $wall_key $quadrant_key $quadrant_description $primary_quadrant_indicator
            sqlite3 "$DB_FILE" "INSERT INTO wall_quadrant (wall_quadrant_key, wall_key, quadrant_key, quadrant_description, primary_quadrant_indicator) VALUES ($wall_quadrant_key, $wall_key, $quadrant_key, '$quadrant_description', $primary_quadrant_indicator );"
        done
    fi 
    
done < "$config_file"


