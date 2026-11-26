#include "game/Dismember.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Memory.h"
#include "game/ActorRoster.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameSound.h"
#include "game/Blood.h"
#include "game/CutJoints.h"
#include "game/CutSideLogic.h"
#include "game/CutSkeletonLogic.h"
#include "game/CutSkinSpan.h"
#include "game/DismemberCaps.h"
#include "game/FakeBone.h"
#include "game/RefFingerprint.h"
#include "game/GameTypes.h"
#include "game/GrabPhysics.h"
#include "game/RagdollHold.h"
#include "game/SkinRead.h"
#include "game/WalkersLink.h"

namespace obvr::game {
namespace {

constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kMaxBones = 128;
constexpr UInt32 kMaxDepth = 48;
constexpr float kCollapsedScale = 0.001f;

bool LooksLikeObject(UInt32 p) { return mem::LooksLikeObjectAddress(p); }
UInt32 Read(UInt32 a) { return *reinterpret_cast<const UInt32*>(a); }

// The cuts' own bones (game/FakeBone.h): with a stand-in parent each.
using FakeNode = FakeBone;

NiAVObject* AsNode(FakeNode& f) { return FakeBoneNode(f); }

void MakeFrom(FakeNode& fake, const NiAVObject* real) { MakeFakeBone(fake, real); }

struct Cut {
	UInt32 actor = 0;
	CutPart part = CutPart::Head;  // the head at the neck, or a hand at its wrist
	UInt32 root = 0;
	NiAVObject* head = nullptr;
	NiAVObject* neck = nullptr;  // the stump's bone: the head's parent (Neck1, a goblin's Neck), a limb's named one
	NiAVObject* joint = nullptr;  // the body it was joined to (Spine2, an ogre's Spine1): the bound's side, the rescans
	NiAVObject* neckTop = nullptr;   // Bip01 Neck1, or null (counted as the head's bones)
	NiAVObject* neckBase = nullptr;  // Bip01 Neck, or null
	NiAVObject* other = nullptr;  // a hand cut: the other hand (its bones stay; SwapBoneOnPartSide)
	NiPoint3 cutInNeck{0.0f, 0.0f, 0.0f};  // the head's origin in the neck's frame at the cut
	FakeNode bodySide{};  // the body's shapes' head bone: at the cut, following the neck
	FakeNode headSide{};  // the head's shapes' other bones: at the head's origin
	// The wound caps (AttachCap): the stump's bone, at the cut with the
	// head's turn in the neck's frame and its scale - not collapsed - and the
	// two caps hung under the actor's root (0: none).
	FakeNode capSide{};
	FakeNode partCapSide{};  // a hand cut: the hand's place and turn at capScale, for the hand's own cap
	float capScale = 1.0f;     // CapScaleFor(part)
	// A head's cap read as facing up into the head: turned over about its
	// centre (FlippedCap), both in the head bone's space.
	bool capFlip = false;
	NiPoint3 capCentre{0.0f, 0.0f, 0.0f};
	NiPoint3 capDown{0.0f, 0.0f, -1.0f};
	NiMatrix33 headRotInNeck{};
	float headScale = 1.0f;
	UInt32 caps[2] = {0, 0};
	float age = 0.0f;   // seconds since the cut
	UInt32 spurts = 0;  // the blood's spurts so far (BloodPulseDue)
	// One bone pointer OBVR wrote and its value: when the engine rebuilt the
	// actor's skins (a load reuses the reference and may reuse the root's
	// address - the harness, 2026-10-31 19:44), it no longer holds it.
	UInt32 checkSlot = 0;
	UInt32 checkValue = 0;
	UInt32 world = 0;   // CurrentWorld() when it was cut or last rechecked (CutWorldFor)
	UInt32 formId = 0;  // the actor's form id, for the engine's form table after a load
	float rescanSeconds = 0.0f;  // CutRescanDue: the skins read again
};
Cut g_cuts[kCutsMost];

// A bone pointer that is one of the cuts' own nodes (bodySide, headSide, the
// caps'): written by a cut, never swapped again nor counted.
bool IsCutNode(UInt32 p) {
	const UInt32 first = reinterpret_cast<UInt32>(&g_cuts[0]);
	const UInt32 end = reinterpret_cast<UInt32>(&g_cuts[0] + kCutsMost);
	return p >= first && p < end;
}

// A cut whose actor may be read: one there, made in this world (CutInWorld).
bool Live(const Cut& c) { return c.actor != 0 && CutInWorld(c.world, CurrentWorld()); }

// The heads cut that are to stay off (DismemberLogic.h CutMemory), and for
// each the 3D it is waiting on: how long it has stood, and a root already
// tried (a refusal is not tried again every frame).
CutMemory g_memory;
struct Waiting {
	UInt32 id = 0;
	UInt32 root = 0;
	float seconds = 0.0f;
	bool tried = false;
};
Waiting g_waiting[kCutMemoryMost];
UInt32 g_redoLines = 16;
// CutSettings' caps, sound and most cuts at once, as StepCuts had them last.
struct Wanted {
	bool caps = true;
	bool sound = true;
	CutLimit limit;
};
Wanted g_wanted;

// The cut's sounds at the part (CutSoundsFor), placed on the actor.
void SoundCut(UInt32 actor, const NiPoint3& at, bool redo) {
	UInt32 sounds[kCutSoundsMost] = {};
	const UInt32 n = CutSoundsFor(g_wanted.sound, redo, sounds);
	UInt32 played = 0;
	for (UInt32 i = 0; i < n; ++i) {
		played += PlaySoundFormAt(sounds[i], actor, at) ? 1 : 0;
	}
	static UInt32 s_lines = 6;
	if (n > 0 && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Dismember: the cut's sound at %.0f %.0f %.0f - %u of %u played", static_cast<double>(at.x),
		         static_cast<double>(at.y), static_cast<double>(at.z), played, n);
	}
}

using GetObjectFn = void*(__fastcall*)(void* self, void* edx, const char* name);

NiAVObject* Named(UInt32 root, const char* name) {
	const UInt32 vtable = LooksLikeObject(root) ? Read(root) : 0;
	const UInt32 fn = LooksLikeObject(vtable) ? Read(vtable + addr::kNiAVObjectGetObjectVtableOffset) : 0;
	if (fn < addr::kTextStart || fn >= addr::kTextEnd) {
		return nullptr;
	}
	auto* const node =
		static_cast<NiAVObject*>(reinterpret_cast<GetObjectFn>(fn)(reinterpret_cast<void*>(root), nullptr, name));
	return LooksLikeObject(reinterpret_cast<UInt32>(node)) ? node : nullptr;
}

// A bone's parent when it reads, else null.
NiAVObject* ParentOf(const NiAVObject* node) {
	return node != nullptr && LooksLikeObject(reinterpret_cast<UInt32>(node->parent)) ? node->parent : nullptr;
}

bool IsUnder(const NiAVObject* node, const NiAVObject* ancestor);

// A bone on the cut limb's own side: under the first of its ancestors that
// holds the part, that one not holding the other limb too - the twist bones
// beside the stump, the upper arm's twist beside the forearm (handRM held
// Bip01 L UpperArmTwist on the body at a hand cut, the harness 2026-11-11).
bool OnCutLimb(const NiAVObject* b, const Cut& c) {
	const NiAVObject* at = b;
	for (UInt32 i = 0; i < kMaxDepth && LooksLikeObject(reinterpret_cast<UInt32>(at)); ++i, at = at->parent) {
		if (IsUnder(c.head, at)) {
			return c.other == nullptr || !IsUnder(c.other, at);
		}
	}
	return false;
}

bool IsUnder(const NiAVObject* node, const NiAVObject* ancestor) {
	const NiAVObject* at = node;
	for (UInt32 i = 0; i < kMaxDepth && LooksLikeObject(reinterpret_cast<UInt32>(at)); ++i) {
		if (at == ancestor) {
			return true;
		}
		at = at->parent;
	}
	return false;
}

struct SwapCount {
	char names[320] = {};
	char parted[200] = {};  // the bones of the part's side shapes sent to the part, once each
	UInt32 shapes = 0;
	UInt32 bodyBones = 0;
	UInt32 headBones = 0;
	UInt32 leftBehind = 0;  // part-side bone pointers left on the body (PartSideBoneLeftBehind): a stretch
	PartCopies copies;      // the body's shapes drawn again for the part, hung after the walk (HangPartCopies)
};

// A bone a part's side shape sent to the part, by name, once (for the log).
void NoteParted(SwapCount& n, const NiAVObject* bone) {
	const char* const name = LooksLikeObject(reinterpret_cast<UInt32>(bone->name) & ~3u) ? bone->name : "?";
	for (const char* at = std::strstr(n.parted, name); at != nullptr; at = std::strstr(at + 1, name)) {
		const char after = at[std::strlen(name)];
		if ((at == n.parted || at[-1] == ' ') && (after == '\0' || after == ',')) {
			return;
		}
	}
	const UInt32 len = static_cast<UInt32>(std::strlen(n.parted));
	if (len + std::strlen(name) + 3 < sizeof(n.parted)) {
		std::snprintf(n.parted + len, sizeof(n.parted) - len, "%s%s", len > 0 ? ", " : "", name);
	}
}

// The other limb's bones: its match, under it, or holding it and not the
// cut one (SwapBoneOnPartSide's inOther).
bool InOther(const NiAVObject* b, const Cut& c) {
	if (c.part == CutPart::Head || c.other == nullptr) {
		return false;
	}
	return b == c.other || IsUnder(b, c.other) || (IsUnder(c.other, b) && !IsUnder(c.head, b));
}

// What a bone is to the cut (DismemberLogic.h SwapBoneOnPartSide's inputs).
struct BoneFacts {
	bool isHead = false;
	bool underHead = false;
	bool holdsPart = false;
	bool holdsOther = false;
	bool underStump = false;
	bool other = false;
};

BoneFacts FactsOf(const NiAVObject* b, const Cut& c) {
	const bool headCut = c.part == CutPart::Head;
	BoneFacts f;
	f.isHead = b == c.head;
	f.underHead = !f.isHead && IsUnder(b, c.head);
	f.holdsPart = !headCut && IsUnder(c.head, b);
	f.holdsOther = !headCut && c.other != nullptr && IsUnder(c.other, b);
	f.underStump = !headCut && OnCutLimb(b, c);
	f.other = InOther(b, c);
	return f;
}

// A shape's bone list (its NiSkinInstance's, +0x14) and its count, or null.
UInt32* BonesOfShape(UInt32 geometry, UInt32& count) {
	count = 0;
	const UInt32 skin = Read(geometry + kGeometrySkinInstance);
	const UInt32 data = LooksLikeObject(skin) ? Read(skin + kSkinInstanceData) : 0;
	const UInt32 bones = LooksLikeObject(skin) ? Read(skin + kSkinInstanceBones) : 0;
	if (!LooksLikeObject(data) || !LooksLikeObject(bones)) {
		return nullptr;
	}
	count = Read(data + kSkinDataBoneCount);
	return count == 0 || count > kMaxBones ? nullptr : reinterpret_cast<UInt32*>(bones);
}

bool ShapeHasOther(const UInt32* bone, UInt32 count, const Cut& c) {
	for (UInt32 i = 0; i < count; ++i) {
		if (LooksLikeObject(bone[i]) && InOther(reinterpret_cast<const NiAVObject*>(bone[i]), c)) {
			return true;
		}
	}
	return false;
}

// A shape a limb cut's bone rule took for the part's: whether its skin, taken
// whole to the part, would stretch (CutSkeletonLogic.h): which of its bones
// go with the part, then its weights. A cut's own node or one of another cut
// counts as staying.
SpanRead PartSideSpans(UInt32 geometry, const Cut& c, UInt32& spanning) {
	spanning = 0;
	UInt32 count = 0;
	const UInt32* const bone = BonesOfShape(geometry, count);
	if (bone == nullptr) {
		return SpanRead::Unread;
	}
	const bool hasOther = ShapeHasOther(bone, count, c);
	bool flies[kMaxBones] = {};
	bool anyFlies = false;
	bool anyStays = false;
	for (UInt32 i = 0; i < count; ++i) {
		if (LooksLikeObject(bone[i]) && !IsCutNode(bone[i])) {
			const BoneFacts f = FactsOf(reinterpret_cast<const NiAVObject*>(bone[i]), c);
			flies[i] = PartSideBoneFlies(false, f.isHead, f.underHead, f.holdsPart, f.holdsOther, f.underStump, f.other,
			                             hasOther);
		}
		anyFlies = anyFlies || flies[i];
		anyStays = anyStays || !flies[i];
	}
	if (!SpanPossible(anyFlies, anyStays)) {
		return SpanRead::None;
	}
	return ShapeSpansCut(geometry, flies, count, spanning);
}

void SwapShape(UInt32 geometry, bool headSide, Cut& c, SwapCount& n) {
	UInt32 count = 0;
	UInt32* const bone = BonesOfShape(geometry, count);
	if (bone == nullptr) {
		return;
	}
	const UInt32 bodyFake = reinterpret_cast<UInt32>(c.bodySide.bytes);
	const UInt32 headFake = reinterpret_cast<UInt32>(c.headSide.bytes);
	const bool headCut = c.part == CutPart::Head;
	const bool shapeHasOther = headSide && ShapeHasOther(bone, count, c);
	bool any = false;
	for (UInt32 i = 0; i < count; ++i) {
		const NiAVObject* const b = reinterpret_cast<const NiAVObject*>(bone[i]);
		if (c.checkSlot == 0 && (bone[i] == bodyFake || bone[i] == headFake)) {
			c.checkSlot = reinterpret_cast<UInt32>(&bone[i]);  // a cut kept through a change of clothes (CutFate::Reswap)
			c.checkValue = bone[i];
		}
		if (!LooksLikeObject(bone[i]) || bone[i] == bodyFake || bone[i] == headFake || IsCutNode(bone[i])) {
			continue;
		}
		const BoneFacts f = FactsOf(b, c);
		if (headSide && PartSideBoneLeftBehind(headCut, f.isHead, f.underHead, f.holdsPart, f.holdsOther, f.underStump,
		                                       f.other, shapeHasOther)) {
			++n.leftBehind;
		}
		if (headSide ? SwapBoneOnPartSide(headCut, f.isHead, f.underHead, f.holdsPart, f.holdsOther, f.underStump,
		                                  f.other, shapeHasOther)
		             : SwapBoneOnBodySide(f.isHead, f.underHead)) {
			if (headSide) {
				NoteParted(n, b);
			}
			bone[i] = headSide ? headFake : bodyFake;
			if (c.checkSlot == 0) {
				c.checkSlot = reinterpret_cast<UInt32>(&bone[i]);
				c.checkValue = bone[i];
			}
			++(headSide ? n.headBones : n.bodyBones);
			any = true;
		}
	}
	n.shapes += any ? 1 : 0;
}

// How many of a shape's bones are the head's - the head, what hangs under
// it, Neck and Neck1 - against all it has (both 0 when its skin is not read),
// and whether it holds the cut's own part or stump node: swapped before
// (CutSideLogic.h ShapeSideFor).
void CountShapeBones(UInt32 geometry, const Cut& c, UInt32& headBones, UInt32& bones, bool& holdsPart, bool& holdsStump) {
	headBones = 0;
	bones = 0;
	holdsPart = false;
	holdsStump = false;
	const UInt32 skin = Read(geometry + kGeometrySkinInstance);
	if (!LooksLikeObject(skin)) {
		return;
	}
	const UInt32 data = Read(skin + kSkinInstanceData);
	const UInt32 list = Read(skin + kSkinInstanceBones);
	if (!LooksLikeObject(data) || !LooksLikeObject(list)) {
		return;
	}
	const UInt32 count = Read(data + kSkinDataBoneCount);
	if (count == 0 || count > kMaxBones) {
		return;
	}
	const UInt32* const bone = reinterpret_cast<const UInt32*>(list);
	for (UInt32 i = 0; i < count; ++i) {
		if (!LooksLikeObject(bone[i])) {
			continue;
		}
		const NiAVObject* const b = reinterpret_cast<const NiAVObject*>(bone[i]);
		++bones;
		holdsPart = holdsPart || bone[i] == reinterpret_cast<UInt32>(c.headSide.bytes);
		holdsStump = holdsStump || bone[i] == reinterpret_cast<UInt32>(c.bodySide.bytes);
		if (b == c.head || IsUnder(b, c.head) || (b == c.neckTop && c.neckTop != nullptr) ||
		    (b == c.neckBase && c.neckBase != nullptr)) {
			++headBones;
		}
	}
}

// Without its bones read: a shape is the head's when its bound's middle lies nearer the head's
// origin than Spine2's (the head mesh carries the neck down to the
// collarbones and is not under the head node; the body's shapes reach from
// the hips) - derived from the vanilla meshes, logged by name per cut.
bool HeadShape(const NiAVObject* shape, const NiPoint3& head, const NiPoint3& spine2) {
	const NiPoint3 c = shape->worldBound.center;
	return (c - head).LengthSquared() < (c - spine2).LengthSquared();
}

// A shape's side of the cut: under the part's node the part's; else by the
// cut nodes it holds, its bones or its bound (ShapeSideFor); and a limb cut's
// shape the bones give the part only when nothing of it would stretch
// (CutSkeletonLogic.h LimbShapeSideFor) - else the body's, drawn again for the part.
ShapeSide SideOfShape(const NiAVObject* node, bool inHead, const Cut& c, const NiPoint3& joint, UInt32 partBones,
                      UInt32 bones, bool holdsPart, bool holdsStump, const char* shapeName) {
	if (inHead) {
		return ShapeSide::Part;
	}
	const ShapeSide byBones =
		ShapeSideFor(holdsPart, holdsStump, partBones, bones, HeadShape(node, c.head->worldTransform.pos, joint));
	const bool headCut = c.part == CutPart::Head;
	if (!SpanWanted(byBones, headCut, holdsPart || holdsStump, bones > 0)) {
		return byBones;
	}
	UInt32 spanning = 0;
	const SpanRead span = PartSideSpans(reinterpret_cast<UInt32>(node), c, spanning);
	const ShapeSide side = LimbShapeSideFor(byBones, headCut, false, span);
	static UInt32 s_lines = 32;
	if (side != byBones && s_lines > 0) {
		--s_lines;
		char why[96] = "its skin's weights did not read";
		if (span == SpanRead::Spans) {
			std::snprintf(why, sizeof(why), "taken whole, %u of its vertices would hang on both the part and the body and stretch",
			              spanning);
		}
		OBVR_LOG("Dismember: %08X's %s - \"%s\" (%u of its %u bones the part's) stays the body's and is drawn again for "
		         "the part: %s",
		         c.actor, CutPartName(c.part), shapeName != nullptr ? shapeName : "?", partBones, bones, why);
	}
	return side;
}

void SwapSkins(const NiAVObject* node, bool underHead, Cut& c, const NiPoint3& spine2, SwapCount& n,
               UInt32 depth) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(node)) || depth > kMaxDepth) {
		return;
	}
	const bool inHead = underHead || node == c.head;
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			if (children[i] != nullptr) {
				SwapSkins(children[i], inHead, c, spine2, n, depth + 1);
			}
		}
		return;
	}
	if (className != nullptr &&
	    (std::strncmp(className, "NiTriShape@", 11) == 0 || std::strncmp(className, "NiTriStrips@", 12) == 0)) {
		UInt32 headBones = 0;
		UInt32 bones = 0;
		bool holdsPart = false;
		bool holdsStump = false;
		CountShapeBones(reinterpret_cast<UInt32>(node), c, headBones, bones, holdsPart, holdsStump);
		const char* const shapeName = LooksLikeObject(reinterpret_cast<UInt32>(node->name) & ~3u) ? node->name : nullptr;
		const ShapeSide side = SideOfShape(node, inHead, c, spine2, headBones, bones, holdsPart, holdsStump, shapeName);
		const bool headSide = side == ShapeSide::Part;
		const bool decal = IsDecalShapeName(shapeName);
		if (PartCopyWanted(c.part == CutPart::Head, side, headBones, decal)) {
			AddPartCopy(n.copies, node, c.head, reinterpret_cast<UInt32>(c.headSide.bytes));  // before the swap: its bones still real
		} else if (decal && PartCopyWanted(c.part == CutPart::Head, side, headBones, false)) {
			static UInt32 s_lines = 64;  // every cut of dismember-marked-body.txt says its own (25 lines there)
			if (s_lines > 0) {
				--s_lines;
				OBVR_LOG("Dismember: the decal shape \"%s\" is not drawn again for the part - a decal shape's copy holds no "
				         "decal and the decal shader reads one; the marks on the limb's skin stay off the limb",
				         shapeName);
			}
		}
		const UInt32 before = n.shapes;
		SwapShape(reinterpret_cast<UInt32>(node), headSide, c, n);
		if (n.shapes != before && std::strlen(n.names) < sizeof(n.names) - 40) {
			const UInt32 len = static_cast<UInt32>(std::strlen(n.names));
			std::snprintf(n.names + len, sizeof(n.names) - len, "%s%s(%s, %u/%u)", len > 0 ? ", " : "",
			              LooksLikeObject(reinterpret_cast<UInt32>(node->name) & ~3u) ? node->name : "?",
			              headSide ? "head" : "body", headBones, bones);
		}
	}
}

