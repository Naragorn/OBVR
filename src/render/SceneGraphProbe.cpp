#include "render/SceneGraphProbe.h"

#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::render {
namespace {

using addr::kCullingProcessListOffsetDeadEnd;
using addr::kNiCameraFrustumOffset;
using addr::kNiChildCountOffset;
using addr::kNiFlagsOffset;
using addr::kNiWorldBoundOffset;
using addr::kRendererAccumulatorOffset;
using addr::kRendererPointer;
using addr::kSceneGraphCameraOffset;
using addr::kSceneGraphCullingOffset;
using addr::kWorldSceneGraphPointer;

// A pointer that could plausibly be an object, on the same terms as the
// cursor probe: below 64K is the null page and its neighbourhood, where a
// stale small integer would sit.
bool Plausible(const void* pointer) {
	return reinterpret_cast<UInt32>(pointer) > 0xFFFF;
}

const UInt8* Deref(const UInt8* base, UInt32 offset) {
	if (base == nullptr) {
		return nullptr;
	}
	const UInt8* value = *reinterpret_cast<const UInt8* const*>(base + offset);
	return Plausible(value) ? value : nullptr;
}

// The first words of a structure, raw.
//
// The layout of the culled list is exactly what is not known: NetImmerse
// arrays of this era carry a base pointer with a size and a capacity beside
// it, but whether those are 16- or 32-bit, and in which order, differs
// between versions - and guessing wrong would print a confident number that
// means nothing. Printing the words and reading the DIFFERENCE between a
// world frame and a menu frame identifies the count field without needing to
// know it in advance: it is the one that collapses.
void LogWords(const char* what, const UInt8* base, UInt32 count) {
	if (base == nullptr) {
		OBVR_LOG("Scene graph probe: %s is null", what);
		return;
	}
	const UInt32* words = reinterpret_cast<const UInt32*>(base);
	OBVR_LOG("Scene graph probe: %s at %p = %08X %08X %08X %08X %08X %08X", what, base, words[0],
	         count > 1 ? words[1] : 0, count > 2 ? words[2] : 0, count > 3 ? words[3] : 0,
	         count > 4 ? words[4] : 0, count > 5 ? words[5] : 0);
}

}  // namespace

void ProbeSceneGraph(UInt32 frameIndex, const char* occasion) {
	const UInt8* const scene =
		*reinterpret_cast<const UInt8* const*>(kWorldSceneGraphPointer);
	if (!Plausible(scene)) {
		OBVR_LOG("Scene graph probe: no world scene graph at %08X (%s, frame %u)",
		         kWorldSceneGraphPointer, occasion, frameIndex);
		return;
	}

	const UInt8* const camera = Deref(scene, kSceneGraphCameraOffset);
	const UInt8* const culling = Deref(scene, kSceneGraphCullingOffset);
	// Kept although the offset is a known dead end: it reads null on world
	// frames too, and printing that beside a working render is what keeps the
	// next reader from taking xOBSE's header at its word the way this probe
	// first did. The disassembly since explained it - the field is null by
	// construction and the render path is built for that - so this line is
	// now a regression check rather than a question.
	const UInt8* const culledList =
		culling != nullptr ? Deref(culling, kCullingProcessListOffsetDeadEnd) : nullptr;

	// The four things that can empty a render which cannot return early.
	//
	// The render walks the graph itself; there is no list to be short. So the
	// walk is where it must stop, and the walk is NiAVObject::Cull: four
	// instructions that test bit 0 of the flags word and turn back if it is
	// set, leaving the accumulator to start and finish around nothing. That
	// is the measured shape exactly - constant setup, no draws - which makes
	// the flags word the first suspect. The world bound is the second: it is
	// tested against the frustum planes, and a zero radius or a stale centre
	// culls everything, and it is rebuilt by the update pass a menu stops.
	// The frustum is the third, rebuilt from the camera on every walk. The
	// accumulator is the fourth: without one there is nothing to register
	// with. Exactly one of these should differ between a world frame and a
	// menu frame, and that one is the cause.
	const UInt16 flags = *reinterpret_cast<const UInt16*>(scene + kNiFlagsOffset);
	const UInt16 childCount = *reinterpret_cast<const UInt16*>(scene + kNiChildCountOffset);
	const float* const bound = reinterpret_cast<const float*>(scene + kNiWorldBoundOffset);
	const UInt8* const renderer =
		*reinterpret_cast<const UInt8* const*>(kRendererPointer);
	const UInt8* const accumulator =
		Plausible(renderer) ? Deref(renderer, kRendererAccumulatorOffset) : nullptr;

	OBVR_LOG("Scene graph probe: flags=%04X (app culled=%u) children=%u bound centre "
	         "(%.1f, %.1f, %.1f) radius %.1f accumulator=%p",
	         flags, static_cast<UInt32>(flags & 1u), childCount, static_cast<double>(bound[0]),
	         static_cast<double>(bound[1]), static_cast<double>(bound[2]),
	         static_cast<double>(bound[3]), accumulator);

	if (camera != nullptr) {
		const float* const frustum = reinterpret_cast<const float*>(camera + kNiCameraFrustumOffset);
		OBVR_LOG("Scene graph probe: frustum l=%.3f r=%.3f t=%.3f b=%.3f near=%.1f far=%.1f",
		         static_cast<double>(frustum[0]), static_cast<double>(frustum[1]),
		         static_cast<double>(frustum[2]), static_cast<double>(frustum[3]),
		         static_cast<double>(frustum[4]), static_cast<double>(frustum[5]));
	}

	// NiNode keeps its children in an array; the scene graph having children
	// at all is the first thing a vanished world would show up in.
	OBVR_LOG("Scene graph probe: %s, frame %u - scene=%p camera=%p culling=%p list=%p",
	         occasion, frameIndex, scene, camera, culling, culledList);
	LogWords("culling process", culling, 6);
	LogWords("culled list", culledList, 6);
}

}  // namespace obvr::render
