#pragma once

// A neck wound that bleeds on (docs/blade-penetration-spec.md section 12;
// Nadi, 2026-11-08: "go Blut"). A blade that went into the neck - skin or
// cloth, light armour only pushed through - without taking the head off
// leaves a wound that spurts with the heartbeat, the way the dismember's cut
// does (DismemberLogic.h BloodPulseDue, its pulse rule). The living bleed
// for so many pulses; after a death the heart goes on a few seconds and
// stops (B&S and its Realistic Blood mod: a body keeps pumping a while after
// the kill, then the wounds only drain); a corpse cut gives one spurt. A
// wound ends after a load (the world counter, as the cuts), when its actor
// is no longer loaded, or switched off. So many at once, the oldest giving
// way. Pure; neck_bleed_test.

#include "core/Types.h"
#include "game/BladeMaterial.h"
#include "game/BodyPart.h"
#include "game/DismemberLogic.h"

namespace obvr::game {

struct NeckBleedSettings {
	bool enabled = true;
	float pulseSeconds = 0.8f;  // a heartbeat under strain
	UInt32 livingPulses = 25;   // some 20 s of spurts while alive
	float deadSeconds = 6.0f;   // the heart goes on this long after a death
	UInt32 mostWounds = 4;
};

inline bool NeckWoundOpens(const NeckBleedSettings& s, bool entered, BodyPart part, BodyMaterial m, bool byPressure,
                           bool headOff) {
	const bool bleeds = m == BodyMaterial::Skin || m == BodyMaterial::Cloth || (m == BodyMaterial::Light && byPressure);
	return s.enabled && entered && part == BodyPart::Neck && bleeds && !headOff;
}

struct NeckWound {
	bool open = false;
	UInt32 actor = 0;
	UInt32 world = 0;
	bool corpse = false;  // cut after the death: one spurt
	float age = 0.0f;
	float deadFor = 0.0f;
	UInt32 spurts = 0;
	UInt32 serial = 0;  // the order they were opened, for the oldest
};

inline bool OpenNeckWound(NeckWound& w, UInt32 actor, UInt32 world, bool alive = true) {
	if (actor == 0) {
		w = NeckWound{};
		return false;
	}
	const UInt32 serial = w.serial;
	w = NeckWound{};
	w.open = true;
	w.actor = actor;
	w.world = world;
	w.corpse = !alive;
	w.serial = serial;
	return true;
}

struct NeckWoundFrame {
	bool there = false;  // the actor still loaded (read by the caller)
	bool alive = false;
	UInt32 worldNow = 0;
	float dtSeconds = 0.0f;
	bool enabled = true;
};

enum class NeckWoundEnd : UInt8 { None, Spent, HeartStopped, Load, Gone, Off };

struct NeckWoundStep {
	bool spurt = false;
	NeckWoundEnd end = NeckWoundEnd::None;
};

// The dismember's pulse rule (BloodPulseDue) with this wound's beat.
inline bool NeckPulseDue(const NeckBleedSettings& s, float age, UInt32 done, UInt32 most) {
	CutSettings c;
	c.blood = true;
	c.bloodPulses = most;
	c.bloodPulseSeconds = s.pulseSeconds;
	return BloodPulseDue(c, age, done);
}

inline NeckWoundStep StepNeckWound(NeckWound& w, const NeckBleedSettings& s, const NeckWoundFrame& f) {
	NeckWoundStep r;
	if (!w.open) {
		return r;
	}
	const float dt = f.dtSeconds > 0.0f && f.dtSeconds <= 1.0f ? f.dtSeconds : 0.0f;
	if (!s.enabled || !f.enabled) {
		r.end = NeckWoundEnd::Off;
	} else if (w.world == 0 || w.world != f.worldNow) {
		r.end = NeckWoundEnd::Load;
	} else if (!f.there) {
		r.end = NeckWoundEnd::Gone;
	}
	if (r.end == NeckWoundEnd::None) {
		w.age += dt;
		if (!f.alive) {
			w.deadFor += dt;
		}
		const bool stopped = w.corpse ? w.spurts >= 1 : (!f.alive && w.deadFor >= s.deadSeconds);
		if (stopped) {
			r.end = NeckWoundEnd::HeartStopped;
		} else if (NeckPulseDue(s, w.age, w.spurts, w.corpse ? 1u : s.livingPulses)) {
			r.spurt = true;
			++w.spurts;
		} else if (w.spurts >= s.livingPulses) {
			r.end = NeckWoundEnd::Spent;
		}
	}
	if (r.end != NeckWoundEnd::None) {
		const UInt32 serial = w.serial;
		w = NeckWound{};
		w.serial = serial;
	}
	return r;
}

inline constexpr UInt32 kNeckWoundsMax = 8;

struct NeckWounds {
	NeckWound w[kNeckWoundsMax];
	UInt32 next = 1;
};

inline UInt32 OpenNeckWounds(const NeckWounds& ws) {
	UInt32 n = 0;
	for (const NeckWound& w : ws.w) {
		n += w.open ? 1u : 0u;
	}
	return n;
}

// A wound in `actor`'s neck: the one there already opened anew, else a free
// slot, else - past the most - the oldest's. The slot's index, -1 none.
inline int AddNeckWound(NeckWounds& ws, const NeckBleedSettings& s, UInt32 actor, UInt32 world, bool alive) {
	if (actor == 0) {
		return -1;
	}
	const UInt32 most = s.mostWounds < kNeckWoundsMax ? s.mostWounds : kNeckWoundsMax;
	int at = -1;
	for (UInt32 i = 0; i < kNeckWoundsMax && at < 0; ++i) {
		at = ws.w[i].open && ws.w[i].actor == actor ? static_cast<int>(i) : -1;
	}
	if (at < 0 && OpenNeckWounds(ws) < most) {
		for (UInt32 i = 0; i < kNeckWoundsMax && at < 0; ++i) {
			at = !ws.w[i].open ? static_cast<int>(i) : -1;
		}
	}
	if (at < 0) {
		UInt32 oldest = 0xFFFFFFFFu;
		for (UInt32 i = 0; i < kNeckWoundsMax; ++i) {
			if (ws.w[i].open && ws.w[i].serial < oldest) {
				oldest = ws.w[i].serial;
				at = static_cast<int>(i);
			}
		}
	}
	if (at < 0) {
		return -1;
	}
	OpenNeckWound(ws.w[at], actor, world, alive);
	ws.w[at].serial = ws.next++;
	return at;
}

// What a wound is kept on, so that it follows the body: the neck's Havok
// body where the ragdoll has one; else the skeleton's own bones - the neck,
// else the head, else the upper spine (a Legion soldier has no Havok body
// on the neck, harness 2026-11-08: "no neck bone found"); none of them, the
// world where it opened.
enum class NeckAnchor : UInt8 { Ragdoll, NeckBone, HeadBone, SpineBone, World };

inline NeckAnchor PickNeckAnchor(bool ragdollPart, bool neckBone, bool headBone, bool spineBone) {
	if (ragdollPart) {
		return NeckAnchor::Ragdoll;
	}
	if (neckBone) {
		return NeckAnchor::NeckBone;
	}
	if (headBone) {
		return NeckAnchor::HeadBone;
	}
	return spineBone ? NeckAnchor::SpineBone : NeckAnchor::World;
}

// A bone read at the wound still holds it only while the actor's 3D is the
// one it was read under: a new 3D (resurrected, rebuilt) frees the old bones.
inline bool NeckAnchorHolds(UInt32 rootThen, UInt32 rootNow, UInt32 node) {
	return node != 0 && rootThen != 0 && rootThen == rootNow;
}

}  // namespace obvr::game
