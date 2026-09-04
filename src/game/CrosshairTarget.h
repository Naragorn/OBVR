#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// What the player is currently aiming at, as far as the engine will say.
//
// This exists for one number: how far away the thing under the crosshair is,
// so the crosshair quad can be hung at that depth instead of at a fixed one.
// The reason it has to be hung at the right depth at all is a fact about eyes
// rather than about rendering - see CrosshairLayer, and camera::CrosshairDepth
// for the arithmetic and the limits.
//
// WHAT THE ENGINE WILL AND WILL NOT SAY. Oblivion keeps the reference under
// the crosshair in HUDInfoMenu::crosshairRef, which is how the name of a door
// or a person appears when it is looked at. It is set only for things that can
// be ACTIVATED, and only within iActivatePickLength - 150 units by default,
// which is 2.14 metres. A wall, the ground, the horizon: nothing. There is no
// hit point and no distance anywhere in it, only the reference. OBVR projects
// that origin along the gaze; it deliberately does not use the scene node's
// broad bound, whose near edge caused a discontinuous middle-distance error.
//
// That sounds thin and is in fact sufficient, because the range where a fixed
// crosshair fails is the same range. The vergence error between a crosshair at
// c and a target at d goes as IPD * (1/d - 1/c): bounded as d grows, divergent
// as d shrinks. Under about two metres a fixed quad doubles badly; past it,
// never by more than about a degree. The pick reaches 2.14 m. The engine tells
// us what we need exactly where we need it, and nothing where we do not.
//
// HOW THE ADDRESS IS TRUSTED. It is not. HUDInfoMenu is reached through the
// global at kHudInfoMenuPointer, and then the object is asked what it is: a
// Menu carries its own type id, and it has to answer kMenuIdHudInfo before a
// further byte is read from it. A wrong global leads to something that is not
// that menu and is refused; a right one identifies itself. Every pointer along
// the way is also checked for looking like a Gamebryo object at all, the same
// test PlayerAim applies and for the same reason - this runs on frames where
// the object model is being torn down and rebuilt, and a global caught
// mid-assignment is not necessarily null.
//
// Nothing here is ever written. This is a read of the game's own state.
struct CrosshairTarget {
	// The HUDInfoMenu was found and identified itself. False means the global
	// did not lead to that menu, and everything below is meaningless - the
	// caller must fall back rather than treat a false as "nothing targeted".
	bool haveMenu = false;

	// Something is under the crosshair and its position was read. False is an
	// ordinary answer, not a failure: most of what a player looks at cannot be
	// activated and sets no reference.
	bool haveRef = false;

	// The reference's world position in Oblivion units, valid only when
	// haveRef. This is the object's ORIGIN, which for an actor is between its
	// feet - see camera::CrosshairDepth, which is why the depth is taken along
	// the view axis rather than as a straight-line distance.
	NiPoint3 position{};

	// For the log, and for nothing else. menuAddress and refAddress say what
	// was found; arrayEntry is the tile menu array's own entry for HUDInfo,
	// reported so that a run can show whether the two descriptions of the menu
	// system agree in practice. arrayEntry is a TileMenu, not a Menu, so it is
	// never dereferenced - its layout is not recorded anywhere OBVR can read.
	UInt32 menuAddress = 0;
	UInt32 refAddress = 0;
	UInt32 arrayEntry = 0;

	// What the object at menuAddress said it was, when it was reachable but
	// answered the wrong id. Zero when the pointer was not followable at all.
	// This is the number to look at if haveMenu is false in a log.
	UInt32 rejectedId = 0;
};

CrosshairTarget ReadCrosshairTarget();

// Makes HUDInfoMenu's existing action-icon tile participate in the isolated
// third-person HUD draw. No icon is invented: Oblivion has already chosen its
// texture and action; this only exposes that tile for the duration of a draw.
bool SetHudInfoActionIconVisible(bool visible);

}  // namespace obvr::game