void Place(Cut& c) {
	NiAVObject* const body = AsNode(c.bodySide);
	NiAVObject* const head = AsNode(c.headSide);
	const NiTransform& neck = c.neck->worldTransform;
	body->worldTransform.rot = neck.rot;
	body->worldTransform.pos = neck.pos + neck.rot * (c.cutInNeck * neck.scale);
	body->worldTransform.scale = kCollapsedScale;
	head->worldTransform.rot = c.head->worldTransform.rot;
	head->worldTransform.pos = c.head->worldTransform.pos;
	head->worldTransform.scale = kCollapsedScale;
	NiAVObject* const cap = AsNode(c.capSide);
	cap->worldTransform.rot = neck.rot * c.headRotInNeck;
	cap->worldTransform.pos = body->worldTransform.pos;
	cap->worldTransform.scale = c.headScale * c.capScale;
	NiAVObject* const partCap = AsNode(c.partCapSide);
	partCap->worldTransform.rot = c.head->worldTransform.rot;
	partCap->worldTransform.pos = c.head->worldTransform.pos;
	partCap->worldTransform.scale = c.head->worldTransform.scale * c.capScale;
	if (c.capFlip) {
		partCap->worldTransform = FlippedCap(partCap->worldTransform, c.capCentre, c.capDown);
	}
	SyncFakeBoneParent(c.bodySide);
	SyncFakeBoneParent(c.headSide);
	SyncFakeBoneParent(c.capSide);
	SyncFakeBoneParent(c.partCapSide);
}

