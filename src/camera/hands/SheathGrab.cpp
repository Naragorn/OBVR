#include "camera/hands/SheathGrab.h"

#include "camera/CameraHook.h"
#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/FirstPersonHide.h"
#include "game/GameTypes.h"
#include "game/PlayerBody.h"
#include "test/HandScriptRuntime.h"
#include "vr/Holster.h"
#include "vr/LaserGeometry.h"
#include "vr/SheathReach.h"

namespace obvr::camera::hands {

namespace {

constexpr UInt32 kHiddenFlag = 1;  // NiAVObject's app-culled bit, as game/SheathTrace.cpp reads it

// Drawn: neither it nor anything above it up to `root` is hidden.
bool Shown(const NiAVObject* node, const NiAVObject* root) {
	const NiAVObject* at = node;
	for (UInt32 i = 0; i < 48 && mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(at)); ++i, at = at->parent) {
		if ((at->flags & kHiddenFlag) != 0) {
			return false;
		}
		if (at == root) {
			return true;
		}
	}
	return false;
}

// The hilt is read again every kReadEvery frames and kept in between: the
// read walks the body's tree for two nodes, and in the body's frame the hilt
// moves slowly (an Index's grip reads a resting hand as a pull, so "while the
// grip closes" is no gate).
constexpr UInt32 kReadEvery = 3;
struct Kept {
	bool valid = false;
	NiPoint3 at{0.0f, 0.0f, 0.0f};
	UInt32 age = 0;
};
Kept& KeptHilt() {
	static Kept s_kept;
	return s_kept;
}

const char* KindName(vr::SlotItem item) {
	return item == vr::SlotItem::Bow ? "bow" : (item == vr::SlotItem::OneHand ? "one-hander" : "two-hander");
}

// At a hand script's mark: where the weapon hangs, whatever the hands do - a
// scenario's grip is put there (sheath-grab.txt).
void SayAtMark(vr::SlotItem item, const NiPoint3& at) {
	if (test::HandScriptMarkedThisFrame()) {
		OBVR_LOG("Sheath grab: at the mark \"%s\" the %s on the body hangs %.2f right, %.2f forward, %.2f up of the eyes",
		         test::HandScriptMarkName(), KindName(item), static_cast<double>(at.x), static_cast<double>(at.y),
		         static_cast<double>(at.z));
	}
}

}  // namespace

void ReadSheathIntoFrame(vr::HandModeFrame& frame, const vr::OpenVRBackend& backend, const Config& config) {
	frame.body.sheathValid = false;
	const vr::SlotItem item = vr::SlotItemOf(frame.equipped);
	const bool bow = item == vr::SlotItem::Bow;
	if (!config.hands.holster.enabled || !frame.headValid || frame.weaponSeen != vr::WeaponSeen::Sheathed ||
	    (!bow && !vr::WeaponHandItem(item))) {
		KeptHilt().valid = false;
		return;
	}
	Kept& kept = KeptHilt();
	if (kept.valid && ++kept.age < kReadEvery) {
		frame.body.sheath = kept.at;
		frame.body.sheathValid = true;
		SayAtMark(item, kept.at);
		return;
	}
	kept.valid = false;
	NiAVObject* const root = game::PlayerBodyRoot();
	if (root == nullptr) {
		return;
	}
	static const char* const kNames[2] = {"SideWeapon", "BackWeapon"};
	NiAVObject* nodes[2] = {nullptr, nullptr};
	game::FindNodesNamed(root, kNames, 2, nodes);
	const NiAVObject* const node = nodes[item == vr::SlotItem::OneHand ? 0 : 1];
	UInt32 count = 0;
	NiAVObject* const* const children = node != nullptr ? game::NodeChildren(node, count) : nullptr;
	const NiAVObject* weapon = nullptr;
	for (UInt32 i = 0; children != nullptr && i < count && weapon == nullptr; ++i) {
		weapon = children[i];
	}
	NiTransform camera;
	vr::openvr::HmdMatrix34 head;
	if (weapon == nullptr || !Shown(weapon, root) || !GetCyclopeanCameraWorld(camera) || !backend.GetRenderPoseMatrix(head)) {
		return;
	}
	const NiPoint3 room = vr::WorldPointInTracking(head, camera.rot, camera.pos, weapon->worldTransform.pos,
	                                               config.tracker.unitsPerMetre);
	frame.body.sheath = vr::PlaceRelative(frame.body, frame.head, frame.headPosition, room);
	frame.body.sheathValid = true;
	kept.valid = true;
	kept.at = frame.body.sheath;
	kept.age = 0;
	SayAtMark(item, kept.at);
	static UInt32 s_lines = 12;
	const vr::HandPose& taker = bow ? frame.left : frame.right;
	const NiPoint3 hand = vr::PlaceRelative(frame.body, frame.head, frame.headPosition, taker.position);
	// said while the hand that takes it is near and its grip closing
	if (s_lines > 0 && taker.valid && vr::GripDown(taker.buttonsPressed) &&
	    (hand - frame.body.sheath).LengthSquared() < 0.4f * 0.4f) {
		--s_lines;
		OBVR_LOG("Sheath grab: the %s on the body hangs %.2f right, %.2f forward, %.2f up of the eyes; the gripping "
		         "hand at %.2f %.2f %.2f, %.2f m from it (taken within %.2f)",
		         KindName(item),
		         static_cast<double>(frame.body.sheath.x), static_cast<double>(frame.body.sheath.y),
		         static_cast<double>(frame.body.sheath.z), static_cast<double>(hand.x), static_cast<double>(hand.y),
		         static_cast<double>(hand.z),
		         static_cast<double>(math::Sqrt((hand - frame.body.sheath).LengthSquared())),
		         static_cast<double>(vr::kSheathReachMetres));
	}
}

}  // namespace obvr::camera::hands
