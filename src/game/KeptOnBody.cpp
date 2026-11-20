#include "game/KeptOnBody.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/Dismember.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "game/HolsterSlots.h"
#include "game/OwnedNode.h"
#include "game/PlayerBody.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

// The pelvis's z against the thighs, as the body's own mirror of the game's
// sheath node judges it (game/BodyIk.cpp StepBodySheaths).
constexpr float kHipAxisSlackDegrees = 30.0f;
// NiAVObject's collision object (a smart pointer, game/HandBodies.cpp).
constexpr UInt32 kCollisionObjectOffset = 0xA8;

// The engine's count of an item in a container's changes (read 2026-11-20 in
// Oblivion.exe 1.2.0.416: thiscall(changes, item), `ret 4`; the owner's base
// container counted by 0x00469CA0, the changes' entry for the item added).
constexpr UInt32 kItemCount = 0x004869C0;
const UInt8 kItemCountBytes[] = {0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x4E, 0x04, 0x85};

struct Hung {
	NiAVObject* node = nullptr;  // OBVR's copy, held by OBVR
	UInt32 form = 0;             // the base form it shows
	KeptKind kind = KeptKind::Nothing;
	UInt32 failed = 0;           // a form that would not hang: tried again when another is wanted, or later
	UInt32 retryIn = 0;          // frames until it is tried again
};

// About two seconds at the headset's rate: a skeleton still being built.
constexpr UInt32 kRetryFrames = 180;

struct KeptState {
	NiAVObject* root = nullptr;    // the skeleton the copies hang on
	NiAVObject* side = nullptr;    // its "SideWeapon"
	NiAVObject* pelvis = nullptr;  // that node's parent, where the copies hang
	NiAVObject* thigh[2] = {};     // right, left
	Hung hung[kKeptHipCount];
	UInt32 lines = 40;
	bool firstSaid = false;
};

KeptState& State() {
	static KeptState s_state;
	return s_state;
}

const char* HipName(UInt32 hip) { return hip == 0 ? "left" : "right"; }

bool Budget(KeptState& s) {
	if (s.lines == 0) {
		return false;
	}
	--s.lines;
	return true;
}

// The copy at `hip` let go: taken off the pelvis when it still hangs there
// and the pelvis is still the skeleton's (`pelvisAlive`), OBVR's reference
// given back.
void Drop(KeptState& s, UInt32 hip, bool pelvisAlive, const char* why) {
	Hung& h = s.hung[hip];
	if (h.node == nullptr) {
		return;
	}
	if (pelvisAlive && s.pelvis != nullptr && h.node->parent == s.pelvis) {
		RemoveChildNode(s.pelvis, h.node);
	}
	if (Budget(s)) {
		OBVR_LOG("Kept on body: the %s hip's %08X taken off - %s", HipName(hip), h.form, why);
	}
	ReleaseNode(h.node);
	h.node = nullptr;
	h.form = 0;
	h.kind = KeptKind::Nothing;
}

const char* KindName(KeptKind kind) {
	switch (kind) {
	case KeptKind::Weapon:
		return "weapon";
	case KeptKind::Shield:
		return "shield";
	case KeptKind::Torch:
		return "torch";
	case KeptKind::Potion:
		return "potion";
	default:
		return "nothing";
	}
}

// The engine's dynamic cast (addr::kDynamicCast, as game/ItemIcons.cpp casts)
// from TESForm to `toType`; 0 when the form has no such part.
UInt32 Cast(const UInt8* form, UInt32 toType) {
	using CastFn = void*(__cdecl*)(void* object, SInt32 delta, const void* from, const void* to, SInt32 isRef);
	return reinterpret_cast<UInt32>(reinterpret_cast<CastFn>(addr::kDynamicCast)(
		const_cast<UInt8*>(form), 0, reinterpret_cast<const void*>(addr::kRttiTESForm),
		reinterpret_cast<const void*>(toType), 0));
}