constexpr UInt16 kAppCulled = 0x0001;

// A head's cap: read which way it faces against the head bone's way down the
// neck; facing up into the head it is turned over (Place, FlippedCap).
void FaceHeadCapDown(Cut& c, UInt32 stumpCap, UInt32 headCap) {
	const NiPoint3 toNeck = c.neck->worldTransform.pos - c.head->worldTransform.pos;
	const float len = math::Sqrt(toNeck.LengthSquared());
	if (!(len > 1e-3f)) {
		return;
	}
	c.capDown = InverseMatrix(c.head->worldTransform.rot) * (toNeck * (1.0f / len));
	const CapFacing head = ReadCapFacing(headCap);
	const CapFacing stump = ReadCapFacing(stumpCap);
	const float headDown = CapFacingAlong(head, c.capDown);
	const float stumpUp = CapFacingAlong(stump, c.capDown * -1.0f);
	c.capCentre = CapCentre(head);
	c.capFlip = head.unitNormals > 0 && headDown < 0.0f;
	Place(c);
	// Measured after the placing: the cap's mean normal in the world, by the
	// node it now follows, against the head's way down the neck in the world.
	const float normalLen = math::Sqrt(head.normal.LengthSquared());
	const NiPoint3 normalWorld =
		AsNode(c.partCapSide)->worldTransform.rot * (head.normal * (normalLen > 1e-6f ? 1.0f / normalLen : 0.0f));
	const NiPoint3 downWorld = c.head->worldTransform.rot * c.capDown;
	const float placed = normalWorld.x * downWorld.x + normalWorld.y * downWorld.y + normalWorld.z * downWorld.z;
	OBVR_LOG("Dismember: the caps' facing read - the stump's %.2f up the neck (%u of %u normals), the head's %.2f "
	         "down it (%u of %u)%s; placed, the head's %.2f down the neck",
	         static_cast<double>(stumpUp), stump.unitNormals, stump.count, static_cast<double>(headDown), head.unitNormals,
	         head.count, c.capFlip ? " - turned over" : "", static_cast<double>(placed));
}

