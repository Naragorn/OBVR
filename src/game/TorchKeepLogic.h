#pragma once

// The torch kept in the other hand while a two-hander is drawn (Nadi,
// 2026-10-26: "Auch wenn ich 2 hander habe rechts gezogen bleibt die fackel
// in der linken Hand aktiv").
//
// The engine's decision, read in Oblivion.exe 1.2.0.416 (objdump,
// 2026-10-26): 0x005E7230, an Actor method (thiscall, esi = the actor, its
// process at +0x58), decides what the left hand holds. It reads the torch
// (process vtable +0xF0), the shield (+0xF8) and the weapon (+0xEC, kept at
// [esp+20h]), whether the weapon is out ([esp+10h], from +0x304 or +0x2FC)
// and a "left hand needed" flag ([esp+12h], set at 0x005E7345 for weapon types
// 1, 3, 4 and 5 - the two-handed blade and blunt, the staff, the bow). With
// that flag and the weapon out it hides the worn torch - the 3D taken off
// (0x004DCAB0) and the worn list cleared from the process's copy (0x004853B0),
// no unequip - on three paths:
//
//   0x005E73E6  je 005E7A02: a weapon equipped (0x005E73DB with "worn"), or a
//               torch equipped with the flag (0x005E75A1 jumps here) - the
//               hide follows at 0x005E73EC
//   0x005E76C6  cmp byte [esp+10h], 0 / je 005E76F1: the refresh with no item,
//               the one the draw calls (0x005FD785) - the hide follows at
//               0x005E76CD, the sheathed path at 0x005E76F1 puts the torch on
//
// A one-hander keeps it in vanilla (the hide there needs a shield too). Each
// site jumps to a stub: the engine's own test first, then - with OBVR's flag
// set, the actor the player, the weapon a two-handed blade or blunt - on
// past the hide as if the weapon were sheathed. The staff and the bow keep
// the engine's way: the bow is held in that hand. The flag is a byte OBVR
// sets each frame ([Hands] TorchWithTwoHander, Full VR on).
//
// Pure; covered by torch_keep_test. game/TorchKeep.cpp checks the bytes and
// writes the jumps.

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kPlayerPointerAddress = 0x00B333C4;

inline constexpr UInt32 kTorchEquipSite = 0x005E73E6;
inline constexpr UInt8 kTorchEquipSiteBytes[6] = {0x0F, 0x84, 0x16, 0x06, 0x00, 0x00};
inline constexpr UInt32 kTorchEquipSkip = 0x005E7A02;
inline constexpr UInt32 kTorchEquipHide = 0x005E73EC;

inline constexpr UInt32 kTorchRefreshSite = 0x005E76C6;
inline constexpr UInt8 kTorchRefreshSiteBytes[7] = {0x80, 0x7C, 0x24, 0x10, 0x00, 0x74, 0x24};
inline constexpr UInt32 kTorchRefreshShow = 0x005E76F1;
inline constexpr UInt32 kTorchRefreshHide = 0x005E76CD;

enum class TorchKeepSite : UInt8 { Equip, Refresh };

// The weapon types whose draw keeps the torch (TESObjectWEAP +0x90).
inline bool TorchKeptWith(UInt32 weaponType) { return weaponType == 1 || weaponType == 3; }

inline constexpr UInt32 kTorchKeepBodySize = 57;
inline constexpr UInt32 kTorchKeepStubMax = 11 + kTorchKeepBodySize;

namespace detail {

inline void PutLe32(UInt8* p, UInt32 v) {
	p[0] = static_cast<UInt8>(v & 0xFF);
	p[1] = static_cast<UInt8>((v >> 8) & 0xFF);
	p[2] = static_cast<UInt8>((v >> 16) & 0xFF);
	p[3] = static_cast<UInt8>((v >> 24) & 0xFF);
}

// A rel32 at `at` in code placed at `base`, for a jump ending at at + 4.
inline void PutRel32(UInt8* code, UInt32 base, UInt32 at, UInt32 target) {
	PutLe32(code + at, target - (base + at + 4));
}

}  // namespace detail

