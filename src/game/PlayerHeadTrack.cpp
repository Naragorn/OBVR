#include "game/PlayerHeadTrack.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/Memory.h"
#include "game/GameAddresses.h"
#include "game/PlayerHeadTrackLogic.h"

namespace obvr::game {
namespace {

constexpr UInt32 kHighProcessVtable = 0x00A71814;
constexpr UInt32 kHeadTrackSlot = kHighProcessVtable + 0xE8;  // 0x00A718FC
constexpr UInt32 kHeadTrackGetter = 0x0064B350;
// push esi; push edi; mov edi,[esp+0Ch]; mov eax,[edi]; mov edx,[eax+154h];
// mov esi,ecx; mov ecx,edi; call edx; test eax,eax; jnz +0Bh; pop edi;
// mov [esi+15Ch],eax - the object kept at the process's +0x15C.
constexpr UInt8 kHeadTrackGetterBytes[] = {0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0x07, 0x8B, 0x90,
                                           0x54, 0x01, 0x00, 0x00, 0x8B, 0xF1, 0x8B, 0xCF, 0xFF, 0xD2,
                                           0x85, 0xC0, 0x75, 0x0B, 0x5F, 0x89, 0x86, 0x5C, 0x01, 0x00, 0x00};
constexpr UInt32 kProcessHeadTrackOffset = 0x15C;
constexpr UInt32 kHeadTrackStopOffset = 0x1DA;

HeadTrackState g_state;
int g_verified = -1;  // -1 not yet looked at, 0 refused, 1 the game's own

UInt32 Read(UInt32 address) { return *reinterpret_cast<const UInt32*>(address); }

bool Verified() {
	if (g_verified < 0) {
		const UInt8 slot[4] = {0x50, 0xB3, 0x64, 0x00};
		g_verified = mem::Verify(kHeadTrackSlot, slot, sizeof(slot)) &&
		                     mem::Verify(kHeadTrackGetter, kHeadTrackGetterBytes, sizeof(kHeadTrackGetterBytes))
		                 ? 1
		                 : 0;
		OBVR_LOG(g_verified == 1
		             ? "Body: the player's head tracking can be stopped while the body is shown (HighProcess +0xE8 "
		               "keeps its object at the process's +0x15C, its byte +0x1DA stops it)"
		             : "Body: the head-tracking getter is not the game's own - the engine's head tracking stays, the "
		               "upper body may turn to NPCs");
	}
	return g_verified == 1;
}

// The player's head-tracking object, 0 when there is none to write to.
UInt32 PlayerHeadTrackObject() {
	const UInt32 player = Read(addr::kPlayerPointer);
	if (!mem::LooksLikeObjectAddress(player)) {
		return 0;
	}
	const UInt32 process = Read(player + addr::kMobileProcessOffset);
	if (!mem::LooksLikeObjectAddress(process) || Read(process) != kHighProcessVtable) {
		return 0;
	}
	const UInt32 object = Read(process + kProcessHeadTrackOffset);
	return mem::LooksLikeObjectAddress(object) ? object : 0;
}

}  // namespace

void StepPlayerHeadTracking(bool wanted, bool bodyShown) {
	if (!Verified()) {
		return;
	}
	const UInt32 object = PlayerHeadTrackObject();
	UInt8* const stop = object != 0 ? reinterpret_cast<UInt8*>(object + kHeadTrackStopOffset) : nullptr;
	const bool wasFrozen = g_state.frozen;
	switch (StepPlayerHeadTrack(g_state, wanted, bodyShown, object != 0)) {
		case HeadTrackAction::Freeze:
			*stop = 1;
			if (!wasFrozen) {
				OBVR_LOG("Body: the engine's head tracking stopped for the player while the body is shown - the "
				         "upper body no longer turns to NPCs (object %08X)",
				         object);
			}
			break;
		case HeadTrackAction::Release:
			*stop = 0;
			OBVR_LOG("Body: the engine's head tracking given back to the player (object %08X)", object);
			break;
		case HeadTrackAction::None:
			break;
	}
}

}  // namespace obvr::game
