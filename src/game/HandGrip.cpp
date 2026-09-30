#include "game/HandGrip.h"

#include <cstdio>
#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

constexpr UInt32 kMaxFingerLinks = 24;

// One hand's fingers while OBVR poses them: the links found under the hand
// bone, the rotations the animation had given them (put back when OBVR lets
// go), and what OBVR last wrote - a link that no longer holds that has been
// written by the animation since, and its rotation is the animation's again.
struct Fingers {
	const NiAVObject* root = nullptr;  // the first-person root they were found under
	NiAVObject* hand = nullptr;
	NiAVObject* links[kMaxFingerLinks] = {};
	NiMatrix33 base[kMaxFingerLinks];
	NiMatrix33 written[kMaxFingerLinks];
	UInt32 count = 0;
	bool found = false;
	FingerPose pose = FingerPose::Animation;
	bool gripReported = false;
	bool trackReported = false;
	bool littleReported = false;
	bool handleReported = false;
};

Fingers g_fingers[2];

// Whether each hand held an item last frame, for the log.
signed char g_holdsLogged[2] = {-1, -1};
UInt32 g_holdLinesLeft = 8;

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

const char* NameOf(const NiAVObject* node) {
	return LooksLikeObject(node->name) ? node->name : "";
}

bool Same(const NiMatrix33& a, const NiMatrix33& b) { return std::memcmp(&a, &b, sizeof(NiMatrix33)) == 0; }

// Each link's animation rotation: what it holds now, unless that is what
// OBVR wrote last.
void TakeBases(Fingers& f, bool fresh) {
	for (UInt32 i = 0; i < f.count; ++i) {
		if (!LooksLikeObject(f.links[i])) {
			continue;
		}
		const NiMatrix33& now = f.links[i]->localTransform.rot;
		if (fresh || !Same(now, f.written[i])) {
			f.base[i] = now;
		}
	}
}

void GiveBack(Fingers& f) {
	for (UInt32 i = 0; i < f.count; ++i) {
		if (LooksLikeObject(f.links[i]) && Same(f.links[i]->localTransform.rot, f.written[i])) {
			f.links[i]->localTransform.rot = f.base[i];
		}
	}
	if (LooksLikeObject(f.hand)) {
		UpdateNodeTransforms(f.hand);
	}
	f.pose = FingerPose::Animation;
}

void Find(Fingers& f, bool rightHand, const char* handBoneName) {
	f.found = true;
	f.hand = FindFirstPersonNode(handBoneName);
	f.count = f.hand != nullptr ? CollectNodesContaining(f.hand, "Finger", f.links, kMaxFingerLinks) : 0;
	char names[256];
	UInt32 at = 0;
	for (UInt32 i = 0; i < f.count && at + 40 < sizeof(names); ++i) {
		const char* name = NameOf(f.links[i]);
		if (at > 0) {
			names[at++] = ',';
		}
		for (UInt32 c = 0; name[c] != '\0' && c < 32; ++c) {
			names[at++] = name[c];
		}
	}
	names[at] = '\0';
	OBVR_LOG("Hands: the %s hand's fingers - %u links under \"%s\" (%s)", rightHand ? "right" : "left", f.count,
	         handBoneName, names);
}

// The first tracked frame's check that the table and the engine agree: the
// index finger's first link is a constant in both the idle and the fists
// (idle.kf, handtohandidle.kf), so what the animation left there should be
// one of the two, to the table's three decimals.
void ReportTracking(const Fingers& f, bool rightHand) {
	for (UInt32 i = 0; i < f.count; ++i) {
		int finger = 0;
		int link = 0;
		if (!FingerLinkOf(NameOf(f.links[i]), finger, link) || link != 3) {
			continue;
		}
		const NiMatrix33 open = TrackedLinkRotation(rightHand, link, 0.0f);
		const NiMatrix33 fist = TrackedLinkRotation(rightHand, link, 1.0f);
		float offOpen = 0.0f;
		float offFist = 0.0f;
		for (int r = 0; r < 3; ++r) {
			for (int c = 0; c < 3; ++c) {
				const float a = f.base[i].data[r][c];
				const float toOpen = a > open.data[r][c] ? a - open.data[r][c] : open.data[r][c] - a;
				const float toFist = a > fist.data[r][c] ? a - fist.data[r][c] : fist.data[r][c] - a;
				offOpen = toOpen > offOpen ? toOpen : offOpen;
				offFist = toFist > offFist ? toFist : offFist;
			}
		}
		const NiMatrix33& b = f.base[i];
		OBVR_LOG("Hands: the %s fingers follow the controller - \"%s\" from the animation (%.3f %.3f %.3f / "
		         "%.3f %.3f %.3f / %.3f %.3f %.3f), off the table's open hand by %.3f, its fist by %.3f",
		         rightHand ? "right" : "left", NameOf(f.links[i]), static_cast<double>(b.data[0][0]),
		         static_cast<double>(b.data[0][1]), static_cast<double>(b.data[0][2]),
		         static_cast<double>(b.data[1][0]), static_cast<double>(b.data[1][1]),
		         static_cast<double>(b.data[1][2]), static_cast<double>(b.data[2][0]),
		         static_cast<double>(b.data[2][1]), static_cast<double>(b.data[2][2]),
		         static_cast<double>(offOpen), static_cast<double>(offFist));
		return;
	}
	OBVR_LOG("Hands: the %s fingers follow the controller - no index finger link found to compare",
	         rightHand ? "right" : "left");
}

