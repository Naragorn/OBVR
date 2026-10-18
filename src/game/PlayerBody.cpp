#include "game/PlayerBody.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "core/Rotation.h"
#include "game/BodyIk.h"
#include "game/BodyPlacement.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "test/HandScriptRuntime.h"

namespace obvr::game {
namespace {

constexpr const char* kHeadName = "Bip01 Head";
constexpr const char* kSkeletonName = "Bip01";
constexpr const char* kLeftClavicleName = "Bip01 L Clavicle";
constexpr const char* kRightClavicleName = "Bip01 R Clavicle";
constexpr UInt16 kHiddenBit = 0x0001;

bool LooksLikeObject(const void* pointer) {
	return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer));
}

bool NameIs(const char* actual, const char* expected) {
	if (!LooksLikeObject(actual) || expected == nullptr) {
		return false;
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const char a = actual[at];
		const char e = expected[at];
		if (a != e) {
			return false;
		}
		if (a == '\0') {
			return at > 0;
		}
	}
	return false;
}

bool NameLooksReal(const char* name) {
	if (!LooksLikeObject(name)) {
		return false;
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const unsigned char value = static_cast<unsigned char>(name[at]);
		if (value == '\0') {
			return at > 0;
		}
		if (value < 0x20 || value > 0x7E) {
			return false;
		}
	}
	return false;
}

// One log line per reason, so a skeleton that cannot be reached says so once
// and a later one that can is not silenced by it.
struct Reported {
	bool root = false;
	bool method = false;
	bool head = false;
	bool skeleton = false;
	bool skeletonMoved = false;
	bool steppedBack = false;
	bool leftClavicle = false;
	bool rightClavicle = false;
	bool parent = false;
	bool shown = false;
};
Reported g_reported{};

// The root this code took the hidden bit off, so it can be put back on that
// node and no other. A load or a skeleton swap makes a different root, and
// the old one is never dereferenced again - only compared.
NiAVObject* g_unhiddenRoot = nullptr;

NiAVObject* ThirdPersonRoot() {
	auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return nullptr;
	}
	NiAVObject* const root =
		*reinterpret_cast<NiAVObject* const*>(player + addr::kReferenceNodeOffset);
	if (!LooksLikeObject(root)) {
		return nullptr;
	}
	// The node has to identify itself before its method table is trusted;
	// the same check ThirdPersonAimVisual makes of the same node.
	if (!NameLooksReal(root->name)) {
		if (!g_reported.root) {
			g_reported.root = true;
			OBVR_LOG("Body: the player node at %08X has no readable name - the body "
			         "stays as the engine has it",
			         reinterpret_cast<UInt32>(root));
		}
		return nullptr;
	}
	return root;
}

// NiAVObject::GetObject through the root's vtable, as ThirdPersonAimVisual
// does it: the slot has to point into Oblivion's code, and the node that
// comes back has to carry the name that was asked for.
NiAVObject* NamedNode(NiAVObject* root, const char* wantedName, bool& missingReported) {
	auto* const table = static_cast<UInt8*>(root->vtable);
	if (!LooksLikeObject(table)) {
		return nullptr;
	}
	const UInt32 functionAddress =
		*reinterpret_cast<const UInt32*>(table + addr::kNiAVObjectGetObjectVtableOffset);
	if (functionAddress < addr::kTextStart || functionAddress >= addr::kTextEnd) {
		if (!g_reported.method) {
			g_reported.method = true;
			OBVR_LOG("Body: GetObject slot +%02X holds %08X, outside Oblivion's code - the "
			         "body stays as the engine has it",
			         addr::kNiAVObjectGetObjectVtableOffset, functionAddress);
		}
		return nullptr;
	}
	using GetObjectFn = NiAVObject*(__fastcall*)(NiAVObject* self, void* unusedEdx,
	                                           const char* name);
	const auto getObject = reinterpret_cast<GetObjectFn>(functionAddress);
	NiAVObject* const node = getObject(root, nullptr, wantedName);
	if (!LooksLikeObject(node) || !NameIs(node->name, wantedName)) {
		if (!missingReported) {
			missingReported = true;
			OBVR_LOG("Body: %s was not found under node \"%s\" - that part is left as "
			         "the engine has it",
			         wantedName, root->name);
		}
		return nullptr;
	}
	return node;
}