// TESModel's type descriptor (".?AVTESModel@@" at 0x00B0330C in Oblivion.exe
// 1.2.0.416, the descriptor 8 bytes before it, found as kRttiTESBipedModelForm
// is) and its path, a BSStringT at +4 (xOBSE GameForms.h: TESModel 0x18
// bytes, nifPath at +4).
constexpr UInt32 kRttiTESModel = 0x00B03304;
constexpr UInt32 kModelPathOffset = 0x04;
// TESBipedModelForm (xOBSE GameForms.h): bipedModel[2] at +0x08, groundModel[2]
// at +0x38, each a TESModel - the paths of the male and female ground model,
// then of the male and female worn one. A vanilla shield has one file for all.
constexpr UInt32 kShieldModelPaths[4] = {0x38 + 4, 0x50 + 4, 0x08 + 4, 0x20 + 4};

const char* PathAt(UInt32 address) {
	const UInt32 text = *reinterpret_cast<const UInt32*>(address);
	return LooksLikeObject(reinterpret_cast<const void*>(text)) && reinterpret_cast<const char*>(text)[0] != '\0'
	           ? reinterpret_cast<const char*>(text)
	           : nullptr;
}

// The model a kept thing is shown with: a shield's ground model (as it lies in
// the world), anything else's own TESModel; nullptr when it has none.
const char* ModelPathOf(const UInt8* form, KeptKind kind) {
	if (!LooksLikeObject(form)) {
		return nullptr;
	}
	if (kind == KeptKind::Weapon) {
		return WeaponModelPath(form);
	}
	if (kind == KeptKind::Shield) {
		const UInt32 biped = Cast(form, addr::kRttiTESBipedModelForm);
		for (UInt32 i = 0; LooksLikeObject(reinterpret_cast<const void*>(biped)) && i < 4; ++i) {
			if (const char* const path = PathAt(biped + kShieldModelPaths[i])) {
				return path;
			}
		}
		return nullptr;
	}
	const UInt32 model = Cast(form, kRttiTESModel);
	return LooksLikeObject(reinterpret_cast<const void*>(model)) ? PathAt(model + kModelPathOffset) : nullptr;
}

// The engine's flames off (0x004DE1C0, cdecl(NiNode*): every "FlameNode<n>"
// emptied, "FlameCap" hidden - game/TorchFire.h), for a torch kept at a hip:
// it hangs there unlit. False when the function is not as read.
bool TorchUnlit(NiAVObject* copy) {
	static const UInt8 kFlamesOffBytes[] = {0x57, 0x8B, 0x7C, 0x24, 0x08, 0x85, 0xFF, 0x75};
	static const bool asRead = mem::Verify(0x004DE1C0, kFlamesOffBytes, sizeof(kFlamesOffBytes));
	if (!asRead) {
		return false;
	}
	using FlamesOffFn = UInt32(__cdecl*)(NiAVObject* node);
	reinterpret_cast<FlamesOffFn>(0x004DE1C0)(copy);
	return true;
}

void FindNodes(KeptState& s) {
	static const char* const kNames[3] = {"SideWeapon", "Bip01 R Thigh", "Bip01 L Thigh"};
	NiAVObject* found[3] = {};
	FindNodesNamed(s.root, kNames, 3, found);
	s.side = found[0];
	s.pelvis = s.side != nullptr && LooksLikeObject(s.side->parent) ? s.side->parent : nullptr;
	s.thigh[0] = found[1];
	s.thigh[1] = found[2];
}

// The copy's collision object let go, as a smart pointer set to null
// (NiPointer's assign, addr::kNiPointerAssign, as read by game/DismemberCaps.cpp):
// the game's own sheathed weapon carries none (read at the first hang,
// 2026-11-20: "collision object 00000000", flags 000E as the copy's). With its
// collision object a copy kept the world transform of the moment it was hung -
// 23.8 m before the eyes while the game's sword hung at the hip; without it, it
// hangs at the exact mirror of the game's sheath (8.6 3.6 -14.6 against
// 8.6 3.6 14.6 in the pelvis's axes). Tried apart in kept-on-body.txt the same
// day: the collision object gone alone puts it there and draws the blade there
// (20261120-152856-slot3); the selective-update bits set alone leave it stuck
// (-152654-slot1). [D: its rigid body writes the node; not read in the EXE.]
// False when the assign is not as read or there was none.
bool DropCollision(NiAVObject* copy) {
	static const UInt8 kAssignBytes[] = {0x53, 0x8B, 0x5C, 0x24, 0x08, 0x56, 0x57, 0x8B};
	static const bool assignAsRead = mem::Verify(addr::kNiPointerAssign, kAssignBytes, sizeof(kAssignBytes));
	UInt8* const slot = reinterpret_cast<UInt8*>(copy) + kCollisionObjectOffset;
	if (!assignAsRead || *reinterpret_cast<const UInt32*>(slot) == 0) {
		return false;
	}
	using AssignFn = void(__fastcall*)(void* slot, void* edx, void* object);
	reinterpret_cast<AssignFn>(addr::kNiPointerAssign)(slot, nullptr, nullptr);
	return true;
}

