#include "game/NearbyItems.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

constexpr UInt32 kRefParentCellOffset = 0x40;
constexpr UInt32 kCellObjectListOffset = 0x48;
constexpr UInt32 kFormFlagsOffset = 0x08;
constexpr UInt32 kFormDeletedOrDisabled = 0x20 | 0x800;
constexpr UInt32 kNiHiddenFlag = 0x1;
// A cell with more references than this is not walked further in a frame.
constexpr UInt32 kMaxRefsPerFrame = 8192;

}  // namespace

UInt8 RefBaseFormType(UInt32 ref) {
	if (!LooksLikeObject(ref)) {
		return 0;
	}
	const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
	return LooksLikeObject(base) ? *reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset)
	                             : 0;
}

NearItem FindNearestItem(const NiPoint3& right, bool rightValid, const NiPoint3& left,
                         bool leftValid, float reachUnits, UInt32 except) {
	NearItem best;
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!LooksLikeObject(player) || !(rightValid || leftValid)) {
		return best;
	}
	const UInt32 cell = Read(player + kRefParentCellOffset);
	if (!LooksLikeObject(cell)) {
		return best;
	}
	// The first entry is inline in the cell; the rest are linked.
	UInt32 entry = cell + kCellObjectListOffset;
	for (UInt32 walked = 0; entry != 0 && walked < kMaxRefsPerFrame; ++walked) {
		const UInt32 ref = Read(entry);
		const UInt32 next = Read(entry + 4);
		entry = LooksLikeObject(next) ? next : 0;
		if (!LooksLikeObject(ref) || ref == player || ref == except) {
			continue;
		}
		if ((Read(ref + kFormFlagsOffset) & kFormDeletedOrDisabled) != 0) {
			continue;
		}
		const UInt32 base = Read(ref + addr::kRefBaseFormOffset);
		if (!LooksLikeObject(base) ||
		    !IsHandItemType(*reinterpret_cast<const UInt8*>(base + addr::kFormTypeOffset))) {
			continue;
		}
		const UInt32 nodeAddress = Read(ref + addr::kRefNiNodeOffset);
		if (!LooksLikeObject(nodeAddress)) {
			continue;
		}
		const auto* node = reinterpret_cast<const NiAVObject*>(nodeAddress);
		if ((node->flags & kNiHiddenFlag) != 0) {
			continue;
		}
		ConsiderNearItem(best, ref, node->worldBound.center, node->worldBound.radius, right,
		                 rightValid, left, leftValid, reachUnits);
	}
	return best;
}

}  // namespace obvr::game
