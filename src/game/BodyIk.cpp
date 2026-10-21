#include "game/BodyIk.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/BodyPlacement.h"
#include "game/BoneMirrorRead.h"
#include "game/BonePin.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(const void* pointer) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(pointer)); }

constexpr UInt16 kHiddenBit = 0x0001;
const NiPoint3 kUp{0.0f, 0.0f, 1.0f};

// The bones, in the order a walk of the third-person tree is asked for them.
// The trunk - NonAccum to the head - is the chain held at the standing
// idle's pose (BodyIkLogic.h, "The trunk"); the probe of 2026-10-11 read it
// as Bip01 > NonAccum > Pelvis > Spine > Spine1 > Spine2 > Neck > Neck1 >
// Head, the pelvis 67 units up the NonAccum node, the clavicles and the
// thighs hanging off the neck and the pelvis.
enum : UInt32 {
	kRoot = 0,
	kNonAccum,
	kPelvis,
	kSpine,
	kSpine1,
	kSpine2,
	kNeck,
	kNeck1,
	kHead,
	kArmRight,                    // clavicle, upper arm and its twist, forearm and its twist, hand
	kArmLeft = kArmRight + 6,
	kLegRight = kArmLeft + 6,     // thigh, calf, foot
	kLegLeft = kLegRight + 3,
	kBodyBones = kLegLeft + 3,
};
enum : UInt32 { kTrunkFirst = kNonAccum, kTrunkBones = kHead - kNonAccum + 1 };
enum : UInt32 { kClavicle = 0, kUpper, kUpperTwist, kFore, kForeTwist, kHand, kArmParts };
enum : UInt32 { kThigh = 0, kCalf, kFoot };

const char* const kBodyNames[kBodyBones] = {
	"Bip01",
	"Bip01 NonAccum",
	"Bip01 Pelvis",
	"Bip01 Spine",
	"Bip01 Spine1",
	"Bip01 Spine2",
	"Bip01 Neck",
	"Bip01 Neck1",
	"Bip01 Head",
	"Bip01 R Clavicle",
	"Bip01 R UpperArm",
	"Bip01 R UpperArmTwist",
	"Bip01 R Forearm",
	"Bip01 R ForearmTwist",
	"Bip01 R Hand",
	"Bip01 L Clavicle",
	"Bip01 L UpperArm",
	"Bip01 L UpperArmTwist",
	"Bip01 L Forearm",
	"Bip01 L ForearmTwist",
	"Bip01 L Hand",
	"Bip01 R Thigh",
	"Bip01 R Calf",
	"Bip01 R Foot",
	"Bip01 L Thigh",
	"Bip01 L Calf",
	"Bip01 L Foot",
};

// The first-person arm bones carry the same names: the arms' twelve.
constexpr UInt32 kFpBones = 2 * kArmParts;
const char* const* const kFpNames = &kBodyNames[kArmRight];

// ------------------------------------------------------------ The booking

struct BoneWrite {
	NiAVObject* node = nullptr;
	NiTransform base{};   // the local transform before this frame's write
	NiTransform wrote{};  // what was written
	bool held = false;    // written this frame (or last, until begun again)
};

bool SameTransform(const NiTransform& a, const NiTransform& b) {
	for (int r = 0; r < 3; ++r) {
		for (int c = 0; c < 3; ++c) {
			if (a.rot.data[r][c] != b.rot.data[r][c]) {
				return false;
			}
		}
	}
	return a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z && a.scale == b.scale;
}

// Before a bone is read: what it held before last frame's write goes back
// if it still holds that write; then its base is what it has now. `kept`
// counts the bones the engine left, `renewed` the ones it wrote again.
bool BeginWrite(BoneWrite& w, NiAVObject* node, UInt32& kept, UInt32& renewed) {
	bool restored = false;
	if (w.held && w.node == node && node != nullptr) {
		if (SameTransform(node->localTransform, w.wrote)) {
			node->localTransform = w.base;
			restored = true;
			++kept;
		} else {
			++renewed;
		}
	}
	w.held = false;
	w.node = node;
	if (node != nullptr) {
		w.base = node->localTransform;
	}
	return restored;
}

void WriteLocal(BoneWrite& w, const NiTransform& local) {
	if (w.node == nullptr) {
		return;
	}
	w.node->localTransform = local;
	w.wrote = local;
	w.held = true;
}

// Given back for good: only the node recorded, found again in the tree the
// engine has now, and only if it still holds the write.
bool ReleaseWrite(BoneWrite& w, NiAVObject* nodeNow) {
	bool restored = false;
	if (w.held && w.node != nullptr && w.node == nodeNow && SameTransform(nodeNow->localTransform, w.wrote)) {
		nodeNow->localTransform = w.base;
		restored = true;
	}
	w = BoneWrite{};
	return restored;
}

// The rotation and position a world pose needs under a parent whose world
// pose is given; the bone's own scale kept.
NiTransform LocalFor(const NiTransform& current, const NiMatrix33& parentRot, const NiPoint3& parentPos,
                     float parentScale, const NiMatrix33& rot, const NiPoint3& pos) {
	const BonePose local = LocalUnderParent(parentRot, parentPos, parentScale, BonePose{rot, pos});
	NiTransform t = current;
	t.rot = local.rot;
	t.pos = local.pos;
	return t;
}

// Collapses a node for this frame's draw (PlayerBody.cpp's, Enhanced
// Camera's): its world at scale zero, its local scale given back.
void CollapseForThisDraw(NiAVObject* node) {
	const float saved = node->localTransform.scale;
	node->localTransform.scale = 0.0f;
	UpdateNodeTransforms(node);
	node->localTransform.scale = saved;
}

const char* SafeName(const NiAVObject* node) {
	if (!LooksLikeObject(node) || !LooksLikeObject(node->name)) {
		return "";
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const char c = node->name[at];
		if (c == '\0') {
			return node->name;
		}
		if (c < 0x20 || c > 0x7E) {
			return "";
		}
	}
	return "";
}

bool StartsWith(const char* text, const char* prefix) {
	for (; *prefix != '\0'; ++text, ++prefix) {
		if (*text != *prefix) {
			return false;
		}
	}
	return true;
}

bool Is(const char* a, const char* b) {
	for (; *a != '\0' && *a == *b; ++a, ++b) {
	}
	return *a == *b;
}

float YawOfRotation(const NiMatrix33& rot, float fallback) {
	Heading h;
	return HeadingOf(rot, h) ? math::Atan2(h.sine, h.cosine) : fallback;
}

// ------------------------------------------------------------ The frame

struct ArmStats {
	bool follows = false;
	bool reached = false;
	float shortUnits = 0.0f;
	float stretch = 1.0f;
	float clavicleDegrees = 0.0f;
	float upperUnits = 0.0f;
	float foreUnits = 0.0f;
	NiPoint3 shoulder{0.0f, 0.0f, 0.0f};
	NiPoint3 elbow{0.0f, 0.0f, 0.0f};
	NiPoint3 wrist{0.0f, 0.0f, 0.0f};
	NiPoint3 target{0.0f, 0.0f, 0.0f};
	float upperRollDegrees = 0.0f;
	float foreTwistDegrees = 0.0f;   // the forearm's twist bone, its roll in the world (ForearmRollsFor)
	float forearmRollDegrees = 0.0f;  // the forearm bone about its own length
	float handRollDegrees = 0.0f;    // the hand's roll on the forearm, continued
	NiPoint3 handAxis{0.0f, 0.0f, 0.0f};  // the hand bone's +x, wrist to fingers
	NiPoint3 pole{0.0f, 0.0f, 0.0f};      // where the elbow was sent
	float fpOffUnits = 0.0f;    // the first-person arm bones read back against the body's, at most
	float fpOffDegrees = 0.0f;
	float reachRatio = 0.0f;    // the hand's distance from the shoulder over the arm's length
	float bendDegrees = 0.0f;   // the upper arm off the shoulder-hand line (0 straight)
	bool elbowMoved = false;    // the pole's elbow was in the torso and was moved round
	UInt32 carried = 0;         // things on the forearm's twist carried with the hand (the shield)
	float carriedOffUnits = 0.0f;  // and how far they were carried off where the forearm would have them
	const char* why = "";  // when it does not follow
};

