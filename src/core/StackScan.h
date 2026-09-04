#pragma once

#include "core/Types.h"

namespace obvr::watchdog {

// The arithmetic of the hang watchdog's stack report, kept apart from the
// thread it runs on so every flow is testable without a suspended thread.
//
// The report reads the stalled render thread's stack as plain words and
// keeps those that point into a module's code: a conservative scan, which
// lists every return address on the stack (and some stale ones and some
// function pointers) without needing frame pointers the release build does
// not keep. That is enough to name the call chain a hang sits in.

// How many 32-bit words can be read from `esp` without leaving the region
// [regionBase, regionBase + regionSize) the stack pointer was found in,
// capped at `wanted`. Zero when the pointer sits outside the region, is not
// word-aligned, or the region ends before it.
inline UInt32 StackWordsAvailable(UInt32 esp, UInt32 regionBase, UInt32 regionSize,
                                  UInt32 wanted) {
	if (esp % 4 != 0 || esp < regionBase) {
		return 0;
	}
	const UInt32 regionEnd = regionBase + regionSize;
	if (regionEnd < regionBase || esp >= regionEnd) {
		return 0;  // wrapped, or the pointer is past the region
	}
	const UInt32 available = (regionEnd - esp) / 4;
	return available < wanted ? available : wanted;
}

// Keeps, in stack order, the words `isCode` accepts, up to `capacity` of
// them. Returns how many were kept. `isCode` is whatever says "this word
// lies in a loaded module's code" - the watchdog asks the loader, the test
// asks a table.
template <class IsCode>
inline UInt32 CollectCodeWords(const UInt32* words, UInt32 count, IsCode isCode, UInt32* out,
                               UInt32 capacity) {
	UInt32 kept = 0;
	for (UInt32 i = 0; i < count && kept < capacity; ++i) {
		if (isCode(words[i])) {
			out[kept++] = words[i];
		}
	}
	return kept;
}

}  // namespace obvr::watchdog
