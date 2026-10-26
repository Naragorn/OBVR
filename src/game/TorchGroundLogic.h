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

// The light's flags while a torch is dropped (game/MeleeHits.cpp
// DropEquippedTorch): its "off by default" bit cleared for the drop's one
// call, so the new reference does not take "fire out" from it (0x0044A968)
// and the engine's own set-up of its 3D puts the flames on (0x004E41DA),
// their render state with them. Until 2026-10-26 the reference was made dark
// and OBVR put the flames on its 3D a frame later - in the frame the engine
// had not yet hung that 3D into the cell, it was assumed; in Nadi's crash and
// in torch-drop-stow-cycle.txt (round 3, 14:27) it already hung there, the
// flames' four shapes stayed without a property state, and the renderer
// faulted drawing the Sparks (0x007659A9).
inline UInt32 LightFlagsForLitDrop(UInt32 lightFlags) { return lightFlags & ~kLightFlagOffByDefaultBit; }

// Whether OBVR puts flames on a dropped torch's 3D itself: only when it has
// none and the 3D does not hang in the scene yet - the engine sets the new
// shapes' render state up as it hangs it there. On a 3D already in the scene
// they would draw without it; then the torch gets only its light.
inline bool FlamesMayBePutOn(UInt32 flames, bool rootInScene) { return flames == 0 && !rootInScene; }

// Whether the engine puts a dropped torch's flames on itself: the reference
// was made with the fire on (LightFlagsForLitDrop), and its set-up adds them
// as it hangs the 3D in - OBVR adding its own as well gave two
// (torch-drop-stow-cycle.txt, 2026-10-26 14:35: "with 2 flame(s)", 10 shapes,
// in nine drops of ten; the one already hung had the engine's alone).
inline bool EngineLightsDrop(UInt32 formFlagsAsMade) { return (formFlagsAsMade & kRefFlagFireOut) == 0; }

}  // namespace obvr::game
