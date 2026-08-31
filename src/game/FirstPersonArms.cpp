#include "game/FirstPersonArms.h"

#include "core/Log.h"
#include "core/MathFns.h"
#include "core/Rotation.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"

namespace obvr::game {
namespace {

// The same test every raw pointer in this project gets before it is followed:
// inside the 32-bit user space, past the pages that catch null-offset reads,
// and four-byte aligned.
bool LooksLikeObject(const void* pointer) {
	const UInt32 address = reinterpret_cast<UInt32>(pointer);
	return address >= 0x00010000u && address <= 0x7FFFFFFFu && (address & 3u) == 0u;
}

// Whether a pointer leads to something that reads like a node's name.
//
// This is what makes a single-sourced offset safe to write through. A scene
// graph node carries a name; the field at that offset either leads to a short
// run of printable characters or it does not, and only one of those is a node.
// It cannot prove the node is the RIGHT one - but a wrong offset inside
// PlayerCharacter lands on a float, a count or a pointer to something else, and
// almost none of those spell a word.
bool NameLooksReal(const char* name) {
	if (!LooksLikeObject(name)) {
		return false;
	}
	for (UInt32 at = 0; at < 64; ++at) {
		const char c = name[at];
		if (c == '\0') {
			return at > 0;
		}
		if (c < 0x20 || c > 0x7E) {
			return false;
		}
	}
	return false;
}

bool SameRotation(const NiMatrix33& a, const NiMatrix33& b) {
	for (int row = 0; row < 3; ++row) {
		for (int col = 0; col < 3; ++col) {
			const float difference = a.data[row][col] - b.data[row][col];
			if (difference > 1.0e-6f || difference < -1.0e-6f) {
				return false;
			}
		}
	}
	return true;
}

// What was written last frame, and what was underneath it. See the header for
// why both are needed.
NiMatrix33 g_wrote{};
NiMatrix33 g_base{};
NiAVObject* g_held = nullptr;

bool g_reported = false;
bool g_rewriteKnown = false;
bool g_tookReported = false;

}  // namespace

NiAVObject* FirstPersonArmsNode() {
	auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
	if (!LooksLikeObject(player)) {
		return nullptr;
	}

	auto* const node =
		*reinterpret_cast<NiAVObject* const*>(player + addr::kPlayerFirstPersonNodeOffset);
	if (!LooksLikeObject(node)) {
		return nullptr;
	}

	const auto* const name =
		*reinterpret_cast<const char* const*>(reinterpret_cast<const UInt8*>(node) +
	                                         addr::kNiObjectNameOffset);
	if (!NameLooksReal(name)) {
		if (!g_reported) {
			g_reported = true;
			OBVR_LOG("First person arms: %08X does not read like a node - no name at +%02X, so "
			         "the weapon is left where the game puts it",
			         reinterpret_cast<UInt32>(node), addr::kNiObjectNameOffset);
		}
		return nullptr;
	}

	if (!g_reported) {
		g_reported = true;
		OBVR_LOG("First person arms: node %08X named \"%s\" - the weapon can follow the gaze",
		         reinterpret_cast<UInt32>(node), name);
	}
	return node;
}

bool TurnFirstPersonArms(float radians) {
	NiAVObject* const node = FirstPersonArmsNode();
	if (node == nullptr) {
		return false;
	}

	// Put back what was underneath, if the engine has not already replaced it.
	//
	// Which of the two happened is decided by looking rather than by assuming:
	// the value written last frame is still there only if nothing overwrote it,
	// and in that case applying another turn on top would accumulate one per
	// frame until the arms span.
	if (g_held == node && SameRotation(node->localTransform.rot, g_wrote)) {
		node->localTransform.rot = g_base;
		if (!g_rewriteKnown) {
			g_rewriteKnown = true;
			OBVR_LOG("First person arms: the engine leaves this node alone between frames, so "
			         "the turn is rebuilt from the kept base each time");
		}
	} else if (!g_rewriteKnown && g_held == node) {
		g_rewriteKnown = true;
		OBVR_LOG("First person arms: the engine rewrites this node every frame, so the turn "
		         "goes on top of whatever the animation left");
	}

	g_base = node->localTransform.rot;

	// About the vertical, in the parent's space, so the arms swing the way the
	// head turned rather than rolling with whatever the animation is doing.
	const Heading heading{math::Cos(radians), math::Sin(radians)};
	node->localTransform.rot = RotationFromHeading(heading) * g_base;

	g_wrote = node->localTransform.rot;
	g_held = node;

	// AND MADE TO TAKE. A local transform is only a request: what gets drawn is
	// the world transform, and that follows only when something recomputes it.
	// UpdateNodeTransforms' own header says so - "the game does that once,
	// after its own camera write" - and by render time that once has long
	// happened. Without this the rotation sits in the node changing nothing,
	// which is exactly what the headset reported twice.
	const NiMatrix33 worldBefore = node->worldTransform.rot;
	UpdateNodeTransforms(node);

	// Only once there is a real angle to see the effect of. Reported on a turn
	// of nearly nothing, this would say "did NOT move" for the honest reason
	// that nothing was asked of it, and that reading would send the search off
	// in the wrong direction.
	if (!g_tookReported && (radians > 0.1f || radians < -0.1f)) {
		g_tookReported = true;
		// Whether the recompute reached the world transform is the one thing
		// that separates "the write does not take" from "the weapon does not
		// hang off this node". If this says the world transform moved and the
		// bow still does not, the node is the wrong one and the weapon node
		// inside the skeleton is next.
		OBVR_LOG("First person arms: turning by %.1f degrees %s the node's world transform",
		         static_cast<double>(radians * 57.2957795f),
		         SameRotation(worldBefore, node->worldTransform.rot) ? "did NOT move"
		                                                            : "moved");
	}
	return true;
}

float FirstPersonArmsWorldYaw() {
	NiAVObject* const node = FirstPersonArmsNode();
	if (node == nullptr) {
		return 0.0f;
	}

	// Read out of the WORLD transform, which is what the renderer uses. The
	// local one is only a request, as this file learned the hard way.
	const NiPoint3 forward = ForwardOf(node->worldTransform.rot);
	return math::Atan2(forward.x, forward.y);
}

void ReleaseFirstPersonArms() {
	if (g_held == nullptr) {
		return;
	}

	// Only if it is still what was written. Anything else means the engine has
	// moved on and putting an old rotation back would be the fault rather than
	// the fix.
	if (LooksLikeObject(g_held) && SameRotation(g_held->localTransform.rot, g_wrote)) {
		g_held->localTransform.rot = g_base;
	}
	g_held = nullptr;
}

}  // namespace obvr::game
