#include "game/AnchoredRef.h"

#include "core/AddressSpace.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/HandBodies.h"
#include "game/NearbyItems.h"

namespace obvr::game {
namespace {

// A dummy has 27 bodies under some 60 nodes (chaindollarena01.nif); a
// skeleton has more nodes, but an actor is not walked.
constexpr UInt32 kCensusMaxNodes = 256;
constexpr UInt32 kCensusMaxDepth = 16;

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

// The motion type of a rigid body, through its motion's vtable (WorldPush.cpp's
// MotionTypeOf): false when the motion or its table does not look right.
bool MotionTypeOfBody(UInt32 body, UInt32& type) {
	const UInt32 motion = Read(body + kBodyMotionOffset);
	if (!LooksLikeObject(motion) || !LooksLikeObject(Read(motion))) {
		return false;
	}
	const UInt32 typeSlot = Read(Read(motion) + kMotionTypeSlot);
	if (!LooksLikeObject(typeSlot)) {
		return false;
	}
	using TypeFn = UInt32(__thiscall*)(void* motion);
	type = reinterpret_cast<TypeFn>(typeSlot)(reinterpret_cast<void*>(motion)) & 0xFF;
	return true;
}

void CensusTree(UInt32 object, UInt32 depth, UInt32& nodesLeft, BodyCensus& census) {
	if (depth > kCensusMaxDepth || nodesLeft == 0) {
		return;
	}
	--nodesLeft;
	// A rigid body of the hands' own class, or 0 (a phantom, none, or the
	// hands' bodies not made yet): only such a one's motion is read.
	const UInt32 body = HavokBodyOfNode(object);
	UInt32 type = 0;
	if (body != 0 && MotionTypeOfBody(body, type)) {
		CountBody(census, type);
	}
	if (!NiClassIsNode(NiClassNameOf(reinterpret_cast<const void*>(object)))) {
		return;
	}
	const UInt32 children = Read(object + addr::kNiChildrenOffset);
	const UInt16 count = *reinterpret_cast<const UInt16*>(object + addr::kNiChildCountOffset);
	if (!LooksLikeObject(children) || count > 512) {
		return;
	}
	for (UInt16 i = 0; i < count; ++i) {
		const UInt32 child = Read(children + i * 4u);
		if (LooksLikeObject(child)) {
			CensusTree(child, depth + 1, nodesLeft, census);
		}
	}
}

}  // namespace

BodyCensus CensusBodiesOf(UInt32 ref) {
	BodyCensus census;
	if (!LooksLikeObject(ref)) {
		return census;
	}
	const UInt32 node = Read(ref + addr::kRefNiNodeOffset);
	if (LooksLikeObject(node)) {
		UInt32 nodesLeft = kCensusMaxNodes;
		CensusTree(node, 0, nodesLeft, census);
	}
	return census;
}

bool RefMayBeCarried(UInt32 ref) {
	// An actor is not walked: a body is dragged as ever, whatever its bones are.
	if (IsActorFormType(RefBaseFormType(ref))) {
		return true;
	}
	return GripMayCarry(CensusBodiesOf(ref), false);
}

}  // namespace obvr::game
