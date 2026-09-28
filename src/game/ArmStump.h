#pragma once

#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The forearm stump ([Hands] ForearmStump, off by default): a bare hand
// with a short piece of forearm behind it, tapering to a point behind the
// elbow, instead of a hand that ends at the wrist (the tester's choice,
// 2026-09-28, after the lid; the lid stays the default).
//
// The forearm is there already: the body's "Arms" shape (upperbody.nif,
// read with pyffi 2026-09-28) runs from the shoulders to the wrists, and its
// wrist edge is exactly the bare hand's (hand.nif): the same eleven
// vertices a side with the same weights - hand 0.42, forearm twist 0.42,
// forearm 0.17. Full VR hides it because its upper arm hangs from the
// animated shoulder while the forearm goes where the controller is.
//
// So the shape is shown, and in its skin instance only (NiSkinInstance
// +0x14, one bone pointer per NiSkinData bone, +0x40 the count; xOBSE
// NiGeometry.h) the bones above the elbow are swapped for OBVR's own nodes:
// - each side's clavicle, upper arm and upper arm twist: a node at that
//   side's elbow (the forearm bone's origin) with almost no scale - every
//   vertex hanging only from them collapses into the elbow, and the skin
//   between the elbow and the upper arm tapers into it;
// - the spine (Spine2, in the naked shape's case: weights 0.1-0.4 on 56
//   shoulder vertices, always together with the same side's upper arm or
//   clavicle): one node shared by both sides, so no single point will do. Its
//   transform is the real spine's followed by a map that sends everything
//   left of the body's middle to the left elbow and everything right of it
//   to the right one, by how far out it is - flat along the other two
//   axes. Fitted on the naked male and female shapes: the shoulder vertices
//   land within 6-8 % of the half-distance between the elbows of where they
//   should, times their weight (at most 0.4) - about a centimetre.
// The rest of the tree keeps its bones: nothing else draws with these
// pointers, and the real bones stay where the animation has them.

enum class StumpRole : UInt8 { Keep, LeftUpper, RightUpper, LeftForearm, RightForearm, Centre };

// Whether a name is exactly this one, case aside.
inline bool NameIs(const char* name, const char* wanted) {
	if (name == nullptr || wanted == nullptr) {
		return false;
	}
	for (; *name != '\0' && *wanted != '\0'; ++name, ++wanted) {
		char x = *name;
		char y = *wanted;
		x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x - 'A' + 'a') : x;
		y = (y >= 'A' && y <= 'Z') ? static_cast<char>(y - 'A' + 'a') : y;
		if (x != y) {
			return false;
		}
	}
	return *name == *wanted;
}

inline bool NameHas(const char* name, const char* part) {
	if (name == nullptr || part == nullptr) {
		return false;
	}
	for (const char* at = name; *at != '\0'; ++at) {
		const char* a = at;
		const char* b = part;
		while (*a != '\0' && *b != '\0') {
			char x = *a;
			char y = *b;
			x = (x >= 'A' && x <= 'Z') ? static_cast<char>(x - 'A' + 'a') : x;
			y = (y >= 'A' && y <= 'Z') ? static_cast<char>(y - 'A' + 'a') : y;
			if (x != y) {
				break;
			}
			++a;
			++b;
		}
		if (*b == '\0') {
			return true;
		}
	}
	return false;
}

// What a skin bone becomes, by its name: a side's bones above the elbow
// collapse into that side's elbow, the forearm and hand bones stay, and a
// bone of neither side (a spine, the neck, the pelvis) is the shared one.
inline StumpRole StumpRoleOf(const char* boneName) {
	if (boneName == nullptr) {
		return StumpRole::Keep;
	}
	const bool left = NameHas(boneName, " L ");
	const bool right = NameHas(boneName, " R ");
	if (left || right) {
		if (NameHas(boneName, "UpperArm") || NameHas(boneName, "Clavicle")) {
			return left ? StumpRole::LeftUpper : StumpRole::RightUpper;
		}
		if (NameHas(boneName, "Forearm")) {
			return left ? StumpRole::LeftForearm : StumpRole::RightForearm;
		}
		return StumpRole::Keep;
	}
	return StumpRole::Centre;
}

// The collapse: a point, as good as no size.
inline constexpr float kStumpCollapseScale = 0.001f;

inline NiTransform CollapseAt(const NiPoint3& point) {
	NiTransform t;
	t.rot = NiMatrix33::Identity();
	t.pos = point;
	t.scale = kStumpCollapseScale;
	return t;
}

// How far out from the body's middle a shoulder vertex sits, game units:
// the fit's 1/0.0708 (male) and 1/0.089 (female), between the two.
inline constexpr float kStumpShoulderUnits = 13.0f;

