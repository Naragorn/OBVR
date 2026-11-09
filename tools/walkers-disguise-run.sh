#!/usr/bin/env bash
# Runs tools/hand-scripts/walkers-disguise.txt (or with --hands walkers-disguise-hands.txt) (the Linux game, headless) with
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
# --hands: walkers-disguise-hands.txt, the disguise started by Walkers' own
# test switch ([Disguise] Test=1) for the near look at the hands' blood.
script=walkers-disguise.txt
keys='Seconds=40'
if [[ "${1:-}" == "--hands" ]]; then
	shift
	script=walkers-disguise-hands.txt
	keys='Seconds=40\nTest=1'
fi
if grep -q '^\[Disguise\]' "$ini"; then
	sed -i "s/^\[Disguise\]\r\?$/[Disguise]\n$keys/" "$ini"
else
	printf "\n[Disguise]\n$keys\n" >> "$ini"
fi
(cd "$root" && python3 tools/hand-script-run.py "tools/hand-scripts/$script" --headless "$@") || true
grep -E "^Disguise:" "$game/OblivionWalkers.log" | tail -20 || true
