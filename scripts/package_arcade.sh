#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "==> Building arcade binary..."
make -C "$ROOT_DIR" arcade

BIN_SRC="$ROOT_DIR/Energy_Crisis"
if [ ! -f "$BIN_SRC" ]; then
    echo "ERROR: Binary $BIN_SRC not found!"
    exit 1
fi

PKG_DIR="$ROOT_DIR/Energy_Crysis"
echo "==> Creating package directory: $PKG_DIR"
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/lib"

echo "==> Copying binary as Energy_Crisis.x86_64..."
cp "$BIN_SRC" "$PKG_DIR/Energy_Crisis.x86_64"
chmod +x "$PKG_DIR/Energy_Crisis.x86_64"

echo "==> Gathering bundled libraries into $PKG_DIR/lib..."
BIN="$PKG_DIR/Energy_Crisis.x86_64"

# Core libraries explicitly required
CORE_LIBS=(
    "libsfml-audio.so.3.1"
    "libsfml-graphics.so.3.1"
    "libsfml-window.so.3.1"
    "libsfml-system.so.3.1"
    "libsqlite3.so.0"
    "libstdc++.so.6"
    "libvorbis.so.0"
    "libvorbisfile.so.3"
    "libvorbisenc.so.2"
    "libogg.so.0"
    "libFLAC.so.14"
    "libfreetype.so.6"
    "libharfbuzz.so.0"
    "libudev.so.1"
)

for libname in "${CORE_LIBS[@]}"; do
    found_path=$(ldd "$BIN" 2>/dev/null | grep "$libname" | awk '{print $3}' | head -n 1 || true)
    if [ -z "$found_path" ] || [ ! -f "$found_path" ]; then
        found_path=$(find /usr/lib /usr/lib64 -name "$libname*" 2>/dev/null | head -n 1 || true)
    fi
    if [ -n "$found_path" ] && [ -f "$found_path" ]; then
        echo "    Copying $libname from $found_path"
        cp -L "$found_path" "$PKG_DIR/lib/$libname"
    fi
done

# Transitive runtime dependencies
for ((i=0; i<3; i++)); do
    ALL_DEPS=$(ldd "$BIN" "$PKG_DIR"/lib/*.so* 2>/dev/null | awk '{if ($3 ~ /^\//) print $3; else if ($1 ~ /^\//) print $1}' | sort -u)
    for dep in $ALL_DEPS; do
        base=$(basename "$dep")
        if [ ! -f "$PKG_DIR/lib/$base" ]; then
            cp -L "$dep" "$PKG_DIR/lib/$base" 2>/dev/null || true
        fi
    done
done

# Ensure ld-linux interpreter is also copied for portable execution on NixOS without /lib64
if [ -f /lib64/ld-linux-x86-64.so.2 ]; then
    cp -L /lib64/ld-linux-x86-64.so.2 "$PKG_DIR/lib/ld-linux-x86-64.so.2" 2>/dev/null || true
fi

# Set DT_RPATH on all bundled libraries using patchelf
if command -v patchelf >/dev/null 2>&1; then
    echo "==> Patching RPATH on bundled shared libraries with patchelf..."
    for so in "$PKG_DIR"/lib/*.so*; do
        if [ -f "$so" ] && [ ! -L "$so" ] && file "$so" | grep -q "ELF"; then
            patchelf --force-rpath --set-rpath '$ORIGIN' "$so" 2>/dev/null || true
        fi
    done
    patchelf --force-rpath --set-rpath '$ORIGIN/lib' "$PKG_DIR/Energy_Crisis.x86_64" 2>/dev/null || true
    patchelf --force-rpath --set-rpath '$ORIGIN/lib' "$BIN_SRC" 2>/dev/null || true
fi

# Copy assets to package (ensure assets/ does NOT contain redundant libs copy)
echo "==> Copying assets to package (clean without duplicated libs)..."
cp -r "$ROOT_DIR/assets" "$PKG_DIR/assets"
rm -rf "$PKG_DIR/assets/libs"

# Ensure root ./lib points to package lib
ln -sfn "$PKG_DIR/lib" "$ROOT_DIR/lib"

echo "==> Creating launch.sh..."
cat << 'EOF' > "$PKG_DIR/launch.sh"
#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

EXE="$DIR/Energy_Crisis.x86_64"

cd "$DIR"

# 1. Standard dynamic linker (works with nix-ld or standard Linux)
if [ -f /lib64/ld-linux-x86-64.so.2 ] && [ -x /lib64/ld-linux-x86-64.so.2 ]; then
    exec "$EXE" "$@"
fi

# 2. NixOS nix-ld environment
if [ -f /run/current-system/sw/share/nix-ld/lib/ld.so ]; then
    exec /run/current-system/sw/share/nix-ld/lib/ld.so "$EXE" "$@"
fi

# 3. Look for ld-linux in /nix/store
NIX_LD=$(ls -d /nix/store/*-glibc-*/lib/ld-linux-x86-64.so.2 2>/dev/null | tail -n 1)
if [ -n "$NIX_LD" ] && [ -x "$NIX_LD" ]; then
    exec "$NIX_LD" --library-path "$DIR/lib:$LD_LIBRARY_PATH" "$EXE" "$@"
fi

# 4. Fallback to bundled ld-linux runtime
if [ -f "$DIR/lib/ld-linux-x86-64.so.2" ]; then
    exec "$DIR/lib/ld-linux-x86-64.so.2" --library-path "$DIR/lib:$LD_LIBRARY_PATH" "$EXE" "$@"
fi

# 5. Direct execution fallback
exec "$EXE" "$@"
EOF
chmod +x "$PKG_DIR/launch.sh"

echo "==> Creating metadata.json..."
cat << 'EOF' > "$PKG_DIR/metadata.json"
{
  "name": "Energy Crisis",
  "id": "energy_crisis",
  "version": "1.0.0",
  "executable": "Energy_Crisis.x86_64",
  "launcher": "launch.sh",
  "description": "Energy Crisis - Post-apocalyptic Renewable Energy Management Game",
  "genre": "Strategy / Arcade",
  "players": 2,
  "controls": {
    "p1": "Joystick 1 / Arcade Buttons",
    "p2": "Joystick 2 / Arcade Buttons"
  }
}
EOF

# Clean any scratch/settings files inside package directory
rm -f "$PKG_DIR/settings.ini"

# Create zip archive where the files are at the root
echo "==> Creating Energy_Crisis.zip..."
rm -f "$ROOT_DIR/Energy_Crisis.zip" "$ROOT_DIR/Energy_Crysis.zip" "$ROOT_DIR/dist/Energy_Crisis.zip"
(cd "$PKG_DIR" && zip -r -9 -q "$ROOT_DIR/Energy_Crisis.zip" ./*)
cp -f "$ROOT_DIR/Energy_Crisis.zip" "$ROOT_DIR/Energy_Crysis.zip"
mkdir -p "$ROOT_DIR/dist"
cp -f "$ROOT_DIR/Energy_Crisis.zip" "$ROOT_DIR/dist/Energy_Crisis.zip"

echo "==> Clean package archive created at:"
echo "    $ROOT_DIR/Energy_Crisis.zip"
