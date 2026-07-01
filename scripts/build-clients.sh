#!/usr/bin/env sh
# Build hybbx-telnet and hybbx-terminal without the hybbx server or plugins.
set -eu

ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build-clients}"

shift 2>/dev/null || true

cmake -B "$BUILD" -DHYBBX_CLIENTS_ONLY=ON "$@"
cmake --build "$BUILD"

echo "Clients:"
echo "  $BUILD/src/clients/hybbx-telnet"
echo "  $BUILD/src/clients/hybbx-terminal"