// The caps of a cut forgotten while its root still stands: culled, so a
// stump cap never reads a node slot given to another cut.
void CullCaps(Cut& c, UInt32 rootNow) {
	for (UInt32& cap : c.caps) {
		if (cap != 0 && rootNow == c.root && LooksLikeObject(cap)) {
			reinterpret_cast<NiAVObject*>(cap)->flags |= kAppCulled;
		}
		cap = 0;
	}
}

// The blood at the cut (game/Blood.h): spurts from the body's neck outward, the
// first with one from the head the other way and a stain under the neck.
void Bleed(Cut& c, const CutSettings& s, float dtSeconds) {
	if (BloodPulseDue(s, c.age, c.spurts)) {
		const NiPoint3 at = AsNode(c.bodySide)->worldTransform.pos;
		const NiPoint3 out = SprayDirection(c.neck->worldTransform.pos, at);
		SprayBlood(c.actor, at, out);
		if (c.spurts == 0) {
			SprayBlood(c.actor, c.head->worldTransform.pos, out * -1.0f);
			StainBlood(c.actor, at, NiPoint3{0.0f, 0.0f, -1.0f});
		}
		++c.spurts;
	}
	if (dtSeconds > 0.0f && dtSeconds < 1.0f) {
		c.age += dtSeconds;
	}
}

