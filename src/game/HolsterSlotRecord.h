#pragma once

// What the four places on the body keep (vr/Holster.h, HolsterSlot), as the
// co-save carries it: one form id per place, the first slot (None) unused. The
// ids are the base forms' (TESForm +0x0C); the co-save's ResolveRefID moves
// them when the load order changed, and an id it cannot resolve - a plugin
// gone - leaves that place empty.
//
// Pure, covered by holster_slot_record_test; game/QuickKeyPages.cpp writes and
// reads the record beside the quick menu's pages (one save callback per plugin).

#include "core/Types.h"
#include "vr/Holster.h"

namespace obvr::game {

inline constexpr UInt32 kHolsterSlotsRecord = 0x544C5348;  // 'HSLT'
inline constexpr UInt32 kHolsterSlotsVersion = 1;

struct HolsterSlotsRecord {
	UInt32 ids[vr::kHolsterSlotCount] = {};
};

// The places from a record read back; false (all empty) for a record of
// another length or version. `resolve(id, out)` is the co-save's
// ResolveRefID.
template <typename Resolve>
bool HolsterSlotsOfRecord(const void* data, UInt32 length, UInt32 version, Resolve resolve,
                          UInt32 (&out)[vr::kHolsterSlotCount]) {
	for (UInt32& id : out) {
		id = 0;
	}
	if (data == nullptr || length != sizeof(HolsterSlotsRecord) || version != kHolsterSlotsVersion) {
		return false;
	}
	const auto* const record = static_cast<const HolsterSlotsRecord*>(data);
	for (UInt32 i = 1; i < vr::kHolsterSlotCount; ++i) {
		UInt32 resolved = 0;
		if (record->ids[i] != 0 && resolve(record->ids[i], resolved)) {
			out[i] = resolved;
		}
	}
	return true;
}

// Each form at one place at most: keeping `id` at `slot` takes it from any
// other. 0 or the None slot clears nothing and keeps nothing.
inline void KeepAtSlot(UInt32 (&ids)[vr::kHolsterSlotCount], vr::HolsterSlot slot, UInt32 id) {
	const UInt32 at = static_cast<UInt32>(slot);
	if (at == 0 || at >= vr::kHolsterSlotCount || id == 0) {
		return;
	}
	for (UInt32 i = 1; i < vr::kHolsterSlotCount; ++i) {
		if (ids[i] == id) {
			ids[i] = 0;
		}
	}
	ids[at] = id;
}

// The place that keeps `id`, None when none does (or for 0).
inline vr::HolsterSlot SlotKeeping(const UInt32 (&ids)[vr::kHolsterSlotCount], UInt32 id) {
	if (id == 0) {
		return vr::HolsterSlot::None;
	}
	for (UInt32 i = 1; i < vr::kHolsterSlotCount; ++i) {
		if (ids[i] == id) {
			return static_cast<vr::HolsterSlot>(i);
		}
	}
	return vr::HolsterSlot::None;
}

}  // namespace obvr::game
