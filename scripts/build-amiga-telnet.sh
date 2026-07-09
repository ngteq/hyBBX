#!/usr/bin/env sh
# Cross-build hybbx-telnet for AmigaOS 3.9+ (bebbo-gcc / clib2).
set -eu

ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build-amiga}"
SDK="${AMIGA_SDK_PATH:-/opt/amiga}"

shift 2>/dev/null || true

if [ ! -x "$SDK/bin/m68k-amigaos-gcc" ]; then
    echo "Amiga SDK not found at $SDK (set AMIGA_SDK_PATH)" >&2
    exit 1
fi

cmake -B "$BUILD" \
    -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/Toolchain-AmigaOS.cmake" \
    -DAMIGA_SDK_PATH="$SDK" \
    -DHYBBX_CLIENTS_ONLY=ON \
    -DCMAKE_BUILD_TYPE=Release \
    "$@"

cmake --build "$BUILD" --target hybbx-telnet

echo "AmigaOS client:"
echo "  $BUILD/src/clients/hybbx-telnet"
