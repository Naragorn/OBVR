#pragma once

// A walker killed by a camp's refugee loses its head (Oblivion Walkers'
// 4.20.6; Nadi, 2026-11-05: the head comes off at such a kill wherever the
// blow landed). Walkers sends 'WGCT' with the dead walker's reference once,
// the frame after the death; OBVR cuts the head as at the player's own cut
// (game/Dismember.h CutHead), which answers 'WGHD' as always. The message
// comes on the sender's call, the cut in the frame: kept here between.
//
// Pure; game/WalkersLink.cpp fills the queue, game/CampKill.cpp empties it.
// camp_kill_test covers every flow.

#include <cstring>

#include "core/Types.h"

namespace obvr::game {

inline constexpr UInt32 kWalkersCampKillMessage = 0x54434757;  // 'WGCT'
inline constexpr UInt32 kCampKillsKept = 8;

inline bool ParseCampKill(const void* data, UInt32 bytes, UInt32& refOut) {
	if (data == nullptr || bytes != sizeof(UInt32)) {
		return false;
	}
	UInt32 ref = 0;
	std::memcpy(&ref, data, sizeof(ref));
	if (ref == 0) {
		return false;
	}
	refOut = ref;
	return true;
}

struct CampKillQueue {
	UInt32 ref[kCampKillsKept] = {};
	UInt32 count = 0;
};

// Keeps a kill; one already kept counts as kept. False when full.
inline bool PushCampKill(CampKillQueue& q, UInt32 ref) {
	for (UInt32 i = 0; i < q.count; ++i) {
		if (q.ref[i] == ref) {
			return true;
		}
	}
	if (q.count >= kCampKillsKept) {
		return false;
	}
	q.ref[q.count++] = ref;
	return true;
}

// The oldest kept, or 0.
inline UInt32 TakeCampKill(CampKillQueue& q) {
	if (q.count == 0) {
		return 0;
	}
	const UInt32 ref = q.ref[0];
	for (UInt32 i = 1; i < q.count; ++i) {
		q.ref[i - 1] = q.ref[i];
	}
	--q.count;
	return ref;
}

}  // namespace obvr::game
