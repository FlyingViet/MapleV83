#!/bin/bash
# Publish the project to the public GitHub repo FlyingViet/MapleV83:
#   /        this tools repo (README, tools/)
#   server/  the Cosmic server repo (branch master)
#   client/  the iOS/Mac client repo maplewasm/ (branch ios)
# Public history = the untouched upstream histories (P0nk/Cosmic, sh-islam/maple-wasm-public) under server/ and
# client/, plus one snapshot commit per sync with the current trees of the three local repos. Local commits, their
# messages and authors never leave this Mac. Every snapshot is scanned first for the private values (addresses,
# names, IDs) listed in MV83_PRIVATE_PATTERNS (one PCRE per line, kept outside the repo); any hit stops the push.
# Hits that are someone else's (e.g. an upstream credit) go in MV83_PRIVATE_ALLOW (fixed strings; a hit line containing one is allowed).
# Only committed work is published: local.env, LocalServers.h, server/config.yaml edits, generated quest scripts
# and data/nx stay here. Work happens in .github-mirror/ (index only, no checkout).
# Usage: tools/push_github.sh [--dry-run]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REMOTE="${REMOTE:-https://github.com/FlyingViet/MapleV83.git}"
PATTERNS="${MV83_PRIVATE_PATTERNS:-$HOME/.config/maplev83/private-patterns.txt}"
ALLOW="${MV83_PRIVATE_ALLOW:-$HOME/.config/maplev83/private-allow.txt}"
NAME="${MV83_PUBLIC_NAME:-FlyingViet}"
EMAIL="${MV83_PUBLIC_EMAIL:-35708271+FlyingViet@users.noreply.github.com}"
M="$ROOT/.github-mirror"
CLIENT="$ROOT/maplewasm"; [ -d "$CLIENT/.git" ] || CLIENT="$ROOT/client"
# prefix | local repo | local branch | upstream ref | upstream name
SOURCES=("server|$ROOT/server|master|origin/master|P0nk/Cosmic" "client|$CLIENT|ios|origin/mobile-web|sh-islam/maple-wasm-public")
DRY=0; [ "${1:-}" = "--dry-run" ] && DRY=1

[ -s "$PATTERNS" ] || { echo "missing $PATTERNS (private patterns); refusing to publish"; exit 1; }
export GIT_AUTHOR_NAME="$NAME" GIT_AUTHOR_EMAIL="$EMAIL" GIT_COMMITTER_NAME="$NAME" GIT_COMMITTER_EMAIL="$EMAIL"

if [ ! -d "$M/.git" ]; then
  if git ls-remote --exit-code "$REMOTE" refs/heads/main >/dev/null 2>&1; then git clone -q --no-checkout "$REMOTE" "$M"
  else git init -q -b main "$M" && git -C "$M" remote add origin "$REMOTE"; fi
fi
cd "$M"
git fetch -q origin 2>/dev/null || true
if git rev-parse -q --verify origin/main >/dev/null; then
  if git rev-parse -q --verify main >/dev/null; then
    git merge-base --is-ancestor origin/main main || { echo "GitHub has commits not made here; rm -rf $M and rerun"; exit 1; }
  else git update-ref refs/heads/main origin/main; fi
fi
HEAD_COMMIT="$(git rev-parse -q --verify main || true)"

# Bring in each repo's HEAD (local refs only; pushing main sends just what main reaches) and its upstream base.
git read-tree --empty
git fetch -q "$ROOT" "+master:refs/local/tools"
git read-tree "refs/local/tools^{tree}"
PARENTS=(); BODY=""; GRAFT=""
for s in "${SOURCES[@]}"; do
  IFS='|' read -r prefix repo branch upstream uname <<<"$s"
  [ "$(git -C "$repo" rev-parse --is-shallow-repository)" = false ] || { echo "$repo is shallow: git -C $repo fetch --unshallow origin"; exit 1; }
  base="$(git -C "$repo" merge-base "$branch" "$upstream")"
  git fetch -q "$repo" "+$branch:refs/local/$prefix" "+$base:refs/upstream/$prefix"
  git read-tree --prefix="$prefix/" "refs/local/$prefix^{tree}"
  if [ -z "$HEAD_COMMIT" ]; then
    # First publish: graft the upstream history under its folder (separate index), then the snapshot goes on top.
    t="$(export GIT_INDEX_FILE="$M/.git/index.graft"; rm -f "$GIT_INDEX_FILE"
      if [ -n "$GRAFT" ]; then git read-tree "$GRAFT^{tree}"; else git read-tree --empty; fi
      git read-tree --prefix="$prefix/" "$base^{tree}" && git write-tree; rm -f "$GIT_INDEX_FILE")"
    GRAFT="$(git commit-tree "$t" ${GRAFT:+-p "$GRAFT"} -p "$base" -m "Add $uname at $prefix/ (upstream $(git rev-parse --short "$base"))")"
  elif ! git merge-base --is-ancestor "$base" "$HEAD_COMMIT"; then
    PARENTS+=(-p "$base")
  fi
  BODY+="$prefix/: $uname $(git rev-parse --short "$base") + local changes"$'\n'
done
[ -z "$HEAD_COMMIT" ] && HEAD_COMMIT="$GRAFT"
TREE="$(git write-tree)"
if [ "$TREE" = "$(git rev-parse "$HEAD_COMMIT^{tree}")" ] && [ ${#PARENTS[@]} -eq 0 ]; then
  if [ "$(git rev-parse -q --verify origin/main || true)" = "$(git rev-parse main)" ]; then
    echo "Nothing new to publish ($(git rev-parse --short main))"; exit 0
  fi
  [ "$DRY" = 1 ] && { echo "Dry run: main $(git rev-parse --short main) not pushed yet"; exit 0; }
  git push -q -u origin main && echo "Pushed $(git rev-parse --short main) to $REMOTE"; exit 0
fi

# Scan every file of the snapshot; stop on any private value not on the allow list.
HITS="$(git grep -nIiP -f "$PATTERNS" "$TREE" | sed "s/^$TREE://" | { [ -s "$ALLOW" ] && grep -vF -f "$ALLOW" || cat; } || true)"
if [ -n "$HITS" ]; then echo "Private values found; nothing pushed:"; echo "$HITS" | cut -c1-200 | head -40; exit 1; fi

NEW="$(git commit-tree "$TREE" -p "$HEAD_COMMIT" ${PARENTS[@]+"${PARENTS[@]}"} -m "Update from $(date +%Y-%m-%d)" -m "$BODY")"
git update-ref refs/heads/main "$NEW"
git read-tree main
if [ "$DRY" = 1 ]; then echo "Dry run: main is $(git rev-parse --short main), not pushed"; exit 0; fi
git push -q -u origin main
echo "Pushed $(git rev-parse --short main) to $REMOTE"
