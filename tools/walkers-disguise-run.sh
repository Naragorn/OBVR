#!/usr/bin/env bash
# Runs tools/hand-scripts/walkers-disguise.txt (the Linux game, headless) with
# Oblivion Walkers' [Disguise] Seconds=40 in its live plugin INI, so the
# disguise ends within the run; the INI is put back whatever happens (the
# way Walkers' own tools/disguise-harness.sh does it). Then prints OBVR's
# disguise lines and Walkers' Disguise lines.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
game="${OBVR_GAME_DIR:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
ini="$game/Data/OBSE/Plugins/OblivionWalkers.ini"
backup="$(mktemp)"
cp "$ini" "$backup"
trap 'cp "$backup" "$ini"; rm -f "$backup"' EXIT
if grep -q '^\[Disguise\]' "$ini"; then
	sed -i 's/^\[Disguise\]\r\?$/[Disguise]\nSeconds=40/' "$ini"
else
	printf '\n[Disguise]\nSeconds=40\n' >> "$ini"
fi
(cd "$root" && python3 tools/hand-script-run.py tools/hand-scripts/walkers-disguise.txt --headless "$@") || true
grep -E "^Disguise:" "$game/OblivionWalkers.log" | tail -20 || true
