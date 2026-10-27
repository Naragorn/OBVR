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

// Whether the seven bytes before a word on the stack end in a call, so the
// word is the return address of a call that is still running: before[6] is
// the byte just before the address, before[0] seven bytes back. An E8 rel32,
// or an FF /2 in any of its addressing forms (register, [reg], [sib],
// [disp32], with a disp8 or a disp32). A conservative scan keeps every word
// that points into code; text read as words (UTF-16 "\WINDOWS\System32\"
// reads 0x0057005C, inside Oblivion.exe's .text) and stale pointers into
// code are not calls, and this tells them apart (the render sampler,
// 2026-10-27). Pure, stack_scan_test.
inline bool FollowsCall(const UInt8* before) {
	if (before[2] == 0xE8) {
		return true;  // call rel32: E8 at -5
	}
	// FF /2: the byte after FF is a ModRM with reg = 2.
	auto isCall = [](UInt8 modrm) { return ((modrm >> 3) & 7) == 2; };
	// -2: FF modrm, mod 11 (a register) or mod 00 without SIB or disp32
	if (before[5] == 0xFF && isCall(before[6])) {
		const UInt8 mod = before[6] >> 6;
		const UInt8 rm = before[6] & 7;
		if (mod == 3 || (mod == 0 && rm != 4 && rm != 5)) {
			return true;
		}
	}
	// -3: FF modrm sib (mod 00, rm 4, base not 5) or FF modrm disp8 (mod 01, rm not 4)
	if (before[4] == 0xFF && isCall(before[5])) {
		const UInt8 mod = before[5] >> 6;
		const UInt8 rm = before[5] & 7;
		if ((mod == 0 && rm == 4 && (before[6] & 7) != 5) || (mod == 1 && rm != 4)) {
			return true;
		}
	}
	// -4: FF modrm sib disp8 (mod 01, rm 4)
	if (before[3] == 0xFF && isCall(before[4]) && (before[4] >> 6) == 1 && (before[4] & 7) == 4) {
		return true;
	}
	// -6: FF modrm disp32 (mod 00 rm 5, the absolute call [disp32]; or mod 10, rm not 4)
	if (before[1] == 0xFF && isCall(before[2])) {
		const UInt8 mod = before[2] >> 6;
		const UInt8 rm = before[2] & 7;
		if ((mod == 0 && rm == 5) || (mod == 2 && rm != 4)) {
			return true;
		}
	}
	// -7: FF modrm sib disp32 (mod 10 rm 4, or mod 00 rm 4 with a SIB of base 5)
	if (before[0] == 0xFF && isCall(before[1]) && (before[1] & 7) == 4) {
		const UInt8 mod = before[1] >> 6;
		if (mod == 2 || (mod == 0 && (before[2] & 7) == 5)) {
			return true;
		}
	}
	return false;
}

// A module's code: one executable section, [begin, end).
struct CodeRange {
	UInt32 begin = 0;
	UInt32 end = 0;
};

inline bool InCodeRange(const CodeRange* ranges, UInt32 count, UInt32 address) {
	for (UInt32 i = 0; i < count; ++i) {
		if (address >= ranges[i].begin && address < ranges[i].end) {
			return true;
		}
	}
	return false;
}

// The executable sections of a module from its PE headers as mapped: `image`
// is the module's first `available` bytes (its headers), `base` where it is
// loaded. The section table is read only where it lies inside those bytes.
// Returns how many ranges were written, up to `capacity`; zero for anything
// that is not a PE image. Pure, stack_scan_test.
inline UInt32 CodeRangesOfImage(const UInt8* image, UInt32 available, UInt32 base, CodeRange* out,
                                UInt32 capacity) {
	auto read16 = [&](UInt32 at) { return static_cast<UInt32>(image[at] | (image[at + 1] << 8)); };
	auto read32 = [&](UInt32 at) {
		return static_cast<UInt32>(image[at]) | (static_cast<UInt32>(image[at + 1]) << 8) |
		       (static_cast<UInt32>(image[at + 2]) << 16) | (static_cast<UInt32>(image[at + 3]) << 24);
	};
	if (available < 0x40 || image[0] != 'M' || image[1] != 'Z') {
		return 0;
	}
	const UInt32 pe = read32(0x3C);
	if (pe > available || available - pe < 24 || image[pe] != 'P' || image[pe + 1] != 'E' || image[pe + 2] != 0 ||
	    image[pe + 3] != 0) {
		return 0;
	}
	const UInt32 sections = read16(pe + 6);
	const UInt32 optional = read16(pe + 20);
	const UInt64 table = static_cast<UInt64>(pe) + 24 + optional;
	constexpr UInt32 kExecute = 0x20000000u;  // IMAGE_SCN_MEM_EXECUTE
	UInt32 written = 0;
	for (UInt32 i = 0; i < sections && written < capacity; ++i) {
		const UInt64 at = table + static_cast<UInt64>(i) * 40;
		if (at + 40 > available) {
			break;
		}
		const UInt32 size = read32(static_cast<UInt32>(at) + 8);
		const UInt32 va = read32(static_cast<UInt32>(at) + 12);
		const UInt32 flags = read32(static_cast<UInt32>(at) + 36);
		if ((flags & kExecute) == 0 || size == 0) {
			continue;
		}
		out[written].begin = base + va;
		out[written].end = base + va + size;
		++written;
	}
	return written;
}

}  // namespace obvr::watchdog
