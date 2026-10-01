#pragma once

// The bow by hand, as in Blade & Sorcery (the tester, 2026-09-30: "Pfeil und
// Bogen wie in blade and sorcery. Linke hand hat ja bereits den Bogen. Jetzt
// muss rechte noch auf rechter schulter den Pfeil bekommen und an den bogen
// führen. Beim loslassen Schuss. Zielen im groben mit links mit dem bogen, im
// feinen mit rechts dem Pfeil").
//
// With the bow drawn, the weapon hand's trigger (or grip, ArrowWithGrip)
// closed at the quiver - over the shoulder on its own side - takes an arrow.
// Brought to the bow hand it is nocked; pulled back from there along the bow's
// line the engine's own draw begins (its attack control held, the way the
// trigger held it before); let go, it looses (the control let go, as vanilla
// looses on release).
//
// The tester, 2026-09-30, after the first round: "zielen doch nur noch mit
// linker hand wo der bogen ist ... wenn man den bogen spannt kann man den nur
// noch in einer linie wie man mit links zielt ... und wenn man maximale
// spannung des bogens erreicht hat ist stopp ... man kann den bogen dann
// entweder loslassen und schiessen oder wieder zurück wie am anfang (inklusive
// pfeil wegstecken)". So:
//   * the shot goes where the bow points (its shot axis, measured off the bow
//     model), the drawing hand no longer steers it; without that axis, the old
//     line from the drawing hand through the bow;
//   * the pull is how far the drawing hand is behind the bow along that axis;
//     the picture keeps the arrow on the axis and stops at full draw
//     (game/BowVisual);
//   * the hand brought back to the bow while drawn eases the string: the
//     engine's draw is cancelled (no shot), the arrow stays nocked; moved off
//     the line it comes off the string, into the hand; let go at the quiver it
//     is put back.
//
// The engine still decides the draw's power (the time the control is held);
// OBVR decides when it is held and which way it flies. An arrow let go before
// the draw began is dropped (or put back, at the quiver): no shot.
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
	// The drawing hand this near the bow hand nocks the arrow: wide, so the
	// nock is not searched for (the tester, 2026-10-01: "pfeil anlegen darf
	// noch großzügiger sein etwas. also früher erkannt werden damit man nicht
	// nach der anlege stelle suchen muss"; 0.15 before).
	float nockMetres = 0.25f;
	// The string at rest: the drawing hand this far behind the bow hand along
	// the bow. The draw begins drawStartMetres further back, and the hand
	// brought back within it eases the draw. Apart from the nock's zone, so a
	// wider nock does not make the draw begin later.
	float stringMetres = 0.15f;
	float drawStartMetres = 0.08f;
	// Nocked, the drawing hand this far off the bow's line - and out of the
	// nock's zone, or it would come off as it goes on - takes the arrow off the
	// string again.
	float unnockMetres = 0.12f;
	// Which button holds the arrow: the trigger (the default), or the grip.
	bool takeWithTrigger = true;
};

// After an eased draw the engine's control is held this much longer, so the
// cancel (game::StepBowDenock) is in before the control goes up: let go
// first, the engine looses (vanilla has no way to take an arrow back).
constexpr float kDenockHoldSeconds = 0.3f;

enum class ArrowState : UInt8 {
	None,     // no arrow in the hand
	InHand,   // taken from the quiver, not on the bow yet
	Nocked,   // on the string
	Drawing,  // pulled: the engine draws
};

struct ArcheryState {
	ArrowState state = ArrowState::None;
	bool gripWas = true;  // a grip closed when the bow comes out is not a take
	float denockSeconds = 0.0f;  // an eased draw's control still held
};

struct ArcheryInput {
	bool bowDrawn = false;    // a bow drawn, in the world, no menu
	bool drawValid = false;   // the drawing hand (the weapon hand) tracked
	bool bowValid = false;    // the bow hand
	NiPoint3 drawBody{0.0f, 0.0f, 0.0f};  // the drawing hand, BodyRelative
	NiPoint3 drawAt{0.0f, 0.0f, 0.0f};    // tracking space, metres
	NiPoint3 bowAt{0.0f, 0.0f, 0.0f};
	bool drawGrip = false;    // the button that holds the arrow (the trigger or the grip)
	bool leftHanded = false;  // the quiver mirrors
	// The bow's shot axis, tracking space, unit length; without it the pull
	// is the distance between the hands.
	bool axisValid = false;
	NiPoint3 bowAxis{0.0f, 0.0f, -1.0f};
	float dtSeconds = 0.0f;
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
	bool eased = false;      // the hand came back while drawn: the draw cancelled
	bool unnocked = false;   // off the string, back in the hand
	bool stowed = false;     // let go at the quiver: put back
	bool denockDone = false; // the eased draw's control goes up this frame
	float handsApartMetres = 0.0f;
	float pullMetres = 0.0f;     // behind the bow along its axis
	float offLineMetres = 0.0f;  // across it
	NiPoint3 bowAxis{0.0f, 0.0f, 0.0f};  // the axis it went by (zero: none), for the log
};

