#!/bin/bash
set -euo pipefail

echo "=========================================================="
echo " Packaging Energy Crisis for DHO Arcade (Debian 12 Target)"
echo "=========================================================="

BUILD_BIN="/src/Energy_Crisis"
if [ ! -f "$BUILD_BIN" ]; then
    echo "ERROR: Compiled binary not found at $BUILD_BIN" >&2
    exit 1
fi

DEST_DIR="/release/Energy_Crisis"
rm -rf "/release"
mkdir -p "$DEST_DIR/lib"

echo "==> 1. Copying main executable..."
cp "$BUILD_BIN" "$DEST_DIR/Energy_Crisis.x86_64"
chmod +x "$DEST_DIR/Energy_Crisis.x86_64"

echo "==> 2. Resolving and bundling runtime libraries..."
# Blacklist: libraries that MUST be provided by the host NixOS system
EXCLUDE_REGEX="^(libc\.so|ld-linux|libm\.so|libmvec\.so|libdl\.so|libpthread\.so|librt\.so|linux-vdso\.so|libGL\.so|libGLX\.so|libOpenGL\.so|libEGL\.so|libdrm\.so|libglapi\.so)"

collect_deps() {
    local target="$1"
    ldd "$target" 2>/dev/null | awk '{if ($3 ~ /^\//) print $3; else if ($1 ~ /^\//) print $1}' | while read -r libpath; do
        if [ -f "$libpath" ]; then
            local libname
            libname=$(basename "$libpath")
            if echo "$libname" | grep -Eq "$EXCLUDE_REGEX"; then
                continue
            fi
            if [ ! -f "$DEST_DIR/lib/$libname" ]; then
                echo "    + Bundling: $libname (from $libpath)"
                cp -L "$libpath" "$DEST_DIR/lib/$libname"
            fi
        fi
    done
}

# Collect direct dependencies of the executable
collect_deps "$DEST_DIR/Energy_Crisis.x86_64"

# Iteratively collect dependencies of dependencies (closure)
for i in 1 2 3 4; do
    for so in "$DEST_DIR"/lib/*.so*; do
        if [ -f "$so" ] && [ ! -L "$so" ]; then
            collect_deps "$so"
        fi
    done
done

echo "==> Bundled $(ls "$DEST_DIR/lib" | wc -l) shared libraries in lib/."

echo "==> 3. Setting portable RPATH with patchelf..."
for so in "$DEST_DIR"/lib/*.so*; do
    if [ -f "$so" ] && [ ! -L "$so" ] && file "$so" | grep -q "ELF"; then
        patchelf --force-rpath --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
    fi
done
patchelf --force-rpath --set-rpath '$ORIGIN/lib' "$DEST_DIR/Energy_Crisis.x86_64" 2>/dev/null || true

echo "==> 4. Copying game assets..."
cp -r /src/assets "$DEST_DIR/assets"
# Ensure no stale or duplicate libs exist in assets/
rm -rf "$DEST_DIR/assets/libs"

echo "==> 5. Installing metadata, launcher, and thumbnail..."
cp /src/launch.sh "$DEST_DIR/launch.sh"
chmod +x "$DEST_DIR/launch.sh"

cp /src/metadata.json "$DEST_DIR/metadata.json"

if [ -f /src/assets/logo.png ]; then
    cp /src/assets/logo.png "$DEST_DIR/thumbnail.png"
elif [ -f /src/thumbnail.png ]; then
    cp /src/thumbnail.png "$DEST_DIR/thumbnail.png"
fi

cat << 'EOF' > "$DEST_DIR/NOTICE.txt"
Energy Crisis - Third Party Notices & Runtime Licenses

- SFML (Simple and Fast Multimedia Library) 3.1.0
  License: zlib/libpng license
  https://www.sfml-dev.org/license.php

- SQLite
  Dedicated to the Public Domain
  https://www.sqlite.org/copyright.html

- Debian GNU/Linux Bookworm runtime packages
  Redistributed under their respective open-source licenses (GPL, LGPL, BSD, MIT, zlib).
EOF

echo "==> 6. Performing automated GLIBC compatibility validation inside container..."
python3 /src/scripts/check_glibc_symbols.py "$DEST_DIR"

echo "==> 7. Creating release ZIP archive..."
(cd "$DEST_DIR" && zip -r -9 -q /release/Energy_Crisis.zip ./*)

echo "=========================================================="
echo " Packaging complete: /release/Energy_Crisis.zip ($(du -h /release/Energy_Crisis.zip | cut -f1))"
echo "=========================================================="
