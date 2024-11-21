#!/bin/bash

# Check if ImageMagick is installed
if ! command -v magick &> /dev/null; then
    echo "Error: ImageMagick is not installed."
    exit 1
fi

# Check if a filename was provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <tileset.png>"
    exit 1
fi

input_file="$1"

# Check if input file exists and is PNG
if [ ! -f "$input_file" ] || [[ ! $(file -b --mime-type "$input_file") == "image/png" ]]; then
    echo "Error: Valid PNG file required!"
    exit 1
fi

# Get image dimensions
read width height < <(magick identify -format "%w %h" "$input_file")
cols=$((width / 32))
rows=$((height / 32))

# Create output filename
output_file="${input_file%.*}_grid.png"

# Create drawing commands for all grid lines at once
grid_commands=()
for ((i=0; i<=cols; i++)); do
    x=$((i * 32))
    grid_commands+=(-draw "line $x,0 $x,$height")
done
for ((i=0; i<=rows; i++)); do
    y=$((i * 32))
    grid_commands+=(-draw "line 0,$y $width,$y")
done

# Create text drawing commands
text_commands=()
counter=1
for ((y=0; y<rows; y++)); do
    for ((x=0; x<cols; x++)); do
        text_x=$((x * 32 + 2))
        text_y=$((y * 32 + 12))
        text_commands+=(-fill "rgba(0,0,0,0.6)" -draw "rectangle $((text_x-1)),$((text_y-8)) $((text_x+14)),$((text_y+2))")
        text_commands+=(-fill white -draw "text $text_x,$text_y '$counter'")
        ((counter++))
    done
done

# Apply all transformations in a single command
magick "$input_file" -strokewidth 1 \
    -stroke white "${grid_commands[@]}" \
    -stroke none -pointsize 10 "${text_commands[@]}" \
    "$output_file"

echo "Created grid reference: $output_file"
echo "Grid size: ${cols}x${rows}"
