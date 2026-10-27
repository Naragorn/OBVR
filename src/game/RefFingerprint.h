#pragma once

#include "core/Types.h"

namespace obvr::game {

// A reference remembered across frames, with what it was when it was taken:
// its vtable and its form id.
//
// A reference can be deleted while something remembers it - an item taken
// into the inventory is - and its memory handed to the next allocation. Read
// through the old pointer, that allocation's bytes are taken for a
// reference. The crash of 2026-10-10 was exactly that: a weapon activated
// (taken) stayed the container panel's "thing activated last" for 180
// frames; dropping another weapon in the inventory reused its memory, a
// float (1.0) stood where its 3D node pointer had been, and reading the
// node's bound faulted (CobbCrashLogger: EIP OBVR+0x71BAF, RefWorldBound,
// called by the panel's placement).
//
// So before a remembered reference is read again, it must still carry the
// vtable and the form id it had, and be neither deleted nor disabled.
//
// And it must be from this world. A load replaces the world: the old one's
// references are freed, and their memory need not even be mapped any more -
// the fingerprint check itself then faults on the read. The crash of
// 2026-10-27 15:40 (the Windows mod set mirrored on Linux, the second load
// out of MemorialCave with forty Walkers about, crash-load-dungeon-win.txt):
// RefStillThere+0x28 reading the pick ring's settled reference (SettledRef in
// CameraHook.cpp) in the first frame after the load. So every fingerprint
// carries the world it was taken in, counted up on every loading frame, and a
// reference of an older world is gone without a read; and the read itself is
// made only where the memory is committed and readable.
struct RefFingerprint {
	UInt32 ref = 0;
	UInt32 vtable = 0;
	UInt32 formId = 0;
	UInt32 world = 0;  // the world counter when it was taken; 0 = none
};

// TESForm flags: deleted 0x20, disabled 0x800.
inline constexpr UInt32 kRefGoneFlags = 0x20 | 0x800;

// The game's own image, where every reference's vtable lies.
inline constexpr UInt32 kGameImageStart = 0x00400000;
inline constexpr UInt32 kGameImageEnd = 0x00C05000;

inline bool LooksLikeGameVtable(UInt32 vtable) { return vtable >= kGameImageStart && vtable < kGameImageEnd; }

// Whether what is read at the remembered address now is the reference that
// was remembered. Pure, ref_fingerprint_test.
inline bool RefFingerprintMatches(const RefFingerprint& was, UInt32 vtableNow, UInt32 formIdNow, UInt32 flagsNow) {
	return was.ref != 0 && LooksLikeGameVtable(was.vtable) && vtableNow == was.vtable && formIdNow == was.formId &&
	       (flagsNow & kRefGoneFlags) == 0;
}

// Whether what is read at an address nobody remembered - the engine's own
// crosshair slot - is a live reference at all: its first word one of the
// game's vtables, its flags neither deleted nor disabled. A reference taken
// into the inventory is deleted and its memory reused; the bytes of a float
// where the vtable was (the crash of 2026-10-12, 0x3F7E0010 where a base
// form pointer had been) are not a reference. Pure, ref_fingerprint_test.
inline bool RefLooksLive(UInt32 vtableNow, UInt32 flagsNow) {
	return LooksLikeGameVtable(vtableNow) && (flagsNow & kRefGoneFlags) == 0;
}

// Whether a fingerprint was taken in the world that is current now. Pure,
// ref_fingerprint_test.
inline bool RefFromWorld(const RefFingerprint& fp, UInt32 worldNow) {
	return fp.ref != 0 && fp.world != 0 && fp.world == worldNow;
}

// The world counter after a frame: a new world on every frame a load runs
// (the loading thread, or the Loading menu up), so a reference remembered
// before or during a load is never read after it. 0 is skipped: it marks no
// world. Pure, ref_fingerprint_test.
inline UInt32 NextWorld(UInt32 world, bool loading) {
	if (!loading) {
		return world;
	}
	const UInt32 next = world + 1u;
	return next == 0u ? 1u : next;
}

// Whether [address, address + bytes) lies inside the memory region
// VirtualQuery described (its base and size, its state and protection), and
// that region is committed and readable: a read there cannot fault. Reserved
// or freed memory - a heap that gave its pages back - is not, nor a guard
// page, nor no-access or execute-only memory. Pure, ref_fingerprint_test.
inline constexpr UInt32 kMemCommit = 0x1000;
inline constexpr UInt32 kPageGuard = 0x100;
// PAGE_READONLY, READWRITE, WRITECOPY, EXECUTE_READ, EXECUTE_READWRITE,
// EXECUTE_WRITECOPY.
inline constexpr UInt32 kPageReadable = 0x02 | 0x04 | 0x08 | 0x20 | 0x40 | 0x80;
inline bool MemoryRangeReadable(UInt32 regionBase, UInt32 regionSize, UInt32 state, UInt32 protect, UInt32 address,
                                UInt32 bytes) {
	if (state != kMemCommit || (protect & kPageGuard) != 0 || (protect & kPageReadable) == 0 || regionSize == 0) {
		return false;
	}
	const UInt64 start = address;
	const UInt64 end = start + bytes;
	const UInt64 regionStart = regionBase;
	const UInt64 regionEnd = regionStart + regionSize;
	return end <= 0x100000000ull && start >= regionStart && end <= regionEnd;
}

// The world counter (NearbyItems.cpp): stepped once a frame by the camera
// hook with whether a load runs, read by TakeRefFingerprint and
// RefStillThere.
UInt32 CurrentWorld();
void StepWorld(bool loading);

// MemoryRangeReadable on the region VirtualQuery reports around the address
// (RefMemory.cpp, kept apart: windows.h's macros clash with NearbyItems).
bool RefBytesReadable(UInt32 address, UInt32 bytes);

// Takes the fingerprint of a reference that is live now (a crosshair target,
// a reference just activated); an empty one (ref 0) for an address that is no
// object or whose vtable is not the game's.
RefFingerprint TakeRefFingerprint(UInt32 ref);

// Whether the remembered reference is still there: from this world, its bytes
// in readable memory, and RefFingerprintMatches on what its address holds
// now. False for an empty fingerprint.
bool RefStillThere(const RefFingerprint& fp);

}  // namespace obvr::game
