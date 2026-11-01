#include "camera/hands/Grab.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/AimAtSource.h"
#include "game/CrosshairTarget.h"
#include "game/DropRotationLogic.h"
#include "game/GameAddresses.h"
#include "game/HandBones.h"
#include "game/HeldObject.h"
#include "game/InventoryGrab.h"
#include "game/MeleeHits.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "game/QuickKeys.h"
#include "game/ReachTargets.h"
#include "game/TakeItem.h"
#include "game/WeaponDrawSpeed.h"
#include "test/HandScriptRuntime.h"
#include "vr/LaserGeometry.h"

namespace obvr::camera::hands {

GrabOutput StepGrab(GrabState& state, const HandStep& step, const HandShared& shared, vr::HandModeFrame& frame,
                    GrabLinks& links) {
	const Config& config = step.config;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const float dt = step.dt;
	const bool fitting = links.holsterFitting;
	vr::HandModeResult& handResult = shared.hand;
links.nearItem = game::NearItem{};
if (!(active && !menuIsUp && shared.cyclopeanCameraValid && !game::PlayerHoldsGrab())) {
	state.pickHold = game::PickHoldState{};
} else {
	const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
	const NiPoint3& camPos = shared.cyclopeanCamera.pos;
	const bool gripping = handResult.grabWanted;
	float reachMetres = config.hands.grabReachMetres;
	reachMetres = config.hands.reachMarkerMetres > reachMetres ? config.hands.reachMarkerMetres
	                                                            : reachMetres;
	reachMetres = config.hands.pullReachMetres > reachMetres ? config.hands.pullReachMetres
	                                                          : reachMetres;
	const float reach = reachMetres * config.tracker.unitsPerMetre;
	// Each hand with its laser, the beam the settings tilt: beyond the
	// grab's reach an item counts only while that beam points at it.
	const auto hand = [&](bool left, bool valid) {
		game::SearchHand h;
		h.valid = valid;
		h.position =
			camPos + camRot * (left ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits);
		if (valid) {
			float pitch = 0.0f;
			float yaw = 0.0f;
			shared.laserAngles(config.hands, left, pitch, yaw);
			h.direction = vr::HandLaserWorldRay(
				camRot, camPos, left ? handResult.leftHandRotation : handResult.rightHandRotation,
				left ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits, pitch, yaw,
				config.hands.laserOriginMetres, config.tracker.unitsPerMetre,
				vr::LaserOffsetLocal(config.hands.laserOffsetRightMetres, config.hands.laserOffsetUpMetres,
			                         vr::PhysicalLeftOf(left, links.rolesSwapped)))
			                  .direction;
			// The palm: the controller's own sideways axis, towards the other
			// hand - a right hand wrapped round the handle faces its palm
			// left (-x), a left hand right (+x). Read off how the controller
			// is held, not measured; a palm turned the wrong way would show
			// as the back of the hand finding items. By the physical side
			// (vr::PalmSideFor): left-handed the roles sit on the other
			// controllers.
			const NiMatrix33& rel = left ? handResult.leftHandRotation : handResult.rightHandRotation;
			const float side = vr::PalmSideFor(left, links.rolesSwapped);
			h.palm = camRot * NiPoint3{rel.data[0][0] * side, rel.data[1][0] * side,
			                           rel.data[2][0] * side};
		}
		return h;
	};
	// Whether the laser picks items at all ([Hands] LaserPicksItems).
	game::g_laserPicksItems = config.hands.laserPicksItems;
	game::NearItem keptByHand[2];
	// Whether the held item is still an item of the cell: one taken with
	// Activate is deleted, and the hold lets it go at once (PickHold.h).
	bool heldAlive = true;
	const game::NearItem best = game::FindNearestItem(
		hand(false, handResult.rightHandValid && (!gripping || !handResult.grabWithLeftHand)),
		hand(true, handResult.leftHandValid && (!gripping || handResult.grabWithLeftHand)), reach,
		config.hands.grabReachMetres * config.tracker.unitsPerMetre, 0,
		state.pickHold.held.valid ? state.pickHold.held.ref : 0, keptByHand, &heldAlive);
	if (test::HandScriptMarkedThisFrame() && state.pickLines > 0) {
		--state.pickLines;
		OBVR_LOG("Pick: at the mark - the hands' search found %08X (valid %d, %s hand, class %u, %.1f units), the "
		         "held pick %08X",
		         best.ref, best.valid ? 1 : 0, best.left ? "left" : "right", static_cast<unsigned>(best.rankClass),
		         static_cast<double>(best.distance), state.pickHold.held.valid ? state.pickHold.held.ref : 0u);
	}
	const UInt32 before = state.pickHold.held.valid ? state.pickHold.held.ref : 0;
	const bool beforeLeft = state.pickHold.held.left;
	if (before != 0 && !heldAlive && state.pickLines > 0) {
		--state.pickLines;
		OBVR_LOG("Pick: %08X is gone from the cell (taken, deleted or hidden) - let go at once", before);
	}
	links.nearItem = game::StepPickHold(state.pickHold, best, keptByHand, dt, heldAlive);
	// An NPC under the pick hand's laser takes the pick from the items
	// (game::NpcTakesPick): the pick runs along the laser, Activate talks.
	{
		const bool pickLeft = links.nearItem.valid ? links.nearItem.left : false;
		const game::SearchHand laser = hand(pickLeft, pickLeft ? handResult.leftHandValid : handResult.rightHandValid);
		const bool npcUnderLaser =
			laser.valid && game::ActorUnderRay(laser.position, laser.direction, game::kNpcTalkUnits,
			                                   game::kNpcUnderLaserCos, nullptr) != nullptr;
		// And a door, a container, an activator or furniture under it the same
		// (game::ActivatorUnderRay): the tester, 2026-10-07, stood before a door
		// with the pick caught on a small thing by the hand.
		const bool activatorUnderLaser =
			laser.valid && links.nearItem.valid && links.nearItem.rankClass != game::kPickTouched &&
			game::ActivatorUnderRay(laser.position, laser.direction, game::kNpcTalkUnits, game::kNpcUnderLaserCos) != 0;
		if (game::NpcTakesPick(npcUnderLaser || activatorUnderLaser, links.nearItem.valid, links.nearItem.rankClass)) {
			if (links.nearItem.valid && !state.npcSaid) {
				state.npcSaid = true;
				OBVR_LOG("Pick: %s under the laser takes it from the items (the laser's pick, Activate %s)",
				         npcUnderLaser ? "an NPC" : "a door or the like", npcUnderLaser ? "talks" : "opens");
			}
			links.nearItem = game::NearItem{};
			state.pickHold = game::PickHoldState{};
		}
	}
	const UInt32 after = links.nearItem.valid ? links.nearItem.ref : 0;
	if ((after != before || (after != 0 && links.nearItem.left != beforeLeft)) && state.pickLines > 0) {
		--state.pickLines;
		OBVR_LOG("Pick: on %08X (class %u, key %.3f, %s hand) - was %08X (%s hand)%s", after,
		         links.nearItem.valid ? links.nearItem.rankClass : 0xFFu,
		         static_cast<double>(links.nearItem.valid ? links.nearItem.rankKey : 0.0f),
		         links.nearItem.left ? "left" : "right", before, beforeLeft ? "left" : "right",
		         best.valid && best.ref != after ? " (the best was another)" : "");
	}
}
handResult.pickWithLeftHand = links.nearItem.valid && links.nearItem.left;
// The grab follows whichever hand is holding it: direction through the aim
// pose (set above based on which grip is down), distance from that hand.
// No floor: a hand brought to the body brings the object with it - the
// quarter-metre floor made it flicker there (2026-09-26), and a gesture
// that stows an object at the body is planned.
const float grabUnits = handResult.grabDistanceMetres * config.tracker.unitsPerMetre;
// The grab by reach (vr::StepGrabReach): the grip arms it, the pick runs
// through the hand, and the key goes down once what it found is within
// reach of that hand - measured from the camera the hands are pinned to.
// Not while the off hand holds a fighter (StepGrapple): that grip is
// theirs.
const bool gripHeld =
	active && !menuIsUp && handResult.grabWanted && !(links.grapple.actor != 0 && handResult.grabWithLeftHand);
bool inReach = false;
UInt32 reachRef = 0;
if (gripHeld && state.grabReachPick && shared.cyclopeanCameraValid) {
	const game::CrosshairTarget target = game::ReadCrosshairTarget();
	const NiPoint3& offset =
		handResult.grabWithLeftHand ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits;
	const NiPoint3 handWorld = shared.cyclopeanCamera.pos +
	                           shared.cyclopeanCamera.rot * offset;
	// The point the laser touched, not the object's origin: the hand has
	// to be at the object itself.
	NiPoint3 hit = target.position;
	game::ReadPickHit(hit);
	// Within the grab's reach, or an item within the pull's, which then
	// floats to the hand (game::GripTakes, game::HeldObject).
	const float perMetre = config.tracker.unitsPerMetre;
	inReach = target.haveRef &&
	          game::GripTakes(
	              math::Sqrt((hit - handWorld).LengthSquared()),
	              game::IsHandItemType(game::RefBaseFormType(target.refAddress)),
	              config.hands.grabReachMetres * perMetre, config.hands.pullReachMetres * perMetre);
	reachRef = target.haveRef ? target.refAddress : 0;
}
// A weapon in the world, at its grip (game/WeaponGrab.h; free-weapons-spec
// section 5): the pick's weapon, its grip point from its model's frame,
// and the weapon hand's palm's distance to it. The hold closing there
// takes it into the inventory, equips it and draws it - the grab key
// stays up; a touch elsewhere on it carries it as before; a weapon is
// never pulled from afar.
state.weaponPick = WeaponPick{};
game::WeaponGrabChoice weaponChoice = game::WeaponGrabChoice::NotAWeapon;
{
	const game::WeaponGrabSettings& wg = config.hands.weaponGrab;
	const float perMetre = config.tracker.unitsPerMetre;
	const SInt32 weaponType = links.nearItem.valid ? game::RefWeaponType(links.nearItem.ref) : -1;
	// A torch is the other hand's (game/TorchGrab.h): the same grip, ring
	// and take, with that hand as the taking one.
	const bool torch = links.nearItem.valid && weaponType < 0 && wg.torches && game::RefIsTorch(links.nearItem.ref);
	NiMatrix33 nodeRot;
	NiPoint3 nodePos;
	if ((weaponType >= 0 || torch) && game::RefWorldTransform(links.nearItem.ref, nodeRot, nodePos)) {
		state.weaponPick.valid = true;
		state.weaponPick.ref = links.nearItem.ref;
		state.weaponPick.type = weaponType;
		state.weaponPick.torch = torch;
		state.weaponPick.gripPoint =
			game::GripPointOf(nodeRot, nodePos, torch ? game::kGripAxialTorch : game::GripAxialFor(weaponType));
		// The taking hand (the weapon hand, the other for a torch): its
		// controller, the point the pick measures every reach from (the
		// ring, the touch, the grab's reach) - the drawn hand's palm sits
		// off it by the tester's own fit, and the run of 21:05 measured the
		// palm 0.15 m from a grip the controller was on.
		NiPoint3 palm{0.0f, 0.0f, 0.0f};
		if (shared.cyclopeanCameraValid) {
			palm = shared.cyclopeanCamera.pos +
			       shared.cyclopeanCamera.rot *
			           (torch ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits);
		} else {
			state.weaponPick.valid = false;
		}
		if (state.weaponPick.valid) {
			state.weaponPick.toGripUnits = math::Sqrt((state.weaponPick.gripPoint - palm).LengthSquared());
			// Nothing in the engine's grab: a reference the grab still holds
			// must not be activated (the run of 2026-10-24 07:58 took a sword
			// the pull had just brought in, with the grab's mode already 0 and
			// the reference still held - the game went down on the spot); a
			// pulled weapon is the pull's business below.
			const bool grabFree = active && !menuIsUp && frame.inWorld && !fitting && !game::PlayerHoldsGrab() &&
			                      game::GrabbedRef() == 0 && !game::PlayerIsDead();
			// A torch: not with the bow in that hand, nor with a shield or a
			// torch already on it - the weapon drawn or not.
			const game::TorchTakeGate torchGate =
				torch ? game::TorchTakeGateFor(wg.torches, grabFree,
				                               frame.equipped == vr::EquippedKind::Bow &&
				                                   frame.weaponSeen == vr::WeaponSeen::Drawn,
				                               game::HandHoldsItem(false, config.hands.leftHandBone),
				                               handResult.holster.leftClaimed)
				      : game::TorchTakeGate::Off;
			const bool allowed = torch ? torchGate == game::TorchTakeGate::Takes
			                           : wg.enabled && grabFree && frame.weaponSeen != vr::WeaponSeen::Drawn &&
			                                 !handResult.holster.rightClaimed;
			const bool takingHand = game::IsTakingHand(torch, links.nearItem.left);
			weaponChoice = game::ChooseWeaponGrab(true, takingHand, allowed, state.weaponPick.toGripUnits,
			                                      takingHand ? links.nearItem.distance : -1.0f,
			                                      wg.gripReachMetres * perMetre, game::kPickTouchUnits,
			                                      wg.pullToHand);
			if ((links.nearItem.ref != state.weaponPickRef || weaponChoice != state.lastWeaponChoice || torchGate != state.torchGate) &&
			    state.weaponGrabLinesLeft > 0) {
				--state.weaponGrabLinesLeft;
				// Where the grip is from the weapon hand's controller in the
				// hand script's frame (metres: right, up, back, along the
				// camera's heading): what a scenario's "right pos" needs.
				NiPoint3 fromController{0.0f, 0.0f, 0.0f};
				if (shared.cyclopeanCameraValid) {
					const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
					const NiPoint3 delta = state.weaponPick.gripPoint - palm;
					NiPoint3 forward = camRot * NiPoint3{0.0f, 1.0f, 0.0f};
					forward.z = 0.0f;
					const float length = math::Sqrt(forward.LengthSquared());
					if (length > 1.0e-4f) {
						forward = forward * (1.0f / length);
						const NiPoint3 right{forward.y, -forward.x, 0.0f};
						fromController = NiPoint3{(delta.x * right.x + delta.y * right.y) / perMetre, delta.z / perMetre,
						                          -(delta.x * forward.x + delta.y * forward.y) / perMetre};
					}
				}
				OBVR_LOG("%s grab: the pick is on %08X (type %d) - its grip at %.1f %.1f %.1f, %.1f units "
				         "(%.2f m) from the %s hand's controller - %.2f right %.2f up %.2f back of it in the "
				         "script's frame - the mesh %.1f units from the %s hand: %s%s%s",
				         torch ? "Torch" : "Weapon", links.nearItem.ref, weaponType,
				         static_cast<double>(state.weaponPick.gripPoint.x),
				         static_cast<double>(state.weaponPick.gripPoint.y), static_cast<double>(state.weaponPick.gripPoint.z),
				         static_cast<double>(state.weaponPick.toGripUnits),
				         static_cast<double>(state.weaponPick.toGripUnits / perMetre), torch ? "other" : "weapon",
				         static_cast<double>(fromController.x),
				         static_cast<double>(fromController.y), static_cast<double>(fromController.z),
				         static_cast<double>(links.nearItem.distance), links.nearItem.left ? "other" : "weapon",
				         weaponChoice == game::WeaponGrabChoice::Equip   ? "a hold takes it"
				         : weaponChoice == game::WeaponGrabChoice::Carry ? "a grip carries it"
				         : weaponChoice == game::WeaponGrabChoice::Pull  ? "a grip pulls it to the hand"
				                                                         : "nothing yet",
				         torch && torchGate != game::TorchTakeGate::Takes ? " - " : "",
				         torch && torchGate != game::TorchTakeGate::Takes ? game::TorchTakeGateWords(torchGate) : "");
			}
			state.weaponPickRef = links.nearItem.ref;
			state.lastWeaponChoice = weaponChoice;
			state.torchGate = torchGate;
		}
	}
}
if (weaponChoice == game::WeaponGrabChoice::Equip || weaponChoice == game::WeaponGrabChoice::Nothing) {
	inReach = false;
}
// The hold that took a weapon, still closed: no grab of anything else
// until it opens (game::WeaponHoldBlocksGrab).
if (game::WeaponHoldBlocksGrab(state.weaponHoldLatch) || game::WeaponHoldBlocksGrab(state.torchHoldLatch)) {
	inReach = false;
	state.grabReach = vr::GrabReachState{};
}
const vr::GrabReachVerdict reach = vr::StepGrabReach(state.grabReach, gripHeld, inReach);
// Whether the grab that just began is a pull of a weapon (game/WeaponGrab.h):
// then the weapon is equipped the moment it arrives in the hand.
{
	if (reach.key && !state.keyWas) {
		// Any weapon the weapon hand's grab takes - pulled from afar or
		// touched on its blade - is equipped the moment it is in the hand:
		// the pick's own choice can be empty in the frame the key goes
		// down (the pick resets as the grip closes, 2026-10-24 07:54), so
		// the key's target itself says what was taken.
		state.weaponPullGrab = reachRef != 0 && game::RefWeaponType(reachRef) >= 0 && !handResult.grabWithLeftHand;
		// A torch the other hand's grab takes is equipped there the same way.
		state.torchPullGrab = reachRef != 0 && handResult.grabWithLeftHand && config.hands.weaponGrab.torches &&
		                  game::RefIsTorch(reachRef);
		state.weaponHoldLeft = handResult.grabWithLeftHand;
		state.sinceGrabKey = 0.0f;
	} else if (!reach.key) {
		state.weaponPullGrab = false;
		state.torchPullGrab = false;
	}
	state.keyWas = reach.key;
	state.sinceGrabKey += links.deltaSeconds == links.deltaSeconds && links.deltaSeconds > 0.0f ? links.deltaSeconds : 0.0f;
}
state.grabReachPick = reach.reachPick;
state.grabKeyDown = reach.key;
handResult.controls.grab = reach.key || links.slapTapPressed;
// Where the held object goes: the palm of the grabbing hand's pinned bone
// (game::HeldObject - the spring pulls the touched point there), moved
// along the fingers by [Hands] HeldObjectMetres; without a pinned bone,
// the controller moved along its laser by as much. Handed to the engine's
// update as a point, which it looks at from its own camera origin.
bool haveHoldPoint = false;
NiPoint3 holdPoint{0.0f, 0.0f, 0.0f};
NiMatrix33 palmRot;
NiPoint3 palmBone;
if (reach.key && game::ReadHandBoneWorld(!handResult.grabWithLeftHand, palmRot, palmBone)) {
	holdPoint = game::PalmPoint(palmRot, palmBone,
	                            (game::kPalmAlongMetres + config.hands.heldObjectMetres) *
	                                config.tracker.unitsPerMetre);
	haveHoldPoint = true;
} else if (reach.key && shared.cyclopeanCameraValid &&
    (handResult.grabWithLeftHand ? handResult.leftHandValid : handResult.rightHandValid)) {
	const bool left = handResult.grabWithLeftHand;
	const vr::LaserWorldRay ray = vr::HandLaserWorldRay(
		shared.cyclopeanCamera.rot, shared.cyclopeanCamera.pos,
		left ? handResult.leftHandRotation : handResult.rightHandRotation,
		left ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits,
		config.hands.laserPitchDegrees,
		vr::LaserYawFor(left, links.rolesSwapped, config.hands.laserYawDegrees),
		config.hands.heldObjectMetres, config.tracker.unitsPerMetre,
		vr::LaserOffsetLocal(config.hands.laserOffsetRightMetres, config.hands.laserOffsetUpMetres,
	                         vr::PhysicalLeftOf(left, links.rolesSwapped)));
	holdPoint = ray.origin;
	haveHoldPoint = true;
}
game::SetGrabAtHand(reach.key, grabUnits, haveHoldPoint, holdPoint, 0.0f);
// The inventory's item taken into the hand whose hold closes
// (game/InventoryGrab.h; Nadi, 2026-10-28): the item under the pointer
// read, the inventory closed, and in the first world frame after it the
// weapon given to the take's steps below (equipped, straight into the
// hand), a shield or a torch equipped in the other hand. Held as an
// object (decision (b), and anything else): dropped at the hand that
// took it, and held by that hand's grab while its hold stays closed.
{
	const game::InventoryHand hand =
		active && config.hands.grabFromInventory
			? game::StepInventoryGrabTrigger(state.inventoryTrigger, frame.inventoryOpen, handResult.rightHold,
			                                 handResult.leftHold)
			: game::InventoryHand::None;
	if (hand != game::InventoryHand::None && state.inventoryTake.take == game::InventoryTake::Nothing) {
		game::InventoryItemUnder item;
		const char* const handName = hand == game::InventoryHand::Right ? "weapon" : "other";
		if (!game::InventoryItemUnderCursor(item)) {
			OBVR_LOG("Inventory grab: the %s hand closed - no item under the pointer", handName);
		} else {
			SInt32 weaponType = -1;
			const game::InventoryItemKind kind = game::InventoryItemKindOfForm(item.form, weaponType);
			const game::InventoryTake take = game::ChooseInventoryTake(kind, hand);
			OBVR_LOG("Inventory grab: the %s hand closed on \"%s\" (form %08X, type %u, kind %u) - take %u",
			         handName, item.name, item.formId, static_cast<unsigned>(item.formType),
			         static_cast<unsigned>(kind), static_cast<unsigned>(take));
			if (take != game::InventoryTake::Nothing) {
				state.inventoryTake.take = take;
				state.inventoryTake.left = hand == game::InventoryHand::Left;
				state.inventoryTake.form = item.form;
				state.inventoryTake.weaponType = weaponType;
				state.inventoryTake.wait = 1.0f;
				if (!game::CloseMenus()) {
					OBVR_LOG("Inventory grab: the inventory could not be closed - nothing taken");
					state.inventoryTake = InventoryTakePending{};
				}
			}
		}
	}
	if (state.inventoryTake.take != game::InventoryTake::Nothing) {
		if (active && !menuIsUp && frame.inWorld) {
			if (state.inventoryTake.take == game::InventoryTake::EquipWeapon) {
				if (state.weaponTake.phase == game::WeaponTakePhase::Idle) {
					const game::WeaponGrabSettings& wg = config.hands.weaponGrab;
					game::BeginWeaponTake(state.weaponTake, state.inventoryTake.form,
					                      vr::KindOfWeaponType(state.inventoryTake.weaponType), wg.waitSeconds,
					                      wg.drawAtOnce);
					OBVR_LOG("Inventory grab: the inventory closed - the weapon to the take's steps");
				} else {
					OBVR_LOG("Inventory grab: a take is running - the weapon stays in the inventory");
				}
			} else if (state.inventoryTake.take == game::InventoryTake::Hold) {
				// Dropped where the hand is, and that hand's grab takes it.
				const bool left = state.inventoryTake.left;
				const NiPoint3 at = shared.cyclopeanCamera.pos +
				                    shared.cyclopeanCamera.rot *
				                        (left ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits);
				const UInt32 ref = shared.cyclopeanCameraValid && links.inventoryHold.ref == 0
				                       ? game::DropItemForm(reinterpret_cast<UInt8*>(state.inventoryTake.form), at)
				                       : 0;
				if (ref != 0) {
					links.inventoryHold = InventoryHold{};
					links.inventoryHold.ref = ref;
					links.inventoryHold.left = left;
					OBVR_LOG("Inventory grab: the inventory closed - %08X dropped at the %s hand (%.1f %.1f %.1f), "
					         "held while the hand stays closed",
					         ref, left ? "other" : "weapon", static_cast<double>(at.x), static_cast<double>(at.y),
					         static_cast<double>(at.z));
				} else {
					OBVR_LOG("Inventory grab: the inventory closed - the drop was refused (a quest item, or no "
					         "camera) - it stays in the inventory");
				}
			} else {
				const bool equipped = game::EquipItemForm(reinterpret_cast<UInt8*>(state.inventoryTake.form), false);
				OBVR_LOG("Inventory grab: the inventory closed - %s", equipped ? "equipped in the other hand"
				                                                              : "the equip refused");
			}
			state.inventoryTake = InventoryTakePending{};
		} else {
			state.inventoryTake.wait -= links.deltaSeconds;
			if (state.inventoryTake.wait <= 0.0f) {
				OBVR_LOG("Inventory grab: no world frame within a second of the close - nothing taken");
				state.inventoryTake = InventoryTakePending{};
			}
		}
	}
}
// The weapon hand's hold closing at a weapon's grip: the take, then the
// take's steps (game/WeaponGrab.h). The hold is the pull's (vr/GripHold.h),
// as the holsters' is; the pick's memory of the thing goes with the take,
// since the activation deletes the reference.
{
	const game::WeaponGrabSettings& wg = config.hands.weaponGrab;
	// Only with no grab in flight at all: not while the grab key is down,
	// not while OBVR's own hold still poses a thing in the hand (a weapon
	// the pull brought in sat at the hand with the engine's grab already
	// gone, the pick found it at 0.07 m and the activation of a thing
	// still posed each frame went down with the game, 2026-10-24 07:54
	// and 07:58). The pull's own take below handles the pulled weapon.
	const bool takeNow = game::StepWeaponTakeTrigger(
		state.weaponHoldLatch, handResult.rightHold,
		weaponChoice == game::WeaponGrabChoice::Equip && !state.weaponPick.torch &&
			state.weaponTake.phase == game::WeaponTakePhase::Idle && !reach.key && !game::HeldObjectArrived() &&
			!state.weaponPullGrab && game::GrabbedRef() == 0 && state.sinceGrabKey > 1.0f);
	if (takeNow) {
		const UInt32 ref = state.weaponPick.ref;
		const UInt32 base = game::RefBaseForm(ref);
		UInt32 owner = 0;
		links.forgetTaken(ref);
		const game::TakeResult taken = game::TakeIntoInventory(ref, &owner);
		if (taken == game::TakeResult::Taken && base != 0) {
			game::BeginWeaponTake(state.weaponTake, base, vr::KindOfWeaponType(state.weaponPick.type), wg.waitSeconds,
			                      wg.drawAtOnce);
			state.pickHold = game::PickHoldState{};
			links.nearItem = game::NearItem{};
			game::BeginTimeline(state.takeTimeline, game::TimelineBit(game::kTakeInHand));
			game::MarkTimeline(state.takeTimeline, game::kTakeTaken);
			state.takeTimelineLeft = vr::KindOfWeaponType(state.weaponPick.type) == vr::EquippedKind::Bow;
		}
		if (state.weaponGrabLinesLeft > 0) {
			--state.weaponGrabLinesLeft;
			char ownerText[32] = "no owner";
			if (owner != 0) {
				std::snprintf(ownerText, sizeof(ownerText), "owner %08X", owner);
			}
			OBVR_LOG("Weapon grab: %08X %s at its grip, %.1f units from the hand (%s; the game's activation "
			         "decides any crime)%s",
			         ref, taken == game::TakeResult::Taken ? "taken" : "NOT taken",
			         static_cast<double>(state.weaponPick.toGripUnits), ownerText,
			         taken == game::TakeResult::Taken
			             ? (base != 0 ? " - equipping" : " - its base form unread, not equipped")
			             : "");
			if (taken != game::TakeResult::Taken) {
				OBVR_LOG("Weapon grab: %s", game::TakeResultName(taken));
			}
		}
	}
	// A pulled weapon arrived in the weapon hand with the hold closed: the
	// grab let go of, then taken and equipped like one grabbed at its grip.
	{
		game::WeaponPullInput pin;
		pin.allowed = wg.enabled && active && !menuIsUp && frame.inWorld && !fitting &&
		              frame.weaponSeen != vr::WeaponSeen::Drawn && !game::PlayerIsDead();
		pin.heldRef = game::GrabbedRef();
		pin.heldType = pin.heldRef != 0 ? game::RefWeaponType(pin.heldRef) : -1;
		pin.heldIsWeapon = pin.heldType >= 0;
		pin.heldForm = pin.heldRef != 0 ? game::RefBaseForm(pin.heldRef) : 0;
		pin.weaponHandHolds = !state.weaponHoldLeft;
		pin.arrived = game::HeldObjectArrived();
		pin.holdClosed = handResult.rightHold;
		pin.pullGrab = state.weaponPullGrab;
		pin.dt = links.deltaSeconds;
		const game::WeaponPullVerdict pv = game::StepWeaponPull(state.weaponPull, pin);
		// The grab physics must not throw or push a weapon that is being
		// taken - the stow's rule (StepGrabPhysics below): a thrown body
		// whose reference the activation deleted went down with the game.
		state.weaponPullTaking = state.weaponPull.phase != game::WeaponPullPhase::Idle || pv.takeNow != 0;
		if (pv.forceGrabUp) {
			handResult.controls.grab = false;
			state.grabReach = vr::GrabReachState{};
			// The pulled weapon arrived: from here until it hangs on the hand.
			if (!state.takeTimeline.running) {
				game::BeginTimeline(state.takeTimeline, game::TimelineBit(game::kTakeInHand));
				state.takeTimelineLeft = vr::KindOfWeaponType(state.weaponPull.type) == vr::EquippedKind::Bow;
			}
		}
		if (pv.gaveUp || pv.cancelled) {
			game::CancelTimeline(state.takeTimeline);
		}
		if (pv.takeNow != 0) {
			UInt32 owner = 0;
			links.forgetTaken(pv.takeNow);
			// Off the grip: no pickup sound (game::WeaponTakeIsLoud).
			const game::TakeResult taken = game::WeaponTakeIsLoud(game::WeaponGrabChoice::Pull)
			                                   ? game::TakeIntoInventory(pv.takeNow, &owner)
			                                   : game::TakeIntoInventoryQuietly(pv.takeNow, &owner);
			if (taken == game::TakeResult::Taken && pv.form != 0 && state.weaponTake.phase == game::WeaponTakePhase::Idle) {
				game::BeginWeaponTake(state.weaponTake, pv.form, vr::KindOfWeaponType(pv.type), wg.waitSeconds,
				                      wg.drawAtOnce);
				game::LatchWeaponHold(state.weaponHoldLatch);
				state.pickHold = game::PickHoldState{};
				links.nearItem = game::NearItem{};
				game::MarkTimeline(state.takeTimeline, game::kTakeTaken);
			} else {
				game::CancelTimeline(state.takeTimeline);
			}
			if (state.weaponGrabLinesLeft > 0) {
				--state.weaponGrabLinesLeft;
				OBVR_LOG("Weapon grab: %08X pulled to the hand and %s (%s)%s", pv.takeNow,
				         taken == game::TakeResult::Taken ? "taken" : "NOT taken", game::TakeResultName(taken),
				         taken == game::TakeResult::Taken && pv.form != 0 ? " - equipping" : "");
			}
		}
		if ((pv.gaveUp || pv.cancelled) && state.weaponGrabLinesLeft > 0) {
			--state.weaponGrabLinesLeft;
			OBVR_LOG("Weapon grab: the pulled weapon %s", pv.gaveUp ? "stayed in the engine's grab - not taken"
			                                                        : "left the hand before the take - nothing done");
		}
	}
	// A torch, the other hand's (game/TorchGrab.h): its hold closing at the
	// grip takes it, a pulled one is taken on arrival, and either is
	// equipped there - burning, as the engine lights a torch in the hand.
	{
		const bool takeNow = game::StepWeaponTakeTrigger(
			state.torchHoldLatch, handResult.leftHold,
			weaponChoice == game::WeaponGrabChoice::Equip && state.weaponPick.torch &&
				state.torchTake.phase == game::TorchTakePhase::Idle && !reach.key && !game::HeldObjectArrived() &&
				!state.torchPullGrab && game::GrabbedRef() == 0 && state.sinceGrabKey > 1.0f);
		if (takeNow) {
			const UInt32 ref = state.weaponPick.ref;
			const UInt32 base = game::RefBaseForm(ref);
			UInt32 owner = 0;
			links.forgetTaken(ref);
			const game::TakeResult taken = game::TakeIntoInventory(ref, &owner);
			if (taken == game::TakeResult::Taken && base != 0) {
				game::BeginTorchTake(state.torchTake, base, wg.waitSeconds);
				state.pickHold = game::PickHoldState{};
				links.nearItem = game::NearItem{};
			}
			if (state.weaponGrabLinesLeft > 0) {
				--state.weaponGrabLinesLeft;
				char ownerText[32] = "no owner";
				if (owner != 0) {
					std::snprintf(ownerText, sizeof(ownerText), "owner %08X", owner);
				}
				OBVR_LOG("Torch grab: %08X %s at its grip by the other hand, %.1f units from it (%s; the game's "
				         "activation decides any crime)%s",
				         ref, taken == game::TakeResult::Taken ? "taken" : "NOT taken",
				         static_cast<double>(state.weaponPick.toGripUnits), ownerText,
				         taken == game::TakeResult::Taken
				             ? (base != 0 ? " - equipping" : " - its base form unread, not equipped")
				             : "");
				if (taken != game::TakeResult::Taken) {
					OBVR_LOG("Torch grab: %s", game::TakeResultName(taken));
				}
			}
		}
		game::WeaponPullInput pin;
		pin.heldRef = game::GrabbedRef();
		const bool heldTorch = pin.heldRef != 0 && game::RefIsTorch(pin.heldRef);
		pin.allowed = wg.torches && active && !menuIsUp && frame.inWorld && !fitting && !game::PlayerIsDead() &&
		              !(frame.equipped == vr::EquippedKind::Bow && frame.weaponSeen == vr::WeaponSeen::Drawn);
		pin.heldIsWeapon = heldTorch;  // the pull's steps are the weapon's, for the torch
		pin.heldForm = heldTorch ? game::RefBaseForm(pin.heldRef) : 0;
		pin.weaponHandHolds = state.weaponHoldLeft;  // the other hand holds it
		pin.arrived = game::HeldObjectArrived();
		pin.holdClosed = handResult.leftHold;
		pin.pullGrab = state.torchPullGrab;
		pin.dt = links.deltaSeconds;
		const game::WeaponPullVerdict pv = game::StepWeaponPull(state.torchPull, pin);
		if (pv.forceGrabUp) {
			handResult.controls.grab = false;
			state.grabReach = vr::GrabReachState{};
		}
		state.weaponPullTaking = state.weaponPullTaking || state.torchPull.phase != game::WeaponPullPhase::Idle ||
		                     pv.takeNow != 0;
		if (pv.takeNow != 0) {
			UInt32 owner = 0;
			links.forgetTaken(pv.takeNow);
			const game::TakeResult taken = game::TakeIntoInventoryQuietly(pv.takeNow, &owner);
			if (taken == game::TakeResult::Taken && pv.form != 0 && state.torchTake.phase == game::TorchTakePhase::Idle) {
				game::BeginTorchTake(state.torchTake, pv.form, wg.waitSeconds);
				game::LatchWeaponHold(state.torchHoldLatch);
				state.pickHold = game::PickHoldState{};
				links.nearItem = game::NearItem{};
			}
			if (state.weaponGrabLinesLeft > 0) {
				--state.weaponGrabLinesLeft;
				OBVR_LOG("Torch grab: %08X pulled to the other hand and %s (%s)%s", pv.takeNow,
				         taken == game::TakeResult::Taken ? "taken" : "NOT taken", game::TakeResultName(taken),
				         taken == game::TakeResult::Taken && pv.form != 0 ? " - equipping" : "");
			}
		}
		if ((pv.gaveUp || pv.cancelled) && state.weaponGrabLinesLeft > 0) {
			--state.weaponGrabLinesLeft;
			OBVR_LOG("Torch grab: the pulled torch %s", pv.gaveUp ? "stayed in the engine's grab - not taken"
			                                                      : "left the hand before the take - nothing done");
		}
		game::TorchTakeInput ttin;
		ttin.allowed = active && !menuIsUp && frame.inWorld;
		ttin.wornTorch = frame.inWorld ? reinterpret_cast<UInt32>(game::WornTorchForm()) : 0;
		ttin.dt = links.deltaSeconds;
		const game::TorchTakeVerdict ttv = game::StepTorchTake(state.torchTake, ttin);
		if (ttv.equipNow && !game::EquipItemForm(reinterpret_cast<UInt8*>(ttv.form), true) &&
		    state.weaponGrabLinesLeft > 0) {
			--state.weaponGrabLinesLeft;
			OBVR_LOG("Torch grab: the equip of %08X could not be sent", ttv.form);
		}
		if ((ttv.done || ttv.gaveUp || ttv.cancelled) && state.weaponGrabLinesLeft > 0) {
			--state.weaponGrabLinesLeft;
			OBVR_LOG("Torch grab: %s", ttv.gaveUp      ? "given up - the torch never showed as worn"
			                           : ttv.cancelled ? "cancelled - a menu, or the world went away"
			                           : game::HandHoldsItem(false, config.hands.leftHandBone)
			                               ? "equipped - in the other hand"
			                               : "equipped - worn, not on the hand yet");
		}
		if (ttv.done) {
			state.torchPalmToSay = true;
		}
		// Where the palm sits on the torch in the hand, once after a take:
		// what the grip point at the model's origin rests on (derived from
		// Torch02.nif, game/TorchGrab.h). In the model's frame, units.
		if (state.torchPalmToSay && game::HandHoldsItem(false, config.hands.leftHandBone)) {
			game::ItemPose held;
			NiMatrix33 boneRot;
			NiPoint3 bone;
			if (links.heldItemPoseNow(1, "Torch", held) && game::ReadHandBoneWorld(false, boneRot, bone)) {
				state.torchPalmToSay = false;
				const NiPoint3 palmNow =
					game::PalmPoint(boneRot, bone, game::kPalmAlongMetres * config.tracker.unitsPerMetre);
				const NiPoint3 local = game::TransposedTurn(held.rot) * (palmNow - held.pos);
				if (state.weaponGrabLinesLeft > 0) {
					--state.weaponGrabLinesLeft;
					OBVR_LOG("Torch grab: in the hand the palm sits at %.1f %.1f %.1f on the torch's model "
					         "(the grip is taken at 0 -2.7 0; the shaft runs -14 to 34 along y)",
					         static_cast<double>(local.x), static_cast<double>(local.y),
					         static_cast<double>(local.z));
				}
			}
		}
	}
	game::WeaponTakeInput tin;
	tin.allowed = active && !menuIsUp && frame.inWorld;
	tin.equipped = frame.equipped;
	tin.seen = frame.weaponSeen;
	tin.dt = links.deltaSeconds;
	if (state.weaponTake.phase != game::WeaponTakePhase::Idle) {
		const bool bowTake = state.weaponTake.kind == vr::EquippedKind::Bow;
		tin.onHand = game::HandHoldsItem(!bowTake, bowTake ? config.hands.leftHandBone : config.hands.rightHandBone);
	}
	// The straight take's frames, for why it does or does not hold.
	if (state.weaponTake.phase != game::WeaponTakePhase::Idle && state.weaponTake.atOnce && state.takeFrameLines > 0) {
		--state.takeFrameLines;
		OBVR_LOG("Weapon grab: at once - equipped kind %u (want %u), seen %u, on the hand %d, sent %d, off the "
		         "hand %.3f s, step %.3f s",
		         static_cast<unsigned>(tin.equipped), static_cast<unsigned>(state.weaponTake.kind),
		         static_cast<unsigned>(tin.seen), tin.onHand ? 1 : 0, state.weaponTake.equipSent ? 1 : 0,
		         static_cast<double>(state.weaponTake.offHand), static_cast<double>(tin.dt));
	}
	const game::WeaponTakeVerdict tv = game::StepWeaponTake(state.weaponTake, tin);
	// Straight into the hand: the weapon on the hip off, the weapon out,
	// then the equip - the game hangs it on the hand without a draw.
	if (tv.unequipFirst) {
		game::UnequipWeapon();
	}
	if (tv.weaponOut && !game::SetPlayerWeaponOut(true) && state.weaponGrabLinesLeft > 0) {
		--state.weaponGrabLinesLeft;
		OBVR_LOG("Weapon grab: the weapon-out flag could not be set - the take draws as before");
	}
	if (tv.fellBack && state.weaponGrabLinesLeft > 0) {
		--state.weaponGrabLinesLeft;
		OBVR_LOG("Weapon grab: straight into the hand did not hold (%s) - drawn by the click (this frame's step "
		         "%.3f s)",
		         frame.weaponSeen == vr::WeaponSeen::Sheathed ? "the game shows it sheathed"
		                                                       : "out, but not on the hand",
		         static_cast<double>(links.deltaSeconds));
	}
	if (tv.equipNow) {
		// The draw that follows goes straight on to its Attach key.
		game::JumpNextDrawToAttach();
		const bool sent = game::EquipWeaponForm(reinterpret_cast<UInt8*>(tv.form));
		if (!sent && state.weaponGrabLinesLeft > 0) {
			--state.weaponGrabLinesLeft;
			OBVR_LOG("Weapon grab: the equip of %08X could not be sent", tv.form);
		}
	}
	if (tv.readyClick) {
		state.weaponTakeReadyClick = true;
	}
	if (tv.done) {
		game::MarkTimeline(state.takeTimeline, game::kTakeEquipped);
	} else if (tv.gaveUp || tv.cancelled) {
		game::CancelTimeline(state.takeTimeline);
	}
	if (state.takeTimeline.running && frame.weaponSeen == vr::WeaponSeen::Drawn &&
	    (state.takeTimeline.marked & game::TimelineBit(game::kTakeTaken)) != 0) {
		game::MarkTimeline(state.takeTimeline, game::kTakeDrawn);
	}
	if ((tv.done || tv.gaveUp || tv.cancelled) && state.weaponGrabLinesLeft > 0) {
		--state.weaponGrabLinesLeft;
		OBVR_LOG("Weapon grab: %s", tv.gaveUp       ? "given up - the weapon never showed as equipped"
		                            : tv.cancelled  ? "cancelled - a menu, or the world went away"
		                            : tv.readyClick ? "equipped - drawing"
		                                            : "equipped and drawn - straight into the hand");
	}
}
	GrabOutput out;
	out.reach = reach;
	out.reachRef = reachRef;
	out.haveHoldPoint = haveHoldPoint;
	out.holdPoint = holdPoint;
	return out;
}

}  // namespace obvr::camera::hands
