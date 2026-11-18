#include "game/BodyDiagnostics.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/FirstPersonHide.h"
#include "game/NiMath.h"
#include "game/NodeName.h"

namespace obvr::game {

namespace {

constexpr UInt16 kHiddenBit = 0x0001;
constexpr UInt32 kDrawnBones = 6;  // a side's: five body bones and the first-person hand

bool Looks(const void* p) { return mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(p)); }

// The arm bones as last drawn, in the root's frame.
struct DrawnArms {
	NiMatrix33 was[2 * kDrawnBones] = {};
	bool seen[2 * kDrawnBones] = {};
};

DrawnArms& Drawn() {
	static DrawnArms d;
	return d;
}

float TurnDegrees(const NiMatrix33& a, const NiMatrix33& b) {
	const NiMatrix33 d = InverseMatrix(a) * b;
	float c = (d.data[0][0] + d.data[1][1] + d.data[2][2] - 1.0f) * 0.5f;
	c = c > 1.0f ? 1.0f : (c < -1.0f ? -1.0f : c);
	return 2.0f * math::Asin(math::Sqrt((1.0f - c) * 0.5f)) * math::kRadiansToDegrees;  // acos(c)
}

void BoundsWalk(const NiAVObject* node, const NiPoint3& camera, const char* mark, UInt32 depth, UInt32& lines) {
	if (!Looks(node) || depth > 40 || lines >= 48) {
		return;
	}
	UInt32 count = 0;
	NiAVObject* const* const children = NodeChildren(const_cast<NiAVObject*>(node), count);
	if (depth == 0 || (count == 0 && !node_name::StartsWith(node_name::SafeName(node), "Bip01"))) {
		const NiPoint3 c = node->worldBound.center - camera;
		const NiPoint3 bone = node->worldTransform.pos - camera;
		++lines;
		OBVR_LOG("Body bounds: at \"%s\" - \"%s\" under \"%s\"%s, its bound's middle %.1f %.1f %.1f from the camera "
		         "(%.1f away), radius %.1f; its node %.1f %.1f %.1f from the camera",
		         mark, node_name::SafeName(node), node_name::SafeName(node->parent),
		         (node->flags & kHiddenBit) != 0 ? " (hidden)" : "", static_cast<double>(c.x),
		         static_cast<double>(c.y), static_cast<double>(c.z),
		         static_cast<double>(math::Sqrt(c.LengthSquared())), static_cast<double>(node->worldBound.radius),
		         static_cast<double>(bone.x), static_cast<double>(bone.y), static_cast<double>(bone.z));
	}
	for (UInt32 i = 0; i < count; ++i) {
		BoundsWalk(children[i], camera, mark, depth + 1, lines);
	}
}

}  // namespace

void LogDrawnArms(const NiAVObject* root, const NiAVObject* const* rightArm, const NiAVObject* const* leftArm,
                  const NiAVObject* rightFpHand, const NiAVObject* leftFpHand, UInt32 frame) {
	if (!Looks(root)) {
		return;
	}
	DrawnArms& d = Drawn();
	const NiMatrix33 toRoot = InverseMatrix(root->worldTransform.rot);
	char line[320];
	int at = std::snprintf(line, sizeof(line), "Body drawn: f=%u", frame);
	for (UInt32 side = 0; side < 2; ++side) {
		const NiAVObject* const* const arm = side == 0 ? rightArm : leftArm;
		at += std::snprintf(line + at, sizeof(line) - at, " | %s", side == 0 ? "R" : "L");
		for (UInt32 k = 0; k < kDrawnBones && at > 0 && at < static_cast<int>(sizeof(line)); ++k) {
			const UInt32 slot = side * kDrawnBones + k;
			const NiAVObject* const node = k + 1 < kDrawnBones ? arm[k] : (side == 0 ? rightFpHand : leftFpHand);
			if (!Looks(node)) {
				at += std::snprintf(line + at, sizeof(line) - at, " -");
				d.seen[slot] = false;
				continue;
			}
			const NiMatrix33 now = toRoot * node->worldTransform.rot;
			const bool hidden = (node->flags & kHiddenBit) != 0 || node->localTransform.scale == 0.0f;
			at += std::snprintf(line + at, sizeof(line) - at, " %.1f%s",
			                    d.seen[slot] ? static_cast<double>(TurnDegrees(d.was[slot], now)) : 0.0,
			                    hidden ? "h" : "");
			d.was[slot] = now;
			d.seen[slot] = true;
		}
	}
	OBVR_LOG("%s", line);
}

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
			const char* const n = node_name::SafeName(nodes[i]);
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

void WalkBodyBounds(const NiAVObject* root, const NiPoint3& camera, const char* mark) {
	UInt32 lines = 0;
	BoundsWalk(root, camera, mark, 0, lines);
}

}  // namespace obvr::game