// The bow's draw sound in its parts (game/BowDrawSound.h; the tester,
// 2026-09-30 evening: "den bogen spann sound müssen wir whl stückeln so dass
// das spannen des bogens und andere teile davon einzeln abspielen"): its first
// part as the arrow goes onto the string, its second - the string's stretch -
// as the draw begins, and that one cut off whenever the bow is not drawn: let
// go, eased back, taken off the string or dropped.
struct BowSoundCue {
	bool nock = false;
	bool stretch = false;
	bool stopStretch = false;
};

inline BowSoundCue BowSoundsFor(const ArcheryVerdict& v) {
	BowSoundCue c;
	c.nock = v.nocked;
	c.stretch = v.drawStarted;
	c.stopStretch = v.state != ArrowState::Drawing;
	return c;
}

// The loose at once (the tester, 2026-10-01: "nach dem loslassen erwartet man
// einen direkten schuss. oftmals kommt aber eine sekunde verzögerung! das muss
// weg!"; game/BowRelease.h). The engine looses at its draw's Release key; let
// go before the Hold key the draw plays on to it first - up to 1.4 s. So,
// once the control is up after a loose, the draw is put at its Hold - but only
// with the arrow attached (action 5): before the Attach key there is no arrow
// to loose, and the engine's key counter steps once a frame, so a jump past
// the Attach would lose the shot. Asked until it is done, the draw ends, or
// kReleaseSnapSeconds pass.
constexpr float kReleaseSnapSeconds = 2.0f;

struct ReleaseSnapState {
	bool pending = false;
	float seconds = 0.0f;
};

enum class ReleaseSnap : UInt8 {
	Idle,   // nothing to do
	Wait,   // loosed, the arrow not attached yet
	Snap,   // put the draw at its Hold now
};

// `action`: the player's (game::kAction*): 4 the draw before the Attach key,
// 5 after it. `snapped`: last frame's Snap was carried out.
inline ReleaseSnap StepReleaseSnap(ReleaseSnapState& s, bool loosedNow, SInt32 action, bool snapped,
                                   float dtSeconds) {
	if (loosedNow) {
		s.pending = true;
		s.seconds = 0.0f;
	} else if (s.pending) {
		s.seconds += dtSeconds > 0.0f ? dtSeconds : 0.0f;
	}
	if (snapped || action < 0 || (action != 4 && action != 5 && !loosedNow) ||
	    s.seconds > kReleaseSnapSeconds) {
		s.pending = false;
	}
	if (!s.pending) {
		return ReleaseSnap::Idle;
	}
	// Not on the loose's own frame: the engine is to see the control up first,
	// or it pauses the draw at the Hold it was put at.
	return action == 5 && !loosedNow ? ReleaseSnap::Snap : ReleaseSnap::Wait;
}

