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

bool RefBytesReadable(ReadableRegions& regions, UInt32 address, UInt32 bytes) {
	if (regions.Covers(address, bytes)) {
		return true;
	}
	MemoryBasicInfo region{};
	if (!QueryMemory(reinterpret_cast<const void*>(address), region)) {
		return false;
	}
	const UInt32 base = reinterpret_cast<UInt32>(region.baseAddress);
	if (!MemoryRangeReadable(base, region.regionSize, region.state, region.protect, address, bytes)) {
		return false;
	}
	regions.Remember(base, region.regionSize);
	return true;
}

}  // namespace obvr::game
