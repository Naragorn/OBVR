#include "game/BowVisual.h"

#include <intrin.h>

#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/BonePin.h"
#include "game/FirstPersonArms.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "game/NearbyItems.h"

namespace obvr::game {
namespace {

bool LooksLikeObject(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

template <typename T>
T& At(void* object, UInt32 offset) {
	return *reinterpret_cast<T*>(reinterpret_cast<UInt8*>(object) + offset);
}

constexpr UInt16 kAppCulledBit = 0x0001;

bool NamedExactly(const NiAVObject* node, const char* name) {
	if (!LooksLikeObject(node) || !LooksLikeObject(node->name)) {
		return false;
	}
	return std::strcmp(node->name, name) == 0;
}

bool ClassIs(const void* object, const char* className) {
	const char* const name = NiClassNameOf(object);
	const UInt32 length = static_cast<UInt32>(std::strlen(className));
	return std::strncmp(name, className, length) == 0 && name[length] == '@';
}

// The engine's own calls, as it makes the drawn arrow at 0x005FD02C-0x005FD03F:
// NiObject::Clone (thiscall, no arguments) and NiNode::AddObject (vtable
// +0x84: the child and 1). RemoveObject is vtable +0x88 (an out smart pointer
// and the child, 0x0070B120); the reference it hands back is released as the
// engine releases one (0x006691A1: decremented, destroyed through vtable +0
// with 1 at zero).
NiAVObject* CloneOf(const NiAVObject* source) {
	using CloneFn = NiAVObject*(__thiscall*)(const NiAVObject* self);
	return reinterpret_cast<CloneFn>(addr::kNiObjectClone)(source);
}

void AddChild(NiAVObject* parent, NiAVObject* child) {
	using AddFn = void(__thiscall*)(NiAVObject* self, NiAVObject* child, UInt32 firstFree);
	const auto add = *reinterpret_cast<AddFn*>(reinterpret_cast<UInt8*>(parent->vtable) + addr::kNiNodeAddObjectSlot);
	add(parent, child, 1);
}

void Release(NiAVObject* object) {
	if (_InterlockedDecrement(reinterpret_cast<volatile long*>(&object->refCount)) == 0) {
		using DestroyFn = void(__thiscall*)(NiAVObject* self, bool freeThis);
		const auto destroy = *reinterpret_cast<DestroyFn*>(object->vtable);
		destroy(object, true);
	}
}

void RemoveChild(NiAVObject* parent, NiAVObject* child) {
	using RemoveFn = NiAVObject**(__thiscall*)(NiAVObject* self, NiAVObject** out, NiAVObject* child);
	const auto remove =
		*reinterpret_cast<RemoveFn*>(reinterpret_cast<UInt8*>(parent->vtable) + addr::kNiNodeRemoveObjectSlot);
	NiAVObject* removed = nullptr;
	remove(parent, &removed, child);
	if (removed != nullptr) {
		Release(removed);
	}
}

// The arrow OBVR shows in the drawing hand: a clone of the quiver's
// "Arrow:0", with a reference of OBVR's own, so the pointer stays good
// whatever happens to the tree it hangs in.
struct HandArrow {
	NiAVObject* node = nullptr;
	const NiAVObject* source = nullptr;  // the quiver's arrow it was made from
	float lengthUnits = 0.0f;
};
HandArrow g_arrow;
UInt32 g_lines = 16;

void Note(const char* what) {
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Bow by hand: %s", what);
	}
}

void HideArrow() {
	if (g_arrow.node != nullptr) {
		g_arrow.node->flags = static_cast<UInt16>(g_arrow.node->flags | kAppCulledBit);
	}
}

// Lets the arrow go: off its holder when it still hangs there, OBVR's
// reference given back. A parent that is no longer the holder is not written
// to - it may be gone with an old skeleton.
void DropArrow(NiAVObject* holder) {
	if (g_arrow.node == nullptr) {
		return;
	}
	HideArrow();
	if (holder != nullptr && g_arrow.node->parent == holder) {
		RemoveChild(holder, g_arrow.node);
	}
	Release(g_arrow.node);
	g_arrow = HandArrow{};
}

const NiAVObject* QuiverArrow() {
	NiAVObject* const quiver = FindFirstPersonNode("Quiver");
	if (quiver == nullptr) {
		return nullptr;
	}
	NiAVObject* found[4] = {};
	const UInt32 count = CollectNodesContaining(quiver, "Arrow:0", found, 4);
	for (UInt32 i = 0; i < count; ++i) {
		if (NamedExactly(found[i], "Arrow:0")) {
			return found[i];
		}
	}
	return nullptr;
}

// The arrow for this quiver on its holder, made when there is none or the
// ammunition changed.
bool EnsureArrow(NiAVObject* holder) {
	const NiAVObject* const source = QuiverArrow();
	if (source == nullptr) {
		Note("no \"Arrow:0\" in the quiver - no arrow in the hand");
		DropArrow(holder);
		return false;
	}
	if (g_arrow.node != nullptr && (g_arrow.source != source || g_arrow.node->parent != holder)) {
		DropArrow(holder);
	}
	if (g_arrow.node != nullptr) {
		return true;
	}
	// Head to nock along the model's own y (vr/Archery.h): the lowest vertex.
	float low = 0.0f;
	float high = 0.0f;
	const NiPoint3 axis = source->worldTransform.rot * NiPoint3{0.0f, 1.0f, 0.0f};
	if (!AxialExtentOf(source, source->worldTransform.pos, axis, low, high) || !(low < -1.0f)) {
		Note("the quiver's arrow could not be measured - no arrow in the hand");
		return false;
	}
	NiAVObject* const clone = CloneOf(source);
	if (!LooksLikeObject(clone)) {
		Note("the quiver's arrow could not be cloned");
		return false;
	}
	_InterlockedIncrement(reinterpret_cast<volatile long*>(&clone->refCount));
	AddChild(holder, clone);
	g_arrow.node = clone;
	g_arrow.source = source;
	const float scale = source->worldTransform.scale > 0.0f ? source->worldTransform.scale : 1.0f;
	g_arrow.lengthUnits = -low / scale;
	if (g_lines > 0) {
		--g_lines;
		OBVR_LOG("Bow by hand: an arrow made for the hand from the quiver's (%08X), %.1f units head to nock",
		         reinterpret_cast<UInt32>(source), static_cast<double>(g_arrow.lengthUnits));
	}
	return true;
}

void Place(NiAVObject* node, const BonePose& world) {
	const NiAVObject* const parent = node->parent;
	const float parentScale = parent->worldTransform.scale > 0.0f ? parent->worldTransform.scale : 1.0f;
	const BonePose local = LocalUnderParent(parent->worldTransform.rot, parent->worldTransform.pos, parentScale, world);
	node->localTransform.rot = local.rot;
	node->localTransform.pos = local.pos;
	node->localTransform.scale = 1.0f / parentScale;
	UpdateNodeTransforms(node);
}

// The engine's arrow on the bow, hidden while the bow is drawn by hand: the
// hand's arrow is the one seen.
NiAVObject* g_hiddenArrowBone = nullptr;

void ShowEngineArrow() {
	if (g_hiddenArrowBone != nullptr && FindFirstPersonNode("ArrowBone") == g_hiddenArrowBone) {
		g_hiddenArrowBone->flags = static_cast<UInt16>(g_hiddenArrowBone->flags & ~kAppCulledBit);
	}
	g_hiddenArrowBone = nullptr;
}

// The string: the bow's morpher (NiGeomMorpherController, found by the
// engine's own blend at 0x006D0B60/0x006D0C30): its weights (a NiTArray of
// floats at +0x40, the data at +0x44 and the count at +0x4A), its morph data
// at +0x50, the flag its blend waits for at +0x58. NiMorphData: the vertex
// count at +0x0C, the morphs at +0x10, 12 bytes each, the vertices first
// (the load at 0x006DE410). A NiTimeController's next is at +0x2C.
constexpr UInt32 kControllerNext = 0x2C;
constexpr UInt32 kMorpherWeights = 0x44;
constexpr UInt32 kMorpherWeightCount = 0x4A;
constexpr UInt32 kMorpherData = 0x50;
constexpr UInt32 kMorpherBlendWanted = 0x58;
constexpr UInt32 kMorphDataVertexCount = 0x0C;
constexpr UInt32 kMorphDataMorphs = 0x10;
constexpr UInt32 kMorphStride = 0x0C;

struct BowString {
	void* morpher = nullptr;
	void* data = nullptr;
	float restAlongBow = -15.6f;  // the iron bow's, until measured
	float travelUnits = 28.13f;
	bool measured = false;
};
BowString g_string;
float g_lastWeight = 0.0f;

void* MorpherOf(NiAVObject* geometry) {
	void* controller = geometry->controller;
	for (int i = 0; i < 8 && LooksLikeObject(controller); ++i) {
		if (ClassIs(controller, "NiGeomMorpherController")) {
			return controller;
		}
		controller = At<void*>(controller, kControllerNext);
	}
	return nullptr;
}

// The string's rest and travel from the morph itself: the vertex the
// "BowMorph" target moves furthest back along the bow's shot axis.
void MeasureString(void* data) {
	const UInt32 count = At<UInt32>(data, kMorphDataVertexCount);
	UInt8* const morphs = At<UInt8*>(data, kMorphDataMorphs);
	if (count == 0 || count > 65535 || !LooksLikeObject(morphs)) {
		return;
	}
	const auto* base = *reinterpret_cast<const NiPoint3* const*>(morphs);
	const auto* pull = *reinterpret_cast<const NiPoint3* const*>(morphs + kMorphStride);
	if (!LooksLikeObject(base) || !LooksLikeObject(pull) || !LooksLikeObject(base + count - 1) ||
	    !LooksLikeObject(pull + count - 1)) {
		return;
	}
	UInt32 furthest = 0;
	for (UInt32 i = 1; i < count; ++i) {
		if (pull[i].x < pull[furthest].x) {
			furthest = i;
		}
	}
	if (pull[furthest].x < -1.0f) {
		g_string.restAlongBow = base[furthest].x;
		g_string.travelUnits = -pull[furthest].x;
		g_string.measured = true;
	}
}

// The bow's morpher, measured the first time it is seen; null when the bow
// has none this code can read.
void* StringOf(NiAVObject* bow) {
	NiAVObject* const geometry = FindFirstPersonNode("Bow:0");
	if (geometry == nullptr || geometry->parent != bow) {
		Note("no \"Bow:0\" on the bow - the string stays the game's");
		return nullptr;
	}
	void* const morpher = MorpherOf(geometry);
	void* const data = morpher != nullptr ? At<void*>(morpher, kMorpherData) : nullptr;
	if (morpher == nullptr || !LooksLikeObject(data) || !ClassIs(data, "NiMorphData") ||
	    At<UInt16>(morpher, kMorpherWeightCount) < 2 || !LooksLikeObject(At<float*>(morpher, kMorpherWeights))) {
		Note("the bow's string morph not found - the string stays the game's");
		return nullptr;
	}
	if (morpher != g_string.morpher || data != g_string.data) {
		g_string = BowString{};
		g_string.morpher = morpher;
		g_string.data = data;
		MeasureString(data);
		if (g_lines > 0) {
			--g_lines;
			OBVR_LOG("Bow by hand: the string %s - at rest %.1f along the bow, %.1f units to full draw",
			         g_string.measured ? "measured" : "NOT measured (the iron bow's kept)",
			         static_cast<double>(g_string.restAlongBow), static_cast<double>(g_string.travelUnits));
		}
	}
	return morpher;
}

void SetStringWeight(void* morpher, float weight) {
	g_lastWeight = weight;
	At<float*>(morpher, kMorpherWeights)[1] = weight;
	At<UInt8>(morpher, kMorpherBlendWanted) = 1;
	using BlendFn = void(__thiscall*)(void* self);
	reinterpret_cast<BlendFn>(addr::kGeomMorpherBlend)(morpher);
}

// The drawing hand moved so its grip point lands on `target`: its bone
// shifted in the world, the grip and fingers riding along ("kann man den nur
// noch in einer linie ... ziehen").
void MoveHandTo(const char* boneName, const NiAVObject* grip, const NiPoint3& target) {
	NiAVObject* const hand = FindFirstPersonNode(boneName);
	if (hand == nullptr || hand->parent == nullptr) {
		return;
	}
	const NiAVObject* const parent = hand->parent;
	const float parentScale = parent->worldTransform.scale > 0.0f ? parent->worldTransform.scale : 1.0f;
	const NiPoint3 shift = target - grip->worldTransform.pos;
	hand->localTransform.pos = hand->localTransform.pos + InverseRotation(parent->worldTransform.rot) * shift *
	                                                          (1.0f / parentScale);
	UpdateNodeTransforms(hand);
}

// The bow's shot axis as the last step found it, and the string's draw.
bool g_shotValid = false;
NiPoint3 g_shot{0.0f, 1.0f, 0.0f};
bool g_drawValid = false;
float g_draw = 0.0f;

}  // namespace

bool BowDrawWeight(float& weight) {
	if (!g_drawValid) {
		return false;
	}
	weight = g_draw;
	return true;
}

bool BowShotAxis(NiPoint3& world) {
	if (!g_shotValid) {
		return false;
	}
	world = g_shot;
	return true;
}

void StepBowVisual(const BowVisualInput& in) {
	g_shotValid = false;
	g_drawValid = false;
	// The arrow hangs on the first-person root, not on the hand's bone: it is
	// placed in the world each frame, so it needs no bone to carry it, and the
	// root is not what the engine's equipping hangs weapons on. Its bound is
	// kept in view by the caller, like the hands'.
	NiAVObject* const holder = FirstPersonArmsNode();
	if (!in.active || holder == nullptr) {
		DropArrow(holder);
		ShowEngineArrow();
		return;
	}
	NiAVObject* const grip = FindFirstPersonNode("Weapon");
	NiAVObject* const arrowBone = FindFirstPersonNode("ArrowBone");
	NiAVObject* const bow = arrowBone != nullptr ? arrowBone->parent : nullptr;
	if (grip == nullptr || bow == nullptr || !LooksLikeObject(bow)) {
		Note("the drawing hand's grip or the bow's ArrowBone not found - nothing drawn by hand");
		DropArrow(holder);
		ShowEngineArrow();
		return;
	}
	if (g_hiddenArrowBone != arrowBone) {
		ShowEngineArrow();
		g_hiddenArrowBone = arrowBone;
	}
	arrowBone->flags = static_cast<UInt16>(arrowBone->flags | kAppCulledBit);

	// The bow's shot axis (its model's +x) and the arrow's rest on it. The bow
	// stays as the game holds it in the hand; the aim follows it (BowShotAxis).
	const float bowScale = bow->worldTransform.scale > 0.0f ? bow->worldTransform.scale : 1.0f;
	NiPoint3 axis = bow->worldTransform.rot * NiPoint3{1.0f, 0.0f, 0.0f};
	const float axisLength = math::Sqrt(axis.LengthSquared());
	const bool axisValid = axisLength > 1e-4f;
	if (axisValid) {
		axis = axis * (1.0f / axisLength);
		g_shot = axis;
		g_shotValid = true;
	}
	const NiPoint3 rest = bow->worldTransform.pos +
	                      bow->worldTransform.rot * (NiPoint3{0.0f, vr::kArrowRestOnBowY, vr::kArrowRestOnBowZ} * bowScale);
	void* const morpher = in.arrow == vr::ArrowShown::OnString || in.string != vr::StringSource::Engine
	                          ? StringOf(bow)
	                          : nullptr;

	vr::ArrowPose pose;
	vr::ArrowOnString onString;
	bool posed = false;
	if (in.arrow != vr::ArrowShown::None && EnsureArrow(holder)) {
		if (in.arrow == vr::ArrowShown::OnString && axisValid) {
			posed = vr::ArrowOnBowLine(rest, axis, grip->worldTransform.rot, grip->worldTransform.pos,
			                           g_arrow.lengthUnits, -g_string.restAlongBow * bowScale,
			                           g_string.travelUnits * bowScale, onString);
			if (posed) {
				pose = onString.pose;
				// The fist on the string: on the bow's line, no further back than
				// a full draw.
				MoveHandTo(in.rightHandBone, grip, onString.gripTarget);
			}
		} else if (in.arrow == vr::ArrowShown::InHand) {
			// Through the fist, straight ahead along the hand.
			const NiAVObject* const wrist = FindFirstPersonNode(in.rightHandBone);
			const NiAVObject* const knuckle = FindFirstPersonNode("Bip01 R Finger2");
			if (wrist != nullptr && knuckle != nullptr) {
				posed = vr::ArrowInFist(grip->worldTransform.pos, wrist->worldTransform.pos,
				                        knuckle->worldTransform.pos, grip->worldTransform.rot, g_arrow.lengthUnits,
				                        pose);
			} else {
				Note("the drawing hand or its middle finger not found - no arrow in the fist");
			}
		}
	}
	if (posed) {
		BonePose world;
		world.rot = pose.rot;
		world.pos = pose.pos;
		Place(g_arrow.node, world);
		g_arrow.node->flags = static_cast<UInt16>(g_arrow.node->flags & ~kAppCulledBit);
		// The engine's arrow, hidden, where the hand's is: whatever it reads
		// off it at the release is the arrow the wearer saw.
		Place(arrowBone, world);
	} else {
		HideArrow();
	}
	if (morpher != nullptr && in.string != vr::StringSource::Engine) {
		SetStringWeight(morpher, in.string == vr::StringSource::Hand && posed && in.arrow == vr::ArrowShown::OnString
		                             ? onString.weight
		                             : 0.0f);
	}
	if (posed && in.arrow == vr::ArrowShown::OnString) {
		g_drawValid = true;
		g_draw = onString.weight;
	}
	const NiPoint3 nock = posed ? pose.nock : grip->worldTransform.pos;
	// Each change of what is shown, the string's weight as it moves, and a
	// full draw reached, with where the nock is: the harness's window pictures
	// do not show the hands as the eyes do.
	static vr::ArrowShown s_shown = vr::ArrowShown::None;
	static vr::StringSource s_source = vr::StringSource::Engine;
	static bool s_full = false;
	static UInt32 s_stateLines = 32;
	static float s_loggedWeight = 0.0f;
	const bool full = posed && in.arrow == vr::ArrowShown::OnString && onString.atFullDraw;
	const float weightMoved = g_lastWeight - s_loggedWeight;
	if ((in.arrow != s_shown || in.string != s_source || full != s_full || weightMoved > 0.2f ||
	     weightMoved < -0.2f) &&
	    s_stateLines > 0) {
		--s_stateLines;
		s_loggedWeight = g_lastWeight;
		const NiPoint3 bowAt = InverseRotation(bow->worldTransform.rot) * (nock - bow->worldTransform.pos);
		OBVR_LOG("Bow by hand: the arrow %s, the string %s (weight %.2f)%s - the nock %.1f %.1f %.1f in the "
		         "bow's frame",
		         in.arrow == vr::ArrowShown::InHand     ? "in the fist along the hand"
		         : in.arrow == vr::ArrowShown::OnString ? "on the string along the bow"
		                                                : "not shown",
		         in.string == vr::StringSource::Hand   ? "pulled by the hand"
		         : in.string == vr::StringSource::Rest ? "at rest"
		                                               : "the game's",
		         static_cast<double>(g_lastWeight), full ? ", at full draw - stopped" : "",
		         static_cast<double>(bowAt.x), static_cast<double>(bowAt.y), static_cast<double>(bowAt.z));
	}
	s_shown = in.arrow;
	s_source = in.string;
	s_full = full;
}

}  // namespace obvr::game
