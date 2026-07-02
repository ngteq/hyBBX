#!/bin/sh
# Start HyBBX only. Build and install with CMake (see README).
#
# Installed:  /usr/local/hybbx/hybbx-start   (system, prefix /usr/local)
#             $HOME/hybbx/hybbx-start         (user, prefix $HOME)
# Dev tree:   ./scripts/hybbx.sh
set -e

self="${0#./}"
case "$self" in
    /*) script_path="$self" ;;
    *)  script_path="$(pwd)/$self" ;;
esac
script_dir="$(cd "$(dirname "$script_path")" && pwd)"
script_name="$(basename "$script_path")"

# Installed layout: <prefix>/hybbx/hybbx-start + <prefix>/hybbx/hybbx
if [ "$script_name" = "hybbx-start" ] && [ -x "$script_dir/hybbx" ]; then
    prefix="$script_dir"
    config="${HYBBX_CONFIG:-$prefix/hybbx.ini}"
    binary="$script_dir/hybbx"
    export HYBBX_ROOT="${HYBBX_ROOT:-$prefix}"
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
elif [ -x "/usr/local/hybbx/hybbx" ]; then
    prefix="/usr/local/hybbx"
elif [ -n "$HOME" ] && [ -x "$HOME/hybbx/hybbx" ]; then
    prefix="$HOME/hybbx"
else
    prefix=""
fi

if [ -n "$prefix" ]; then
    config="${HYBBX_CONFIG:-$prefix/hybbx.ini}"
    binary="$prefix/hybbx"
    if [ -x "$binary" ] && [ -f "$config" ]; then
        export HYBBX_ROOT="${HYBBX_ROOT:-$prefix}"
        cd "$prefix"
        exec "$binary" -c "$config"
    fi
fi

# Development tree (repo checkout)
root="$(cd "$script_dir/.." && pwd)"
config="${HYBBX_CONFIG:-$root/local/hybbx.ini}"
build="$root/build/src/hybbx"

if [ -f "$config" ] && [ -x "$build" ]; then
    export HYBBX_ROOT="${HYBBX_ROOT:-$root}"
    cd "$root"
    exec "$build" -c "$config"
fi

echo "HyBBX not found." >&2
echo "Build with CMake:" >&2
echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release" >&2
echo "  cmake --build build" >&2
echo "Install (system):" >&2
echo "  sudo cmake --install build --prefix /usr/local" >&2
echo "Install (user):" >&2
echo "  cmake --install build --prefix \"\$HOME\"" >&2
echo "Then start:" >&2
echo "  /usr/local/hybbx/hybbx-start" >&2
echo "  \$HOME/hybbx/hybbx-start" >&2
exit 1
