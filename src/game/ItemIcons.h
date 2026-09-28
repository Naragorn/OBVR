#pragma once

#include "core/Types.h"
#include "render/TestPattern.h"

namespace obvr::game {

// The game's own icons for the items and spells on the hotkeys, for the
// quick menu's ring (docs/controls-spec.md 4.4): the same DDS files the
// inventory and the magic menu show, opened through the engine's file
// opener (loose files and archives, as the game finds them), decoded on the
// CPU (render/DdsImage.h) and kept, a few dozen at a time.

// The side of every icon handed out: the vanilla icons are 64 pixels; 32s
// are grown and bigger ones shrunk to it.
inline constexpr UInt32 kItemIconSide = 64;

// The icon path of a form (an item's base form or a spell), as the form
// keeps it, relative to Textures\Menus\Icons\. Empty when it has none.
void ReadIconPath(UInt32 form, char* out, UInt32 size);

// The file the engine opens for an icon path: "Textures\Menus\Icons\" in
// front (TESIcon's prefix, GameAddresses.h), unless the path already starts
// at Textures\ or Data\. Leading slashes dropped. False when there is no path
// or it does not fit. Pure, for its test.
inline constexpr const char* kItemIconPrefix = "Textures\\Menus\\Icons\\";

inline bool StartsWithNoCase(const char* text, const char* start) {
	for (; *start != '\0'; ++text, ++start) {
		char a = *text;
		char b = *start;
		a = (a >= 'A' && a <= 'Z') ? static_cast<char>(a - 'A' + 'a') : a;
		b = (b >= 'A' && b <= 'Z') ? static_cast<char>(b - 'A' + 'a') : b;
		if (a != b) {
			return false;
		}
	}
	return true;
}

inline bool IconFilePath(const char* iconPath, char* out, UInt32 size) {
	if (out == nullptr || size == 0) {
		return false;
	}
	out[0] = '\0';
	if (iconPath == nullptr) {
		return false;
	}
	while (*iconPath == '\\' || *iconPath == '/') {
		++iconPath;
	}
	if (*iconPath == '\0') {
		return false;
	}
	const bool rooted = StartsWithNoCase(iconPath, "textures\\") || StartsWithNoCase(iconPath, "textures/") ||
	                    StartsWithNoCase(iconPath, "data\\") || StartsWithNoCase(iconPath, "data/");
	UInt32 used = 0;
	const char* const parts[2] = {rooted ? "" : kItemIconPrefix, iconPath};
	for (const char* part : parts) {
		for (; *part != '\0'; ++part) {
			if (used + 1 >= size) {
				out[0] = '\0';
				return false;
			}
			out[used++] = *part;
		}
	}
	out[used] = '\0';
	return true;
}

// The icon for a path, kItemIconSide square, its colours in the texture's
// order (A8R8G8B8: blue first, as the canvas overlays paint). Loaded the
// first time it is asked for; a path that fails is remembered as failed and
// answers null. The pointer stays good until kItemIconCacheSize other icons
// have been asked for since - the ring asks for its eight every time they
// change, so the ones it shows are never the ones pushed out.
inline constexpr UInt32 kItemIconCacheSize = 24;
const render::Pixel* ItemIcon(const char* iconPath);

// Which cache place a new icon goes to: the first unused one, else the one
// asked for longest ago. Pure, for its test.
inline UInt32 IconCacheVictim(const bool (&used)[kItemIconCacheSize],
                              const UInt32 (&lastUse)[kItemIconCacheSize]) {
	UInt32 victim = 0;
	for (UInt32 i = 0; i < kItemIconCacheSize; ++i) {
		if (!used[i]) {
			return i;
		}
		if (lastUse[i] < lastUse[victim]) {
			victim = i;
		}
	}
	return victim;
}

}  // namespace obvr::game
