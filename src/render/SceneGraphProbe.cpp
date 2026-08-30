#include "render/SceneGraphProbe.h"

#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::render {
namespace {

using addr::kCullingProcessListOffsetDeadEnd;
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

void ProbeSceneGraph(UInt32 frameIndex, bool menuIsUp) {
	const UInt8* const scene =
		*reinterpret_cast<const UInt8* const*>(kWorldSceneGraphPointer);
	if (!Plausible(scene)) {
		OBVR_LOG("Scene graph probe: no world scene graph at %08X (%s, frame %u)",
		         kWorldSceneGraphPointer, menuIsUp ? "menu" : "world", frameIndex);
		return;
	}

	const UInt8* const camera = Deref(scene, kSceneGraphCameraOffset);
	const UInt8* const culling = Deref(scene, kSceneGraphCullingOffset);
	// Kept although the offset is a known dead end: it reads null on world
	// frames too, and printing that beside a working render is what keeps the
	// next reader from taking xOBSE's header at its word the way this probe
	// first did.
	const UInt8* const culledList =
		culling != nullptr ? Deref(culling, kCullingProcessListOffsetDeadEnd) : nullptr;

	// NiNode keeps its children in an array; the scene graph having children
	// at all is the first thing a vanished world would show up in.
	OBVR_LOG("Scene graph probe: %s frame %u - scene=%p camera=%p culling=%p list=%p",
	         menuIsUp ? "menu" : "world", frameIndex, scene, camera, culling, culledList);
	LogWords("culling process", culling, 6);
	LogWords("culled list", culledList, 6);
}

}  // namespace obvr::render