// Room for one more cut under [Hands] DismemberMostCuts (CutRoom).
bool RoomForACut() {
	UInt32 standing = 0;
	for (const Cut& c : g_cuts) {
		standing += c.actor != 0 ? 1u : 0u;
	}
	return CutRoom(standing, g_wanted.limit.most);
}

// A slot no cut holds, or null when all kCutsMost do.
Cut* FreeSlot() {
	for (Cut& c : g_cuts) {
		if (c.actor == 0) {
			return &c;
		}
	}
	return nullptr;
}

Cut* Find(UInt32 actor, CutPart part) {
	for (Cut& c : g_cuts) {
		if (c.actor == actor && Live(c) && c.part == part) {
			return &c;
		}
	}
	return nullptr;
}

}  // namespace

bool HeadIsCut(UInt32 actor) { return Find(actor, CutPart::Head) != nullptr; }

bool PartIsCut(UInt32 actor, CutPart part) { return Find(actor, part) != nullptr; }

CutMemory& RememberedCuts() { return g_memory; }

void ForgetRememberedCuts() {
	g_memory = CutMemory{};
	for (Waiting& w : g_waiting) {
		w = Waiting{};
	}
}

bool CutHead(UInt32 actor, const NiPoint3& pushUnits, char* why, UInt32 whySize, bool redo) {
	return CutPartOff(actor, CutPart::Head, pushUnits, why, whySize, redo);
}