// The little finger's links as OBVR wrote them this frame (the angle about
// z: the open hand's 4 to 30 degrees, the fist's 65 to 102) and whether last
// frame's write was still there - "kept no" means something else wrote the
// link in between, which the animation does each frame.
void ReportLittleFinger(const Fingers& f, bool rightHand, float share, const bool* kept) {
	char text[256];
	UInt32 at = 0;
	for (UInt32 i = 0; i < f.count && at + 64 < sizeof(text); ++i) {
		int finger = 0;
		int link = 0;
		if (!FingerLinkOf(NameOf(f.links[i]), finger, link) || finger != kLittleFinger) {
			continue;
		}
		const int written = std::snprintf(text + at, sizeof(text) - at, "%s\"%s\" %.0f degrees, kept %s",
		                                  at > 0 ? "; " : "", NameOf(f.links[i]),
		                                  static_cast<double>(LinkCurlDegrees(f.written[i])), kept[i] ? "yes" : "no");
		at += written > 0 ? static_cast<UInt32>(written) : 0;
	}
	OBVR_LOG("Hands: the %s little finger curled %.2f - %s", rightHand ? "right" : "left",
	         static_cast<double>(share), at > 0 ? text : "no little finger link found");
}

}  // namespace

void ForgetHandGrip() {
	g_fingers[0] = Fingers{};
	g_fingers[1] = Fingers{};
}

void StepHandFingers(bool rightHand, const char* handBoneName, FingerPose pose, float curlDegrees,
                     const FingerCurls* curls, float towardHandle) {
	Fingers& f = g_fingers[rightHand ? 0 : 1];
	NiAVObject* const root = FirstPersonArmsNode();
	if (root == nullptr || f.root != root) {
		// A new model: the old links went with the old tree, never written.
		const bool gripReported = f.gripReported;
		const bool trackReported = f.trackReported;
		const bool littleReported = f.littleReported;
		const bool handleReported = f.handleReported;
		f = Fingers{};
		f.root = root;
		f.gripReported = gripReported;
		f.trackReported = trackReported;
		f.littleReported = littleReported;
		f.handleReported = handleReported;
		if (root == nullptr) {
			return;
		}
	}
	if (!f.found) {
		Find(f, rightHand, handBoneName);
	}
	// The animation's fingers on their way to a handle are written too.
	if (pose == FingerPose::Animation && !(towardHandle > 0.0f)) {
		if (f.pose != FingerPose::Animation) {
			GiveBack(f);
		}
		return;
	}
	if (f.count == 0) {
		return;
	}
	// Before the bases are taken: whether last frame's writes were kept.
	const bool littleDue = curls != nullptr && f.pose == FingerPose::Tracked &&
	                       LittleFingerCheckDue(f.littleReported, pose, curls->curl[kLittleFinger]);
	bool littleKept[kMaxFingerLinks] = {};
	for (UInt32 i = 0; littleDue && i < f.count; ++i) {
		littleKept[i] = LooksLikeObject(f.links[i]) && Same(f.links[i]->localTransform.rot, f.written[i]);
	}
	TakeBases(f, f.pose == FingerPose::Animation);
	if (pose == FingerPose::Grip && !f.gripReported) {
		f.gripReported = true;
		OBVR_LOG("Hands: the %s hand closes around what it holds, %.0f degrees a link",
		         rightHand ? "right" : "left", static_cast<double>(curlDegrees));
	}
	if (pose == FingerPose::Handle && !f.handleReported) {
		f.handleReported = true;
		OBVR_LOG("Hands: the %s hand's fingers close round the handle - the game's own grip (twohandidle.kf), "
		         "%u links", rightHand ? "right" : "left", f.count);
	}
	if (pose == FingerPose::Tracked && !f.trackReported) {
		f.trackReported = true;
		ReportTracking(f, rightHand);
	}
	// Written this frame, whichever pose: the next frame keeps its bases.
	f.pose = pose == FingerPose::Animation ? FingerPose::Handle : pose;
	for (UInt32 i = 0; i < f.count; ++i) {
		if (!LooksLikeObject(f.links[i])) {
			continue;
		}
		const char* const name = NameOf(f.links[i]);
		NiMatrix33 rot = f.base[i];
		int finger = 0;
		int link = 0;
		const bool known = FingerLinkOf(name, finger, link);
		if (pose == FingerPose::Grip) {
			rot = CurledAboutZ(f.base[i], FingerCurlDegrees(name, curlDegrees));
		} else if (pose == FingerPose::Tracked) {
			if (known && curls != nullptr) {
				rot = TrackedLinkRotation(rightHand, link, LinkShare(*curls, finger, link));
			}
		}
		// Round the handle: the game's own grip (a link it does not know keeps
		// the animation's).
		NiMatrix33 onHandle = f.base[i];
		if (known) {
			HandleLinkRotation(rightHand, link, onHandle);
		}
		if (pose == FingerPose::Handle) {
			rot = onHandle;
		} else if (towardHandle > 0.0f) {
			rot = BlendRotation(rot, onHandle, towardHandle);
		}
		f.links[i]->localTransform.rot = rot;
		f.written[i] = rot;
	}
	UpdateNodeTransforms(f.hand);
	if (littleDue) {
		f.littleReported = true;
		ReportLittleFinger(f, rightHand, curls->curl[kLittleFinger], littleKept);
	}
}

