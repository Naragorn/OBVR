#include "game/DialogZoom.h"

#include "core/Config.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "game/GameAddresses.h"

namespace obvr::game {
namespace {

// The first eight bytes of SetDialogCamera in 1.2.0.416, read from this
// machine's Oblivion.exe - sub esp,18h; push ebp; mov ebp,[esp+20h]. Eight
// rather than the five being replaced, because the check is "is this the
// function I read", not "is there room".
constexpr UInt8 kOriginalBytes[] = {0x83, 0xEC, 0x18, 0x55, 0x8B, 0x6C, 0x24, 0x20};

// How many bytes the jmp rel32 takes. The five land inside the first two
// instructions (three bytes and one), so the tail of the second is orphaned -
// harmless, because nothing ever runs past the jump.
constexpr UInt32 kJumpSize = 5;

// Whether the shim flipped the player into first person for the current
// conversation, so only that flip is undone. A player already in first
// person is left exactly as they are, both ways.
bool g_flippedForDialog = false;

// The game's own point-of-view switch; see kToggleCamera for the three
// sources. __fastcall with a dummy edx mirrors __thiscall's register use,
// and the byte argument travels the stack in both.
using ToggleCameraFn = void(__fastcall*)(UInt8* self, void* edx, UInt8 firstPerson);

// How many shim invocations are still logged. The game's call pattern for
// SetDialogCamera - how often, with which arguments, at a conversation's
// start, middle and end - has never been observed, only assumed, and the
// first assumption (start once, end once) cost the second dialogue its
// flip. These lines are what replaces the assumption.
UInt32 g_shimReportsLeft = 12;

// The stand-in for SetDialogCamera. Reached by jmp, so the register and
// stack state are exactly the original call's: this in ecx, then the Actor,
// the float and the byte on the stack - which is precisely what a __fastcall
// with a dummy edx and three stack arguments receives and cleans up.
//
// One of the original's two jobs is kept and one is dropped. Kept: a
// third-person player is flipped into first person when a conversation
// starts (a non-null Actor) and back when it ends (null) - vanilla does this
// through the very same ToggleCamera, and losing it was the bare ret's
// mistake. Dropped: the camera transition, which is the zoom and the dead
// time it spends. The flip itself is switchable: Look.DialogFirstPerson,
// on by default, read live so the hot reload reaches it.
void __fastcall DialogCameraShim(UInt8* player, void* /*edx*/, void* actor, float focus,
                                 UInt32 flag) {
	if (player == nullptr) {
		return;
	}

	const bool isThirdPerson = player[addr::kPlayerIsThirdPersonOffset] != 0;
	const DialogPovAction action =
		DecideDialogPov(actor != nullptr, isThirdPerson, g_flippedForDialog,
	                    GetConfig().dialogFirstPerson);

	if (g_shimReportsLeft > 0) {
		--g_shimReportsLeft;
		OBVR_LOG("Dialog: SetDialogCamera(actor=%p, focus=%g, flag=%u), third person=%d - "
		         "%s",
		         actor, static_cast<double>(focus), flag, isThirdPerson ? 1 : 0,
		         action == DialogPovAction::FlipToFirst
		             ? "flipping to first person"
		             : (action == DialogPovAction::FlipBack ? "flipping back to third"
		                                                    : "nothing to do"));
	}

	auto toggleCamera = reinterpret_cast<ToggleCameraFn>(addr::kToggleCamera);
	switch (action) {
		case DialogPovAction::Nothing:
			return;
		case DialogPovAction::FlipToFirst:
			g_flippedForDialog = true;
			toggleCamera(player, nullptr, 1);
			return;
		case DialogPovAction::FlipBack:
			g_flippedForDialog = false;
			toggleCamera(player, nullptr, 0);
			return;
	}
}

bool g_patched = false;
bool g_refused = false;

}  // namespace

void ApplyDialogZoom(bool zoomWanted) {
	switch (DecideDialogZoom(zoomWanted, g_patched)) {
		case DialogZoomAction::Nothing:
			return;

		case DialogZoomAction::Patch: {
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

			UInt8 jump[kJumpSize];
			jump[0] = 0xE9;
			const UInt32 relative = reinterpret_cast<UInt32>(&DialogCameraShim) -
			                        (addr::kSetDialogCamera + kJumpSize);
			jump[1] = static_cast<UInt8>(relative);
			jump[2] = static_cast<UInt8>(relative >> 8);
			jump[3] = static_cast<UInt8>(relative >> 16);
			jump[4] = static_cast<UInt8>(relative >> 24);

			if (mem::SafeWrite(addr::kSetDialogCamera, jump, kJumpSize)) {
				g_patched = true;
				OBVR_LOG("Dialog: the dialogue camera zoom is off - SetDialogCamera at %08X "
				         "jumps to the shim, which keeps the first-person flip and skips "
				         "the transition",
				         addr::kSetDialogCamera);
			}
			return;
		}

		case DialogZoomAction::Restore:
			if (mem::SafeWrite(addr::kSetDialogCamera, kOriginalBytes, kJumpSize)) {
				g_patched = false;
				OBVR_LOG("Dialog: the dialogue camera zoom is back on - SetDialogCamera at "
				         "%08X restored",
				         addr::kSetDialogCamera);
			}
			return;
	}
}

}  // namespace obvr::game
