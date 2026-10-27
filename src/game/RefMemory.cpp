#include "game/RefFingerprint.h"
#include "platform/Win32Min.h"

namespace obvr::game {

bool RefBytesReadable(UInt32 address, UInt32 bytes) {
	MemoryBasicInfo region{};
	if (!QueryMemory(reinterpret_cast<const void*>(address), region)) {
		return false;
	}
	return MemoryRangeReadable(reinterpret_cast<UInt32>(region.baseAddress), region.regionSize, region.state,
	                           region.protect, address, bytes);
}

}  // namespace obvr::game