// Said once, at the first copy hung: what the game's own sheathed weapon is
// under its node (its local transform, its collision object) and what the
// copy is - so a reader can see the copy hangs as the game's does.
void SayFirst(KeptState& s, const NiAVObject* copy) {
	if (s.firstSaid) {
		return;
	}
	s.firstSaid = true;
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(s.side, count);
	const NiAVObject* game = nullptr;
	for (UInt32 i = 0; children != nullptr && i < count && game == nullptr; ++i) {
		game = children[i];
	}
	const UInt32 copyCollision = *reinterpret_cast<const UInt32*>(reinterpret_cast<UInt32>(copy) + kCollisionObjectOffset);
	if (game == nullptr) {
		OBVR_LOG("Kept on body: the copy is a %s, its collision object %08X; the game's sheath holds nothing now",
		         NiClassNameOf(copy), copyCollision);
		return;
	}
	const NiTransform& t = game->localTransform;
	OBVR_LOG("Kept on body: the copy is a %s, its collision object %08X; the game's sheathed weapon is a %s (%s), "
	         "flags %04X, its collision object %08X, under the sheath at %.2f %.2f %.2f, turned [%.2f %.2f %.2f / %.2f %.2f %.2f / %.2f "
	         "%.2f %.2f], scale %.2f",
	         NiClassNameOf(copy), copyCollision, NiClassNameOf(game), LooksLikeObject(game->name) ? game->name : "?",
	         game->flags, *reinterpret_cast<const UInt32*>(reinterpret_cast<UInt32>(game) + kCollisionObjectOffset),
	         static_cast<double>(t.pos.x), static_cast<double>(t.pos.y), static_cast<double>(t.pos.z),
	         static_cast<double>(t.rot.data[0][0]), static_cast<double>(t.rot.data[0][1]),
	         static_cast<double>(t.rot.data[0][2]), static_cast<double>(t.rot.data[1][0]),
	         static_cast<double>(t.rot.data[1][1]), static_cast<double>(t.rot.data[1][2]),
	         static_cast<double>(t.rot.data[2][0]), static_cast<double>(t.rot.data[2][1]),
	         static_cast<double>(t.rot.data[2][2]), static_cast<double>(t.scale));
}

