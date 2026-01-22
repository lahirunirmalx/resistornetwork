# Resistor Network Calculator

Find series/parallel resistor combinations to achieve a target resistance.

Given a set of standard resistor values and a target resistance, this tool 
computes all series and parallel combinations (up to 5 resistors) that fall 
within your specified tolerance. Also shows color codes (4-band, 5-band) and 
SMD markings.

![Icon](data/icons/resistorcal.svg)

## Features

- Calculate series/parallel resistor networks
- Support for up to 5 resistors in a network
- Display 4-band and 5-band color codes
- Show SMD (3-digit and 4-digit) markings
- Cross-platform: Linux, Windows, macOS

## Building

### Linux (Debian/Ubuntu)

```bash
# Install dependencies
sudo apt install build-essential cmake pkg-config libgtk-3-dev

# Build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Install (optional)
sudo make install
```

### Linux (Fedora)

```bash
sudo dnf install gcc cmake pkgconfig gtk3-devel
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Linux (Arch)

```bash
sudo pacman -S base-devel cmake gtk3
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Windows (MSYS2)

```bash
# Install MSYS2 from https://www.msys2.org/
# Open MINGW64 terminal

pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-gtk3

mkdir build && cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
mingw32-make
```

### macOS (Homebrew)

```bash
brew install gtk+3 cmake pkg-config

mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Creates resistorcal.app bundle
```

## Generating Icons

Icons are generated from the SVG source. Requires `librsvg2-bin` and `imagemagick`:

```bash
# Linux
sudo apt install librsvg2-bin imagemagick

# Generate all icon formats
./scripts/generate-icons.sh
```

## Usage

Run from project root (development):
```bash
./build/resistorcal
```

Or after install:
```bash
resistorcal
```

1. Select which resistor values you have available
2. Enter target resistance in ohms
3. Select tolerance percentage
4. Click Calculate

The tool shows all networks that achieve the target within tolerance, along with
color codes for single-resistor solutions.

### Color Codes

**4-Band** (5% tolerance): 2 digits + multiplier + gold
- Example: 4.7KΩ → Yellow-Violet-Red-Gold

**5-Band** (1% precision): 3 digits + multiplier + brown
- Example: 4.7KΩ → Yellow-Violet-Black-Brown-Brown

**SMD Codes**:
- 3-digit: `472` = 47 × 10² = 4.7KΩ
- 4-digit: `4701` = 470 × 10¹ = 4.7KΩ
- R notation: `4R7` = 4.7Ω

## Web Version (PWA)

A Progressive Web App is included in `web/` - works offline and can be 
installed on any device:

```bash
# Serve locally (any static server works)
cd web
python3 -m http.server 8080
# Open http://localhost:8080
```

On mobile, use "Add to Home Screen" in your browser to install it.

## Mobile Apps (Android/iOS)

Native mobile apps are built using Capacitor:

```bash
cd mobile
npm install
mkdir -p www && cp -r ../web/* www/
npx cap add android
npx cap add ios
npx cap sync

# Open in Android Studio
npx cap open android

# Open in Xcode (macOS only)
npx cap open ios
```

See `mobile/README.md` for detailed build instructions.

## Packaging

Create distribution packages:

```bash
cd build

# Debian/Ubuntu
cpack -G DEB

# Fedora/RHEL
cpack -G RPM

# macOS
cpack -G DragNDrop

# Windows
cpack -G ZIP
```

## License

MIT - see [LICENSE](LICENSE)
