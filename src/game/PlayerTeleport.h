#pragma once

#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::game {

// The engine side of the Full VR teleport (vr::Teleport holds the logic).
// Read from Oblivion.exe 1.2.0.416 on 2026-09-27:
//
// - Moving the player: the script command SetPos (0x00508FC0) for an actor
//   is TESObjectREFR::SetPos 0x004D8A30 (thiscall, x, y, z by value, ret 0Ch)
//   and then, unless the character controller's state (0x0088D370 on
//   ctrl+0x1E0) is 4, the controller placed too (0x00452A10, thiscall(ctrl,
//   const float* xyz in game units), ret 4, which scales to Havok and moves
//   the proxy), then the 3D node's local translation (+0x54) written,
//   0x00897A20(node, 1) (cdecl) and the node's world update 0x00707370
//   (thiscall(node, float 0, int 0)). PlacePlayerAt runs the same sequence,
//   so nothing snaps back. The controller of an actor is 0x0065A2C0.
// - The ray: bhkWorld's vtable +0x88 (0x0088B850, thiscall(world,
//   bhkPickData*), ret 4, bool), as the grab ray does at 0x0066DB83. The
//   world is the interior cell's (0x00424180 on cell+0x28) or the exterior
//   one at [0x00B35C24]; the cell is 0x006ECC80(player), interior when
//   0x004C97F0(cell). The pick data (0x80 bytes, 16-aligned, zeroed): from
//   at +0x00 and the ray at +0x60, both set from game units by 0x004F8840 /
//   0x00663FF0 (they scale by 0x00A39088), the end derived by 0x0043F450;
//   +0x24 the filter (the player's group << 16 | layer); out: +0x30 the
//   normal, +0x44 the hit fraction (1 none), +0x50 the collidable.
//   The layer is 31, OL_DROPPING_PICK (niflib enums.h OblivionLayer): "where
//   would a dropped thing land", and outside 20..29, the layers the engine
//   counts against a per-frame pick budget (0x00BA7924 against [0x00B2E2F4]).
//   That it collides with the ground and statics is inferred from its name.
// - Untouchable: the console's tgm flag (get 0x0065D820, set 0x0065D810
//   cdecl(bool)). With it the player's actor value damage returns early for
//   Health, Magicka and Fatigue (0x0065E530 and its siblings), which covers
//   weapons and falls; spells and traps are expected to take the same path
//   (not traced). It also blocks fatigue loss, so the price is paid first.
// - Fatigue: the jump's price (0x00672AEF): 0x00547F60(encumbrance / max)
//   (fFatigueJumpMult * x + fFatigueJumpBase), times fPerkJumpFatigueExpertMult
//   (0x00B37510, through 0x00403C00) from Acrobatics mastery 3. The dodge
//   roll is a jump with the block held and pays the same (its action 9 is
//   set by 0x005F5050, which has no fatigue code of its own). Current
//   fatigue vtbl+0x288 (GetAV_F, 10), the base 0x005F1910 (10); paid through
//   0x005E07D0 (thiscall(actor, float delta)), as the jump pays.

struct WorldPick {
	bool hit = false;
	NiPoint3 point{0.0f, 0.0f, 0.0f};
	NiPoint3 normal{0.0f, 0.0f, 1.0f};
	float fraction = 1.0f;    // along the segment, 0..1
	UInt32 collidable = 0;    // the hkCollidable hit, 0 for none
};

// The Havok layers a pick may ask as (niflib enums.h, OblivionLayer): what
// the pick collides with is what that layer does. 31, OL_DROPPING_PICK, for
// where a dropped thing would land; 4, OL_CLUTTER, as the grab's own update
// asks - the held body's layer (0x0066DAB1..0x0066DABB: the player's group
// << 16 | the low word of the body's filter) - for what clutter meets.
inline constexpr UInt32 kLayerClutter = 4;
inline constexpr UInt32 kLayerDroppingPick = 31;

// The closest thing on the segment from -> to (game units), ignoring the
// player. False when the world could not be asked.
bool PickWorldSegment(const NiPoint3& from, const NiPoint3& to, WorldPick& out,
                      UInt32 layer = kLayerDroppingPick);

// The Havok world the player stands in (the bhkWorld: the interior cell's,
// or the exterior one), 0 for none. Its hkWorld is at +8.
UInt32 PlayerBhkWorld();

// The player's collision group, from the character controller's filter
// (9 when it cannot be read).
UInt32 PlayerCollisionGroup();

// The player's position (the feet), game units.
bool ReadPlayerFeet(NiPoint3& out);

// Puts the player at a position, the way SetPos does.
bool PlacePlayerAt(const NiPoint3& at);

// Fatigue now and at most; false when the player cannot be read.
bool ReadPlayerFatigue(float& now, float& base);

// What a dodge roll (a jump) costs the player now, vanilla's formula.
float PlayerDodgeFatigueCost();

// Takes `amount` of fatigue, as a jump does. Never below zero.
bool SpendPlayerFatigue(float amount);

// How high the player can jump, game units: the controller's own apex
// (ctrl+0x31C, Havok units, set by 0x0065AB40), or 64 + Acrobatics.
float PlayerJumpUnits();

bool PlayerInCombat();
bool PlayerRiding();

// While on, the player cannot be hurt (the tgm flag). Turning it off puts
// back what it was before - a player in god mode stays in it.
void SetPlayerUntouchable(bool on);

}  // namespace obvr::game
