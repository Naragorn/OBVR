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
//
// `tabletWanted` is [Tablet] Enabled in Full VR: the menus held in the hand
// keep the world running whatever the option says (vr/TabletLogic.h,
// TabletMenusUp and TabletMenuRunsWorld).
void ApplyUnpausedMenus(bool wanted, bool containerWanted = false, bool lockWanted = false,
                        bool tabletWanted = false);

// The sticks walk the player under a container's menu opened by reaching
// (vr/ReachOpen.h), or under the tablet's menus (`underTablet`;
// docs/inventory-tablet-spec.md 4.6): the update step's one site that gates
// the player's controls (addr::kPlayerControlsIsMenuModeSite) answers "no
// menu" while this is set and that menu is on top (MenuPausePolicy.h,
// PlayerControlsRunUnderMenu). Until the player's update is done
// (addr::kPlayerControlsScopeEndSite) their control reads are held to the
// walk - the menu's click, which is the attack's button, is no blow - and
// under the tablet the mouse's x and y read 0 to them, so the laser's
// cursor steps turn nobody, and the player's update walks with the player's
// own anim data, not the F1-F4 figure's (PlayerOwnAnimDataForSpan) - its
// root motion is the step.
// The sites are redirected the first time they are asked for; vanilla's
// answers otherwise.
void SetPlayerWalksUnderMenu(bool walking, bool underTablet = false);

// A site the tablet's walk needs could not be taken (another mod's code
// there): the tablet does not walk, its sticks scroll as before.
bool TabletWalkRefused();

// The id at the top of the interface manager's menu stack
// (GetTopVisibleMenuID), kMenuIdNone with none - unlike ActiveMenuId, which
// is the menu under the mouse and empties whenever the cursor is off it.
UInt32 TopVisibleMenu();

// The whole stack, from the bottom up, as GetTopVisibleMenuID reads its ten
// slots (addr::kInterfaceMenuStackOffset): the ids into `out`, stopping at
// the first empty slot or one that is no menu id. Answers how many were
// read, 0 with no manager - for the hands, which stay in some menus and not
// others (camera/FrameLogic.h, HandsAwayForMenus).
inline constexpr UInt32 kMenuStackEntries = 10;
UInt32 MenuStackEntries(UInt32* out, UInt32 capacity);

// Whether the world is paused behind the menus now - the answer the update
// step's redirected sites get (MenuPausePolicy.h, WorldPausesForMenu): no
// outside menu mode; in it vanilla's pause, unless the menu on top keeps
// the world running (UnpausedMenus, the container's and the lock's own).
// Not logged, nothing remembered: for what OBVR itself moves in the world
// each frame (the fighters' ring, game/CombatRing.h).
bool WorldPausedForMenus();

// The dialogue anywhere in the stack now (menu mode).
bool DialogueMenuUp();

// A merchant's barter up now: menu mode, and the dialogue under the
// container's menu in the stack (MenuPausePolicy.h, BarterInMenuStack).
bool BarterMenuUp();

// The barter opening this call (vr::BarterBegan against the last call's
// answer): for the one caller a frame that places the menus' panel.
bool BarterMenuBegan();

}  // namespace obvr::game
