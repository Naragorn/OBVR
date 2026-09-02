#include "game/AimAtSource.h"

#include "camera/FrameLogic.h"
#include "core/AroundCall.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/PlayerAim.h"

// The callbacks the stub reaches. cdecl and extern "C", as the cast hook's
// is, so the names the stub is built against are the names the linker sees.
extern "C" void __cdecl OBVR_AimSourceBefore(void* caster);
extern "C" void __cdecl OBVR_AimSourceAfter();

namespace obvr::game {
namespace {

// The stub's slots: the return address it keeps aside and the address of the
// arguments it records. Static, because the stub's bytes carry these
// addresses; they live in the stub's own allocation, after the code.
mem::AroundCallSlots g_slots;
bool g_installed = false;

AimSourcePose g_pose;

// The rotation as it was on the way in, and whether it is currently swapped.
// g_swapped is also the reentrancy guard the stub relies on: a second entry
// while one stands is left alone rather than overwriting the saved values.
PlayerRotation g_saved{};
bool g_swapped = false;

bool g_reported = false;

// Big enough for the 46-byte stub and the two slots.
constexpr UInt32 kStubCapacity = 64;

// The player, on the same terms PlayerAim follows a pointer: inside the user
// address space, past the null pages, aligned. Kept separate so this file
// does not reach into PlayerAim's internals.
UInt32 PlayerAddressOrZero() {
	const UInt32 address = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	if (address < 0x00010000u || address > 0x7FFFFFFFu || (address & 3u) != 0u) {
		return 0;
	}
	return address;
}

}  // namespace

void InstallAimAtSource() {
	const UInt32 slot = addr::kPlayerCasterVtableKeyHandlerSlot;
	const UInt32 target = addr::kAnimationKeyHandler;

	const UInt8 expected[4] = {static_cast<UInt8>(target & 0xFF),
	                           static_cast<UInt8>((target >> 8) & 0xFF),
	                           static_cast<UInt8>((target >> 16) & 0xFF),
	                           static_cast<UInt8>((target >> 24) & 0xFF)};
	if (!mem::Verify(slot, expected, sizeof(expected))) {
		// Another game version, or another mod there first. The slot is
		// left to whoever holds it and the turn keeps its job.
		OBVR_LOG("Aim: the player's vtable slot at %08X does not hold the key handler %08X - "
		         "the aim stays with the body's turn",
		         slot, target);
		return;
	}

	auto* stub = static_cast<UInt8*>(mem::AllocExecutable(kStubCapacity));
	if (stub == nullptr) {
		OBVR_LOG("Aim: no executable memory for the key handler stub");
		return;
	}
	g_slots.returnAddress = reinterpret_cast<UInt32>(stub + kStubCapacity - 8);
	g_slots.arguments = reinterpret_cast<UInt32>(stub + kStubCapacity - 4);

	const UInt32 stubAddress = reinterpret_cast<UInt32>(stub);
	const UInt32 size = mem::BuildAroundCallStub(
		stub, kStubCapacity - 8, stubAddress, target,
		reinterpret_cast<UInt32>(&OBVR_AimSourceBefore),
		reinterpret_cast<UInt32>(&OBVR_AimSourceAfter), g_slots);
	if (size == 0) {
		OBVR_LOG("Aim: the key handler stub does not fit into %u bytes", kStubCapacity - 8);
		return;
	}

	if (!mem::SafeWrite(slot, &stubAddress, sizeof(stubAddress))) {
		OBVR_LOG("Aim: could not write the player's vtable slot at %08X", slot);
		return;
	}

	g_installed = true;
	OBVR_LOG("Aim: the key handler is wrapped - the player's vtable slot at %08X goes through "
	         "%08X (%u bytes) around %08X, so bow, spell and swing take the gaze inside the "
	         "one call that reads it",
	         slot, stubAddress, size, target);
}

bool AimAtSourceInstalled() { return g_installed; }

void SetAimSourcePose(const AimSourcePose& pose) { g_pose = pose; }

}  // namespace obvr::game

// The callbacks run inside an engine call OBVR does not own: a few reads, a
// few writes, no allocation, nothing that can throw.

extern "C" void __cdecl OBVR_AimSourceBefore(void* caster) {
	using namespace obvr;
	if (game::g_swapped) {
		return;
	}

	const UInt32 player = game::PlayerAddressOrZero();
	const bool isPlayer =
		player != 0 && reinterpret_cast<UInt32>(caster) == player + addr::kPlayerMagicCasterOffset;
	if (!camera::AimSourceSwapDue(game::g_pose.wanted, isPlayer, game::ReadPlayerAction())) {
		return;
	}
	if (!game::ReadPlayerRotation(game::g_saved)) {
		return;
	}

	// The same arithmetic the turn used, so the sign has the measurement
	// behind it: the head's turn is subtracted from the engine's heading.
	const float yaw = camera::PlayerYawForGaze(game::g_saved.yaw, game::g_pose.headYaw);
	if (!game::WritePlayerYaw(yaw)) {
		return;
	}
	game::WritePlayerPitch(game::g_pose.pitch);
	game::g_swapped = true;

	if (!game::g_reported) {
		game::g_reported = true;
		OBVR_LOG("Aim: an attack takes the gaze at the source - heading %.4f set to %.4f and "
		         "pitch %.4f to %.4f for the length of one engine call, the body never turned",
		         static_cast<double>(game::g_saved.yaw), static_cast<double>(yaw),
		         static_cast<double>(game::g_saved.pitch),
		         static_cast<double>(game::g_pose.pitch));
	}
}

extern "C" void __cdecl OBVR_AimSourceAfter() {
	using namespace obvr;
	if (!game::g_swapped) {
		return;
	}
	game::WritePlayerYaw(game::g_saved.yaw);
	game::WritePlayerPitch(game::g_saved.pitch);
	game::g_swapped = false;
}
