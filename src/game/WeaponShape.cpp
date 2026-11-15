#include "game/WeaponShape.h"

#include <cstdio>
#include <vector>

#include "game/GameTypes.h"
#include "game/NearbyItems.h"
#include "game/SkinRead.h"

namespace obvr::game {
namespace {

struct Gather {
	std::vector<NiPoint3> pts;
};

void TakeInNode(void* context, const NiPoint3& inNode) {
	static_cast<Gather*>(context)->pts.push_back(inNode);
}

}  // namespace

bool ReadWeaponShape(const NiAVObject* node, WeaponShape& out, const char** why) {
	out = WeaponShape{};
	// every vertex in the node's own space, through the local transforms
	// under it (game/NearbyItems.h, ForEachVertexUnder) - never through the
	// world data, which a weapon equipped this frame does not have yet
	Gather g;
	if (!ForEachVertexUnder(node, TakeInNode, &g) || g.pts.empty()) {
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
		*why = "too few vertices past the node, shorter than a blade or longer than any weapon";
		return false;
	}
	return true;
}

}  // namespace obvr::game
