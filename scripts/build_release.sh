#!/bin/bash
# Build script for Lander release on macOS
# Creates a signed .app bundle ready for itch.io distribution

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build-release"
DIST_DIR="$PROJECT_DIR/dist"
APP_NAME="Lander"
APP_BUNDLE="$DIST_DIR/$APP_NAME.app"

echo "=== Lander Release Build ==="
echo "Project: $PROJECT_DIR"
echo ""

# Step 1: Create directories
echo "Step 1: Creating directories..."
mkdir -p "$BUILD_DIR"
mkdir -p "$DIST_DIR"
mkdir -p "$DIST_DIR/icons"

# Step 2: Build and run icon generator
echo "Step 2: Generating icons..."
cd "$BUILD_DIR"

# Compile icon generator
clang++ -std=c++17 -O2 \
    "$PROJECT_DIR/tools/generate_icon.cpp" \
    -o generate_icon

./generate_icon "$DIST_DIR/icons"

# Step 3: Create .icns file from PNGs
echo "Step 3: Creating .icns file..."
ICONSET_DIR="$DIST_DIR/lander.iconset"
mkdir -p "$ICONSET_DIR"

# Copy icons to iconset with proper naming
cp "$DIST_DIR/icons/icon_16x16.png" "$ICONSET_DIR/icon_16x16.png"
cp "$DIST_DIR/icons/icon_32x32.png" "$ICONSET_DIR/icon_16x16@2x.png"
cp "$DIST_DIR/icons/icon_32x32.png" "$ICONSET_DIR/icon_32x32.png"
cp "$DIST_DIR/icons/icon_64x64.png" "$ICONSET_DIR/icon_32x32@2x.png"
cp "$DIST_DIR/icons/icon_128x128.png" "$ICONSET_DIR/icon_128x128.png"
cp "$DIST_DIR/icons/icon_256x256.png" "$ICONSET_DIR/icon_128x128@2x.png"
cp "$DIST_DIR/icons/icon_256x256.png" "$ICONSET_DIR/icon_256x256.png"
cp "$DIST_DIR/icons/icon_512x512.png" "$ICONSET_DIR/icon_256x256@2x.png"
cp "$DIST_DIR/icons/icon_512x512.png" "$ICONSET_DIR/icon_512x512.png"
cp "$DIST_DIR/icons/icon_1024x1024.png" "$ICONSET_DIR/icon_512x512@2x.png"

# Convert to .icns
iconutil -c icns "$ICONSET_DIR" -o "$DIST_DIR/lander.icns"
rm -rf "$ICONSET_DIR"

echo "Created lander.icns"

# Step 4: Build release binary
echo "Step 4: Building release binary..."
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release "$PROJECT_DIR"
make -j$(sysctl -n hw.ncpu) lander

# Step 5: Create app bundle structure
echo "Step 5: Creating app bundle..."
rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS"
mkdir -p "$APP_BUNDLE/Contents/Resources"
mkdir -p "$APP_BUNDLE/Contents/Frameworks"

# Copy binary
cp "$BUILD_DIR/lander" "$APP_BUNDLE/Contents/MacOS/"

# Copy Info.plist
cp "$PROJECT_DIR/macos/Info.plist" "$APP_BUNDLE/Contents/"

# Copy icon
cp "$DIST_DIR/lander.icns" "$APP_BUNDLE/Contents/Resources/"

# Copy sounds directory
cp -r "$PROJECT_DIR/sounds" "$APP_BUNDLE/Contents/Resources/"

# Create PkgInfo
echo -n "APPL????" > "$APP_BUNDLE/Contents/PkgInfo"

# Step 5b: Bundle SDL2 framework
echo "Step 5b: Bundling SDL2..."

# Find SDL2 dylib path from the binary
SDL2_PATH=$(otool -L "$APP_BUNDLE/Contents/MacOS/lander" | grep -o '/.*libSDL2.*\.dylib' | head -1)
if [ -z "$SDL2_PATH" ]; then
    echo "ERROR: Could not find SDL2 library path"
    exit 1
fi
echo "Found SDL2 at: $SDL2_PATH"

# Copy SDL2 dylib to Frameworks
SDL2_DYLIB_NAME=$(basename "$SDL2_PATH")
cp "$SDL2_PATH" "$APP_BUNDLE/Contents/Frameworks/$SDL2_DYLIB_NAME"

# Update the binary to use the bundled SDL2
install_name_tool -change "$SDL2_PATH" "@executable_path/../Frameworks/$SDL2_DYLIB_NAME" \
    "$APP_BUNDLE/Contents/MacOS/lander"

# Fix the SDL2 dylib's install name
install_name_tool -id "@executable_path/../Frameworks/$SDL2_DYLIB_NAME" \
    "$APP_BUNDLE/Contents/Frameworks/$SDL2_DYLIB_NAME"

echo "SDL2 bundled successfully"

# Step 6: Sign the app bundle
echo "Step 6: Signing app bundle..."

# First, check if we have a Developer ID
SIGNING_IDENTITY=""
if security find-identity -v -p codesigning 2>/dev/null | grep -q "Developer ID Application"; then
    SIGNING_IDENTITY=$(security find-identity -v -p codesigning 2>/dev/null | grep "Developer ID Application" | head -1 | awk -F'"' '{print $2}')
    echo "Found Developer ID: $SIGNING_IDENTITY"
else
    echo "No Developer ID found, using ad-hoc signing"
    SIGNING_IDENTITY="-"
fi

# Remove any existing signatures
codesign --remove-signature "$APP_BUNDLE/Contents/Frameworks/$SDL2_DYLIB_NAME" 2>/dev/null || true
codesign --remove-signature "$APP_BUNDLE" 2>/dev/null || true

# Sign SDL2 framework first (inner components must be signed before outer)
echo "Signing SDL2 framework..."
codesign --force --sign "$SIGNING_IDENTITY" \
    --options runtime \
    --timestamp \
    "$APP_BUNDLE/Contents/Frameworks/$SDL2_DYLIB_NAME"

# Sign the main app bundle
echo "Signing main app..."
codesign --force --sign "$SIGNING_IDENTITY" \
    --options runtime \
    --timestamp \
    "$APP_BUNDLE"

# Verify signature
echo "Verifying signature..."
codesign --verify --deep --strict "$APP_BUNDLE"
echo "Signature verified!"

# Step 7: Create zip for distribution
echo "Step 7: Creating distribution archive..."
cd "$DIST_DIR"
rm -f "Lander-macOS.zip"
ditto -c -k --keepParent "$APP_NAME.app" "Lander-macOS.zip"

echo ""
echo "=== Build Complete ==="
echo "App bundle: $APP_BUNDLE"
echo "Distribution zip: $DIST_DIR/Lander-macOS.zip"
echo ""
echo "To test: open \"$APP_BUNDLE\""
echo ""
echo "After testing, distribute with:"
echo "  butler push \"$DIST_DIR/Lander-macOS.zip\" kranzky/lander:mac"
