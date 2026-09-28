#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The fingers closed around what the hand holds (docs/holding-objects-spec.md,
// part 1). The first-person hand's finger bones take whatever the animation
// gives them - an open hand while an object floats at it, the magician's
// look (2026-09-26). While the engine holds an object for a hand, each finger
// bone under that hand is turned by a fixed curl about its own z axis, on
// top of the rotation it had when the hold began; when the hold ends, that
// rotation is put back.
//
// Found by name, not listed: every node under the hand bone whose name holds
// "Finger" is a finger link, whatever the skeleton calls each one. A link
// bends about its own z, +z closing the hand: the game's own fist
// (handtohandidle.kf, below) is each link turned that way from the open
// hand. The sign is still a setting: a negative [Hands] GripCurlDegrees bends
// the other way.

// Whether this hand's fingers close: the engine holds an object, and it is
// this hand that grabbed it.
inline bool HandGripWanted(bool rightHand, bool engineHolding, bool grabWithLeftHand) {
	return engineHolding && (rightHand != grabWithLeftHand);
}

// How far one link bends. The thumb ("Finger0", its links "Finger01",
// "Finger02") half as far, so it rests across the object instead of
// folding into the palm.
inline float FingerCurlDegrees(const char* boneName, float curlDegrees) {
	if (boneName == nullptr) {
		return curlDegrees;
	}
	for (const char* at = boneName; *at != '\0'; ++at) {
		if ((at[0] == 'F' || at[0] == 'f') && (at[1] == 'i' || at[1] == 'I') &&
		    (at[2] == 'n' || at[2] == 'N') && (at[3] == 'g' || at[3] == 'G') &&
		    (at[4] == 'e' || at[4] == 'E') && (at[5] == 'r' || at[5] == 'R')) {
			return at[6] == '0' ? 0.5f * curlDegrees : curlDegrees;
		}
	}
	return curlDegrees;
}

// A link's rotation bent by `degrees` about its own z: base * Rz.
inline NiMatrix33 CurledAboutZ(const NiMatrix33& base, float degrees) {
	const float radians = degrees * (math::kPi / 180.0f);
	const float c = math::Cos(radians);
	const float s = math::Sin(radians);
	NiMatrix33 turn = NiMatrix33::Identity();
	turn.data[0][0] = c;
	turn.data[0][1] = -s;
	turn.data[1][0] = s;
	turn.data[1][1] = c;
	return base * turn;
}

// ------------------------------------------------------ the tracked fingers
//
// Each finger follows the controller's own finger (SteamVR's skeletal
// summary, the same curls the fist is judged by - vr/Fist.h): 0 the game's
// open hand, 1 the game's fist, and in between the two blended. Both poses
// are the game's own, read from its first-person animations with pyffi on
// 2026-09-28 (meshes\characters\_1stperson, Oblivion - Meshes.bsa):
// - open: idle.kf, the relaxed hand of the first-person idle - each link
//   bent 4 to 30 degrees about its z;
// - fist: handtohandidle.kf, the raised fists - each link 65 to 102 degrees
//   about its z, the thumb (Finger0) across them.
// These are the links' local rotations, (w, x, y, z), for the right hand.
// The left hand's are their mirror image: x and y negated (the left idle's
// and the left fist's keys are exactly that of the right's). The open hand
// is taken from the left idle, mirrored: two of the right idle's keys
// (Finger02, Finger42) start curled and move on, the left's do not.
//
// The curl bends about +z on both hands in both files - the direction the
// grip curl (above) has bent from the start.

enum class FingerPose : UInt8 {
	Animation,  // the fingers as the animation has them
	Grip,       // closed around what the engine holds (the grip curl)
	Tracked,    // each finger where the controller's finger is
};

// Which pose a hand's fingers take this frame: closed around a held object
// before anything else; the controller's fingers when tracking is on, the
// device gives curls, and the hand holds nothing of its own (a drawn weapon,
// a torch - the animation's grip on its handle stays); else the animation.
inline FingerPose FingerPoseFor(bool tracking, bool curlValid, bool gripClosed, bool handHoldsItem) {
	if (gripClosed) {
		return FingerPose::Grip;
	}
	if (tracking && curlValid && !handHoldsItem) {
		return FingerPose::Tracked;
	}
	return FingerPose::Animation;
}

