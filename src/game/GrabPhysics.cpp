#include "game/GrabPhysics.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"

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

// Let go while still inside the player's capsule: the old group waits until
// the object is clear of it (InsidePlayerCapsule).
constexpr UInt32 kMaxWaiting = 4;
struct Waiting {
	Held held;
	UInt32 frames = 0;
};
Waiting g_waiting[kMaxWaiting];
UInt32 g_waitingCount = 0;

// The object's flight after a release, for the log: its speed each frame.
constexpr UInt32 kFlightFrames = 12;
struct Flight {
	UInt32 ref = 0;
	NiPoint3 last{0.0f, 0.0f, 0.0f};
	bool haveLast = false;
	float speeds[kFlightFrames] = {};
	UInt32 count = 0;
	float given = 0.0f;  // what OBVR set, units a second; 0 for none
	bool inside = false;
};
Flight g_flight;
UInt32 g_flightLinesLeft = 12;

// The last pose the object was shown at in the hand, for the release.
struct SeenPose {
	UInt32 ref = 0;
	NiMatrix33 rot;
	NiPoint3 pos{0.0f, 0.0f, 0.0f};
	bool valid = false;
};
SeenPose g_seen;
UInt32 g_linesLeft = 12;

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

// The capsule's radius in game units the way the grab update takes it, or
// zero when the controller cannot be asked.
float PlayerCapsuleUnits(UInt32 player) {
	using ControllerOfFn = void*(__thiscall*)(void* actor);
	using RadiusFn = float(__thiscall*)(void* controller);
	void* const controller =
		reinterpret_cast<ControllerOfFn>(kControllerOf)(reinterpret_cast<void*>(player));
	if (!LooksLikeObject(reinterpret_cast<UInt32>(controller)) ||
	    !LooksLikeObject(Read(reinterpret_cast<UInt32>(controller) + 0x374))) {
		return 0.0f;
	}
	const float radius = reinterpret_cast<RadiusFn>(kControllerRadius)(controller);
	if (!(radius > 0.0f && radius < 100.0f)) {
		return 0.0f;
	}
	return radius * kControllerRadiusToUnits + kControllerRadiusMarginUnits;
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

// Whether the held object's bound is still inside the player's capsule; false
// when it cannot be told (then the group goes back at once, as before).
bool StillInsidePlayer(UInt32 player, const Held& h, float& capsuleOut, float& distanceOut) {
	capsuleOut = 0.0f;
	distanceOut = 0.0f;
	const UInt32 nodeAddress = Read(h.ref + addr::kRefNiNodeOffset);
	if (player == 0 || !LooksLikeObject(nodeAddress)) {
		return false;
	}
	const float capsule = PlayerCapsuleUnits(player);
	if (capsule <= 0.0f) {
		return false;
	}
	const auto* node = reinterpret_cast<const NiAVObject*>(nodeAddress);
	const NiPoint3 playerPos = *reinterpret_cast<const NiPoint3*>(player + addr::kRefPositionOffset);
	const NiPoint3& centre = node->worldBound.center;
	const float dx = centre.x - playerPos.x;
	const float dy = centre.y - playerPos.y;
	capsuleOut = capsule;
	distanceOut = math::Sqrt(dx * dx + dy * dy);
	return InsidePlayerCapsule(centre, node->worldBound.radius, playerPos, capsule);
}

void Wait(const Held& h) {
	if (g_waitingCount == kMaxWaiting) {
		// Full: the oldest gets its group back now.
		if (BodyStillThere(g_waiting[0].held)) {
			SetGroup(g_waiting[0].held.wrapper, g_waiting[0].held.savedGroup);
		}
		for (UInt32 i = 1; i < kMaxWaiting; ++i) {
			g_waiting[i - 1] = g_waiting[i];
		}
		--g_waitingCount;
	}
	g_waiting[g_waitingCount].held = h;
	g_waiting[g_waitingCount].frames = 0;
	++g_waitingCount;
}

void DropWaiting(UInt32 index) {
	for (UInt32 i = index + 1; i < g_waitingCount; ++i) {
		g_waiting[i - 1] = g_waiting[i];
	}
	--g_waitingCount;
}

// The waiting ones: the group back once clear, forgotten once gone.
void StepWaiting(UInt32 player) {
	for (UInt32 i = 0; i < g_waitingCount;) {
		Waiting& w = g_waiting[i];
		++w.frames;
		if (!BodyStillThere(w.held)) {
			DropWaiting(i);
			continue;
		}
		float capsule = 0.0f;
		float distance = 0.0f;
		if (StillInsidePlayer(player, w.held, capsule, distance)) {
			++i;
			continue;
		}
		SetGroup(w.held.wrapper, w.held.savedGroup);
		if (g_linesLeft > 0) {
			--g_linesLeft;
			OBVR_LOG("Hands: %08X is clear of the player (%.0f units out, capsule %.0f) after %u "
			         "frames - group %u put back",
			         w.held.ref, static_cast<double>(distance), static_cast<double>(capsule),
			         w.frames, w.held.savedGroup);
		}
		DropWaiting(i);
	}
}

void Released(UInt32 player, Held& h, float throwStrength) {
	if (!BodyStillThere(h)) {
		if (g_linesLeft > 0) {
			--g_linesLeft;
			OBVR_LOG("Hands: the held object went away with its hold - nothing put back");
		}
		return;
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
	// The old group back now, or once the object is clear of the capsule.
	float capsule = 0.0f;
	float distance = 0.0f;
	const bool inside = h.grouped && StillInsidePlayer(player, h, capsule, distance);
	if (h.grouped) {
		if (inside) {
			Wait(h);
		} else {
			SetGroup(h.wrapper, h.savedGroup);
		}
	}
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
	const float given = math::Sqrt(v.LengthSquared());
	if (g_linesLeft > 0) {
		--g_linesLeft;
		OBVR_LOG("Hands: let go of %08X - %s, %s%s (%.0f units/s)", h.ref,
		         !h.grouped ? "its group untouched"
		         : inside   ? "inside the player's capsule, its group waits until it is clear"
		                    : "clear of the player, its group put back",
		         placed ? "placed where it was seen, " : "",
		         thrown ? "thrown with the hand's speed" : "set down",
		         static_cast<double>(given));
		if (h.grouped && capsule > 0.0f) {
			OBVR_LOG("Hands: ... its middle %.0f units from the player's axis, capsule %.0f",
			         static_cast<double>(distance), static_cast<double>(capsule));
		}
	}
	g_flight = Flight{};
	g_flight.ref = h.ref;
	g_flight.given = given;
	g_flight.inside = inside;
}

// The flight after the release: the node follows the body again, so its
// movement is the body's speed. One line once the frames are in.
void StepFlight(float dtSeconds) {
	if (g_flight.ref == 0) {
		return;
	}
	const UInt32 nodeAddress = Read(g_flight.ref + addr::kRefNiNodeOffset);
	if (!LooksLikeObject(nodeAddress)) {
		g_flight = Flight{};
		return;
	}
	const NiPoint3 pos = reinterpret_cast<const NiAVObject*>(nodeAddress)->worldTransform.pos;
	if (g_flight.haveLast && dtSeconds > 0.0f) {
		const NiPoint3 d = pos - g_flight.last;
		g_flight.speeds[g_flight.count++] = math::Sqrt(d.LengthSquared()) / dtSeconds;
	}
	g_flight.last = pos;
	g_flight.haveLast = true;
	if (g_flight.count < kFlightFrames) {
		return;
	}
	if (g_flightLinesLeft > 0) {
		--g_flightLinesLeft;
		const float* s = g_flight.speeds;
		OBVR_LOG("Hands: flight of %08X after the release (%s, OBVR gave %.0f units/s), units/s "
		         "per frame: %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f",
		         g_flight.ref, g_flight.inside ? "let go inside the capsule" : "let go outside",
		         static_cast<double>(g_flight.given), static_cast<double>(s[0]),
		         static_cast<double>(s[1]), static_cast<double>(s[2]), static_cast<double>(s[3]),
		         static_cast<double>(s[4]), static_cast<double>(s[5]), static_cast<double>(s[6]),
		         static_cast<double>(s[7]), static_cast<double>(s[8]), static_cast<double>(s[9]),
		         static_cast<double>(s[10]), static_cast<double>(s[11]));
	}
	g_flight = Flight{};
}

}  // namespace

void NoteHeldPose(UInt32 ref, const NiMatrix33& rot, const NiPoint3& pos) {
	g_seen.ref = ref;
	g_seen.rot = rot;
	g_seen.pos = pos;
	g_seen.valid = true;
}

void StepGrabPhysics(bool passBody, float throwStrength, bool velocityValid,
                     const NiPoint3& velocityUnits, float dtSeconds) {
	const UInt32 player = PlayerOrZero();
	const UInt32 body = player != 0 ? GrabbedBody(player) : 0;
	const UInt32 ref = player != 0 ? Read(player + addr::kPlayerGrabbedRefOffset) : 0;

	if (g_held.body != 0 && body != g_held.body) {
		// The engine let go (or took something else): this is the release.
		Released(player, g_held, throwStrength);
		g_held = Held{};
	}
	StepWaiting(player);
	StepFlight(dtSeconds);
	if (body == 0 || !LooksLikeObject(ref)) {
		return;
	}
	if (g_held.body == 0) {
		g_held.ref = ref;
		g_held.body = body;
		g_held.bodyVtable = Read(body);
		g_held.wrapper = Read(body + kBodyWrapperOffset);
		// Taken again while its group still waits: the group it had before
		// the first hold is the one to keep, not the player's it still has.
		bool rewaited = false;
		UInt32 waitedGroup = 0;
		for (UInt32 i = 0; i < g_waitingCount; ++i) {
			if (g_waiting[i].held.body == body) {
				rewaited = true;
				waitedGroup = g_waiting[i].held.savedGroup;
				DropWaiting(i);
				break;
			}
		}
		if (passBody && LooksLikeObject(g_held.wrapper)) {
			const UInt32 filter = Read(body + kBodyFilterOffset);
			g_held.savedGroup = rewaited ? waitedGroup : FilterGroup(filter);
			const UInt32 group = PlayerGroup(player);
			if (group != FilterGroup(filter)) {
				SetGroup(g_held.wrapper, group);
			}
			g_held.grouped = group != g_held.savedGroup;
			if (g_linesLeft > 0) {
				--g_linesLeft;
				OBVR_LOG("Hands: holding %08X - its filter %08X, group %u -> the player's %u, so "
				         "it passes through the body%s",
				         ref, filter, g_held.savedGroup, group,
				         rewaited ? " (taken again before its group was back)" : "");
			}
		} else if (rewaited && LooksLikeObject(g_held.wrapper)) {
			SetGroup(g_held.wrapper, waitedGroup);
		}
	}
	if (velocityValid) {
		PushVelocity(g_held.velocities, velocityUnits);
	}
}

}  // namespace obvr::game