// Collapses a node for this frame's draw: its world transform is recomputed
// at scale zero, so everything skinned to it and everything hanging under it
// lands on one point, and then the LOCAL scale is put back so nothing the
// engine or a menu reads off the local transform later sees a zero. This is
// Enhanced Camera's UpdateSkeletonNodes(0) / ApplyAnimData /
// UpdateSkeletonNodes(1) sequence, with the update pass in the middle.
void CollapseForThisDraw(NiAVObject* node) {
	const float saved = node->localTransform.scale;
	node->localTransform.scale = 0.0f;
	UpdateNodeTransforms(node);
	node->localTransform.scale = saved;
}

// Debug.BodyTreeProbe: the whole third-person tree once per skeleton, and
// the bones an arm solver would write - each one's local transform under
// its parent (the bone's length is its child's local position, its axes
// the rows of its rotation) and its world position, as the animation left
// them this frame. At a hand script's mark the tree is logged again, for
// what an equip changed in it (the shapes of a robe put on by the script's
// console lines come after the first probe; robe-hands.txt, 2026-10-18).
const NiAVObject* g_probedRoot = nullptr;

void ProbeBodyTree(NiAVObject* root) {
	if (root == g_probedRoot && !test::HandScriptMarkedThisFrame()) {
		return;
	}
	g_probedRoot = root;
	ProbeNodeTree(root, "Body tree", 400);
	static const char* const kBones[] = {
		"Bip01",           "Bip01 Pelvis",       "Bip01 Spine",         "Bip01 Spine1",
		"Bip01 Spine2",    "Bip01 Neck",         "Bip01 Head",          "Bip01 L Clavicle",
		"Bip01 L UpperArm", "Bip01 L UpperArmTwist", "Bip01 L Forearm",  "Bip01 L ForearmTwist",
		"Bip01 L Hand",    "Bip01 R Clavicle",   "Bip01 R UpperArm",    "Bip01 R UpperArmTwist",
		"Bip01 R Forearm", "Bip01 R ForearmTwist", "Bip01 R Hand",      "Bip01 L Thigh",
		"Bip01 L Calf",    "Bip01 L Foot",       "Bip01 R Thigh",       "Bip01 R Calf",
		"Bip01 R Foot",
	};
	for (const char* name : kBones) {
		bool missing = false;
		NiAVObject* const bone = NamedNode(root, name, missing);
		if (bone == nullptr) {
			OBVR_LOG("Body bones: \"%s\" - not under the skeleton", name);
			continue;
		}
		const NiTransform& l = bone->localTransform;
		const NiTransform& w = bone->worldTransform;
		const NiAVObject* const parent = bone->parent;
		OBVR_LOG("Body bones: \"%s\" under \"%s\" - local pos %.2f %.2f %.2f scale %.3f, rot rows [%.3f %.3f %.3f] "
		         "[%.3f %.3f %.3f] [%.3f %.3f %.3f]; world %.1f %.1f %.1f scale %.3f",
		         name, LooksLikeObject(parent) && NameLooksReal(parent->name) ? parent->name : "?",
		         static_cast<double>(l.pos.x), static_cast<double>(l.pos.y), static_cast<double>(l.pos.z),
		         static_cast<double>(l.scale), static_cast<double>(l.rot.data[0][0]),
		         static_cast<double>(l.rot.data[0][1]), static_cast<double>(l.rot.data[0][2]),
		         static_cast<double>(l.rot.data[1][0]), static_cast<double>(l.rot.data[1][1]),
		         static_cast<double>(l.rot.data[1][2]), static_cast<double>(l.rot.data[2][0]),
		         static_cast<double>(l.rot.data[2][1]), static_cast<double>(l.rot.data[2][2]),
		         static_cast<double>(w.pos.x), static_cast<double>(w.pos.y), static_cast<double>(w.pos.z),
		         static_cast<double>(w.scale));
	}
}

// The skeleton's readings reported ("Body check:"): once per skeleton when it
// reads sane, and each time a verdict changes - some lines at most.
const NiAVObject* g_checkedRoot = nullptr;
SkeletonVerdict g_checkedVerdict = SkeletonVerdict::Sane;
bool g_checkedOnce = false;
const NiAVObject* g_dryRunRoot = nullptr;
const NiAVObject* g_afterRoot = nullptr;
SkeletonVerdict g_afterVerdict = SkeletonVerdict::Sane;
bool g_afterOnce = false;
UInt32 g_checkLinesLeft = 24;

