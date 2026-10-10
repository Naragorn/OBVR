#pragma once

#include "core/Types.h"

namespace obvr::game {

// Render.UnpausedMenus: the world keeps running behind the player's own
// menus - inventory, magic, map, stats, containers, books - while every
// other menu pauses it as vanilla does.
//
// How: the update step asks IsMenuMode afresh for each subsystem it pauses,
// and those calls (kUpdateStepIsMenuModeSites) are redirected to a function
// that answers "paused" only for the menus vanilla should still pause. The
// player's controls stay gated by the untouched vanilla call, so the
// character does not walk while the inventory is open. See
// MenuPausePolicy.h for the decision and GameAddresses.h for the sites.
//
// Asked every frame, like ApplyLiveMenuBackground: the sites are redirected
// once, the first time the option is on, and the answer follows the option
// from then on, so switching it off in the INI restores vanilla answers
// without touching code again.
void ApplyUnpausedMenus(bool wanted, bool containerWanted = false, bool lockWanted = false);

// The sticks walk the player under a container's menu opened by reaching
// (vr/ReachOpen.h): the update step's one site that gates the player's
// controls (addr::kPlayerControlsIsMenuModeSite) answers "no menu" while
// this is set and the container's menu is on top (MenuPausePolicy.h,
// PlayerControlsRunUnderMenu). The site is redirected the first time it is
// asked for; vanilla's answer otherwise.
void SetPlayerWalksUnderMenu(bool walking);

// The id at the top of the interface manager's menu stack
// (GetTopVisibleMenuID), kMenuIdNone with none - unlike ActiveMenuId, which
// is the menu under the mouse and empties whenever the cursor is off it.
UInt32 TopVisibleMenu();

}  // namespace obvr::game
