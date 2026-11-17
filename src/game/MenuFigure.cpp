#include "game/MenuFigure.h"

#include <cstring>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/FirstPersonHide.h"
#include "game/GameAddresses.h"
#include "game/GameTypes.h"
#include "render/InterfaceRenderHook.h"

namespace obvr::game {
namespace {

constexpr UInt32 kMostPoints = 512;
constexpr UInt32 kMostDepth = 24;
// A bound this small is a piece carried on the figure - a weapon, a shield, a
// torch - that reaches past the nodes; the body's own bound is far larger
// and would stand for the whole sphere round it (the woman's 91 units,
// paperdoll-size.txt 2026-10-26).
constexpr float kPieceRadiusMost = 50.0f;
// Room round the figure, of its height: the nodes stand inside the skin.
constexpr float kFigureMargin = 0.05f;

struct Collect {
	render::FigurePoint points[kMostPoints];
	const char* names[kMostPoints] = {};
	UInt32 count = 0;
	NiPoint3 sceneOrigin{0.0f, 0.0f, 0.0f};
	// The model's own bound: a node outside it is no part of the figure (the
	// empty "Camera01" under the head sits at the scene's origin, 198 units
	// off: paperdoll-size.txt 2026-10-26).
	NiPoint3 boundCentre{0.0f, 0.0f, 0.0f};
	float boundRadius = 0.0f;
};

void Add(Collect& c, const NiPoint3& at, float radius, const char* name) {
	const NiPoint3 off = at - c.boundCentre;
	const float reach = c.boundRadius * 1.05f;
	if (!(off.x * off.x + off.y * off.y + off.z * off.z <= reach * reach)) {
		return;
	}
	if (c.count < kMostPoints) {
		c.names[c.count] = name;
		c.points[c.count++] = render::FigurePoint{at - c.sceneOrigin, radius};
	}
}

// The bones that outline a human figure, with how far the body reaches past
// each (the head's crown over its bone); -1 for any other node.
float BoneReach(const char* name) {
	if (name == nullptr || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(name))) {
		return -1.0f;
	}
	static const char* const kBones[] = {"Bip01 Head",      "Bip01 Spine2",    "Bip01 Pelvis",   "Bip01 L UpperArm",
	                                     "Bip01 R UpperArm", "Bip01 L Forearm", "Bip01 R Forearm", "Bip01 L Hand",
	                                     "Bip01 R Hand",     "Bip01 L Thigh",   "Bip01 R Thigh",  "Bip01 L Calf",
	                                     "Bip01 R Calf",     "Bip01 L Foot",    "Bip01 R Foot",   "Bip01 L Toe0",
	                                     "Bip01 R Toe0"};
	for (UInt32 i = 0; i < sizeof(kBones) / sizeof(kBones[0]); ++i) {
		if (std::strcmp(name, kBones[i]) == 0) {
			return i == 0 ? 14.0f : 4.0f;
		}
	}
	return -1.0f;
}

void Walk(Collect& c, const NiAVObject* object, UInt32 depth) {
	if (object == nullptr || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(object)) || c.count >= kMostPoints) {
		return;
	}
	UInt32 count = 0;
	NiAVObject* const* children = NodeChildren(object, count);
	if (children == nullptr) {
		// Geometry: a small piece counts with its bound, the body by its bones.
		const float r = object->worldBound.radius;
		if (r > 0.0f && r < kPieceRadiusMost) {
			Add(c, object->worldBound.center, r, object->name);
		}
		return;
	}
	// A node counts when it is one of the body's own bones: the skeleton
	// carries bones that skin nothing on a human ("Bip01 Wing 05 L" stood 75
	// units off the body, tablet-open.txt 2026-11-17), and empty helpers.
	const float bone = BoneReach(object->name);
	if (bone >= 0.0f) {
		Add(c, object->worldTransform.pos, bone, object->name);
	}
	if (depth >= kMostDepth) {
		return;
	}
	for (UInt32 i = 0; i < count; ++i) {
		Walk(c, children[i], depth + 1);
	}
}

}  // namespace

bool ReadMenuFigure(float aspect, render::FigureRect& out) {
	const float yScale = render::MenuModelYScale();
	if (!(yScale > 0.01f) || !(aspect > 0.1f)) {
		return false;
	}
	const UInt32 manager = *reinterpret_cast<const UInt32*>(addr::kInterfaceManagerPointer);
	if (!mem::LooksLikeObjectAddress(manager)) {
		return false;
	}
	const UInt32 holderAddress = *reinterpret_cast<const UInt32*>(manager + 0x60);
	if (!mem::LooksLikeObjectAddress(holderAddress)) {
		return false;
	}
	const NiAVObject* const holder = reinterpret_cast<const NiAVObject*>(holderAddress);
	UInt32 count = 0;
	NiAVObject* const* children = NodeChildren(holder, count);
	static Collect c;  // 2.3 KB, one walk at a time on the game's thread
	c.count = 0;
	// The view stands at the scene's origin: the holder's parent, which places
	// it unturned (its world less its local place).
	c.sceneOrigin = holder->worldTransform.pos - holder->localTransform.pos;
	for (UInt32 i = 0; i < count && children != nullptr; ++i) {
		const NiAVObject* const model = children[i];
		if (model == nullptr || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(model))) {
			continue;
		}
		c.boundCentre = model->worldBound.center;
		c.boundRadius = model->worldBound.radius;
		if (c.boundRadius > 20.0f && c.boundRadius < 400.0f) {
			Walk(c, model, 0);
		}
	}
	const float tanHalfY = 1.0f / yScale;
	const bool ok = render::FigureScreenRect(c.points, c.count, tanHalfY * aspect, tanHalfY, kFigureMargin, out);
	static UInt32 s_lines = 3;
	if (s_lines > 0 && c.count > 0) {
		--s_lines;
		UInt32 far = 0;
		for (UInt32 i = 1; i < c.count; ++i) {
			if (c.points[i].at.x / c.points[i].at.y > c.points[far].at.x / c.points[far].at.y) far = i;
		}
		OBVR_LOG("Tablet: the figure from %u point(s) - %.3f,%.3f..%.3f,%.3f of the screen (%s); the scene's origin "
		         "%.1f %.1f %.1f, the model's bound at %.1f %.1f %.1f radius %.1f; the rightmost point %.1f %.1f %.1f r %.1f; "
		         "tan %.3f x %.3f; it is \"%s\"",
		         c.count, static_cast<double>(out.left), static_cast<double>(out.top), static_cast<double>(out.right),
		         static_cast<double>(out.bottom), ok ? "read" : "refused", static_cast<double>(c.sceneOrigin.x),
		         static_cast<double>(c.sceneOrigin.y), static_cast<double>(c.sceneOrigin.z),
		         static_cast<double>(c.boundCentre.x), static_cast<double>(c.boundCentre.y),
		         static_cast<double>(c.boundCentre.z), static_cast<double>(c.boundRadius),
		         static_cast<double>(c.points[far].at.x), static_cast<double>(c.points[far].at.y),
		         static_cast<double>(c.points[far].at.z), static_cast<double>(c.points[far].radius),
		         static_cast<double>(tanHalfY * aspect), static_cast<double>(tanHalfY),
		         c.names[far] != nullptr && mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(c.names[far])) ? c.names[far] : "?");
	}
	return ok;
}

}  // namespace obvr::game
