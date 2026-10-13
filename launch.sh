#!/bin/bash
set -eu

# Resolve the absolute path to the directory containing this launcher script
GAME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$GAME_DIR"

# Prepend the bundled library directory to LD_LIBRARY_PATH while preserving arcade/system paths
if [ -d "$GAME_DIR/lib" ]; then
    export LD_LIBRARY_PATH="$GAME_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

EXECUTABLE="$GAME_DIR/Energy_Crisis.x86_64"

if [ ! -f "$EXECUTABLE" ]; then
    echo "[launch.sh] ERROR: Executable not found: $EXECUTABLE" >&2
    exit 127
fi

if [ ! -x "$EXECUTABLE" ]; then
    chmod +x "$EXECUTABLE" 2>/dev/null || true
fi

# Execute the game binary, forwarding all arguments and replacing shell process
exec "$EXECUTABLE" "$@"
