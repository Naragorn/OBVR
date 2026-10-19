#pragma once

#include "core/Types.h"

// The pure part of Debug.FlameProbe (game/FlameProbe.h): which scene objects
// are time controllers by their compiled class name, and which of a
// controller's two candidate "next" words to follow.

namespace obvr::game {

// Whether a class name as NiClassNameOf gives it ("NiFlipController@@",
// "NiPSysEmitterCtlr@@", "NiControllerManager@@") is a time controller's:
// NetImmerse names them with "Controller" or the short "Ctlr" (the particle
// system controllers). "?" and the geometry and property classes are not.
inline bool IsControllerClassName(const char* name) {
	if (name == nullptr) {
		return false;
	}
	static const char* const kWords[] = {"Controller", "Ctlr"};
	for (const char* word : kWords) {
		for (UInt32 start = 0; name[start] != '\0' && start < 96; ++start) {
			UInt32 at = 0;
			while (word[at] != '\0' && name[start + at] == word[at]) {
				++at;
			}
			if (word[at] == '\0') {
				return true;
			}
		}
	}
	return false;
}

// A NiTimeController's next controller in its owner's chain: at +0x34 in
// this executable (read off a torch's flame, 2026-10-19: the word at +0x30
// is the controller's target - the particle system, the texturing property -
// and +0x34 the next controller or 0), at +0x30 by Gamebryo's published
// layout, at +0x2C by the reading game/BowVisual.cpp works with. The probe
// reads all three words and follows the first that holds a controller, in
// that order. 0: none does, the chain ends.
inline UInt32 NextControllerOffset(bool controllerAt2C, bool controllerAt30, bool controllerAt34) {
	if (controllerAt34) {
		return 0x34;
	}
	if (controllerAt2C) {
		return 0x2C;
	}
	if (controllerAt30) {
		return 0x30;
	}
	return 0;
}

}  // namespace obvr::game
