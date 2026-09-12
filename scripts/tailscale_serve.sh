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

# Notes learned the hard way:
#  - a mount without a trailing slash matches that exact path only; "/maple/" covers the subtree.
#  - `tailscale serve` flips the port to "tailnet only" even if Funnel was on; use
#    `tailscale funnel` to keep (or enable) public access on the port. Set
#    TS_PUBLIC=0 to publish to the tailnet only.
#  - both commands need root; run with sudo.
CMD=serve
[ "${TS_PUBLIC:-1}" = "1" ] && CMD=funnel
tailscale "$CMD" --bg --https="$HTTPS_PORT" --set-path /maple/ "http://127.0.0.1:${WEB_PORT}"
tailscale "$CMD" --bg --https="$HTTPS_PORT" --set-path /maple-ws "http://127.0.0.1:${WS_PORT}"
tailscale "$CMD" --bg --https="$HTTPS_PORT" --set-path /maple-assets "http://127.0.0.1:${ASSETS_PORT}"
tailscale serve status
