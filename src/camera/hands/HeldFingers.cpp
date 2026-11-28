#include "camera/hands/HeldFingers.h"

#include <cstdio>

#include "core/Log.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {

namespace {

const char* const kHandBones[2] = {"Bip01 L Hand", "Bip01 R Hand"};
const char* const kLinkBones[2][4][3] = {
	{{"Bip01 L Finger1", "Bip01 L Finger11", "Bip01 L Finger12"},
	 {"Bip01 L Finger2", "Bip01 L Finger21", "Bip01 L Finger22"},
	 {"Bip01 L Finger3", "Bip01 L Finger31", "Bip01 L Finger32"},
	 {"Bip01 L Finger4", "Bip01 L Finger41", "Bip01 L Finger42"}},
	{{"Bip01 R Finger1", "Bip01 R Finger11", "Bip01 R Finger12"},
	 {"Bip01 R Finger2", "Bip01 R Finger21", "Bip01 R Finger22"},
	 {"Bip01 R Finger3", "Bip01 R Finger31", "Bip01 R Finger32"},
	 {"Bip01 R Finger4", "Bip01 R Finger41", "Bip01 R Finger42"}},
};
const char* const kThumbBones[2][3] = {{"Bip01 L Finger0", "Bip01 L Finger01", "Bip01 L Finger02"},
                                       {"Bip01 R Finger0", "Bip01 R Finger01", "Bip01 R Finger02"}};
const char* const kFingerWords[4] = {"index", "middle", "ring", "little"};

// The bones, once per tree: a finger whose links do not hang one under the
// other from the hand bone is left out (its chain could not be carried down).
void FindBones(HeldFingersState& state) {
	const NiAVObject* const root = game::FirstPersonArmsNode();
	if (root == state.root && state.searched) {
		return;
	}
	const UInt32 linesLeft = state.linesLeft;
	state = HeldFingersState{};
	state.linesLeft = linesLeft;
	state.root = root;
	state.searched = root != nullptr;
	for (int side = 0; side < 2 && root != nullptr; ++side) {
		state.hand[side] = game::FindFirstPersonNode(kHandBones[side]);
		UInt32 found = 0;
		for (int f = 0; f < 4; ++f) {
			const NiAVObject* parent = state.hand[side];
			bool chained = parent != nullptr;
			for (int i = 0; i < 3; ++i) {
				NiAVObject* const link = chained ? game::FindFirstPersonNode(kLinkBones[side][f][i]) : nullptr;
				chained = link != nullptr && link->parent == parent;
				state.links[side][f][i] = chained ? link : nullptr;
				parent = link;
			}
			found += chained ? 1u : 0u;
		}
		for (int i = 0; i < 3; ++i) {
			state.thumb[side][i] = game::FindFirstPersonNode(kThumbBones[side][i]);
		}
		OBVR_LOG("Tablet: the %s hand's finger links for the page's hold - %u of 4 fingers hung from \"%s\"%s",
		         side == 1 ? "right" : "left", found, kHandBones[side],
		         found == 4 ? "" : " (a finger not found is not held behind the page)");
	}
}

vr::HeldHandBones ReadBones(const HeldFingersState& state, bool rightBones) {
	vr::HeldHandBones b;
	const int side = rightBones ? 1 : 0;
	const NiAVObject* const hand = state.hand[side];
	if (hand == nullptr) {
		return b;
	}
	b.valid = true;
	b.hand = hand->worldTransform;
	for (int f = 0; f < 4; ++f) {
		vr::FingerChain& c = b.fingers[f];
		c.valid = state.links[side][f][2] != nullptr;
		for (int i = 0; c.valid && i < 3; ++i) {
			c.local[i] = state.links[side][f][i]->localTransform.pos;
			c.scale[i] = state.links[side][f][i]->localTransform.scale;
		}
	}
	return b;
}

// The holding hand's bones as drawn now (after the pose): the wrist, each
// finger's joints and its tip, the thumb's.
vr::HandBonesNow BonesNow(const HeldFingersState& state, bool rightBones) {
	vr::HandBonesNow b;
	const int side = rightBones ? 1 : 0;
	if (state.hand[side] == nullptr) {
		return b;
	}
	b.valid = true;
	b.wrist = state.hand[side]->worldTransform.pos;
	for (int f = 0; f < 4; ++f) {
		b.fingerValid[f] = state.links[side][f][2] != nullptr;
		b.finger[f][0] = b.wrist;
		for (int i = 0; b.fingerValid[f] && i < 3; ++i) {
			b.finger[f][i] = state.links[side][f][i]->worldTransform.pos;
		}
		b.finger[f][3] = vr::IndexTipBeyond(b.finger[f][1], b.finger[f][2]);
	}
	b.thumbValid = state.thumb[side][0] != nullptr && state.thumb[side][1] != nullptr && state.thumb[side][2] != nullptr;
	for (int i = 0; b.thumbValid && i < 3; ++i) {
		b.thumb[i] = state.thumb[side][i]->worldTransform.pos;
	}
	b.thumb[3] = vr::IndexTipBeyond(b.thumb[1], b.thumb[2]);
	return b;
}

// The line: per finger the curl asked and allowed, its skin as kept, and its
// deepest vertex against the page (through it, or behind it).
void SaySkin(const HeldFingersState& state, const vr::FingerSkinRead& r, bool mark) {
	char text[480];
	int at = 0;
	for (int f = 0; f < 4 && at >= 0 && at < static_cast<int>(sizeof(text)); ++f) {
		const vr::CurlLimit& l = state.out.limit[f];
		const double deepest = static_cast<double>(r.deepest[f] * 100.0f);
		at += std::snprintf(text + at, sizeof(text) - static_cast<size_t>(at),
		                    "%s%s asked %.2f allowed %.2f%s, skin %.1f cm%s, %s %.1f cm %s", f > 0 ? "; " : "",
		                    kFingerWords[f], static_cast<double>(state.asked[f]), static_cast<double>(l.curl),
		                    !l.limited ? " (free)" : (l.behind ? "" : " (none behind: the least deep)"),
		                    static_cast<double>(vr::SkinOf(state.skin, f) * 100.0f),
		                    state.skin.measured[f] ? "" : " (not measured)", r.over[f] ? "its deepest" : "not over the page,",
		                    r.over[f] ? (deepest > 0.0 ? deepest : -deepest) : 0.0,
		                    !r.over[f] ? "" : (deepest > 0.0 ? "through" : "behind"));
	}
	OBVR_LOG("Tablet: the %s hand's fingers against the page%s - %s", state.holderRight ? "right" : "left",
	         mark ? " (at the mark)" : "", text);
}

}  // namespace

