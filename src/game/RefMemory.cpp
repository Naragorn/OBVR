#include "game/RefFingerprint.h"
#include "platform/Win32Min.h"

namespace obvr::game {
namespace {

// The probes asked of the system so far (RefProbesMade).
UInt32& Probes() {
	static UInt32 s_probes = 0;
	return s_probes;
}

// SpanReadableBy's reader: the bytes copied out by the system, or not.
bool ReadOwn(UInt32 address, UInt32 bytes) {
	++Probes();
	UInt8 copy[kProbeChunkBytes];
	return ReadOwnMemory(reinterpret_cast<const void*>(address), copy, bytes);
}

}  // namespace

UInt32 RefProbesMade() { return Probes(); }

bool RefBytesReadable(UInt32 address, UInt32 bytes) { return SpanReadableBy(&ReadOwn, address, bytes); }

bool RefBytesReadable(ReadablePages& pages, UInt32 address, UInt32 bytes) {
	return SpanReadableBy(pages, &ReadOwn, address, bytes);
}

}  // namespace obvr::game
