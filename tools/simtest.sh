#!/bin/bash
# Run an autotest script in the iOS Simulator build and collect its screenshots.
# Usage: tools/simtest.sh <script.txt> <seconds> [--install]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$ROOT/local.env" ] && { set -a; . "$ROOT/local.env"; set +a; }  # this Mac's settings (local.env.example)
C="$ROOT/maplewasm"; [ -d "$C" ] || C="$ROOT/client"  # client/ in the GitHub repo
export DEVELOPER_DIR="${XCODE_DEV:-/Applications/Xcode.app/Contents/Developer}"
SIM="${SIM:-booted}"
BUNDLE=com.flyingviet.maplev83
OUT="${OUT:-/tmp/mv83-shots}"
xcrun simctl boot "$SIM" 2>/dev/null || true
xcrun simctl bootstatus "$SIM" -b >/dev/null 2>&1
if [[ " $* " == *" --install "* ]]; then
  xcrun simctl install "$SIM" "${APP:-$C/build-sim/Debug-iphonesimulator/MapleV83.app}"
fi
D="$(xcrun simctl get_app_container "$SIM" "$BUNDLE" data)"
mkdir -p "$D/Documents"
for f in "$ROOT"/data/nx/*.nx; do [ -e "$D/Documents/$(basename "$f")" ] || cp -c "$f" "$D/Documents/"; done
xcrun simctl terminate "$SIM" "$BUNDLE" 2>/dev/null || true
rm -rf "$D/Documents/shots"; cp "$1" "$D/Documents/autotest.txt"
xcrun simctl launch "$SIM" "$BUNDLE" >/dev/null
sleep "$2"
mkdir -p "$OUT"; rm -f "$OUT"/*.png
cp "$D/Documents/shots/"*.png "$OUT/" 2>/dev/null || true
for p in "$OUT"/*.png; do [ -e "$p" ] && sips -Z 1100 "$p" --out "$p" >/dev/null; done
grep -v -E "^loading val|^after val" "$D/Documents/client.log" | tail -${TAIL:-25}
ls "$OUT"
