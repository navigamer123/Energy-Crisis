#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "==> Delegating arcade package build to Docker-based release build..."
exec "$SCRIPT_DIR/build-linux-release.sh" "$@"
