#!/bin/bash
# Copy the converted v83 NX files into the iOS app's Documents folder.
# Normally unnecessary: the app downloads them from tools/data_server.py on first launch.
# Usage: tools/push_data.sh            # the iPhone MV83_DEVICE in local.env (USB or same Wi-Fi, app installed)
#        tools/push_data.sh --sim      # the booted Simulator
#        tools/push_data.sh --device <UDID>
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$ROOT/local.env" ] && { set -a; . "$ROOT/local.env"; set +a; }  # this Mac's settings (local.env.example)
NX="$ROOT/data/nx"
BUNDLE=com.flyingviet.maplev83
DEVICE="${MV83_DEVICE:-}"
# Base.nx (13 KB) goes last: the app starts loading once every file exists, so the
# set only completes after all the large files are fully copied.
FILES=(Character Effect Etc Item Map Mob Morph Npc Quest Reactor Skill Sound String TamingMob UI Base)

for f in "${FILES[@]}"; do [ -s "$NX/$f.nx" ] || { echo "missing $NX/$f.nx - run tools/convert_data.sh first"; exit 1; }; done

if [ "${1:-}" = "--sim" ]; then
  DATA="$(xcrun simctl get_app_container booted "$BUNDLE" data)"
  mkdir -p "$DATA/Documents"
  for f in "${FILES[@]}"; do cp -c "$NX/$f.nx" "$DATA/Documents/$f.nx" 2>/dev/null || cp "$NX/$f.nx" "$DATA/Documents/$f.nx"; done
  echo "Copied ${#FILES[@]} files to the Simulator."
  exit 0
fi
[ "${1:-}" = "--device" ] && DEVICE="${2:?UDID}"
[ -n "$DEVICE" ] || { echo "set MV83_DEVICE (iPhone UDID) in local.env or pass --device <UDID>"; exit 1; }

for f in "${FILES[@]}"; do
  echo "  $f.nx ($(du -h "$NX/$f.nx" | cut -f1))"
  xcrun devicectl device copy to --device "$DEVICE" --domain-type appDataContainer \
    --domain-identifier "$BUNDLE" --source "$NX/$f.nx" --destination "Documents/$f.nx" >/dev/null
done
echo "Copied ${#FILES[@]} files to the iPhone. Relaunch the app if it was waiting for data."
