#!/usr/bin/env bash
# Build, codesign, notarise and staple Lander for macOS, and zip it for itch.io.
#
#   ./scripts/build_release.sh
#
# Prerequisites (one-time):
#   - A "Developer ID Application" certificate in your login keychain.
#   - A notarytool credential profile (the one shared with Lineage/MultiMouse):
#       xcrun notarytool store-credentials lineage-notary \
#         --apple-id <you@example.com> --team-id 869X25LQ6F --password <app-specific-pw>
#
# Output: dist/Lander.app and dist/Lander-macOS.zip. Builds without a signing
# identity are ad-hoc signed and zipped as Lander-macOS-unsigned.zip, so they
# can't be mistaken for a release.
#
# Environment overrides:
#   SIGN_IDENTITY="Developer ID Application: ..."  signing identity
#   NOTARY_PROFILE=lineage-notary                   notarytool keychain profile
#   SKIP_NOTARIZE=1   sign but don't notarise (fast local check)
#   UNSIGNED=1        ad-hoc sign only (automatic when the identity is missing)
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-release"
DIST_DIR="$PROJECT_DIR/dist"
APP_BUNDLE="$DIST_DIR/Lander.app"
VERSION="$(sed -nE 's/^project\(lander VERSION ([0-9.]+).*/\1/p' "$PROJECT_DIR/CMakeLists.txt")"

SIGN_IDENTITY="${SIGN_IDENTITY:-Developer ID Application: Jason Hutchens (869X25LQ6F)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-lineage-notary}"

# Decide signed vs unsigned before spending time on the build
UNSIGNED="${UNSIGNED:-0}"
if [ "$UNSIGNED" != "1" ] && ! security find-identity -v -p codesigning 2>/dev/null | grep -qF "$SIGN_IDENTITY"; then
    echo "WARNING: signing identity not found in the keychain:"
    echo "           $SIGN_IDENTITY"
    echo "         Building UNSIGNED (ad-hoc). Set SIGN_IDENTITY or install the certificate."
    UNSIGNED=1
fi
SUFFIX=""
[ "$UNSIGNED" = "1" ] && SUFFIX="-unsigned"
ZIP="$DIST_DIR/Lander-macOS$SUFFIX.zip"

echo "=== Lander $VERSION macOS Release Build ==="

echo "==> Generate icons"
mkdir -p "$BUILD_DIR" "$DIST_DIR/icons"
clang++ -std=c++17 -O2 "$PROJECT_DIR/tools/generate_icon.cpp" -o "$BUILD_DIR/generate_icon"
"$BUILD_DIR/generate_icon" "$DIST_DIR/icons"

ICONSET_DIR="$DIST_DIR/lander.iconset"
rm -rf "$ICONSET_DIR"
mkdir -p "$ICONSET_DIR"
for size in 16 32 128 256 512; do
    double=$((size * 2))
    cp "$DIST_DIR/icons/icon_${size}x${size}.png" "$ICONSET_DIR/icon_${size}x${size}.png"
    cp "$DIST_DIR/icons/icon_${double}x${double}.png" "$ICONSET_DIR/icon_${size}x${size}@2x.png"
done
iconutil -c icns "$ICONSET_DIR" -o "$DIST_DIR/lander.icns"
rm -rf "$ICONSET_DIR"

echo "==> Build release binary"
cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --target lander -j "$(sysctl -n hw.ncpu)"

echo "==> Assemble app bundle"
rm -rf "$APP_BUNDLE"
mkdir -p "$APP_BUNDLE/Contents/MacOS" "$APP_BUNDLE/Contents/Resources" "$APP_BUNDLE/Contents/Frameworks"
BINARY="$APP_BUNDLE/Contents/MacOS/lander"
cp "$BUILD_DIR/lander" "$BINARY"
cp "$DIST_DIR/lander.icns" "$APP_BUNDLE/Contents/Resources/"
cp -R "$PROJECT_DIR/sounds" "$APP_BUNDLE/Contents/Resources/"
printf 'APPL????' > "$APP_BUNDLE/Contents/PkgInfo"

# Info.plist takes its version from CMakeLists.txt, and its minimum macOS
# version from the binary (Homebrew SDL2 targets the build machine's macOS)
PLIST="$APP_BUNDLE/Contents/Info.plist"
cp "$PROJECT_DIR/macos/Info.plist" "$PLIST"
MIN_MACOS="$(otool -l "$BINARY" | awk '/minos/ { print $2; exit }')"
/usr/libexec/PlistBuddy \
    -c "Set :CFBundleShortVersionString $VERSION" \
    -c "Set :CFBundleVersion $VERSION" \
    -c "Set :LSMinimumSystemVersion $MIN_MACOS" \
    "$PLIST"

echo "==> Bundle SDL2"
SDL2_PATH="$(otool -L "$BINARY" | grep -o '/.*libSDL2.*\.dylib' | head -1)"
if [ -z "$SDL2_PATH" ]; then
    echo "ERROR: Could not find SDL2 library path"
    exit 1
fi
SDL2_DYLIB="$APP_BUNDLE/Contents/Frameworks/$(basename "$SDL2_PATH")"
cp "$SDL2_PATH" "$SDL2_DYLIB"
chmod u+w "$SDL2_DYLIB"
install_name_tool -change "$SDL2_PATH" "@executable_path/../Frameworks/$(basename "$SDL2_PATH")" "$BINARY"
install_name_tool -id "@executable_path/../Frameworks/$(basename "$SDL2_PATH")" "$SDL2_DYLIB"

# Inner code first, then the bundle. Hardened runtime and a secure timestamp
# are required for notarisation; no entitlements are needed.
if [ "$UNSIGNED" = "1" ]; then
    echo "==> Codesign (ad-hoc, UNSIGNED build)"
    codesign --force --sign - "$SDL2_DYLIB"
    codesign --force --sign - "$APP_BUNDLE"
else
    echo "==> Codesign ($SIGN_IDENTITY)"
    codesign --force --options runtime --timestamp --sign "$SIGN_IDENTITY" "$SDL2_DYLIB"
    codesign --force --options runtime --timestamp --sign "$SIGN_IDENTITY" "$APP_BUNDLE"
fi
codesign --verify --deep --strict --verbose=2 "$APP_BUNDLE"

make_zip() {
    # ditto keeps the bundle's symlinks, executable bits and stapled ticket
    rm -f "$DIST_DIR"/Lander-macOS*.zip
    ditto -c -k --norsrc --noextattr --noacl --keepParent "$APP_BUNDLE" "$ZIP"
}

if [ "$UNSIGNED" = "1" ]; then
    make_zip
    echo "==> UNSIGNED build (not for release): $ZIP"
    exit 0
fi

if [ "${SKIP_NOTARIZE:-0}" = "1" ]; then
    make_zip
    echo "==> SKIP_NOTARIZE=1: signed but not notarised: $ZIP"
    exit 0
fi

echo "==> Notarise (submits to Apple and waits)"
make_zip
xcrun notarytool submit "$ZIP" --keychain-profile "$NOTARY_PROFILE" --wait

echo "==> Staple"
xcrun stapler staple "$APP_BUNDLE"
xcrun stapler validate "$APP_BUNDLE"
spctl --assess --type execute --verbose=4 "$APP_BUNDLE"

# Re-zip so the download carries the stapled ticket and opens cleanly offline
make_zip

echo ""
echo "=== Build Complete ==="
echo "App bundle: $APP_BUNDLE"
echo "Distribution zip: $ZIP"
echo ""
echo "To publish:"
echo "  butler push \"$ZIP\" kranzky/lander:mac --userversion $VERSION"