struct FingerQuat {
	float w;
	float x;
	float y;
	float z;
};

// Finger0 (thumb), 01, 02, Finger1 (index), 11, 12, ... Finger4 (little), 41, 42.
inline constexpr int kFingerLinkCount = 15;

inline constexpr FingerQuat kOpenRight[kFingerLinkCount] = {
	{0.827f, -0.530f, -0.164f, -0.090f}, {0.999f, 0.0f, 0.0f, 0.037f}, {0.993f, 0.0f, 0.0f, 0.115f},
	{0.996f, -0.007f, 0.027f, 0.089f},   {0.993f, 0.0f, 0.0f, 0.115f}, {0.996f, 0.0f, 0.0f, 0.085f},
	{0.993f, 0.0f, 0.007f, 0.116f},      {0.972f, 0.0f, 0.0f, 0.233f}, {0.993f, 0.0f, 0.0f, 0.115f},
	{0.974f, 0.050f, 0.010f, 0.220f},    {0.967f, 0.0f, 0.0f, 0.255f}, {0.999f, 0.0f, 0.0f, 0.034f},
	{0.974f, 0.093f, 0.032f, 0.205f},    {0.969f, 0.0f, 0.0f, 0.246f}, {0.996f, 0.0f, 0.0f, 0.092f},
};

inline constexpr FingerQuat kFistRight[kFingerLinkCount] = {
	{0.719f, -0.580f, -0.379f, -0.048f}, {0.886f, 0.0f, 0.0f, 0.464f}, {0.833f, 0.0f, 0.0f, 0.553f},
	{0.722f, -0.075f, -0.012f, 0.688f},  {0.628f, 0.0f, 0.0f, 0.778f}, {0.680f, 0.0f, 0.0f, 0.733f},
	{0.747f, -0.011f, -0.104f, 0.656f},  {0.666f, 0.0f, 0.0f, 0.746f}, {0.723f, 0.0f, 0.0f, 0.691f},
	{0.755f, 0.093f, -0.059f, 0.647f},   {0.754f, 0.0f, 0.0f, 0.656f}, {0.742f, 0.0f, 0.0f, 0.670f},
	{0.784f, 0.220f, -0.100f, 0.572f},   {0.763f, 0.0f, 0.0f, 0.646f}, {0.721f, 0.0f, 0.0f, 0.693f},
};

// A finger link by its name: "...FingerN" is finger N's first link,
// "...FingerN1" and "...FingerN2" the next two. The finger is the index into
// the curls (0 the thumb), the link its table index. False for anything else.
inline bool FingerLinkOf(const char* boneName, int& finger, int& link) {
	if (boneName == nullptr) {
		return false;
	}
	for (const char* at = boneName; *at != '\0'; ++at) {
		if ((at[0] == 'F' || at[0] == 'f') && (at[1] == 'i' || at[1] == 'I') &&
		    (at[2] == 'n' || at[2] == 'N') && (at[3] == 'g' || at[3] == 'G') &&
		    (at[4] == 'e' || at[4] == 'E') && (at[5] == 'r' || at[5] == 'R')) {
			const char digit = at[6];
			const char joint = at[7];
			if (digit < '0' || digit > '4') {
				return false;
			}
			int depth = 0;
			if (joint == '1' || joint == '2') {
				depth = joint - '0';
				if (at[8] != '\0') {
					return false;
				}
			} else if (joint != '\0') {
				return false;
			}
			finger = digit - '0';
			link = finger * 3 + depth;
			return true;
		}
	}
	return false;
}

// The rotation a unit quaternion (w, x, y, z) stands for, as a matrix that
// turns column vectors - the way NiMatrix33 is used here (ThroughTransform,
// CurledAboutZ: a turn by +a about z has data[1][0] = sin a).
inline NiMatrix33 RotationOfQuat(const FingerQuat& q) {
	const float w = q.w;
	const float x = q.x;
	const float y = q.y;
	const float z = q.z;
	NiMatrix33 m;
	m.data[0][0] = 1.0f - 2.0f * (y * y + z * z);
	m.data[0][1] = 2.0f * (x * y - z * w);
	m.data[0][2] = 2.0f * (x * z + y * w);
	m.data[1][0] = 2.0f * (x * y + z * w);
	m.data[1][1] = 1.0f - 2.0f * (x * x + z * z);
	m.data[1][2] = 2.0f * (y * z - x * w);
	m.data[2][0] = 2.0f * (x * z - y * w);
	m.data[2][1] = 2.0f * (y * z + x * w);
	m.data[2][2] = 1.0f - 2.0f * (x * x + y * y);
	return m;
}

