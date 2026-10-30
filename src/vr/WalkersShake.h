#pragma once

// Shaking a walker off in VR (Oblivion Walkers' grab, its docs/walkers-spec.md
// 4.19 "VR shake"; Nadi, 2026-10-28: "abschütteln in vr, ähnlich wie saints and
// sinners", 2026-10-30: "mit ui einblendung dass der user controller shaken
// muss wie in saints and sinners"). Walkers decides who holds which arm, how
// many strokes free it and when a holder bites; OBVR is the sensor and the
// body: it counts the held hand's strokes and sends each one, shows the call
// at the held hand, jolts that hand at a grab and a bite, flashes red at a
// bite and keeps the stick from walking off while held (2026-10-30, at the
// headset: "griff kommt zwar aber ich kann einfach weg").
//
// The messages (xOBSE's messaging, Walkers' GrabLogic.h):
// - 'WGVR' to Walkers once OBVR runs: one word, 1 in VR - Walkers then says no
//   line and leaves the grab to the body.
// - 'WGRB' from Walkers at each grab and let-go: the count, then each holder's
//   reference and arm (1 left, 2 right).
// - 'WGBT' from Walkers at each bite: the arm, which bite, of how many, 1 for
//   the last.
// - 'WGSK' to Walkers at each stroke of a held hand: one word, the arm.
//
// A stroke is a turn of the hand's way after enough travel and speed: the
// farthest point from where the last stroke turned is kept, and once the hand
// has come back from it by kShakeReturnMetres, with the travel at least
// strokeMetres and the fastest frame at least strokeSpeed, that was a stroke,
// and the far point is where the next one starts. Any way counts - up and
// down, side to side - as players shake in Saints & Sinners. Pure, covered by
// walkers_shake_test.

#include <cstdio>

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

inline constexpr UInt32 kWalkersVrMessage = 0x52564757;     // 'WGVR'
inline constexpr UInt32 kWalkersHoldsMessage = 0x42524757;  // 'WGRB'
inline constexpr UInt32 kWalkersBiteMessage = 0x54424757;   // 'WGBT'
inline constexpr UInt32 kWalkersShakeMessage = 0x4B534757;  // 'WGSK'
inline constexpr const char* kWalkersPluginName = "OblivionWalkers";

inline constexpr UInt32 kWalkersArmLeft = 1;
inline constexpr UInt32 kWalkersArmRight = 2;
inline constexpr UInt32 kWalkersMostHolders = 2;

inline constexpr float kShakeReturnMetres = 0.04f;

struct ShakeSettings {
	float strokeMetres = 0.12f;  // [Hands] ShakeStrokeMetres
	float strokeSpeed = 0.8f;    // [Hands] ShakeStrokeSpeed, metres a second
};

struct ShakeStrokeState {
	bool started = false;
	NiPoint3 anchor{0.0f, 0.0f, 0.0f};   // where the last stroke turned
	NiPoint3 extreme{0.0f, 0.0f, 0.0f};  // the farthest point from it since
	NiPoint3 last{0.0f, 0.0f, 0.0f};
	float extremeMetres = 0.0f;
	float peakSpeed = 0.0f;
};

inline void ResetShakeStroke(ShakeStrokeState& s) { s = ShakeStrokeState{}; }

inline float ShakeDistance(const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 d{a.x - b.x, a.y - b.y, a.z - b.z};
	const float squared = d.x * d.x + d.y * d.y + d.z * d.z;
	// Newton's root, as the logic headers keep away from <cmath>'s sqrt.
	if (squared <= 0.0f) {
		return 0.0f;
	}
	float r = squared > 1.0f ? squared : 1.0f;
	for (int i = 0; i < 20; ++i) {
		r = 0.5f * (r + squared / r);
	}
	return r;
}

// One frame of the held hand at `position` (tracking space, metres): true at
// a stroke. A lost controller starts the stroke anew.
inline bool StepShakeStroke(ShakeStrokeState& s, const ShakeSettings& settings, const NiPoint3& position,
                            float dt, bool valid) {
	if (!valid) {
		ResetShakeStroke(s);
		return false;
	}
	if (!s.started) {
		s.started = true;
		s.anchor = s.extreme = s.last = position;
		s.extremeMetres = 0.0f;
		s.peakSpeed = 0.0f;
		return false;
	}
	if (!(dt > 0.0f)) {
		return false;
	}
	const float speed = ShakeDistance(position, s.last) / dt;
	s.last = position;
	if (speed > s.peakSpeed) {
		s.peakSpeed = speed;
	}
	const float out = ShakeDistance(position, s.anchor);
	if (out > s.extremeMetres) {
		s.extremeMetres = out;
		s.extreme = position;
	}
	const float back = ShakeDistance(position, s.extreme);
	if (s.extremeMetres >= settings.strokeMetres && back >= kShakeReturnMetres &&
	    s.peakSpeed >= settings.strokeSpeed) {
		s.anchor = s.extreme;
		s.extreme = position;
		s.extremeMetres = back;
		s.peakSpeed = speed;
		return true;
	}
	return false;
}

