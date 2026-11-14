#include "game/RefFingerprint.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// The probes asked of the system so far (RefProbesMade).
UInt32& Probes() {
	static UInt32 s_probes = 0;
	return s_probes;
}

}  // namespace

UInt32 RefProbesMade() { return Probes(); }

bool RefBytesReadable(UInt32 address, UInt32 bytes) {
	MemoryBasicInfo region{};
	++Probes();
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
	++Probes();
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
