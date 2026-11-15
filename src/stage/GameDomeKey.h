#pragma once

// When the sky dome (stage/DomeLook.h) is made again: it is made once from
// the game's textures and kept beside OBVR.dll with a key - which files it
// was made from, their sizes and times, and the painter's version. The same
// key the next start: the kept dome is used and nothing is worked out. A
// texture mod installed or removed, or a new painter, changes the key and
// the dome is made once more. Pure over plain values (render/GameDomeJob.cpp
// asks the files).

#include <cstdio>
#include <string>

#include "core/Types.h"

namespace obvr::stage {

// Counted up whenever the sky's painting changes (stage/DomeGameLayers.h,
// DomePaint.h, DomeAtlas.h), so every kept dome is made again.
inline constexpr int kGameDomeVersion = 1;

struct TextureStamp {
	const char* path = "";  // the texture's path under Data
	bool found = false;
	bool loose = false;     // its own file under Data, not the archive's
	UInt64 time = 0;        // of the file it comes from: its own, or the archive's
	UInt64 size = 0;
};

// The key's text; empty when a texture was not found anywhere.
inline std::string GameDomeKey(int width, const TextureStamp* stamps, int count) {
	char line[400];
	std::snprintf(line, sizeof(line), "OBVR sky dome %d, %d across\n", kGameDomeVersion, width);
	std::string key = line;
	for (int i = 0; i < count; ++i) {
		if (!stamps[i].found) {
			return std::string();
		}
		std::snprintf(line, sizeof(line), "%s %s %llu %llu\n", stamps[i].path, stamps[i].loose ? "file" : "archive",
		              static_cast<unsigned long long>(stamps[i].time), static_cast<unsigned long long>(stamps[i].size));
		key += line;
	}
	return key;
}

enum class GameDomePlan {
	Use,      // the kept dome fits: nothing to do
	Make,     // make it (again)
	GiveUp,  // a texture is missing: the dark dome stays
};

// keyNow: from the files as they are; keyKept: the kept dome's, empty
// without one; domeThere: its model, material and atlas are all there.
inline GameDomePlan PlanGameDome(const std::string& keyNow, const std::string& keyKept, bool domeThere) {
	if (keyNow.empty()) {
		return GameDomePlan::GiveUp;
	}
	return domeThere && keyNow == keyKept ? GameDomePlan::Use : GameDomePlan::Make;
}

}  // namespace obvr::stage
