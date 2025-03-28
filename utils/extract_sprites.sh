#!/bin/bash
cd $(git rev-parse --show-toplevel)

# Example config file format
# @assets/tileset1.png"     # Specify input file first with @file
# 2-21:tile:grass:1    # Sequential format <start>-<end>:<type>:<name>:<tile_key>
config_file='assets/sprites.conf'
db_file='test.db'
current_file=""

# Function to initialize file processing
init_file() {
    local input_file=$1
    
    if [ ! -f "$input_file" ]; then
        echo "Error: Image file '$input_file' not found!" 
        return 1
    fi

    if [[ ! $(file -b --mime-type "$input_file") == "image/png" ]]; then
        echo "Error: '$input_file' must be a PNG image!"
        return 1
    fi

    dimensions=$(identify -format "%wx%h" "$input_file")
    width=$(echo $dimensions | cut -d'x' -f1)
    height=$(echo $dimensions | cut -d'x' -f2)

    cols=$((width / 32))
    rows=$((height / 32))
    total_tiles=$((cols * rows))

    echo "Processing '$input_file'"
    echo "Image dimensions: ${width}x${height}"
    echo "Grid size: ${cols}x${rows} (${total_tiles} total tiles)"
    
    return 0
}

pad_number() {
    printf "%02d" "$1"
}

# Function to extract scprite and store in database
extract_sprite() {
    local style=$1
    local type=$2
    local name=$3
    local source=$4
    local sprite_pos=$5
    local key_value=$6

    local tile_key="NULL"
    local wall_quadrant_key="NULL"

    if [[ "$source" == *"tiles"* ]]; then
        tile_key=$key_value
    elif [[ "$source" == *"walls"* ]]; then
        wall_quadrant_key=$key_value
    fi

    local x=$(( (sprite_pos - 1) % cols ))
    local y=$(( (sprite_pos - 1) / cols ))
    
    local crop_x=$((x * 32))
    local crop_y=$((y * 32))
    
    local base_name="${type}_${name}_style$(pad_number $style)"
    local bin_file="${base_name}.bin"
    
    # Extract and convert to binary RGBA format
    magick "$source" -crop "32x32+${crop_x}+${crop_y}" +repage -depth 8 RGBA:"$bin_file"
    
    # Verify binary file size
    local bin_size=$(stat -f%z "$bin_file" 2>/dev/null || stat -c%s "$bin_file")
    if [ "$bin_size" -ne 4096 ]; then
        echo "Warning: Binary file size for ${base_name} is $bin_size bytes (expected 4096 bytes)"
        return 1
    fi
    
    # Insert into database
    sqlite3 "$db_file" << EOF
INSERT INTO texture (type, name, style, source, tile_key, wall_quadrant_key, data)
VALUES (
    '${type}',
    '${name}',
    ${style},
    '${source}',
    ${tile_key},
    ${wall_quadrant_key},
    readfile('${bin_file}')
);
EOF
    
    echo "Processed: Tile $tile_num → ${base_name} (stored in database)"
    
    # Clean up binary file
    rm "$bin_file"
}

# Process config file
while IFS= read -r line || [[ -n "$line" ]]; do
    [[ -z "$line" || "$line" == \#* ]] && continue
    
    line=$(echo "$line" | tr -d ' ')
    
    if [[ "$line" == @* ]]; then
        current_file="${line#@}"
        if ! init_file "$current_file"; then
            current_file=""
            continue
        fi
        continue
    fi
    
    if [ -z "$current_file" ]; then
        echo "Warning: No input file selected, skipping line: $line"
        continue
    fi
    
    IFS=: read -r range type name key_value <<< "$line"
    
    echo "Processing $type $name from $current_file..."

    if [[ "$range" == *-* ]]; then # Check if it's a range (contains '-')
      
        start=$(echo "$range" | cut -d'-' -f1)
        end=$(echo "$range" | cut -d'-' -f2)

        if [ "$start" -gt "$total_tiles" ] || [ "$end" -gt "$total_tiles" ]; then
            echo "Warning: Range $start-$end exceeds total tiles ($total_tiles)"
            return 1 # Indicate an error
        fi

        style_counter=1
        for i in $(seq "$start" "$end"); do
            extract_sprite "$style_counter" "$type" "$name" "$current_file" "$i" "$key_value"
            style_counter=$((style_counter+1))
        done
    
    elif [[ "$range" == *,* ]]; then # Check if it's a list (contains ',')

        style_counter=1
        IFS=',' read -r -a tiles <<< "$range"

        for tile in "${tiles[@]}"; do

            tile=$(echo "$tile" | xargs) # trim whitespace
            if [ "$tile" -gt "$total_tiles" ]; then
                echo "Warning: Tile $tile exceeds total tiles ($total_tiles)"
                return 1 # Indicate an error
            fi
            extract_sprite "$style_counter" "$type" "$name" "$current_file" "$tile" "$tile"
            style_counter=$((style_counter+1))
        done

    else #single number
        if [ "$range" -gt "$total_tiles" ]; then
            echo "Warning: Tile $range exceeds total tiles ($total_tiles)"
            return 1 # Indicate an error
        fi

        extract_sprite "1" "$type" "$name" "$current_file" "$range" "$key_value"

    fi

done < "$config_file"

# Generate database summary
echo ""
echo "Processing complete! Database summary:"
sqlite3 "$db_file" << 'END_SQL'
SELECT 
    type,
    name,
    COUNT(*) as count,
    source
FROM texture 
GROUP BY type, name, source
ORDER BY type, name;

SELECT 
    COUNT(*) as total_textures,
    COUNT(DISTINCT type) as unique_types,
    COUNT(DISTINCT name) as unique_names,
    COUNT(DISTINCT source) as source_files,
    COUNT(DISTINCT COALESCE(tile_key, wall_quadrant_key)) as unique_objects
FROM texture;
END_SQL

