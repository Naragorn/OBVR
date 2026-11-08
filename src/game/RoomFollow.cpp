#include "game/RoomFollow.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "core/AddressSpace.h"
#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/AnalogWalk.h"
#include "game/GameAddresses.h"
#include "vr/RoomFollowLogic.h"

namespace obvr::game {
namespace {

constexpr UInt32 kPlayerMoveSlot = 0x00A73BC0;  // PlayerCharacter vtable 0x00A73A0C + 0x1B4
constexpr UInt32 kActorMove = 0x005EFAE0;
// sub esp,28h; push esi; mov esi,ecx - Actor::Move's first bytes.
constexpr UInt8 kActorMoveBytes[] = {0x83, 0xEC, 0x28, 0x56, 0x8B, 0xF1};
// The walk's caller, UpdateMovement 0x0063C730..0x0063CDB6: only its call
// takes the step (AI packages and others call Move too).
constexpr UInt32 kWalkCallerFirst = 0x0063C730;
constexpr UInt32 kWalkCallerLast = 0x0063CDB6;
constexpr UInt32 kRefRotZOffset = 0x28;
constexpr UInt32 kRefPositionOffset = 0x2C;

// What Move hands back is passed through untouched as a register value: declared
// float, the wrapper stored the x87 top after the call and loaded it again on
// return, but Move leaves nothing on the x87 stack and its caller (0x0063CB37)
// pops nothing - every walking step left one slot on the stack, the FPU's stack
// fault turned the frame's float maths to NaN, and the body check hid the
// visible body while the stick walked (Nadi in the headset, 2026-11-01: "der
// Körper verschwindet bis auf die Hände wenn ich mit dem linken Stick
// navigiere"; the harness: "the head -nan(ind) units" at body-lean-stick's
// walking mark). An integer return touches no x87 register, so the wrapper
// leaves the FPU as Move left it, whatever Move returns.
using MoveFn = UInt32(__fastcall*)(void* self, void* edx, float dt, float* step, UInt32 flags);

MoveFn g_original = nullptr;
bool g_pending = false;
float g_wantX = 0.0f;
float g_wantY = 0.0f;
// What the last taken request achieved for the room (world x, y), until taken.
struct RoomStepResult {
	bool ready = false;
	float x = 0.0f;
	float y = 0.0f;
};
RoomStepResult g_achieved;
UInt32 g_lines = 8;

UInt32 ReturnAddress() {
#if defined(_MSC_VER)
	return reinterpret_cast<UInt32>(_ReturnAddress());
#else
	return reinterpret_cast<UInt32>(__builtin_return_address(0));
#endif
}

}  // namespace

// The player's Move, wrapped: thiscall, so ecx is `self` and edx unused.
UInt32 __fastcall OBVR_PlayerMove(void* self, void* edx, float dt, float* step, UInt32 flags) {
	const UInt32 caller = ReturnAddress();
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	// Debug.WaterCaptureTrace: every Move of the player, whoever asks it -
	// how often the engine moves the player at all (the water's "reset" of
	// 2026-11-02 fell on the frames the player's feet moved, four to five
	// frames apart).
	if (GetConfig().waterCaptureTrace && reinterpret_cast<UInt32>(self) == player) {
		const float* const at = reinterpret_cast<const float*>(static_cast<const UInt8*>(self) + kRefPositionOffset);
		OBVR_LOG("Move trace: the player's Move from %08X, dt %.4f, step %.3f %.3f %.3f, flags %u, at %.2f %.2f %.2f, pending %d",
		         caller, static_cast<double>(dt), static_cast<double>(step != nullptr ? step[0] : 0.0f),
		         static_cast<double>(step != nullptr ? step[1] : 0.0f),
		         static_cast<double>(step != nullptr ? step[2] : 0.0f), flags, static_cast<double>(at[0]),
		         static_cast<double>(at[1]), static_cast<double>(at[2]), g_pending ? 1 : 0);
	}
	if (reinterpret_cast<UInt32>(self) != player || step == nullptr || !(dt > 0.0f) || caller < kWalkCallerFirst ||
	    caller > kWalkCallerLast) {
		return g_original(self, edx, dt, step, flags);
	}
	// The walk's own step, as the engine handed it: given back after the call.
	const float engineX = step[0];
	const float engineY = step[1];
	// The left stick's exact heading (game/AnalogWalk.h): the octant's step turned.
	TurnAnalogStep(step);
	if (!g_pending) {
		const UInt32 result = g_original(self, edx, dt, step, flags);
		step[0] = engineX;
		step[1] = engineY;
		return result;
	}
	g_pending = false;
	const UInt8* const ref = reinterpret_cast<const UInt8*>(self);
	const float rotZ = *reinterpret_cast<const float*>(ref + kRefRotZOffset);
	const float* const at = reinterpret_cast<const float*>(ref + kRefPositionOffset);
	const NiPoint3 room = vr::WorldStepToActor(g_wantX, g_wantY, rotZ);
	const float walkX = step[0];
	const float walkY = step[1];
	step[0] += room.x;
	step[1] += room.y;
	const float beforeX = at[0];
	const float beforeY = at[1];
	const UInt32 result = g_original(self, edx, dt, step, flags);
	// The whole step asked, back in the world (the inverse of WorldStepToActor).
	const float s = math::Sin(rotZ);
	const float c = math::Cos(rotZ);
	const float askedX = c * step[0] + s * step[1];
	const float askedY = -s * step[0] + c * step[1];
	const float movedX = at[0] - beforeX;
	const float movedY = at[1] - beforeY;
	const bool walking = walkX * walkX + walkY * walkY > 1e-6f;
	const NiPoint3 achieved =
		vr::RoomStepAchieved(movedX, movedY, askedX, askedY, g_wantX, g_wantY, walking);
	g_achieved = RoomStepResult{true, achieved.x, achieved.y};
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("RoomFollow: the room's step %.1f %.1f (the actor's %.1f %.1f, the walk's %.1f %.1f) - moved %.1f "
		         "%.1f, %.0f %% of it along the step, the room's part %.1f %.1f",
		         static_cast<double>(g_wantX), static_cast<double>(g_wantY), static_cast<double>(room.x),
		         static_cast<double>(room.y), static_cast<double>(walkX), static_cast<double>(walkY),
		         static_cast<double>(movedX), static_cast<double>(movedY),
		         static_cast<double>(vr::RoomStepShare(movedX, movedY, askedX, askedY)) * 100.0,
		         static_cast<double>(achieved.x), static_cast<double>(achieved.y));
	}
	step[0] = engineX;
	step[1] = engineY;
	return result;
}