// Why a part is not cut before its bones are looked at, or nullptr.
static const char* CutRefusedBefore(UInt32 actor, CutPart part) {
	if (!CutJointsVerified()) {
		return "the engine's functions are not as read";
	}
	if (Find(actor, part) != nullptr) {
		return "cut already";
	}
	for (const Cut& other : g_cuts) {
		if (other.actor == actor && Live(other) && CutPartsClash(part, other.part)) {
			return "a part of that limb is cut already";
		}
	}
	return nullptr;
}

// The cut's bones as this skeleton has them (CutSkeletonLogic.h; the names
// are the human skeleton's, DismemberLogic.h BonesOfCut): the part, the body
// it is joined to, the bone the stump follows. False when the part or a body
// to join it to is not there.
static bool CutBonesOf(UInt32 root, CutPart part, NiAVObject*& head, NiAVObject*& joint, NiAVObject*& stump) {
	const CutPartBones bones = BonesOfCut(part);
	head = Named(root, bones.part);
	if (head == nullptr) {
		return false;
	}
	NiAVObject* const named = Named(root, bones.stump);
	NiAVObject* const parent = ParentOf(head);
	const StumpFrom from = StumpBoneFrom(part == CutPart::Head, named != nullptr, parent != nullptr);
	stump = from == StumpFrom::Named ? named : (from == StumpFrom::Parent ? parent : nullptr);
	NiAVObject* const namedJoint = Named(root, bones.joint);
	NiAVObject* const stumpParent = ParentOf(stump);
	const JointFrom jointFrom = JointBoneFrom(namedJoint != nullptr, stumpParent != nullptr);
	joint = jointFrom == JointFrom::Named ? namedJoint : (jointFrom == JointFrom::StumpParent ? stumpParent : nullptr);
	if (joint == nullptr) {
		return false;
	}
	if (stump == nullptr) {
		stump = joint;
	}
	return true;
}

