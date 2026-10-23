#!/bin/bash
# Build the native macOS (Apple Silicon) client, sign it with the Apple Development
# identity, zip it, and optionally Taildrop it to another Mac (PEER in local.env).
# Usage: tools/build_mac.sh [--send]          VERSION=0.3.0 overrides the marketing version.
# First launch on the other Mac: System Settings > Privacy & Security > Open Anyway (not notarized).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$ROOT/local.env" ] && { set -a; . "$ROOT/local.env"; set +a; }  # this Mac's settings (local.env.example)
C="$ROOT/maplewasm"; [ -d "$C" ] || C="$ROOT/client"  # client/ in the GitHub repo
B="$ROOT/build/mac"
VERSION="${VERSION:-0.3.0}"
BUILD="$((1000 + $(git -C "$C" rev-list --count HEAD)))"
APP="$B/Maple v83.app"
ZIP="$ROOT/build/MapleV83-mac-$VERSION-$BUILD.zip"
PEER="${PEER:-}"
TS=/Applications/Tailscale.app/Contents/MacOS/Tailscale

cmake -S "$C/src/platform/macos" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DMAPLE_VERSION="$VERSION" -DMAPLE_BUILD="$BUILD" > "$B.cmake.log" 2>&1 \
  || { tail -20 "$B.cmake.log"; exit 1; }
cmake --build "$B" > "$B.build.log" 2>&1 || { grep -E "error|FAILED" "$B.build.log" | head -20; exit 1; }

IDENTITY="$(security find-identity -v -p codesigning | awk -F'"' '/Apple Development/ {print $2; exit}')"
codesign --force --deep --sign "${IDENTITY:--}" --timestamp=none "$APP"
codesign --verify --deep --strict "$APP"
echo "Signed with: ${IDENTITY:-ad hoc}"

rm -f "$ZIP"
ditto -c -k --sequesterRsrc --keepParent "$APP" "$ZIP"
echo "App: $APP"
echo "Zip: $ZIP ($(du -h "$ZIP" | cut -f1)) version $VERSION ($BUILD)"

if [[ " $* " == *" --send "* ]]; then
  [ -n "$PEER" ] || { echo "set PEER (Tailscale machine name) in local.env"; exit 1; }
  # The GUI Tailscale CLI is sandboxed and cannot open files itself: pipe the zip in.
  "$TS" file cp --name="$(basename "$ZIP")" - "$PEER:" < "$ZIP" && echo "Taildropped to $PEER"
fi
