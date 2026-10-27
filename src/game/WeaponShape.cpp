#include "game/WeaponShape.h"

#include <cstdio>
#include <vector>

#include "game/BodyViewLogic.h"
#include "game/BoneMirror.h"
#include "game/GameTypes.h"
#include "game/NearbyItems.h"
#include "game/SkinRead.h"

namespace obvr::game {
namespace {

struct Gather {
	NiTransform toNode;
	std::vector<NiPoint3> pts;
};

void TakeIntoNode(void* context, const NiPoint3& world) {
	Gather& g = *static_cast<Gather*>(context);
	g.pts.push_back(TransformPoint(g.toNode, world));
}

}  // namespace

bool ReadWeaponShape(const NiAVObject* node, WeaponShape& out, const char** why) {
	out = WeaponShape{};
	// every vertex in the world, as the torch's shaft and the shield's disc
	// are read (game/NearbyItems.h), then into the node's space
	Gather g;
	g.toNode = InverseTransform(node->worldTransform);
	if (!ForEachWorldVertex(node, TakeIntoNode, &g) || g.pts.empty()) {
		// what is under it, for the log: how many shapes, how many hidden
		std::vector<ShapeUnder> shapes;
		CollectShapesUnder(node, shapes);
		UInt32 hidden = 0;
		for (const ShapeUnder& under : shapes) {
			hidden += under.hidden ? 1u : 0u;
		}
		static char s_why[160];
		std::snprintf(s_why, sizeof(s_why), "no readable geometry under the weapon's node: %u shapes, %u hidden, %u vertices taken",
		              static_cast<UInt32>(shapes.size()), hidden, static_cast<UInt32>(g.pts.size()));
		*why = s_why;
		return false;
	}
	if (!FitWeaponShape(g.pts.data(), static_cast<UInt32>(g.pts.size()), NiPoint3{0.0f, 1.0f, 0.0f}, out)) {
		*why = "too few vertices past the node, or shorter than a blade";
		return false;
	}
	return true;
}

}  // namespace obvr::game