struct WalkersHolds {
	UInt32 count = 0;
	UInt32 refId[kWalkersMostHolders] = {0, 0};
	UInt32 arm[kWalkersMostHolders] = {0, 0};
};

inline bool ParseWalkersHolds(const void* data, UInt32 bytes, WalkersHolds& out) {
	if (data == nullptr || bytes < 4) {
		return false;
	}
	const UInt32* const words = static_cast<const UInt32*>(data);
	const UInt32 count = words[0];
	if (count > kWalkersMostHolders || bytes < 4 * (1 + 2 * count)) {
		return false;
	}
	WalkersHolds h;
	h.count = count;
	for (UInt32 i = 0; i < count; ++i) {
		h.refId[i] = words[1 + 2 * i];
		h.arm[i] = words[2 + 2 * i];
		if (h.arm[i] != kWalkersArmLeft && h.arm[i] != kWalkersArmRight) {
			return false;
		}
	}
	out = h;
	return true;
}

inline UInt32 HolderOf(const WalkersHolds& h, UInt32 arm) {
	for (UInt32 i = 0; i < h.count && i < kWalkersMostHolders; ++i) {
		if (h.arm[i] == arm) {
			return h.refId[i];
		}
	}
	return 0;
}

inline bool ArmHeld(const WalkersHolds& h, UInt32 arm) { return HolderOf(h, arm) != 0; }

// The stick does not walk while a walker holds the player (Walkers' movement
// flags do not reach OBVR's own locomotion).
inline bool WalkLocked(const WalkersHolds& h, bool enabled) { return enabled && h.count > 0; }

struct WalkersBite {
	UInt32 arm = 0;
	UInt32 number = 0;
	UInt32 toDie = 0;
	bool lethal = false;
};

inline bool ParseWalkersBite(const void* data, UInt32 bytes, WalkersBite& out) {
	if (data == nullptr || bytes < 16) {
		return false;
	}
	const UInt32* const words = static_cast<const UInt32*>(data);
	if (words[0] != kWalkersArmLeft && words[0] != kWalkersArmRight) {
		return false;
	}
	out.arm = words[0];
	out.number = words[1];
	out.toDie = words[2];
	out.lethal = words[3] != 0;
	return true;
}

// The red flash at a bite, `since` seconds after it (negative: none): up in
// kBiteFlashRiseSeconds, then fading out by kBiteFlashSeconds; stronger at the
// last bite.
inline constexpr float kBiteFlashRiseSeconds = 0.08f;
inline constexpr float kBiteFlashSeconds = 0.8f;

inline float BiteFlashAlpha(float since, bool lethal) {
	if (!(since >= 0.0f) || since >= kBiteFlashSeconds) {
		return 0.0f;
	}
	const float peak = lethal ? 0.95f : 0.65f;
	if (since < kBiteFlashRiseSeconds) {
		return peak * (0.5f + 0.5f * since / kBiteFlashRiseSeconds);
	}
	return peak * (1.0f - (since - kBiteFlashRiseSeconds) / (kBiteFlashSeconds - kBiteFlashRiseSeconds));
}

// The call at the held hand, and under it the bites so far.
inline constexpr const char* kShakePromptText = "Shake it off!";

inline void ShakePromptText(UInt32 bites, UInt32 toDie, char* line, UInt32 size) {
	if (size == 0) {
		return;
	}
	if (bites == 0 || toDie == 0) {
		line[0] = '\0';
		return;
	}
	std::snprintf(line, size, "Bitten %u of %u", bites, toDie);
}

// True when the arm's controller is the one in the right role (g_hand's
// "right"): the arm is the body's, its controller the physical one on that
// side, which a left-hander's swapped roles call the left.
inline bool ShakePromptHand(UInt32 arm, bool rolesSwapped) { return (arm == kWalkersArmRight) != rolesSwapped; }

}  // namespace obvr::vr