bool Hang(KeptState& s, UInt32 hip, UInt32 formId, KeptKind kind, const char*& why) {
	const char* const path = ModelPathOf(HolsterSlotForm(formId), kind);
	if (path == nullptr || path[0] == '\0') {
		why = "it has no model";
		return false;
	}
	if (s.pelvis == nullptr || s.thigh[0] == nullptr || s.thigh[1] == nullptr) {
		why = "the body has no \"SideWeapon\" under a pelvis, or no thighs";
		return false;
	}
	const NiTransform& pelvis = s.pelvis->worldTransform;
	const NiPoint3 right = s.thigh[0]->worldTransform.pos;
	const NiPoint3 left = s.thigh[1]->worldTransform.pos;
	if (!LateralAxisAgrees(pelvis.rot, right, left, kHipAxisSlackDegrees)) {
		why = "the pelvis's z is not the body's left-right (a skeleton built otherwise)";
		return false;
	}
	const float rightOf = RightOfAcross(pelvis.rot, pelvis.scale, s.side->localTransform.pos, right, left);
	NiAVObject* const copy = LoadModelCopy(path, why);
	if (copy == nullptr) {
		return false;
	}
	// The world's up (z) in the pelvis's axes, for a potion standing upright.
	const NiPoint3 up{pelvis.rot.data[2][0], pelvis.rot.data[2][1], pelvis.rot.data[2][2]};
	copy->localTransform = KeptHipLocalFor(kind, s.side->localTransform, rightOf, KeptHipSlot(hip), up);
	if (kind == KeptKind::Torch && !TorchUnlit(copy)) {
		ReleaseNode(copy);
		why = "the engine's flames-off is not as read";
		return false;
	}
	SayFirst(s, copy);  // before the collision object goes: what the file brought
	const bool noCollision = DropCollision(copy);
	const UInt16 flagsAsLoaded = copy->flags;
	AddChildNode(s.pelvis, copy);
	if (!SettleNewNode(copy)) {
		RemoveChildNode(s.pelvis, copy);
		ReleaseNode(copy);
		why = "the scene's updates are not as read";
		return false;
	}
	s.hung[hip].node = copy;
	s.hung[hip].form = formId;
	s.hung[hip].kind = kind;
	if (Budget(s)) {
		OBVR_LOG("Kept on body: the %s hip shows %08X, a %s (%s) under \"%s\", %.1f units to the body's %s; its flags "
		         "%04X as loaded",
		         HipName(hip), formId, KindName(kind), path, LooksLikeObject(s.pelvis->name) ? s.pelvis->name : "?",
		         static_cast<double>(rightOf < 0.0f ? -rightOf : rightOf), hip == 0 ? "left" : "right",
		         flagsAsLoaded);
		if (noCollision) {
			OBVR_LOG("Kept on body: the copy's collision object let go, as the game's sheathed weapon has none");
		}
	}
	return true;
}

// A world point in the pelvis's own axes, units.
NiPoint3 InPelvis(const NiTransform& pelvis, const NiPoint3& world) {
	const NiPoint3 d = world - pelvis.pos;
	const float scale = pelvis.scale > 0.0f ? pelvis.scale : 1.0f;
	NiPoint3 out;
	out.x = (pelvis.rot.data[0][0] * d.x + pelvis.rot.data[1][0] * d.y + pelvis.rot.data[2][0] * d.z) / scale;
	out.y = (pelvis.rot.data[0][1] * d.x + pelvis.rot.data[1][1] * d.y + pelvis.rot.data[2][1] * d.z) / scale;
	out.z = (pelvis.rot.data[0][2] * d.x + pelvis.rot.data[1][2] * d.y + pelvis.rot.data[2][2] * d.z) / scale;
	return out;
}

}  // namespace

void StepKeptOnBody(const KeptOnBody& wanted) {
	KeptState& s = State();
	NiAVObject* const root = PlayerBodyRoot();
	if (root != s.root) {
		for (UInt32 hip = 0; hip < kKeptHipCount; ++hip) {
			Drop(s, hip, false, root == nullptr ? "no body" : "the body is a new one");
			s.hung[hip].failed = 0;
		}
		s.root = root;
		s.side = nullptr;
		s.pelvis = nullptr;
		if (root != nullptr) {
			FindNodes(s);
		}
	}
	if (root == nullptr) {
		return;
	}
	for (UInt32 hip = 0; hip < kKeptHipCount; ++hip) {
		Hung& h = s.hung[hip];
		const UInt32 want = wanted.form[hip];
		if (h.node != nullptr && (h.form != want || h.kind != wanted.kind[hip] || h.node->parent != s.pelvis)) {
			Drop(s, hip, true, h.node->parent != s.pelvis ? "it no longer hangs on the pelvis" : "the hip wants another or none");
		}
		if (want == 0 || want != h.failed || (h.retryIn > 0 && --h.retryIn == 0)) {
			h.failed = 0;
		}
		if (want == 0 || h.node != nullptr || h.failed != 0) {
			continue;
		}
		if (s.pelvis == nullptr) {
			FindNodes(s);  // a skeleton read before its sheath node was there
		}
		const char* why = "";
		if (!Hang(s, hip, want, wanted.kind[hip], why)) {
			h.failed = want;
			h.retryIn = kRetryFrames;
			if (Budget(s)) {
				OBVR_LOG("Kept on body: the %s hip's %08X NOT hung - %s", HipName(hip), want, why);
			}
		}
	}
}

