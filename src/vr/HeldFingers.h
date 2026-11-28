#pragma once

// The holding hand's four fingers kept behind the tablet's page
// (docs/inventory-tablet-spec.md 32.2; Nadi, 2026-11-28: "dass die finger
// der hand die das tablet hält (mit ausnahme vom daumen) nun nicht mehr durch
// das tablet clippen können"). The hand that holds the tablet is drawn with
// the controller's finger curls (game::FingerPose::Tracked); the page stands
// where the player set it (HeldOutCm, 10 cm back by default), so a curled
// finger could reach through its face. Each frame, before the fingers are
// posed, every one of the holding hand's four fingers (not the thumb) is
// given the curl nearest the controller's at which none of its joints, nor
// its tip, nor their skin stands before the page's face - over the face
// only; beside it nothing shows through. A limit on the curl, finger by
// finger, not a pose blended in: the finger is posed by its curl alone
// (game::TrackedLinkRotation, the game's open hand to its fist), so the curl
// is the one thing to limit, and the finger keeps following the controller
// up to the page and from there on again.
//
// The page is the one placed this frame (the hand mode's menuTransform, in
// the left role's controller frame), carried into the world by the same
// mapping the tap's fingertip is carried out of it (vr::ControllerLocalOf),
// so the limit and the tap agree on where the face is. Pure; covered by
// held_fingers_test.

#include "game/HandGrip.h"
#include "vr/FingerTip.h"
#include "vr/HandReach.h"

