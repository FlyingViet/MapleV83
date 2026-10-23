#!/bin/bash
# Fetch the pinned iOS dependency sources into src/platform/iphone/deps (not committed).
set -euo pipefail
D="$(cd "$(dirname "$0")" && pwd)/deps"; mkdir -p "$D"
fetch() { [ -d "$D/$1" ] || git clone -q --depth 1 --branch "$3" "$2" "$D/$1"; }
fetch SDL2 https://github.com/libsdl-org/SDL.git release-2.32.10
fetch freetype https://github.com/freetype/freetype.git VER-2-14-3
fetch asio https://github.com/chriskohlhoff/asio.git asio-1-12-2
echo "iOS deps ready in $D"
