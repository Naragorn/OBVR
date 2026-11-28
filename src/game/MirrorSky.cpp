#include "game/MirrorSky.h"

#include <cmath>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/GameCamera.h"
#include "game/GameTypes.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// The two follower roots RenderScene places on the camera node each call
// (0x0040C95F-0x0040C9F0: the scene graph's first child's local translation
// +0x54..+0x5C copied into theirs, then 0x00707370(0.0f, 0) on each). Made in
// 0x00406950: [0x00B333D8] named "Sky" (the string at 0x00A30044), handed to
// Sky::Init 0x005411D0 as the Sky's root (Sky+0x04; "Moons Root" hangs under
// it), and [0x00B333DC] named "Weather" (the precipitation's parent, 0x0053D8F0:
// "Can't find Weather Root Node"). Both attached to the node 0x007C7340 makes
// (0x130 bytes, under the "World" root 0x00B333CC).
constexpr UInt32 kSkyRootPointer = 0x00B333D8;
constexpr UInt32 kWeatherRootPointer = 0x00B333DC;

bool Readable(UInt32 address) { return mem::LooksLikeObjectAddress(address); }
bool Readable(const void* p) { return Readable(reinterpret_cast<UInt32>(p)); }
UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

NiAVObject* RootAt(UInt32 pointer) {
	const UInt32 root = Read(pointer);
	return Readable(root) ? reinterpret_cast<NiAVObject*>(root) : nullptr;
}

// The node RenderScene copies from: the scene graph's first child, read as
// 0x0040C960-0x0040C972 reads it (the count word +0xB6, the list +0xB0).
NiAVObject* CameraNode() {
	const UInt32 graph = Read(addr::kWorldSceneGraphPointer);
	if (!Readable(graph) || *reinterpret_cast<const UInt16*>(graph + addr::kNiChildCountOffset) == 0) {
		return nullptr;
	}
	const UInt32 list = Read(graph + addr::kNiChildrenOffset);
	if (!Readable(list)) {
		return nullptr;
	}
	const UInt32 first = Read(list);
	return Readable(first) ? reinterpret_cast<NiAVObject*>(first) : nullptr;
}

float Distance(const NiPoint3& a, const NiPoint3& b) { return std::sqrt((a - b).LengthSquared()); }

void SayRefusal(MirrorSkyRefusal r) {
	static bool s_known = false;
	static MirrorSkyRefusal s_said = MirrorSkyRefusal::None;
	if (s_known && r == s_said) {
		return;
	}
	const bool first = !s_known;
	s_known = true;
	s_said = r;
	if (r != MirrorSkyRefusal::None) {
		OBVR_LOG("Mirror sky: not placed on the mirror's camera - %s; the sky stays centred on the eye in the mirror",
		         MirrorSkyRefusalText(r));
	} else if (!first) {
		OBVR_LOG("Mirror sky: placed on the mirror's camera again");
	}
}

// The first passes of a session: where the roots stood in the render against
// the mirror's camera, where the engine's own copy would have put them, the
// Sky root's world bound (its radius bounds the dome's), and the cost.
void TracePlaced(const MirrorSkySwitch& s, const NiPoint3 (&during)[2], bool nodeMoved, double ms) {
	static UInt32 s_linesLeft = 4;
	if (s_linesLeft == 0) {
		return;
	}
	--s_linesLeft;
	const float radius = s.roots[0] != nullptr ? s.roots[0]->worldBound.radius : 0.0f;
	OBVR_LOG("Mirror sky: in the mirror's render the Sky root stood %.2f units from the mirror's camera, the Weather "
	         "root %.2f (%s); the engine's own copy would have put them on the camera node, %.1f units from it; the Sky "
	         "root's world bound radius %.1f; the camera node %s in the render; put back in %.3f ms",
	         static_cast<double>(Distance(during[0], s.camera)),
	         static_cast<double>(s.roots[1] != nullptr ? Distance(during[1], s.camera) : -1.0f),
	         s.roots[1] != nullptr ? "both placed" : "no Weather root", static_cast<double>(Distance(s.oldCentre, s.camera)),
	         static_cast<double>(radius), nodeMoved ? "MOVED (its world put back)" : "not moved", ms);
}

}  // namespace

void PlaceSkyForMirror(MirrorSkySwitch& s, const NiPoint3& camera) {
	s = MirrorSkySwitch{};
	s.camera = camera;
	s.node = CameraNode();
	s.roots[0] = RootAt(kSkyRootPointer);
	s.roots[1] = RootAt(kWeatherRootPointer);
	MirrorSkyInputs in;
	in.cameraNode = s.node != nullptr;
	in.skyRoot = s.roots[0] != nullptr;
	in.skyParent = in.skyRoot && Readable(s.roots[0]->parent);
	if (in.skyParent) {
		in.skyParentWorld = s.roots[0]->parent->worldTransform;
	}
	in.camera = camera;
	s.plan = PlanMirrorSky(in);
	SayRefusal(s.plan.refusal);
	if (s.plan.refusal != MirrorSkyRefusal::None) {
		return;
	}
	s.nodeLocal = s.node->localTransform.pos;
	s.nodeWorld = s.node->worldTransform.pos;
	for (int i = 0; i < 2; ++i) {
		if (s.roots[i] != nullptr) {
			s.rootLocal[i] = s.roots[i]->localTransform.pos;
		}
	}
	s.oldCentre = in.skyParent ? in.skyParentWorld.pos +
	                                 in.skyParentWorld.rot * (s.nodeLocal * in.skyParentWorld.scale)
	                           : s.nodeLocal;
	// Only the 12 bytes RenderScene copies; the node's world transform is not
	// updated, so the camera under it stays where the pass set it.
	s.node->localTransform.pos = s.plan.nodeLocal;
	s.placed = true;
}

void PutSkyBackAfterMirror(MirrorSkySwitch& s) {
	if (!s.placed) {
		return;
	}
	const long long start = ReadPerformanceCounter();
	NiPoint3 during[2] = {};
	for (int i = 0; i < 2; ++i) {
		if (s.roots[i] != nullptr) {
			during[i] = s.roots[i]->worldTransform.pos;
		}
	}
	const NiPoint3 nodeWorld = s.node->worldTransform.pos;
	const bool nodeMoved = nodeWorld.x != s.nodeWorld.x || nodeWorld.y != s.nodeWorld.y || nodeWorld.z != s.nodeWorld.z;
	s.node->localTransform.pos = s.nodeLocal;
	if (nodeMoved) {
		// Something in the render made the node's world from the local set
		// here: made again from the saved one (the camera under it follows;
		// the pass puts the camera's own transform back right after).
		UpdateNodeTransforms(s.node);
	}
	for (int i = 0; i < 2; ++i) {
		if (s.roots[i] != nullptr) {
			s.roots[i]->localTransform.pos = s.rootLocal[i];
			UpdateNodeTransforms(s.roots[i]);  // as RenderScene updates them, 0x00707370(0.0f, 0)
		}
	}
	static const long long ticksPerSecond = ReadPerformanceFrequency();
	const double ms = ticksPerSecond > 0 ? 1000.0 * static_cast<double>(ReadPerformanceCounter() - start) /
	                                           static_cast<double>(ticksPerSecond)
	                                     : 0.0;
	TracePlaced(s, during, nodeMoved, ms);
	s.placed = false;
}

}  // namespace obvr::game
