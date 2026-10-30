#pragma once

// The engine's stream copy, and a faster one with the same result.
//
// Read in Oblivion.exe 1.2.0.416 (2026-10-23, the render sampler with the
// opening's five people): when the engine packs a geometry's vertex streams
// into its render record (docs/performance-spec.md section 2, the copier at
// 0x0076F590 - NiDX9ShaderDeclaration::PackEntry, __stdcall with seven
// arguments, reached through the declaration's vtable slot +0x6C; OBNG
// 2026-10-30), each stream's elements are copied by one of three routines
// out of a table (0x0076E430, 0x0076E4B0, 0x0076E610; the table is filled
// at 0x00771356). The one the skinned bodies take, 0x0076E610, copies a
// skin partition's vertices through an index table - and does so with a
// CRT memcpy call PER VERTEX, 12 or 8 bytes each: 16 % of a scene pass with
// five people in view sat in memcpy called from that loop (0x0076E6AC),
// another 4 % in the loop itself. The non-indexed path of the same routine
// has unrolled copies for 8, 12 and 16 bytes; the indexed one does not.
//
// StreamCopyFast does what the routine does - the same bytes land at the
// same places, the same count comes back - with the element size decided
// once per call instead of once per vertex, so the compiler's inline moves
// replace the call. The pure half here, process-free, so a test can run the
// engine's own loops (StreamCopyReference) and this one over the same data
// and compare; the detour in render/StreamCopy.cpp.

#include <cstring>

#include "core/Types.h"