SkeletonReading ReadSkeleton(const NiAVObject* skeleton, const NiAVObject* head, bool ik) {
	SkeletonReading r;
	r.rootScale = skeleton->worldTransform.scale;
	const NiAVObject* const parent = skeleton->parent;
	if (LooksLikeObject(parent)) {
		r.parentScale = parent->worldTransform.scale;
		const NiMatrix33& m = parent->worldTransform.rot;
		r.parentColumns =
		    NiPoint3{ik::Length(ik::Column(m, 0)), ik::Length(ik::Column(m, 1)), ik::Length(ik::Column(m, 2))};
	}
	const NiPoint3 headOff = head->worldTransform.pos - skeleton->worldTransform.pos;
	r.finite = ik::Finite(skeleton->worldTransform.pos) && ik::Finite(head->worldTransform.pos) && ik::Finite(headOff);
	r.headUnits = ik::Length(headOff);
	if (ik) {
		for (UInt32 side = 0; side < 2; ++side) {
			ReadBodyArmUnits(side, r.upperUnits[side], r.foreUnits[side]);
		}
	}
	return r;
}

void LogSkeletonCheck(const char* when, const SkeletonReading& r, SkeletonVerdict verdict) {
	if (g_checkLinesLeft == 0) {
		return;
	}
	--g_checkLinesLeft;
	OBVR_LOG("Body check%s: Bip01 scale %.3f under a parent of scale %.3f (columns %.2f %.2f %.2f), the head %.1f "
	         "units from it, the right arm %.1f + %.1f, the left %.1f + %.1f units (0 = not read) - %s%s",
	         when, static_cast<double>(r.rootScale), static_cast<double>(r.parentScale),
	         static_cast<double>(r.parentColumns.x), static_cast<double>(r.parentColumns.y),
	         static_cast<double>(r.parentColumns.z), static_cast<double>(r.headUnits),
	         static_cast<double>(r.upperUnits[0]), static_cast<double>(r.foreUnits[0]),
	         static_cast<double>(r.upperUnits[1]), static_cast<double>(r.foreUnits[1]),
	         SkeletonVerdictWords(verdict),
	         verdict == SkeletonVerdict::Sane ? "" : ": the body is left hidden this frame and the IK stands down");
}

// The body hidden again for this frame and the IK stood down: nothing more
// is written to the skeleton.
void HideBodyForThisFrame(NiAVObject* root) {
	StandDownBodyIk();
	if (g_unhiddenRoot == root) {
		root->flags = static_cast<UInt16>(root->flags | kHiddenBit);
		g_unhiddenRoot = nullptr;
	}
}

// The skeleton as it reads this frame, judged before anything is written to
// it (BodyPlacement.h, JudgeSkeleton). On a skeleton's first sane frame the
// engine's own update of Bip01 is run once with nothing written and the
// reading taken again: a tester's skeleton (2026-10-16) read sane before the
// placement and 2170 times out after it, in the same frame, and whether the
// write or the update does that is what the second reading tells. False
// when either reading is no body's: the body is hidden again, the IK stands
// down, and the log says what was read.
bool SkeletonReadsLikeABody(NiAVObject* root, NiAVObject* skeleton, const NiAVObject* head, bool ik) {
	const SkeletonReading r = ReadSkeleton(skeleton, head, ik);
	const SkeletonVerdict verdict = JudgeSkeleton(r);
	if (!g_checkedOnce || root != g_checkedRoot || verdict != g_checkedVerdict) {
		LogSkeletonCheck("", r, verdict);
	}
	g_checkedOnce = true;
	g_checkedRoot = root;
	g_checkedVerdict = verdict;
	if (verdict != SkeletonVerdict::Sane) {
		HideBodyForThisFrame(root);
		return false;
	}
	if (g_dryRunRoot != root) {
		g_dryRunRoot = root;
		UpdateNodeTransforms(skeleton);
		const SkeletonReading after = ReadSkeleton(skeleton, head, ik);
		const SkeletonVerdict afterVerdict = JudgeSkeleton(after);
		LogSkeletonCheck(" after the engine's own update of Bip01, nothing written", after, afterVerdict);
		if (afterVerdict != SkeletonVerdict::Sane) {
			HideBodyForThisFrame(root);
			return false;
		}
	}
	return true;
}

