#include "game/CrosshairTarget.h"

#include "core/AddressSpace.h"
#include "game/GameAddresses.h"
#include "game/MenuType.h"

namespace obvr::game {
namespace {

// Whether a value looks like a pointer to a Gamebryo object before it is
// followed.
//
// The same test PlayerAim applies, and it is here for the same reason rather
// than because a read is dangerous in itself: this runs every frame, including
// the frames a cell change tears the object model down and builds it again. A
// global caught mid-assignment is not necessarily null, and a null check alone
// would follow it.
//
// Inside the 32-bit user address space, past the reserved low pages that catch
// null-offset reads, and four-byte aligned as every allocation here is. That
// rejects a half-written value, a small integer and a pointer into kernel
// space. It cannot reject a plausible pointer to an object that is not
// finished, which is why the menu is additionally asked to identify itself.
bool LooksLikeObject(const void* pointer) {
	const UInt32 address = reinterpret_cast<UInt32>(pointer);
	return mem::LooksLikeObjectAddress(address);
}

// Whether three floats look like a position in Oblivion's world.
//
// The bound is deliberately far outside anything the game uses - Tamriel's
// playable area is a few hundred thousand units across - because this is a
// check on whether the memory holds coordinates at all, not on whether the
// coordinates are somewhere sensible. Written as a pair of comparisons so that
// a NaN, which fails every comparison it appears in, is refused as well: a NaN
// reaching the depth arithmetic would come out the far side as a quad placed
// nowhere.
bool LooksLikePosition(const NiPoint3& position) {
	constexpr float kLimit = 1.0e7f;
	const auto sane = [](float value) { return value > -kLimit && value < kLimit; };
	return sane(position.x) && sane(position.y) && sane(position.z);
}

// The tile menu array's entry for HUDInfo, or zero.
//
// Reported only. It is a TileMenu and OBVR does not have that class's layout,
// so it is never followed - what it is worth is as evidence in a log that the
// menu system looks the way two independent descriptions say it does. A
// non-zero entry beside a menu that identified itself is that agreement; the
// two disagreeing would be worth knowing before anything else is believed.
UInt32 HudInfoArrayEntry() {
	const auto* const data = *reinterpret_cast<const UInt32* const*>(addr::kTileMenuArrayData);
	if (!LooksLikeObject(data)) {
		return 0;
	}

	const UInt32 count = *reinterpret_cast<const UInt16*>(addr::kTileMenuArrayCount);
	const UInt32 index = kMenuIdHudInfo - kMenuIdFirst;
	if (index >= count) {
		return 0;
	}
	return data[index];
}

}  // namespace

CrosshairTarget ReadCrosshairTarget() {
	CrosshairTarget target;

	target.arrayEntry = HudInfoArrayEntry();

	// The global holds the menu; the address names the global. Reading through
	// two levels is what the declaration in xOBSE says to do -
	// HUDInfoMenu** - and getting that wrong would read the menu's vtable
	// pointer as if it were the menu.
	const auto* const menu = *reinterpret_cast<const UInt8* const*>(addr::kHudInfoMenuPointer);
	if (!LooksLikeObject(menu)) {
		return target;
	}

	// The one check that matters. A Menu carries its own type id, and this one
	// has to say it is HUDInfo before anything further is read from it. If the
	// address is wrong, whatever it leads to will not answer 0x3ED, and the
	// crosshair keeps its fixed distance instead of being placed on a number
	// read out of an unrelated object.
	const UInt32 id = *reinterpret_cast<const UInt32*>(menu + addr::kMenuIdOffset);
	target.menuAddress = reinterpret_cast<UInt32>(menu);
	if (id != kMenuIdHudInfo) {
		target.rejectedId = id;
		return target;
	}
	target.haveMenu = true;

	// Null here is the ordinary case, not a failure: nothing activatable is
	// being looked at, which is most of the time.
	const auto* const ref =
		*reinterpret_cast<const UInt8* const*>(menu + addr::kHudInfoCrosshairRefOffset);
	if (!LooksLikeObject(ref)) {
		return target;
	}
	target.refAddress = reinterpret_cast<UInt32>(ref);

	const auto* const position = reinterpret_cast<const float*>(ref + addr::kRefPositionOffset);
	const NiPoint3 read{position[0], position[1], position[2]};
	if (!LooksLikePosition(read)) {
		return target;
	}

	target.position = read;
	target.haveRef = true;
	return target;
}

}  // namespace obvr::game
