#pragma once

// Which dome SteamVR shows in place of its grid ([Render] LoadingStageLook):
//
//   * sky (the default): the night sky made from the game's own textures -
//     its stars, its nebula, Masser and Secunda, Cyrodiil's map under the
//     feet. Made on the player's machine from the player's own game files
//     (render/GameDomeJob.h) and kept beside OBVR.dll; nothing of Bethesda's
//     is shipped.
//   * dark: OBVR's own painted dome, shipped (assets/stage). Also what shows
//     until the sky is made, and when it cannot be made.
//
// Nadi, 2026-11-15: "B bauen aber mit A höher aufgelöst (8192) als fallback.
// und im obvr menü die möglichkeit zwischen beiden zu wechseln".

#include "core/ChoiceWord.h"
#include "core/Types.h"

namespace obvr::stage {

enum class DomeLook : UInt8 {
	Sky = 0,
	Dark = 1,
};

inline constexpr UInt32 kDomeLookCount = 2;
inline constexpr const char* kDomeLookNames[kDomeLookCount] = {"sky", "dark"};
// The dark dome by default (Nadi, 2026-11-15: "stell den schwarzen als default"); the sky is the choice.
inline constexpr DomeLook kDomeLookDefault = DomeLook::Dark;

inline bool ParseDomeLook(const char* text, DomeLook& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kDomeLookNames, kDomeLookCount, index)) {
		return false;
	}
	out = static_cast<DomeLook>(index);
	return true;
}

inline DomeLook DomeLookFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kDomeLookCount) - 0.5f)) {
		return kDomeLookDefault;
	}
	return static_cast<DomeLook>(static_cast<int>(value + 0.5f));
}

}  // namespace obvr::stage
