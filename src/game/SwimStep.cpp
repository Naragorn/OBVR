#include "game/SwimStep.h"

#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "vr/RoomFollowLogic.h"
#include "vr/SwimStroke.h"

namespace obvr::game {
namespace {

constexpr UInt32 kSwimWalkSpeed = 0x005E3920;
// sub esp,10h; push esi; mov esi,ecx; mov eax,[esi] - the speed getters' first bytes.
constexpr UInt8 kSpeedBytes[] = {0x83, 0xEC, 0x10, 0x56, 0x8B, 0xF1, 0x8B, 0x06};
constexpr UInt32 kRefRotZOffset = 0x28;
// How long a request holds without being asked again.
constexpr float kSwimRequestSeconds = 0.25f;

using SpeedFn = float(__fastcall*)(void* self, void* edx);

struct SwimRequest {
	NiPoint3 velocity{0.0f, 0.0f, 0.0f};
	float secondsLeft = 0.0f;
	int speedGetter = 0;  // 0 not looked at, 1 the game's, 2 not
};

SwimRequest& Request() {
	static SwimRequest request;
	return request;
}

}  // namespace

void RequestSwimVelocity(const NiPoint3& worldUnitsPerSecond) {
	SwimRequest& r = Request();
	const bool any = worldUnitsPerSecond.LengthSquared() > 1e-6f;
	r.velocity = any ? worldUnitsPerSecond : NiPoint3{0.0f, 0.0f, 0.0f};
	r.secondsLeft = any ? kSwimRequestSeconds : 0.0f;
}

bool AddSwimStep(const void* self, float dt, float* step) {
	SwimRequest& r = Request();
	if (!(r.secondsLeft > 0.0f) || step == nullptr || !(dt > 0.0f)) {
		return false;
	}
	r.secondsLeft -= dt;
	const float rotZ = *reinterpret_cast<const float*>(static_cast<const UInt8*>(self) + kRefRotZOffset);
	const NiPoint3 actor = vr::WorldStepToActor(r.velocity.x * dt, r.velocity.y * dt, rotZ);
	const NiPoint3 arms{actor.x, actor.y, r.velocity.z * dt};
	const NiPoint3 total = vr::CombineSwimStep(NiPoint3{step[0], step[1], step[2]}, arms);
	step[0] = total.x;
	step[1] = total.y;
	step[2] = total.z;
	return true;
}

float PlayerSwimSpeed() {
	SwimRequest& r = Request();
	void* const player = *reinterpret_cast<void* const*>(addr::kPlayerPointer);
	if (player == nullptr) {
		return 0.0f;
	}
	if (r.speedGetter == 0) {
		r.speedGetter = mem::Verify(kSwimWalkSpeed, kSpeedBytes, sizeof(kSpeedBytes)) ? 1 : 2;
	}
	return r.speedGetter == 1 ? reinterpret_cast<SpeedFn>(kSwimWalkSpeed)(player, nullptr) : 0.0f;
}

bool PlayerHeadingRadians(float& rotZ) {
	const UInt8* const player = *reinterpret_cast<const UInt8* const*>(addr::kPlayerPointer);
	if (player == nullptr) {
		return false;
	}
	rotZ = *reinterpret_cast<const float*>(player + kRefRotZOffset);
	return true;
}

}  // namespace obvr::game
