#include "game/ThrowNoise.h"

#include "game/GrabPhysics.h"
#include "game/ThrowNoiseLogic.h"

namespace obvr::game {
namespace {

UInt32& SentOff() {
	static UInt32 s_ref = 0;
	return s_ref;
}

}  // namespace

void NoteSentOff(UInt32 ref, const NiPoint3& velocityUnits) {
	if (ref != 0 && IsThrowSpeed(velocityUnits, kThrowMinUnitsPerSecond)) {
		SentOff() = ref;
	}
}

UInt32 TakeSentOff() {
	const UInt32 ref = SentOff();
	SentOff() = 0;
	return ref;
}

}  // namespace obvr::game
