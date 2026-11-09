#pragma once

// Why a person's hold did not slide off them (game/BladeContactLogic.h,
// SlideOffPeople), as the contact trace says it.

#include "core/Types.h"

namespace obvr::game {

// Why a person's hold did not slide this frame (slidesOffPeople), for the
// contact trace: the grip then stays where the frame before left it.
enum class SlideRefusal : UInt8 {
	None,        // slid, or nobody's hold to slide off
	StillIn,     // turned every pass and still in them
	NoWayOut,    // no side out of them (the blade's line on a capsule's axis, nothing to turn about)
	GripIn,      // their deepest point at the grip: the grip itself in them
	AlongBlade,  // the way out along the blade: a thrust at them
	WayCrosses,  // the grip's way to the hand's goes through them
	Fixed,       // the way goes through a wall, a stopper or a shield
	Inside,      // the slid pose inside the world
	Across,      // the slid pose across someone held slowly
};

inline const char* SlideRefusalName(SlideRefusal r) {
	switch (r) {
	case SlideRefusal::None: return "none";
	case SlideRefusal::StillIn: return "still in them";
	case SlideRefusal::NoWayOut: return "no way out";
	case SlideRefusal::GripIn: return "the grip in them";
	case SlideRefusal::AlongBlade: return "along the blade";
	case SlideRefusal::WayCrosses: return "the way crosses them";
	case SlideRefusal::Fixed: return "a wall on the way";
	case SlideRefusal::Inside: return "inside the world";
	case SlideRefusal::Across: return "across someone";
	}
	return "?";
}

}  // namespace obvr::game
