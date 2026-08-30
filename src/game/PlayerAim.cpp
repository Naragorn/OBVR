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

// Past this, a float in the rotation field is not an angle at all.
//
// A full turn either way, which is more than the field is ever meant to hold -
// the player's pitch stops at 89 degrees - but deliberately loose rather than
// tight: a script may set an actor's angle past the limits the mouse can
// reach, and this is a check on whether the memory looks like a rotation, not
// on whether the game put a sensible value there.
constexpr float kPlausibleRotationRadians = 7.0f;

// The player, or null when there is nothing safe to follow.
//
// A null check is not enough here, and the reason is a session that ended in
// a crash shortly after a loading screen with the aim probe switched on. It
// was not proven to be the cause - but this is the only code OBVR added that
// follows a raw pointer into the game's object model, and it does so on
// frames where that object is being torn down and rebuilt. A global caught
// mid-assignment is not necessarily null.
//
// So the value has to look like a pointer to a Gamebryo object before it is
// followed: inside the 32-bit user address space, past the reserved low pages
// that catch null-offset reads, and four-byte aligned as every allocation
// here is. That rejects a half-written value, a small integer and a pointer
// into kernel space; it cannot reject a plausible pointer to an object that
// is not finished.
//
// Shared by the read and the write rather than written twice, because the
// write is the one that matters: a read of a torn-down object returns
// nonsense, a write to one corrupts whatever now lives there.
UInt8* PlayerOrNull() {
	auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);

	const UInt32 address = reinterpret_cast<UInt32>(player);
	if (address < 0x00010000u || address > 0x7FFFFFFFu || (address & 3u) != 0u) {
		return nullptr;
	}
	return player;
}

}  // namespace

bool ReadPlayerRotation(PlayerRotation& out) {
	const auto* const player = PlayerOrNull();
	if (player == nullptr) {
		return false;
	}

	const auto* const rot = reinterpret_cast<const float*>(player + kRotationOffset);
	out.pitch = rot[0];
	out.roll = rot[1];
	out.yaw = rot[2];
	return true;
}

namespace {

// The shared body of both writes: check, then put one component.
//
// which is 0 for the pitch and 2 for the yaw, matching rotX and rotZ in the
// order the engine stores them.
bool WriteRotationComponent(int which, float radians) {
	auto* const player = PlayerOrNull();
	if (player == nullptr) {
		return false;
	}

	auto* const rot = reinterpret_cast<float*>(player + kRotationOffset);

	// A second check, on the value rather than on the pointer - and this one is
	// here because a write earns it where a read did not.
	//
	// The pointer test above cannot reject a plausible pointer to an object
	// that is not finished being built, and this code runs on frames where the
	// player is being torn down and rebuilt. A read of such an object returns
	// nonsense and nothing else happens; a write to it corrupts whatever now
	// lives at that address. The aim probe, which only read, is already
	// suspected of ending one session in a crash shortly after a loading
	// screen.
	//
	// So the field has to already hold something that could be an angle in
	// radians before it is written. Anything past a full turn is not a
	// rotation, and the comparison is written so that a NaN - which fails
	// every comparison it is in - is refused too.
	//
	// What this can do: reject a live object that is not a player, and reject
	// uninitialised memory holding a float that is not an angle. What it
	// cannot do: tell a half-built player from a finished one when both happen
	// to hold a plausible angle, which is why the switch that reaches here
	// exists and is documented as the way out.
	const float existing = rot[which];
	if (!(existing > -kPlausibleRotationRadians && existing < kPlausibleRotationRadians)) {
		return false;
	}

	// One component, never the triple. The roll beside them does nothing on an
	// actor, and writing all three would mean deciding a value for something
	// nothing here has an opinion about.
	rot[which] = radians;
	return true;
}

}  // namespace

bool WritePlayerPitch(float radians) { return WriteRotationComponent(0, radians); }

bool WritePlayerYaw(float radians) { return WriteRotationComponent(2, radians); }

}  // namespace obvr::game
