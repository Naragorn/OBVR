#include "game/MirrorHead.h"

#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonHide.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/HeadCull.h"
#include "game/PlayerBody.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// The engine's hidden bit (NiAVObject +0x18, bit 0; game/FirstPersonHide.h):
// on the third-person root while the body is not drawn in first person.
constexpr UInt16 kHiddenBit = 0x0001;
// NiGeometry's skin instance (+0xB8); NiSkinInstance's skin data (+0x08), its
// bone array (+0x14) and its build stamp (+0x18, read and written by the
// palette builder 0x007655F0 at 0x00765608 and 0x00765638); NiSkinData's bone
// count (+0x40). As BodyView.cpp and ArmStump.cpp read them.
constexpr UInt32 kGeometrySkinInstance = 0xB8;
constexpr UInt32 kSkinInstanceData = 0x08;
constexpr UInt32 kSkinInstanceBones = 0x14;
constexpr UInt32 kSkinInstanceStamp = 0x18;
constexpr UInt32 kSkinDataBoneCount = 0x40;
constexpr UInt32 kMaxBones = 96;
// The walk's bounds: the player's tree has a few hundred nodes (Debug.
// BodyTreeProbe logs up to 400) and is a dozen deep.
constexpr UInt32 kMaxDepth = 24;
constexpr UInt32 kMaxNodes = 1024;
constexpr UInt32 kMaxParentSteps = 12;