UInt32 PlayerItemCount(UInt8* item) {
	static int verified = 0;
	if (verified == 0) {
		verified = mem::Verify(kItemCount, kItemCountBytes, sizeof(kItemCountBytes)) ? 1 : -1;
		if (verified < 0) {
			mem::ReportForeignCode("Kept on body", kItemCount);
			OBVR_LOG("Kept on body: the item count at %08X is not as read - no weapon is shown at the hips", kItemCount);
		}
	}
	UInt8* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (verified < 0 || !LooksLikeObject(player) || !LooksLikeObject(item) ||
	    *reinterpret_cast<const UInt32*>(player) != addr::kVtblPlayerCharacter) {
		return 0;
	}
	using ChangesFn = void*(__fastcall*)(void* extraList, void* edx);
	void* const changes =
		reinterpret_cast<ChangesFn>(addr::kGetContainerChanges)(player + addr::kActorExtraListOffset, nullptr);
	if (!LooksLikeObject(changes)) {
		return 0;
	}
	using CountFn = SInt32(__fastcall*)(void* changes, void* edx, UInt8* item);
	const SInt32 count = reinterpret_cast<CountFn>(kItemCount)(changes, nullptr, item);
	return count > 0 ? static_cast<UInt32>(count) : 0;
}

bool KeptOnBodyHilt(UInt32 hip, NiPoint3& world, KeptKind* kind) {
	const KeptState& s = State();
	const Hung* const h = hip < kKeptHipCount ? &s.hung[hip] : nullptr;
	if (h == nullptr || h->node == nullptr || s.pelvis == nullptr || h->node->parent != s.pelvis ||
	    s.root != PlayerBodyRoot()) {
		return false;
	}
	// Each is taken where it hangs from: the model's origin - a weapon's and
	// a torch's grip, a shield's grip at the hip, a potion's middle. (A copy's
	// world bound stays at 0 0 0: the engine's selective pass does not bring
	// it up - kept-on-body-shield.txt, 2026-11-20.)
	world = h->node->worldTransform.pos;
	if (kind != nullptr) {
		*kind = h->kind;
	}
	return true;
}

void LogKeptOnBody(const char* mark) {
	KeptState& s = State();
	if (s.root == nullptr || s.pelvis == nullptr || s.root != PlayerBodyRoot()) {
		OBVR_LOG("Kept on body: (mark %s) no body read", mark);
		return;
	}
	const NiTransform& pelvis = s.pelvis->worldTransform;
	char game[160] = "the game's sheath holds nothing";
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(s.side, count);
	for (UInt32 i = 0; children != nullptr && i < count; ++i) {
		if (children[i] != nullptr) {
			const NiPoint3 at = InPelvis(pelvis, children[i]->worldTransform.pos);
			std::snprintf(game, sizeof(game), "the game's sheathed weapon at %.1f %.1f %.1f before the body's mirror",
			              static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z));
			break;
		}
	}
	for (UInt32 hip = 0; hip < kKeptHipCount; ++hip) {
		const Hung& h = s.hung[hip];
		if (h.node == nullptr) {
			OBVR_LOG("Kept on body: (mark %s) the %s hip shows nothing of OBVR's; %s (in the pelvis's axes)", mark,
			         HipName(hip), game);
			continue;
		}
		const NiPoint3 at = InPelvis(pelvis, h.node->worldTransform.pos);
		OBVR_LOG("Kept on body: (mark %s) the %s hip shows %08X, a %s, at %.1f %.1f %.1f, %s; %s (in the pelvis's axes)",
		         mark, HipName(hip), h.form, KindName(h.kind), static_cast<double>(at.x), static_cast<double>(at.y), static_cast<double>(at.z),
		         (h.node->flags & 1) != 0 ? "hidden" : "not hidden", game);
	}
}

}  // namespace obvr::game