// The reading again after the placement wrote Bip01's position and ran the
// engine's update of it - where the tester's skeleton came apart. The body
// moved further than any body's height is the same verdict. Said once per
// skeleton and whenever it changes.
bool SkeletonStillReadsLikeABody(NiAVObject* root, NiAVObject* skeleton, const NiAVObject* head, bool ik,
                                 float loweredUnits) {
	const SkeletonReading r = ReadSkeleton(skeleton, head, ik);
	SkeletonVerdict verdict = JudgeSkeleton(r);
	const float scale = ik::Finite(r.rootScale) && r.rootScale > 0.0f ? r.rootScale : 1.0f;
	if (verdict == SkeletonVerdict::Sane &&
	    (!ik::Finite(loweredUnits) || loweredUnits > kSkeletonHeadMaxUnits * scale ||
	     loweredUnits < -kSkeletonHeadMaxUnits * scale)) {
		verdict = SkeletonVerdict::HeadOff;
	}
	if (!g_afterOnce || root != g_afterRoot || verdict != g_afterVerdict) {
		char when[96];
		std::snprintf(when, sizeof(when), " after the placement (Bip01 let down %.1f units)",
		              static_cast<double>(loweredUnits));
		LogSkeletonCheck(when, r, verdict);
	}
	g_afterOnce = true;
	g_afterRoot = root;
	g_afterVerdict = verdict;
	if (verdict != SkeletonVerdict::Sane) {
		HideBodyForThisFrame(root);
		return false;
	}
	return true;
}

}  // namespace

