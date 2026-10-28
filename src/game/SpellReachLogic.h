#pragma once

#include "core/Types.h"
#include "vr/SpellAim.h"

namespace obvr::game {

// The widest reach among a MagicItem's effects (vr::SpellReach).
//
// The list is the one the projectile factory walks (0x0069A060, read
// 2026-10-28): its head embedded at item+0x0C, a node's effect at node+4 and
// its next node at node+8, the next pointer aimed past the node's first word
// (the walk goes on at next-4) - so the first effect is at item+0x10 and the
// next node's pointer at item+0x14. The engine's own walk compares the
// effect's first word with 'TELE', the effect code at +0x00 as xOBSE's
// EffectItem has it; the range at +0x10 is xOBSE's (GameMagicEffects.h,
// kRange_Self 0, kRange_Touch 1, kRange_Target 2) and not read in the binary.
//
// read answers the word at an address; plausible whether an address may be
// followed. A list longer than maxEffects - a loop, say - ends there.
template <typename Read, typename Plausible>
vr::SpellReach SpellReachOf(UInt32 item, const Read& read, Plausible plausible, int maxEffects) {
	if (!plausible(item)) {
		return vr::SpellReach::Unknown;
	}
	vr::SpellReach widest = vr::SpellReach::Unknown;
	UInt32 node = item + 0x0C;
	for (int i = 0; i < maxEffects; ++i) {
		const UInt32 effect = read(node + 4);
		if (!plausible(effect)) {
			break;
		}
		const UInt32 range = read(effect + 0x10);
		if (range > 2) {
			return vr::SpellReach::Unknown;
		}
		const auto reach = static_cast<vr::SpellReach>(range + 1);
		if (reach > widest) {
			widest = reach;
		}
		const UInt32 next = read(node + 8);
		if (next == 0 || !plausible(next)) {
			break;
		}
		node = next - 4;
	}
	return widest;
}

}  // namespace obvr::game
