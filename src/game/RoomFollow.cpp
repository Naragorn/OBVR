#include "game/RoomFollow.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
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

using MoveFn = float(__fastcall*)(void* self, void* edx, float dt, float* step, UInt32 flags);

MoveFn g_original = nullptr;
bool g_pending = false;
float g_wantX = 0.0f;
float g_wantY = 0.0f;
bool g_shareReady = false;
float g_share = 0.0f;
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
float __fastcall OBVR_PlayerMove(void* self, void* edx, float dt, float* step, UInt32 flags) {
	const UInt32 caller = ReturnAddress();
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	if (!g_pending || reinterpret_cast<UInt32>(self) != player || step == nullptr || !(dt > 0.0f) ||
	    caller < kWalkCallerFirst || caller > kWalkCallerLast) {
		return g_original(self, edx, dt, step, flags);
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
	const float result = g_original(self, edx, dt, step, flags);
	// The whole step asked, back in the world (the inverse of WorldStepToActor).
	const float s = math::Sin(rotZ);
	const float c = math::Cos(rotZ);
	const float askedX = c * step[0] + s * step[1];
	const float askedY = -s * step[0] + c * step[1];
	g_share = vr::RoomStepShare(at[0] - beforeX, at[1] - beforeY, askedX, askedY);
	g_shareReady = true;
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("RoomFollow: the room's step %.1f %.1f (the actor's %.1f %.1f, the walk's %.1f %.1f) - moved %.1f "
		         "%.1f, %.0f %% of it",
		         static_cast<double>(g_wantX), static_cast<double>(g_wantY), static_cast<double>(room.x),
		         static_cast<double>(room.y), static_cast<double>(walkX), static_cast<double>(walkY),
		         static_cast<double>(at[0] - beforeX), static_cast<double>(at[1] - beforeY),
		         static_cast<double>(g_share) * 100.0);
	}
	step[0] = walkX;
	step[1] = walkY;
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

bool TakeRoomStepShare(float& share) {
	if (!g_shareReady) {
		return false;
	}
	g_shareReady = false;
	share = g_share;
	return true;
}

}  // namespace obvr::game
