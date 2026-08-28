#pragma once

#include <cstdio>

#include "core/Types.h"

namespace obvr::render {

// The by-register half of the palette probe. The flat sums said the two
// world renders upload different bytes somewhere in the big uploads; they
// could not say where, because camera blocks and bone palettes ride the
// same counter. Splitting by start register separates them: a register is
// either camera-dependent, and its sums must differ between the eyes, or
// it is not, and they must match. The register that breaks its own rule is
// the finding.
struct PaletteRegisterBucket {
	UInt32 startRegister = 0;
	UInt32 calls = 0;
	UInt32 vectors = 0;
	UInt32 sum = 0;  // the uploads' float bits summed, order-independent
};

// Distinct start registers tracked per run. The big uploads come from a
// handful of shader constant blocks, so a handful of buckets covers them;
// anything past that lands in an overflow counter rather than a bucket.
inline constexpr UInt32 kPaletteBucketCount = 8;

// Picks the bucket for an upload to startRegister: the bucket already
// holding that register, or the first empty one, which the caller then
// claims. Empty buckets only ever trail the claimed ones, so a match is
// always found before a free slot, and buckets never move once claimed -
// which is what lets two snapshots of the array subtract by index.
// Returns bucketCount when every bucket holds some other register.
inline UInt32 SelectPaletteBucket(const PaletteRegisterBucket* buckets, UInt32 bucketCount,
                                  UInt32 startRegister) {
	for (UInt32 i = 0; i < bucketCount; ++i) {
		if (buckets[i].calls == 0 || buckets[i].startRegister == startRegister) {
			return i;
		}
	}
	return bucketCount;
}

// Formats one dual frame's per-register deltas from four snapshots of the
// bucket array: entry, after the first world render, after the moment
// between the renders, and after the second. Every register active in
// either render gets "cN first calls/vectors/sum second calls/vectors/sum".
// Returns how many registers were written; out is always terminated, holds
// the empty string when none were active, and keeps what fits when it runs
// out of room.
inline UInt32 FormatPaletteRegisterDeltas(const PaletteRegisterBucket* entry,
                                          const PaletteRegisterBucket* afterFirst,
                                          const PaletteRegisterBucket* afterBetween,
                                          const PaletteRegisterBucket* afterSecond,
                                          UInt32 bucketCount, char* out, UInt32 outSize) {
	if (out == nullptr || outSize == 0) {
		return 0;
	}
	out[0] = '\0';

	UInt32 used = 0;
	UInt32 written = 0;
	for (UInt32 i = 0; i < bucketCount; ++i) {
		const UInt32 firstCalls = afterFirst[i].calls - entry[i].calls;
		const UInt32 secondCalls = afterSecond[i].calls - afterBetween[i].calls;
		if (firstCalls == 0 && secondCalls == 0) {
			continue;
		}

		// A bucket claimed after the entry snapshot has its register only in
		// the later snapshots; buckets never move, so the last one has it.
		const int wrote = std::snprintf(
		    out + used, outSize - used, "%sc%u first %u/%u/%08X second %u/%u/%08X",
		    written == 0 ? "" : ", ", afterSecond[i].startRegister, firstCalls,
		    afterFirst[i].vectors - entry[i].vectors, afterFirst[i].sum - entry[i].sum,
		    secondCalls, afterSecond[i].vectors - afterBetween[i].vectors,
		    afterSecond[i].sum - afterBetween[i].sum);
		if (wrote < 0 || used + static_cast<UInt32>(wrote) >= outSize) {
			// Out of room: snprintf kept what fits and terminated it.
			break;
		}
		used += static_cast<UInt32>(wrote);
		++written;
	}
	return written;
}

}  // namespace obvr::render