bool Readable(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool Readable(const void* p) { return Readable(reinterpret_cast<UInt32>(p)); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool StartsWith(const char* text, const char* prefix) { return std::strncmp(text, prefix, std::strlen(prefix)) == 0; }

// The bone is the head or hangs under it.
bool OnHead(const NiAVObject* bone, const NiAVObject* head) {
	const NiAVObject* at = bone;
	for (UInt32 step = 0; step < kMaxParentSteps && Readable(at); ++step) {
		if (at == head) {
			return true;
		}
		at = at->parent;
	}
	return false;
}

// A shape's skin, kept when one of its bones is on the head.
void TakeIfOnHead(const NiAVObject* geometry, const NiAVObject* head, MirrorHeadSkins& out) {
	const UInt32 skin = Read(reinterpret_cast<UInt32>(geometry) + kGeometrySkinInstance);
	if (!Readable(skin)) {
		return;
	}
	const UInt32 data = Read(skin + kSkinInstanceData);
	const UInt32 bones = Read(skin + kSkinInstanceBones);
	if (!Readable(data) || !Readable(bones)) {
		return;
	}
	const UInt32 count = Read(data + kSkinDataBoneCount);
	if (count == 0 || count > kMaxBones || !Readable(bones + (count - 1) * 4)) {
		return;
	}
	for (UInt32 b = 0; b < count; ++b) {
		if (OnHead(reinterpret_cast<const NiAVObject*>(Read(bones + b * 4)), head)) {
			AddMirrorHeadSkin(out, static_cast<std::uintptr_t>(skin));
			return;
		}
	}
}

struct Walk {
	const NiAVObject* head = nullptr;
	MirrorHeadSkins* out = nullptr;
	UInt32 nodes = 0;
};

// Every shape under the root, hidden or not (the FaceGen nodes carry this
// render's hidden bit, which the switch takes off).
void WalkNode(const NiAVObject* node, UInt32 depth, Walk& w) {
	if (!Readable(node) || depth > kMaxDepth || w.nodes >= kMaxNodes || w.out->overflow) {
		return;
	}
	++w.nodes;
	const char* const className = NiClassNameOf(node);
	if (NiClassIsNode(className)) {
		UInt32 count = 0;
		NiAVObject* const* const children = NodeChildren(node, count);
		for (UInt32 i = 0; children != nullptr && i < count; ++i) {
			if (children[i] != nullptr) {
				WalkNode(children[i], depth + 1, w);
			}
		}
		return;
	}
	if (StartsWith(className, "NiTriShape@") || StartsWith(className, "NiTriStrips@")) {
		TakeIfOnHead(node, w.head, *w.out);
	}
}

// Every listed skin's stamp turned: its palette is built again at its next draw.
void StaleStamps(const MirrorHeadSkins& skins) {
	for (UInt32 i = 0; i < skins.count; ++i) {
		const UInt32 skin = static_cast<UInt32>(skins.skins[i]);
		if (Readable(skin)) {
			UInt32* const stamp = reinterpret_cast<UInt32*>(skin + kSkinInstanceStamp);
			*stamp = StaleMirrorStamp(*stamp);
		}
	}
}

// PlayerBody.cpp's CollapseForThisDraw, the same three steps: the world
// transforms at scale zero, the local scale put back.
void CollapseAgain(NiAVObject* head) {
	const float saved = head->localTransform.scale;
	head->localTransform.scale = 0.0f;
	UpdateNodeTransforms(head);
	head->localTransform.scale = saved;
}

double MsSince(long long start) {
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	return ticksPerSecond > 0 ? 1000.0 * static_cast<double>(ReadPerformanceCounter() - start) /
	                                static_cast<double>(ticksPerSecond)
	                          : 0.0;
}

void NoteRefusal(const MirrorHeadSwitch& s, const Walk& w) {
	static MirrorHeadRefusal s_said = MirrorHeadRefusal::Count;
	static UInt32 s_linesLeft = 24;
	if (s.plan.refusal == s_said || s_linesLeft == 0) {
		return;
	}
	s_said = s.plan.refusal;
	--s_linesLeft;
	OBVR_LOG("Mirror head: %s - %s (%u skin(s) on the head's bones of %u node(s) walked, FaceGen bits lifted %d %d)",
	         MirrorHeadRefusalName(s.plan.refusal),
	         s.plan.refusal == MirrorHeadRefusal::None ? "the head, face, hair and helmet drawn in the mirror's render only"
	                                                    : "the mirror shows what the eyes' renders draw",
	         s.skins.count, w.nodes, s.plan.liftCull[0] ? 1 : 0, s.plan.liftCull[1] ? 1 : 0);
}

}  // namespace

void ShowPlayerHeadForMirror(MirrorHeadSwitch& s) {
	const long long start = ReadPerformanceCounter();
	s = MirrorHeadSwitch{};
	s.asked = true;
	MirrorHeadInputs in;
	NiAVObject* const root = PlayerBodyRoot();
	in.bodyRead = root != nullptr;
	in.bodyDrawn = in.bodyRead && (root->flags & kHiddenBit) == 0;
	NiAVObject* const head = in.bodyDrawn ? PlayerHeadBone() : nullptr;
	in.headRead = head != nullptr;
	if (head != nullptr) {
		in.headWorldScale = head->worldTransform.scale;
		in.headLocalScale = head->localTransform.scale;
	}
	PlayerHeadCullOurs(in.cullOurs);
	Walk w;
	w.head = head;
	w.out = &s.skins;
	if (head != nullptr && in.headWorldScale == 0.0f) {
		WalkNode(root, 0, w);
	}
	in.skins = s.skins.count;
	in.skinsOverflow = s.skins.overflow;
	s.plan = PlanMirrorHead(in);
	NoteRefusal(s, w);
	if (s.plan.refusal == MirrorHeadRefusal::None) {
		s.head = head;
		UpdateNodeTransforms(head);         // (1) the true transform; back in HidePlayerHeadAfterMirror (2)
		LiftPlayerHeadCull(s.plan.liftCull);  // (2) back there (3)
		StaleStamps(s.skins);               // (3) back there (1)
	}
	s.ms = MsSince(start);
}

void HidePlayerHeadAfterMirror(MirrorHeadSwitch& s) {
	if (s.plan.refusal != MirrorHeadRefusal::None || s.head == nullptr) {
		return;
	}
	const long long start = ReadPerformanceCounter();
	StaleStamps(s.skins);                  // (1) the rows now carry the shown head: built again at the next draw
	CollapseAgain(s.head);                 // (2) the collapse the eyes drew with
	PutBackPlayerHeadCull(s.plan.liftCull);  // (3) the bits on, until ReleasePlayerHeadCull
	s.head = nullptr;
	s.ms += MsSince(start);
}

}  // namespace obvr::game
