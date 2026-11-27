#include "game/ShieldShape.h"

#include "core/AddressSpace.h"
#include "game/GameTypes.h"
#include "game/HandBodyLogic.h"
#include "game/NearbyItems.h"

namespace obvr::game {
namespace {

struct ScatterWalk {
	NiMatrix33 rot;
	NiPoint3 pos;
	DiscScatter scatter;
};

void TakeIntoScatter(void* context, const NiPoint3& world) {
	ScatterWalk& w = *static_cast<ScatterWalk*>(context);
	w.scatter.Add(ToBodyFrame(w.rot, w.pos, world));
}

struct ExtentWalk {
	NiMatrix33 rot;
	NiPoint3 pos;
	NiPoint3 centre;
	NiPoint3 axes[3];
	DiscExtent extent;
	OutlineBins outline;  // the silhouette in the disc's plane (ShieldOutlineLogic.h)
};

void TakeIntoExtent(void* context, const NiPoint3& world) {
	ExtentWalk& w = *static_cast<ExtentWalk*>(context);
	const NiPoint3 p = ToBodyFrame(w.rot, w.pos, world);
	w.extent.Add(p, w.centre, w.axes);
	const NiPoint3 d = p - w.centre;
	w.outline.Add(DotOf3(d, w.axes[0]), DotOf3(d, w.axes[1]));
}

struct ShaftWalk {
	NiMatrix33 rot;
	NiPoint3 pos;
	NiPoint3 centre;
	NiPoint3 axis;
	TorchExtent extent;
};

void TakeIntoShaft(void* context, const NiPoint3& world) {
	ShaftWalk& w = *static_cast<ShaftWalk*>(context);
	w.extent.Add(ToBodyFrame(w.rot, w.pos, world), w.centre, w.axis);
}

}  // namespace

bool ReadShieldDisc(const NiAVObject* node, const NiMatrix33& handRot, const NiPoint3& handPos, ShieldDisc& out,
                    const char** why) {
	out = ShieldDisc{};
	const char* reason = "";
	if (node == nullptr || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node))) {
		reason = "no node";
	} else {
		ScatterWalk first;
		first.rot = handRot;
		first.pos = handPos;
		if (!ForEachWorldVertex(node, TakeIntoScatter, &first)) {
			reason = "no readable geometry under the twist bone";
		} else {
			ExtentWalk second;
			second.rot = handRot;
			second.pos = handPos;
			float spread[3];
			if (!DiscAxesFrom(first.scatter, second.centre, second.axes, spread)) {
				reason = "the vertices have no spread";
			} else if (!ForEachWorldVertex(node, TakeIntoExtent, &second)) {
				reason = "the model changed between the two readings";
			} else if (!ShieldDiscFrom(second.centre, second.axes, second.extent, out)) {
				reason = "what was read is no shield's disc (its size or its thickness)";
			} else {
				const DiscExtent& e = second.extent;
				out.outline = OutlineFromBins(second.outline, e.low[0], e.high[0], (e.low[1] + e.high[1]) * 0.5f);
			}
		}
	}
	if (why != nullptr) {
		*why = reason;
	}
	return out.valid;
}

bool ReadTorchShaft(const NiAVObject* node, const NiMatrix33& handRot, const NiPoint3& handPos, TorchShaft& out,
                    const char** why) {
	out = TorchShaft{};
	const char* reason = "";
	if (node == nullptr || !mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(node))) {
		reason = "no node";
	} else {
		ScatterWalk first;
		first.rot = handRot;
		first.pos = handPos;
		if (!ForEachWorldVertex(node, TakeIntoScatter, &first)) {
			reason = "no readable geometry under the Torch node";
		} else {
			ShaftWalk second;
			second.rot = handRot;
			second.pos = handPos;
			NiPoint3 axes[3];
			float spread[3];
			if (!DiscAxesFrom(first.scatter, second.centre, axes, spread)) {
				reason = "the vertices have no spread";
			} else {
				second.axis = axes[0];
				if (!ForEachWorldVertex(node, TakeIntoShaft, &second)) {
					reason = "the model changed between the two readings";
				} else if (!TorchShaftFrom(second.centre, second.axis, spread, second.extent, out)) {
					reason = "what was read is no torch's shaft (its length or its slenderness)";
				}
			}
		}
	}
	if (why != nullptr) {
		*why = reason;
	}
	return out.valid;
}

}  // namespace obvr::game