bool HandHoldsItem(bool rightHand, const char* handBoneName) {
	// The hand bone the fingers were found under, while that tree stands.
	const Fingers& f = g_fingers[rightHand ? 0 : 1];
	NiAVObject* const root = FirstPersonArmsNode();
	const NiAVObject* const hand =
		f.found && root != nullptr && f.root == root ? f.hand : FindFirstPersonNode(handBoneName);
	if (!LooksLikeObject(hand)) {
		return false;
	}
	const UInt8* const bytes = reinterpret_cast<const UInt8*>(hand);
	UInt8* const* const children = *reinterpret_cast<UInt8* const* const*>(bytes + addr::kNiChildrenOffset);
	const UInt16 count = *reinterpret_cast<const UInt16*>(bytes + addr::kNiChildCountOffset);
	if (!LooksLikeObject(children) || count > 64) {
		return true;  // cannot be told: the animation keeps the fingers
	}
	const char* holding = nullptr;
	for (UInt16 i = 0; i < count && holding == nullptr; ++i) {
		const NiAVObject* const child = reinterpret_cast<const NiAVObject*>(children[i]);
		if (!LooksLikeObject(child)) {
			continue;
		}
		const char* const name = NameOf(child);
		int finger = 0;
		int link = 0;
		if (FingerLinkOf(name, finger, link) || !NiClassIsNode(NiClassNameOf(child))) {
			continue;
		}
		const UInt8* const childBytes = reinterpret_cast<const UInt8*>(child);
		UInt8* const* const hung = *reinterpret_cast<UInt8* const* const*>(childBytes + addr::kNiChildrenOffset);
		const UInt16 hungCount = *reinterpret_cast<const UInt16*>(childBytes + addr::kNiChildCountOffset);
		if (!LooksLikeObject(hung)) {
			continue;
		}
		for (UInt16 k = 0; k < hungCount && k < 64; ++k) {
			if (LooksLikeObject(hung[k])) {
				holding = name;
				break;
			}
		}
	}
	const signed char now = holding != nullptr ? 1 : 0;
	signed char& logged = g_holdsLogged[rightHand ? 0 : 1];
	if (now != logged && g_holdLinesLeft > 0) {
		--g_holdLinesLeft;
		if (holding != nullptr) {
			OBVR_LOG("Hands: the %s hand holds \"%s\" - its fingers stay as the animation has them",
			         rightHand ? "right" : "left", holding);
		} else {
			OBVR_LOG("Hands: the %s hand holds nothing of its own", rightHand ? "right" : "left");
		}
		logged = now;
	}
	return holding != nullptr;
}

}  // namespace obvr::game
