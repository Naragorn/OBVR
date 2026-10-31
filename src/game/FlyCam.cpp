#include "game/FlyCam.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

constexpr UInt32 kFlyCamToggle = 0x006669F0;
// push esi; mov esi,ecx; cmp byte [esi+748h],0 - the toggle's test of the flag.
constexpr UInt8 kFlyCamToggleBytes[] = {0x56, 0x8B, 0xF1, 0x80, 0xBE, 0x48, 0x07, 0x00, 0x00, 0x00};
constexpr UInt32 kFlyCamOnOffset = 0x748;
constexpr UInt32 kFlyCamYawOffset = 0x74C;
constexpr UInt32 kFlyCamPitchOffset = 0x750;
constexpr UInt32 kFlyCamAtOffset = 0x754;

int g_known = -1;  // -1 not yet looked at, 0 refused, 1 the game's own

UInt32 Player() {
	const UInt32 player = *reinterpret_cast<const UInt32*>(addr::kPlayerPointer);
	return mem::LooksLikeObjectAddress(player) ? player : 0;
}

}  // namespace

bool FlyCamKnown() {
	if (g_known < 0) {
		g_known = mem::Verify(kFlyCamToggle, kFlyCamToggleBytes, sizeof(kFlyCamToggleBytes)) ? 1 : 0;
		OBVR_LOG(g_known == 1 ? "FlyCam: the console's tfc is the game's own (the player's +0x748) - the sticks steer it"
		                      : "FlyCam: the tfc toggle is not the game's own - the sticks leave the fly camera alone");
	}
	return g_known == 1;
}

bool ReadFlyCam(FlyCamPose& out) {
	const UInt32 player = FlyCamKnown() ? Player() : 0;
	if (player == 0) {
		return false;
	}
	const float* const at = reinterpret_cast<const float*>(player + kFlyCamAtOffset);
	out.on = *reinterpret_cast<const UInt8*>(player + kFlyCamOnOffset) != 0;
	out.at = NiPoint3{at[0], at[1], at[2]};
	out.yaw = *reinterpret_cast<const float*>(player + kFlyCamYawOffset);
	out.pitch = *reinterpret_cast<const float*>(player + kFlyCamPitchOffset);
	return true;
}

bool WriteFlyCam(const NiPoint3& at, float yaw) {
	const UInt32 player = FlyCamKnown() ? Player() : 0;
	if (player == 0 || !(at.x == at.x && at.y == at.y && at.z == at.z && yaw == yaw)) {
		return false;
	}
	float* const to = reinterpret_cast<float*>(player + kFlyCamAtOffset);
	to[0] = at.x;
	to[1] = at.y;
	to[2] = at.z;
	*reinterpret_cast<float*>(player + kFlyCamYawOffset) = yaw;
	*reinterpret_cast<float*>(player + kFlyCamPitchOffset) = 0.0f;
	return true;
}

}  // namespace obvr::game
