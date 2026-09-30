#pragma once

// The bow by hand, as in Blade & Sorcery (the tester, 2026-09-30: "Pfeil und
// Bogen wie in blade and sorcery. Linke hand hat ja bereits den Bogen. Jetzt
// muss rechte noch auf rechter schulter den Pfeil bekommen und an den bogen
// führen. Beim loslassen Schuss. Zielen im groben mit links mit dem bogen, im
// feinen mit rechts dem Pfeil").
//
// With the bow drawn, the weapon hand's grip closed at the quiver - over the
// shoulder on its own side - takes an arrow. Brought to the bow hand it is
// nocked; pulled back from there the engine's own draw begins (its attack
// control held, the way the trigger held it before); the grip let go looses
// it (the control let go, as vanilla looses on release). The arrow flies along
// the line from the drawing hand through the bow hand - the bow sets it
// roughly, the drawing hand finely.
//
// The engine still decides the draw's power (the time the control is held)
// and draws its own arrow on the string; OBVR decides when it is held and
// which way it flies. An arrow let go before the draw began is dropped: no
// shot.
//
// Positions are the controllers' in tracking space, metres; the quiver's place
// is in the body's frame (vr::BodyRelative: right, forward, up from the eyes).
// Pure, covered by archery_test.

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

struct ArcherySettings {
	bool enabled = true;
	// The quiver, for a right-handed archer: over the right shoulder, behind
	// it - where the two-hander hangs, which is drawn from there only with no
	// bow in hand. Mirrored when left-handed.
	NiPoint3 quiverZone{0.15f, -0.12f, -0.10f};
	float quiverRadius = 0.20f;
	// The drawing hand this near the bow hand nocks the arrow...
	float nockMetres = 0.15f;
	// ...and this much further back from there begins the draw.
	float drawStartMetres = 0.08f;
};

enum class ArrowState : UInt8 {
	None,     // no arrow in the hand
	InHand,   // taken from the quiver, not on the bow yet
	Nocked,   // on the string
	Drawing,  // pulled: the engine draws
};

struct ArcheryState {
	ArrowState state = ArrowState::None;
	bool gripWas = true;  // a grip closed when the bow comes out is not a take
};

struct ArcheryInput {
	bool bowDrawn = false;    // a bow drawn, in the world, no menu
	bool drawValid = false;   // the drawing hand (the weapon hand) tracked
	bool bowValid = false;    // the bow hand
	NiPoint3 drawBody{0.0f, 0.0f, 0.0f};  // the drawing hand, BodyRelative
	NiPoint3 drawAt{0.0f, 0.0f, 0.0f};    // tracking space, metres
	NiPoint3 bowAt{0.0f, 0.0f, 0.0f};
	bool drawGrip = false;
	bool leftHanded = false;  // the quiver mirrors
};

struct ArcheryVerdict {
	ArrowState state = ArrowState::None;
	bool attackHeld = false;  // the engine's attack control: the draw
	bool claimsGrip = false;  // the drawing hand's grip is the arrow's: no grab
	bool aiming = false;      // nocked or drawing: the shot goes along the arrow
	// What happened this frame, for the log.
	bool took = false;
	bool nocked = false;
	bool drawStarted = false;
	bool loosed = false;
	bool dropped = false;
	float handsApartMetres = 0.0f;
};

inline float MetresBetween(const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 d{a.x - b.x, a.y - b.y, a.z - b.z};
	return math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

inline bool AtQuiver(const ArcherySettings& s, const NiPoint3& drawBody, bool leftHanded) {
	const NiPoint3 zone{leftHanded ? -s.quiverZone.x : s.quiverZone.x, s.quiverZone.y, s.quiverZone.z};
	const NiPoint3 d{drawBody.x - zone.x, drawBody.y - zone.y, drawBody.z - zone.z};
	return d.x * d.x + d.y * d.y + d.z * d.z <= s.quiverRadius * s.quiverRadius;
}

inline ArcheryVerdict StepArchery(ArcheryState& st, const ArcheryInput& in, const ArcherySettings& s) {
	ArcheryVerdict v;
	const bool press = in.drawGrip && !st.gripWas;
	st.gripWas = in.drawGrip;
	if (!s.enabled || !in.bowDrawn || !in.drawValid || !in.bowValid) {
		// The bow went, or a hand: the arrow with it. A draw under way is not
		// loosed - the control simply stops being held.
		v.dropped = st.state != ArrowState::None;
		st.state = ArrowState::None;
		v.state = st.state;
		return v;
	}
	v.handsApartMetres = MetresBetween(in.drawAt, in.bowAt);
	switch (st.state) {
	case ArrowState::None:
		if (press && AtQuiver(s, in.drawBody, in.leftHanded)) {
			st.state = ArrowState::InHand;
			v.took = true;
		}
		break;
	case ArrowState::InHand:
		if (!in.drawGrip) {
			st.state = ArrowState::None;
			v.dropped = true;
		} else if (v.handsApartMetres <= s.nockMetres) {
			st.state = ArrowState::Nocked;
			v.nocked = true;
		}
		break;
	case ArrowState::Nocked:
		if (!in.drawGrip) {
			st.state = ArrowState::None;
			v.dropped = true;
		} else if (v.handsApartMetres >= s.nockMetres + s.drawStartMetres) {
			st.state = ArrowState::Drawing;
			v.drawStarted = true;
		}
		break;
	case ArrowState::Drawing:
		if (!in.drawGrip) {
			st.state = ArrowState::None;
			v.loosed = true;
		}
		break;
	}
	v.state = st.state;
	v.attackHeld = st.state == ArrowState::Drawing;
	v.claimsGrip = st.state != ArrowState::None || v.took;
	v.aiming = st.state == ArrowState::Nocked || st.state == ArrowState::Drawing;
	return v;
}

// The line kept a moment after the loose. The engine makes the arrow a few
// frames after the attack control goes up - the release key comes with the
// arrow still on the string (action 5), and it flies from frame 7
// (game/PlayerAim.h) - by which time the grip is open and the hand no longer
// on the string: the shot has to go along where the arrow was, not along the
// gaze. Kept for keepSeconds after the loose, or until the next arrow aims.
struct ArrowAimHold {
	float secondsLeft = 0.0f;
};

constexpr float kArrowAimKeepSeconds = 0.4f;

// Whether the kept line is still the shot's this frame.
inline bool StepArrowAimHold(ArrowAimHold& h, bool aimingNow, bool loosedNow, float dtSeconds) {
	if (aimingNow) {
		h.secondsLeft = 0.0f;
		return false;
	}
	if (loosedNow) {
		h.secondsLeft = kArrowAimKeepSeconds;
		return true;
	}
	if (h.secondsLeft > 0.0f && dtSeconds > 0.0f) {
		h.secondsLeft -= dtSeconds;
	}
	return h.secondsLeft > 0.0f;
}

// The arrow's line in tracking space: from the drawing hand through the bow
// hand. False when the hands are in one place.
inline bool ArrowLine(const NiPoint3& drawAt, const NiPoint3& bowAt, NiPoint3& direction) {
	const float length = MetresBetween(bowAt, drawAt);
	if (!(length > 1e-4f)) {
		return false;
	}
	direction = NiPoint3{(bowAt.x - drawAt.x) / length, (bowAt.y - drawAt.y) / length, (bowAt.z - drawAt.z) / length};
	return true;
}

}  // namespace obvr::vr