void InstallRoomFollow() {
	const UInt8 slot[4] = {static_cast<UInt8>(kActorMove & 0xFF), static_cast<UInt8>((kActorMove >> 8) & 0xFF),
	                       static_cast<UInt8>((kActorMove >> 16) & 0xFF), static_cast<UInt8>((kActorMove >> 24) & 0xFF)};
	if (!mem::Verify(kPlayerMoveSlot, slot, sizeof(slot)) ||
	    !mem::Verify(kActorMove, kActorMoveBytes, sizeof(kActorMoveBytes))) {
		OBVR_LOG("RoomFollow: the player's Move (%08X in the slot %08X) is not the game's own - the body does not "
		         "follow the head through the room",
		         kActorMove, kPlayerMoveSlot);
		return;
	}
	g_original = reinterpret_cast<MoveFn>(kActorMove);
	const UInt32 detour = reinterpret_cast<UInt32>(&OBVR_PlayerMove);
	if (!mem::SafeWrite(kPlayerMoveSlot, &detour, sizeof(detour))) {
		OBVR_LOG("RoomFollow: could not write the vtable slot %08X", kPlayerMoveSlot);
		return;
	}
	OBVR_LOG("RoomFollow: the player's Move wrapped (the slot %08X, around %08X) - the room's steps go through the "
	         "character controller",
	         kPlayerMoveSlot, kActorMove);
}

void RequestRoomStep(float worldX, float worldY) {
	g_wantX = worldX;
	g_wantY = worldY;
	g_pending = g_original != nullptr && (worldX != 0.0f || worldY != 0.0f);
}

bool TakeRoomStepAchieved(float& worldX, float& worldY) {
	if (!g_achieved.ready) {
		return false;
	}
	g_achieved.ready = false;
	worldX = g_achieved.x;
	worldY = g_achieved.y;
	return true;
}

}  // namespace obvr::game
