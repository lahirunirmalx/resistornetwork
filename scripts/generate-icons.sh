#!/bin/bash
#
# Generate icon files from SVG source
# Requires: inkscape or rsvg-convert, imagemagick (for ICO), iconutil (macOS for ICNS)
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
ICONS_DIR="$PROJECT_DIR/data/icons"
SVG_FILE="$ICONS_DIR/resistorcal.svg"

cd "$ICONS_DIR"

echo "Generating icons from $SVG_FILE..."

# Function to convert SVG to PNG
svg_to_png() {
    local size=$1
    local output=$2
    
    if command -v rsvg-convert >/dev/null 2>&1; then
        rsvg-convert -w "$size" -h "$size" "$SVG_FILE" -o "$output"
    elif command -v inkscape >/dev/null 2>&1; then
        inkscape -w "$size" -h "$size" "$SVG_FILE" -o "$output" 2>/dev/null
    elif command -v convert >/dev/null 2>&1; then
        convert -background none -resize "${size}x${size}" "$SVG_FILE" "$output"
    else
        echo "Error: Need rsvg-convert, inkscape, or imagemagick"
        exit 1
    fi
}

# Generate PNG files for various sizes
for size in 16 24 32 48 64 128 256 512; do
    echo "  Creating ${size}x${size} PNG..."
    svg_to_png $size "resistorcal-${size}.png"
done

# Copy main icon
cp resistorcal-128.png resistorcal.png

# Generate Windows ICO (multi-resolution)
if command -v convert >/dev/null 2>&1; then
    echo "  Creating Windows ICO..."
    convert resistorcal-16.png resistorcal-24.png resistorcal-32.png \
            resistorcal-48.png resistorcal-64.png resistorcal-128.png \
            resistorcal-256.png resistorcal.ico
else
    echo "  Warning: ImageMagick not found, skipping ICO generation"
fi

# Generate macOS ICNS
if command -v iconutil >/dev/null 2>&1; then
    echo "  Creating macOS ICNS..."
    ICONSET="resistorcal.iconset"
    mkdir -p "$ICONSET"
    
    cp resistorcal-16.png "$ICONSET/icon_16x16.png"
    cp resistorcal-32.png "$ICONSET/icon_16x16@2x.png"
    cp resistorcal-32.png "$ICONSET/icon_32x32.png"
    cp resistorcal-64.png "$ICONSET/icon_32x32@2x.png"
    cp resistorcal-128.png "$ICONSET/icon_128x128.png"
    cp resistorcal-256.png "$ICONSET/icon_128x128@2x.png"
    cp resistorcal-256.png "$ICONSET/icon_256x256.png"
    cp resistorcal-512.png "$ICONSET/icon_256x256@2x.png"
    cp resistorcal-512.png "$ICONSET/icon_512x512.png"
    svg_to_png 1024 "$ICONSET/icon_512x512@2x.png"
    
    iconutil -c icns "$ICONSET" -o resistorcal.icns
    rm -rf "$ICONSET"
elif command -v png2icns >/dev/null 2>&1; then
    echo "  Creating macOS ICNS (via png2icns)..."
    png2icns resistorcal.icns resistorcal-16.png resistorcal-32.png \
             resistorcal-128.png resistorcal-256.png resistorcal-512.png
else
    echo "  Warning: iconutil/png2icns not found, skipping ICNS generation"
fi

echo "Done! Generated icons in $ICONS_DIR"
ls -la *.png *.ico *.icns 2>/dev/null || true