inline float MetresBetween(const NiPoint3& a, const NiPoint3& b) {
	const NiPoint3 d{a.x - b.x, a.y - b.y, a.z - b.z};
	return math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

inline bool AtQuiver(const ArcherySettings& s, const NiPoint3& drawBody, bool leftHanded) {
	const NiPoint3 zone{leftHanded ? -s.quiverZone.x : s.quiverZone.x, s.quiverZone.y, s.quiverZone.z};
	const NiPoint3 d{drawBody.x - zone.x, drawBody.y - zone.y, drawBody.z - zone.z};
	return d.x * d.x + d.y * d.y + d.z * d.z <= s.quiverRadius * s.quiverRadius;
}

// How far the drawing hand is behind the bow hand along the bow's axis, and
// how far across it; without an axis, the whole distance and none across.
inline void PullAlongBow(const ArcheryInput& in, float& pull, float& across) {
	const NiPoint3 d{in.bowAt.x - in.drawAt.x, in.bowAt.y - in.drawAt.y, in.bowAt.z - in.drawAt.z};
	if (!in.axisValid) {
		pull = math::Sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
		across = 0.0f;
		return;
	}
	const NiPoint3& a = in.bowAxis;
	pull = d.x * a.x + d.y * a.y + d.z * a.z;
	const NiPoint3 side{d.x - a.x * pull, d.y - a.y * pull, d.z - a.z * pull};
	across = math::Sqrt(side.x * side.x + side.y * side.y + side.z * side.z);
}

inline ArcheryVerdict StepArchery(ArcheryState& st, const ArcheryInput& in, const ArcherySettings& s) {
	ArcheryVerdict v;
	const bool press = in.drawGrip && !st.gripWas;
	st.gripWas = in.drawGrip;
	// An eased draw's control, held a moment longer (kDenockHoldSeconds).
	const bool denocking = st.denockSeconds > 0.0f;
	if (denocking && in.dtSeconds > 0.0f) {
		st.denockSeconds -= in.dtSeconds;
		if (st.denockSeconds <= 0.0f) {
			st.denockSeconds = 0.0f;
			v.denockDone = true;
		}
	}
	if (!s.enabled || !in.bowDrawn || !in.drawValid || !in.bowValid) {
		// The bow went, or a hand: the arrow with it. A draw under way is not
		// loosed - the control simply stops being held.
		v.dropped = st.state != ArrowState::None;
		st.state = ArrowState::None;
		v.denockDone = v.denockDone || st.denockSeconds > 0.0f;
		st.denockSeconds = 0.0f;
		v.state = st.state;
		return v;
	}
	v.handsApartMetres = MetresBetween(in.drawAt, in.bowAt);
	PullAlongBow(in, v.pullMetres, v.offLineMetres);
	if (in.axisValid) {
		v.bowAxis = in.bowAxis;
	}
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
			if (AtQuiver(s, in.drawBody, in.leftHanded)) {
				v.stowed = true;
			} else {
				v.dropped = true;
			}
		} else if (v.handsApartMetres <= s.nockMetres) {
			st.state = ArrowState::Nocked;
			v.nocked = true;
		}
		break;
	case ArrowState::Nocked:
		if (!in.drawGrip) {
			st.state = ArrowState::None;
			v.dropped = true;
		} else if (v.pullMetres >= s.stringMetres + s.drawStartMetres && st.denockSeconds <= 0.0f) {
			st.state = ArrowState::Drawing;
			v.drawStarted = true;
		} else if (in.axisValid && v.offLineMetres > s.unnockMetres && v.handsApartMetres > s.nockMetres) {
			st.state = ArrowState::InHand;
			v.unnocked = true;
		}
		break;
	case ArrowState::Drawing:
		if (!in.drawGrip) {
			st.state = ArrowState::None;
			v.loosed = true;
		} else if (v.pullMetres <= s.stringMetres) {
			st.state = ArrowState::Nocked;
			v.eased = true;
			st.denockSeconds = kDenockHoldSeconds;
		}
		break;
	}
	v.state = st.state;
	v.attackHeld = st.state == ArrowState::Drawing || st.denockSeconds > 0.0f;
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

// The string's weight from how far the nock is behind the arrow's rest along
// the bow's shot axis: 0 with it no further back than the string at rest, 1 a
// full draw's travel further.
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

// The arrow's pose in the world; the model's origin is its head, its nock
// `lengthUnits` back along -y.
struct ArrowPose {
	NiMatrix33 rot = NiMatrix33::Identity();
	NiPoint3 pos{0.0f, 0.0f, 0.0f};  // the head: the model's origin
	NiPoint3 nock{0.0f, 0.0f, 0.0f};
};

// An arrow from `nock` along `direction` (any length), `rollFrom` turned onto
// it to keep its roll. False for a model with no length or no direction.
inline bool ArrowAlong(const NiPoint3& nock, const NiPoint3& direction, const NiMatrix33& rollFrom,
                       float lengthUnits, ArrowPose& out) {
	const float d = direction.x * direction.x + direction.y * direction.y + direction.z * direction.z;
	if (!(lengthUnits > 0.0f) || !(d > 1e-6f)) {
		return false;
	}
	const NiPoint3 unit = direction * (1.0f / math::Sqrt(d));
	out.nock = nock;
	out.rot = TurnYOnto(rollFrom, unit);
	out.pos = nock + unit * lengthUnits;
	return true;
}