namespace obvr::vr {

// A finger's flesh round its bones: the joints are the bones' ends, the skin
// lies this far round them. [G] from the human finger, 1.6 to 2 cm thick;
// the headset decides.
inline constexpr float kFingerSkinMetres = 0.01f;
// The curl searched in steps this wide from the controller's, both ways, and
// the step that found the page halved this often: to a sixtieth of a step.
inline constexpr float kFingerCurlStep = 0.1f;
inline constexpr int kFingerCurlHalvings = 6;

// The page in the world: its middle, its axes (its face along `front`) and
// half its sides, in the world's units.
struct SheetInWorld {
	bool valid = false;
	NiPoint3 centre{0.0f, 0.0f, 0.0f};
	NiPoint3 right{1.0f, 0.0f, 0.0f};
	NiPoint3 up{0.0f, 1.0f, 0.0f};
	NiPoint3 front{0.0f, 0.0f, 1.0f};
	float halfWidth = 0.0f;
	float halfHeight = 0.0f;
};

// A direction in a controller's frame (x right, y up, -z forward) in the
// world: vr::ControllerLocalOf undone - OpenVR's axes to the game's, then the
// hand's rotation relative to the head, then the head's.
inline NiPoint3 WorldOfControllerLocal(const NiMatrix33& headRot, const NiMatrix33& handRelativeRot,
                                       const NiPoint3& local) {
	return headRot * (handRelativeRot * NiPoint3{local.x, -local.z, local.y});
}

// The page placed in the controller's frame (`m`: its across, up and face the
// first three columns, its middle the fourth, metres; `width` and `height`
// metres) in the world: that controller's rotation relative to the head and
// its offset from the eyes in units, as the hand mode gives them.
inline SheetInWorld SheetOfPlace(const openvr::HmdMatrix34& m, float width, float height, const NiMatrix33& headRot,
                                 const NiMatrix33& handRelativeRot, const NiPoint3& cameraPos,
                                 const NiPoint3& handOffsetUnits, float unitsPerMetre) {
	SheetInWorld s;
	if (!(width > 0.0f) || !(height > 0.0f) || !(unitsPerMetre > 0.0f)) {
		return s;
	}
	const auto column = [&](int c) { return NiPoint3{m.m[0][c], m.m[1][c], m.m[2][c]}; };
	const NiPoint3 handWorld = cameraPos + headRot * handOffsetUnits;
	s.centre = handWorld + WorldOfControllerLocal(headRot, handRelativeRot, column(3)) * unitsPerMetre;
	s.right = WorldOfControllerLocal(headRot, handRelativeRot, column(0));
	s.up = WorldOfControllerLocal(headRot, handRelativeRot, column(1));
	s.front = WorldOfControllerLocal(headRot, handRelativeRot, column(2));
	s.halfWidth = 0.5f * width * unitsPerMetre;
	s.halfHeight = 0.5f * height * unitsPerMetre;
	s.valid = true;
	return s;
}

// How deep the deepest of `points` stands before the page's face, its skin
// (`skin`, units) counted: above nought, through it. Only a point over the
// face - its outline grown by the skin - counts; one beside the page shows
// through nothing. Nought or below: all behind, or none over it.
inline float DepthBeforeSheet(const SheetInWorld& s, const NiPoint3* points, int count, float skin) {
	float deepest = -1.0e30f;
	for (int i = 0; s.valid && points != nullptr && i < count; ++i) {
		const NiPoint3 d = points[i] - s.centre;
		const float across = game::DotOf3(d, s.right);
		const float up = game::DotOf3(d, s.up);
		if (across > s.halfWidth + skin || across < -s.halfWidth - skin || up > s.halfHeight + skin ||
		    up < -s.halfHeight - skin) {
			continue;
		}
		const float depth = game::DotOf3(d, s.front) + skin;
		deepest = depth > deepest ? depth : deepest;
	}
	return deepest;
}

// One finger's three links as the skeleton has them: each one's place under
// its parent (the first under the hand bone, "Bip01 R Finger1"; then
// "Finger11", "Finger12") and its own scale.
struct FingerChain {
	bool valid = false;
	NiPoint3 local[3] = {};
	float scale[3] = {1.0f, 1.0f, 1.0f};
};

// The finger's three joints and its tip in the world at `curl`, under the
// hand bone's world transform as the pin left it: each link turned as
// game::StepHandFingers turns it (game::TrackedLinkRotation) and carried down
// as the engine's update pass carries a child (its place scaled and turned by
// the parent's). The tip lies past the last joint as the tap's
// (vr::IndexTipBeyond). `finger` 1 the index to 4 the little finger;
// `rightBones` the R bones' table (the right role's hand, either side's
// controller).
inline void FingerPointsAt(const NiTransform& handWorld, const FingerChain& chain, bool rightBones, int finger,
                           float curl, NiPoint3 out[4]) {
	NiTransform w = handWorld;
	for (int i = 0; i < 3; ++i) {
		const NiPoint3 pos = w.pos + w.rot * (chain.local[i] * w.scale);
		w.rot = w.rot * game::TrackedLinkRotation(rightBones, finger * 3 + i, curl);
		w.pos = pos;
		w.scale = w.scale * chain.scale[i];
		out[i] = pos;
	}
	out[3] = IndexTipBeyond(out[1], out[2]);
}

// Which way a limited finger was moved from the controller's curl: kept from
// frame to frame, so a finger with the page on both sides does not flip.
enum class CurlSide : UInt8 { None, Less, More };

struct CurlLimit {
	float curl = 0.0f;
	bool limited = false;  // moved off the controller's curl
	bool behind = true;    // and behind the page there (false: no curl was; the least deep)
	CurlSide side = CurlSide::None;
};

// The curl nearest `wanted` (0 to 1) at which `depthAt(curl)` is nought or
// below: the controller's own when it is; else searched a step at a time
// both ways, the open side first at equal steps, and the step that found it
// halved down to the page, so the finger stands at the face rather than a
// step short of it. A finger that took a side last frame (`prefer`) searches
// that side through first and keeps it while the page is in its way - the
// hysteresis: no jump through the page from one side to the other while the
// controller's finger moves on. No curl behind it (a knuckle already
// through): the least deep one tried.
template <class DepthAt>
inline CurlLimit CurlBehindSheet(float wanted, CurlSide prefer, DepthAt depthAt) {
	wanted = wanted < 0.0f ? 0.0f : (wanted > 1.0f ? 1.0f : wanted);
	CurlLimit r;
	r.curl = wanted;
	float least = depthAt(wanted);
	if (!(least > 0.0f)) {
		return r;
	}
	r.limited = true;
	float leastCurl = wanted;
	bool ended[2] = {false, false};  // 0 the open side, 1 the closed
	const int steps = static_cast<int>(1.0f / kFingerCurlStep + 0.5f);
	// One try `k` steps to `side`: true when that curl is behind the page.
	const auto tryAt = [&](int side, int k) {
		const float sign = side == 1 ? 1.0f : -1.0f;
		float c = wanted + sign * static_cast<float>(k) * kFingerCurlStep;
		if (c >= 1.0f || c <= 0.0f) {
			c = c >= 1.0f ? 1.0f : 0.0f;
			ended[side] = true;
		}
		const float d = depthAt(c);
		if (d > 0.0f) {
			least = d < least ? d : least;
			leastCurl = d <= least ? c : leastCurl;
			return false;
		}
		float clear = c;
		float through = wanted + sign * static_cast<float>(k - 1) * kFingerCurlStep;
		for (int h = 0; h < kFingerCurlHalvings; ++h) {
			const float mid = 0.5f * (clear + through);
			(depthAt(mid) > 0.0f ? through : clear) = mid;
		}
		r.curl = clear;
		r.side = side == 1 ? CurlSide::More : CurlSide::Less;
		return true;
	};
	const bool kept = prefer != CurlSide::None;
	const int first = prefer == CurlSide::More ? 1 : 0;
	for (int pass = 0; pass < (kept ? 2 : 1); ++pass) {
		for (int k = 1; k <= steps; ++k) {
			for (int t = 0; t < (kept ? 1 : 2); ++t) {
				const int side = kept ? (pass == 0 ? first : 1 - first) : (t == 0 ? 0 : 1);
				if (!ended[side] && tryAt(side, k)) {
					return r;
				}
			}
		}
	}
	r.curl = leastCurl;
	r.behind = false;
	return r;
}

// The holding hand as the pin left it this frame: its bone's world transform
// and its four fingers' links (0 the index to 3 the little finger).
struct HeldHandBones {
	bool valid = false;
	NiTransform hand{NiMatrix33::Identity(), NiPoint3{0.0f, 0.0f, 0.0f}, 1.0f};
	FingerChain fingers[4];
};

struct HeldFingersInput {
	bool holding = false;      // FingersHeldBehindPage
	bool holderRight = false;  // the right role's hand holds it (its R bones)
	SheetInWorld sheet;
	HeldHandBones bones;
	// Each finger's skin round its bones, in the world's units: measured from
	// the drawn hand where it was (vr/FingerSkin.h), else the guess.
	float skin[4] = {kFingerSkinMetres, kFingerSkinMetres, kFingerSkinMetres, kFingerSkinMetres};
	// Each hand's curls as the controller says, thumb to little finger.
	float rightCurl[5] = {};
	float leftCurl[5] = {};
	CurlSide prefer[4] = {};  // each finger's side last frame
};

struct HeldFingersOutput {
	bool any = false;  // a finger held off the controller's curl
	float rightCurl[5] = {};
	float leftCurl[5] = {};
	CurlLimit limit[4];
};

// Both hands' curls to pose by: the holding hand's four fingers each limited
// by the page, its thumb and the other hand - the one that taps - as the
// controller says.
inline HeldFingersOutput HoldFingersBehind(const HeldFingersInput& in) {
	HeldFingersOutput out;
	for (int f = 0; f < 5; ++f) {
		out.rightCurl[f] = in.rightCurl[f];
		out.leftCurl[f] = in.leftCurl[f];
	}
	if (!in.holding || !in.bones.valid || !in.sheet.valid) {
		return out;
	}
	float* const curls = in.holderRight ? out.rightCurl : out.leftCurl;
	for (int f = 0; f < 4; ++f) {
		const FingerChain& chain = in.bones.fingers[f];
		if (!chain.valid) {
			continue;
		}
		const auto depthAt = [&](float curl) {
			NiPoint3 points[4];
			FingerPointsAt(in.bones.hand, chain, in.holderRight, f + 1, curl, points);
			return DepthBeforeSheet(in.sheet, points, 4, in.skin[f]);
		};
		out.limit[f] = CurlBehindSheet(curls[f + 1], in.prefer[f], depthAt);
		curls[f + 1] = out.limit[f].curl;
		out.any = out.any || out.limit[f].limited;
	}
	return out;
}

// Whether the holding hand's fingers are held behind the page this frame:
// the tablet held in one hand (not unrolling between the two, not rolled
// up), that hand's drawn fingers following the controller's, and its reach
// read - the page's first place taken with the fingers free (KeptReach).
inline bool FingersHeldBehindPage(bool menuOnTablet, float tabletShare, bool rolledUp, bool holderTracked,
                                  bool reachRead) {
	return menuOnTablet && tabletShare < 0.0f && !rolledUp && holderTracked && reachRead;
}

// The page stands before the hand's foremost point (PageLiftMetres), and the
// fingers held behind the page would pull that point back with them - the
// page after them, the fingers after the page. While they are held, a new
// reading is taken only where it reaches further, side by side: the page
// stays where the free hand put it. A misread stays one.
inline HandReach KeptReach(const HandReach& kept, const HandReach& read) {
	if (!read.valid || !kept.valid) {
		return read;
	}
	HandReach r = read;
	r.beforeLeft = kept.beforeLeft > read.beforeLeft ? kept.beforeLeft : read.beforeLeft;
	r.beforeRight = kept.beforeRight > read.beforeRight ? kept.beforeRight : read.beforeRight;
	return r;
}

}  // namespace obvr::vr
