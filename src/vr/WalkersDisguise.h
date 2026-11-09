#pragma once

// Oblivion Walkers' disguise (Walkers 4.20.1, 475c3f3; docs/walkers-disguise-spec.md):
// smearing a dead walker's blood over the chest makes the walkers take the
// player for one of theirs for a while. Walkers keeps the rules (the player
// sneaking, the body a walker's, how long); OBVR gives it the hands:
//
//   * the gesture: one hand inside a dead body, then - within a few seconds,
//     the same hand - at the chest (the stow spot, vr/Stow.h). OBVR then
//     sends 'WGBL' with the body's reference to Walkers, which starts the
//     disguise without its dialogue or says why not. OBVR only sends dead
//     bodies; which of them count is Walkers' call.
//   * while Walkers says 'WGDS' 1: a black-red vignette at the edge of the
//     view, the picture a little darker, blood on the player's hands - eased
//     in and out, gone at 'WGDS' 0.
//
// Pure, covered by walkers_disguise_test; the engine's side is
// game/WalkersLink.h (the messages), game/CorpseTouch.h (the hand in a body)
// and camera/hands/WalkersDisguise.h (the frame).

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

inline constexpr UInt32 kWalkersBloodMessage = 0x4C424757;     // 'WGBL': OBVR -> Walkers, the body's reference
inline constexpr UInt32 kWalkersDisguiseMessage = 0x53444757;  // 'WGDS': Walkers -> OBVR, 1 disguised, 0 over

struct DisguiseSettings {
	bool enabled = true;             // [Hands] WalkersDisguise
	float windowSeconds = 4.0f;      // from leaving the body to the chest
	float cooldownSeconds = 3.0f;    // after a gesture was sent, before the next
	float corpseReachUnits = 18.0f;  // how far round the body's spine a hand counts as in it
	float fadeInSeconds = 0.8f;
	float fadeOutSeconds = 1.5f;
	float vignetteStrength = 0.9f;   // the black-red ring's alpha at full
	float darken = 0.3f;             // the whole view's darkening at full (0..1)
	bool bloodyHands = true;
};

// 'WGDS': one word, 1 disguised or 0 over. Anything else is refused.
inline bool ParseWalkersDisguise(const void* data, UInt32 bytes, bool& disguised) {
	if (data == nullptr || bytes != sizeof(UInt32)) {
		return false;
	}
	const UInt32 word = *static_cast<const UInt32*>(data);
	if (word > 1u) {
		return false;
	}
	disguised = word == 1u;
	return true;
}

// A point's squared distance from the segment a..b.
inline float SegmentDistanceSquared(const NiPoint3& p, const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 ab = b - a;
	const float len2 = ab.LengthSquared();
	float t = 0.0f;
	if (len2 > 1e-6f) {
		const NiPoint3 ap = p - a;
		t = (ap.x * ab.x + ap.y * ab.y + ap.z * ab.z) / len2;
		t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	}
	const NiPoint3 q = a + ab * t;
	return (p - q).LengthSquared();
}

// A hand inside a body: within `reach` units of its trunk - from Spine2
// (`spine`) up to the head (`head`) and down past Spine2 as far again, where
// the pelvis is on a human's build (derived from the bones' proportions, not
// measured on every race). A body without both points is never touched.
inline bool HandInBody(const NiPoint3& hand, const NiPoint3& spine, const NiPoint3& head, float reach) {
	if (!(reach > 0.0f)) {
		return false;
	}
	const NiPoint3 down = spine - (head - spine);
	return SegmentDistanceSquared(hand, down, head) <= reach * reach;
}

// One hand, this frame.
struct DisguiseHandInput {
	bool valid = false;
	UInt32 deadBody = 0;  // the dead body the hand is inside (a form id), 0 none
	bool atChest = false; // the drawn hand in the chest's spot
};

struct DisguiseGestureState {
	UInt32 body[2] = {0, 0};          // the body each hand came out of, 0 none
	float sinceOut[2] = {0.0f, 0.0f}; // seconds since it left it
	float cooldown = 0.0f;
};

