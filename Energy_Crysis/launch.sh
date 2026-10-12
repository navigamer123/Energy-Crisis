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
