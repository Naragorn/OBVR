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

// Said as the set of fingers held changes, and at a hand script's mark.
void SayHeld(HeldFingersState& state, const vr::HeldFingersInput& in, const vr::HeldFingersOutput& out) {
	UInt8 mask = 0;
	for (int f = 0; f < 4; ++f) {
		mask = static_cast<UInt8>(mask | (out.limit[f].limited ? 1u << f : 0u));
	}
	const bool mark = test::HandScriptMarkedThisFrame();
	if (!mark && (mask == state.heldMask || state.linesLeft == 0)) {
		return;
	}
	if (mask != state.heldMask && state.linesLeft > 0) {
		--state.linesLeft;
	}
	state.heldMask = mask;
	const float* const wanted = in.holderRight ? in.rightCurl : in.leftCurl;
	char text[320];
	int at = 0;
	for (int f = 0; f < 4 && at >= 0 && at < static_cast<int>(sizeof(text)); ++f) {
		const vr::CurlLimit& l = out.limit[f];
		at += std::snprintf(text + at, sizeof(text) - static_cast<size_t>(at), "%s%s %.2f%s%.2f%s", f > 0 ? ", " : "",
		                    kFingerWords[f], static_cast<double>(wanted[f + 1]), l.limited ? " to " : " (free)",
		                    static_cast<double>(l.curl), !l.limited ? "" : (l.behind ? "" : " (none behind: the least deep)"));
	}
	OBVR_LOG("Tablet: the %s hand's fingers by the page%s - %s", in.holderRight ? "right" : "left",
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
		in.skin = vr::kFingerSkinMetres * unitsPerMetre;
		for (int f = 0; f < 4; ++f) {
			in.prefer[f] = state.prefer[f];
		}
		out = vr::HoldFingersBehind(in);
		for (int f = 0; f < 4; ++f) {
			state.prefer[f] = out.limit[f].limited && out.limit[f].behind ? out.limit[f].side : vr::CurlSide::None;
		}
		SayHeld(state, in, out);
	} else {
		out = vr::HoldFingersBehind(in);
		for (int f = 0; f < 4; ++f) {
			state.prefer[f] = vr::CurlSide::None;
		}
		if (state.active && state.linesLeft > 0) {
			--state.linesLeft;
			OBVR_LOG("Tablet: the fingers follow the controller again - the page no longer held in one hand");
		}
		state.heldMask = 0;
	}
	state.active = in.holding;
	for (int f = 0; f < 5; ++f) {
		right.curl[f] = out.rightCurl[f];
		left.curl[f] = out.leftCurl[f];
	}
}

}  // namespace obvr::camera::hands