bool ShowPlayerBody(const BodyFrame& frame) {
	NiAVObject* const root = ThirdPersonRoot();
	if (root == nullptr) {
		return false;
	}

	// The engine hides the whole body in first person with the bit it tests
	// at 0x00664FC2; taking it off is what makes the body draw. Remembered so
	// Release can put it back on this very node.
	if ((root->flags & kHiddenBit) != 0) {
		root->flags = static_cast<UInt16>(root->flags & ~kHiddenBit);
		g_unhiddenRoot = root;
	}

	NiAVObject* const head = NamedNode(root, kHeadName, g_reported.head);
	if (head == nullptr) {
		return false;
	}

	// The body IK (game/BodyIk.h) first puts back what it wrote last frame
	// where the engine left it, then turns the body with the head - both
	// before anything below reads the head's place.
	const bool ik = BeginBodyIk(root);

	// The skeleton read for sense before anything is written to it: one that
	// reads like no body (a tester's log of 2026-10-16, BodyPlacement.h) is
	// left hidden and nothing below runs.
	NiAVObject* const skeleton = NamedNode(root, kSkeletonName, g_reported.skeleton);
	NiAVObject* const parent = skeleton != nullptr ? skeleton->parent : nullptr;
	if (skeleton != nullptr && !SkeletonReadsLikeABody(root, skeleton, head, ik)) {
		return false;
	}
	if (ik) {
		BodyTorsoFrame torso;
		torso.headValid = frame.headYawValid;
		torso.headWorldYaw = frame.headWorldYaw;
		torso.headTrackingYaw = frame.headTrackingYaw;
		torso.dtSeconds = frame.dtSeconds;
		torso.standsIdle = frame.standsIdle;
		StepBodyTorso(frame.ik, torso);
	}
	float lowered = 0.0f;

	// Stand the skeleton where the head lands under the headset. The
	// SKELETON, not the player's root node: the camera hangs under that root
	// too (xOBSE finds Camera01 from it), so moving the root moved the
	// camera with the body, and the first headset run had the view lurch
	// with every look up or down. Bip01 is the skeleton's top bone, the
	// camera's sibling; moving it moves the body and nothing else. Written
	// under its parent, as every node write in OBVR is, and propagated with
	// the engine's own pass so the whole skeleton follows.
	if (skeleton == nullptr || !LooksLikeObject(parent)) {
		if (skeleton != nullptr && !g_reported.parent) {
			g_reported.parent = true;
			OBVR_LOG("Body: node \"%s\" has no readable parent - the body is shown where "
			         "the engine has it, not under the headset",
			         skeleton->name);
		}
	} else {
		BodyRootInput in;
		in.cameraWorld = frame.cameraWorld;
		in.headWorld = head->worldTransform.pos;
		in.rootWorld = skeleton->worldTransform.pos;
		in.rootWorldRot = skeleton->worldTransform.rot;
		in.rootWorldScale = skeleton->worldTransform.scale;
		in.eyeOffsetUnits = frame.eyeOffsetUnits;
		in.parentRot = parent->worldTransform.rot;
		in.parentPos = parent->worldTransform.pos;
		in.parentScale = parent->worldTransform.scale;
		// The eyes in front of the head bone the way the HEAD faces, pitch and
		// all - the head bone the neck's pivot - and the body stepped back
		// under a bowed head, always, as far as the camera's near plane asks
		// (BodyPlacement.h, BodyLookDownShift, LookDownBackUnitsFor).
		if (frame.headRotValid) {
			in.eyesFollowHead = true;
			in.headRot = frame.headRot;
			in.lookDownBackUnits = LookDownBackUnitsFor(frame.cameraNearUnits);
		}
		const float zBefore = skeleton->worldTransform.pos.z;
		const NiPoint3 local = BodyRootLocalPos(in);
		if (ik) {
			WriteBodyRootPosition(local);
		} else {
			skeleton->localTransform.pos = local;
		}
		UpdateNodeTransforms(skeleton);
		lowered = zBefore - skeleton->worldTransform.pos.z;
		if (!SkeletonStillReadsLikeABody(root, skeleton, head, ik, lowered)) {
			return false;
		}
		BodyPlacementTrace trace;
		trace.cameraWorld = in.cameraWorld;
		trace.parentWorld = in.parentPos;
		trace.headOffsetRaw = in.headWorld - in.rootWorld;
		trace.headOffsetUsed = trace.headOffsetRaw;
		trace.lowered = lowered;
		NoteBodyPlacement(trace);
		if (!g_reported.skeletonMoved) {
			g_reported.skeletonMoved = true;
			OBVR_LOG("Body: \"%s\" at %08X under \"%s\" is what moves under the headset; the "
			         "player root \"%s\" stays where the engine puts it",
			         skeleton->name, reinterpret_cast<UInt32>(skeleton),
			         NameLooksReal(parent->name) ? parent->name : "?", root->name);
		}
		// Said once, the first time a look down takes the body back past
		// half its way: that the step back runs, and how far.
		if (in.eyesFollowHead && !g_reported.steppedBack) {
			const float scale = in.rootWorldScale > 0.0f ? in.rootWorldScale : 1.0f;
			const NiPoint3 back = BodyLookDownShift(in.headRot, in.lookDownBackUnits * scale);
			const float units = math::Sqrt(back.x * back.x + back.y * back.y);
			if (units > in.lookDownBackUnits * scale * 0.5f) {
				g_reported.steppedBack = true;
				const float sine = -in.headRot.data[2][1];
				const float s = sine > 1.0f ? 1.0f : (sine < -1.0f ? -1.0f : sine);
				OBVR_LOG("Body: stepped back %.1f units under a head bowed %.0f degrees - the near plane's cut of "
				         "the chest out of view (at most %.0f looking straight down, for a near plane of %.1f)",
				         static_cast<double>(units),
				         static_cast<double>(math::Atan2(s, math::Sqrt(1.0f - s * s)) * math::kRadiansToDegrees),
				         static_cast<double>(in.lookDownBackUnits * scale),
				         static_cast<double>(frame.cameraNearUnits > 0.0f ? frame.cameraNearUnits : 10.0f));
			}
		}
	}

	// The knees, for a body let down below its legs' own height.
	if (ik) {
		StepBodyLegs(frame.ik, lowered);
	}

	if (frame.treeProbe) {
		ProbeBodyTree(root);
	}

	// The head and the animated arms are collapsed AFTER the move, so the
	// zero-scale world transforms are the ones the draw sees. With the IK
	// running, the arms are decided after the pins (StepBodyArms): each one
	// that follows its hand stays, the others collapse there.
	if (frame.hideHead) {
		CollapseForThisDraw(head);
	}
	if (frame.hideArms && !ik) {
		if (NiAVObject* const left = NamedNode(root, kLeftClavicleName, g_reported.leftClavicle)) {
			CollapseForThisDraw(left);
		}
		if (NiAVObject* const right =
		        NamedNode(root, kRightClavicleName, g_reported.rightClavicle)) {
			CollapseForThisDraw(right);
		}
	}

	if (!g_reported.shown) {
		g_reported.shown = true;
		OBVR_LOG("Body: the third-person skeleton \"%s\" at %08X is shown in first person, "
		         "stood under the headset each frame (eyes %.1f forward, %.1f up "
		         "from %s), head %s, clavicles %s; the IK %s (arms %d, torso %d, legs %d)",
		         root->name, reinterpret_cast<UInt32>(root),
		         static_cast<double>(frame.eyeOffsetUnits.y),
		         static_cast<double>(frame.eyeOffsetUnits.z), kHeadName,
		         frame.hideHead ? "collapsed" : "kept",
		         ik ? "decided after the pins" : (frame.hideArms ? "collapsed" : "kept"),
		         ik ? "running" : "not running", frame.ik.arms ? 1 : 0, frame.ik.torso ? 1 : 0,
		         frame.ik.legs ? 1 : 0);
	}
	return true;
}

