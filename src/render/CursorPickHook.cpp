#include "render/CursorPickHook.h"

#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "game/GameAddresses.h"
#include "render/ResolutionHook.h"

namespace obvr::render {
namespace {

// The first seven bytes of the pick's point normalization in 1.2.0.416, read
// from this machine's Oblivion.exe: cmp byte ptr [ecx+20Ch],0 - the branch
// between the renderer's two size sources. Whole instructions, nothing
// relative; verified before patching, which is the second source for the
// address.
constexpr UInt8 kEntryBytes[] = {0x80, 0xB9, 0x0C, 0x02, 0x00, 0x00, 0x00};

// thiscall with four stack arguments the callee pops (ret 10h): the pixel
// pair in, the normalized pair out. __fastcall with a dummy edx mirrors it.
using NormalizePointFn = UInt8(__fastcall*)(void* self, void* edx, SInt32 x, SInt32 y,
                                            float* outX, float* outY);
NormalizePointFn g_original = nullptr;

UInt32 g_traceLeft = 4;

// The pick's one wrong number, replaced at its source. The cursor pixels
// live in the screen-size copy's space - the tile search reads and clamps
// them against the copy - but this normalization divides them by the
// renderer's real width and height on their way into the camera's port and
// frustum. Equal in vanilla; with the copy raised, every hit rectangle sat
// below its picture by the difference, highlight and click three entries
// off the pointer. Believed pixels over believed size is the identity the
// drawing already uses.
UInt8 __fastcall HookedNormalizePoint(void* self, void* edx, SInt32 x, SInt32 y,
                                      float* outX, float* outY) {
	UInt32 frameWidth = 0;
	UInt32 frameHeight = 0;
	UInt32 believedWidth = 0;
	UInt32 believedHeight = 0;
	if (!WasDeviceCreated(frameWidth, frameHeight) ||
	    !GameBelievedSize(believedWidth, believedHeight) ||
	    (believedWidth == frameWidth && believedHeight == frameHeight) ||
	    believedWidth == 0 || believedHeight == 0 || outX == nullptr || outY == nullptr) {
		return g_original(self, edx, x, y, outX, outY);
	}

	*outX = static_cast<float>(x) / static_cast<float>(believedWidth);
	*outY = 1.0f - static_cast<float>(y) / static_cast<float>(believedHeight);

	if (g_traceLeft > 0) {
		--g_traceLeft;
		OBVR_LOG("Cursor pick: (%d, %d) normalized over the believed %ux%u -> "
		         "(%.3f, %.3f)",
		         x, y, believedWidth, believedHeight, static_cast<double>(*outX),
		         static_cast<double>(*outY));
	}
	return 1;
}

}  // namespace

bool InstallCursorPickHook() {
	if (g_original != nullptr) {
		return true;
	}

	if (!mem::Verify(addr::kPickNormalizePoint, kEntryBytes, sizeof(kEntryBytes))) {
		OBVR_LOG("Cursor pick: bytes at %08X differ, the hover keeps its offset",
		         addr::kPickNormalizePoint);
		mem::ReportForeignCode("Cursor pick", addr::kPickNormalizePoint);
		return false;
	}

	constexpr UInt32 kTrampolineCapacity = 16;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(kTrampolineCapacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Cursor pick: no executable memory for the trampoline");
		return false;
	}

	const UInt32 trampolineAddress = reinterpret_cast<UInt32>(trampoline);
	const UInt32 trampolineSize = mem::BuildEntryTrampoline(
		trampoline, kTrampolineCapacity, trampolineAddress, addr::kPickNormalizePoint,
		kEntryBytes, sizeof(kEntryBytes));
	if (trampolineSize == 0) {
		OBVR_LOG("Cursor pick: the trampoline does not fit");
		return false;
	}

	UInt8 patch[sizeof(kEntryBytes)];
	const UInt32 patchSize = mem::BuildEntryPatch(
		patch, sizeof(patch), addr::kPickNormalizePoint,
		reinterpret_cast<UInt32>(&HookedNormalizePoint), sizeof(kEntryBytes));
	if (patchSize != sizeof(patch)) {
		OBVR_LOG("Cursor pick: the patch has unexpected length %u", patchSize);
		return false;
	}

	g_original = reinterpret_cast<NormalizePointFn>(trampoline);
	if (!mem::SafeWrite(addr::kPickNormalizePoint, patch, patchSize)) {
		g_original = nullptr;
		OBVR_LOG("Cursor pick: SafeWrite to %08X failed, the hover keeps its offset",
		         addr::kPickNormalizePoint);
		return false;
	}

	OBVR_LOG("Cursor pick: point normalization at %08X answers in the believed size, "
	         "trampoline at %08X",
	         addr::kPickNormalizePoint, trampolineAddress);
	return true;
}

}  // namespace obvr::render
