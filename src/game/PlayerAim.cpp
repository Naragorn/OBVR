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

	// A null check is not enough here, and the reason is a session that ended
	// in a crash shortly after a loading screen with this probe switched on.
	// It was not proven to be the cause - but this is the only code OBVR added
	// that follows a raw pointer into the game's object model, and it does so
	// on frames where that object is being torn down and rebuilt. A global
	// caught mid-assignment is not necessarily null.
	//
	// So the value has to look like a pointer to a Gamebryo object before it
	// is followed: inside the 32-bit user address space, past the reserved
	// low pages that catch null-offset reads, and four-byte aligned as every
	// allocation here is. That rejects a half-written value, a small integer
	// and a pointer into kernel space; it cannot reject a plausible pointer
	// to an object that is not finished, which is why the probe is off by
	// default and stays a diagnostic.
	const UInt32 address = reinterpret_cast<UInt32>(player);
	if (address < 0x00010000u || address > 0x7FFFFFFFu || (address & 3u) != 0u) {
		return false;
	}

	const auto* const rot = reinterpret_cast<const float*>(player + kRotationOffset);
	out.pitch = rot[0];
	out.roll = rot[1];
	out.yaw = rot[2];
	return true;
}

}  // namespace obvr::game