struct Frame {
	bool valid = false;  // BeginBodyIk found the skeleton this frame
	NiAVObject* root = nullptr;
	NiAVObject* bone[kBodyBones] = {};
	float bodyYaw = 0.0f;         // the body's world yaw, HeadingOf's sense
	bool torsoTurned = false;
	float torsoOffHead = 0.0f;    // radians
	float chestCorrection = 0.0f;
	bool trunkHeld = false;       // the trunk at the standing pose this frame
	float lowered = 0.0f;
	float feetLift = 0.0f;
	// Where the animation put each foot (the right, the left) this frame,
	// turned with the body, before the trunk was held and the body moved -
	// and the root then, so the feet can be carried with the body's move
	// across the ground (the placement stands the body forward of where the
	// animation keeps its root, every frame, by the eyes' distance from the
	// camera; the feet go with it, not the legs angled back to the spot).
	bool footValid[2] = {false, false};
	NiPoint3 footAnimated[2] = {};
	NiPoint3 rootAnimated{0.0f, 0.0f, 0.0f};
	// How far each foot stood from where the animation had it, up (positive:
	// sunk, solved up) or down (floating, solved down), and whether it was
	// solved - for the trace.
	float footDelta[2] = {0.0f, 0.0f};
	bool footSolved[2] = {false, false};
	ArmStats arm[2];
};

Frame g_frame;
BoneWrite g_body[kBodyBones];
BoneWrite g_fp[kFpBones];

// The trunk's standing pose (BodyIkLogic.h, "The trunk"): the local
// transforms of the chain, taken once per skeleton from a frame the player
// stood idle long enough. A skeleton seen anew takes it again.
struct TrunkReference {
	bool valid = false;
	NiAVObject* root = nullptr;
	NiTransform local[kTrunkBones] = {};
};
TrunkReference g_trunk;
float g_standingSeconds = 0.0f;
bool g_trunkReported = false;

// What each arm carries from frame to frame: the elbow's eased direction,
// which way round the torso it went, and the two rolls continued the short
// way (UnwrapAngle). Forgotten when the arm stops following.
struct ArmMemory {
	bool valid = false;
	NiPoint3 pole{0.0f, 0.0f, -1.0f};
	float sideSign = 0.0f;
	float upperRoll = 0.0f;
	float foreRoll = 0.0f;  // the hand's roll on the forearm, continued (HoldForeRollMemory)
};
ArmMemory g_armMemory[2];
// The arms traced over the first seconds after a load (StepStartTrace) -
// the session's first start and the next three times the body is taken up
// again (a load, a menu, third person), so menus do not fill the log.
StartTrace g_startTrace;
UInt32 g_startTracesLeft = 3;
// What hangs on a first-person forearm's twist bone (the shield), carried
// with the hand (BodyIkLogic.h, "The shield"): a side's four at most.
constexpr UInt32 kCarriedMax = 4;
BoneWrite g_carried[2][kCarriedMax];
NiAVObject* g_fpBone[kFpBones] = {};
TorsoYawState g_torso;
// The arm bones' partners on the other side (BoneMirror.h), for the
// first-person arms laid onto the body's other arm under the left-handed
// mirror: read from the body's skins once per body root.
ArmBonePartnersRead g_partners;
const NiAVObject* g_partnersRoot = nullptr;
bool g_partnersTried = false;

// The partners for this body, read on first need. False, with the why in
// g_partners, when the skins did not give them.
bool ArmPartnersReady() {
	if (g_partnersRoot != g_frame.root || !g_partnersTried) {
		g_partnersRoot = g_frame.root;
		g_partnersTried = true;
		const bool ok = ReadArmBonePartners(g_frame.root, &g_frame.bone[kArmRight], g_partners);
		if (ok) {
			OBVR_LOG("Body IK: the arm bones' partners for the mirror read from the body's skins (%u of %u shapes "
			         "skinned, taken from %s) - the first-person arms are laid onto the body's other arm",
			         g_partners.shapesSkinned, g_partners.shapesWalked, g_partners.shapes);
		} else {
			char missing[160] = "";
			UInt32 at = 0;
			for (UInt32 i = 0; i < 2 * kArmPartCount && at < sizeof(missing) - 40; ++i) {
				if (!g_partners.partners.found[i / kArmPartCount][i % kArmPartCount]) {
					at += static_cast<UInt32>(std::snprintf(missing + at, sizeof(missing) - at, "%s%s",
					                                        at > 0 ? ", " : "", kBodyNames[kArmRight + i]));
				}
			}
			OBVR_LOG("Body IK: the arm bones' partners for the mirror did not read - %s (%u of %u shapes skinned%s%s); "
			         "the body's arms are left to the body",
			         g_partners.why != nullptr ? g_partners.why : "?", g_partners.shapesSkinned,
			         g_partners.shapesWalked, at > 0 ? "; missing: " : "", missing);
		}
	}
	return g_partners.partners.valid;
}

// ------------------------------------------------------------ The trace
//
// Debug.BodyTrace: one line a frame with everything the body's smoothness
// hangs on - the root against the camera, the skeleton's parent as the
// engine has it, the head bone's offset from the root as the animation left
// it and as the placement took it, how far the body was let down, the
// torso's yaw, each arm's reach, bend and elbow direction - and how many of
// the frame's writes the engine left through the draw (NoteBodyRenderDone:
// a bone rewritten between the write and the draw never showed the write).
// tools/body-trace.ps1 reads the frame-to-frame differences out of it.
BodyPlacementTrace g_placement;
UInt32 g_heldThroughDraw = 0;
UInt32 g_writtenThroughDraw = 0;
UInt32 g_fpHeldThroughDraw = 0;
UInt32 g_fpWrittenThroughDraw = 0;

// One line per thing worth saying once.
bool g_bonesReported = false;
bool g_keptReported = false;
bool g_fpKeptReported = false;
bool g_torsoReported = false;
bool g_legsReported = false;
bool g_armReported[2] = {false, false};
bool g_whyReported[2] = {false, false};
bool g_swappedReported = false;
UInt32 g_hideLines = 10;

void ReportKept(const char* what, UInt32 kept, UInt32 renewed, const bool* restored, NiAVObject* const* nodes,
                UInt32 count, bool& reported) {
	if (reported || kept + renewed == 0) {
		return;
	}
	reported = true;
	char names[256] = "";
	UInt32 at = 0;
	for (UInt32 i = 0; i < count && at < sizeof(names) - 40; ++i) {
		if (restored[i] && nodes[i] != nullptr) {
			const char* const n = SafeName(nodes[i]);
			for (UInt32 k = 0; n[k] != '\0' && at < sizeof(names) - 4; ++k) {
				names[at++] = n[k];
			}
			names[at++] = ',';
			names[at++] = ' ';
			names[at] = '\0';
		}
	}
	OBVR_LOG("Body IK: of the %u %s bones written last frame, %u still held the writes - the engine left them, put back "
	         "to the animation's pose (%s...) - and %u were the animation's again",
	         kept + renewed, what, kept, names, renewed);
}

// ------------------------------------------------------------ The hides
//
// While an arm follows its hand the body's own hands would sit inside the
// first-person ones (the same mesh, other fingers), and what they hold
// would be there twice: the body's hand models ("Hand  (<form>)"), whatever
// hangs on a hand bone that is not a bone itself (the "Weapon" and "Torch"
// nodes, a bow), and whatever hangs on a forearm's twist bone (the shield)
// are hidden - the engine's own hidden bit, set only on nodes this code
// found shown, cleared only on those.
constexpr UInt32 kMaxHidden = 24;
NiAVObject* g_hidden[kMaxHidden] = {};
UInt32 g_hiddenCount = 0;
NiAVObject* g_hiddenRoot = nullptr;

