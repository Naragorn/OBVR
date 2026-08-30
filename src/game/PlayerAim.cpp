#include "game/PlayerAim.h"

#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// TESObjectREFR::rotX, rotY, rotZ, from xOBSE's GameObjects.h where they are
// commented "// 020". The class is anchored independently at the other end:
// this file's kPlayerIsThirdPersonOffset of 0x588 was established from
// ToggleCamera's own write, and xOBSE places isThirdPerson at the same 0x588 -
// so the two descriptions of PlayerCharacter agree on a field half a kilobyte
// further in, which is not a coincidence a wrong layout produces.
constexpr UInt32 kRotationOffset = 0x20;

}  // namespace

bool ReadPlayerRotation(PlayerRotation& out) {
	auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (player == nullptr) {
		return false;
	}

	const auto* const rot = reinterpret_cast<const float*>(player + kRotationOffset);
	out.pitch = rot[0];
	out.roll = rot[1];
	out.yaw = rot[2];
	return true;
}

}  // namespace obvr::game
