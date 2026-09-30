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

// What is seen: the arrow in the drawing hand from the quiver on, on the
// string from the nock, and the string where that hand pulls it
// (game/BowVisual). The engine's own arrow and string follow its draw
// animation, which the hands no longer play.
//
// From the game's files (pyffi over Oblivion - Meshes.bsa, 2026-09-30):
//   * an arrow's model, "Arrow:0" in its ammunition's quiver, has its head at
//     y = 0 and runs back along -y to the nock (46.6 units on the iron one,
//     43 to 58 on the others: the length is read from the model);
//   * a bow's frame has the shot along +x; its string rests at x = -15.6 and
//     the "BowMorph" target, at weight 1, takes the string's middle 28 units
//     further back (25 to 29 by bow: read from the morph);
//   * bowattack.kf, at full draw, lays the arrow across the bow at (0, 2.8,
//     -2.45) in the bow's frame - beside the grip, not through it.
constexpr float kArrowRestOnBowY = 2.8f;
constexpr float kArrowRestOnBowZ = -2.45f;
// The nock this far behind the drawing fist's grip point (the right-hand
// Weapon node, where a sword's handle is held: the palm sits 0.9 units behind
// it), so the fletching shows behind the fingers.
constexpr float kArrowNockBehindGripUnits = 3.0f;

// The string's weight from how far the nock is behind the arrow's rest on the
// bow: 0 with it no further back than the string at rest, 1 a full draw's
// travel further. From the distance, not along the bow's own shot axis, so a
// bow held turned in the hand still shows the pull the hand makes.
inline float StringWeightAt(float nockFromRest, float stringFromRest, float travelUnits) {
	if (!(travelUnits > 0.0f)) {
		return 0.0f;
	}
	const float w = (nockFromRest - stringFromRest) / travelUnits;
	return w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
}

// Whether the string is OBVR's to set this frame, and from what.
enum class StringSource : UInt8 {
	Engine,  // the game's animation has it
	Hand,    // where the drawing hand pulls it
	Rest,    // at rest: after the loose, and once after an arrow is dropped
};

struct BowStringState {
	bool pulled = false;          // the hand had it last frame
	bool afterLoose = false;      // loosed; the engine's shot still playing
	float looseSecondsLeft = 0.0f;
};

// The engine's release still plays its draw a moment after the loose, and a
// short draw let go early would carry on pulling the string to full: held at
// rest until the player's action is over (-1), for at most this long.
constexpr float kStringRestAfterLooseSeconds = 3.0f;

inline StringSource StepBowString(BowStringState& s, ArrowState state, bool loosed, bool engineActionOn,
                                  float dtSeconds) {
	if (state == ArrowState::Nocked || state == ArrowState::Drawing) {
		s.pulled = true;
		s.afterLoose = false;
		return StringSource::Hand;
	}
	if (loosed) {
		s.pulled = false;
		s.afterLoose = true;
		s.looseSecondsLeft = kStringRestAfterLooseSeconds;
		return StringSource::Rest;
	}
	if (s.afterLoose) {
		if (dtSeconds > 0.0f) {
			s.looseSecondsLeft -= dtSeconds;
		}
		if (engineActionOn && s.looseSecondsLeft > 0.0f) {
			return StringSource::Rest;
		}
		s.afterLoose = false;
		return StringSource::Engine;
	}
	if (s.pulled) {
		// Dropped, or the bow put away with it: back to rest once, then the
		// game's again.
		s.pulled = false;
		return StringSource::Rest;
	}
	return StringSource::Engine;
}

// Where the arrow is seen, for its state: none, in the fist, or on the string
// from the fist through the bow.
enum class ArrowShown : UInt8 { None, InHand, OnString };

inline ArrowShown ArrowShownFor(ArrowState state) {
	switch (state) {
	case ArrowState::InHand:
		return ArrowShown::InHand;
	case ArrowState::Nocked:
	case ArrowState::Drawing:
		return ArrowShown::OnString;
	case ArrowState::None:
		break;
	}
	return ArrowShown::None;
}

// `from` turned by the shortest arc that takes its y axis onto `direction`
// (a unit vector): the arrow on the string keeps the roll it had in the fist.
// Straight back, the half turn is about from's own x axis.
inline NiMatrix33 TurnYOnto(const NiMatrix33& from, const NiPoint3& direction) {
	const NiPoint3 a = from * NiPoint3{0.0f, 1.0f, 0.0f};
	const NiPoint3& b = direction;
	const float c = a.x * b.x + a.y * b.y + a.z * b.z;
	NiPoint3 k{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
	if (c < -0.9999f) {
		// Half a turn about from's x: k along it, sin 0, cos -1.
		k = from * NiPoint3{1.0f, 0.0f, 0.0f};
		NiMatrix33 half = NiMatrix33::Identity();
		for (int i = 0; i < 3; ++i) {
			for (int j = 0; j < 3; ++j) {
				const float ki = i == 0 ? k.x : (i == 1 ? k.y : k.z);
				const float kj = j == 0 ? k.x : (j == 1 ? k.y : k.z);
				half.data[i][j] = 2.0f * ki * kj - (i == j ? 1.0f : 0.0f);
			}
		}
		return half * from;
	}
	const float factor = 1.0f / (1.0f + c);
	// Rodrigues with the unnormalised axis: I + [k] + [k]^2 / (1 + c).
	NiMatrix33 r = NiMatrix33::Identity();
	const float kx = k.x;
	const float ky = k.y;
	const float kz = k.z;
	r.data[0][0] = 1.0f - (ky * ky + kz * kz) * factor;
	r.data[0][1] = -kz + kx * ky * factor;
	r.data[0][2] = ky + kx * kz * factor;
	r.data[1][0] = kz + kx * ky * factor;
	r.data[1][1] = 1.0f - (kx * kx + kz * kz) * factor;
	r.data[1][2] = -kx + ky * kz * factor;
	r.data[2][0] = -ky + kx * kz * factor;
	r.data[2][1] = kx + ky * kz * factor;
	r.data[2][2] = 1.0f - (kx * kx + ky * ky) * factor;
	return r * from;
}

// The arrow's pose in the world. `gripRot`/`gripPos` is the drawing fist's
// grip (the right-hand Weapon node); `rest` the point on the bow the arrow
// lies across. In the fist the arrow points along the grip, as a sword does,
// its nock just behind it; on the string it runs from that nock through the
// rest. `lengthUnits` is the model's head-to-nock length; the model's origin
// is its head. False when the pose cannot be made (a zero length, the rest on
// the nock).
struct ArrowPose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};  // the head: the model's origin
	NiPoint3 nock{0.0f, 0.0f, 0.0f};
};

inline bool ArrowPoseFor(ArrowShown shown, const NiMatrix33& gripRot, const NiPoint3& gripPos,
                         const NiPoint3& rest, float lengthUnits, ArrowPose& out) {
	if (shown == ArrowShown::None || !(lengthUnits > 0.0f)) {
		return false;
	}
	const NiPoint3 along = gripRot * NiPoint3{0.0f, 1.0f, 0.0f};
	out.nock = gripPos - along * kArrowNockBehindGripUnits;
	out.rot = gripRot;
	if (shown == ArrowShown::OnString) {
		NiPoint3 direction{};
		if (!ArrowLine(out.nock, rest, direction)) {
			return false;
		}
		out.rot = TurnYOnto(gripRot, direction);
	}
	out.pos = out.nock + (out.rot * NiPoint3{0.0f, 1.0f, 0.0f}) * lengthUnits;
	return true;
}

}  // namespace obvr::vr