NiAVObject* PlayerBodyRoot() { return ThirdPersonRoot(); }

void ReleasePlayerBody(bool isThirdPerson) {
	// The IK's writes and hides given back first, while the root they were
	// made under is still the one looked for.
	ReleaseBodyIk(BodyIkHolds() ? ThirdPersonRoot() : nullptr);
	if (g_unhiddenRoot == nullptr) {
		return;
	}
	NiAVObject* const root = ThirdPersonRoot();
	if (root == g_unhiddenRoot && !isThirdPerson) {
		root->flags = static_cast<UInt16>(root->flags | kHiddenBit);
	}
	g_unhiddenRoot = nullptr;
}

bool InstallPlayerBodyPatches() {
	static const UInt8 kJumpA[addr::kPovSwitchSkipBodyPatchSize] = {0x0F, 0x84, 0x33, 0x02, 0x00, 0x00};
	static const UInt8 kJumpB[addr::kPovSwitchSkipBodyPatchSize] = {0x0F, 0x84, 0xFE, 0x01, 0x00, 0x00};
	static const UInt8 kNops[addr::kPovSwitchSkipBodyPatchSize] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};

	const bool aIsJump = mem::Verify(addr::kPovSwitchSkipBodyA, kJumpA, addr::kPovSwitchSkipBodyPatchSize);
	const bool bIsJump = mem::Verify(addr::kPovSwitchSkipBodyB, kJumpB, addr::kPovSwitchSkipBodyPatchSize);
	const bool aIsNops = mem::Verify(addr::kPovSwitchSkipBodyA, kNops, addr::kPovSwitchSkipBodyPatchSize);
	const bool bIsNops = mem::Verify(addr::kPovSwitchSkipBodyB, kNops, addr::kPovSwitchSkipBodyPatchSize);
	if (aIsNops && bIsNops) {
		// Enhanced Camera itself, or an earlier OBVR load, already did this.
		OBVR_LOG("Body: the POV switch already runs the third-person body's setup in first "
		         "person (both jumps at %08X and %08X are NOPs)",
		         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
		return true;
	}
	if (!aIsJump || !bIsJump) {
		OBVR_LOG("Body: bytes at %08X or %08X are not the expected jumps, the POV switch is "
		         "left alone - the body may be missing after a load in first person until "
		         "the view is switched once",
		         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
		mem::ReportForeignCode("Body", aIsJump ? addr::kPovSwitchSkipBodyB : addr::kPovSwitchSkipBodyA);
		return false;
	}
	if (!mem::SafeWrite(addr::kPovSwitchSkipBodyA, kNops, addr::kPovSwitchSkipBodyPatchSize) ||
	    !mem::SafeWrite(addr::kPovSwitchSkipBodyB, kNops, addr::kPovSwitchSkipBodyPatchSize)) {
		OBVR_LOG("Body: writing the POV switch patches at %08X / %08X failed",
		         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
		return false;
	}
	OBVR_LOG("Body: the POV switch now runs the third-person body's setup in first person "
	         "too (jumps at %08X and %08X replaced with NOPs, after Enhanced Camera)",
	         addr::kPovSwitchSkipBodyA, addr::kPovSwitchSkipBodyB);
	return true;
}

}  // namespace obvr::game
