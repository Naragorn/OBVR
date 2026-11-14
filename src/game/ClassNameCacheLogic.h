#pragma once

// The compiled class of a scene object is read off MSVC's type records behind
// its vtable (game/FirstPersonHide.cpp ClassNameOf): four dependent reads and
// a check of up to 96 characters, for every object of every walk of a model
// tree - and the walks run every frame (the first-person hide, the bones by
// name, the bounds kept in view). The records are the image's own data and
// never change, so the name is kept by vtable. In Nadi's Windows session of
// 2026-11-13 ClassNameOf alone was 919 of the render thread's 222 000 samples
// outside the scene pass (docs/performance-spec.md). Pure,
// class_name_cache_test.

#include "core/Types.h"

namespace obvr::game {

// One slot a vtable, a later vtable taking the slot of an earlier one; 0 is
// no vtable and never kept. Only for vtables whose type records cannot go
// away - the game image's (the caller's to say).
struct VtableNames {
	static constexpr UInt32 kSlots = 256;
	UInt32 vtable[kSlots] = {};
	const char* name[kSlots] = {};

	// Vtables are words apart in the image, a class's a few dozen words long.
	static constexpr UInt32 SlotOf(UInt32 vtableAddress) {
		return ((vtableAddress >> 2) ^ (vtableAddress >> 10)) % kSlots;
	}

	const char* Find(UInt32 vtableAddress) const {
		const UInt32 slot = SlotOf(vtableAddress);
		return vtableAddress != 0 && vtable[slot] == vtableAddress ? name[slot] : nullptr;
	}

	void Remember(UInt32 vtableAddress, const char* className) {
		if (vtableAddress == 0 || className == nullptr) {
			return;
		}
		const UInt32 slot = SlotOf(vtableAddress);
		vtable[slot] = vtableAddress;
		name[slot] = className;
	}
};

}  // namespace obvr::game
