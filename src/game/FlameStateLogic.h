#pragma once

// Whether a torch's shapes have their render state (the crash of 2026-10-26,
// Nadi on Windows after torches dropped and stowed: NiDX9Renderer's vtable
// slot 0x170, 0x00765990, read a null at renderer+0xC while drawing a torch's
// "Sparks" - derived from the code, not verified: the current property state,
// so a shape drawn without one).
//
// One suspect is OBVR's own: the dropped torch's flames are put on by OBVR
// (game/TorchFire.cpp IgniteDroppedTorch, 0x004E26F0), and a flame put on a
// 3D already in the scene draws without its render state - the renderer
// faulted that way at the relight (torch-drop-relight.txt, 2026-10-26 09:02).
// The fresh drop's flames are put on in the frame the engine made the 3D,
// before it hangs it into the cell - assumed, never read. So the instrument
// reads, for a torch's model, whether its root already hangs in the scene and
// how many of its drawn shapes lack the property state (NiGeometry +0xAC,
// xOBSE NiObjects.h: propertyState 0AC, effectState 0B0, geomData 0B4 - the
// last measured in game/NearbyItems.cpp).
//
// Pure; covered by flame_state_test. game/TorchFire.cpp reads the scene.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kGeometryPropertyStateOffset = 0xAC;

struct FlameShapeStates {
	bool haveRoot = false;
	bool inScene = false;       // the root hangs under a parent
	char parent[48] = {};       // that parent's class and name
	UInt32 shapes = 0;          // drawn shapes under the root
	UInt32 withoutState = 0;    // of them with no property state
	char firstWithout[48] = {}; // the first such shape's name
	char stateClass[40] = {};   // the class a state read as, to check the offset
};

inline bool StartsWith(const char* text, const char* head) {
	if (text == nullptr) {
		return false;
	}
	for (UInt32 i = 0; head[i] != '\0'; ++i) {
		if (text[i] != head[i]) {
			return false;
		}
	}
	return true;
}

// A drawn shape by its compiled class (FirstPersonHide.h NiClassNameOf).
inline bool IsDrawnShapeClass(const char* className) {
	return StartsWith(className, "NiTriShape") || StartsWith(className, "NiTriStrips") ||
	       StartsWith(className, "NiParticleSystem") || StartsWith(className, "NiMeshParticleSystem") ||
	       StartsWith(className, "NiParticles") || StartsWith(className, "NiLines");
}

// A torch's model by its node's name: "torch" in it, any case.
inline bool IsTorchModelName(const char* name) {
	if (name == nullptr) {
		return false;
	}
	static const char kTorch[] = "torch";
	for (const char* t = name; *t != '\0'; ++t) {
		UInt32 i = 0;
		while (kTorch[i] != '\0' && t[i] != '\0' &&
		       (t[i] >= 'A' && t[i] <= 'Z' ? static_cast<char>(t[i] - 'A' + 'a') : t[i]) == kTorch[i]) {
			++i;
		}
		if (kTorch[i] == '\0') {
			return true;
		}
	}
	return false;
}

inline bool ShapesLackState(const FlameShapeStates& s) { return s.withoutState > 0; }

}  // namespace obvr::game
