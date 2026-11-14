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

// Whether a remembered object's bytes can be read without a fault is asked
// by reading them: a copy of the span through the system's own read of this
// process's memory (ReadOwnMemory, platform/Win32Min.h), which fails for
// reserved or freed memory - a heap that gave its pages back - a guard page
// or no-access memory, and raises nothing in this process. It was asked of
// VirtualQuery before, which answers with the size of the whole run of pages
// alike from the address on and scans that run for it: in the game's heap,
// one committed run of hundreds of megabytes, a single question cost
// milliseconds (docs/performance-spec.md, "The hand fit's read", 2026-11-14).

// A read asked about at all: some bytes, not past the end of the address
// space. Pure, ref_fingerprint_test.
inline bool ReadSpanAsked(UInt32 address, UInt32 bytes) {
	return bytes > 0 && static_cast<UInt64>(address) + bytes <= 0x100000000ull;
}

// The most one read of the system copies: a longer span is read in chunks.
inline constexpr UInt32 kProbeChunkBytes = 64;

// Whether every byte of the span reads, by `read` - bool(UInt32 address,
// UInt32 bytes), at most kProbeChunkBytes a call, true when the system read
// them all. A span not asked about is not readable, and the system is not
// asked; a refused chunk ends it. Pure, ref_fingerprint_test.
template <typename ReadFn>
bool SpanReadableBy(ReadFn&& read, UInt32 address, UInt32 bytes) {
	if (!ReadSpanAsked(address, bytes)) {
		return false;
	}
	while (bytes > 0) {
		const UInt32 chunk = bytes < kProbeChunkBytes ? bytes : kProbeChunkBytes;
		if (!read(address, chunk)) {
			return false;
		}
		address += chunk;
		bytes -= chunk;
	}
	return true;
}

// The pages found readable during one read of many objects (a skin's bones,
// the shapes of one body): the system asked once a page, not once an object.
// Kept only for that read - a heap may give pages back between frames. A page
// is readable or not as a whole, so one byte read on it answers for every
// span inside it. One slot a page number's low bits, a later page taking the
// slot of an earlier one; a span over more than kMostSpanPages pages is
// neither remembered nor covered. Pure, ref_fingerprint_test.
inline constexpr UInt32 kPageBytes = 0x1000;
struct ReadablePages {
	static constexpr UInt32 kSlots = 256;
	static constexpr UInt32 kMostSpanPages = 4;
	UInt32 pagePlusOne[kSlots] = {};  // 0: empty

	// The span's first and last page, false for a span not kept here.
	static bool Pages(UInt32 address, UInt32 bytes, UInt32& first, UInt32& last) {
		if (!ReadSpanAsked(address, bytes)) {
			return false;
		}
		first = address / kPageBytes;
		last = (address + (bytes - 1)) / kPageBytes;
		return last - first < kMostSpanPages;
	}

	bool Covers(UInt32 address, UInt32 bytes) const {
		UInt32 first = 0;
		UInt32 last = 0;
		if (!Pages(address, bytes, first, last)) {
			return false;
		}
		for (UInt32 page = first; page <= last; ++page) {
			if (pagePlusOne[page % kSlots] != page + 1) {
				return false;
			}
		}
		return true;
	}

	void Remember(UInt32 address, UInt32 bytes) {
		UInt32 first = 0;
		UInt32 last = 0;
		if (!Pages(address, bytes, first, last)) {
			return;
		}
		for (UInt32 page = first; page <= last; ++page) {
			pagePlusOne[page % kSlots] = page + 1;
		}
	}
};

// The same, asking `read` only for a span `pages` does not cover yet, and
// remembering the pages of a span it read. Pure, ref_fingerprint_test.
template <typename ReadFn>
bool SpanReadableBy(ReadablePages& pages, ReadFn&& read, UInt32 address, UInt32 bytes) {
	if (pages.Covers(address, bytes)) {
		return true;
	}
	if (!SpanReadableBy(read, address, bytes)) {
		return false;
	}
	pages.Remember(address, bytes);
	return true;
}

// The world counter (NearbyItems.cpp): stepped once a frame by the camera
// hook with whether a load runs, read by TakeRefFingerprint and
// RefStillThere.
UInt32 CurrentWorld();
void StepWorld(bool loading);

// Whether `bytes` bytes at `address` read without a fault, asked of the
// system by reading them (SpanReadableBy; RefMemory.cpp, kept apart:
// windows.h's macros clash with NearbyItems).
bool RefBytesReadable(UInt32 address, UInt32 bytes);
// The same, asking the system only for pages `pages` does not hold yet.
bool RefBytesReadable(ReadablePages& pages, UInt32 address, UInt32 bytes);
// The memory probes asked of the system so far, by both (wraps): a read's
// cost is said with the probes it made (game/HandReadCostLogic.h).
UInt32 RefProbesMade();

// Takes the fingerprint of a reference that is live now (a crosshair target,
// a reference just activated); an empty one (ref 0) for an address that is no
// object or whose vtable is not the game's.
RefFingerprint TakeRefFingerprint(UInt32 ref);

// Whether the remembered reference is still there: from this world, its bytes
// in readable memory, and RefFingerprintMatches on what its address holds
// now. False for an empty fingerprint.
bool RefStillThere(const RefFingerprint& fp);

}  // namespace obvr::game
