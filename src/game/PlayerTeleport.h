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

// Puts any actor there the same way (its controller placed too, its node
// updated): the off hand's drag of a held fighter (game/GrappleLogic.h).
// False for an address that is no object or a position that is not a
// number.
bool PlaceActorAt(void* actor, const NiPoint3& at);

// Fatigue now and at most; false when the player cannot be read.
bool ReadPlayerFatigue(float& now, float& base);

// Health the same way (actor value 8).
bool ReadPlayerHealth(float& now, float& base);

// Whether the player is paralysed: actor value 0x30 (Paralysis) above 0,
// as the engine's own check asks it (0x005E17E0; game/PlayerStagger.h).
bool PlayerParalyzed();

// The player's Strength now (actor value 0, with its modifiers; the same
// getter as the paralysis); -1 when nothing reads.
SInt32 PlayerStrength();

// The player's sex as the game's GetIsSex reads it: 1 female, 0 male, -1
// when the base form is no NPC or nothing reads.
SInt32 PlayerSex();

// The player's encumbrance as the game counts it now (actor value 11, the
// weight carried with every script's modAV on it); false when the player
// cannot be read.
bool ReadPlayerEncumbrance(float& now);

// What a dodge roll (a jump) costs the player now, vanilla's formula.
float PlayerDodgeFatigueCost();

// Takes `amount` of fatigue, as a jump does. Never below zero. A cost the
// player makes - `scaled` - is dearer while a two-hander is held in one hand
// (game/FatigueCost.h); the hold's own burn is not scaled again.
bool SpendPlayerFatigue(float amount, bool scaled = true);

// How high the player can jump, game units: the controller's own apex
// (ctrl+0x31C, Havok units, set by 0x0065AB40), or 64 + Acrobatics.
float PlayerJumpUnits();

bool PlayerInCombat();
bool PlayerRiding();
// The horse the player sits on (the actor virtual GetMountedHorse, xOBSE's
// slot 0x380), 0 on foot.
UInt32 PlayerMountedHorse();
// The player's mounted yaw, the float at +0x61C the input handler
// accumulates instead of rotZ while mounted (GameAddresses.h, the heading
// virtual 0x0065DA60 adds it to rotZ); 0 when the player cannot be read.
float PlayerMountedYaw();
// An actor's heading (rotZ, radians) and position, by its address; false
// when it does not look like an object.
bool ReadActorPlacement(UInt32 actor, float& rotZ, NiPoint3& position);
// The player's two animation bytes Enhanced Camera reads at +0x71C ("Sit
// Down, Mount Horse, Vampire Feed") and +0x71D ("Sit GetUp, Dismount
// Horse"), the first in the low byte, the second in the high; 0 when the
// player cannot be read. Unverified against the engine - logged first.
UInt16 PlayerSitMountBytes();
// The player's high process's sit/sleep state (+0x11D: 0 none, 3 sitting
// down, 4 seated, 5 getting up, 8 and over asleep - Enhanced Camera's
// reading); 0 when the player or the process cannot be read.
UInt8 PlayerSitSleepState();
// The player's knock state (the high process's +0x11C: 0 standing, 1 to 4
// down, 5 and 6 getting up, GrappleLogic.h's reading); -1 when the player or
// the process cannot be read.
SInt32 PlayerKnockState();
// Whether the player is in the character generation of a new game (the
// byte at +0x6E5 the console's SetInChargen sets); false when the player
// cannot be read.
bool PlayerInChargen();
// The centre of the player's face: the world bound's centre of the FaceGen
// node the player's virtual at 0x134 returns - what the race menu's own
// camera (0x005C2BF0) stands before. False when the player, the node or the
// centre cannot be read.
bool PlayerFaceCentre(NiPoint3& out);
// The game's third-person zoom (the float at 0x00B3BB24 the mount, the
// dismount and the vampire feed write before switching the view, Enhanced
// Camera's hooks at 0x0066CED2, 0x0066CF41 and 0x0066CCA0).
float ThirdPersonZoom();

// While on, the player cannot be hurt (the tgm flag). Turning it off puts
// back what it was before - a player in god mode stays in it.
void SetPlayerUntouchable(bool on);

}  // namespace obvr::game
