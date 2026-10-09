#include "game/BladeContact.h"

#include "game/HandBodyLogic.h"
#include "game/PlayerTeleport.h"
#include "game/WorldPush.h"

namespace obvr::game {

BladeRayHit HavokBladeWorld::Cast(const NiPoint3& from, const NiPoint3& to) {
	++casts;
	BladeRayHit out;
	WorldPick pick;
	if (!PickWorldSegment(from, to, pick, kHandBodyQuietLayer) || !pick.hit) {
		return out;
	}
	out.hit = true;
	out.fraction = pick.fraction;
	out.point = pick.point;
	out.normal = pick.normal;
	UInt32 body = 0;
	UInt32 motion = 0;
	UInt32 layer = 0;
	const bool isBody = ReadPickBody(pick.collidable, body, motion, layer);
	if (!isBody) {
		++notBodies;
	}
	out.kind = ContactKindOf(isBody, motion, layer);
	out.body = body;
	if (out.kind == ContactKind::Fixed) {
		lastLayer = layer;
		lastMotion = motion;
	}
	return out;
}

UInt32 KickBladeKicks(const BladeKicks& kicks) {
	UInt32 moved = 0;
	for (UInt32 i = 0; i < kicks.count; ++i) {
		if (KickBodyByBlade(kicks.kick[i].body, kicks.kick[i].velocity)) {
			++moved;
		}
	}
	return moved;
}

}  // namespace obvr::game
