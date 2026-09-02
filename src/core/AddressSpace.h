#pragma once

#include "core/Types.h"

namespace obvr::mem {

// Whether a value read out of the game looks like a pointer to one of its
// objects: inside the user address space, past the reserved low pages that
// catch null-offset reads, and four-byte aligned as every allocation here is.
//
// THE UPPER BOUND IS THE WHOLE 32-BIT SPACE, LESS THE TOP 64 KB. Oblivion.exe
// on this machine is marked "Application can handle large (>2GB) addresses"
// (the 4 GB patch, read off the file header with dumpbin), and under that
// flag Windows hands a 32-bit process addresses above 0x80000000 as soon as
// the low half is busy. A session on 2026-09-02 did exactly that: the camera
// node came in at 0x800B5964 where earlier sessions had 0x1812BCA0, and a
// check that ended at 0x7FFFFFFF refused the player pointer for the whole
// session. Every arrow, spell and swing went straight ahead, and the log's
// gate line read player=0 while the headset was up and no menu was. The top
// 64 KB stay excluded: the system keeps them, and a value there is not an
// object.
//
// One function so every reader agrees. It rejects a half-written value, a
// small integer and an unaligned one; it cannot reject a plausible pointer to
// an object that is not finished, which is what the callers' own checks are
// for.
inline constexpr UInt32 kLowestObjectAddress = 0x00010000u;
inline constexpr UInt32 kHighestObjectAddress = 0xFFFEFFFFu;

inline bool LooksLikeObjectAddress(UInt32 address) {
	return address >= kLowestObjectAddress && address <= kHighestObjectAddress &&
	       (address & 3u) == 0u;
}

}  // namespace obvr::mem