bool CutPartOff(UInt32 actor, CutPart part, const NiPoint3& pushUnits, char* why, UInt32 whySize, bool redo) {
	auto say = [&](const char* text) {
		std::snprintf(why, whySize, "%s", text);
		return false;
	};
	if (const char* const refusal = CutRefusedBefore(actor, part)) {
		return say(refusal);
	}
	const UInt32 root = LooksLikeObject(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
	// The part's bones (variables named for the head, the first part): the
	// part, the body its joint holds it to, the bone the stump follows.
	const bool isHead = part == CutPart::Head;
	const CutPartBones bones = BonesOfCut(part);
	NiAVObject* head = nullptr;
	NiAVObject* spine2 = nullptr;
	NiAVObject* neck = nullptr;
	if (!CutBonesOf(root, part, head, spine2, neck)) {
		return say("the part's bone or its joint's body is not in its skeleton");
	}
	if (!RoomForACut()) {
		return say("DismemberMostCuts cuts standing - no room");
	}
	Cut* const slot = FreeSlot();
	if (slot == nullptr) {
		return say("all cut slots taken - no room");
	}
	const UInt32 joints = ReleasePartJoints(head);
	Cut& c = *slot;
	c.actor = actor;
	c.world = CurrentWorld();
	c.formId = Read(actor + addr::kFormIdOffset);
	c.part = part;
	c.root = root;
	c.head = head;
	c.neck = neck;
	c.joint = spine2;
	c.neckTop = isHead ? Named(root, "Bip01 Neck1") : nullptr;
	c.neckBase = isHead ? Named(root, "Bip01 Neck") : nullptr;
	c.other = bones.other != nullptr ? Named(root, bones.other) : nullptr;
	const NiTransform& nw = neck->worldTransform;
	const NiPoint3 rel = head->worldTransform.pos - nw.pos;
	const float inv = nw.scale > 0.0f ? 1.0f / nw.scale : 1.0f;
	c.cutInNeck = InverseMatrix(nw.rot) * rel * inv;
	MakeFrom(c.bodySide, head);
	MakeFrom(c.headSide, head);
	c.headRotInNeck = InverseMatrix(nw.rot) * head->worldTransform.rot;
	c.headScale = head->worldTransform.scale > 0.0f ? head->worldTransform.scale : 1.0f;
	MakeFrom(c.capSide, head);
	MakeFrom(c.partCapSide, head);
	c.capScale = CapScaleFor(part);
	Place(c);
	SwapCount n;
	SwapSkins(reinterpret_cast<const NiAVObject*>(root), false, c, spine2->worldTransform.pos, n, 0);
	HangPartCopies(n.copies, actor, CutPartName(part));
	const NiPoint3 push = CutPush(pushUnits);
	const bool pushed = ThrowBodyOfNode(reinterpret_cast<UInt32>(head), push);
	OBVR_LOG("Dismember: %08X's %s cut - %u %s joint(s) released, %u skinned shape(s) redirected (%u body-side bone pointers to the stump, %u part-side to the part: %s; to the part: %s; %u of the part's left on the body), the part %s at %.0f %.0f %.0f units/s",
	         actor, CutPartName(part), joints, CutJointName(part), n.shapes, n.bodyBones, n.headBones, n.names, n.parted,
	         n.leftBehind, pushed ? "woken and sent off" : "NOT woken",
	         static_cast<double>(push.x), static_cast<double>(push.y), static_cast<double>(push.z));
	SoundCut(actor, head->worldTransform.pos, redo);
	if (g_wanted.caps) {
		char capWhy[64] = "";
		char headWhy[64] = "";
		c.caps[0] = AttachCap(root, AsNode(c.capSide), capWhy, sizeof(capWhy));
		// The part's cap on a node following it at its scale (the head's at
		// 1), turned over when it faces into the head.
		c.capFlip = false;
		c.caps[1] = AttachCap(root, AsNode(c.partCapSide), headWhy, sizeof(headWhy));
		if (isHead && c.caps[0] != 0 && c.caps[1] != 0) {
			FaceHeadCapDown(c, c.caps[0], c.caps[1]);
		}
		OBVR_LOG("Dismember: %08X's wound caps - the stump %s (%s), the %s %s (%s)", actor,
		         c.caps[0] != 0 ? "capped" : "NOT capped", capWhy, CutPartName(part), c.caps[1] != 0 ? "capped" : "NOT capped", headWhy);
	}
	if (redo) {
		// A cut done again after a load or a reload of the cell: no blood,
		// no message - both were the first cut's.
		c.spurts = 0xFFFFFFFFu;
	} else {
		RememberCut(g_memory, Read(actor + addr::kFormIdOffset), part);
		// Oblivion Walkers ends a walker whose head is off (its spec 4.17).
		if (isHead) {
			SendWalkersHead(Read(actor + addr::kFormIdOffset), kHeadCut);
		}
	}
	std::snprintf(why, whySize, "cut");
	return true;
}

namespace {

using LookupFn = UInt32(__cdecl*)(UInt32 id);
constexpr UInt32 kLookupFormById = 0x0046B250;  // xOBSE GameAPI.cpp, cdecl, as HandBones.cpp

bool CutStands(UInt32 formId, CutPart part) {
	for (const Cut& c : g_cuts) {
		if (Live(c) && c.part == part && Read(c.actor + addr::kFormIdOffset) == formId) {
			return true;
		}
	}
	return false;
}

// An actor raised again alive (RedoFor's Forget): its remembered cuts go,
// and the waits that followed them start afresh.
void ForgetRaised(UInt32 id) {
	const UInt32 gone = ForgetCuts(g_memory, id);
	for (Waiting& w : g_waiting) {
		w = Waiting{};
	}
	OBVR_LOG("Dismember: %08X raised again alive - its %u remembered cut(s) forgotten, its 3D whole", id, gone);
}

// The heads to stay off (g_memory) whose actor's 3D is there and not cut:
// cut again once it has stood kRedoAfterSeconds (the ragdoll built).
void RedoCuts(float dtSeconds) {
	for (UInt32 i = 0; i < g_memory.count && i < kCutMemoryMost; ++i) {
		const UInt32 id = g_memory.ids[i];
		const CutPart part = static_cast<CutPart>(g_memory.parts[i]);
		Waiting& w = g_waiting[i];
		if (w.id != id) {
			w = Waiting{};
			w.id = id;
		}
		if (CutStands(id, part)) {
			w.root = 0;
			w.seconds = 0.0f;
			w.tried = false;
			continue;
		}
		const UInt32 actor = reinterpret_cast<LookupFn>(kLookupFormById)(id);
		const UInt32 root = LooksLikeObject(actor) ? Read(actor + addr::kRefNiNodeOffset) : 0;
		if (!LooksLikeObject(root)) {
			w.root = 0;
			w.seconds = 0.0f;
			w.tried = false;
			continue;
		}
		if (root != w.root) {
			w.root = root;
			w.seconds = 0.0f;
			w.tried = false;
		}
		if (w.tried) {
			continue;
		}
		if (dtSeconds > 0.0f && dtSeconds < 1.0f) {
			w.seconds += dtSeconds;
		}
		bool dead = false;
		const bool loaded = ActorInWorld(actor, dead);
		const RedoStep step = RedoFor(loaded, dead, w.seconds);
		if (step == RedoStep::Forget) {
			ForgetRaised(id);
			return;  // the memory moved under the loop: the rest next frame
		}
		if (step != RedoStep::Redo) {
			continue;
		}
		w.tried = true;
		char why[96] = "";
		const bool cut = CutPartOff(actor, part, NiPoint3{0.0f, 0.0f, 0.0f}, why, sizeof(why), true);
		if (g_redoLines > 0) {
			--g_redoLines;
			OBVR_LOG("Dismember: %08X's %s cut again after its 3D came back (a load, the cell again) - %s", id, CutPartName(part),
			         cut ? "off again" : why);
		}
	}
}

// After a load (CutWorldFor): each cut whose actor the form table still
// holds is taken into this world, read from the next frame on; the others
// are dropped unread - the load freed them and their 3D, the caps with it -
// and the waits start afresh, so the co-save's memory cuts the parts again
// once their 3D stands.
void RecheckCutsAfterLoad() {
	const UInt32 world = CurrentWorld();
	UInt32 kept = 0;
	UInt32 dropped = 0;
	for (Cut& c : g_cuts) {
		if (c.actor == 0 || CutInWorld(c.world, world)) {
			continue;
		}
		const UInt32 byId = c.formId != 0 ? reinterpret_cast<LookupFn>(kLookupFormById)(c.formId) : 0;
		if (CutWorldFor(c.world, world, c.actor, byId) == CutWorldStep::Recheck) {
			c.world = world;
			++kept;
			continue;
		}
		c = Cut{};
		++dropped;
	}
	if (dropped != 0) {
		for (Waiting& w : g_waiting) {
			w = Waiting{};
		}
	}
	// Said once a load for the cuts kept (each of its frames rechecks them),
	// and for every drop, from a budget of its own.
	static UInt32 s_lines = 16;
	static bool s_keptSaid = false;
	if (dropped != 0 && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Dismember: a load since - %u cut(s) dropped unread, their actor gone from the form table (world %u)",
		         dropped, world);
	}
	if (kept != 0 && !s_keptSaid && s_lines > 0) {
		--s_lines;
		OBVR_LOG("Dismember: a load since - %u cut(s) still on their actor, read again once the load is done (world %u)",
		         kept, world);
	}
	s_keptSaid = kept != 0;
}

}  // namespace

