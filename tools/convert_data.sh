#!/bin/bash
# Convert your own MapleStory v83 client's WZ files into the NX files the iOS client
# reads, then give Cosmic's quests their dialogue from the same data.
#
# Usage: tools/convert_data.sh <v83 client folder | v83 .rar>
#   All 16 files (Base Character Effect Etc Item Map Mob Morph Npc Quest Reactor Skill
#   Sound String TamingMob UI) come from the same v83 client.
# Output: data/nx/*.nx (gitignored; never published anywhere).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -f "$ROOT/local.env" ] && { set -a; . "$ROOT/local.env"; set +a; }  # this Mac's settings (local.env.example)
V83="${1:?usage: convert_data.sh <v83 client folder | v83 .rar>}"
CONV="$ROOT/wztonx/wztonx"
OUT="$ROOT/data/nx"
WORK="$ROOT/data/work"
FILES=(Base Character Effect Etc Item Map Mob Morph Npc Quest Reactor Skill Sound String TamingMob UI)

if [ -f "$V83" ]; then
  # Homebrew's 7z lacks the RAR codec; bsdtar (libarchive) reads it. Only the .wz files.
  echo "Extracting the .wz files from $(basename "$V83"):"
  mkdir -p "$ROOT/data/v83-wz"
  bsdtar -xf "$V83" -C "$ROOT/data/v83-wz" --include '*.wz' --include '*.WZ'
  V83="$ROOT/data/v83-wz"
fi

[ -x "$CONV" ] || { echo "converter missing: build $ROOT/wztonx first"; exit 1; }

find_wz() { # dir name -> path (case-insensitive, a few levels down, e.g. MapleStory/Data/)
  find "$1" -maxdepth 3 -iname "$2.wz" -type f 2>/dev/null | head -1
}

mkdir -p "$OUT" "$WORK"
missing=0
for f in "${FILES[@]}"; do
  [ -n "$(find_wz "$V83" "$f")" ] || { echo "v83: $f.wz not found under $V83"; missing=1; }
done
[ "$missing" = 0 ] || exit 1

convert() { # src.wz name
  local src="$1" name="$2"
  if [ -f "$OUT/$name.nx" ] && [ "$OUT/$name.nx" -nt "$src" ]; then echo "  $name.nx up to date"; return; fi
  rm -f "$WORK/$name.wz" "$WORK/$name.nx"
  ln -s "$src" "$WORK/$name.wz"
  echo "  $name.wz -> $name.nx ($(du -h "$src" | cut -f1))"
  (cd "$WORK" && "$CONV" -c "$name.wz" > "$WORK/$name.log" 2>&1) || { tail -5 "$WORK/$name.log"; exit 1; }
  [ -s "$WORK/$name.nx" ] || { echo "conversion produced nothing for $name"; tail -5 "$WORK/$name.log"; exit 1; }
  mv "$WORK/$name.nx" "$OUT/$name.nx"
  rm -f "$WORK/$name.wz"
}

echo "Converting v83 data:"
for f in "${FILES[@]}"; do convert "$(find_wz "$V83" "$f")" "$f"; done

echo "Generating quest dialogue scripts for Cosmic (never overwrites hand-written ones):"
python3 "$ROOT/tools/gen_quest_scripts.py" "$OUT/Quest.nx" "$ROOT/server/scripts/quest" | tail -3

echo "Restarting the server so it picks up the scripts:"
if [ -n "${MV83_SERVER_AGENT:-}" ]; then
  launchctl kickstart -k "gui/$(id -u)/$MV83_SERVER_AGENT" && sleep 15 && tail -1 ~/Library/Logs/maplev83-server.log
else
  echo "(set MV83_SERVER_AGENT in local.env to restart it here; restart your Cosmic server by hand)"
fi

du -ch "$OUT"/*.nx | tail -1
echo "Done. Next: tools/push_data.sh (iPhone) or tools/push_data.sh --sim"