namespace obvr::render {

// The descriptor the copier builds on its stack (0x0076F87C) and hands the
// routine as its one cdecl argument, as 0x0076E610 reads it.
struct StreamCopyDesc {
	UInt16 word0;            // +0x00: the stream's type code (0x0076F86B), unused here
	UInt16 word1;            // +0x02
	UInt16 sourceWords;      // +0x04: an index steps this many dwords in the source
	UInt16 pad6;             // +0x06
	UInt16 count;            // +0x08: elements to copy
	UInt16 padA;             // +0x0A
	const UInt16* indices;   // +0x0C: the index table, or null for a straight run
	const UInt8* source;     // +0x10: the source elements, or null to zero-fill
	                         // (PackEntry never passes null, OBNG 2026-10-30)
	UInt32 elementBytes;     // +0x14: bytes copied per element
	UInt32 sourceStride;     // +0x18: the source's step per element (straight run)
	UInt32 countedBytes;     // +0x1C: what each element adds to the returned total
	UInt32 destStride;       // +0x20: the destination's step per element
	UInt8* dest;             // +0x24: where the elements go
};
static_assert(sizeof(StreamCopyDesc) == 0x28, "the copier's descriptor");

// The engine's routine as read, loop for loop, memcpy for memcpy - the
// reference the fast copy is checked against. Returns the counted total.
inline UInt32 StreamCopyReference(const StreamCopyDesc& d) {
	UInt32 total = 0;
	UInt8* dst = d.dest;
	if (d.source == nullptr) {
		// The zero-fill clears countedBytes (+0x1C), not elementBytes: the
		// memset's length is read at 0x0076E642/0x0076E64E (OBNG,
		// re/functions/0x0076E610.md, 2026-10-30).
		for (UInt32 i = 0; i < d.count; ++i) {
			std::memset(dst, 0, d.countedBytes);
			dst += d.destStride;
			total += d.countedBytes;
		}
		return total;
	}
	if (d.indices != nullptr) {
		const UInt32 step = static_cast<UInt32>(d.sourceWords) * 4u;
		for (UInt32 i = 0; i < d.count; ++i) {
			std::memcpy(dst, d.source + static_cast<UInt32>(d.indices[i]) * step, d.elementBytes);
			dst += d.destStride;
			total += d.countedBytes;
		}
		return total;
	}
	const UInt8* src = d.source;
	if (d.elementBytes == 8 || d.elementBytes == 12 || d.elementBytes == 16) {
		for (UInt32 i = 0; i < d.count; ++i) {
			std::memcpy(dst, src, d.elementBytes);
			src += d.sourceStride;
			dst += d.destStride;
		}
		return static_cast<UInt32>(d.count) * d.countedBytes;
	}
	for (UInt32 i = 0; i < d.count; ++i) {
		std::memcpy(dst, src, d.elementBytes);
		src += d.sourceStride;
		dst += d.destStride;
		total += d.countedBytes;
	}
	return total;
}

namespace detail {

// One fixed-size element per step, the size a compile-time constant so the
// copy is a handful of moves. Indexed: the source element is picked by the
// table; straight: the source steps by its stride.
template <UInt32 kBytes>
inline void CopyIndexed(const StreamCopyDesc& d) {
	UInt8* dst = d.dest;
	const UInt32 step = static_cast<UInt32>(d.sourceWords) * 4u;
	const UInt16* idx = d.indices;
	const UInt16* const end = idx + d.count;
	for (; idx != end; ++idx) {
		std::memcpy(dst, d.source + static_cast<UInt32>(*idx) * step, kBytes);
		dst += d.destStride;
	}
}

template <UInt32 kBytes>
inline void CopyStraight(const StreamCopyDesc& d) {
	UInt8* dst = d.dest;
	const UInt8* src = d.source;
	for (UInt32 i = 0; i < d.count; ++i) {
		std::memcpy(dst, src, kBytes);
		src += d.sourceStride;
		dst += d.destStride;
	}
}

}  // namespace detail

// The same result as StreamCopyReference, the element size decided once.
inline UInt32 StreamCopyFast(const StreamCopyDesc& d) {
	const UInt32 total = static_cast<UInt32>(d.count) * d.countedBytes;
	if (d.count == 0) return 0;
	if (d.source == nullptr) {
		UInt8* dst = d.dest;
		if (d.destStride == d.countedBytes) {
			std::memset(dst, 0, static_cast<UInt32>(d.count) * d.countedBytes);
		} else {
			for (UInt32 i = 0; i < d.count; ++i) {
				std::memset(dst, 0, d.countedBytes);
				dst += d.destStride;
			}
		}
		return total;
	}
	if (d.indices != nullptr) {
		switch (d.elementBytes) {
			case 4: detail::CopyIndexed<4>(d); break;
			case 8: detail::CopyIndexed<8>(d); break;
			case 12: detail::CopyIndexed<12>(d); break;
			case 16: detail::CopyIndexed<16>(d); break;
			default: {
				UInt8* dst = d.dest;
				const UInt32 step = static_cast<UInt32>(d.sourceWords) * 4u;
				for (UInt32 i = 0; i < d.count; ++i) {
					std::memcpy(dst, d.source + static_cast<UInt32>(d.indices[i]) * step, d.elementBytes);
					dst += d.destStride;
				}
			}
		}
		return total;
	}
	switch (d.elementBytes) {
		case 4: detail::CopyStraight<4>(d); break;
		case 8: detail::CopyStraight<8>(d); break;
		case 12: detail::CopyStraight<12>(d); break;
		case 16: detail::CopyStraight<16>(d); break;
		default: {
			UInt8* dst = d.dest;
			const UInt8* src = d.source;
			for (UInt32 i = 0; i < d.count; ++i) {
				std::memcpy(dst, src, d.elementBytes);
				src += d.sourceStride;
				dst += d.destStride;
			}
		}
	}
	return total;
}

// The bytes a copy touches in the destination, for the check mode that
// compares the engine's result with the fast one: the last element ends
// at (count - 1) * destStride + its bytes - elementBytes copied, countedBytes
// zeroed. Zero for an empty copy.
inline UInt32 StreamCopyDestSpan(const StreamCopyDesc& d) {
	if (d.count == 0) return 0;
	const UInt32 last = d.source == nullptr ? d.countedBytes : d.elementBytes;
	return (static_cast<UInt32>(d.count) - 1u) * d.destStride + last;
}

}  // namespace obvr::render
