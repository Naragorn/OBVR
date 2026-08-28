#pragma once

#include "core/Types.h"

namespace obvr::render {

// A small fixed set of pointers, for remembering which vertex buffers have
// ever been locked with DISCARD - the software-skinning pool the collapse
// investigation now watches. Bounded because the game recycles a handful
// of dynamic buffers, not hundreds; when the bound is wrong the insert
// says so instead of growing, and the caller counts what fell out.
struct PointerSet {
	static constexpr UInt32 kCapacity = 64;

	void* items[kCapacity] = {};
	UInt32 count = 0;

	bool Contains(const void* pointer) const {
		for (UInt32 i = 0; i < count; ++i) {
			if (items[i] == pointer) {
				return true;
			}
		}
		return false;
	}

	// True when the pointer is in the set afterwards - already known or
	// newly added. False only when the set is full and the pointer is new.
	bool Insert(void* pointer) {
		if (Contains(pointer)) {
			return true;
		}
		if (count >= kCapacity) {
			return false;
		}
		items[count] = pointer;
		++count;
		return true;
	}
};

}  // namespace obvr::render
