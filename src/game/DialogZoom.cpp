#include "game/DialogZoom.h"

#include "core/AddressSpace.h"
#include "core/AtomicFlag.h"
#include "core/Config.h"
#include "core/EntryDetour.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "core/Types.h"
#include "game/GameAddresses.h"
#include "game/PlayerLookAt.h"

namespace obvr::game {
namespace {

// The first eight bytes of SetDialogCamera in 1.2.0.416, read from this
// machine's Oblivion.exe - sub esp,18h; push ebp; mov ebp,[esp+20h]. These
// complete instructions can be replayed unchanged in the original trampoline.
constexpr UInt8 kOriginalBytes[] = {0x83, 0xEC, 0x18, 0x55, 0x8B, 0x6C, 0x24, 0x20};

using DialogCameraFn = void(__fastcall*)(UInt8*, void*, void*, float, UInt32);
DialogCameraFn g_original = nullptr;
bool g_zoomWanted = false;
DialogZoomRoute g_route;

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

// Set by the shim when the call carries an actor; taken once a frame by the
// camera hook, which hides the hands for the conversation's approach.
AtomicFlag g_calledWithActor;

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

	const bool speakerValid = mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(actor));
	NiPoint3 speaker{};
	if (speakerValid) {
		speaker = *reinterpret_cast<const NiPoint3*>(
			static_cast<const UInt8*>(actor) + addr::kRefPositionOffset);
	}
	// Before ToggleCamera or the original camera can move the player's view.
	ObservePlayerDialog(actor != nullptr, speakerValid, speaker);
	if (actor != nullptr) g_calledWithActor.Set(true);
	if (g_route.UseOriginal(actor != nullptr, g_zoomWanted)) {
		g_original(player, nullptr, actor, focus, flag);
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
	g_zoomWanted = zoomWanted;
	if (!PlayerDialogActive()) g_route.inConversation = false;
	// Observation must also work with vanilla zoom enabled. The verified
	// eight-byte prologue consists of whole, non-relative instructions.
	if (g_patched || g_refused) return;
	g_refused = true;  // allocation/write failures do not leak a retry each frame
	if (!mem::Verify(addr::kSetDialogCamera, kOriginalBytes, sizeof(kOriginalBytes))) {
		OBVR_LOG("Dialog: camera entry differs; conversation focus hook refused");
		mem::ReportForeignCode("Dialog", addr::kSetDialogCamera);
		return;
	}
	constexpr UInt32 capacity = 24;
	auto* trampoline = static_cast<UInt8*>(mem::AllocExecutable(capacity));
	if (trampoline == nullptr) {
		OBVR_LOG("Dialog: no executable memory for camera observer");
		return;
	}
	if (mem::BuildEntryTrampoline(trampoline, capacity, reinterpret_cast<UInt32>(trampoline),
	                              addr::kSetDialogCamera, kOriginalBytes, sizeof(kOriginalBytes)) == 0) {
		OBVR_LOG("Dialog: camera observer trampoline did not fit");
		return;
	}
	g_original = reinterpret_cast<DialogCameraFn>(trampoline);
	UInt8 patch[sizeof(kOriginalBytes)];
	if (mem::BuildEntryPatch(patch, sizeof(patch), addr::kSetDialogCamera,
	                         reinterpret_cast<UInt32>(&DialogCameraShim), sizeof(kOriginalBytes)) == 0 ||
	    !mem::SafeWrite(addr::kSetDialogCamera, patch, sizeof(patch))) {
		OBVR_LOG("Dialog: could not install camera observer");
		return;
	}
	g_patched = true;
	OBVR_LOG("Dialog: camera observer installed; actual speaker and pre-dialogue eyes captured, zoom=%d",
	         zoomWanted ? 1 : 0);
}

bool TakeDialogCameraCall() { return g_calledWithActor.Take(); }
bool DialogCameraCallPending() { return g_calledWithActor.Get(); }

}  // namespace obvr::game
