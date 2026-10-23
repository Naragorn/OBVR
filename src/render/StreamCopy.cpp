#include "render/StreamCopy.h"

#include <cstring>

#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "render/StreamCopyLogic.h"

namespace obvr::render {
namespace {

// The indexed stream copy of the NiDX9 renderer: cdecl(const desc*), the
// counted total in eax. Its first five bytes in 1.2.0.416 - sub esp,0Ch;
// push ebx; push ebp - are whole instructions with nothing relative in them.
constexpr UInt32 kStreamCopy = 0x0076E610;
constexpr UInt8 kOriginalBytes[] = {0x83, 0xEC, 0x0C, 0x53, 0x55};

using CopyFn = UInt32(__cdecl*)(const StreamCopyDesc* desc);
CopyFn g_original = nullptr;

// The check mode's scratch: the engine's result is copied out of the
// destination before the fast copy overwrites it, then compared.
constexpr UInt32 kCheckCapacity = 1u << 20;
UInt8* g_checkScratch = nullptr;
UInt32 g_checked = 0;
UInt32 g_mismatches = 0;
UInt32 g_tooLarge = 0;
UInt32 g_mismatchLinesLeft = 8;
UInt32 g_nextReport = 2000;

UInt32 __cdecl StreamCopyChecked(const StreamCopyDesc* desc) {
	const UInt32 span = StreamCopyDestSpan(*desc);
	if (g_checkScratch == nullptr || span > kCheckCapacity) {
		++g_tooLarge;
		return g_original(desc);
	}
	const UInt32 engineTotal = g_original(desc);
	std::memcpy(g_checkScratch, desc->dest, span);
	const UInt32 fastTotal = StreamCopyFast(*desc);
	++g_checked;
	if (fastTotal != engineTotal || std::memcmp(g_checkScratch, desc->dest, span) != 0) {
		++g_mismatches;
		if (g_mismatchLinesLeft != 0) {
			--g_mismatchLinesLeft;
			UInt32 firstDiff = span;
			for (UInt32 i = 0; i < span; ++i) {
				if (g_checkScratch[i] != desc->dest[i]) {
					firstDiff = i;
					break;
				}
			}
			OBVR_LOG("Stream copy: MISMATCH - count %u, element %u bytes, indexed %d, source %s, "
			         "source words %u, strides %u/%u, counted %u; totals %u/%u, first differing byte %u of %u",
			         desc->count, desc->elementBytes, desc->indices != nullptr ? 1 : 0,
			         desc->source != nullptr ? "set" : "null", desc->sourceWords, desc->sourceStride,
			         desc->destStride, desc->countedBytes, engineTotal, fastTotal, firstDiff, span);
		}
	}
	if (g_checked >= g_nextReport) {
		g_nextReport += 20000;
		OBVR_LOG("Stream copy: check - %u copies compared, %u mismatches, %u too large to compare",
		         g_checked, g_mismatches, g_tooLarge);
	}
	return fastTotal;
}

UInt32 __cdecl StreamCopyFastEntry(const StreamCopyDesc* desc) {
	return StreamCopyFast(*desc);
}

}  // namespace

void InstallStreamCopy() {
	static bool s_done = false;
	if (s_done) return;
	s_done = true;
	const Config& config = GetConfig();
	if (!config.tracker.fastStreamCopy) {
		OBVR_LOG("Stream copy: the engine's ([Render] FastStreamCopy=0)");
		return;
	}
	if (!mem::Verify(kStreamCopy, kOriginalBytes, sizeof(kOriginalBytes))) {
		OBVR_LOG("Stream copy: the routine's entry at %08X differs; the engine's stays", kStreamCopy);
		mem::ReportForeignCode("Stream copy", kStreamCopy);
		return;
	}
	constexpr UInt32 capacity = 24;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(capacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Stream copy: no executable memory; the engine's stays");
		return;
	}
	if (mem::BuildEntryTrampoline(trampoline, capacity, reinterpret_cast<UInt32>(trampoline),
	                              kStreamCopy, kOriginalBytes, sizeof(kOriginalBytes)) == 0) {
		OBVR_LOG("Stream copy: trampoline did not fit; the engine's stays");
		return;
	}
	g_original = reinterpret_cast<CopyFn>(trampoline);
	const bool check = config.streamCopyCheck;
	if (check) {
		g_checkScratch = new (std::nothrow) UInt8[kCheckCapacity];
		if (g_checkScratch == nullptr) {
			OBVR_LOG("Stream copy: no memory for the check scratch; the engine's stays");
			return;
		}
	}
	const void* replacement = check ? reinterpret_cast<const void*>(&StreamCopyChecked)
	                                : reinterpret_cast<const void*>(&StreamCopyFastEntry);
	UInt8 patch[sizeof(kOriginalBytes)];
	if (mem::BuildEntryPatch(patch, sizeof(patch), kStreamCopy,
	                         reinterpret_cast<UInt32>(replacement), sizeof(kOriginalBytes)) == 0 ||
	    !mem::SafeWrite(kStreamCopy, patch, sizeof(patch))) {
		OBVR_LOG("Stream copy: could not patch the routine; the engine's stays");
		return;
	}
	OBVR_LOG("Stream copy: fast - the indexed stream copy at %08X decides the element size once "
	         "instead of a memcpy call per vertex%s",
	         kStreamCopy, check ? "; check mode compares every copy with the engine's" : "");
}

}  // namespace obvr::render
