#include "camera/hands/WeaponRelease.h"

#include <cstdio>

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/HolsterSlotRecord.h"
#include "game/HolsterSlots.h"
#include "game/MeleeHits.h"
#include "game/NearbyItems.h"
#include "game/PlayerTeleport.h"
#include "game/QuickKeys.h"
#include "game/TakeItem.h"
#include "game/TorchFire.h"
#include "game/TorchGroundLogic.h"
#include "game/TorchKeep.h"
#include "render/SceneRenderHook.h"
#include "test/HandScriptRuntime.h"

namespace obvr::camera::hands {

void StepRelease(ReleaseState& state, const HandStep& step, const HandShared& shared,
                 const vr::HandModeFrame& frame, ReleaseLinks& links) {
	const Config& config = step.config;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const float deltaSeconds = links.deltaSeconds;
	const bool fitting = links.holsterFitting;
	vr::HandModeResult& handResult = shared.hand;
	const game::WeaponReleaseSettings& wr = config.hands.weaponRelease;
	const bool bow = frame.equipped == vr::EquippedKind::Bow;
	const vr::HandPose& holdingHand = bow ? frame.left : frame.right;
	const bool holdValid = bow ? handResult.leftHandValid : handResult.rightHandValid;
	game::WeaponReleaseInput rin;
	rin.allowed = active && !menuIsUp && frame.inWorld && !fitting && holdValid && frame.headValid &&
	              holdingHand.valid && links.weaponTake.phase == game::WeaponTakePhase::Idle &&
	              links.weaponPull.phase == game::WeaponPullPhase::Idle && game::PlayerMountedHorse() == 0 &&
	              !game::PlayerIsDead();
	rin.equipped = frame.equipped;
	rin.seen = frame.weaponSeen;
	rin.holdClosed = bow ? handResult.leftHold : handResult.rightHold;
	// At a place that takes the weapon and keeps nothing else: let go
	// there, it is sheathed and kept there (vr/Holster.h); at one that does
	// not take it, it falls as anywhere.
	const vr::HolsterSlot releaseSlot = bow ? handResult.holster.leftSlot : handResult.holster.rightSlot;
	{
		const UInt32 (&places)[vr::kHolsterSlotCount] = game::HolsterSlotIds();
		const UInt32 at = static_cast<UInt32>(releaseSlot);
		rin.inHolsterZone = releaseSlot != vr::HolsterSlot::None &&
		                    vr::SlotAccepts(releaseSlot, vr::SlotItemOf(frame.equipped)) &&
		                    (places[at] == 0 || releaseSlot == frame.equippedSlot);
	}
	rin.atChest = vr::InStowZone(vr::BodyRelative(frame.head, frame.headPosition, holdingHand.position),
	                             config.hands.stow);
	rin.questItem = game::EquippedWeaponIsQuestItem();
	// The left hand holding the sword's blade keeps it when the right lets go
	// (vr/HalfSwordLogic.h).
	rin.heldByOther = !bow && vr::WeaponHeldByOther(links.rightHold, links.rightHoldIn, config.hands.halfSword);
	rin.dt = deltaSeconds;
	const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
	const float perMetre = config.tracker.unitsPerMetre;
	const NiPoint3 handWorld =
		shared.cyclopeanCamera.pos +
		camRot * (bow ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits);
	rin.handVelocity = camRot * (bow ? handResult.leftVelocity : handResult.rightVelocity) * perMetre;
	rin.handAngularVelocity = camRot * (bow ? handResult.leftAngularVelocity : handResult.rightAngularVelocity);
	const game::WeaponReleaseVerdict rv = game::StepWeaponRelease(state.weaponRelease, rin, wr);
	if (rv.opened) {
		game::BeginTimeline(state.releaseTimeline, game::TimelineBit(game::kReleaseOutOfHand));
		state.releaseTimelineLeft = bow;
	}
	if (rv.heldOn || rv.cancelled || rv.refused) {
		game::CancelTimeline(state.releaseTimeline);
	}
	if (rv.heldOn && state.heldOnLinesLeft > 0) {
		--state.heldOnLinesLeft;
		OBVR_LOG("Weapon hold: the hold read open for %.3f s and closed again - kept (the release time is %.2f s)",
		         static_cast<double>(rv.openSeconds), static_cast<double>(wr.releaseSeconds));
	}
	if (rv.sheathe || rv.stow || rv.drop) {
		game::MarkTimeline(state.releaseTimeline, game::kReleaseLetGo);
	}
	if (rv.sheathe || rv.lowerFists) {
		links.weaponTakeReadyClick = true;
	}
	if (rv.sheathe && links.equippedWeaponForm != nullptr) {
		game::KeepAtSlot(game::HolsterSlotIds(), releaseSlot, game::FormIdOf(links.equippedWeaponForm));
		if (links.holster.linesLeft > 0) {
			--links.holster.linesLeft;
			char name[64] = "?";
			game::ReadFormFullName(reinterpret_cast<UInt32>(links.equippedWeaponForm), name, sizeof(name));
			OBVR_LOG("Holster: the %s let go at %s - sheathed and kept there", name, vr::SlotWords(releaseSlot));
		}
	}
	if (rv.stow) {
		game::UnequipWeapon();
	}
	if (rv.lowerFists && state.weaponReleaseLinesLeft > 0) {
		--state.weaponReleaseLinesLeft;
		OBVR_LOG("Weapon hold: the slot is empty and the game shows the fists up on the open hand - lowered");
	}
	// Handed to the other hand (game/HandoverLogic.h, free-weapons-spec
	// 5.14): the weapon hand lets go of a one-hander while the other
	// hand's hold is closed on it - dropped there and held by that hand
	// as an object, thrown or let go as any held thing; the weapon hand's
	// grip on its handle takes it back.
	// The bow the same from the other hand to the weapon hand: it is worn
	// at that hand, taken as a ball of HandoverThingMetres round it.
	bool handOver = false;
	if (rv.drop && (bow || rin.equipped == vr::EquippedKind::OneHand) && links.inventoryHold.ref == 0) {
		const bool takerLeft = !bow;
		const NiPoint3 takerWorld = shared.cyclopeanCamera.pos +
		                            camRot * (takerLeft ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits);
		const bool takerValid = takerLeft ? handResult.leftHandValid : handResult.rightHandValid;
		const bool takerHold = takerLeft ? handResult.leftHold : handResult.rightHold;
		float toThing = -1.0f;
		if (takerValid && !bow && links.drawnWeaponValid) {
			toThing = game::HandToSpanUnits(takerWorld, links.drawnWeaponGrip, links.drawnWeaponTip);
		} else if (takerValid && bow && handResult.leftHandValid) {
			toThing = game::NearHeldInHandUnits(math::Sqrt((takerWorld - handWorld).LengthSquared()),
			                                    config.hands.handoverThingMetres * perMetre);
		}
		handOver = game::HandOverAtLetGo(true, takerHold, toThing, config.hands.handoverReachMetres * perMetre);
		if (takerHold) {
			OBVR_LOG("Handover: the %s hand let go of the %s with the %s hand closed %.1f units from it (reach "
			         "%.1f) - %s",
			         bow ? "other" : "weapon", bow ? "bow" : "weapon", takerLeft ? "other" : "weapon",
			         static_cast<double>(toThing), static_cast<double>(config.hands.handoverReachMetres * perMetre),
			         handOver                                         ? "handed over"
			         : config.hands.handoverReachMetres <= 0.0f ? "the handover off ([Hands] HandoverReachMetres 0), a drop"
			                                                    : "too far, a drop");
		}
	}
	if (rv.drop) {
		// Dropped: no place keeps it any more.
		{
			UInt32 (&places)[vr::kHolsterSlotCount] = game::HolsterSlotIds();
			const vr::HolsterSlot keeping = game::SlotKeeping(places, game::FormIdOf(links.equippedWeaponForm));
			if (keeping != vr::HolsterSlot::None) {
				places[static_cast<UInt32>(keeping)] = 0;
			}
		}
		const NiPoint3 velocity = rv.velocity;
		// Turned and placed as it was held, not as the engine drops it.
		game::ItemPose held;
		const bool haveHeld = links.heldItemPoseNow(bow ? 1 : 0, "Weapon", held);
		const NiPoint3 heldAngles = game::RefAnglesFromMatrix(held.rot);
		const bool asHeld = haveHeld && config.dropTurnAsHeld;
		const UInt32 ref = game::DropEquippedWeapon(asHeld ? held.pos : handWorld, asHeld ? &heldAngles : nullptr);
		if (ref != 0 && haveHeld) {
			float before = -1.0f;
			float after = -1.0f;
			if (asHeld) {
				game::PlaceDroppedRef(ref, held.rot, held.pos, before, after);
			}
			state.weaponDropTurn.ref = ref;
			state.weaponDropTurn.rot = held.rot;
			if (state.weaponReleaseLinesLeft > 0) {
				--state.weaponReleaseLinesLeft;
				OBVR_LOG("Weapon hold: the dropped %08X %s - its 3D %.1f degrees off it as the "
				         "engine made it, %.1f after; %.1f units from the hand's point",
				         ref, asHeld ? "turned as it was held" : "left to the engine's turn (Debug.DropTurnAsHeld 0)", static_cast<double>(before), static_cast<double>(after),
				         static_cast<double>(math::Sqrt((held.pos - handWorld).LengthSquared())));
			}
		} else if (ref != 0 && state.weaponReleaseLinesLeft > 0) {
			--state.weaponReleaseLinesLeft;
			OBVR_LOG("Weapon hold: the held weapon's pose could not be read - %08X dropped as the engine turns it", ref);
		}
		if (ref != 0 && handOver) {
			links.inventoryHold = InventoryHold{};
			links.inventoryHold.ref = ref;
			links.inventoryHold.left = !bow;
			OBVR_LOG("Handover: %08X dropped as it was held and given to the %s hand's grab - held there as an "
			         "object while that hand stays closed",
			         ref, bow ? "weapon" : "other");
		} else if (ref != 0) {
			game::BeginDroppedWeapon(links.droppedWeapon, ref, velocity, wr.bodyWaitSeconds, rv.spin);
			state.droppedAt = handWorld;
			game::WantTimeline(state.releaseTimeline, game::kReleaseShown);
			game::WantTimeline(state.releaseTimeline, game::kReleaseBody);
		}
		// Every let go, as the torch's: its peaks tell a throw that fell.
		{
			const float speed = math::Sqrt(velocity.LengthSquared()) / perMetre;
			OBVR_LOG("Weapon hold: %s - %s%08X at %.1f %.1f %.1f, the hand at %.2f m/s (%s); its fastest %.2f m/s "
			         "in the last %.2f s before it opened, %.2f m/s in the last %.1f s (%.0f ms before)",
			         game::WeaponReleaseWord(rv), ref != 0 ? "the new reference " : "NOTHING dropped, the engine returned ",
			         ref, handWorld.x, handWorld.y, handWorld.z, speed,
			         speed * perMetre >= game::kThrowMinUnitsPerSecond ? "a throw" : "a fall",
			         static_cast<double>(rv.peaks.windowUnits / perMetre),
			         static_cast<double>(game::kThrowWindowSeconds),
			         static_cast<double>(rv.peaks.lookUnits / perMetre),
			         static_cast<double>(game::kThrowLookSeconds),
			         static_cast<double>(rv.peaks.lookAgeSeconds * 1000.0f));
		}
	} else if ((rv.sheathe || rv.stow || rv.refused || rv.armed || rv.cancelled) && state.weaponReleaseLinesLeft > 0) {
		--state.weaponReleaseLinesLeft;
		OBVR_LOG("Weapon hold: %s", rv.armed       ? "the hold closed on the drawn weapon - letting go drops it"
		                            : rv.cancelled ? "the weapon went, or a menu came - the hold is nobody's"
		                                           : game::WeaponReleaseWord(rv));
	}
	// The dropped weapons passing through the hands get their own group
	// back once clear of them.
	{
		const NiPoint3 rightWorld = shared.cyclopeanCamera.pos + camRot * handResult.rightHandOffsetUnits;
		const NiPoint3 leftWorld = shared.cyclopeanCamera.pos + camRot * handResult.leftHandOffsetUnits;
		game::StepDroppedBodies(handResult.rightHandValid, rightWorld, handResult.leftHandValid, leftWorld);
	}
	// The dropped reference: its body comes frames after the drop, and
	// gets the hand's speed then.
	if (links.droppedWeapon.ref != 0) {
		const UInt32 body = game::RefBodyOf(links.droppedWeapon.ref);
		const game::DroppedWeaponVerdict dv = game::StepDroppedWeapon(links.droppedWeapon, body != 0, deltaSeconds);
		if (state.releaseTimeline.running) {
			NiMatrix33 shownRot;
			NiPoint3 shownPos{0.0f, 0.0f, 0.0f};
			if (game::RefWorldTransform(links.droppedWeapon.ref != 0 ? links.droppedWeapon.ref : dv.ref, shownRot, shownPos)) {
				game::MarkTimeline(state.releaseTimeline, game::kReleaseShown);
			}
			if (dv.sendOff) {
				game::MarkTimeline(state.releaseTimeline, game::kReleaseBody);
			}
		}
		if (dv.sendOff) {
			if (state.weaponDropTurn.ref == dv.ref && state.weaponReleaseLinesLeft > 0) {
				--state.weaponReleaseLinesLeft;
				OBVR_LOG("Weapon hold: the dropped %08X has its body - its 3D %.1f degrees off the held turn",
				         dv.ref, static_cast<double>(game::DroppedRefTurnOff(dv.ref, state.weaponDropTurn.rot)));
			}
			// Through the hands and the player until it is clear of them, so
			// the hand it leaves does not knock it away.
			const bool passed = game::PassDroppedBody(dv.ref, body);
			const bool sent = game::SendBodyOff(body, dv.velocity);
			if (dv.spin.LengthSquared() > 0.0f) {
				NiPoint3 kept{0.0f, 0.0f, 0.0f};
				const bool spun = game::SpinBody(body, dv.spin, &kept);
				if (state.weaponReleaseLinesLeft > 0) {
					--state.weaponReleaseLinesLeft;
					OBVR_LOG("Weapon hold: the thrown %08X %s at %.1f radians a second (%.2f turns), its body keeps "
					         "%.1f",
					         dv.ref, spun ? "spun" : "could NOT be spun",
					         static_cast<double>(math::Sqrt(dv.spin.LengthSquared())),
					         static_cast<double>(math::Sqrt(dv.spin.LengthSquared()) / math::kTwoPi),
					         static_cast<double>(math::Sqrt(kept.LengthSquared())));
				}
			}
			NiMatrix33 rot;
			NiPoint3 pos{0.0f, 0.0f, 0.0f};
			const bool placed = game::RefWorldTransform(dv.ref, rot, pos);
			state.droppedFlight = DroppedFlight{};
			state.droppedFlight.ref = dv.ref;
			state.droppedFlight.given = math::Sqrt(dv.velocity.LengthSquared());
			if (state.weaponReleaseLinesLeft > 0) {
				--state.weaponReleaseLinesLeft;
				OBVR_LOG("Weapon hold: the dropped %08X has its body %08X after %u frame(s) - %s at %.2f m/s%s; its model %s%.1f units from where the hand let go",
				         dv.ref, body, dv.frames, sent ? "sent off" : "NOT sent off (motion unreadable)",
				         math::Sqrt(dv.velocity.LengthSquared()) / perMetre,
				         passed ? ", through the hands and the player until clear of them" : "",
				         placed ? "" : "(no transform read) ",
				         placed ? math::Sqrt((pos - state.droppedAt).LengthSquared()) : 0.0f);
			}
		} else if (dv.gaveUp && state.weaponReleaseLinesLeft > 0) {
			--state.weaponReleaseLinesLeft;
			OBVR_LOG("Weapon hold: the dropped %08X got no body within the wait (%u frames) - it falls as the engine lets it",
			         dv.ref, dv.frames);
		}
	} else if (state.droppedFlight.ref != 0) {
		NiMatrix33 rot;
		NiPoint3 pos{0.0f, 0.0f, 0.0f};
		if (!game::RefWorldTransform(state.droppedFlight.ref, rot, pos)) {
			state.droppedFlight = DroppedFlight{};
		} else {
			if (!state.droppedFlight.haveLast) {
				state.droppedFlight.first = pos;
			} else if (deltaSeconds > 0.0f) {
				state.droppedFlight.speeds[state.droppedFlight.count++] =
					math::Sqrt((pos - state.droppedFlight.last).LengthSquared()) / deltaSeconds;
			}
			state.droppedFlight.last = pos;
			state.droppedFlight.haveLast = true;
			if (state.droppedFlight.count == 12) {
				if (state.droppedFlightLines > 0) {
					--state.droppedFlightLines;
					const float* const f = state.droppedFlight.speeds;
					const NiPoint3 moved = pos - state.droppedFlight.first;
					OBVR_LOG("Weapon hold: flight of the dropped %08X (OBVR gave %.0f units/s), units/s per frame: "
					         "%.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f %.0f; moved %.1f %.1f %.1f in "
					         "those frames",
					         state.droppedFlight.ref, static_cast<double>(state.droppedFlight.given),
					         static_cast<double>(f[0]), static_cast<double>(f[1]), static_cast<double>(f[2]),
					         static_cast<double>(f[3]), static_cast<double>(f[4]), static_cast<double>(f[5]),
					         static_cast<double>(f[6]), static_cast<double>(f[7]), static_cast<double>(f[8]),
					         static_cast<double>(f[9]), static_cast<double>(f[10]), static_cast<double>(f[11]),
					         static_cast<double>(moved.x), static_cast<double>(moved.y),
					         static_cast<double>(moved.z));
				}
				state.droppedFlight = DroppedFlight{};
			}
		}
	}
	// The other hand's torch let go of (game/TorchRelease.h): held while
	// the hold is closed on it, dropped still burning when it opens
	// anywhere but a holster or the chest (Nadi, 2026-10-26).
	{
		game::SetTorchKeep(active && config.hands.torchWithTwoHander);
		// A two-hander drawn: whether the torch is in the other hand,
		// said on each change (game/TorchKeepLogic.h).
		{
			const bool twoHandDrawn = active && frame.inWorld && frame.equipped == vr::EquippedKind::TwoHand &&
			                          frame.weaponSeen == vr::WeaponSeen::Drawn;
			const bool worn = twoHandDrawn && game::WornTorchForm() != nullptr;
			const bool held = worn && game::HandHoldsItem(false, config.hands.leftHandBone);
			const int now = !twoHandDrawn ? -1 : held ? 2 : worn ? 1 : 0;
			if (now != state.twoHandTorch) {
				state.twoHandTorch = now;
				if (now >= 0 && state.torchLines > 0) {
					--state.torchLines;
					OBVR_LOG("Torch: a two-hander drawn - %s (TorchWithTwoHander %d, the stubs %s)",
					         now == 2   ? "the torch in the other hand"
					         : now == 1 ? "the torch worn but off the hand"
					                    : "no torch worn - put away, or none",
					         config.hands.torchWithTwoHander ? 1 : 0, game::TorchKeepInstalled() ? "in" : "out");
				}
			}
		}
		const game::TorchReleaseSettings& tr = config.hands.torchRelease;
		game::TorchReleaseInput tin;
		tin.allowed = active && !menuIsUp && frame.inWorld && !fitting && handResult.leftHandValid && frame.headValid &&
		              frame.left.valid && game::PlayerMountedHorse() == 0 && !game::PlayerIsDead();
		tin.torchInHand = frame.offHand == vr::OffHandItem::Torch &&
		                  game::HandHoldsItem(false, config.hands.leftHandBone);
		tin.holdClosed = handResult.leftHold;
		// The holster's claim is not in the way when it is the take of the
		// torch itself: that hold now holds the torch (vr::HolsterVerdict
		// leftTook).
		tin.holdIsTaken = (frame.holdingObject && handResult.grabWithLeftHand) || links.grapple.actor != 0 ||
		                  links.leftGripOnHandle || (handResult.holster.leftClaimed && !handResult.holster.leftTook);
		// At a free place that takes a torch (the hips): let go there, it is
		// stowed there (vr/Holster.h); anywhere else it falls.
		const vr::HolsterSlot torchSlot = handResult.holster.leftSlot;
		tin.torchWorn = game::WornTorchForm() != nullptr;
		tin.justEquipped = links.torchJustEquipped;
		links.torchJustEquipped = false;
		tin.inHolsterZone = torchSlot != vr::HolsterSlot::None &&
		                    vr::SlotAccepts(torchSlot, vr::SlotItem::Torch) &&
		                    (game::HolsterSlotIds()[static_cast<UInt32>(torchSlot)] == 0 ||
		                     frame.slotWorn[static_cast<UInt32>(torchSlot)]);
		tin.atChest = vr::InStowZone(vr::BodyRelative(frame.head, frame.headPosition, frame.left.position),
		                             config.hands.stow);
		tin.handVelocity = camRot * handResult.leftVelocity * perMetre;
		tin.handAngularVelocity = camRot * handResult.leftAngularVelocity;
		tin.dt = deltaSeconds;
		const game::TorchReleaseVerdict tv = game::StepTorchRelease(state.torchRelease, tin, tr);
		const NiPoint3 leftWorld = shared.cyclopeanCamera.pos + camRot * handResult.leftHandOffsetUnits;
		if (tv.stow) {
			game::UnequipItemForm(game::WornTorchForm(), true);
		}
		if (tv.holster && config.hands.holster.offHandItems) {
			UInt8* const torch = game::WornTorchForm();
			const bool sent = torch != nullptr && game::UnequipItemForm(torch, false);
			if (sent) {
				game::KeepAtSlot(game::HolsterSlotIds(), torchSlot, game::FormIdOf(torch));
			}
			if (links.holster.offHandLinesLeft > 0) {
				--links.holster.offHandLinesLeft;
				char name[64] = "?";
				if (torch != nullptr) {
					game::ReadFormFullName(reinterpret_cast<UInt32>(torch), name, sizeof(name));
				}
				OBVR_LOG("Holster: the other hand's %s let go at %s - %s", name, vr::SlotWords(torchSlot),
				         sent ? "taken off, stowed there" : "could NOT be taken off");
			}
		}
		// Handed to the weapon hand (game/HandoverLogic.h): its hold closed
		// near the torch's hand when this one lets go - held there as an
		// object, burning, no throw.
		bool torchHandOver = false;
		if (tv.drop && !tv.unheld && links.inventoryHold.ref == 0 && handResult.rightHold && handResult.rightHandValid) {
			const NiPoint3 rightWorld = shared.cyclopeanCamera.pos + camRot * handResult.rightHandOffsetUnits;
			const float toThing = game::NearHeldInHandUnits(math::Sqrt((rightWorld - leftWorld).LengthSquared()),
			                                                config.hands.handoverThingMetres * perMetre);
			torchHandOver =
				game::HandOverAtLetGo(true, true, toThing, config.hands.handoverReachMetres * perMetre);
			OBVR_LOG("Handover: the other hand let go of the torch with the weapon hand closed %.1f units from it - %s",
			         static_cast<double>(toThing), torchHandOver ? "handed over"
			         : config.hands.handoverReachMetres <= 0.0f ? "the handover off ([Hands] HandoverReachMetres 0), a drop"
			                                                    : "too far, a drop");
		}
		if (tv.drop) {
			game::ItemPose held;
			const bool haveHeld = links.heldItemPoseNow(1, "Torch", held);
			const NiPoint3 heldAngles = game::RefAnglesFromMatrix(held.rot);
			const bool asHeld = haveHeld && config.dropTurnAsHeld;
			const UInt32 ref =
				game::DropEquippedTorch(asHeld ? held.pos : leftWorld, asHeld ? &heldAngles : nullptr);
			if (ref != 0 && haveHeld) {
				float before = -1.0f;
				float after = -1.0f;
				if (asHeld) {
					game::PlaceDroppedRef(ref, held.rot, held.pos, before, after);
				}
				state.torchDropTurn.ref = ref;
				state.torchDropTurn.rot = held.rot;
				if (state.torchLines > 0) {
					--state.torchLines;
					OBVR_LOG("Torch: the dropped %08X %s - its 3D %.1f degrees off it as the "
					         "engine made it, %.1f after; %.1f units from the hand's point",
					         ref, asHeld ? "turned as it was held" : "left to the engine's turn (Debug.DropTurnAsHeld 0)", static_cast<double>(before), static_cast<double>(after),
					         static_cast<double>(math::Sqrt((held.pos - leftWorld).LengthSquared())));
				}
			}
			if (ref != 0 && torchHandOver) {
				links.inventoryHold = InventoryHold{};
				links.inventoryHold.ref = ref;
				links.inventoryHold.left = false;
				state.torchFireRef = ref;
				state.torchFireFrames = 0;
				state.torchFireLit = false;
				OBVR_LOG("Handover: the torch %08X dropped as it was held and given to the weapon hand's grab", ref);
			} else if (ref != 0) {
				game::BeginDroppedWeapon(state.droppedTorch, ref, tv.velocity, config.hands.weaponRelease.bodyWaitSeconds,
				                         tv.spin);
				state.torchFireRef = ref;
				state.torchFireFrames = 0;
				state.torchFireLit = false;
			}
			// Every let go, not out of the line budget: a throw that fell
			// is told by its peaks (2026-10-27, two of five unlogged).
			{
				const float speed = math::Sqrt(tv.velocity.LengthSquared()) / perMetre;
				OBVR_LOG("Torch: let go - %s%08X at %.1f %.1f %.1f, the hand at %.2f m/s (%s)%s; its fastest %.2f m/s "
				         "in the last %.2f s before it opened, %.2f m/s in the last %.1f s (%.0f ms before)",
				         ref != 0 ? "the new reference " : "NOTHING dropped, the engine returned ", ref,
				         static_cast<double>(leftWorld.x), static_cast<double>(leftWorld.y),
				         static_cast<double>(leftWorld.z), static_cast<double>(speed),
				         speed * perMetre >= game::kThrowMinUnitsPerSecond ? "a throw" : "a fall",
				         tv.unheld ? " - equipped into an open hand and never held (TorchGraceSeconds)" : "",
				         static_cast<double>(tv.peaks.windowUnits / perMetre),
				         static_cast<double>(game::kThrowWindowSeconds),
				         static_cast<double>(tv.peaks.lookUnits / perMetre),
				         static_cast<double>(game::kThrowLookSeconds),
				         static_cast<double>(tv.peaks.lookAgeSeconds * 1000.0f));
			}
		} else if ((tv.armed || tv.stow || tv.holster || tv.cancelled) && state.torchLines > 0) {
			--state.torchLines;
			OBVR_LOG("Torch: %s", tv.armed     ? "the hold closed on the torch - letting go drops it, burning"
			                      : tv.stow    ? "let go at the chest - into the inventory"
			                      : tv.holster ? "opened at a place on the body - stowed there"
			                                   : "the torch went, a menu came or the hold was taken - not dropped");
		}
		if (tv.heldOn && state.torchLines > 0) {
			--state.torchLines;
			OBVR_LOG("Torch: the hold read open for %.3f s and closed again - kept",
			         static_cast<double>(tv.openSeconds));
		}
		if (state.droppedTorch.ref != 0) {
			const UInt32 body = game::RefBodyOf(state.droppedTorch.ref);
			const game::DroppedWeaponVerdict dv = game::StepDroppedWeapon(state.droppedTorch, body != 0, deltaSeconds);
			if (dv.sendOff) {
				if (state.torchDropTurn.ref == dv.ref && state.torchLines > 0) {
					--state.torchLines;
					OBVR_LOG("Torch: the dropped %08X has its body - its 3D %.1f degrees off the held turn", dv.ref,
					         static_cast<double>(game::DroppedRefTurnOff(dv.ref, state.torchDropTurn.rot)));
				}
				game::PassDroppedBody(dv.ref, body);
				game::SendBodyOff(body, dv.velocity);
				if (dv.spin.LengthSquared() > 0.0f) {
					NiPoint3 kept{0.0f, 0.0f, 0.0f};
					const bool spun = game::SpinBody(body, dv.spin, &kept);
					if (state.torchLines > 0) {
						--state.torchLines;
						OBVR_LOG("Torch: the thrown %08X %s at %.1f radians a second, its body keeps %.1f", dv.ref,
						         spun ? "spun" : "could NOT be spun",
						         static_cast<double>(math::Sqrt(dv.spin.LengthSquared())),
						         static_cast<double>(math::Sqrt(kept.LengthSquared())));
					}
				}
			}
		}
		// The fire: put on once the dropped torch has its 3D, read
		// before and after, and read again a second on.
		if (state.torchFireRef != 0) {
			++state.torchFireFrames;
			const auto logReading = [](const char* when, UInt32 ref, const game::TorchFireReading& r) {
				OBVR_LOG("Torch: the dropped %08X %s - form flags %08X (fire out %d), the light's flags %08X, %u "
				         "flame node(s) with %u flame(s), the cap %s, %u under AttachLight%s%s; at %.1f %.1f %.1f%s",
				         ref, when, r.formFlags, (r.formFlags & 0x80) != 0 ? 1 : 0, r.lightFlags, r.flameNodes,
				         r.flames, !r.capFound ? "not found" : r.capShown ? "shown" : "hidden", r.lightsUnder,
				         r.lightsUnder > 0 ? " - " : "", r.lightClass, static_cast<double>(r.position.x),
				         static_cast<double>(r.position.y), static_cast<double>(r.position.z),
				         r.hidden ? ", hidden" : "");
			};
			game::TorchFireReading reading;
			const bool have = game::ReadTorchFire(state.torchFireRef, reading) && reading.haveNode;
			if (!state.torchFireLit && have) {
				if (state.torchLines > 0) {
					--state.torchLines;
					logReading("as the engine made it", state.torchFireRef, reading);
				}
				char why[96] = "";
				game::FlameShapeStates states;
				game::ReadDroppedTorchShapeStates(state.torchFireRef, states);
				links.logTorchShapeStates("the shapes of the dropped", state.torchFireRef, states);
				// Made with the fire on, the engine's set-up puts the flames on;
				// OBVR adds only the light (TorchGroundLogic.h, EngineLightsDrop).
				state.torchFireLit = game::IgniteDroppedTorch(state.torchFireRef, !game::EngineLightsDrop(reading.formFlags),
				                                          why, sizeof(why));
				state.torchFireLitAt = state.torchFireFrames;
				game::ReadDroppedTorchShapeStates(state.torchFireRef, states);
				links.logTorchShapeStates("with its flames on, the shapes of the dropped", state.torchFireRef, states);
				game::TorchFireReading after;
				game::ReadTorchFire(state.torchFireRef, after);
				if (state.torchLines > 0) {
					--state.torchLines;
					if (state.torchFireLit) {
						logReading("with its fire put on", state.torchFireRef, after);
					} else {
						OBVR_LOG("Torch: the dropped %08X could not be lit (%s)", state.torchFireRef, why);
					}
				}
				if (!state.torchFireLit) {
					state.torchFireRef = 0;
				}
			} else if (state.torchFireLit && state.torchFireFrames <= state.torchFireLitAt + 3) {
				// The frames the renderer first draws the flames in.
				game::FlameShapeStates states;
				game::ReadDroppedTorchShapeStates(state.torchFireRef, states);
				char what[64];
				std::snprintf(what, sizeof(what), "%u frame(s) after its flames, the shapes of the dropped",
				              state.torchFireFrames - state.torchFireLitAt);
				links.logTorchShapeStates(what, state.torchFireRef, states);
			} else if (state.torchFireLit && state.torchFireFrames == 90) {
				if (state.torchLines > 0) {
					--state.torchLines;
					if (have) {
						logReading("a second on", state.torchFireRef, reading);
					} else {
						OBVR_LOG("Torch: the dropped %08X has no 3D a second on", state.torchFireRef);
					}
				}
				state.torchFireRef = 0;
			} else if (!state.torchFireLit && state.torchFireFrames > 180) {
				if (state.torchLines > 0) {
					--state.torchLines;
					OBVR_LOG("Torch: the dropped %08X got no 3D within two seconds - not lit", state.torchFireRef);
				}
				state.torchFireRef = 0;
			}
		}
		// The harness's take of the torch it let go of (Debug.TorchTakeBackSeconds).
		{
			if (state.torchFireLit && state.torchFireRef != 0 && state.takeRef != state.torchFireRef) {
				state.takeRef = state.torchFireRef;
				state.takeIn = config.torchTakeBackSeconds;
			}
			if (state.takeRef != 0 && config.torchTakeBackSeconds > 0.0f) {
				state.takeIn -= deltaSeconds;
				if (state.takeIn <= 0.0f) {
					links.forgetTaken(state.takeRef);
					UInt32 owner = 0;
					const game::TakeResult taken = game::TakeIntoInventory(state.takeRef, &owner);
					OBVR_LOG("Torch: Debug.TorchTakeBackSeconds - the burning %08X taken back (%s)", state.takeRef,
					         game::TakeResultName(taken));
					if (state.torchFireRef == state.takeRef) {
						state.torchFireRef = 0;
					}
					state.takeRef = 0;
				}
			}
		}
		// The torches lying in the player's cell that OBVR lit: one whose 3D
		// came back without its light - a cell left and come back to, a save
		// loaded - gets it again (game/TorchGroundLogic.h; the mark is the
		// reference's own fire flag, kept in the save by the engine).
		if (frame.inWorld && !menuIsUp && ++state.litTorchFrames >= 30) {
			state.litTorchFrames = 0;
			game::TorchCellScan scan;
			bool scanned = false;
			{
				perf::Profiler::ScopedSpan span(perf::Profiler::Instance(), perf::EventType::TorchScan);
				scanned = game::ScanTorchesInPlayerCell(scan);
			}
			if (scanned) {
				if ((scan.cell != state.cell || scan.ours != state.ours || scan.relit > 0) && state.litTorchLines > 0) {
					--state.litTorchLines;
					OBVR_LOG("Torch: the cell %08X - %u carried light(s) lying in it, %u burning by OBVR's mark "
					         "(%u not ready); %u given their light again%s",
					         scan.cell, scan.carried, scan.ours, scan.waiting, scan.relit,
					         scan.relit > 0 ? " - the 3D came back without it" : "");
				}
				state.cell = scan.cell;
				state.ours = scan.ours;
			}
		}
	}
	// The weapon on or off the hand's bone, and the timelines' frame.
	if (links.takeTimeline.running && (links.takeTimeline.marked & game::TimelineBit(game::kTakeTaken)) != 0 &&
	    game::HandHoldsItem(!links.takeTimelineLeft,
	                        links.takeTimelineLeft ? config.hands.leftHandBone : config.hands.rightHandBone)) {
		game::MarkTimeline(links.takeTimeline, game::kTakeInHand);
	}
	if (state.releaseTimeline.running && (state.releaseTimeline.marked & game::TimelineBit(game::kReleaseLetGo)) != 0 &&
	    !game::HandHoldsItem(!state.releaseTimelineLeft,
	                         state.releaseTimelineLeft ? config.hands.leftHandBone : config.hands.rightHandBone)) {
		game::MarkTimeline(state.releaseTimeline, game::kReleaseOutOfHand);
	}
	// The probe of an equip with the weapon out ([Debug] WeaponNodeTrace,
	// free-weapons-spec 5.6 option 1): what hangs on each hand and the
	// game's weapon state, at every change, with the scene call to count
	// frames by.
	if (config.hands.weaponNodeTrace) {
		const UInt32 seen = (static_cast<UInt32>(frame.weaponSeen) << 8) | static_cast<UInt32>(frame.equipped);
		if (game::HungChanged(state.hungState, seen) && state.traceLines > 0) {
			--state.traceLines;
			OBVR_LOG("Weapon node trace: the game's weapon %s, equipped kind %u (scene call %u)",
			         frame.weaponSeen == vr::WeaponSeen::Drawn      ? "out"
			         : frame.weaponSeen == vr::WeaponSeen::Sheathed ? "sheathed"
			                                                         : "unknown",
			         static_cast<UInt32>(frame.equipped), render::CurrentSceneCall());
		}
		for (int side = 0; side < 2; ++side) {
			const bool right = side == 0;
			const char* holder = nullptr;
			const UInt32 hung = game::HandHungItem(
				right, right ? config.hands.rightHandBone : config.hands.leftHandBone, &holder);
			const UInt32 was = state.hung[side].last;
			if (game::HungChanged(state.hung[side], hung) && state.traceLines > 0) {
				--state.traceLines;
				OBVR_LOG("Weapon node trace: the %s hand's %s%s%s now holds %08X (was %08X) (scene call %u)",
				         right ? "right" : "left", holder != nullptr ? "\"" : "", holder != nullptr ? holder : "bone",
				         holder != nullptr ? "\"" : "", hung, was, render::CurrentSceneCall());
			}
		}
	}
	const game::TimelineEnd takeEnd = game::StepTimeline(links.takeTimeline, deltaSeconds);
	if ((takeEnd == game::TimelineEnd::Done || takeEnd == game::TimelineEnd::RanOut) && state.timelineLinesLeft > 0) {
		--state.timelineLinesLeft;
		links.logTakeTimeline(links.takeTimeline, takeEnd);
	}
	const game::TimelineEnd releaseEnd = game::StepTimeline(state.releaseTimeline, deltaSeconds);
	if ((releaseEnd == game::TimelineEnd::Done || releaseEnd == game::TimelineEnd::RanOut) &&
	    state.timelineLinesLeft > 0) {
		--state.timelineLinesLeft;
		links.logReleaseTimeline(state.releaseTimeline, releaseEnd);
	}
}

}  // namespace obvr::camera::hands