// In the fist: through its middle, straight ahead along the hand - the line
// from the wrist to the middle finger's knuckle - turned kArrowInFistDownDegrees
// down, away from the fist's grip axis (`rollFrom`'s y: where a sword's blade
// stands out of the fist, by the thumb), its nock kArrowNockBehindGripUnits
// behind the fist's grip point. The tester, 2026-09-30 evening: "rechte hand
// der pfeil zeigt nicht wie der laserpointer sondern hier auch einfach mittig
// in der hand gerade aus"; 2026-10-01, of the line along the hand: "der pfeil
// muss noch 30 grad runter damit er geradeaus schaut". Before it lay on the
// laser, and before that along the grip, pointing up. False with the wrist on
// the knuckle, the grip axis along the hand, or a model with no length.
constexpr float kArrowInFistDownDegrees = 30.0f;

inline bool ArrowInFist(const NiPoint3& grip, const NiPoint3& wrist, const NiPoint3& knuckle,
                        const NiMatrix33& rollFrom, float lengthUnits, ArrowPose& out) {
	const NiPoint3 along{knuckle.x - wrist.x, knuckle.y - wrist.y, knuckle.z - wrist.z};
	const float d = along.x * along.x + along.y * along.y + along.z * along.z;
	if (!(d > 1e-6f)) {
		return false;
	}
	const NiPoint3 unit = along * (1.0f / math::Sqrt(d));
	// The grip axis made square to the hand's line: "up" in the fist.
	const NiPoint3 gripAxis = rollFrom * NiPoint3{0.0f, 1.0f, 0.0f};
	const float onLine = gripAxis.x * unit.x + gripAxis.y * unit.y + gripAxis.z * unit.z;
	NiPoint3 up{gripAxis.x - unit.x * onLine, gripAxis.y - unit.y * onLine, gripAxis.z - unit.z * onLine};
	const float upLength = math::Sqrt(up.x * up.x + up.y * up.y + up.z * up.z);
	if (!(upLength > 1e-4f)) {
		return false;
	}
	up = up * (1.0f / upLength);
	const float down = kArrowInFistDownDegrees * math::kDegreesToRadians;
	const NiPoint3 ahead = unit * math::Cos(down) - up * math::Sin(down);
	return ArrowAlong(grip - ahead * kArrowNockBehindGripUnits, ahead, rollFrom, lengthUnits, out);
}

// On the string: along the bow's shot axis through the arrow's rest, the nock
// as far behind the rest as the drawing fist is - but no nearer than the
// string at rest and no further than a full draw ("wenn man maximale spannung
// des bogens erreicht hat ist stopp"). `stringRestUnits` is the string's rest
// behind the arrow's rest along the axis, `travelUnits` a full draw's travel
// (both read from the bow's morph). The fist is to go where the nock's grip
// point is (`gripTarget`), so the hand stays on the string; `weight` is the
// string's morph weight. False for a model with no length.
struct ArrowOnString {
	ArrowPose pose;
	NiPoint3 gripTarget{0.0f, 0.0f, 0.0f};
	float nockBehindRest = 0.0f;
	float weight = 0.0f;
	bool atFullDraw = false;
};

inline bool ArrowOnBowLine(const NiPoint3& rest, const NiPoint3& axis, const NiMatrix33& gripRot,
                           const NiPoint3& gripPos, float lengthUnits, float stringRestUnits, float travelUnits,
                           ArrowOnString& out) {
	if (!(lengthUnits > 0.0f)) {
		return false;
	}
	const NiPoint3 d{rest.x - gripPos.x, rest.y - gripPos.y, rest.z - gripPos.z};
	const float behind = d.x * axis.x + d.y * axis.y + d.z * axis.z + kArrowNockBehindGripUnits;
	const float full = stringRestUnits + (travelUnits > 0.0f ? travelUnits : 0.0f);
	float nockBehind = behind < stringRestUnits ? stringRestUnits : behind;
	out.atFullDraw = behind >= full;
	nockBehind = nockBehind > full ? full : nockBehind;
	out.nockBehindRest = nockBehind;
	out.pose.nock = rest - axis * nockBehind;
	out.pose.rot = TurnYOnto(gripRot, axis);
	out.pose.pos = out.pose.nock + axis * lengthUnits;
	out.gripTarget = out.pose.nock + axis * kArrowNockBehindGripUnits;
	out.weight = StringWeightAt(nockBehind, stringRestUnits, travelUnits);
	return true;
}

}  // namespace obvr::vr
