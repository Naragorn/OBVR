#pragma once

// Which torch lying in the world OBVR gives its light back to (Nadi,
// 2026-10-26: the list of lit torches kept with the save, or none at all -
// "go" for the way without save data).
//
// The mark is the reference's own: the form flag 0x80, "fire out". A
// carried light that is "off by default" - Torch02, flags 0x2B - starts every
// new reference with it set (0x0044A968), so vanilla's dropped torch is dark;
// OBVR clears it when it lights one (game/TorchFire.h), and the engine keeps
// it so through a 3D built anew (torch-drop-relight.txt 2026-10-26 09:04:
// "fire out 0, 1 flame(s), 0 under AttachLight"). The engine reads the
// cleared flag at the 3D's set-up and puts the flames on itself, but makes no
// light for it - that is OBVR's to add. A torch whose flames are not there yet
// waits: OBVR puts no flames on a 3D already in the scene (the renderer
// faulted, 09:02).
//
// Pure; covered by torch_ground_test. game/TorchFire.cpp walks the player's
// cell.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kLightFlagCarried = 0x02;
inline constexpr UInt32 kLightFlagOffByDefaultBit = 0x20;
inline constexpr UInt32 kRefFlagFireOut = 0x80;

struct TorchOnGround {
	bool isLight = true;     // the base form a light
	UInt32 lightFlags = 0;   // the light's (+0x7C)
	UInt32 formFlags = 0;    // the reference's
	bool haveNode = false;
	UInt32 flames = 0;       // children under its "FlameNode<n>"
	UInt32 lightsUnder = 0;  // children under its "AttachLight"
};

enum class TorchGroundNeed : UInt8 {
	NotOurs,  // no torch OBVR lit: vanilla's dark log, a fixed light, no light at all
	Wait,     // OBVR's, its 3D or the engine's flames not there yet
	Light,    // OBVR's, its flames there and no light: the light made again
	Nothing,  // OBVR's and burning with its light
};

inline TorchGroundNeed TorchOnGroundNeeds(const TorchOnGround& t) {
	if (!t.isLight || (t.lightFlags & kLightFlagCarried) == 0 || (t.lightFlags & kLightFlagOffByDefaultBit) == 0 ||
	    (t.formFlags & kRefFlagFireOut) != 0) {
		return TorchGroundNeed::NotOurs;
	}
	if (!t.haveNode || t.flames == 0) {
		return TorchGroundNeed::Wait;
	}
	return t.lightsUnder == 0 ? TorchGroundNeed::Light : TorchGroundNeed::Nothing;
}

}  // namespace obvr::game