// Once a frame, the hands left [0] and right [1]. Returns the body to send as
// 'WGBL', 0 for none. `allowed`: the setting on, Full VR in the world, no
// menu, Walkers listening.
inline UInt32 StepDisguiseGesture(DisguiseGestureState& s, const DisguiseHandInput hands[2], bool allowed, float dt,
                                  const DisguiseSettings& settings) {
	if (!allowed || !settings.enabled) {
		s = DisguiseGestureState{};
		return 0;
	}
	if (s.cooldown > 0.0f) {
		s.cooldown -= dt;
	}
	for (UInt32 h = 0; h < 2; ++h) {
		const DisguiseHandInput& in = hands[h];
		if (!in.valid) {
			s.body[h] = 0;
			continue;
		}
		if (in.deadBody != 0) {
			// in it (again): the window starts when it comes out
			s.body[h] = in.deadBody;
			s.sinceOut[h] = 0.0f;
			continue;
		}
		if (s.body[h] == 0) {
			continue;
		}
		s.sinceOut[h] += dt;
		if (s.sinceOut[h] > settings.windowSeconds) {
			s.body[h] = 0;
			continue;
		}
		if (in.atChest && s.cooldown <= 0.0f) {
			const UInt32 body = s.body[h];
			s.body[0] = 0;
			s.body[1] = 0;
			s.cooldown = settings.cooldownSeconds;
			return body;
		}
	}
	return 0;
}

// The hand's stains (game/HandBlood.h), laid from several sides so one lands
// wherever the hand is turned (Nadi, 2026-11-09: "go a"): 0 from the eye -
// the side seen - then from either way along each of the hand bone's three
// axes (back, palm and edges, whichever axis each is on a skeleton). The
// direction the decal is projected in, unit; a hand at the eye goes down.
inline constexpr UInt32 kHandStainSides = 7;

inline NiPoint3 HandStainDirection(UInt32 side, const NiMatrix33& handRot, const NiPoint3& eyeToHand) {
	if (side == 0 || side >= kHandStainSides) {
		const float len2 = eyeToHand.LengthSquared();
		if (!(len2 > 1e-6f)) {
			return NiPoint3{0.0f, 0.0f, -1.0f};
		}
		return eyeToHand * (1.0f / math::Sqrt(len2));
	}
	const UInt32 axis = (side - 1) / 2;
	const float sign = (side - 1) % 2 == 0 ? -1.0f : 1.0f;
	// the axis's column in the world: the matrix applied to the unit axis
	const NiPoint3 unit{axis == 0 ? 1.0f : 0.0f, axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f};
	return (handRot * unit) * sign;
}

// The look while disguised: one level 0..1, eased towards `show`.
struct DisguiseLook {
	float level = 0.0f;
};

struct DisguiseLookOut {
	float vignetteAlpha = 0.0f;  // the black-red ring
	float darkenAlpha = 0.0f;    // the whole view
	bool bloodyHands = false;
};

inline DisguiseLookOut StepDisguiseLook(DisguiseLook& look, bool show, float dt, const DisguiseSettings& settings) {
	const bool on = show && settings.enabled;
	if (on) {
		const float rate = settings.fadeInSeconds > 0.0f ? dt / settings.fadeInSeconds : 1.0f;
		look.level = look.level + rate > 1.0f ? 1.0f : look.level + rate;
	} else {
		const float rate = settings.fadeOutSeconds > 0.0f ? dt / settings.fadeOutSeconds : 1.0f;
		look.level = look.level - rate < 0.0f ? 0.0f : look.level - rate;
	}
	DisguiseLookOut out;
	// eased both ways (smoothstep), so neither end snaps
	const float l = look.level;
	const float eased = l * l * (3.0f - 2.0f * l);
	out.vignetteAlpha = eased * settings.vignetteStrength;
	out.darkenAlpha = eased * settings.darken;
	out.bloodyHands = on && settings.bloodyHands;
	return out;
}

}  // namespace obvr::vr
