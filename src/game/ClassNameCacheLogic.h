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

// Whether a class name (still carrying its "@@" tail) is a node - one that
// keeps children. Every NetImmerse node class ends in "Node": NiNode,
// BSFadeNode, BSFaceGenNiNode, NiBillboardNode, NiSwitchNode; geometry -
// NiTriShape, NiTriStrips - does not. Reading a children array off a
// geometry would read its own fields as pointers, so the question is asked
// before a walk goes down.
inline bool ClassNameIsNode(const char* className) {
	UInt32 length = 0;
	while (className[length] != '\0' && length < 96) {
		++length;
	}
	while (length > 0 && className[length - 1] == '@') {
		--length;  // the "@@" tail
	}
	if (length < 4) {
		return false;
	}
	return className[length - 4] == 'N' && className[length - 3] == 'o' && className[length - 2] == 'd' &&
	       className[length - 1] == 'e';
}

// One slot a vtable, a later vtable taking the slot of an earlier one; 0 is
// no vtable and never kept. Only for vtables whose type records cannot go
// away - the game image's (the caller's to say). With the name, whether the
// class is a node: the walks ask that of every object, and the name was
// measured to its end for the answer each time.
struct VtableNames {
	static constexpr UInt32 kSlots = 256;
	UInt32 vtable[kSlots] = {};
	const char* name[kSlots] = {};
	bool node[kSlots] = {};

	// Whether the class kept for this vtable is a node; false for one not
	// kept, which Find tells apart.
	bool IsNode(UInt32 vtableAddress) const {
		const UInt32 slot = SlotOf(vtableAddress);
		return vtableAddress != 0 && vtable[slot] == vtableAddress && node[slot];
	}

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
		node[slot] = ClassNameIsNode(className);
	}
};

}  // namespace obvr::game