// From one quaternion to another by t (0 to 1), along the shorter way,
// normalised: a straight blend, not a slerp - the two poses of a link are at
// most 80 degrees apart, where the blend's speed along the arc differs from
// even by a few per cent.
inline FingerQuat BlendQuat(const FingerQuat& a, const FingerQuat& b, float t) {
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	const float dot = a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z;
	const float sign = dot < 0.0f ? -1.0f : 1.0f;
	FingerQuat q{a.w + (sign * b.w - a.w) * t, a.x + (sign * b.x - a.x) * t, a.y + (sign * b.y - a.y) * t,
	             a.z + (sign * b.z - a.z) * t};
	const float length = math::Sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z);
	if (!(length > 1e-6f)) {
		return a;
	}
	const float k = 1.0f / length;
	return FingerQuat{q.w * k, q.x * k, q.y * k, q.z * k};
}

// The left hand's link is the right's mirror image: x and y negated.
inline FingerQuat MirroredForLeft(const FingerQuat& q) { return FingerQuat{q.w, -q.x, -q.y, q.z}; }

// A tracked link's local rotation: its open and fist poses blended by its
// finger's curl.
inline NiMatrix33 TrackedLinkRotation(bool rightHand, int link, float curl) {
	if (link < 0 || link >= kFingerLinkCount) {
		return NiMatrix33::Identity();
	}
	const FingerQuat blended = BlendQuat(kOpenRight[link], kFistRight[link], curl);
	return RotationOfQuat(rightHand ? blended : MirroredForLeft(blended));
}

// What the controller says of a hand's fingers: each finger's curl, thumb to
// little finger, and - when the full skeleton gave it - the thumb joint by
// joint, base to tip (vr/ThumbPose.h).
struct FingerCurls {
	float curl[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	bool thumbJoints = false;
	float thumb[3] = {0.0f, 0.0f, 0.0f};
};

// The share a link is blended by: a thumb link by its own joint when the
// joints are known, every other link - and the thumb without them - by its
// finger's curl.
inline float LinkShare(const FingerCurls& curls, int finger, int link) {
	if (finger == 0 && curls.thumbJoints && link >= 0 && link < 3) {
		return curls.thumb[link];
	}
	return finger >= 0 && finger < 5 ? curls.curl[finger] : 0.0f;
}

// The left little finger that stays straight (the tester, 2026-09-28; open
// bug in docs/holding-objects-spec.md, part 1b): once per hand, the first
// time a tracked little finger is curled this far, its links are logged -
// the angle OBVR writes and whether last frame's write was still there - so
// the right hand's line can be set against the left's.
inline constexpr float kLittleFingerCheckShare = 0.8f;
inline constexpr int kLittleFinger = 4;

inline bool LittleFingerCheckDue(bool reported, FingerPose pose, float share) {
	return !reported && pose == FingerPose::Tracked && share >= kLittleFingerCheckShare;
}

// A link's turn about its own z, the axis the curl bends it about, in degrees.
inline float LinkCurlDegrees(const NiMatrix33& rot) {
	return math::Atan2(rot.data[1][0], rot.data[0][0]) * math::kRadiansToDegrees;
}

// Once per frame, after the hand bone has been pinned: gives the named hand's
// fingers the pose asked for - closed by `curlDegrees` around what it holds
// (Grip), each where the controller's finger is (Tracked), or back to what
// the animation had (Animation).
void StepHandFingers(bool rightHand, const char* handBoneName, FingerPose pose, float curlDegrees,
                     const FingerCurls* curls);

// Whether the named hand holds a thing of its own: anything under the hand
// bone besides its finger links that carries a child - the drawn weapon's
// "Weapon" node, the "Torch" node with the torch.
bool HandHoldsItem(bool rightHand, const char* handBoneName);

// Forgets the fingers found (a new model), without writing anything.
void ForgetHandGrip();

}  // namespace obvr::game