void CurlsForFingers(HeldFingersState& state, const vr::HandModeResult& hand, bool rightTracked, bool leftTracked,
                     bool reachRead, const NiMatrix33& cameraRot, const NiPoint3& cameraPos, float unitsPerMetre,
                     game::FingerCurls& right, game::FingerCurls& left) {
	vr::HeldFingersInput in;
	for (int f = 0; f < 5; ++f) {
		in.rightCurl[f] = hand.rightCurl[f];
		in.leftCurl[f] = hand.leftCurl[f];
	}
	right.thumbJoints = hand.rightThumbValid;
	left.thumbJoints = hand.leftThumbValid;
	for (int joint = 0; joint < 3; ++joint) {
		right.thumb[joint] = hand.rightThumb[joint];
		left.thumb[joint] = hand.leftThumb[joint];
	}
	in.holderRight = hand.tabletHolderRight;
	in.holding = hand.leftHandValid && unitsPerMetre > 0.0f &&
	             vr::FingersHeldBehindPage(hand.menuOnTablet, hand.tabletShare, hand.tabletRolledUp,
	                                       in.holderRight ? rightTracked : leftTracked, reachRead);
	vr::HeldFingersOutput out;
	if (in.holding) {
		FindBones(state);
		in.bones = ReadBones(state, in.holderRight);
		in.sheet = vr::SheetOfPlace(hand.menuTransform, hand.tabletWidth, hand.tabletHeight, cameraRot,
		                            hand.leftHandRotation, cameraPos, hand.leftHandOffsetUnits, unitsPerMetre);
		for (int f = 0; f < 4; ++f) {
			in.skin[f] = vr::SkinOf(state.skin, f) * unitsPerMetre;
			in.prefer[f] = state.prefer[f];
			state.asked[f] = (in.holderRight ? in.rightCurl : in.leftCurl)[f + 1];
		}
		out = vr::HoldFingersBehind(in);
		for (int f = 0; f < 4; ++f) {
			state.prefer[f] = out.limit[f].limited && out.limit[f].behind ? out.limit[f].side : vr::CurlSide::None;
		}
		state.sheet = in.sheet;
		state.out = out;
	} else {
		out = vr::HoldFingersBehind(in);
		for (int f = 0; f < 4; ++f) {
			state.prefer[f] = vr::CurlSide::None;
		}
		vr::SkinLineDue(state.skinLine, false, 0, false);  // the next hold says its first reading again
		if (state.active && state.linesLeft > 0) {
			--state.linesLeft;
			OBVR_LOG("Tablet: the fingers follow the controller again - the page no longer held in one hand");
		}
	}
	state.active = in.holding;
	state.holderRight = in.holderRight;
	for (int f = 0; f < 5; ++f) {
		right.curl[f] = out.rightCurl[f];
		left.curl[f] = out.leftCurl[f];
	}
}

void MeasureFingerSkin(HeldFingersState& state, const std::vector<NiPoint3>& vertices, float unitsPerMetre) {
	if (!state.active) {
		vr::SkinLineDue(state.skinLine, false, 0, false);
		return;
	}
	const vr::FingerSkinRead r = vr::ReadFingerSkin(vertices.data(), static_cast<UInt32>(vertices.size()),
	                                                BonesNow(state, state.holderRight), state.sheet, unitsPerMetre);
	vr::KeepFingerSkin(state.skin, r);
	UInt8 mask = 0;
	for (int f = 0; f < 4; ++f) {
		const bool through = r.over[f] && r.deepest[f] > 0.0f;
		mask = static_cast<UInt8>(mask | (state.out.limit[f].limited ? 1u << f : 0u) | (through ? 0x10u << f : 0u));
	}
	const bool mark = test::HandScriptMarkedThisFrame();
	if (r.valid && vr::SkinLineDue(state.skinLine, true, mask, mark)) {
		SaySkin(state, r, mark);
	}
}

}  // namespace obvr::camera::hands
