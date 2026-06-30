#!/bin/sh
# Start HyBBX only. Build and install with CMake (see README).
#
# Installed:  /usr/local/hybbx/bin/hybbx-start   (system)
#             $HOME/hybbx/bin/hybbx-start         (user)
# Dev tree:   ./scripts/hybbx.sh
set -e

self="${0#./}"
case "$self" in
    /*) script_path="$self" ;;
    *)  script_path="$(pwd)/$self" ;;
esac
script_dir="$(cd "$(dirname "$script_path")" && pwd)"
script_name="$(basename "$script_path")"

# Installed layout: <prefix>/bin/hybbx-start + <prefix>/bin/hybbx
if [ "$script_name" = "hybbx-start" ] && [ -x "$script_dir/hybbx" ]; then
    prefix="$(cd "$script_dir/.." && pwd)"
    config="${HYBBX_CONFIG:-$prefix/etc/hybbx.ini}"
    binary="$script_dir/hybbx"
    cd "$prefix"
    if [ ! -f "$config" ]; then
        echo "Config not found: $config" >&2
        exit 1
    fi
    exec "$binary" -c "$config"
fi

# Explicit install root (either default location)
if [ -n "$HYBBX_ROOT" ]; then
    prefix="$HYBBX_ROOT"
elif [ -x "/usr/local/hybbx/bin/hybbx" ]; then
    prefix="/usr/local/hybbx"
elif [ -n "$HOME" ] && [ -x "$HOME/hybbx/bin/hybbx" ]; then
    prefix="$HOME/hybbx"
else
    prefix=""
fi

if [ -n "$prefix" ]; then
    config="${HYBBX_CONFIG:-$prefix/etc/hybbx.ini}"
    binary="$prefix/bin/hybbx"
    if [ -x "$binary" ] && [ -f "$config" ]; then
        cd "$prefix"
        exec "$binary" -c "$config"
    fi
fi

# Development tree (repo checkout)
root="$(cd "$script_dir/.." && pwd)"
config="${HYBBX_CONFIG:-$root/local/hybbx.ini}"
build="$root/build/src/hybbx"

if [ -f "$config" ] && [ -x "$build" ]; then
    cd "$root"
    exec "$build" -c "$config"
fi

echo "HyBBX not found." >&2
echo "Build with CMake:" >&2
echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release" >&2
echo "  cmake --build build" >&2
echo "Install (system):" >&2
echo "  sudo cmake --install build --prefix /usr/local/hybbx" >&2
echo "Install (user):" >&2
echo "  cmake --install build --prefix \"\$HOME/hybbx\"" >&2
echo "Then start:" >&2
echo "  /usr/local/hybbx/bin/hybbx-start" >&2
echo "  \$HOME/hybbx/bin/hybbx-start" >&2
exit 1
