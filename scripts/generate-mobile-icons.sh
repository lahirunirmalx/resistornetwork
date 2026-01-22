#!/bin/bash
#
# Generate mobile app icons from SVG source
# For PWA, Android (Capacitor), and iOS (Capacitor)
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SVG_FILE="$PROJECT_DIR/data/icons/resistorcal.svg"
WEB_ICONS="$PROJECT_DIR/web/icons"

mkdir -p "$WEB_ICONS"

echo "Generating mobile icons from $SVG_FILE..."

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
    echo "  Created $output"
}

# PWA icons
echo "Creating PWA icons..."
for size in 48 72 96 128 144 152 180 192 256 512; do
    svg_to_png $size "$WEB_ICONS/resistorcal-${size}.png"
done

# Copy SVG
cp "$SVG_FILE" "$WEB_ICONS/resistorcal.svg"

# Android icons (if Capacitor android folder exists)
ANDROID_RES="$PROJECT_DIR/mobile/android/app/src/main/res"
if [ -d "$ANDROID_RES" ]; then
    echo "Creating Android icons..."
    svg_to_png 48 "$ANDROID_RES/mipmap-mdpi/ic_launcher.png"
    svg_to_png 72 "$ANDROID_RES/mipmap-hdpi/ic_launcher.png"
    svg_to_png 96 "$ANDROID_RES/mipmap-xhdpi/ic_launcher.png"
    svg_to_png 144 "$ANDROID_RES/mipmap-xxhdpi/ic_launcher.png"
    svg_to_png 192 "$ANDROID_RES/mipmap-xxxhdpi/ic_launcher.png"
    
    # Round icons
    svg_to_png 48 "$ANDROID_RES/mipmap-mdpi/ic_launcher_round.png"
    svg_to_png 72 "$ANDROID_RES/mipmap-hdpi/ic_launcher_round.png"
    svg_to_png 96 "$ANDROID_RES/mipmap-xhdpi/ic_launcher_round.png"
    svg_to_png 144 "$ANDROID_RES/mipmap-xxhdpi/ic_launcher_round.png"
    svg_to_png 192 "$ANDROID_RES/mipmap-xxxhdpi/ic_launcher_round.png"
    
    # Foreground for adaptive icons
    mkdir -p "$ANDROID_RES/mipmap-anydpi-v26"
    for dpi in mdpi hdpi xhdpi xxhdpi xxxhdpi; do
        svg_to_png 108 "$ANDROID_RES/mipmap-${dpi}/ic_launcher_foreground.png"
    done
fi

# iOS icons (if Capacitor ios folder exists)
IOS_ASSETS="$PROJECT_DIR/mobile/ios/App/App/Assets.xcassets/AppIcon.appiconset"
if [ -d "$IOS_ASSETS" ]; then
    echo "Creating iOS icons..."
    svg_to_png 20 "$IOS_ASSETS/AppIcon-20x20@1x.png"
    svg_to_png 40 "$IOS_ASSETS/AppIcon-20x20@2x.png"
    svg_to_png 60 "$IOS_ASSETS/AppIcon-20x20@3x.png"
    svg_to_png 29 "$IOS_ASSETS/AppIcon-29x29@1x.png"
    svg_to_png 58 "$IOS_ASSETS/AppIcon-29x29@2x.png"
    svg_to_png 87 "$IOS_ASSETS/AppIcon-29x29@3x.png"
    svg_to_png 40 "$IOS_ASSETS/AppIcon-40x40@1x.png"
    svg_to_png 80 "$IOS_ASSETS/AppIcon-40x40@2x.png"
    svg_to_png 120 "$IOS_ASSETS/AppIcon-40x40@3x.png"
    svg_to_png 120 "$IOS_ASSETS/AppIcon-60x60@2x.png"
    svg_to_png 180 "$IOS_ASSETS/AppIcon-60x60@3x.png"
    svg_to_png 76 "$IOS_ASSETS/AppIcon-76x76@1x.png"
    svg_to_png 152 "$IOS_ASSETS/AppIcon-76x76@2x.png"
    svg_to_png 167 "$IOS_ASSETS/AppIcon-83.5x83.5@2x.png"
    svg_to_png 1024 "$IOS_ASSETS/AppIcon-1024x1024@1x.png"
fi

echo "Done!"
