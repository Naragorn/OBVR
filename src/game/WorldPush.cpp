#include "game/WorldPush.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GrabPhysics.h"
#include "game/PlayerTeleport.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

struct Last {
	bool have = false;
	PushSegment segment;
};
Last g_last[static_cast<int>(Pusher::Count)];

UInt32 g_pushLinesLeft = 24;
UInt32 g_ownerLinesLeft = 3;

// The rigid body a pick's collidable belongs to, or zero when it is not one.
UInt32 BodyOfCollidable(UInt32 collidable) {
	if (!LooksLikeObject(collidable)) {
		return 0;
	}
	if (*reinterpret_cast<const UInt8*>(collidable + kCollidableTypeOffset) != kCollidableOwnerEntity) {
		return 0;
	}
	const SInt32 offset = *reinterpret_cast<const SInt32*>(collidable + kCollidableOwnerOffset);
	const UInt32 body = collidable + static_cast<UInt32>(offset);
	if (!LooksLikeObject(body) || body + kBodyCollidableOffset != collidable) {
		if (g_ownerLinesLeft > 0) {
			--g_ownerLinesLeft;
			OBVR_LOG("Hands: a pick hit collidable %08X whose owner offset %d does not lead to a "
			         "body keeping it at +0x14 - left alone", collidable, offset);
		}
		return 0;
	}
	return body;
}

const char* PusherName(Pusher p, bool blade) {
	if (p == Pusher::WeaponHand) {
		return blade ? "the weapon" : "the weapon hand";
	}
	return "the other hand";
}

// Pushes the body a ray met, once a frame at most: answers whether it did.
bool Push(UInt32 body, const NiPoint3& pusherUnitsPerSecond, Pusher who, bool blade) {
	const UInt32 motion = Read(body + kBodyMotionOffset);
	if (!LooksLikeObject(motion) || !LooksLikeObject(Read(motion))) {
		return false;
	}
	const UInt32 vtable = Read(motion);
	using TypeFn = UInt32(__thiscall*)(void* motion);
	const UInt32 typeSlot = Read(vtable + kMotionTypeSlot);
	const UInt32 setSlot = Read(vtable + kMotionSetLinearVelocitySlot);
	if (!LooksLikeObject(typeSlot) || !LooksLikeObject(setSlot)) {
		return false;
	}
	const UInt32 type = reinterpret_cast<TypeFn>(typeSlot)(reinterpret_cast<void*>(motion)) & 0xFF;
	if (!MotionTypeDrivable(type)) {
		return false;  // a wall, the ground, a living actor's keyframed bones
	}
	const float* const v = reinterpret_cast<const float*>(motion + kMotionLinearVelocityOffset);
	const NiPoint3 bodyV{v[0], v[1], v[2]};
	const NiPoint3 pusherV = pusherUnitsPerSecond * kHavokPerUnit;
	NiPoint3 pushed;
	if (!PushedVelocity(bodyV, pusherV, pushed)) {
		return false;
	}
	using ActivateFn = void(__thiscall*)(void* body);
	using SetVectorFn = void(__thiscall*)(void* motion, const float* v);
	reinterpret_cast<ActivateFn>(kActivateBody)(reinterpret_cast<void*>(body));
	alignas(16) float linear[4] = {pushed.x, pushed.y, pushed.z, 0.0f};
	reinterpret_cast<SetVectorFn>(setSlot)(reinterpret_cast<void*>(motion), linear);
	if (g_pushLinesLeft > 0) {
		--g_pushLinesLeft;
		OBVR_LOG("Hands: %s pushed body %08X (motion type %u) - %.2f m/s before, %.2f m/s after, "
		         "the pusher at %.2f m/s",
		         PusherName(who, blade), body, type,
		         static_cast<double>(math::Sqrt(bodyV.LengthSquared()) / 10.0f),
		         static_cast<double>(math::Sqrt(pushed.LengthSquared()) / 10.0f),
		         static_cast<double>(math::Sqrt(pusherV.LengthSquared()) / 10.0f));
	}
	return true;
}

// The motion type of a rigid body, through its motion's vtable.
bool MotionTypeOf(UInt32 body, UInt32& type) {
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

}  // namespace

bool ReadPickBody(UInt32 collidable, UInt32& body, UInt32& motionType, UInt32& layer) {
	body = BodyOfCollidable(collidable);
	if (body == 0 || !MotionTypeOf(body, motionType)) {
		return false;
	}
	layer = Read(body + kBodyFilterOffset) & 0x7F;
	return true;
}

bool KickBodyByBlade(UInt32 body, const NiPoint3& unitsPerSecond) {
	return LooksLikeObject(body) && body != HeldBody() && Push(body, unitsPerSecond, Pusher::WeaponHand, true);
}

void StepWorldPush(bool enabled, const PushFrame& frame) {
	const int count = static_cast<int>(Pusher::Count);
	if (!enabled) {
		for (int i = 0; i < count; ++i) {
			g_last[i] = Last{};
		}
		return;
	}
	const bool timed = frame.dtSeconds > 0.0f && frame.dtSeconds <= kPushMaxFrameSeconds;
	const UInt32 held = HeldBody();
	UInt32 pushed[8] = {};
	UInt32 pushedCount = 0;
	for (int i = 0; i < count; ++i) {
		if (!frame.valid[i]) {
			g_last[i] = Last{};
			continue;
		}
		const PushSegment now = frame.segment[i];
		if (g_last[i].have && timed) {
			const PushSegment last = g_last[i].segment;
			PushRay rays[kPushRays];
			const UInt32 rayCount = PushRaysFor(last, now, rays);
			for (UInt32 r = 0; r < rayCount; ++r) {
				WorldPick hit;
				if (!PickWorldSegment(rays[r].from, rays[r].to, hit, kLayerClutter) || !hit.hit) {
					continue;
				}
				const UInt32 body = BodyOfCollidable(hit.collidable);
				if (body == 0 || body == held) {
					continue;
				}
				bool seen = false;
				for (UInt32 k = 0; k < pushedCount; ++k) {
					seen = seen || pushed[k] == body;
				}
				if (seen) {
					continue;
				}
				const float s = PushPointOnRay(rays[r], hit.fraction);
				const NiPoint3 v = PusherVelocityAt(last, now, s, frame.dtSeconds);
				if (Push(body, v, static_cast<Pusher>(i), frame.blade[i]) &&
				    pushedCount < sizeof(pushed) / sizeof(pushed[0])) {
					pushed[pushedCount++] = body;
				}
			}
		}
		g_last[i].have = true;
		g_last[i].segment = now;
	}
}

}  // namespace obvr::game
