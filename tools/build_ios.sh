#!/bin/bash
# Build the v83 iOS client (maplewasm/src/client + src/platform/iphone; CMake -> Xcode), archive, export an ad hoc (release-testing) IPA,
# and optionally publish it to the OTA/Builds page.
# Usage: tools/build_ios.sh [--publish] [--no-data]      VERSION=0.1.0 overrides the marketing version.
# By default the 3.9 GB of v83 NX data (data/nx) goes inside the app, so the phone needs no download;
# --no-data makes a small build that downloads the data from tools/data_server.py on first launch.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$ROOT/local.env" ] && { set -a; . "$ROOT/local.env"; set +a; }  # this Mac's settings (local.env.example)
C="$ROOT/maplewasm"; [ -d "$C" ] || C="$ROOT/client"  # client/ in the GitHub repo
B="$ROOT/build"; NAME=MapleV83
# SDL2's UIKit shell predates UIScene; the Xcode 27 SDK enforces it, so build with 26.x.
export DEVELOPER_DIR="${XCODE_DEV:-/Applications/Xcode.app/Contents/Developer}"
VERSION="${VERSION:-0.2.0}"
# Above the 480 builds of the earlier LocalStory-based client.
BUILD="$((1000 + $(git -C "$C" rev-list --count HEAD)))"
TEAM="${MV83_TEAM_ID:?set MV83_TEAM_ID (your Apple team ID) in local.env}"
mkdir -p "$B"

cmake -S "$C/src/platform/iphone" -B "$B/ios" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_XCODE_GENERATE_SCHEME=ON -DCMAKE_XCODE_ATTRIBUTE_SKIP_INSTALL=YES \
  -DMAPLE_VERSION="$VERSION" -DMAPLE_BUILD="$BUILD" -DMAPLE_TEAM_ID="$TEAM" > "$B/cmake.log" 2>&1 \
  || { tail -20 "$B/cmake.log"; exit 1; }

rm -rf "$B/dd" "$B/$NAME.xcarchive" "$B/export"
xcodebuild -project "$B/ios/MapleV83.xcodeproj" -scheme MapleV83 -configuration Release \
  -destination generic/platform=iOS -derivedDataPath "$B/dd" -archivePath "$B/$NAME.xcarchive" \
  -allowProvisioningUpdates DEVELOPMENT_TEAM="$TEAM" archive > "$B/archive.log" 2>&1 \
  || { grep -E "error:" "$B/archive.log" | head -20; tail -5 "$B/archive.log"; exit 1; }

# The data goes into the archived app (APFS clones, no extra disk) so the export signs it with the app.
WITH_DATA=1; [[ " $* " == *" --no-data "* ]] && WITH_DATA=0
if [ "$WITH_DATA" = 1 ]; then
  APPDIR="$B/$NAME.xcarchive/Products/Applications/$NAME.app"
  mkdir -p "$APPDIR/nx"
  for f in Base Character Effect Etc Item Map Mob Morph Npc Quest Reactor Skill Sound String TamingMob UI; do
    [ -s "$ROOT/data/nx/$f.nx" ] || { echo "missing data/nx/$f.nx - run tools/convert_data.sh first"; exit 1; }
    cp -c "$ROOT/data/nx/$f.nx" "$APPDIR/nx/"
  done
  # Hashes + versions of the bundled set: the app downloads only files that later change on the Mac.
  /usr/bin/python3 "$ROOT/tools/data_server.py" --print-manifest > "$APPDIR/nx/manifest.json"
  echo "Bundled $(du -sh "$APPDIR/nx" | cut -f1) of NX data"
fi

cat > "$B/ExportOptions.plist" <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>method</key><string>release-testing</string><key>signingStyle</key><string>automatic</string>
<key>teamID</key><string>$TEAM</string><key>compileBitcode</key><false/><key>thinning</key><string>&lt;none&gt;</string>
</dict></plist>
PL
xcodebuild -exportArchive -archivePath "$B/$NAME.xcarchive" -exportOptionsPlist "$B/ExportOptions.plist" \
  -exportPath "$B/export" -allowProvisioningUpdates > "$B/exportipa.log" 2>&1 || { tail -20 "$B/exportipa.log"; exit 1; }
IPA="$(ls "$B"/export/*.ipa)"; echo "IPA: $IPA ($(du -h "$IPA" | cut -f1)) version $VERSION ($BUILD)"
rm -rf "$B/dd"

if [[ " $* " == *" --publish "* ]]; then
  COMMIT="$(git -C "$C" rev-parse --short HEAD)"
  if [ "$WITH_DATA" = 1 ]; then
    NOTE="${NOTE:-MapleStory v83 client with the game data built in, for the Cosmic server on the Mac}"
    KEEP=1   # each build carries ~3.9 GB; keep only the newest on the Builds page
  else
    NOTE="${NOTE:-MapleStory v83 client (downloads the game data from the Mac) for the Cosmic server}"
    KEEP=2
  fi
  [ -n "${MV83_OTA_PUBLISH:-}" ] || { echo "set MV83_OTA_PUBLISH in local.env to publish"; exit 1; }
  python3 "$MV83_OTA_PUBLISH" "$IPA" --title "Maple v83" --note "$NOTE; commit $COMMIT" --commit "$COMMIT" --keep "$KEEP"
fi