// The stub for `site`, to lie at `stubAddress`: the engine's own test, then
// the keep's. Answers the bytes written, 0 without room.
//
//   [equip]    je   skip                     ; not worn / not drawn, as before
//   [refresh]  cmp  byte [esp+10h], 0
//              je   show                     ; sheathed, as before
//              cmp  byte [flag], 0
//              je   hide
//              mov  eax, [00B333C4h]         ; the player
//              cmp  eax, esi
//              jne  hide
//              mov  eax, [esp+20h]           ; the weapon's entry
//              test eax, eax
//              je   hide
//              mov  eax, [eax+8]             ; its form
//              test eax, eax
//              je   hide
//              mov  al, [eax+90h]            ; its type
//              cmp  al, 1
//              je   keep
//              cmp  al, 3
//              je   keep
//   hide:      jmp  <the engine's hide>
//   keep:      jmp  <the engine's skip / show>
inline UInt32 BuildTorchKeepStub(UInt8* out, UInt32 capacity, UInt32 stubAddress, TorchKeepSite site,
                                  UInt32 flagAddress) {
	const UInt32 prefix = site == TorchKeepSite::Equip ? 6u : 11u;
	if (out == nullptr || capacity < prefix + kTorchKeepBodySize) {
		return 0;
	}
	const UInt32 skip = site == TorchKeepSite::Equip ? kTorchEquipSkip : kTorchRefreshShow;
	const UInt32 hide = site == TorchKeepSite::Equip ? kTorchEquipHide : kTorchRefreshHide;
	UInt32 at = 0;
	if (site == TorchKeepSite::Refresh) {
		const UInt8 compare[5] = {0x80, 0x7C, 0x24, 0x10, 0x00};
		for (UInt8 b : compare) {
			out[at++] = b;
		}
	}
	out[at++] = 0x0F;
	out[at++] = 0x84;
	detail::PutRel32(out, stubAddress, at, skip);
	at += 4;
	const UInt8 body[kTorchKeepBodySize] = {
		0x80, 0x3D, 0, 0, 0, 0, 0x00,      // 0  cmp byte [flag], 0
		0x74, 47 - 9,                      // 7  je hide
		0xA1, 0, 0, 0, 0,                  // 9  mov eax, [player]
		0x3B, 0xC6,                        // 14 cmp eax, esi
		0x75, 47 - 18,                     // 16 jne hide
		0x8B, 0x44, 0x24, 0x20,            // 18 mov eax, [esp+20h]
		0x85, 0xC0,                        // 22 test eax, eax
		0x74, 47 - 26,                     // 24 je hide
		0x8B, 0x40, 0x08,                  // 26 mov eax, [eax+8]
		0x85, 0xC0,                        // 29 test eax, eax
		0x74, 47 - 33,                     // 31 je hide
		0x8A, 0x80, 0x90, 0x00, 0x00, 0x00,  // 33 mov al, [eax+90h]
		0x3C, 0x01,                        // 39 cmp al, 1
		0x74, 52 - 43,                     // 41 je keep
		0x3C, 0x03,                        // 43 cmp al, 3
		0x74, 52 - 47,                     // 45 je keep
		0xE9, 0, 0, 0, 0,                  // 47 hide: jmp <hide>
		0xE9, 0, 0, 0, 0,                  // 52 keep: jmp <skip>
	};
	const UInt32 b = at;
	for (UInt32 i = 0; i < kTorchKeepBodySize; ++i) {
		out[at++] = body[i];
	}
	detail::PutLe32(out + b + 2, flagAddress);
	detail::PutLe32(out + b + 10, kPlayerPointerAddress);
	detail::PutRel32(out, stubAddress, b + 48, hide);
	detail::PutRel32(out, stubAddress, b + 53, skip);
	return at;
}

// The jump written at `site` to the stub, the rest of the replaced bytes
// nops. Answers the bytes written (6 or 7), 0 without room.
inline UInt32 BuildTorchKeepSitePatch(UInt8* out, UInt32 capacity, TorchKeepSite site, UInt32 stubAddress) {
	const UInt32 size = site == TorchKeepSite::Equip ? 6u : 7u;
	const UInt32 address = site == TorchKeepSite::Equip ? kTorchEquipSite : kTorchRefreshSite;
	if (out == nullptr || capacity < size) {
		return 0;
	}
	out[0] = 0xE9;
	detail::PutRel32(out, address, 1, stubAddress);
	for (UInt32 i = 5; i < size; ++i) {
		out[i] = 0x90;
	}
	return size;
}

}  // namespace obvr::game
