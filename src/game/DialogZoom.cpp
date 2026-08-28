#include "game/DialogZoom.h"

#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// The first eight bytes of SetDialogCamera in 1.2.0.416, read from this
// machine's Oblivion.exe - sub esp,18h; push ebp; mov ebp,[esp+20h]. Eight
// rather than the three being replaced, because the check is "is this the
// function I read", not "is there room".
constexpr UInt8 kOriginalBytes[] = {0x83, 0xEC, 0x18, 0x55, 0x8B, 0x6C, 0x24, 0x20};

// ret 0Ch: return immediately and clean the three dword arguments, which is
// what __thiscall owes its caller on x86. The this pointer rides in ecx and
// costs nothing.
constexpr UInt8 kReturnBytes[] = {0xC2, 0x0C, 0x00};

bool g_patched = false;
bool g_refused = false;

}  // namespace

void ApplyDialogZoom(bool zoomWanted) {
	switch (DecideDialogZoom(zoomWanted, g_patched)) {
		case DialogZoomAction::Nothing:
			return;

		case DialogZoomAction::Patch:
			// Verified on every application, not just the first: after a
			// restore the bytes should be the originals again, and if some
			// other mod has since claimed the function, patching over it
			// would corrupt whatever it installed.
			if (!mem::Verify(addr::kSetDialogCamera, kOriginalBytes, sizeof(kOriginalBytes))) {
				if (!g_refused) {
					g_refused = true;
					OBVR_LOG("Dialog: the bytes at %08X are not SetDialogCamera as this "
					         "build knows it, so the dialogue zoom stays alive",
					         addr::kSetDialogCamera);
				}
				return;
			}
			if (mem::SafeWrite(addr::kSetDialogCamera, kReturnBytes, sizeof(kReturnBytes))) {
				g_patched = true;
				OBVR_LOG("Dialog: the dialogue camera zoom is off - SetDialogCamera at %08X "
				         "returns without starting the transition",
				         addr::kSetDialogCamera);
			}
			return;

		case DialogZoomAction::Restore:
			if (mem::SafeWrite(addr::kSetDialogCamera, kOriginalBytes, sizeof(kReturnBytes))) {
				g_patched = false;
				OBVR_LOG("Dialog: the dialogue camera zoom is back on - SetDialogCamera at "
				         "%08X restored",
				         addr::kSetDialogCamera);
			}
			return;
	}
}

}  // namespace obvr::game
