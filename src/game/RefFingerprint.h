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
struct RefFingerprint {
	UInt32 ref = 0;
	UInt32 vtable = 0;
	UInt32 formId = 0;
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

// Takes the fingerprint of a reference that is live now (a crosshair target,
// a reference just activated); an empty one (ref 0) for an address that is no
// object or whose vtable is not the game's.
RefFingerprint TakeRefFingerprint(UInt32 ref);

// Whether the remembered reference is still there (RefFingerprintMatches on
// what its address holds now). False for an empty fingerprint.
bool RefStillThere(const RefFingerprint& fp);

}  // namespace obvr::game
