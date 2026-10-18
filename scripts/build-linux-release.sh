#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

IMAGE_NAME="energy-crisis-arcade:debian12"
DIST_DIR="$ROOT_DIR/dist"

echo "=========================================================="
echo " Building Portable Arcade Release for NixOS (DHO Arcade)"
echo " Base Image: Debian 12 (Bookworm) glibc 2.36 target"
echo "=========================================================="

echo "==> 1. Building Docker image: $IMAGE_NAME..."
docker build -f docker/Dockerfile -t "$IMAGE_NAME" .

echo "==> 2. Extracting release artifacts from container..."
CONTAINER_ID=$(docker create "$IMAGE_NAME")
trap 'docker rm -f "$CONTAINER_ID" >/dev/null 2>&1 || true' EXIT

rm -rf "$DIST_DIR/Energy_Crisis"
mkdir -p "$DIST_DIR"

docker cp "$CONTAINER_ID:/release/Energy_Crisis" "$DIST_DIR/Energy_Crisis"
docker cp "$CONTAINER_ID:/release/Energy_Crisis.zip" "$DIST_DIR/Energy_Crisis.zip"
cp -f "$DIST_DIR/Energy_Crisis.zip" "$ROOT_DIR/Energy_Crisis.zip"
cp -f "$DIST_DIR/Energy_Crisis.zip" "$ROOT_DIR/Energy_Crysis.zip"
cp -f "$DIST_DIR/Energy_Crisis.zip" "$DIST_DIR/Energy_Crisis-0.1.2.zip"

# Point host ./lib to the clean portable release libraries
ln -sfn "dist/Energy_Crisis/lib" "$ROOT_DIR/lib"

# Create transitional platform uploader package
PLATFORM_STAGE="$ROOT_DIR/build/platform_stage"
rm -rf "$PLATFORM_STAGE"
mkdir -p "$PLATFORM_STAGE/StandaloneLinux64"
cp -r "$DIST_DIR/Energy_Crisis"/* "$PLATFORM_STAGE/StandaloneLinux64/"
mkdir -p "$PLATFORM_STAGE/StandaloneLinux64/Energy_Crisis_Data"
if [ -f "$ROOT_DIR/game_release.json" ]; then
    cp -f "$ROOT_DIR/game_release.json" "$PLATFORM_STAGE/game_release.json"
fi
if [ -f "$ROOT_DIR/capability_manifest.json" ]; then
    cp -f "$ROOT_DIR/capability_manifest.json" "$PLATFORM_STAGE/capability_manifest.json"
fi
if [ -f "$DIST_DIR/Energy_Crisis/thumbnail.png" ]; then
    cp -f "$DIST_DIR/Energy_Crisis/thumbnail.png" "$PLATFORM_STAGE/thumbnail.png"
fi
(cd "$PLATFORM_STAGE" && zip -r -9 -q "$DIST_DIR/Energy_Crisis-0.1.2-platform.zip" ./*)
rm -rf "$PLATFORM_STAGE"

echo "==> 3. Host-side GLIBC validation..."
python3 "$SCRIPT_DIR/check_glibc_symbols.py" "$DIST_DIR/Energy_Crisis"

echo "==> 4. Validating archive structure..."
unzip -l "$ROOT_DIR/Energy_Crisis.zip"

echo "=========================================================="
echo " Build successful!"
echo " Release directory: $DIST_DIR/Energy_Crisis"
echo " Release archive:   $ROOT_DIR/Energy_Crisis.zip ($(du -h "$ROOT_DIR/Energy_Crisis.zip" | cut -f1))"
echo "=========================================================="