int HiddenIndex(const NiAVObject* node) {
	for (UInt32 i = 0; i < g_hiddenCount; ++i) {
		if (g_hidden[i] == node) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

// BodyNodeHiddenOnArm (BodyIkLogic.h): the hand slot's model, a suit's own
// "Hand" shape, what hangs on a hand bone or a forearm twist.
bool HiddenOnArm(const char* name, const char* parentName) { return BodyNodeHiddenOnArm(name, parentName); }

void HideWalk(NiAVObject* node, const char* parentName, UInt32 depth, bool* keep) {
	if (!LooksLikeObject(node) || depth > 40) {
		return;
	}
	const char* const name = SafeName(node);
	const int at = HiddenIndex(node);
	if (HiddenOnArm(name, parentName)) {
		if (at >= 0) {
			keep[at] = true;
		} else if ((node->flags & kHiddenBit) == 0 && g_hiddenCount < kMaxHidden) {
			node->flags = static_cast<UInt16>(node->flags | kHiddenBit);
			keep[g_hiddenCount] = true;
			g_hidden[g_hiddenCount++] = node;
			if (g_hideLines > 0) {
				--g_hideLines;
				OBVR_LOG("Body IK: the body's \"%s\" (on \"%s\") hidden while an arm follows its hand - the "
				         "first-person hand and what it holds are the ones seen",
				         name, parentName);
			}
		}
		return;
	}
	if (at >= 0) {
		node->flags = static_cast<UInt16>(node->flags & ~kHiddenBit);  // no longer one of them: shown, forgotten
	}
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(node, count);
	for (UInt32 i = 0; i < count; ++i) {
		HideWalk(children[i], name, depth + 1, keep);
	}
}

void ShowWalk(NiAVObject* node, UInt32 depth) {
	if (!LooksLikeObject(node) || depth > 40) {
		return;
	}
	if (HiddenIndex(node) >= 0) {
		node->flags = static_cast<UInt16>(node->flags & ~kHiddenBit);
	}
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(node, count);
	for (UInt32 i = 0; i < count; ++i) {
		ShowWalk(children[i], depth + 1);
	}
}

void ReleaseHides(NiAVObject* rootNow) {
	if (g_hiddenCount > 0 && rootNow != nullptr && rootNow == g_hiddenRoot && LooksLikeObject(rootNow)) {
		ShowWalk(rootNow, 0);
	}
	g_hiddenCount = 0;
	g_hiddenRoot = nullptr;
}

void StepHides(NiAVObject* root) {
	if (root != g_hiddenRoot) {
		g_hiddenCount = 0;  // a new tree: what was hidden went with the old one
		g_hiddenRoot = root;
	}
	bool keep[kMaxHidden] = {};
	HideWalk(root, "", 0, keep);
	UInt32 kept = 0;
	for (UInt32 i = 0; i < g_hiddenCount; ++i) {
		if (keep[i]) {
			g_hidden[kept++] = g_hidden[i];
		}
	}
	g_hiddenCount = kept;
}

// ------------------------------------------------------------ The arms

// The first-person arm laid onto the body's: the clavicle, the upper arm and
// the forearm with their twist bones at the body's own world poses, the hand
// at its pin. The hand's mesh is skinned to the forearm and its twist as
// well (BonePin.h, "Bare wrists"; its wrist edge the body's "Arms" edge, the
// same eleven vertices with the same weights - ArmStump.h), so with the same
// bones at the same places the cuff meets the body's sleeve or skin exactly.
//
// Under the left-handed mirror (BodyIk.h, `mirrored`) the first-person arm
// of the OTHER side is laid onto this body arm, each bone's world pose
// carried through the skin's bind-pose partner (BoneMirror.h): the
// first-person right-arm bones, drawn turned over, land on the body's left
// arm with their cuff on its sleeve. The hand stays at its pin.
void AlignFirstPersonArm(UInt32 side, const NiMatrix33& handRot, const NiPoint3& handPos, bool mirrored) {
	NiAVObject* const* const body = &g_frame.bone[side == 0 ? kArmRight : kArmLeft];
	const UInt32 fpSide = FirstPersonSideFor(side, mirrored);
	NiAVObject* const* const fp = &g_fpBone[fpSide * kArmParts];
	BoneWrite* const writes = &g_fp[fpSide * kArmParts];
	// The twist bone where the pins left it, before it is laid onto the
	// body's: what hangs on it keeps that place (BodyIkLogic.h, "The shield").
	NiAVObject* const twist = fp[kForeTwist];
	const NiTransform twistBefore = twist != nullptr ? twist->worldTransform : NiTransform{};
	struct Want {
		NiMatrix33 rot;
		NiPoint3 pos;
		float scale;
		bool set;
	} want[kArmParts] = {};
	NiAVObject* top = nullptr;
	for (UInt32 part = 0; part < kArmParts; ++part) {
		NiAVObject* const node = fp[part];
		if (node == nullptr || !LooksLikeObject(node->parent)) {
			continue;
		}
		const NiAVObject* const parent = node->parent;
		NiMatrix33 parentRot = parent->worldTransform.rot;
		NiPoint3 parentPos = parent->worldTransform.pos;
		float parentScale = parent->worldTransform.scale;
		for (UInt32 q = 0; q < part; ++q) {
			if (fp[q] == parent && want[q].set) {
				parentRot = want[q].rot;
				parentPos = want[q].pos;
				parentScale = want[q].scale;
			}
		}
		Want& w = want[part];
		if (part == kHand) {
			w.rot = handRot;
			w.pos = handPos;
			w.scale = node->worldTransform.scale;  // the hand keeps its size
		} else {
			if (body[part] == nullptr) {
				continue;
			}
			const NiTransform at = mirrored ? PartnerWorld(body[part]->worldTransform,
			                                               g_partners.partners.toOther[side][part])
			                                : body[part]->worldTransform;
			w.rot = at.rot;
			w.pos = at.pos;
			w.scale = at.scale;
		}
		const float divisor = parentScale > 0.0f ? parentScale : 1.0f;
		// The true inverse: the parent's matrix may carry a race's height.
		const NiMatrix33 inverseParent = InverseMatrix(parentRot);
		NiTransform local;
		local.rot = inverseParent * w.rot;
		local.pos = (inverseParent * (w.pos - parentPos)) * (1.0f / divisor);
		local.scale = w.scale / divisor;
		WriteLocal(writes[part], local);
		w.set = true;
		if (top == nullptr) {
			top = node;
		}
	}
	if (top != nullptr) {
		UpdateNodeTransforms(top);
	}
	ArmStats& stats = g_frame.arm[side];
	// What hangs on the twist: carried with the hand where the pins held it.
	stats.carried = 0;
	stats.carriedOffUnits = 0.0f;
	if (twist != nullptr && want[kForeTwist].set) {
		UInt32 count = 0;
		NiAVObject* const* const children = NodeChildren(twist, count);
		for (UInt32 i = 0; i < count && stats.carried < kCarriedMax; ++i) {
			NiAVObject* const child = children[i];
			if (!LooksLikeObject(child)) {
				continue;
			}
			// Its own local transform is the base: RestoreFirstPersonArmWrites
			// put last frame's carry back before the pins.
			BoneWrite& w = g_carried[fpSide][stats.carried];
			w = BoneWrite{};
			w.node = child;
			w.base = child->localTransform;
			const NiPoint3 wasAt = twist->worldTransform.pos + twist->worldTransform.rot *
			                                                     (child->localTransform.pos * twist->worldTransform.scale);
			WriteLocal(w, CarriedLocal(twistBefore, twist->worldTransform, w.base));
			UpdateNodeTransforms(child);
			const float off = ik::Length(child->worldTransform.pos - wasAt);
			stats.carriedOffUnits = off > stats.carriedOffUnits ? off : stats.carriedOffUnits;
			++stats.carried;
		}
	}
	// Read back: how far each first-person bone ended from where it was
	// sent, after the engine's update - the log's word on the seam.
	stats.fpOffUnits = 0.0f;
	stats.fpOffDegrees = 0.0f;
	for (UInt32 part = 0; part < kArmParts; ++part) {
		if (fp[part] == nullptr || !want[part].set) {
			continue;
		}
		const float off = ik::Length(fp[part]->worldTransform.pos - want[part].pos);
		const float along = ik::Dot(ik::Column(fp[part]->worldTransform.rot, 0), ik::Column(want[part].rot, 0));
		const float degrees = ik::Acos(along) * math::kRadiansToDegrees;
		stats.fpOffUnits = off > stats.fpOffUnits ? off : stats.fpOffUnits;
		stats.fpOffDegrees = degrees > stats.fpOffDegrees ? degrees : stats.fpOffDegrees;
	}
}

// The torso the elbows are kept out of, from this frame's animated bones
// (put back and turned with the body, no arm written yet): between the
// shoulder joints, half their width, their height and the thighs'.
TorsoKeepOut TorsoOfFrame() {
	TorsoKeepOut t;
	t.right = NiPoint3{math::Cos(g_frame.bodyYaw), math::Sin(g_frame.bodyYaw), 0.0f};
	t.forward = NiPoint3{-t.right.y, t.right.x, 0.0f};
	const NiAVObject* const left = g_frame.bone[kArmLeft + kUpper];
	const NiAVObject* const right = g_frame.bone[kArmRight + kUpper];
	if (left == nullptr || right == nullptr) {
		t.shoulderLateral = 0.0f;  // nothing to keep out of: no elbow is inside a torso of no width
		t.shoulderZ = -1.0e9f;
		return t;
	}
	const NiPoint3& l = left->worldTransform.pos;
	const NiPoint3& r = right->worldTransform.pos;
	t.mid = (l + r) * 0.5f;
	const float dx = r.x - l.x;
	const float dy = r.y - l.y;
	t.shoulderLateral = 0.5f * math::Sqrt(dx * dx + dy * dy);
	t.shoulderZ = t.mid.z;
	const NiAVObject* const leftThigh = g_frame.bone[kLegLeft + kThigh];
	const NiAVObject* const rightThigh = g_frame.bone[kLegRight + kThigh];
	t.pelvisZ = leftThigh != nullptr && rightThigh != nullptr
	                ? 0.5f * (leftThigh->worldTransform.pos.z + rightThigh->worldTransform.pos.z)
	                : t.shoulderZ - 37.0f;
	return t;
}

bool SolveArm(UInt32 side, const BodyArmsFrame& f, const TorsoKeepOut& torso) {
	ArmStats& stats = g_frame.arm[side];
	const UInt32 base = side == 0 ? kArmRight : kArmLeft;
	NiAVObject* const clavicle = g_frame.bone[base + kClavicle];
	NiAVObject* const upper = g_frame.bone[base + kUpper];
	NiAVObject* const upperTwist = g_frame.bone[base + kUpperTwist];
	NiAVObject* const fore = g_frame.bone[base + kFore];
	NiAVObject* const foreTwist = g_frame.bone[base + kForeTwist];
	NiAVObject* const hand = g_frame.bone[base + kHand];
	// The first-person hand this arm reaches: its own side's, or under the
	// mirror the other side's bone (on this side's controller), its world
	// pose carried through the bind-pose partner so the body's own hand bone
	// gets a proper rotation of its own convention (BoneMirror.h).
	const UInt32 fpSide = FirstPersonSideFor(side, f.mirrored);
	NiAVObject* const fpHand = g_fpBone[fpSide * kArmParts + kHand];
	if (clavicle == nullptr || upper == nullptr || fore == nullptr || hand == nullptr ||
	    !LooksLikeObject(clavicle->parent)) {
		stats.why = "the body's arm bones are not all there";
		return false;
	}
	if (fpHand == nullptr) {
		stats.why = "no first-person hand bone to reach";
		return false;
	}
	const NiTransform targetWorld =
		f.mirrored ? PartnerWorld(fpHand->worldTransform, g_partners.partners.toOther[fpSide][kHand])
	               : fpHand->worldTransform;
	const NiMatrix33 targetRot = targetWorld.rot;
	const NiPoint3 target = targetWorld.pos;
	const float sideSign = side == 0 ? 1.0f : -1.0f;
	const NiPoint3 right{math::Cos(g_frame.bodyYaw), math::Sin(g_frame.bodyYaw), 0.0f};
	const NiPoint3 forward{-right.y, right.x, 0.0f};
	// The animation's bone lengths (the bones were put back to its pose).
	const float upperUnits = ik::Length(fore->worldTransform.pos - upper->worldTransform.pos);
	const float foreUnits = ik::Length(hand->worldTransform.pos - fore->worldTransform.pos);
	if (!(upperUnits > 1.0f) || !(foreUnits > 1.0f)) {
		stats.why = "the arm's bones read no length";
		return false;
	}
	const NiAVObject* const skeletonRoot = g_frame.bone[kRoot];
	if (!ArmBonesReadLikeAnArm(upperUnits, foreUnits,
	                           skeletonRoot != nullptr ? skeletonRoot->worldTransform.scale : 1.0f)) {
		stats.why = "the arm's bones read longer than any arm's - nothing is laid onto them";
		return false;
	}
	// The clavicle towards a far or high hand; the shoulder joint goes with it.
	const ClavicleTurn turn = TurnClavicle(clavicle->worldTransform.pos, ik::Column(clavicle->worldTransform.rot, 0),
	                                       upper->worldTransform.pos, target, upperUnits + foreUnits, kUp);
	const NiMatrix33 clavicleRot =
		turn.angle > 0.0f ? ik::TurnAbout(turn.axis, turn.angle) * clavicle->worldTransform.rot : clavicle->worldTransform.rot;
	const NiPoint3 shoulder =
		clavicle->worldTransform.pos + clavicleRot * (upper->localTransform.pos * clavicle->worldTransform.scale);
	const NiAVObject* const clavicleParent = clavicle->parent;
	// The two bones.
	ArmMemory& memory = g_armMemory[side];
	LimbInput in;
	in.root = shoulder;
	in.upperLength = upperUnits;
	in.lowerLength = foreUnits;
	in.target = target;
	// The elbow's direction: guessed, eased from last frame's (BodyIkLogic.h,
	// EasedDirection), and never in the torso (ElbowClearOfTorso) - the way
	// round it went last frame kept.
	const NiPoint3 wantedPole =
		ElbowPole(sideSign, right, forward, kUp, shoulder, target, ik::Column(targetRot, 0), foreUnits);
	const NiPoint3 easedPole =
		memory.valid ? EasedDirection(memory.pole, wantedPole, EaseShare(f.dtSeconds, kElbowEaseSeconds)) : wantedPole;
	in.pole = easedPole;
	const ElbowClear clear = ElbowClearOfTorso(in, sideSign, torso, memory.sideSign);
	in.pole = clear.pole;
	stats.elbowMoved = clear.moved;
	in.maxStretch = f.ik.armStretch;
	const LimbSolve s = SolveLimb(in);
	if (!s.valid) {
		stats.why = "no solve (the hand on the shoulder, or a number that is none)";
		return false;
	}
	// The twists: half the upper arm's roll taken back by its twist bone; the
	// hand's roll on the forearm spread over the forearm, its twist bone and
	// the hand (ForearmRollsFor) - each continued the short way from last
	// frame's (UnwrapAngle), so a roll past a half turn does not flip a twist
	// bone round, and the hand's roll followed up to three quarters of a turn
	// either way (HoldForeRollMemory).
	const float upperRollRaw = TwistAboutX(InverseMatrix(upper->worldTransform.rot) * s.upperRot);
	const float upperRoll = memory.valid ? UnwrapAngle(memory.upperRoll, upperRollRaw) : upperRollRaw;
	const float foreRollRaw = math::WrapAngle(TwistAboutX(ik::Transposed(s.lowerRot) * targetRot) -
	                                          TwistAboutX(hand->localTransform.rot));
	const float foreRoll = memory.valid ? UnwrapAngle(memory.foreRoll, foreRollRaw) : foreRollRaw;
	memory.valid = true;
	memory.pole = easedPole;
	memory.sideSign = clear.moved ? clear.sign : 0.0f;
	memory.upperRoll = HoldRollMemory(upperRoll);
	memory.foreRoll = HoldForeRollMemory(foreRoll);
	const float upperHalf = HalfOfRoll(upperRoll);
	const ForearmRolls rolls = ForearmRollsFor(foreRoll);
	const NiMatrix33 foreRot = s.lowerRot * TurnAboutX(rolls.forearm);
	stats.handRollDegrees = foreRoll * math::kRadiansToDegrees;
	// Written, parent first.
	NiTransform clavicleLocal = clavicle->localTransform;
	clavicleLocal.rot = InverseMatrix(clavicleParent->worldTransform.rot) * clavicleRot;
	WriteLocal(g_body[base + kClavicle], clavicleLocal);
	WriteLocal(g_body[base + kUpper], LocalFor(upper->localTransform, clavicleRot, clavicle->worldTransform.pos,
	                                           clavicle->worldTransform.scale, s.upperRot, shoulder));
	if (upperTwist != nullptr && upperTwist->parent == upper) {
		NiTransform t = upperTwist->localTransform;
		t.rot = TurnAboutX(-upperHalf) * t.rot;
		WriteLocal(g_body[base + kUpperTwist], t);
	}
	WriteLocal(g_body[base + kFore],
	           LocalFor(fore->localTransform, s.upperRot, shoulder, upper->worldTransform.scale, foreRot, s.joint));
	if (foreTwist != nullptr && foreTwist->parent == fore) {
		// Its local turn the difference: on the rolled forearm, its own roll
		// in the world comes out at its share.
		NiTransform t = foreTwist->localTransform;
		t.rot = TurnAboutX(rolls.twist - rolls.forearm) * t.rot;
		WriteLocal(g_body[base + kForeTwist], t);
	}
	WriteLocal(g_body[base + kHand],
	           LocalFor(hand->localTransform, foreRot, s.joint, fore->worldTransform.scale, targetRot, s.end));
	UpdateNodeTransforms(clavicle);
	// The first-person hand stays where its pin put it (under the mirror that
	// is not the body hand's own pose - the other bone's, turned over).
	AlignFirstPersonArm(side, fpHand->worldTransform.rot, fpHand->worldTransform.pos, f.mirrored);

	stats.follows = true;
	stats.reached = s.reached;
	stats.shortUnits = s.shortUnits;
	stats.stretch = s.stretch;
	stats.clavicleDegrees = turn.angle * math::kRadiansToDegrees;
	stats.upperUnits = upperUnits;
	stats.foreUnits = foreUnits;
	stats.shoulder = shoulder;
	stats.elbow = s.joint;
	stats.wrist = hand->worldTransform.pos;
	stats.target = target;
	stats.upperRollDegrees = upperRoll * math::kRadiansToDegrees;
	stats.foreTwistDegrees = rolls.twist * math::kRadiansToDegrees;
	stats.forearmRollDegrees = rolls.forearm * math::kRadiansToDegrees;
	stats.handAxis = ik::Column(targetRot, 0);
	stats.pole = in.pole;
	stats.reachRatio = ik::Length(target - shoulder) / (upperUnits + foreUnits);
	NiPoint3 line;
	stats.bendDegrees = ik::Unit(target - shoulder, line, 1e-3f)
	                        ? ik::Acos(ik::Dot(line, ik::Column(s.upperRot, 0))) * math::kRadiansToDegrees
	                        : 0.0f;
	if (!g_armReported[side]) {
		g_armReported[side] = true;
		OBVR_LOG("Body IK: the %s arm follows its hand - the body's upper arm %.1f and forearm %.1f units, the "
		         "clavicle turned %.1f degrees towards it, the elbow at %.1f %.1f %.1f, the wrist at %.1f %.1f %.1f "
		         "for the hand at %.1f %.1f %.1f (%s, grown x%.2f); the first-person arm laid onto it",
		         side == 0 ? "right" : "left", static_cast<double>(upperUnits), static_cast<double>(foreUnits),
		         static_cast<double>(stats.clavicleDegrees), static_cast<double>(s.joint.x),
		         static_cast<double>(s.joint.y), static_cast<double>(s.joint.z), static_cast<double>(stats.wrist.x),
		         static_cast<double>(stats.wrist.y), static_cast<double>(stats.wrist.z), static_cast<double>(target.x),
		         static_cast<double>(target.y), static_cast<double>(target.z), s.reached ? "reached" : "short of it",
		         static_cast<double>(s.stretch));
		// The rig as the solver takes it: the forearm turned about z only
		// under the upper arm, folding towards -y.
		const NiMatrix33 local = fore->localTransform.rot;
		OBVR_LOG("Body IK: the %s forearm under its upper arm now - rows [%.3f %.3f %.3f] [%.3f %.3f %.3f] "
		         "[%.3f %.3f %.3f] (a turn about z alone, %.1f degrees)",
		         side == 0 ? "right" : "left", static_cast<double>(local.data[0][0]), static_cast<double>(local.data[0][1]),
		         static_cast<double>(local.data[0][2]), static_cast<double>(local.data[1][0]),
		         static_cast<double>(local.data[1][1]), static_cast<double>(local.data[1][2]),
		         static_cast<double>(local.data[2][0]), static_cast<double>(local.data[2][1]),
		         static_cast<double>(local.data[2][2]),
		         static_cast<double>(math::Atan2(local.data[1][0], local.data[0][0]) * math::kRadiansToDegrees));
	}
	return true;
}

void LogArm(const char* sideName, const ArmStats& a, char* out, UInt32 size) {
	if (!a.follows) {
		std::snprintf(out, size, "the %s arm left to the body (%s)", sideName, a.why);
		return;
	}
	const float off = ik::Length(a.wrist - a.target);
	if (a.reached) {
		std::snprintf(out, size,
		              "the %s arm reaches its hand (the wrist %.2f units off it, grown x%.2f) - shoulder %.1f %.1f %.1f, "
		              "elbow %.1f %.1f %.1f, hand %.1f %.1f %.1f (its axis %.2f %.2f %.2f, the elbow sent %.2f %.2f %.2f%s), "
		              "the clavicle turned %.1f, the upper arm rolled %.1f (its twist bone half), the hand rolled %.1f on "
		              "the forearm - the forearm rolled %.1f, its twist %.1f degrees; the first-person arm on it "
		              "within %.2f units, %.1f degrees; %u thing(s) on its forearm carried with the hand, %.1f units "
		              "off where the forearm would have them",
		              sideName, static_cast<double>(off), static_cast<double>(a.stretch),
		              static_cast<double>(a.shoulder.x), static_cast<double>(a.shoulder.y),
		              static_cast<double>(a.shoulder.z), static_cast<double>(a.elbow.x), static_cast<double>(a.elbow.y),
		              static_cast<double>(a.elbow.z), static_cast<double>(a.target.x), static_cast<double>(a.target.y),
		              static_cast<double>(a.target.z), static_cast<double>(a.handAxis.x),
		              static_cast<double>(a.handAxis.y), static_cast<double>(a.handAxis.z), static_cast<double>(a.pole.x),
		              static_cast<double>(a.pole.y), static_cast<double>(a.pole.z),
		              a.elbowMoved ? ", moved out of the torso" : "", static_cast<double>(a.clavicleDegrees),
		              static_cast<double>(a.upperRollDegrees), static_cast<double>(a.handRollDegrees),
		              static_cast<double>(a.forearmRollDegrees), static_cast<double>(a.foreTwistDegrees),
		              static_cast<double>(a.fpOffUnits),
		              static_cast<double>(a.fpOffDegrees), a.carried, static_cast<double>(a.carriedOffUnits));
	} else {
		std::snprintf(out, size,
		              "the %s arm falls short of its hand by %.1f units (grown x%.2f) - shoulder %.1f %.1f %.1f, elbow "
		              "%.1f %.1f %.1f, hand %.1f %.1f %.1f, the clavicle turned %.1f",
		              sideName, static_cast<double>(a.shortUnits), static_cast<double>(a.stretch),
		              static_cast<double>(a.shoulder.x), static_cast<double>(a.shoulder.y),
		              static_cast<double>(a.shoulder.z), static_cast<double>(a.elbow.x), static_cast<double>(a.elbow.y),
		              static_cast<double>(a.elbow.z), static_cast<double>(a.target.x), static_cast<double>(a.target.y),
		              static_cast<double>(a.target.z), static_cast<double>(a.clavicleDegrees));
	}
}

}  // namespace

bool BeginBodyIk(NiAVObject* root) {
	g_frame = Frame{};
	if (!LooksLikeObject(root)) {
		return false;
	}
	g_frame.root = root;
	const UInt32 found = FindNodesNamed(root, kBodyNames, kBodyBones, g_frame.bone);
	if (!g_bonesReported) {
		g_bonesReported = true;
		char missing[192] = "";
		UInt32 at = 0;
		for (UInt32 i = 0; i < kBodyBones && at < sizeof(missing) - 32; ++i) {
			if (g_frame.bone[i] == nullptr) {
				for (UInt32 k = 0; kBodyNames[i][k] != '\0' && at < sizeof(missing) - 4; ++k) {
					missing[at++] = kBodyNames[i][k];
				}
				missing[at++] = ';';
				missing[at++] = ' ';
				missing[at] = '\0';
			}
		}
		OBVR_LOG("Body IK: the third-person skeleton - %u of %u bones found%s%s", found, kBodyBones,
		         found < kBodyBones ? ", missing: " : "", missing);
	}
	UInt32 kept = 0;
	UInt32 renewed = 0;
	bool restored = false;
	bool restoredBone[kBodyBones] = {};
	for (UInt32 i = 0; i < kBodyBones; ++i) {
		restoredBone[i] = BeginWrite(g_body[i], g_frame.bone[i], kept, renewed);
		restored = restoredBone[i] || restored;
	}
	ReportKept("body", kept, renewed, restoredBone, g_frame.bone, kBodyBones, g_keptReported);
	if (restored && g_frame.bone[kRoot] != nullptr) {
		UpdateNodeTransforms(g_frame.bone[kRoot]);
	}
	g_frame.valid = g_frame.bone[kRoot] != nullptr;
	if (g_frame.valid) {
		g_frame.bodyYaw = YawOfRotation(g_frame.bone[kRoot]->worldTransform.rot, 0.0f);
	}
	return g_frame.valid;
}

bool ReadBodyArmUnits(UInt32 side, float& upperUnits, float& foreUnits) {
	if (!g_frame.valid || side > 1) {
		return false;
	}
	const UInt32 base = side == 0 ? kArmRight : kArmLeft;
	const NiAVObject* const upper = g_frame.bone[base + kUpper];
	const NiAVObject* const fore = g_frame.bone[base + kFore];
	const NiAVObject* const hand = g_frame.bone[base + kHand];
	if (upper == nullptr || fore == nullptr || hand == nullptr) {
		return false;
	}
	upperUnits = ik::Length(fore->worldTransform.pos - upper->worldTransform.pos);
	foreUnits = ik::Length(hand->worldTransform.pos - fore->worldTransform.pos);
	return true;
}

void StandDownBodyIk() { g_frame.valid = false; }

// Where the animation put the feet this frame, turned with the body - noted
// before the trunk is held and the body moved, for the legs.
void NoteAnimatedFeet() {
	if (g_frame.bone[kRoot] != nullptr) {
		g_frame.rootAnimated = g_frame.bone[kRoot]->worldTransform.pos;
	}
	for (UInt32 side = 0; side < 2; ++side) {
		NiAVObject* const foot = g_frame.bone[(side == 0 ? kLegRight : kLegLeft) + kFoot];
		g_frame.footValid[side] = foot != nullptr;
		if (foot != nullptr) {
			g_frame.footAnimated[side] = foot->worldTransform.pos;
		}
	}
}

// The trunk's pose taken, if the player stands idle and the chain reads as
// a standing one; held at it once taken.
void StepTrunk(bool standsIdle, float dtSeconds) {
	NiAVObject* const skeleton = g_frame.bone[kRoot];
	NiAVObject* const head = g_frame.bone[kHead];
	NiAVObject* const top = g_frame.bone[kTrunkFirst];
	if (skeleton == nullptr || head == nullptr || top == nullptr) {
		return;
	}
	for (UInt32 i = 0; i < kTrunkBones; ++i) {
		if (g_frame.bone[kTrunkFirst + i] == nullptr) {
			return;  // a chain with a bone missing is left to the animation
		}
	}
	if (g_trunk.valid && g_trunk.root != g_frame.root) {
		g_trunk = TrunkReference{};  // a new skeleton: its own pose
		g_trunkReported = false;
	}
	if (!g_trunk.valid) {
		const bool settled = StandingSettled(g_standingSeconds, standsIdle, dtSeconds);
		const float scale = skeleton->worldTransform.scale > 0.0f ? skeleton->worldTransform.scale : 1.0f;
		const NiPoint3 headFromRoot =
			(InverseMatrix(skeleton->worldTransform.rot) * (head->worldTransform.pos - skeleton->worldTransform.pos)) *
			(1.0f / scale);
		if (!settled || !HeadOffsetStands(headFromRoot)) {
			return;
		}
		g_trunk.valid = true;
		g_trunk.root = g_frame.root;
		for (UInt32 i = 0; i < kTrunkBones; ++i) {
			g_trunk.local[i] = g_frame.bone[kTrunkFirst + i]->localTransform;
		}
		if (!g_trunkReported) {
			g_trunkReported = true;
			OBVR_LOG("Body IK: the trunk's pose taken from the standing idle after %.1f s - the head %.1f %.1f %.1f "
			         "from the root; held there from now on, so no animation moves the body under the headset",
			         static_cast<double>(g_standingSeconds), static_cast<double>(headFromRoot.x),
			         static_cast<double>(headFromRoot.y), static_cast<double>(headFromRoot.z));
		}
	}
	for (UInt32 i = 0; i < kTrunkBones; ++i) {
		WriteLocal(g_body[kTrunkFirst + i], g_trunk.local[i]);
	}
	UpdateNodeTransforms(top);
	g_frame.trunkHeld = true;
}

float StepBodyTorso(const BodyIkSettings& s, const BodyTorsoFrame& t) {
	NiAVObject* const skeleton = g_frame.bone[kRoot];
	if (!g_frame.valid || skeleton == nullptr) {
		return 0.0f;
	}
	const float rootYaw = YawOfRotation(skeleton->worldTransform.rot, 0.0f);
	if (!s.torso || !t.headValid || !LooksLikeObject(skeleton->parent)) {
		g_torso = TorsoYawState{};
		g_standingSeconds = 0.0f;
		g_frame.bodyYaw = rootYaw;
		NoteAnimatedFeet();
		return rootYaw;
	}
	const float offHead = StepTorsoYaw(g_torso, t.headTrackingYaw, t.dtSeconds, s.torsoYaw);
	const float wanted = math::WrapAngle(t.headWorldYaw + offHead);
	const float turn = math::WrapAngle(wanted - rootYaw);
	NiTransform local = skeleton->localTransform;
	// The true inverse of the parent's matrix: an Altmer's carries the race's
	// height, and a transpose of that squared the scale into the root and every
	// bone below it (2026-10-16).
	local.rot = InverseMatrix(skeleton->parent->worldTransform.rot) * (ik::TurnAbout(kUp, turn) * skeleton->worldTransform.rot);
	WriteLocal(g_body[kRoot], local);
	UpdateNodeTransforms(skeleton);
	g_frame.bodyYaw = wanted;
	g_frame.torsoTurned = true;
	g_frame.torsoOffHead = offHead;
	// The feet as the animation put them, then the trunk held steady.
	NoteAnimatedFeet();
	StepTrunk(t.standsIdle, t.dtSeconds);
	// The chest's own twist out, a third on each spine bone.
	NiAVObject* const leftShoulder = g_frame.bone[kArmLeft + kUpper];
	NiAVObject* const rightShoulder = g_frame.bone[kArmRight + kUpper];
	float chestYaw = 0.0f;
	if (leftShoulder != nullptr && rightShoulder != nullptr &&
	    ChestYaw(leftShoulder->worldTransform.pos, rightShoulder->worldTransform.pos, chestYaw)) {
		const float correction = ChestCorrection(wanted, chestYaw);
		const UInt32 spines[3] = {kSpine, kSpine1, kSpine2};
		UInt32 present = 0;
		for (UInt32 b : spines) {
			present += g_frame.bone[b] != nullptr && LooksLikeObject(g_frame.bone[b]->parent) ? 1u : 0u;
		}
		if (present > 0 && correction != 0.0f) {
			const float share = correction / static_cast<float>(present);
			for (UInt32 b : spines) {
				NiAVObject* const bone = g_frame.bone[b];
				if (bone == nullptr || !LooksLikeObject(bone->parent)) {
					continue;
				}
				NiTransform spine = bone->localTransform;
				spine.rot =
					InverseMatrix(bone->parent->worldTransform.rot) * (ik::TurnAbout(kUp, share) * bone->worldTransform.rot);
				WriteLocal(g_body[b], spine);
				UpdateNodeTransforms(bone);
			}
		}
		g_frame.chestCorrection = correction;
	}
	if (!g_torsoReported) {
		g_torsoReported = true;
		OBVR_LOG("Body IK: the body turns with the head - %.1f degrees off it now (free %.0f either way, followed "
		         "over %.1f s); the chest's own twist of %.1f degrees taken out over the spine",
		         static_cast<double>(offHead * math::kRadiansToDegrees),
		         static_cast<double>(s.torsoYaw.freeRadians * math::kRadiansToDegrees),
		         static_cast<double>(s.torsoYaw.followSeconds),
		         static_cast<double>(g_frame.chestCorrection * math::kRadiansToDegrees));
	}
	return wanted;
}

void WriteBodyRootPosition(const NiPoint3& localPos) {
	NiAVObject* const skeleton = g_frame.bone[kRoot];
	if (!g_frame.valid || skeleton == nullptr) {
		return;
	}
	NiTransform local = skeleton->localTransform;
	local.pos = localPos;
	WriteLocal(g_body[kRoot], local);
}

float StepBodyLegs(const BodyIkSettings& s, float loweredUnits) {
	g_frame.lowered = loweredUnits;
	g_frame.feetLift = 0.0f;
	if (!g_frame.valid || !s.legs) {
		return 0.0f;
	}
	const NiPoint3 right{math::Cos(g_frame.bodyYaw), math::Sin(g_frame.bodyYaw), 0.0f};
	const NiPoint3 forward{-right.y, right.x, 0.0f};
	NiAVObject* const root = g_frame.bone[kRoot];
	if (root == nullptr) {
		return 0.0f;
	}
	// The body's move across the ground since the feet were noted (the
	// placement; the let-down is not carried: the feet stay on the ground).
	NiPoint3 carried = root->worldTransform.pos - g_frame.rootAnimated;
	carried.z = 0.0f;
	UInt32 legs = 0;
	float lift = 0.0f;
	for (UInt32 side = 0; side < 2; ++side) {
		const UInt32 base = side == 0 ? kLegRight : kLegLeft;
		NiAVObject* const thigh = g_frame.bone[base + kThigh];
		NiAVObject* const calf = g_frame.bone[base + kCalf];
		NiAVObject* const foot = g_frame.bone[base + kFoot];
		if (thigh == nullptr || calf == nullptr || foot == nullptr || !LooksLikeObject(thigh->parent) ||
		    !g_frame.footValid[side]) {
			continue;
		}
		// Back to where the animation put the foot, carried with the body
		// across the ground (FootSolveWanted): the body let down puts it in
		// the floor, the trunk held higher than the animation's leaves it
		// floating, the trunk's lean held back leaves it forward.
		const NiPoint3 target = g_frame.footAnimated[side] + carried;
		const NiPoint3 delta = target - foot->worldTransform.pos;
		g_frame.footDelta[side] = delta.z;
		if (!FootSolveWanted(delta)) {
			continue;
		}
		g_frame.footSolved[side] = true;
		lift = delta.z > lift ? delta.z : lift;
		LimbInput in;
		in.root = thigh->worldTransform.pos;
		in.upperLength = ik::Length(calf->worldTransform.pos - thigh->worldTransform.pos);
		in.lowerLength = ik::Length(foot->worldTransform.pos - calf->worldTransform.pos);
		in.target = target;
		in.pole = KneePole(side == 0 ? 1.0f : -1.0f, right, forward);
		in.softShare = 0.0f;  // the animation's own feet: solved exactly
		const LimbSolve leg = SolveLimb(in);
		if (!leg.valid) {
			continue;
		}
		const NiMatrix33 footRot = foot->worldTransform.rot;  // the foot keeps its angle to the ground
		NiTransform t = thigh->localTransform;
		t.rot = InverseMatrix(thigh->parent->worldTransform.rot) * leg.upperRot;
		WriteLocal(g_body[base + kThigh], t);
		WriteLocal(g_body[base + kCalf], LocalFor(calf->localTransform, leg.upperRot, thigh->worldTransform.pos,
		                                          thigh->worldTransform.scale, leg.lowerRot, leg.joint));
		WriteLocal(g_body[base + kFoot],
		           LocalFor(foot->localTransform, leg.lowerRot, leg.joint, calf->worldTransform.scale, footRot, leg.end));
		UpdateNodeTransforms(thigh);
		++legs;
	}
	g_frame.feetLift = lift;
	if (!g_legsReported && legs > 0 && lift > 0.0f) {
		g_legsReported = true;
		OBVR_LOG("Body IK: the body let down %.1f units to put its eyes at the headset - the feet put back %.1f up to "
		         "where the animation had them, %u knee(s) bent",
		         static_cast<double>(loweredUnits), static_cast<double>(lift), legs);
	}
	return lift;
}

UInt32 StepBodyArms(const BodyArmsFrame& f) {
	if (!g_frame.valid || g_frame.root == nullptr) {
		return 0;
	}
	const TorsoKeepOut torso = TorsoOfFrame();
	UInt32 following = 0;
	for (UInt32 side = 0; side < 2; ++side) {
		ArmStats& stats = g_frame.arm[side];
		// The hand this arm reaches: under the mirror the other side's bone.
		const bool pinned = FirstPersonSideFor(side, f.mirrored) == 0 ? f.rightPinned : f.leftPinned;
		bool follows = ArmFollowsHand(f.ik.arms, true, pinned, f.rolesSwapped, f.mirrored);
		if (follows && f.mirrored && !ArmPartnersReady()) {
			follows = false;
			stats.why = "the mirror's bone partners did not read from the body's skins";
		} else if (!follows) {
			stats.why = !f.ik.arms         ? "the arms part off"
			            : f.rolesSwapped   ? "left-handed without the mirror: the hands on each other's controllers"
			                               : "its hand not pinned this frame";
			if (f.ik.arms && f.rolesSwapped && !g_swappedReported) {
				g_swappedReported = true;
				OBVR_LOG("Body IK: left-handed play without the mirror puts the right hand's bone on the left "
				         "controller - the body's arms are left to the body (HideArms) rather than reach across the "
				         "chest");
			}
		} else {
			follows = SolveArm(side, f, torso);
		}
		if (follows) {
			following |= 1u << side;
		} else {
			g_armMemory[side] = ArmMemory{};
			if (!g_whyReported[side] && f.ik.arms) {
				g_whyReported[side] = true;
				OBVR_LOG("Body IK: the %s arm left to the body - %s", side == 0 ? "right" : "left", stats.why);
			}
			NiAVObject* const clavicle = g_frame.bone[(side == 0 ? kArmRight : kArmLeft) + kClavicle];
			if (f.hideArms && clavicle != nullptr) {
				CollapseForThisDraw(clavicle);
			}
		}
	}
	if (following != 0) {
		StepHides(g_frame.root);
		// The arms carried out of the bound the engine worked out from the
		// animation: kept in view round the camera, the way the first-person
		// hands are (KeepFirstPersonNodesInView) - arm's reach and the body
		// under it, 130 units.
		KeepNodesInView(g_frame.root, "UpperBody, Arms", f.cameraWorld, 130.0f);
	} else {
		ReleaseHides(g_frame.root);
	}
	// The first seconds after a load, the arms as at a mark (StepStartTrace).
	if (following != 0 && StepStartTrace(g_startTrace, f.dtSeconds)) {
		char arm[1024];
		for (UInt32 side = 0; side < 2; ++side) {
			LogArm(side == 0 ? "right" : "left", g_frame.arm[side], arm, sizeof(arm));
			OBVR_LOG("Body IK: %.2f s after the load - %s", static_cast<double>(g_startTrace.elapsed), arm);
		}
	}
	if (f.logState) {
		// Three lines: a log line holds 1024 characters (core/Log.cpp).
		OBVR_LOG("Body IK: at the mark - the body %s (%.1f degrees off the head, the chest's twist %.1f taken out), "
		         "the trunk %s, let down %.1f units, the feet lifted %.1f; %u node(s) hidden",
		         g_frame.torsoTurned ? "turned with the head" : "as the game turns it",
		         static_cast<double>(g_frame.torsoOffHead * math::kRadiansToDegrees),
		         static_cast<double>(g_frame.chestCorrection * math::kRadiansToDegrees),
		         g_frame.trunkHeld ? "held at the standing pose" : "the animation's",
		         static_cast<double>(g_frame.lowered), static_cast<double>(g_frame.feetLift), g_hiddenCount);
		char arm[1024];
		LogArm("right", g_frame.arm[0], arm, sizeof(arm));
		OBVR_LOG("Body IK: at the mark - %s", arm);
		LogArm("left", g_frame.arm[1], arm, sizeof(arm));
		OBVR_LOG("Body IK: at the mark - %s", arm);
	}
	if (f.trace) {
		const NiPoint3 root = g_frame.bone[kRoot] != nullptr ? g_frame.bone[kRoot]->worldTransform.pos
		                                                     : NiPoint3{0.0f, 0.0f, 0.0f};
		char arms[2][160];
		for (UInt32 side = 0; side < 2; ++side) {
			const ArmStats& a = g_frame.arm[side];
			std::snprintf(arms[side], sizeof(arms[side]),
			              "%s f%d d%.3f bend %.2f pole %.3f %.3f %.3f mv%d clav %.2f off %.2f", side == 0 ? "R" : "L",
			              a.follows ? 1 : 0, static_cast<double>(a.reachRatio), static_cast<double>(a.bendDegrees),
			              static_cast<double>(a.pole.x), static_cast<double>(a.pole.y), static_cast<double>(a.pole.z),
			              a.elbowMoved ? 1 : 0, static_cast<double>(a.clavicleDegrees),
			              static_cast<double>(ik::Length(a.wrist - a.target)));
		}
		OBVR_LOG("Body trace: f=%u cam %.2f %.2f %.2f root %.2f %.2f %.2f par %.2f %.2f %.2f headoff %.2f %.2f %.2f "
		         "used %.2f %.2f %.2f low %.2f lift %.2f yaw %.2f chest %.2f held %u/%u fp %u/%u trunk %d feet %.2f%s "
		         "%.2f%s | %s | %s",
		         f.traceFrame, static_cast<double>(f.cameraWorld.x), static_cast<double>(f.cameraWorld.y),
		         static_cast<double>(f.cameraWorld.z), static_cast<double>(root.x), static_cast<double>(root.y),
		         static_cast<double>(root.z), static_cast<double>(g_placement.parentWorld.x),
		         static_cast<double>(g_placement.parentWorld.y), static_cast<double>(g_placement.parentWorld.z),
		         static_cast<double>(g_placement.headOffsetRaw.x), static_cast<double>(g_placement.headOffsetRaw.y),
		         static_cast<double>(g_placement.headOffsetRaw.z), static_cast<double>(g_placement.headOffsetUsed.x),
		         static_cast<double>(g_placement.headOffsetUsed.y), static_cast<double>(g_placement.headOffsetUsed.z),
		         static_cast<double>(g_placement.lowered), static_cast<double>(g_frame.feetLift),
		         static_cast<double>(g_frame.torsoOffHead * math::kRadiansToDegrees),
		         static_cast<double>(g_frame.chestCorrection * math::kRadiansToDegrees), g_heldThroughDraw,
		         g_writtenThroughDraw, g_fpHeldThroughDraw, g_fpWrittenThroughDraw, g_frame.trunkHeld ? 1 : 0,
		         static_cast<double>(g_frame.footDelta[0]), g_frame.footSolved[0] ? "s" : "",
		         static_cast<double>(g_frame.footDelta[1]), g_frame.footSolved[1] ? "s" : "", arms[0], arms[1]);
	}
	g_frame.valid = false;  // the frame's bones are not trusted past it
	return following;
}

void NoteBodyPlacement(const BodyPlacementTrace& t) { g_placement = t; }

void NoteBodyRenderDone() {
	g_heldThroughDraw = 0;
	g_writtenThroughDraw = 0;
	for (const BoneWrite& w : g_body) {
		if (w.held && w.node != nullptr) {
			++g_writtenThroughDraw;
			g_heldThroughDraw += SameTransform(w.node->localTransform, w.wrote) ? 1u : 0u;
		}
	}
	g_fpHeldThroughDraw = 0;
	g_fpWrittenThroughDraw = 0;
	for (const BoneWrite& w : g_fp) {
		if (w.held && w.node != nullptr) {
			++g_fpWrittenThroughDraw;
			g_fpHeldThroughDraw += SameTransform(w.node->localTransform, w.wrote) ? 1u : 0u;
		}
	}
}

// ------------------------------------------------------------ The sheaths

enum : UInt32 { kSheathSide = 0, kSheathBack, kSheathQuiver, kSheathNodes };
const char* const kSheathNames[kSheathNodes] = {"SideWeapon", "BackWeapon", "Quiver"};
BoneWrite g_sheath[kSheathNodes];
bool g_sheathMirroredReported[kSheathNodes] = {};
bool g_sheathRefusedReported[kSheathNodes] = {};
constexpr float kSheathAxisSlackDegrees = 30.0f;

// The right and the left bone a sheath's parent is judged against: the
// thighs for the pelvis, the clavicles for Spine2. False for any other
// parent.
bool SheathParentSides(const NiAVObject* parent, NiPoint3& right, NiPoint3& left) {
	const NiAVObject* r = nullptr;
	const NiAVObject* l = nullptr;
	if (parent != nullptr && parent == g_frame.bone[kPelvis]) {
		r = g_frame.bone[kLegRight + kThigh];
		l = g_frame.bone[kLegLeft + kThigh];
	} else if (parent != nullptr && parent == g_frame.bone[kSpine2]) {
		r = g_frame.bone[kArmRight + kClavicle];
		l = g_frame.bone[kArmLeft + kClavicle];
	}
	if (r == nullptr || l == nullptr) {
		return false;
	}
	right = r->worldTransform.pos;
	left = l->worldTransform.pos;
	return true;
}

// How far a local position under `parent` lies to the body's right, units.
float SheathRightOf(const NiAVObject* parent, const NiPoint3& localPos, const NiPoint3& right, const NiPoint3& left) {
	const NiPoint3 across = right - left;
	const float length = math::Sqrt(across.LengthSquared());
	if (!(length > 1.0e-4f)) {
		return 0.0f;
	}
	const NiPoint3 d = parent->worldTransform.rot * (localPos * parent->worldTransform.scale);
	return (d.x * across.x + d.y * across.y + d.z * across.z) / length;
}

void StepBodySheaths(bool side, bool back, bool quiver) {
	if (!g_frame.valid || !LooksLikeObject(g_frame.root)) {
		return;
	}
	NiAVObject* nodes[kSheathNodes] = {};
	FindNodesNamed(g_frame.root, kSheathNames, kSheathNodes, nodes);
	const bool want[kSheathNodes] = {side, back, quiver};
	for (UInt32 i = 0; i < kSheathNodes; ++i) {
		UInt32 kept = 0;
		UInt32 renewed = 0;
		const bool restored = BeginWrite(g_sheath[i], nodes[i], kept, renewed);
		NiAVObject* const node = nodes[i];
		if (node == nullptr) {
			continue;
		}
		bool wrote = false;
		if (want[i]) {
			NiPoint3 right{};
			NiPoint3 left{};
			NiAVObject* const parent = node->parent;
			const bool sides = LooksLikeObject(parent) && SheathParentSides(parent, right, left);
			if (sides && LateralAxisAgrees(parent->worldTransform.rot, right, left, kSheathAxisSlackDegrees)) {
				const NiTransform mirrored = MirroredAcrossLocalZ(g_sheath[i].base);
				WriteLocal(g_sheath[i], mirrored);
				wrote = true;
				if (!g_sheathMirroredReported[i]) {
					g_sheathMirroredReported[i] = true;
					const float before = SheathRightOf(parent, g_sheath[i].base.pos, right, left);
					const float after = SheathRightOf(parent, mirrored.pos, right, left);
					OBVR_LOG("Body: the sheath \"%s\" mirrored for the left hand - under \"%s\" it sat %.1f units to "
					         "the body's %s, now %.1f to its %s",
					         kSheathNames[i], SafeName(parent), static_cast<double>(before < 0.0f ? -before : before),
					         before < 0.0f ? "left" : "right", static_cast<double>(after < 0.0f ? -after : after),
					         after < 0.0f ? "left" : "right");
				}
			} else if (!g_sheathRefusedReported[i]) {
				g_sheathRefusedReported[i] = true;
				OBVR_LOG("Body: the sheath \"%s\" left where the game hangs it - %s", kSheathNames[i],
				         !sides ? "its parent is neither the pelvis nor Spine2, or their side bones are missing"
				                : "its parent's z is not the body's left-right (a skeleton built otherwise)");
			}
		}
		if (wrote || restored) {
			UpdateNodeTransforms(node);
		}
	}
}

bool BodyIkHolds() {
	bool held = g_hiddenCount > 0;
	for (UInt32 i = 0; i < kBodyBones && !held; ++i) {
		held = g_body[i].held;
	}
	for (UInt32 i = 0; i < kSheathNodes && !held; ++i) {
		held = g_sheath[i].held;
	}
	return held;
}

void ReleaseBodyIk(NiAVObject* rootNow) {
	g_frame = Frame{};
	g_torso = TorsoYawState{};
	g_standingSeconds = 0.0f;
	g_armMemory[0] = ArmMemory{};
	g_armMemory[1] = ArmMemory{};
	g_startTrace = StartTrace{};
	if (g_startTracesLeft > 0) {
		--g_startTracesLeft;
	} else {
		g_startTrace.elapsed = kStartTraceSeconds + 1.0f;  // traced enough
	}
	if (!BodyIkHolds()) {
		return;
	}
	NiAVObject* now[kBodyBones] = {};
	if (LooksLikeObject(rootNow)) {
		FindNodesNamed(rootNow, kBodyNames, kBodyBones, now);
	}
	bool restored = false;
	for (UInt32 i = 0; i < kBodyBones; ++i) {
		if (g_body[i].held) {
			restored = ReleaseWrite(g_body[i], now[i]) || restored;
		}
	}
	if (restored && now[kRoot] != nullptr) {
		UpdateNodeTransforms(now[kRoot]);
	}
	// The sheaths given back too, found again under the root there is now.
	NiAVObject* sheathsNow[kSheathNodes] = {};
	if (LooksLikeObject(rootNow)) {
		FindNodesNamed(rootNow, kSheathNames, kSheathNodes, sheathsNow);
	}
	for (UInt32 i = 0; i < kSheathNodes; ++i) {
		if (g_sheath[i].held && ReleaseWrite(g_sheath[i], sheathsNow[i]) && sheathsNow[i] != nullptr) {
			UpdateNodeTransforms(sheathsNow[i]);
		}
		g_sheath[i] = BoneWrite{};
	}
	ReleaseHides(rootNow);
}

void RestoreFirstPersonArmWrites(bool lookUp) {
	bool held = false;
	for (UInt32 i = 0; i < kFpBones && !held; ++i) {
		held = g_fp[i].held;
	}
	for (UInt32 side = 0; side < 2 && !held; ++side) {
		for (UInt32 k = 0; k < kCarriedMax && !held; ++k) {
			held = g_carried[side][k].held;
		}
	}
	if (!held && !lookUp) {
		for (UInt32 i = 0; i < kFpBones; ++i) {
			g_fpBone[i] = nullptr;
		}
		return;
	}
	NiAVObject* const fpRoot = FirstPersonArmsNode();
	if (fpRoot != nullptr) {
		FindNodesNamed(fpRoot, kFpNames, kFpBones, g_fpBone);
	} else {
		for (UInt32 i = 0; i < kFpBones; ++i) {
			g_fpBone[i] = nullptr;
		}
	}
	UInt32 kept = 0;
	UInt32 renewed = 0;
	bool restored = false;
	bool restoredBone[kFpBones] = {};
	for (UInt32 i = 0; i < kFpBones; ++i) {
		restoredBone[i] = BeginWrite(g_fp[i], g_fpBone[i], kept, renewed);
		restored = restoredBone[i] || restored;
	}
	ReportKept("first-person arm", kept, renewed, restoredBone, g_fpBone, kFpBones, g_fpKeptReported);
	// What was carried on a twist bone, put back where it still holds the
	// carry - and only if it still hangs there (a shield taken off is gone).
	for (UInt32 side = 0; side < 2; ++side) {
		NiAVObject* const twist = g_fpBone[side * kArmParts + kForeTwist];
		UInt32 count = 0;
		NiAVObject* const* const children = twist != nullptr ? NodeChildren(twist, count) : nullptr;
		for (UInt32 k = 0; k < kCarriedMax; ++k) {
			BoneWrite& w = g_carried[side][k];
			NiAVObject* now = nullptr;
			for (UInt32 i = 0; i < count && w.held; ++i) {
				now = children[i] == w.node ? w.node : now;
			}
			restored = ReleaseWrite(w, now) || restored;
		}
	}
	if (restored && fpRoot != nullptr) {
		UpdateNodeTransforms(fpRoot);
	}
}

}  // namespace obvr::game
