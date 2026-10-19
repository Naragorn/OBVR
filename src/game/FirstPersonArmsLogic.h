#pragma once

// The pure part of game/FirstPersonArms: whether the first-person skeleton's
// root may be placed by OBVR at all.
//
// On foot the root ("Player1stPerson") hangs under the camera, and a
// rotation and an offset written to it move the arms relative to the view.
// On a horse the engine hangs that whole skeleton under the horse's
// "ActorParent" bone (a node on the horse's spine - its chain read
// 2026-10-19, horse-ride.txt: Camera01 < Bip01 NonAccum < Bip01 < BASE
// Characters\_1stPerson\Skeleton.NIF < Player1stPerson < ActorParent <
// Bip02 Spine2) and takes the mounted first-person camera's POSITION from
// the Camera01 inside it (FUN_0066b710, [0x00B3BB0C]). The same writes then
// move the engine's camera with the hands' offset, the headset's eyes with
// it, and the view slid sideways and dipped 40-95 units while the head was
// turned (Nadi, 2026-10-18; measured 2026-10-19 11:18). Under ActorParent
// the root is left where the engine's mount animation puts it; the hands
// are pinned to the controllers by the bone pins either way.

namespace obvr::game {

inline bool ArmsRootUnderHorse(const char* parentName) {
	if (parentName == nullptr) {
		return false;
	}
	static const char kActorParent[] = "ActorParent";
	for (unsigned at = 0; at < sizeof(kActorParent); ++at) {
		if (parentName[at] != kActorParent[at]) {
			return false;
		}
	}
	return true;
}

}  // namespace obvr::game
