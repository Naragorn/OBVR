#pragma once

#include "core/Types.h"

// Fixed addresses in Oblivion.exe 1.2.0.416 (image base 0x00400000).
//
// Every address here was disassembled from the actual binary and is
// documented below with the evidence. None of it is guessed, and nothing was
// taken from another source without checking it first.

namespace obvr::addr {

// End of the vanilla camera calculation.
//
// The relevant block reads:
//
//   0066BE1B  mov  ebx, [esp+0x14]              ; camera object
//   0066BE1F  cmp  word ptr [ebx+0xB6], 0       ; node list empty?
//   0066BE27  ja   0066BE2D
//   0066BE29  xor  eax, eax
//   0066BE2B  jmp  0066BE35
//   0066BE2D  mov  edx, [ebx+0xB0]              ; node list
//   0066BE33  mov  eax, [edx]                   ; eax = CameraNode (NiAVObject*)
//   0066BE35  mov  ecx, [esp+0x38]
//   0066BE39  mov  edx, [esp+0x3C]
//   0066BE3D  mov  [eax+0x54], ecx              ; localTransform.pos.x
//   0066BE40  mov  ecx, [esp+0x40]
//   0066BE44  mov  [eax+0x58], edx              ; localTransform.pos.y
//   0066BE47  mov  [eax+0x5C], ecx              ; localTransform.pos.z
//   0066BE4A  cmp  word ptr [ebx+0xB6], 0
//   0066BE52  ja   0066BE58
//   0066BE54  xor  eax, eax
//   0066BE56  jmp  0066BE60
//   0066BE58  mov  edx, [ebx+0xB0]
//   0066BE5E  mov  eax, [edx]                   ; eax = CameraNode
//   0066BE60  lea  edi, [eax+0x30]              ; localTransform.rot
//   0066BE63  mov  ecx, 9
//   0066BE68  lea  esi, [esp+0x60]
//   0066BE6C  rep  movsd                        ; 9 DWORDs = NiMatrix33
//   0066BE6E  <-- kHookCameraUpdate
//
// From 0x0066BE6E onwards the camera's position and rotation are settled and
// eax still holds the CameraNode: exactly the point where OBVR wants to lay
// the head rotation on top.
//
// The write targets [eax+0x54] and [eax+0x30] also establish the NiAVObject
// layout in GameTypes.h.
inline constexpr UInt32 kHookCameraUpdate = 0x0066BE6E;

// The original instruction overwritten at kHookCameraUpdate:
//
//   0066BE6E  66 83 BB B6 00 00 00 00   cmp word ptr [ebx+0xB6], 0
//
// Eight bytes, so there is room for a 5-byte jmp. The control flow that
// follows is faithfully rebuilt inside the trampoline:
//
//   0066BE76  ja   0066BE7C     -> kHookCameraUpdateResumeTaken
//   0066BE78  xor  ecx, ecx
//   0066BE7A  jmp  0066BE84     -> kHookCameraUpdateResumeEmpty
inline constexpr UInt32 kHookCameraUpdatePatchSize = 8;
inline constexpr UInt32 kHookCameraUpdateResumeTaken = 0x0066BE7C;
inline constexpr UInt32 kHookCameraUpdateResumeEmpty = 0x0066BE84;

// MagicCaster::CastMagicItem - where a spell actually becomes a thing in the
// world, and the one moment the caster's heading has to be right.
//
// WHY HOOK A FUNCTION AT ALL, when the aim has managed without one until now.
// Turning the player's heading is what makes a spell go where the wearer looks,
// but the heading is also what walking follows - one field, two jobs - so for
// as long as the turn stands the character walks the way the spell went. The
// window was cut from 1.32 seconds to 0.9 by watching the action field, and
// that is the floor: nothing outside the engine knows WHEN inside the cast
// animation the spell is made.
//
// This function does. The heading only has to be right while it runs, so
// setting it here and giving it back leaves walking alone - the cause removed
// rather than another consequence compensated.
//
// TWO SOURCES, as this file requires of every address.
//
// The first is xOBSE, which names it: EventManager.cpp declares
// kMagicCasterCastMagicItemFnAddr = 0x00699190 and calls it through
// ThisStdCall(addr, caster, magicItem, target, noHitVFX). Its OnMagicCast thunk
// ends in `retn 0xC`, which fixes the shape as __thiscall with three stack
// arguments.
//
// The second is the executable itself, read out of Oblivion.exe at the file
// offset that 0x00699190 maps to (.text, ImageBase 0x00400000, so RVA 0x299190
// -> file 0x298590). It begins:
//
//   53              push ebx
//   56              push esi
//   57              push edi
//   8B 7C 24 10     mov  edi, [esp+0x10]     <- first stack argument
//   8B F1           mov  esi, ecx            <- this
//
// Three pushes plus the return address put the first argument at exactly
// [esp+0x10], and `this` arrives in ecx. That is a __thiscall taking arguments,
// which is what xOBSE's call shape says it must be - the two descriptions agree
// without either being derived from the other.
//
// Seven bytes are taken rather than five: a jump needs five, and the fourth
// instruction ends at seven. Half an instruction must never be left standing.
inline constexpr UInt32 kHookMagicCastItem = 0x00699190;
inline constexpr UInt32 kHookMagicCastItemPatchSize = 7;
inline constexpr UInt32 kHookMagicCastItemResume = 0x00699197;

// Where the MagicCaster base subobject sits inside PlayerCharacter.
//
// The hook is handed a MagicCaster*, and the player is a PlayerCharacter*;
// MagicCaster is one of its bases, so telling the player's own casts from an
// NPC's beside him is a matter of this one number.
//
// Two sources, and they were arrived at independently. OBVR measured it at
// runtime, from a cast it knew was the player's, and wrote the result into the
// log: "MagicCaster sits +5C inside the player". xOBSE states the same layout
// in the PlayerCharacter comment block of obse/obse/GameObjects.h:
//
//   // [ vtbl ]
//   // +000 = PlayerCharacter
//   // +018 = TESChildCell
//   // +05C = MagicCaster
//   // +068 = MagicTarget
//
// https://github.com/llde/xOBSE/blob/5078a1dc/obse/obse/GameObjects.h
//
// Written down rather than learned, because learning it needed a cast OBVR
// could already prove was the player's - which meant holding the cast key -
// and a spell cast without that key was left alone. In a recorded run the
// offset was not learned until the seventh window, so six casts before it went
// out unturned and the feature looked as though it did nothing.
inline constexpr UInt32 kPlayerMagicCasterOffset = 0x5C;

// THE ONE CALL THAT READS THE HEADING FOR EVERY ATTACK, for the aim that sets
// it only while that call runs.
//
// A spell leaves along the caster's rotation, an arrow along the shooter's,
// a swing is tested against the attacker's heading - and the player walks
// along the same rotZ. The turn machinery above this file separated those in
// TIME: the body turned for a few frames around the moment that mattered,
// and a few frames of walking pulled towards the aim, and of the engine
// animating a turn, was what remained. This address separates them in PLACE.
// The rotation is set on the way into the call that reads it and put back
// on the way out, so no frame ever sees it. What that call is was read out
// of the binary; the notes give a second source for every part of it.
//
// The function at 0x005FC890 is the handler the animation system invokes
// for an actor's text keys - the attack's hit, the bow's release, the cast.
// It is a virtual on the actor's MagicCaster base: its first instructions
// take `this` in ecx and reach the actor as [ecx-0x5C], the same 0x5C the
// offset above records, and it is reached from nowhere but four vtables -
// Actor, Character, Creature and PlayerCharacter, the last at 0x00A739D4,
// which is slot 0x18 of the vtable whose RTTI locator names PlayerCharacter
// with a subobject offset of 0x5C. (JRoush's Common Oblivion Engine
// Framework lists that slot as ApplyMagicItemCost; the body below is not
// that, and the disassembly is what is trusted here.) Inside it, in one
// invocation each:
//
//   * THE BOW. At 0x005FD250 it asks the shooter's heading through the
//     virtual at +0x1E0, at 0x005FD260 its pitch through 0x004A9720 - which
//     is `fld dword ptr [ecx+0x20]`, the rotX field xOBSE places at +0x20 -
//     allocates 0x9C bytes (the size xOBSE gives ArrowProjectile) and at
//     0x005FD47C calls 0x0060C940, which writes the ArrowProjectile vtable
//     0x00A6F08C (xOBSE's kVtbl_ArrowProjectile) into the new object. The
//     arrow's first update, 0x006079A0, then turns the rotation it was given
//     into its velocity. So the heading the arrow flies along is read here,
//     before the constructor, and nowhere later.
//   * THE SPELL. At 0x005FCE1D and 0x005FCF6E it calls 0x0069BEC0 with
//     ecx = actor + 0x5C. JRoush's MagicCaster.eed exports that address as
//     MagicCaster::UseActiveMagicItem, in the chain CastMagicItem 0x00699190
//     (the address xOBSE and this file already agree on) -> animation ->
//     UseActiveMagicItem -> ApplyActiveMagicItem 0x0069AF30. There the
//     projectile factory 0x0069A060 takes the launch rotation from the
//     caster's rotX/rotY/rotZ fields, and a touch spell finds its target
//     through FindTouchTarget (0x00699500), which uses the hit cone below.
//   * THE SWING. At 0x005FCE85 it calls the actor virtual at +0x3AC with
//     three zero arguments - xOBSE's GameObjects.h declares that slot as
//     AttackHandling(unused, arrowRef, target) and notes "args all null for
//     melee attacks"; the player's vtable holds 0x005FEBF0 there. Inside,
//     the hit cone 0x006131D0 compares the direction to each candidate
//     against the attacker's heading from the virtual at +0x1E0 and the
//     setting registered as "fCombatHitConeAngle".
//
// The heading virtual at +0x1E0 is 0x0065ABB0 for NPCs and creatures - the
// rotZ field - and 0x0065DA60 for the player: rotZ plus the float at
// +0x61C, which NorthernUI's PlayerCharacter.h lists as "rotation angle;
// probably yaw" and the input handler accumulates instead of rotZ while
// mounted. Every reader above therefore comes back to rotZ and rotX, read
// during this one call.
//
// What walks along rotZ, for completeness: HighProcess::Move (0x0063C730)
// hands the step, still in the actor's own frame, to the virtual at +0x1B4
// and MobileObject::Move (0x0065AF30) turns it by MakeZRotation(rotZ)
// (0x0070FDD0) before adding it to the position. NorthernUI's 360-degree
// movement patch hooks 0x0063CB35 in exactly that function. It never runs
// inside the key handler, which is why the swap is invisible to it.
inline constexpr UInt32 kAnimationKeyHandler = 0x005FC890;
inline constexpr UInt32 kPlayerCasterVtableKeyHandlerSlot = 0x00A739D4;

// WHAT A HEADSET RUN THEN SHOWED, and the two sites added for it.
//
// With the key handler wrapped, a run on 2026-09-02 logged its entries for
// swings and casts (action 2) and the swap written - and the spell still
// left straight ahead. The bow's release never entered the handler at all.
// So the player's spell and arrow are made through calls the reading above
// did not follow: the handler is where an NPC's are, the player's take
// another route. Two further sites, each certain of what it does:
//
//   * The projectile factory 0x0069A060 - the function in which the launch
//     rotation is read off the caster's fields (see above) - has one direct
//     call, `E8 C5 E6 FF FF` at 0x0069B996 inside ApplyActiveMagicItem, with
//     ecx = the MagicCaster. Whatever calls ApplyActiveMagicItem for the
//     player, the projectile passes through here.
//   * ArrowProjectile's other constructor, 0x006078E0, is called once
//     directly, `E8 F8 73 1A 00` at 0x004604E3, from the generic reference
//     creator 0x0045FDA0 (type 1, 0x9C bytes) - TESObjectCELL's virtual at
//     slot 27, reached through vtables the file cannot trace. If the
//     player's arrow is made there, the caller that sets its rotation is on
//     the stack at that moment, and the probe writes the return addresses
//     it finds.
inline constexpr UInt32 kMagicProjectileFactory = 0x0069A060;
inline constexpr UInt32 kCallMagicProjectileFactory = 0x0069B996;
inline constexpr UInt32 kArrowProjectileCreatorConstructor = 0x006078E0;
inline constexpr UInt32 kCallArrowProjectileCreatorConstructor = 0x004604E3;

// The code section, for telling a return address from a number.
inline constexpr UInt32 kTextStart = 0x00401000;
inline constexpr UInt32 kTextEnd = 0x00A27C39;

// THE CALL THAT ACTUALLY MAKES THE PLAYER'S ATTACK, found from the stack.
//
// With the factory wrapped, a spell's callers read, nearest first,
// 0x0069C087 (inside UseActiveMagicItem), 0x005FCF73 (inside the function
// below, right after its second UseActiveMagicItem call) and 0x00672E12 -
// which is inside PlayerCharacter's input handler 0x00671620, the function
// NorthernUI's notes call "the subroutine that handles most player input".
// The instruction before that return address is
//
//   E8 9E 9C F8 FF   call 005FCAB0      at 00672E0D  (ecx = the player)
//
// with two floats pushed from 0x00B14E58 and 0x00B14E5C and, just before,
// `mov byte ptr [ebx+588h],1` - the player put into third person for the
// call and taken back out after it, the flip NorthernUI's notes remark on.
// So 0x005FCAB0 is the attack's own update: given the control's state it
// advances the attack, and inside it, at the moments the animation says,
// it makes the arrow (0x005FD47C), calls UseActiveMagicItem (0x005FCE1D,
// 0x005FCF6E) and AttackHandling (0x005FCE85) - the reading above placed
// those inside 0x005FC890 because no padding separates the two, but that
// one ends at its `ret 8` at 0x005FCA87 and is only the virtual wrapper an
// NPC's animation reaches them through. The player never comes that way:
// the wrapped slot logged nothing for the bow at all.
//
// Thirteen direct calls exist; three are the player's, each with `this` =
// the player and preceded by the same third-person flip: 0x00672E0D from
// the input handler with the control's axes, and 0x0066CB49 (in
// 0x0066C6F0) and 0x006758C6 (in 0x00675880) with 1.0, 1.0. The rest are
// AI. All three are wrapped; the one from the input handler is the one
// every frame of an attack goes through.
inline constexpr UInt32 kAttackUpdate = 0x005FCAB0;
inline constexpr UInt32 kCallAttackUpdateFromInput = 0x00672E0D;
inline constexpr UInt32 kCallAttackUpdateFromPlayerA = 0x0066CB49;
// The grab (Z key). PlayerCharacter::HandleInput calls 0x00671170 every
// frame ("related to Z-keying (Havok-grabbing) objects" in NorthernUI's
// reading of 0x00671620); with a grab in progress that function calls the
// per-frame update 0x0066D930 at 0x0067125E (`mov ecx,edi` - the player -
// then `call 0066D930`). The update checks the grabbed reference at
// player+0x578 and the spring at +0x574 (NorthernUI: NiPointer
// telekinesisSpring at 0x574, "constructed shortly before, and assigned at,
// 0x0066D879"), then loads player+0x584, adds a constant and hands it with
// two out-pointers to 0x005F11F0 - which builds a rotation from the
// player's rotation fields through MakeZRotation 0x0070FDD0 and
// MakeXRotation 0x0070FD30, the same makers the aim work read - and the
// results become the spring's target. So the target is the eye vector of
// the player's OWN rotation, scaled by +0x584: +0x584 is the grab distance
// (written from the grab handler's argument at 0x0066D8EF, zeroed with the
// spring at 0x0066AD72), and swapping the rotation and that float around
// the call is what puts the grabbed object where the hand is.
inline constexpr UInt32 kCallGrabUpdate = 0x0067125E;
inline constexpr UInt32 kGrabUpdate = 0x0066D930;
inline constexpr UInt32 kPlayerGrabDistanceOffset = 0x584;

inline constexpr UInt32 kCallAttackUpdateFromPlayerB = 0x006758C6;

// Shortly after the hook the game calls, on the CameraNode:
//
//   0066BE84  fldz
//   0066BE86  push 0
//   0066BE88  push ecx                          ; ecx = CameraNode
//   0066BE89  fstp [esp]
//   0066BE8C  call 00707370
//
// and 0x00707370 dispatches through vtable slot 0x64:
//
//   007073A7  mov  eax, [esi]
//   007073A9  mov  edx, [eax+0x64]
//   007073B0  call edx
//
// Slot 0x64 / 4 = index 25 = NiAVObject::UpdateSelectedDownwardPass.
//
// Which yields the decisive point for OBVR: a scene graph update pass still
// runs after the hook. OBVR therefore has to modify localTransform rather
// than worldTransform - the world transform gets recomputed from
// parent * local regardless.
inline constexpr UInt32 kUpdateSelectedDownwardPass = 0x00707370;

// Pointer to the player. Established by 0x0066C580 (ToggleCamera), which
// writes to [ecx+0x588] - the isThirdPerson flag in PlayerCharacter.
inline constexpr UInt32 kPlayerPointer = 0x00B333C4;
inline constexpr UInt32 kPlayerIsThirdPersonOffset = 0x588;

// MobileObject::process, from xOBSE's GameObjects.h where it is commented
// "BaseProcess * process; // 058". PlayerCharacter inherits it through Actor,
// so this counts from the player pointer above - which is the same anchoring
// the rotation at 0x20 and isThirdPerson at 0x588 already rest on, both of
// them right in the running game for weeks.
inline constexpr UInt32 kMobileProcessOffset = 0x058;

// PlayerCharacter::firstPersonNiNode - the root of the skeleton the first
// person arms and weapon hang from.
//
// Wanted so the bow can follow the gaze while it is being drawn, without
// turning the body: turning the body would take the walking with it, which is
// the whole fault the aiming work has been circling. The node is only drawn,
// so rotating it moves the weapon and nothing else - no heading, no
// projectile, no locomotion.
//
// ONE SOURCE, and the one that has been wrong before. xOBSE's GameObjects.h
// gives the layout as
//
//   ActorAnimData * firstPersonAnimData;  // 5CC
//   NiNode        * firstPersonNiNode;    // 5D0
//   float           unk5D4;               // 5D4
//
// and nothing independent confirms it. So it is checked the way the HUDInfo
// menu is: the object has to identify itself before anything is written to it.
// A NiAVObject carries a name at +0x08, and a pointer that leads to a readable
// name is a scene graph node; one that does not is refused and reported. The
// neighbouring evidence is that isThirdPerson at +0x588 and the rotation at
// +0x20 are both from this same layout and both right in the running game.
inline constexpr UInt32 kPlayerFirstPersonNodeOffset = 0x5D0;

// TESObjectREFR::niNode, the actor's third-person scene-graph root.
//
// xOBSE's GameObjects.h lays TESObjectREFR out as rot at +0x20, position at
// +0x2C, scale at +0x38 and `NiNode* niNode` at +0x3C. OBVR already verifies
// the same class at +0x20 in PlayerAim and the far end at PlayerCharacter
// +0x588 in the camera switch, so this is the field between two matching
// anchors, not an isolated borrowed offset.
inline constexpr UInt32 kReferenceNodeOffset = 0x3C;

// NiAVObject::GetObject(const char* name), used by xOBSE itself in
// PlayerCharacter::SetSkeletonPath to find Camera01 from the new third-person
// root: it reads the root's vtable, takes [vtable+0x58], and calls it with the
// node and name. The same recursive lookup finds Bip01 Spine2 without walking
// an undocumented child-array layout here.
inline constexpr UInt32 kNiAVObjectGetObjectVtableOffset = 0x58;

// NiAVObject::name, used only to make a candidate node prove it is one.
inline constexpr UInt32 kNiObjectNameOffset = 0x08;

// The byte behind BaseProcess::GetWeaponOut - whether the actor is in combat
// stance, weapon or spell readied.
//
// READ DIRECTLY RATHER THAN CALLED, and that is the whole point of this
// constant existing. GetWeaponOut is a virtual at vtable index 0xBE, and
// calling it would mean trusting that index: a wrong one calls some other
// virtual, and the neighbouring entries take arguments and set things. That
// fails by corrupting the game. Reading a byte at a wrong offset fails by
// returning a wrong byte, which shows up as a crosshair that appears at the
// wrong moment - visible, harmless, and easy to correct.
//
// Both the offset and what sits behind the call come from the same file read
// two independent ways. xOBSE's GameProcess.h declares
//
//   virtual UInt8 GetWeaponOut(void) = 0;    // 0xBE
//   virtual UInt8 SetWeaponOut(UInt8 out) = 0;
//
// and the vtable analysis table at the top of that same file, which was built
// from disassembly rather than from the declarations, has for that index:
//
//   // 0BE  0  8  retn0  <-  <-  get unk114  <-
//   // 0BF  1  x  null   <-  <-  set unk114  <-
//
// Zero arguments, an 8-bit return, and a plain read of unk114 on
// MiddleHighProcess - which HighProcess inherits, and the player always has a
// high process. The getter and the field agree, and so do the two halves of
// the file.
//
// It is still checked before it is believed: the field is a boolean, so
// anything but 0 or 1 means this offset is not what this build has, and the
// answer is then "no idea" rather than a number.
inline constexpr UInt32 kProcessWeaponOutOffset = 0x114;

// The actor's movement flags, which carry whether it is sneaking.
//
// Read rather than called, for exactly the reasons above. GetMovementFlags is
// the virtual at index 0xAF, and the same two readings of the same file agree
// on what sits behind it - the declaration in GameProcess.h,
//
//   virtual UInt32 GetMovementFlags(void) = 0;
//
// coming directly after Unk_AE, and the vtable analysis table at the top of
// that file, built from disassembly:
//
//   // 0AF  0  16  retn0  <-  <-  <-  get unk1FC
//
// Zero arguments, a 16-bit return, a plain read of unk1FC on HighProcess -
// which is the process the player always has. The declared return is 32-bit
// where the table says 16; the field is read as 16, which is what the table
// says is there and is wide enough for every flag xOBSE names.
//
// This one is used for a cosmetic decision only - whether third person needs
// the borrowed crosshair, or the game is already drawing its own sneak eye -
// so a wrong offset costs a crosshair that looks wrong, not a crash.
inline constexpr UInt32 kProcessMovementFlagsOffset = 0x1FC;

// From BaseProcess's own enum in GameProcess.h:
//   kMovementFlag_Sneaking = 0x00000400
inline constexpr UInt32 kMovementFlagSneaking = 0x0400;

// HighProcess::currentAction - what the actor is in the middle of doing.
//
// Wanted for one question that a keypress cannot answer: has the arrow
// actually LEFT the bow? Releasing the attack control starts the shot; the
// arrow spawns several frames later, at the end of the release animation. Any
// change to the player's heading in between goes into the arrow, so a body
// straightened on the release frame sends the shot forwards instead of at what
// was aimed at.
//
// Read as a field, on the same terms and for the same reasons as the two
// above. GetCurrentAction is the virtual at index 0xB3, and the two readings of
// GameProcess.h agree: the declaration
//
//   virtual SInt16 GetCurrentAction() = 0;
//
// and the disassembled vtable table's
//
//   // 0B3  0  32  retn-1  <-  <-  <-  get unk1F4
//
// A plain read of unk1F4 on HighProcess. The table's default of -1 is itself a
// third agreement: kAction_None is -1, so an actor with no process answers
// exactly what an actor doing nothing would.
inline constexpr UInt32 kProcessCurrentActionOffset = 0x1F4;

// The actions that mean an attack is still in flight, from HighProcess's own
// kAction_ enum. Bow shots pass through 4 and 5; melee through 2 and 3.
//
// Read out twice now, from the same header on two occasions separated by the
// work on the aim, and unchanged both times - see the note below the values,
// which also records what the second reading found MISSING. Nothing depends on
// them alone even so: the return has a time limit behind it, so a wrong value
// here would delay the straightening rather than prevent it, and the log says
// which of the two ended the wait.
inline constexpr SInt32 kActionNone = -1;
inline constexpr SInt32 kActionAttack = 2;
inline constexpr SInt32 kActionAttackFollowThrough = 3;
inline constexpr SInt32 kActionAttackBow = 4;
inline constexpr SInt32 kActionAttackBowArrowAttached = 5;

// SPELLS ARE NOT IN THAT ENUM, and that is a finding rather than an omission.
//
// The whole enum was read out a second time when the aim was extended from the
// bow to magic, and it runs: None -1, EquipWeapon 0, UnequipWeapon 1, Attack 2,
// AttackFollowThrough 3, AttackBow 4, AttackBowArrowAttached 5, Block 6,
// Recoil 7, Stagger 8, Dodge 9, LowerBodyAnim 10, SpecialIdle 11,
// ScriptAnimation 12. Nothing in it is a cast.
//
// Two things follow. The five values above now have the second source this file
// asks of every address and did not have when they went in - the note above
// calling them the least certain thing here is out of date, and they came back
// unchanged. And the machinery that decides how long the body stays turned for
// a bow shot cannot be pointed at a spell: there is no action to watch.
//
// BUT A CAST STILL DRIVES THE FIELD, which the enum could not have told anyone
// and only the trace did.
//
// Reading it a line a frame through six casts: 54 frames of Attack, then 24 to
// 26 of AttackFollowThrough, then None - that shape every time. A spell is
// reported as an attack. So the values above cover casting as well, and
// IsShotUnreleased draws the line in the right place for it without a change.
//
// This replaced a field found for the purpose and then measured away.
// MiddleHighProcess+0x14C, which xOBSE hedges as "looks like true if casting",
// does read true through a cast - and stands for 79 frames, which is 54 plus 25
// exactly. It covers Attack AND FollowThrough together and cannot separate
// them, so it would hold the body turned for four tenths of a second after the
// spell had already left. Single-sourced, hedged, and superseded by a field
// this file already had two readings of: not kept.

// Pointer to NiDX9Renderer, the object that owns Oblivion's Direct3D 9
// device. This is where 0.1.0 has to start: the camera hook works on the
// scene graph and has never touched the renderer, but OpenVR takes a texture
// and only the device can produce one.
//
// Two independent sources, which is the standard this file holds addresses
// to. The address comes from OBGEv2's Nodes/NiDX9Renderer.cpp, inside a
// namespace named v1_2_416 - the same build OBVR targets:
//
//   mov eax,0x00B3F928
//   mov eax,[eax]
//
// The offset comes from xOBSE's obse/obse/NiRenderer.h, which lays out
// NiDX9Renderer with "IDirect3DDevice9 * device; // 280" and asserts the
// struct's size and two of its offsets at compile time.
//
// Checked against the binary before adoption, as everything here is: the
// encoding of "mov eax, [0x00B3F928]" - A1 28 F9 B3 00 - appears 41 times in
// Oblivion.exe. A five byte sequence does not occur 41 times by chance, and
// a global read that often is one the renderer is genuinely reached through.
inline constexpr UInt32 kRendererPointer = 0x00B3F928;
inline constexpr UInt32 kRendererDeviceOffset = 0x280;

// The working copy of the screen size that Oblivion's whole 2D reads, found
// by disassembling this very binary rather than quoted from anyone.
//
// (The NiDX9Renderer's own width/height at +0xA58/+0xA5C were the first
// suspect and were measured in game already holding the believed size - real
// fields, wrong lever.)
//
// The evidence, all from dumpbin /disasm of Oblivion.exe 1.2.0.416:
//
//   * The UI normalization UESP describes - height fixed at 960, or width at
//     1280 in portrait - exists at 0x57D7A0/0x57D7F0: fild [0x00B06C4C],
//     fild [0x00B06C50], compare, divide, multiply by 960.0. The float
//     constants 960.0f/1280.0f each occur exactly once in .rdata, which is
//     what made the functions findable.
//   * Those two integers are written in exactly ONE place in the whole
//     executable, 0x4983AC/0x4983B2, inside the window-creation function:
//     copied from [0x00B06C5C]/[0x00B06C64] - and those are the iSize
//     SettingInfo objects, proven by the name pointers beside them reading
//     "iSize W:Display" and "iSize H:Display" in the image.
//   * Some ninety reads spread over the interface code (0x57xxxx around the
//     2D pass), the window/display code (0x498xxx) and the input side
//     (0x682xxx, 0x5DF1B7's cursor-range compares).
//
// So: settings -> copied once at window creation -> read everywhere. The INI
// is saved from the settings themselves, never from this copy, which is what
// makes the copy patchable where the settings are not (see the ban in
// IniSettings.h). Rewritten under Render.UiFollowsFrameSize, after the
// window and display-mode decisions have consumed the game's own numbers,
// and only when it still reads exactly the asked-for size.
inline constexpr UInt32 kUiScreenWidthCopy = 0x00B06C4C;
inline constexpr UInt32 kUiScreenHeightCopy = 0x00B06C50;

// A dead end, recorded so it is not walked twice: the cursor-movement
// function at 0x57E7C0 calls a renderer getter (0x403190, returning
// [this+0x1B20/0x1B24/0x1B28] for axis 1/2/3) inside its position math,
// which read like a size-based conversion of the sprite position. It is not.
// Measured in game: the getter answers 0 for both axes - the three fields
// are set together in one place (0x40433C, same value into all three) and
// stay zero here - so the whole term multiplies to nothing in vanilla.
// Redirecting those calls to the screen-size copy (commit 40d7e13) armed the
// inert term instead: the cursor node walked off screen and the sprite
// vanished. The mouse offset between sprite and hit test under a raised copy
// comes from somewhere else, still unfound.

// The InterfaceManager singleton pointer, for the read-only cursor probe
// that hunts that unfound source. Two sources: xOBSE's GameAPI.cpp calls a
// GetSingleton at 0x582160, and this binary's disassembly of that function
// reads the pointer from [0x00B3A6E0] (creating a 0x134-byte object into it
// when null). Field offsets, from xOBSE's GameAPI.h (STATIC_ASSERTed there)
// and this binary's cursor-movement function 0x57E7C0: +0x1C the cursor
// Tile*, +0x88 altActiveTile, +0x98 activeTile; the movement function writes
// the cursor position as floats at +0x20/+0x24/+0x28 and a derived triple at
// +0x2C/+0x30/+0x34, and reaches the cursor tile's render node through
// tile+0x24, whose NiAVObject translation sits at +0x54. The probe only ever
// reads, and only behind null checks.
// The tile-under-cursor search - what decides the hover highlight, and with
// it what a click activates. Called from the InterfaceManager update
// (0x582406, right after GetSingleton at 0x582160) with the cursor position
// it reads from the manager's own pixel fields (+0x2C/+0x34, clamped
// against the screen-size copy inside), and answered by a scene-graph pick
// (0x70D300 on the ui scene at [manager+0xDC]). The pick maps the pixels
// through the renderer's camera geometry, not through the copy - measured
// with a cursor ladder: identical spacing to the drawn items, the centre
// half the height difference lower, and re-asserting the believed viewport
// around the search is what moves the zones. Detoured at entry so the
// search runs under the believed viewport and everything downstream of it -
// highlight and click alike - answers in the drawn space.
inline constexpr UInt32 kFindTileAtCursor = 0x00581390;

// Where that pick turns pixels into camera coordinates - and the one place
// the wrong size enters. 0x70D325 (the pick's only normalization call, and
// this function's only caller) passes the cursor pixels here; the function
// divides x by the renderer's width getter and y by its height getter
// (vtable calls through [[0x00B3F928]], slots 0x4C/0x50 on the size source
// its [renderer+0x20C] flag selects) and hands back 0..1 for the camera's
// port and frustum test. The pixels live in the screen-size copy's space,
// so with the copy raised the division is by the wrong height - the
// measured hover offset. Detoured at entry to divide by the believed size
// instead; inert while belief and frame agree.
inline constexpr UInt32 kPickNormalizePoint = 0x00701540;

inline constexpr UInt32 kInterfaceManagerPointer = 0x00B3A6E0;
inline constexpr UInt32 kInterfaceCursorTileOffset = 0x1C;
inline constexpr UInt32 kInterfaceCursorPosOffset = 0x20;
inline constexpr UInt32 kInterfaceCursorDerivedOffset = 0x2C;
inline constexpr UInt32 kInterfaceAltActiveTileOffset = 0x88;
inline constexpr UInt32 kInterfaceActiveTileOffset = 0x98;
inline constexpr UInt32 kTileRenderNodeOffset = 0x24;
inline constexpr UInt32 kNiTranslateOffset = 0x54;

// Expected game version. OBSE reports it as oblivionVersion.
inline constexpr UInt32 kOblivionVersion_1_2_416 = 0x010201A0;

// The engine's INI settings, alive in memory: the IniSettingCollection
// singleton object - the object itself at this address, not a pointer to it.
// Its layout, from xOBSE's obse/GameAPI.h: vtable at +0, the INI file's path
// at +4, and the setting list starting inline at +0x10C as {SettingInfo*,
// next*} entries whose SettingInfo is {value union, name pointer}.
//
// One documented source, from xOBSE's obse/GameAPI.cpp:
//
//   static const UInt32 g_IniSettingCollection = 0x00B07BF0;
//
// and its GetIniSetting walks exactly the list described above - code this
// installation runs, since xOBSE 22.13 is what loads OBVR. The second source
// is the same runtime validation the cached Direct3DCreate9 pointer gets: the
// object is only trusted after its vtable points into the executable image
// and its path field reads as an .ini path, and a setting is only written
// after its name matches exactly and its current value is the very number the
// game is asking CreateDevice for. A wrong address cannot pass those checks
// except by holding the right structure, in which case it is not wrong.
inline constexpr UInt32 kIniSettingCollection = 0x00B07BF0;
inline constexpr UInt32 kIniSettingListOffset = 0x10C;

// The per-frame clock. xOBSE's g_timeInfo points at a TimeInfo structure at
// 0x00B33E90 whose float at +0x0C is the seconds the last frame took - the
// delta every time-driven update in the frame advances by. The second world
// render of a dual frame must not advance anything: animation controllers
// ticking twice per frame were the player's long-standing stutter, and the
// NPC head-aim re-running each walk is the sideways helmets. The scene hook
// zeroes this for the second render and restores it after - guarded by a
// plausibility check at runtime (a frame delta reads between zero and one),
// which is the second source for this address.
inline constexpr UInt32 kFrameSecondsAddress = 0x00B33E9C;

// Whether a menu is up: the main menu, a loading screen, an inventory, the
// ESC menu, a dialogue. A function rather than a flag, and nullary.
//
// Two sources, as everything here has. xOBSE names the address in GameAPI.cpp
// as the target of its _IsMenuMode function pointer for 1.2.0.416. The bytes
// at that address in Oblivion.exe say the same thing independently:
//
//   00578F60  push 1; push 0; call 00582160    <- InterfaceManager singleton,
//   00578F6C  add esp,8; test eax,eax; jz +2A     the address xOBSE also names
//   00578F70  push 1; push 0; call 00582160
//   00578F7C  add esp,8; cmp dword [eax+1C],0; jz +18
//   00578F82  push 1; push 0; call 00582160
//   00578F8B  xor ecx,ecx; add esp,8
//   00578F90  cmp byte [eax+8],1; setne cl; mov al,cl; ret
//   00578F9A  xor al,al; ret
//
// A function that reaches the interface manager three times and returns a
// byte is the one being described. The ret takes no argument, so it is
// nullary and the calling convention does not matter.
//
// Why OBVR wants it: without it, "is a menu up" has to be guessed from
// whether the camera hook ran this frame - and in a menu Oblivion still draws
// the world behind the menu on some frames and not others. The guess
// therefore flips back and forth, and with it the whole presentation: one
// frame the world fills the headset, the next a small flat rectangle hangs in
// black. That is the flicker seen when opening the ESC menu.
inline constexpr UInt32 kIsMenuMode = 0x00578F60;

// The update step, 0x0040D800, asks IsMenuMode afresh for every subsystem
// it pauses: fourteen `call 00578F60` sites inside it, counted in this
// binary's disassembly, and Real Time Menus (Nexus 55800, source on GitHub)
// redirects a subset of them to keep the world running behind menus. The
// two sources agree on every address below; what each site gates is Real
// Time Menus' reading, confirmed here only for the first (it feeds
// SetHavokPaused, 0x00889A30, directly: `call IsMenuMode; push eax; call
// 889A30`). The ones left alone are deliberate: 0x0040DC61 gates the
// player's controls, and a player who can walk while the inventory is
// open is not what "unpaused" means; 0x0040D9D4 the engine's own menu
// shading, which OBVR replaces anyway.
inline constexpr UInt32 kUpdateStepIsMenuModeSites[] = {
	0x0040D809,  // physics pause - the argument to SetHavokPaused
	0x0040DB5B,  // animations
	0x0040DBAB,  // sound
	0x0040DBFB,  // actors (AI)
	0x0040DE3F,  // physics step
	0x0040DE76,  // weather
	0x00663176,  // scripts, in the script runner rather than the update step
};

// InterfaceManager::GetTopVisibleMenuID - the id at the top of the active
// menu stack, or 0 with none. xOBSE names it (GameAPI.cpp, ThisStdCall on
// 0x0057CF60) and NorthernUI has the same address as GetTopmostMenuID; the
// disassembly shows it walking the ten dwords at [this+0xE0] and returning
// the last one that is set. NorthernUI records that the F1-F4 menus stand
// in that stack as 1 rather than under their own ids.
inline constexpr UInt32 kGetTopVisibleMenuId = 0x0057CF60;

// PlayerCharacter::SetDialogCamera - the function behind the dialogue zoom,
// called when a conversation starts (with the NPC) and again when it ends
// (with null), each time starting the camera transition whose distance
// fDlgFocus sets. OBVR patches its first bytes to ret 0Ch - three dword
// arguments, callee-cleaned under __thiscall - so neither transition ever
// starts.
//
// Two sources, as everything here has. TESReloaded (llde/TESReloaded10,
// Framework/Oblivion/Base.h) names Hooks::SetDialogCamera = 0x0066C6F0 as
// __thiscall (PlayerCharacter*, Actor*, float, UInt8), and its camera mode
// detours it without ever calling the original - which is exactly the "no
// zoom at all" that mod is known for. The bytes in this machine's 1.2.0.416
// say the same thing independently:
//
//   0066C6F0  sub esp,18; push ebp
//   0066C6F4  mov ebp,[esp+20]        <- the first stack argument (the Actor)
//   0066C6F8  test ebp,ebp
//   0066C6FA  push esi; mov esi,ecx   <- __thiscall
//   0066C6FD  jz +527                 <- null Actor takes the ending path
//   0066C703  fld1; fcomp [esp+28]    <- the float argument against 1.0
//   ...       and at +1AD the body reads dword [00B14F10] - the fDlgFocus
//             setting's value slot, found from its name string: 00B14F14
//             holds the pointer to "fDlgFocus" at 00A7409C, and the float
//             before it holds 2.1, the setting's documented default.
//
// A function with that signature, split on a null Actor, reading fDlgFocus,
// at the very address TESReloaded names, is the one being described.
//
// The intervention here went through three shapes, each correcting the last:
// a bare ret 0Ch (cut everything), then holding fDlgFocus at 15 in memory
// (the transition ran, going nowhere), then the ret again - and the ret
// turned out to cut one thing too many. SetDialogCamera is ALSO what flips a
// third-person player into first person for the conversation and back after
// it, and that half was wanted. So the entry now jumps to a shim of OBVR's
// own that does the flip through ToggleCamera below and nothing else: no
// transition, no zoom, the vanilla point-of-view dance kept.
inline constexpr UInt32 kSetDialogCamera = 0x0066C6F0;

// PlayerCharacter::ToggleCamera - the game's own "put the player in first or
// third person", one byte argument, 1 meaning first person.
//
// Three sources for once. TESReloaded (Framework/Oblivion/Base.h) names
// ToggleCamera = 0x0066C580 and calls it __thiscall with a byte; xOBSE
// (obse/GameObjects.cpp) implements PlayerCharacter::TogglePOV(bool
// bFirstPerson) as ThisStdCall(0x0066C580, this, bFirstPerson), and its
// command documentation fixes the meaning: "Passing 1 enables first person
// view, 0 enables third person". And this file already leaned on the
// function once: the kPlayerPointer comment below records that 0x0066C580
// writes the isThirdPerson flag at +0x588, which is how that offset was
// established.
inline constexpr UInt32 kToggleCamera = 0x0066C580;

// Where Oblivion keeps d3d9.dll and the Direct3DCreate9 it looked up in it.
//
// This exists because Oblivion.exe does not import d3d9.dll at all - the
// import table lists d3dx9_27.dll and thirteen others, and no d3d9. It loads
// it by hand, at 00761DF0:
//
//   00761DF0  push esi; xor esi,esi
//   00761DF3  cmp [00B42154],esi; jnz +5E
//   00761DFB  mov eax,[00B42158]           <- already resolved? then done
//   00761E00  test eax,eax; jnz +29
//   00761E04  push "D3D9.DLL"
//   00761E09  call [00A28118]              <- LoadLibraryA, from the IAT
//   00761E11  mov [00B42150],eax           <- the module handle
//   00761E18  push "Direct3DCreate9"
//   00761E1D  push eax
//   00761E1E  call [00A2811C]              <- GetProcAddress, from the IAT
//   00761E26  mov [00B42158],eax           <- the function, cached here
//
// The two IAT slots agree with the import table read separately, which is
// what makes this two sources rather than one reading.
//
// So an import hook on Direct3DCreate9 can never fire: there is no import to
// replace. GetProcAddress is imported, and is hooked instead. The cached
// pointer is the second way in, for the case where the lookup has already
// happened by the time OBVR loads - and it is validated before being written,
// by resolving Direct3DCreate9 independently and requiring the same value.
inline constexpr UInt32 kD3D9Module = 0x00B42150;
inline constexpr UInt32 kDirect3DCreate9Pointer = 0x00B42158;

// The function that draws one frame of the world: culling, both scene graph
// passes, water, and the image space shaders, but not the 2D layer - menus
// and HUD are drawn later, on a different path. __thiscall, one argument (a
// BSRenderedTexture*, null on the ordinary world pass).
//
// Two sources. Oblivion Reloaded's RenderHook.cpp names it kRender for this
// exact build, and detours it. The bytes agree independently, in three ways:
//
//   * inside it, at 0040CCD3 and 0040CE48, are the only two calls in the
//     whole binary to 0070C0B0 that render a scene graph on the world path -
//     the function Oblivion Reloaded names RenderObject, and which begins
//     with mov ecx,[00B3F928], the renderer global this project has already
//     verified twice over
//   * at 0040CF6E is the single call in the whole binary to 007B48E0, the
//     image space shaders (HDR) - so the picture is finished, tone mapping
//     included, when this function returns
//   * at 0040C95F it reads the scene graph at 00B333CC and walks the same
//     node list ([eax+0xB6] count, [eax+0xB0] list, first entry) that the
//     camera hook site walks - the CameraNode - and copies its position
//     (+0x54) into two follower nodes before drawing
//
// The third point matters beyond verification: Render re-reads the camera
// node's position itself, each call, to place the sky and LOD roots. A second
// call with the camera moved therefore keeps everything consistent without
// further help.
//
// Called from three places: 0040D41B (a menu wants the world in a texture),
// 0040D658 (the ordinary world pass, texture null), 00411CBF (the save game
// screenshot). Only the ordinary pass is drawn twice; the argument and a
// once-per-frame guard tell them apart.
//
// The entry reads
//
//   0040C830  push -1              6A FF
//   0040C832  push 0x9AA163        68 63 A1 9A 00
//   0040C837  mov eax,fs:[0]       (the SEH frame; not moved)
//
// so the first seven bytes are two whole instructions with nothing relative
// in them, which is what the entry detour relocates.
inline constexpr UInt32 kRenderScene = 0x0040C830;
inline constexpr UInt32 kRenderSceneEntryLength = 7;

// g_worldSceneGraph - the pointer to the world's SceneGraph (a NiNode
// subclass), with its camera and its culling process beside it.
//
// Two sources. xOBSE's headers name the global and the two offsets
// (SceneGraph::camera at 0xDC, SceneGraph::cullingProcess at 0xE4). The
// second is this machine's runtime: across a whole session the three read
// back as stable, plausible heap pointers - scene 1812BBA8, camera 187E424C,
// culling 187E4EE8 - identical on world frames and menu frames alike, and the
// culling process's first word is a vtable in the executable's read-only data
// (00A7E610), which a wrong offset would not produce.
//
// What they were read to answer, and the answer: whether Oblivion takes the
// world away while a pause menu is up. It does not. Every one of these fields
// is unchanged between a frame the engine renders and a frame it refuses to,
// so a live background is not blocked by a missing scene - see the dead end
// below for where the emptiness actually comes from.
inline constexpr UInt32 kWorldSceneGraphPointer = 0x00B333CC;
inline constexpr UInt32 kSceneGraphCameraOffset = 0xDC;
inline constexpr UInt32 kSceneGraphCullingOffset = 0xE4;

// DEAD END, measured 2026-08-30: NiCullingProcess + 0x08 is NOT a pointer to
// the culled-geometry list the renderer consumes. xOBSE's headers put a
// NiCulledGeoList there, and the reasoning that follows from it - "the list
// is empty while a menu is up, fill it and the world draws" - is the obvious
// next step and it is wrong. The field reads null on EVERY frame, including
// the world frames where the render provably draws the whole scene. Whatever
// the renderer walks, it is not reached through there.
//
// The two facts that stand instead, both from the menu-world probe: a
// self-initiated render on a menu frame runs to completion, issuing ~344
// vertex setup calls and not one draw; and the scene graph it walks is intact
// while it does so. So the geometry is registered somewhere the per-frame
// update fills - BSShaderAccumulator is the documented candidate - and the
// list, wherever it is, is not this one.
inline constexpr UInt32 kCullingProcessListOffsetDeadEnd = 0x08;

// The fields the emptiness has to come from, read from the disassembly of the
// render path on 2026-08-30.
//
// The chain, whole: kRenderScene calls 0x0070C0B0, which reads the culling
// process's visible set at +0x08, finds the null above, and so calls
// NiCullingProcess::Process (0x0070E0A0) - which fetches the renderer's
// accumulator itself, brackets the walk with StartAccumulating and
// FinishAccumulating (vtable +0x4C and +0x50), and walks the graph through
// NiAVObject::Cull at 0x007073D0. That walk is four instructions:
//
//   007073D0  test byte ptr [ecx+18h],1
//   007073D4  jne 007073E7            <- set: turn back, register nothing
//   007073E2  mov eax,[edx+4]         <- clear: NiCullingProcess::Cull
//   007073E7  ret 4
//
// So a render that cannot return early - kRenderScene has exactly one ret,
// at 0x0040D150 - still comes away with nothing whenever that bit is set,
// or whenever the world bound at +0x20 fails the frustum planes the walk
// rebuilds from the camera each time. Those, and a missing accumulator, are
// the only ways the measured shape happens: full vertex setup, no draws.
//
// The flag offset is derived rather than documented: 0x0040C830 sets bit 0 at
// [node+0x18] on the first-person node at 0x0040C95A and clears it again at
// 0x0040CDA5, using it as its own visibility switch, and 0x007073D0 tests the
// same bit on any NiAVObject. Second source is the probe that reads them.
inline constexpr UInt32 kNiFlagsOffset = 0x18;
inline constexpr UInt32 kNiWorldBoundOffset = 0x20;
inline constexpr UInt32 kNiChildrenOffset = 0xB0;
inline constexpr UInt32 kNiChildCountOffset = 0xB6;
inline constexpr UInt32 kNiCameraFrustumOffset = 0xEC;

// NiCullingProcess::Process(camera, scene, visibleSet) - the walk that
// decides what each world pass draws. __thiscall, three stack arguments.
//
// Two sources. The 2026-08-30 chain above reached it by reading kRenderScene
// downwards. The 2026-09-04 reading confirmed it from the other side: the
// culling process's vtable at 00A7E610 (.rdata, read out of the executable)
// carries 0070E0A0 in slot 2 (+0x8), which is the slot 0070C0B0 - Oblivion
// Reloaded's RenderObject - calls with (scene, camera, visible set) pushed,
// and the function itself hands camera+0xEC (the frustum, above) to 0070E040,
// which copies it and calls 00717A40 with camera+0x64 - the camera's world
// transform - to build the frustum planes, then walks the scene through
// NiAVObject::Cull at 007073D0 between the accumulator's StartAccumulating
// and FinishAccumulating (vtable +0x4C, +0x50). Slot 1 (0070DFB0) is
// NiCullingProcess::Cull, the one 007073D0 dispatches to.
//
// Why it is hooked: the planes come from the camera's world position, which
// the dual pass moves by one eye baseline between the two renders. A body
// straddling a frustum plane is therefore culled in one pass and drawn in the
// other, and a skinned body drawn in the second pass alone has no first-pass
// palette to be locked to - the edge-of-view collapse of followers, measured
// in the log as second-render uploads with no pair (1571 first, 1655 second).
// The hook culls the second pass from the first pass's camera position, so
// both passes draw the same bodies; the draw itself still uses the moved
// camera, since the renderer took its view before the cull (00701970 at
// 0070C0DE) and the position is put back before the walk returns.
//
// The entry reads
//
//   0070E0A0  push -1              6A FF
//   0070E0A2  push 0x9AEFA8        68 A8 EF 9A 00
//   0070E0A7  mov eax,fs:[0]       (the SEH frame; not moved)
//
// the same two relocatable instructions as kRenderScene's entry.
inline constexpr UInt32 kCullingProcessProcess = 0x0070E0A0;
inline constexpr UInt32 kCullingProcessProcessEntryLength = 7;

// The engine's own switch for a live world behind menus, and the reason it
// is normally still.
//
// Oblivion does not simply stop rendering while a menu is up. It renders the
// world ONCE into a texture (0x0040D160), sets a "the snapshot is valid" byte
// at 0x00B33397, and from then on blits that texture instead of rendering -
// which is why the scene counter stands still, measured, across every menu
// frame. The guard is at
//
//   0040D5FE  cmp byte ptr ds:[00B33397h],bl
//   0040D604  jne 0040D662              <- snapshot valid: skip the render
//   ...
//   0040D658  call 0040C830             <- otherwise, render the world live
//
// and whether the snapshot is ever taken hangs on one byte earlier:
//
//   0040DA54  cmp byte ptr ds:[00B33396h],bl
//   0040DA5A  je  0040DB37              <- zero: never take one
//
// kStaticMenuBackground is that byte. It is a copy of Oblivion's own display
// setting bStaticMenuBackground, written once during start-up by
//
//   0040713D  mov dl,byte ptr ds:[00B06DC4h]
//   00407143  mov byte ptr ds:[00B33396h],dl
//
// from a call site that runs a single time in WinMain. Clear it and the
// engine renders the world live behind every menu, on its own, through its
// own call - no patched bytes, no self-initiated render.
//
// The copy is deliberately the thing OBVR touches rather than the setting at
// 0x00B06DC4. Writing the setting is the shape that made iSize dangerous:
// settings can be written back to the user's INI, and a value the game
// persists is a value that outlives the session. Nothing persists this copy.
//
// The simulation is not affected, which is the whole point. Every pause in
// the update step is guarded by its own fresh IsMenuMode call (0x00578F60 at
// 0x0040DC61 and seven other sites in 0x0040D800), not by this byte - so the
// world stays frozen while the picture comes alive. The engine itself already
// drives this path: with SleepWait open (menu id 0x3F4) it retakes the
// snapshot every frame because world time is running.
inline constexpr UInt32 kStaticMenuBackground = 0x00B33396;
inline constexpr UInt32 kMenuSnapshotValid = 0x00B33397;

// The accumulator hanging off the renderer singleton (kRendererPointer, well
// above) at +0x08 - the one the walk registers geometry with. 0x0040CE1B
// swaps a second accumulator in there for the first-person pass and
// 0x0040CE79 swaps it back, which is the second source for the offset.
inline constexpr UInt32 kRendererAccumulatorOffset = 0x08;

// NiAVObject::UpdateSelectedDownwardPass - recomputes world transforms from
// parent * local, downward from the given node. __thiscall on the node, two
// arguments: a float time and an int flags, both observed as zero.
//
// Two sources. The camera hook site itself: immediately after the hooked
// instruction the game calls it on the CameraNode it has just written -
//
//   0066BE84  fldz
//   0066BE86  push 0
//   0066BE88  push ecx
//   0066BE89  fstp dword ptr [esp]        <- (0.0f, 0)
//   0066BE8C  call 00707370               <- ecx = CameraNode
//
// and Render at 0040C9A5/0040C9F0 makes the identical call, with identical
// arguments, on the two follower nodes it has just repositioned. HANDOFF
// section 3 records the same address a third way, as the vtable dispatch that
// overwrites worldTransform after the hook - which is why the hook writes
// localTransform.
//
// Why OBVR calls it: moving the camera to the second eye between the two
// render passes edits localTransform, exactly as the camera hook does, and
// this is the call the game itself uses to make the world transform follow.
inline constexpr UInt32 kUpdateNodeTransforms = 0x00707370;

// The function that draws the 2D layer: HUD, menus, dialogues and loading
// screens. __thiscall on the InterfaceManager singleton, one argument (a
// rendered texture, null on the ordinary path), ret 4.
//
// Two sources, twice over. Oblivion Reloaded's RenderHook.cpp hooks a call
// site inside it (its kRenderInterface, 0x0057F3F3) for this exact build.
// The bytes agree, and they agree through addresses this project has already
// verified independently:
//
//   * its callers fetch `this` through 0x00582160 - the InterfaceManager
//     singleton getter that kIsMenuMode calls three times - and the wrapper
//     at 0x00579260 checks the same [manager+0x1C] field IsMenuMode checks
//   * at 0057F2C3 it draws the menu scene graph through 0x0070C0B0, the same
//     RenderObject the world passes use, with the camera at [scenegraph+0xDC]
//     - the offset kSceneGraphCameraOffset already confirmed in the game
//   * at 0057F3A0 it calls 0x00701970, the SetCameraViewProj OBGEv2 names
//
// Every route to the 2D layer in the whole binary funnels through this one
// function - four call sites, all wrappers deciding when. Full walk in
// HANDOFF, "The 2D pass is located".
//
// One property that matters to the redirect: it begins the *default* render
// target group from inside itself (clear flags 6 - depth and stencil, not
// colour, which is why menus sit on the world instead of on black). So a
// wrapper cannot simply set a target first; the substitution happens at the
// device's SetRenderTarget while the pass runs.
//
// The entry reads
//
//   0057F170  push -1              6A FF
//   0057F172  push 0x9BEAE6        68 E6 EA 9B 00
//   0057F177  mov eax,fs:[0]       (the SEH frame; not moved)
//
// - the same seven-byte relocatable shape as kRenderScene.
inline constexpr UInt32 kRenderInterface = 0x0057F170;
inline constexpr UInt32 kRenderInterfaceEntryLength = 7;

// The handle of the loading thread Oblivion runs while a cell streams in,
// and the reason a frame can end with no 2D layer on it at all.
//
// The wrapper that owns the interface pass, 00579260, guards it three ways:
// the interface manager must exist, its [+1Ch] must be set, and 0040FDA0
// must answer false. That last one reads this global, and when it is not
// null asks GetExitCodeThread whether the thread is still 0x103 -
// STILL_ACTIVE:
//
//   0040FDA1  mov eax,ds:[00B33434]
//   0040FDA6  test eax,eax
//   0040FDA8  jne 0040FDAE          ; null -> false, the interface draws
//   0040FDB3  call ds:[00A280E8]    ; GetExitCodeThread(handle, &code)
//   0040FDBB  cmp dword ptr [esp],103h
//   0040FDC2  sete al               ; still running -> true
//
// and 00579289 turns that true into a jump straight past the interface
// pass. So while this handle names a living thread, kRenderInterface is
// never called - which is what a pass with no draws and no clears in it
// looks like from the outside. Read only, and only to log: the value is a
// diagnosis, never something OBVR writes.
inline constexpr UInt32 kLoadingThreadHandle = 0x00B33434;


// The menu stack the interface pass branches on at 0057F358, and the object
// whose [+18h] decides which of the two draw calls it makes:
//
//   0057F358  cmp word ptr ds:[00B1397A],6
//   0057F360  jbe 0057F376          ; -> 005903E0
//   0057F362  mov eax,ds:[00B13974]
//   0057F367  mov ecx,[eax+18h]
//   0057F36A  cmp ecx,ebx
//   0057F36C  je 0057F376           ; -> 005903E0
//   0057F36F  call 0058FBA0
//
// Both arms draw, so this branch cannot be why nothing is drawn - it is
// logged to say which of the two paths the pass took, so a gate that closed
// can be looked for in the right one.
//
// WHAT THIS ACTUALLY IS, established later and from the other direction. It
// was read here as "the menu stack", which was a guess from the shape of the
// branch and was wrong. xOBSE's GameMenus.cpp declares
//
//   NiTArray<TileMenu*> * g_TileMenuArray = (NiTArray<TileMenu*> *)0x00B13970;
//
// and the two addresses above are that object's own fields: the data pointer
// at +0x04 and the UInt16 count at +0x0A. A decompiled branch in this binary
// and a modding SDK's source, arrived at years and methods apart, describing
// the same object - which is the two-source standard this file holds itself
// to, met without anyone setting out to meet it.
//
// It is an array indexed by menu type, not a stack: entry n belongs to the
// menu with id kMenuIdFirst + n, loaded or not. That is what makes it a second
// route to any menu by id, and the crosshair depth uses it as the check on the
// direct pointer below.
inline constexpr UInt32 kTileMenuArray = 0x00B13970;
inline constexpr UInt32 kTileMenuArrayData = 0x00B13974;
inline constexpr UInt32 kTileMenuArrayCount = 0x00B1397A;

// Pointer to the pointer to HUDInfoMenu - the menu that owns crosshairRef,
// the reference whatever the player is aiming at. From xOBSE's GameMenus.cpp:
//
//   HUDInfoMenu ** g_HUDInfoMenu = (HUDInfoMenu**)0x00B3B33C;
//
// A double pointer, which matters: reading this address gives the global, and
// the global holds the menu.
//
// HOW IT IS CHECKED, since one document is not the standard this file holds
// itself to. Not by a second route to the same pointer - reaching the menu
// through the tile menu array would need TileMenu's layout, which is not
// recorded here, and a guessed offset into a live pointer crashes rather than
// answers. Instead the object is asked what it is: every Menu carries its own
// id at kMenuIdOffset, and the one this address leads to has to answer
// kMenuIdHudInfo before a single further byte is read from it.
//
// That is the stronger check anyway. A second pointer route would only show
// that two addresses agree; the id shows that the address leads to the RIGHT
// menu, and it does so using an offset that has been read in the running game
// for weeks rather than a new one taken on faith.
inline constexpr UInt32 kHudInfoMenuPointer = 0x00B3B33C;

// Where a Menu keeps its own type id.
//
// Already relied on by game::ActiveMenuId, which reads it through the
// InterfaceManager's activeMenu and has been reporting menu types correctly
// since the persuasion work. Named here because the crosshair depth uses it
// for something stricter than logging: as the proof that kHudInfoMenuPointer
// leads where it claims.
inline constexpr UInt32 kMenuIdOffset = 0x20;

// TESObjectREFR::crosshairRef inside HUDInfoMenu, from the class layout in
// xOBSE's GameMenus.h: name 028, valueText 02C ... actionIcon 050,
// crosshairRef 054, unk058, class size 05C.
inline constexpr UInt32 kHudInfoCrosshairRefOffset = 0x54;

// Oblivion's activation/HUD target ray is assembled in the middle of
// InterfaceManager's world-pick update.  Immediately before this address it
// has written the ray origin to stack offsets 20/24/28 and its unit direction
// to 34/38/3C; immediately afterwards it multiplies that direction by
// iActivatePickLength and performs the scene pick.  Patching here changes the
// one ray consumed by both HUDInfoMenu::crosshairRef and Activate, rather than
// trying to repair those two consumers independently.
//
// Verified from this executable's disassembly:
//   005807F4..00580808  stores direction x/y/z at esp+34/+38/+3C
//   0058080C            fild dword ptr [ebx+10]
//   0058080F            fstp dword ptr [esp+18]
//   005809F3..00580A01  passes the resulting segment to the scene pick
inline constexpr UInt32 kHookWorldPickRay = 0x0058080C;
inline constexpr UInt32 kHookWorldPickRayPatchSize = 7;
inline constexpr UInt32 kHookWorldPickRayResume = 0x00580813;

// Tile::UpdateFloat. xOBSE's GameTiles.cpp binds this exact address, and the
// local disassembly at 005865DD uses it to update HUDReticle's visible trait.
// HUDReticle is a TileMenu but does not necessarily own a Menu object, so its
// root tile is the correct level to enable for an isolated draw.
inline constexpr UInt32 kTileUpdateFloat = 0x0058CEB0;

// TESObjectREFR's world position.
//
// Two sources, the second being OBVR's own working code: xOBSE's GameObjects.h
// puts posX/posY/posZ directly after the rotation triple, and PlayerAim.cpp
// has been reading that rotation at +0x20 - and writing to it - in the running
// game for weeks. A rotation at 0x20/0x24/0x28 puts the position at 0x2C.
inline constexpr UInt32 kRefPositionOffset = 0x2C;

// THE MELEE HIT, for the hand-tracked mode's strikes by motion.
//
// Actor::AttackHandling - the function that resolves one melee hit and
// applies it: the damage with the weapon, skill and power attack, the block,
// the sneak attack, the enchantment, the crime, the OnHit and OnHitWith
// script events. Two sources for the function and its three arguments:
//
//   * xOBSE's GameObjects.h declares Actor's virtual 0xEB as
//     `AttackHandling(UInt32 unused, TESObjectREFR* arrowRef,
//     TESObjectREFR* target)` with "args all null for melee attacks", and
//     EventManager.cpp gives the PlayerCharacter, Character and Creature
//     vtables as 0x00A73A0C, 0x00A6FC9C and 0x00A710F4. Slot 0xEB is byte
//     0x3AC, and the executable's .rdata holds 0x005FEBF0 at that slot in
//     all three tables (read out of the file, section .rdata at VA 0x628000
//     from raw 0x627400).
//   * The attack update 0x005FCAB0 (kAttackUpdate) calls that slot at the
//     animation's hit moments, `[edx+3ACh]` at 0x005FCD96 and 0x005FCE85,
//     for the player only (`cmp esi,[00B333C4h]` before each) - NPCs go
//     through slot 0x3B0 - with `push 0, push 0, push 1` at the first and
//     three zeros at the second: (flag, 0, 0). Inside 0x005FEBF0 the third
//     argument at [esp+1ECh] is tested at 0x005FEC32 and, at 0x005FF001, a
//     non-null one is TAKEN AS THE TARGET (`test ebx,ebx; jne` skips the
//     search 0x006156C0 and `mov esi,ebx`); the function marks the target's
//     script events with masks 0x100 and 0x80 at 0x005FF5FE..0x005FF630
//     through MarkEventList 0x004FBF90 - xOBSE's kMarkEvent hook address,
//     and its GameAPI.h masks kEvent_OnHitWith and kEvent_OnHit - and logs
//     "%.20s attempts a Sneak Attack on %.20s" (0x00A6EC84) on the way. The
//     first argument is read as a byte at 0x005FF434 and 0x005FF4C5: when
//     set, the damage is multiplied by 0x00546BA0 of the skill's value -
//     the power attack's factor, which is what the 1 at the first call site
//     stands for. So the call is thiscall with three stack arguments and
//     `ret 0Ch` at 0x006005E7, and passing the target makes the engine
//     apply a hit to it without looking for one.
inline constexpr UInt32 kAttackHandling = 0x005FEBF0;
inline constexpr UInt32 kVtblPlayerCharacter = 0x00A73A0C;
inline constexpr UInt32 kVtblCharacter = 0x00A6FC9C;
inline constexpr UInt32 kVtblCreature = 0x00A710F4;

// The virtuals the hit function itself uses on actors, and that the strike
// uses to choose whom to hit - each an index xOBSE's GameObjects.h declares
// and a byte offset the disassembly of 0x005FEBF0 or of the engine's own
// target search 0x006156C0 calls:
//
//   * IsDead(bool): TESObjectREFR index 0x66, `[edx+198h]` with `push 0` at
//     0x005FF08B (a dead target returns early) and at 0x0061579D in the
//     search, where a dead candidate is skipped.
//   * GetNiNode(): TESObjectREFR index 0x55, `[eax+154h]` at 0x005FED5E
//     (its result + 0x64, the world transform, feeds the debug drawing) and
//     at 0x006157AF in the search, where a candidate without one is skipped.
//   * GetHandReachDistance(): Actor index 0x9B, `[edx+26Ch]` at 0x005FEFD3,
//     the reach when there is no weapon; returned in st(0).
//   * GetScale(): TESObjectREFR index 0x3B, `[edx+0ECh]` at 0x005FEFE3, the
//     reach is multiplied by it at 0x005FEFED.
//   * BaseProcess::GetEquippedWeaponData(bool): index 0x3B in xOBSE's
//     GameProcess.h, `[edx+0ECh]` with `push 1` on the process at
//     0x005FEF92; the entry's +8 is the weapon (xOBSE's
//     ExtraContainerChanges::EntryData: countDelta, extendData, type), and
//     its byte at +0x90 the weapon type (0x005FF07D), which GameForms.h
//     places there with the reach as the float at +0x98 (0x005FEFAF).
inline constexpr UInt32 kActorVtableIsDeadOffset = 0x198;
inline constexpr UInt32 kActorVtableGetNiNodeOffset = 0x154;
inline constexpr UInt32 kActorVtableHandReachOffset = 0x26C;
inline constexpr UInt32 kActorVtableGetScaleOffset = 0x0EC;
inline constexpr UInt32 kActorVtableAttackHandlingOffset = 0x3AC;
inline constexpr UInt32 kProcessVtableEquippedWeaponOffset = 0x0EC;
inline constexpr UInt32 kEntryDataTypeOffset = 0x08;
inline constexpr UInt32 kWeaponTypeOffset = 0x90;
inline constexpr UInt32 kWeaponReachOffset = 0x98;

// The reach in game units. 0x00547540 is `fld [00B36F20h]; fmul [esp+4]`:
// the weapon's reach times a setting, and it is what 0x005FEBF0 calls at
// 0x005FEFC1 with the weapon's +0x98. The setting lives in the uninitialised
// data (past .data's raw size), so its name cannot be read out of the file;
// the strike reads the name pointer the engine's Setting object keeps after
// the value (xOBSE's GameAPI.h: SettingInfo is vtable, data, name) and logs
// it once, expecting "fCombatDistance".
inline constexpr UInt32 kReachInUnits = 0x00547540;
inline constexpr UInt32 kCombatDistanceSetting = 0x00B36F20;

// The actors the engine considers for a melee target. The search 0x006156C0
// walks the list returned by `0x00673A50(0x00B3BD00, 0)` at 0x0061573D -
// a getter on the ActorProcessManager, which xOBSE's GameProcess.cpp binds
// to 0x00B3BD00 - for the level of processing given: 0 is high (the actors
// near the player, animated every frame), which the jump table at
// 0x00673A7C sends to `add eax,68h`; 1, 2 and 3 are the middle-high,
// middle-low and low lists at +0x00, +0x0C and +0x18, the first three
// ActorLists in xOBSE's layout. The list is a tList: each node a data
// pointer and a next pointer, the head node itself the first entry and an
// empty list a head with neither.
inline constexpr UInt32 kActorProcessManager = 0x00B3BD00;
inline constexpr UInt32 kActorListByLevel = 0x00673A50;

// NiAVObject::worldBound - centre and radius of the sphere round the
// model, at +0x20, before the local transform at +0x30 this project has
// been writing to for weeks (GameTypes.h). Read only, to decide whether a
// blade came close: a wrong offset would show as strikes that never land,
// not as damage to the game.
inline constexpr UInt32 kNiAVObjectWorldBoundOffset = 0x20;

}  // namespace obvr::addr
