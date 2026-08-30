#include "render/CursorProbe.h"

#include "core/Log.h"
#include "game/GameAddresses.h"

namespace obvr::render {
namespace {

// A pointer that could plausibly be an object. Below 64K is the null page
// and its neighbourhood, where a stale small integer would sit.
bool Plausible(const void* pointer) {
	return reinterpret_cast<UInt32>(pointer) > 0xFFFF;
}

const UInt8* TileNode(const UInt8* tile) {
	if (tile == nullptr || !Plausible(tile)) {
		return nullptr;
	}
	const UInt8* node = *reinterpret_cast<const UInt8* const*>(tile + addr::kTileRenderNodeOffset);
	return Plausible(node) ? node : nullptr;
}

}  // namespace

void ProbeCursor(UInt32 frameIndex) {
	const UInt8* im =
	    *reinterpret_cast<const UInt8* const*>(addr::kInterfaceManagerPointer);
	if (!Plausible(im)) {
		return;
	}

	const UInt8* cursor =
	    *reinterpret_cast<const UInt8* const*>(im + addr::kInterfaceCursorTileOffset);
	const UInt8* active =
	    *reinterpret_cast<const UInt8* const*>(im + addr::kInterfaceActiveTileOffset);
	const UInt8* altActive =
	    *reinterpret_cast<const UInt8* const*>(im + addr::kInterfaceAltActiveTileOffset);

	const float* pos = reinterpret_cast<const float*>(im + addr::kInterfaceCursorPosOffset);
	const float* derived =
	    reinterpret_cast<const float*>(im + addr::kInterfaceCursorDerivedOffset);

	const UInt8* cursorNode = TileNode(cursor);
	const UInt8* activeNode = TileNode(active);

	const float* cursorTranslate =
	    cursorNode != nullptr
	        ? reinterpret_cast<const float*>(cursorNode + addr::kNiTranslateOffset)
	        : nullptr;
	const float* activeTranslate =
	    activeNode != nullptr
	        ? reinterpret_cast<const float*>(activeNode + addr::kNiTranslateOffset)
	        : nullptr;

	// One line per measurement, everything the offset hunt needs side by
	// side: where the manager says the cursor is (both triples), where the
	// sprite is actually planted, and which tile the game holds the mouse
	// over, with where that tile is planted.
	OBVR_LOG("Cursor probe: pos (%.1f, %.1f, %.1f) derived (%.1f, %.1f, %.1f) "
	         "sprite node (%.1f, %.1f) active=%p at (%.1f, %.1f) alt=%p (frame %u)",
	         static_cast<double>(pos[0]), static_cast<double>(pos[1]),
	         static_cast<double>(pos[2]), static_cast<double>(derived[0]),
	         static_cast<double>(derived[1]), static_cast<double>(derived[2]),
	         cursorTranslate != nullptr ? static_cast<double>(cursorTranslate[0]) : -1.0,
	         cursorTranslate != nullptr ? static_cast<double>(cursorTranslate[1]) : -1.0,
	         static_cast<const void*>(active),
	         activeTranslate != nullptr ? static_cast<double>(activeTranslate[0]) : -1.0,
	         activeTranslate != nullptr ? static_cast<double>(activeTranslate[1]) : -1.0,
	         static_cast<const void*>(altActive), frameIndex);
}

}  // namespace obvr::render
