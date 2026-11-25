#include "game/StuckArrow.h"

#include "core/AddressSpace.h"

namespace obvr::game {
namespace {

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

}  // namespace

ArrowRead ReadArrow(UInt32 ref) {
	ArrowRead a;
	if (!mem::LooksLikeObjectAddress(ref) || Read(ref) != kArrowProjectileVtable) {
		return a;
	}
	a.isArrow = true;
	a.state = Read(ref + kArrowStateOffset);
	const UInt32 impact = Read(ref + kArrowImpactOffset);
	a.haveImpact = mem::LooksLikeObjectAddress(impact);
	a.kind = a.haveImpact ? Read(impact) : 0;
	return a;
}

}  // namespace obvr::game