UInt32 StandingCuts() {
	UInt32 n = 0;
	for (const Cut& c : g_cuts) {
		n += Live(c) ? 1 : 0;
	}
	return n;
}

void StepCuts(const CutSettings& settings, float dtSeconds) {
	g_wanted.caps = settings.caps;
	g_wanted.sound = settings.sound;
	const CutLimit limit = CutLimitFor(settings.most);
	if (limit.most != g_wanted.limit.most || limit.clamped != g_wanted.limit.clamped) {
		OBVR_LOG("Dismember: at most %u cuts at once ([Hands] DismemberMostCuts=%d%s)", limit.most, settings.most,
		         limit.clamped ? ", kept within 16 and the slots there are" : "");
	}
	g_wanted.limit = limit;
	RecheckCutsAfterLoad();
	for (Cut& c : g_cuts) {
		if (!Live(c)) {
			continue;  // none, or rechecked this frame: read from the next
		}
		const UInt32 root = LooksLikeObject(c.actor) ? Read(c.actor + addr::kRefNiNodeOffset) : 0;
		const bool skinsRebuilt =
			c.checkSlot != 0 && (!RefBytesReadable(c.checkSlot, 4) || Read(c.checkSlot) != c.checkValue);
		const bool bonesThere =
			LooksLikeObject(reinterpret_cast<UInt32>(c.head)) && LooksLikeObject(reinterpret_cast<UInt32>(c.neck));
		// Only when the check is gone (rare): the part's bone still the skeleton's, and who still points at the cut.
		const bool held = skinsRebuilt && bonesThere && root == c.root &&
		                  Named(root, BonesOfCut(c.part).part) == c.head &&
		                  ShapesHoldBoneIn(root, reinterpret_cast<UInt32>(&c), sizeof(Cut));
		const CutFate fate = CutFateFor(root == c.root, bonesThere, skinsRebuilt, held);
		if (fate == CutFate::Reswap) {
			OBVR_LOG("Dismember: %08X's %s - a shape of its body gone (clothes changed), the cut kept and its skins read again",
			         c.actor, CutPartName(c.part));
			c.checkSlot = 0;
			c.rescanSeconds = kCutRescanSeconds;  // read again this frame, a new check taken
		} else if (fate == CutFate::Forget) {
			OBVR_LOG("Dismember: %08X's 3D changed - its cut forgotten (%s %s: root %08X -> %08X, skins %s)", c.actor,
			         CutPartName(c.part), CutJointName(c.part), c.root, root, skinsRebuilt ? "rebuilt" : "kept");
			CullCaps(c, root);
			c = Cut{};
			continue;
		}
		Place(c);
		Bleed(c, settings, dtSeconds);
		// Shapes drawn after the cut (CutRescanDue): their part's bones too.
		const NiAVObject* const joint =
			CutRescanDue(c.rescanSeconds, dtSeconds) && LooksLikeObject(reinterpret_cast<UInt32>(c.joint)) ? c.joint : nullptr;
		if (joint != nullptr) {
			SwapCount n;
			SwapSkins(reinterpret_cast<const NiAVObject*>(root), false, c, joint->worldTransform.pos, n, 0);
			HangPartCopies(n.copies, c.actor, CutPartName(c.part));
			static UInt32 s_rescanLines = 40;
			if (n.shapes > 0 && s_rescanLines > 0) {
				--s_rescanLines;
				OBVR_LOG("Dismember: %08X's %s - %u skinned shape(s) drawn since the cut redirected (%u to the stump, %u "
				         "to the part: %s; %u of the part's left on the body)",
				         c.actor, CutPartName(c.part), n.shapes, n.bodyBones, n.headBones, n.names, n.leftBehind);
			}
		}
	}
	if (settings.enabled) {
		RedoCuts(dtSeconds);
	}
}

void LogCuts(const char* mark) {
	for (Cut& c : g_cuts) {
		if (!Live(c)) {
			continue;
		}
		const NiPoint3 at = AsNode(c.bodySide)->worldTransform.pos;
		const NiPoint3 head = c.head->worldTransform.pos;
		OBVR_LOG("Dismember: at the mark \"%s\" %08X's %s at %.1f %.1f %.1f, the cut at %.1f %.1f %.1f - %.1f "
		         "units apart",
		         mark, c.actor, CutPartName(c.part), static_cast<double>(head.x), static_cast<double>(head.y), static_cast<double>(head.z),
		         static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
		         static_cast<double>(math::Sqrt((head - at).LengthSquared())));
	}
}

const char* WeaponModelPath(const UInt8* weapon) {
	if (!LooksLikeObject(reinterpret_cast<UInt32>(weapon))) {
		return nullptr;
	}
	const UInt32 path = Read(reinterpret_cast<UInt32>(weapon) + 0x34);
	return LooksLikeObject(path & ~3u) ? reinterpret_cast<const char*>(path) : nullptr;
}

}  // namespace obvr::game
