# MapleV83 — MapleStory v83 private server + native iPhone/Mac client

- `server/` Cosmic v83 ([P0nk/Cosmic](https://github.com/P0nk/Cosmic) fork): Java 21 + MariaDB. Patched to hand
  each client the address it connected through (LAN or Tailscale), plus gameplay changes (rebirth at 300, Aran combos,
  uncapped stats, ...).
- `client/` the client ([sh-islam/maple-wasm-public](https://github.com/sh-islam/maple-wasm-public), an OpenStory fork with
  the pure v83 UI): the iOS layer is `src/platform/iphone` (SDL2 + OpenGL ES 3, touch controls, AVFoundation audio) and
  the native macOS app is `src/platform/macos` (SDL2 + OpenGL 4.1 core, mouse/keyboard, Apple Silicon). Both share
  `PLATFORM_SDL` code paths, the data downloader, audio and gamepad.
- `tools/` data conversion, data server, build and test scripts (below).

Not included: game data. Convert your own GMS v83 client (`tools/convert_data.sh`); its NX files go to `data/nx/`
(gitignored), and never commit or publish them. All three upstreams are AGPL-3.0; see `server/LICENSE` and `client/LICENSE`.

## Setup
1. Server: MariaDB database/user `cosmic`, set `DB_*` in `server/config.yaml` (don't commit your password), then
   `JAVA_HOME=<JDK 21> ./mvnw -q -DskipTests package` in `server/` and run it (e.g. a LaunchAgent; log
   `~/Library/Logs/maplev83-server.log`). Extra SQL tweaks: `tools/server_tweaks.sql`.
2. Data: `tools/convert_data.sh <v83 client folder | v83 .rar>` (needs `wztonx/wztonx`, NoLifeWzToNx built for macOS)
   writes `data/nx/*.nx` and generates quest dialogue scripts for the server (`server/scripts/quest`, not committed).
3. `tools/data_server.py` serves `data/nx` to the apps on port 8485, only to private-network clients (LAN, Tailscale,
   localhost). LaunchAgent template: `tools/maplev83-data.plist.example`.
4. Client: copy `client/src/client/LocalServers.example.h` to `LocalServers.h` (gitignored) with the server Mac's LAN
   and Tailscale addresses; copy `local.env.example` to `local.env` (gitignored) with your Apple team ID and devices.
   For your own bundle ID pass `-DMAPLE_BUNDLE_ID=...` to CMake. Working from separate checkouts, the client may live
   in `maplewasm/` instead of `client/`; the scripts accept either.

## Building
iPhone: `tools/build_ios.sh [--publish] [--no-data]` archives and exports an ad hoc IPA (Xcode 26.x). By default it carries
the 3.9 GB of NX data in `MapleV83.app/nx` (with `nx/manifest.json` hashes) so the app plays right after installing;
`--no-data` makes a small build that downloads the data from the data server on first launch (resumable, Base.nx last).
`--publish` hands the IPA to the script in `MV83_OTA_PUBLISH`. `tools/push_data.sh` (USB) or `--sim` copy the data
directly instead. Simulator checks: `tools/simtest.sh <script> <seconds> [--install]`.

Mac: `tools/build_mac.sh [--send]` builds, signs (Apple Development, not notarized) and zips to
`build/MapleV83-mac-<version>-<build>.zip`; `--send` Taildrops it to `PEER`. First launch: System Settings > Privacy &
Security > Open Anyway. Data and log: `~/Library/Application Support/MapleV83` (downloaded from the data server like
the iPhone app). `autotest.txt` there runs a scripted session.

## Editing data
Quest logic, requirements, rewards, NPC scripts, drops and shops live on the server (`server/scripts`, `server/wz/*.xml`,
the database): restart the server, nothing reaches the phone. Client-side text and art (quest names/descriptions, item
names, maps) are the NX files: re-convert them into `data/nx` and each app downloads only the files whose SHA-256
changed (e.g. Quest.nx, 7.5 MB) at its next launch on home Wi-Fi or Tailscale. A newer download wins over the bundled
copy until an app update ships a newer one.

## In game (iPhone)
D-pad bottom left; chips along the top open Items, Equip, Stats, Skills, Quests, Keys and Menu (they flow beside an open
side panel). Stats, Skills, Touch Controls and Menu are the iPhone's own panels (`src/client/IO/MobileUI.*`,
`UIMobile*.cpp`): tap + for one point, hold to repeat; Reset (free, unlimited) gives AP or SP back, as does chat
`@resetap` / `@resetsp`. Touch Controls binds all ten action buttons (ATTACK=Ctrl, JUMP=Alt, LOOT=Z, 1-7 =
Shift/Ins/Home/PgUp/Del/End/PgDn) to a skill, item or basic action in the server keymap; holding a button bound to an
attack skill keeps casting. The Mac keeps v83's windows (`MV83_MOBILE_UI=1` shows the mobile panels there). Make a
character GM with `characters.gm` (e.g. 6): `!help` lists GM commands, `@help` player commands.

## Publishing (maintainer)
The GitHub repo is published from separate local repos (tools at the root, `server/`, `maplewasm/`) by
`tools/push_github.sh`: it keeps the upstream Cosmic and maple-wasm histories and adds each sync as one snapshot
commit, after a scan for private values listed in a local, uncommitted pattern file.
