#include "game/GrabPhysics.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(UInt32 address) { return mem::LooksLikeObjectAddress(address); }

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

// The grab being followed.
struct Held {
	UInt32 ref = 0;
	UInt32 body = 0;         // hkRigidBody
	UInt32 bodyVtable = 0;   // to see that the body is still what it was
	UInt32 wrapper = 0;      // bhkRigidBody
	UInt32 savedGroup = 0;
	bool grouped = false;
	VelocityHistory velocities;
};

Held g_held;

// The last pose the object was shown at in the hand, for the release.
struct SeenPose {
	UInt32 ref = 0;
	NiMatrix33 rot;
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	bool valid = false;
};
SeenPose g_seen;
UInt32 g_linesLeft = 8;

UInt32 PlayerOrZero() {
	const UInt32 player = Read(addr::kPlayerPointer);
	return LooksLikeObject(player) ? player : 0;
}

// The grabbed hkRigidBody, or zero: [[player+0x574]+8]+0x18.
UInt32 GrabbedBody(UInt32 player) {
	const UInt32 spring = Read(player + addr::kPlayerGrabSpringOffset);
	if (!LooksLikeObject(spring)) {
		return 0;
	}
	const UInt32 action = Read(spring + 8);
	if (!LooksLikeObject(action)) {
		return 0;
	}
	const UInt32 body = Read(action + 0x18);
	return LooksLikeObject(body) ? body : 0;
}

UInt32 PlayerGroup(UInt32 player) {
	using FilterOfFn = UInt32*(__thiscall*)(void* self, UInt32* out);
	UInt32 filter = 0;
	UInt32* const got =
		reinterpret_cast<FilterOfFn>(kControllerFilterOf)(reinterpret_cast<void*>(player), &filter);
	const UInt32 group = got != nullptr ? FilterGroup(*got) : 0;
	return group != 0 ? group : kPlayerCollisionGroupFallback;
}

void SetGroup(UInt32 wrapper, UInt32 group) {
	using SetGroupFn = void(__thiscall*)(void* self, UInt32 group);
	reinterpret_cast<SetGroupFn>(kSetBodyGroup)(reinterpret_cast<void*>(wrapper), group);
}

// Whether the body saved at the start is still there and still a body.
bool BodyStillThere(const Held& h) {
	if (!LooksLikeObject(h.body) || Read(h.body) != h.bodyVtable) {
		return false;
	}
	const UInt32 node = Read(h.ref + addr::kRefNiNodeOffset);
	return LooksLikeObject(node) && Read(h.body + kBodyWrapperOffset) == h.wrapper;
}

void Released(Held& h, float throwStrength) {
	if (!BodyStillThere(h)) {
		if (g_linesLeft > 0) {
			--g_linesLeft;
			OBVR_LOG("Hands: the held object went away with its hold - nothing put back");
		}
		return;
	}
	if (h.grouped) {
		SetGroup(h.wrapper, h.savedGroup);
	}
	// In the hand, the body goes to where the object was seen, so letting go
	// does not jump it to where the spring had dragged it.
	bool placed = false;
	if (g_seen.valid && g_seen.ref == h.ref) {
		alignas(16) float pos[4] = {g_seen.pos.x * kHavokPerUnit, g_seen.pos.y * kHavokPerUnit,
		                            g_seen.pos.z * kHavokPerUnit, 0.0f};
		alignas(16) float rot[4];
		QuaternionFromRotation(g_seen.rot, rot);
		const UInt32 slot = Read(Read(h.wrapper) + kBodySetTranslationAndRotationSlot);
		if (LooksLikeObject(slot)) {
			using LockFn = void(__thiscall*)(void* lock);
			using PlaceFn = void(__thiscall*)(void* body, const float* pos, const float* rot);
			void* const lock = reinterpret_cast<void*>(kHavokLock);
			reinterpret_cast<LockFn>(kHavokLockEnter)(lock);
			reinterpret_cast<PlaceFn>(slot)(reinterpret_cast<void*>(h.wrapper), pos, rot);
			reinterpret_cast<LockFn>(kHavokLockLeave)(lock);
			placed = true;
		}
	}
	g_seen = SeenPose{};
	const NiPoint3 v = ThrowVelocity(h.velocities, throwStrength);
	const bool thrown = v.LengthSquared() > 0.0f;
	if (thrown) {
		const UInt32 motion = Read(h.body + kBodyMotionOffset);
		if (LooksLikeObject(motion) && LooksLikeObject(Read(motion))) {
			using ActivateFn = void(__thiscall*)(void* body);
			reinterpret_cast<ActivateFn>(kActivateBody)(reinterpret_cast<void*>(h.body));
			alignas(16) float havok[4] = {v.x * kHavokPerUnit, v.y * kHavokPerUnit,
			                              v.z * kHavokPerUnit, 0.0f};
			using SetVelocityFn = void(__thiscall*)(void* motion, const float* v);
			const UInt32 slot = Read(Read(motion) + kMotionSetLinearVelocitySlot);
			reinterpret_cast<SetVelocityFn>(slot)(reinterpret_cast<void*>(motion), havok);
		}
	}
	if (g_linesLeft > 0) {
		--g_linesLeft;
		OBVR_LOG("Hands: let go of %08X - group %u put back, %s%s (%.0f units/s)", h.ref,
		         h.savedGroup, placed ? "placed where it was seen, " : "",
		         thrown ? "thrown with the hand's speed" : "set down",
		         static_cast<double>(math::Sqrt(v.LengthSquared())));
	}
}

}  // namespace

void NoteHeldPose(UInt32 ref, const NiMatrix33& rot, const NiPoint3& pos) {
	g_seen.ref = ref;
	g_seen.rot = rot;
	g_seen.pos = pos;
	g_seen.valid = true;
}

void StepGrabPhysics(bool passBody, float throwStrength, bool velocityValid,
                     const NiPoint3& velocityUnits) {
	const UInt32 player = PlayerOrZero();
	const UInt32 body = player != 0 ? GrabbedBody(player) : 0;
	const UInt32 ref = player != 0 ? Read(player + addr::kPlayerGrabbedRefOffset) : 0;

	if (g_held.body != 0 && body != g_held.body) {
		// The engine let go (or took something else): this is the release.
		Released(g_held, throwStrength);
		g_held = Held{};
	}
	if (body == 0 || !LooksLikeObject(ref)) {
		return;
	}
	if (g_held.body == 0) {
		g_held.ref = ref;
		g_held.body = body;
		g_held.bodyVtable = Read(body);
		g_held.wrapper = Read(body + kBodyWrapperOffset);
		if (passBody && LooksLikeObject(g_held.wrapper)) {
			const UInt32 filter = Read(body + kBodyFilterOffset);
			g_held.savedGroup = FilterGroup(filter);
			const UInt32 group = PlayerGroup(player);
			if (group != g_held.savedGroup) {
				SetGroup(g_held.wrapper, group);
				g_held.grouped = true;
			}
			if (g_linesLeft > 0) {
				--g_linesLeft;
				OBVR_LOG("Hands: holding %08X - its filter %08X, group %u -> the player's %u, so "
				         "it passes through the body",
				         ref, filter, g_held.savedGroup, group);
			}
		}
	}
	if (velocityValid) {
		PushVelocity(g_held.velocities, velocityUnits);
	}
}

}  // namespace obvr::game
