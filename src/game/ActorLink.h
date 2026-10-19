#pragma once

// Whether an actor's values may be read. The actor value getters (vtable
// +0x284 and +0x288, Player::GetActorValue_F 0x0065E110) end in GetBaseAV
// (0x005EAD00), which calls through the base form (TESObjectREFR +0x1C) with
// no check. A tester's game (2026-10-18) crashed there five seconds after the
// start, in the intro videos: the player object stood, its base form not yet
// linked while his many plugins were still loading, and OBVR's exhaustion
// step had read the player's fatigue at every Present since the first flat
// frame. An actor is linked once its base form is an object; until then no
// value of it is read, and the readers answer "unknown".

namespace obvr::game {

inline bool ActorValuesReadable(bool actorLooks, bool baseFormLooks) {
	return actorLooks && baseFormLooks;
}

}  // namespace obvr::game
