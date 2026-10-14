#!/usr/bin/env bash
# Builds OBVR.dll on Linux with clang-cl (cmake/toolchain-linux-clang-cl.cmake,
# docs/linux.md) and deploys it into the Linux game folder - the Linux side of
# building and deploying. Nothing is deployed when the build fails: the DLL in
# the game folder stays as it was, and the script says so.
#
#   tools/build-linux.sh            build, then deploy OBVR.dll and OBVR.map
#   tools/build-linux.sh --test     also build and run the unit tests first
#   tools/build-linux.sh --assets   also deploy the menus, the input manifest
#                                   and the sounds from assets/
#   tools/build-linux.sh --no-deploy
#
# OBVR_GAME_DIR points somewhere other than Steam's default Oblivion folder.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build="$root/build-linux-clangcl"
tests_build="$root/build-linux-tests"
game="${OBVR_GAME_DIR:-$HOME/.local/share/Steam/steamapps/common/Oblivion}"
plugins="$game/Data/OBSE/Plugins"

run_tests=0
deploy=1
assets=0
for arg in "$@"; do
	case "$arg" in
		--test) run_tests=1 ;;
		--assets) assets=1 ;;
		--no-deploy) deploy=0 ;;
		-h|--help) sed -n '2,13p' "${BASH_SOURCE[0]}"; exit 0 ;;
		*) echo "unknown option: $arg" >&2; exit 2 ;;
	esac
done

# The game, if it runs: the process whose program is Oblivion.exe (under
# Proton its name reads "Main", so pgrep -x does not find it).
game_running() {
	local cmdline
	for cmdline in /proc/[0-9]*/cmdline; do
		local argv0
		argv0="$(tr '\0' '\n' < "$cmdline" 2>/dev/null | head -n 1)" || continue
		argv0="${argv0//\\//}"
		if [[ "${argv0##*/}" =~ ^[Oo][Bb][Ll][Ii][Vv][Ii][Oo][Nn]\.[Ee][Xx][Ee]$ ]]; then
			return 0
		fi
	done
	return 1
}

if [[ ! -f "$build/build.ninja" ]]; then
	cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		--toolchain "$root/cmake/toolchain-linux-clang-cl.cmake"
fi

if [[ $run_tests -eq 1 ]]; then
	if [[ ! -f "$tests_build/build.ninja" ]]; then
		cmake -S "$root/tests" -B "$tests_build" -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_FLAGS=-m32
	fi
	cmake --build "$tests_build"
	ctest --test-dir "$tests_build" --output-on-failure
fi

if ! cmake --build "$build"; then
	echo "BUILD FAILED - nothing deployed; the game folder keeps its DLL." >&2
	exit 1
fi

if [[ $deploy -eq 0 ]]; then
	echo "Built $build/OBVR.dll (not deployed)."
	exit 0
fi
if [[ ! -d "$plugins" ]]; then
	echo "No plugin folder at $plugins - set OBVR_GAME_DIR." >&2
	exit 1
fi
if game_running; then
	echo "Oblivion is running - nothing deployed. Close it and run the script again." >&2
	exit 1
fi

# Copied beside the target first, then renamed over it: never a half-written DLL.
for file in OBVR.dll OBVR.map; do
	cp "$build/$file" "$plugins/.$file.new"
	mv -f "$plugins/.$file.new" "$plugins/$file"
done
built="$(sha256sum "$build/OBVR.dll" | cut -c1-16)"
deployed="$(sha256sum "$plugins/OBVR.dll" | cut -c1-16)"
if [[ "$built" != "$deployed" ]]; then
	echo "DEPLOY MISMATCH: built $built, in the game folder $deployed" >&2
	exit 1
fi

if [[ $assets -eq 1 ]]; then
	mkdir -p "$game/Data/Menus" "$plugins/OBVR_Input" "$plugins/OBVR_Sounds"
	cp -r "$root/assets/menus/." "$game/Data/Menus/"
	cp "$root/assets/input/"*.json "$plugins/OBVR_Input/"
	cp "$root/assets/sounds/"*.wav "$plugins/OBVR_Sounds/"
	echo "Assets deployed: menus, input manifest, sounds."
fi

echo "Deployed OBVR.dll $deployed ($(stat -c %s "$plugins/OBVR.dll") bytes) and OBVR.map to $plugins"
