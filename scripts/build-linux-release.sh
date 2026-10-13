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
cp "$DIST_DIR/Energy_Crisis.zip" "$ROOT_DIR/Energy_Crisis.zip"

echo "==> 3. Host-side GLIBC validation..."
python3 "$SCRIPT_DIR/check_glibc_symbols.py" "$DIST_DIR/Energy_Crisis"

echo "==> 4. Validating archive structure..."
unzip -l "$ROOT_DIR/Energy_Crisis.zip"

echo "=========================================================="
echo " Build successful!"
echo " Release directory: $DIST_DIR/Energy_Crisis"
echo " Release archive:   $ROOT_DIR/Energy_Crisis.zip ($(du -h "$ROOT_DIR/Energy_Crisis.zip" | cut -f1))"
echo "=========================================================="
