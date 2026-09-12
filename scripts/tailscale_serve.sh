#!/usr/bin/env bash
# Publish the web client through Tailscale on the shared HTTPS port using path
# mounts, so it coexists with other apps already served on 443:
#   https://<node>.ts.net/maple/         -> static site   (html-server, :8000)
#   https://<node>.ts.net/maple-ws       -> game proxy    (ws-proxy,    :8082)
#   https://<node>.ts.net/maple-assets   -> asset stream  (assets-server, :8765)
# Funnel (public internet) follows the port's existing Funnel setting.
set -euo pipefail
WEB_PORT="${WEB_HOST_PORT:-8000}"
WS_PORT="${WS_PROXY_HOST_PORT:-8082}"
ASSETS_PORT="${ASSETS_HOST_PORT:-8765}"
HTTPS_PORT="${TS_HTTPS_PORT:-443}"

tailscale serve --bg --https="$HTTPS_PORT" --set-path /maple "http://127.0.0.1:${WEB_PORT}"
tailscale serve --bg --https="$HTTPS_PORT" --set-path /maple-ws "http://127.0.0.1:${WS_PORT}"
tailscale serve --bg --https="$HTTPS_PORT" --set-path /maple-assets "http://127.0.0.1:${ASSETS_PORT}"
tailscale serve status
