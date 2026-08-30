#include "render/CursorMapHook.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "game/GameAddresses.h"
#include "render/UiScreenSize.h"

namespace obvr::render {
namespace {

// The two size calls inside the cursor-node placement, read from this
// machine's Oblivion.exe 1.2.0.416:
//
//   0057E80A: push 1; mov ecx,edi; call 0x403190              (axis 1, width)
//   0057E813: push 2; mov ecx,edi; mov [esp+0Ch],eax; call 0x403190
//
// All 22 bytes are checked, not just the eight rel32 bytes being rewritten,
// because the check is "is this the code I read", not "is there room".
constexpr UInt8 kOriginalBytes[] = {0x6A, 0x01, 0x8B, 0xCF, 0xE8, 0x7D, 0x49, 0xE8,
                                    0xFF, 0x6A, 0x02, 0x8B, 0xCF, 0x89, 0x44, 0x24,
                                    0x0C, 0xE8, 0x70, 0x49, 0xE8, 0xFF};

bool g_patched = false;
bool g_refused = false;

// The first calls after the patch carry their own proof: what the copy
// answers and what the renderer's bookkeeping would have said. Two axes per
// placement, so four lines show two placements.
UInt32 g_reportsLeft = 4;

// The getter's own convention: thiscall with one stack argument the callee
// pops. __fastcall with a dummy edx mirrors that exactly, here as at the
// dialogue shim.
using RendererAxisFn = UInt32(__fastcall*)(void* self, void* edx, UInt32 axis);

UInt32 __fastcall CursorAxisShim(void* self, void* /*edx*/, UInt32 axis) {
	const UInt32 copyWidth = *reinterpret_cast<const UInt32*>(addr::kUiScreenWidthCopy);
	const UInt32 copyHeight = *reinterpret_cast<const UInt32*>(addr::kUiScreenHeightCopy);
	const UInt32 value = CursorSurfaceAxis(axis, copyWidth, copyHeight);

	if (g_reportsLeft > 0) {
		--g_reportsLeft;
		auto original = reinterpret_cast<RendererAxisFn>(addr::kRendererSizeGetter);
		OBVR_LOG("Cursor: the sprite gets %u for axis %u from the copy, where the "
		         "renderer's bookkeeping says %u",
		         value, axis, original(self, nullptr, axis));
	}
	return value;
}

// Rewrites one call's rel32 so it reaches the shim instead of the getter.
bool RedirectCall(UInt32 callAt) {
	const UInt32 relative = reinterpret_cast<UInt32>(&CursorAxisShim) - (callAt + 5);
	UInt8 bytes[4];
	bytes[0] = static_cast<UInt8>(relative);
	bytes[1] = static_cast<UInt8>(relative >> 8);
	bytes[2] = static_cast<UInt8>(relative >> 16);
	bytes[3] = static_cast<UInt8>(relative >> 24);
	return mem::SafeWrite(callAt + 1, bytes, sizeof(bytes));
}

}  // namespace

void ApplyCursorMap(bool raised) {
	switch (DecideCursorMap(raised, g_patched)) {
		case CursorMapAction::Nothing:
			return;

		case CursorMapAction::Patch: {
			if (!mem::Verify(addr::kCursorSizeCalls, kOriginalBytes, sizeof(kOriginalBytes))) {
				if (!g_refused) {
					g_refused = true;
					OBVR_LOG("Cursor: the bytes at %08X are not the cursor placement this "
					         "build knows, so the sprite keeps the renderer's size and "
					         "will sit below its own clicks",
					         addr::kCursorSizeCalls);
				}
				return;
			}

			const bool width =
			    RedirectCall(addr::kCursorSizeCalls + addr::kCursorSizeCallWidthOffset);
			const bool height =
			    width && RedirectCall(addr::kCursorSizeCalls + addr::kCursorSizeCallHeightOffset);
			if (width && !height) {
				// Half a redirect would mix the two spaces inside one
				// conversion; the whole sequence goes back.
				mem::SafeWrite(addr::kCursorSizeCalls, kOriginalBytes, sizeof(kOriginalBytes));
				return;
			}
			if (width && height) {
				g_patched = true;
				OBVR_LOG("Cursor: both size calls at %08X redirected to the copy - sprite "
				         "and hit test are one point again",
				         addr::kCursorSizeCalls);
			}
			return;
		}

		case CursorMapAction::Restore:
			if (mem::SafeWrite(addr::kCursorSizeCalls, kOriginalBytes, sizeof(kOriginalBytes))) {
				g_patched = false;
				OBVR_LOG("Cursor: the raise was taken back, and the sprite placement reads "
				         "the renderer again");
			}
			return;
	}
}

}  // namespace obvr::render