// The shared node's transform: the real bone's, then the map
// x -> middle + half * (across . (x - centre)) / shoulderUnits, where middle
// and half are the midpoint of the two elbows and half their difference,
// across the unit from the body's left to its right and centre a point on
// its middle. A vertex `shoulderUnits` to the right lands on the right
// elbow, as far to the left on the left one.
inline NiTransform CentreStump(const NiTransform& real, const NiPoint3& leftElbow, const NiPoint3& rightElbow,
                               const NiPoint3& across, const NiPoint3& centre,
                               float shoulderUnits = kStumpShoulderUnits) {
	const NiPoint3 middle = (leftElbow + rightElbow) * 0.5f;
	const NiPoint3 half = (rightElbow - leftElbow) * 0.5f;
	const float k = shoulderUnits > 0.0f ? 1.0f / shoulderUnits : 0.0f;
	// across^T * (real.rot * real.scale): the row the map reads a bone-space
	// vertex through.
	float row[3];
	for (int j = 0; j < 3; ++j) {
		row[j] = (across.x * real.rot.data[0][j] + across.y * real.rot.data[1][j] +
		          across.z * real.rot.data[2][j]) *
		         real.scale;
	}
	const float offset = across.x * (real.pos.x - centre.x) + across.y * (real.pos.y - centre.y) +
	                     across.z * (real.pos.z - centre.z);
	const float h[3] = {half.x, half.y, half.z};
	NiTransform t;
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			t.rot.data[i][j] = h[i] * k * row[j];
		}
	}
	t.pos = middle + half * (k * offset);
	t.scale = 1.0f;
	return t;
}

// The stump is short: the forearm's own bones (forearm and its twist) are
// swapped too, for their real transform followed by a squash along the
// forearm towards the wrist, to this share of its length. Across the arm
// nothing changes, so the skin at the wrist - which the hand's own mesh,
// still on the real bones, meets - stays where it was. The first harness run
// (2026-09-28) drew the full forearm, and with the controller held before
// the eyes it reached back through the head.
inline constexpr float kStumpForearmShare = 0.35f;

// x -> x - (1 - share) * axis (axis . (x - wrist)): lengths along the unit
// `axis` from the wrist scaled by `share`, nothing across it; after the real
// transform.
inline NiTransform SquashAlong(const NiTransform& real, const NiPoint3& wrist, const NiPoint3& axis,
                               float share = kStumpForearmShare) {
	const float cut = 1.0f - share;
	const float a[3] = {axis.x, axis.y, axis.z};
	NiMatrix33 squash = NiMatrix33::Identity();
	for (int i = 0; i < 3; ++i) {
		for (int j = 0; j < 3; ++j) {
			squash.data[i][j] -= cut * a[i] * a[j];
		}
	}
	NiTransform t;
	t.rot = squash * real.rot;
	const NiPoint3 d = real.pos - wrist;
	const float along = axis.x * d.x + axis.y * d.y + axis.z * d.z;
	t.pos = real.pos - axis * (cut * along);
	t.scale = real.scale;
	return t;
}

// Where a side's stump ends: the elbow brought towards the wrist by the
// same squash.
inline NiPoint3 StumpEnd(const NiPoint3& wrist, const NiPoint3& elbow, float share = kStumpForearmShare) {
	return wrist + (elbow - wrist) * share;
}

// A point through a transform, the way the engine places a vertex.
inline NiPoint3 ThroughTransform(const NiTransform& t, const NiPoint3& u) {
	return t.rot * (u * t.scale) + t.pos;
}

// The unit from one point to another, or +x for two that coincide.
inline NiPoint3 UnitFromTo(const NiPoint3& from, const NiPoint3& to) {
	const NiPoint3 d = to - from;
	const float length = math::Sqrt(d.LengthSquared());
	if (!(length > 1e-4f)) {
		return NiPoint3{1.0f, 0.0f, 0.0f};
	}
	return d * (1.0f / length);
}

// Whether the stump is drawn this frame: wanted, the hands bare, and the
// forearms pinned to the controllers (their elbows are where the stump
// ends). Without all three the shape stays hidden.
inline bool StumpWanted(bool setting, bool bareHands, bool pinned) { return setting && bareHands && pinned; }

// ------------------------------------------------------------ the game side

// Once a frame after the hand pins: swaps the bones of the first-person
// "Arms" shapes' skins, or - `wanted` false - gives them back. Answers
// whether the stump is in place, which is what shows the shape (the hide
// list leaves "Arms" out) on the next frame.
bool StepForearmStumps(bool wanted);

}  // namespace obvr::game
