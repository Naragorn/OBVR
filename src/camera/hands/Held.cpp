#include "camera/hands/Held.h"

#include <cstdio>

#include "camera/hands/HandGeometry.h"
#include "camera/hands/OverlayPose.h"
#include "core/AddressSpace.h"
#include "core/Log.h"
#include "core/MathFns.h"
#include "game/GameAddresses.h"
#include "game/GrabPhysics.h"
#include "game/HolsterSlots.h"
#include "game/HandAdjust.h"
#include "game/MeleeHits.h"
#include "game/NearbyItems.h"
#include "game/PlayerTeleport.h"
#include "game/TakeItem.h"
#include "render/GameDevice.h"
#include "test/HandScriptRuntime.h"
#include "vr/PotionBelt.h"

namespace obvr::camera::hands {

// The holster place a held potion would be kept at if let go now
// (vr/PotionBelt.h, [Hands] PotionBelt): a potion that is no poison, the
// drawn hand at a place keeping no weapon and no other potion. 0 for none.
static UInt8 PotionPlaceOfHeld(const Config& config, const vr::StowInput& in, bool poison) {
	if (!config.hands.potionBelt || !in.heldIsEdible || poison ||
	    game::RefBaseFormType(in.heldRef) != addr::kFormTypePotion) {
		return 0;
	}
	const vr::HolsterSlot at = vr::SlotAt(vr::SlotZonesFor(config.hands.holster), in.handRelative);
	const UInt32 base = game::RefBaseForm(in.heldRef);
	const UInt32 id = mem::LooksLikeObjectAddress(base) ? *reinterpret_cast<const UInt32*>(base + addr::kFormIdOffset) : 0;
	return static_cast<UInt8>(vr::PotionPlaceFor(at, game::HolsterSlotIds(), game::PotionPlaceIds(), id));
}

// Let go at the body, or a potion at a place: taken as activating takes it;
// the potion's form then kept at the place (vr/PotionBelt.h).
// What the hints read (camera/hands/Hints.h): an item held, and whether to
// eat or drink - a poison is not.
static void NoteHeldForHints(HeldState& state, const vr::StowInput& in, bool poison) {
	state.holdingItem = in.keyDown && in.heldIsItem;
	state.holdingEdible = state.holdingItem && in.heldIsEdible && !poison;
	state.holdingPotion = state.holdingEdible && game::RefBaseFormType(in.heldRef) == addr::kFormTypePotion;
}

static void TakeStowed(HeldState& state, const HeldLinks& links, const vr::StowVerdict& stow) {
	UInt32 owner = 0;
	const UInt32 base = game::RefBaseForm(stow.take);
	const UInt32 id = mem::LooksLikeObjectAddress(base) ? *reinterpret_cast<const UInt32*>(base + addr::kFormIdOffset) : 0;
	links.forgetTaken(stow.take);
	const game::TakeResult taken = game::TakeIntoInventory(stow.take, &owner);
	state.stows += taken == game::TakeResult::Taken;
	const vr::HolsterSlot place = static_cast<vr::HolsterSlot>(stow.keepAt);
	if (stow.keepAt != 0 && taken == game::TakeResult::Taken && id != 0) {
		vr::KeepPotionAt(game::PotionPlaceIds(), place, id);
		OBVR_LOG("Potion belt: the potion %08X kept at %s", id, vr::SlotWords(place));
	}
	if (state.stowLinesLeft > 0) {
		--state.stowLinesLeft;
		char ownerText[32] = "no owner";
		if (owner != 0) {
			std::snprintf(ownerText, sizeof(ownerText), "owner %08X", owner);
		}
		OBVR_LOG("Stow: let go at the %s - %s (%s; the game's activation decides "
		         "any crime)",
		         stow.keepAt != 0 ? "place" : "body", game::TakeResultName(taken), ownerText);
	}
}

void StepHeldObject(HeldState& state, const HandStep& step, const HandShared& shared,
                    const vr::HandModeFrame& frame, const HeldLinks& links) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& handResult = shared.hand;
	const float deltaSeconds = links.deltaSeconds;
	// The hand that held it, kept past the grip opening: the engine lets
	// go a frame or two after the key, and the throw is that hand's speed.
	if (links.grabKey) {
		state.holdLeft = handResult.grabWithLeftHand;
	}
	// SteamVR's velocity of that controller, into the world, carried to
	// the held point: v + w x r (game::PointVelocity).
	const bool throwValid = shared.cyclopeanCameraValid &&
	                        (state.holdLeft ? handResult.leftHandValid : handResult.rightHandValid);
	NiPoint3 throwVelocity{0.0f, 0.0f, 0.0f};
	if (throwValid) {
		const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
		const float perMetre = config.tracker.unitsPerMetre;
		const NiPoint3 v =
			camRot * (state.holdLeft ? handResult.leftVelocity : handResult.rightVelocity) * perMetre;
		const NiPoint3 w =
			camRot * (state.holdLeft ? handResult.leftAngularVelocity : handResult.rightAngularVelocity);
		const NiPoint3 controller =
			shared.cyclopeanCamera.pos +
			camRot * (state.holdLeft ? handResult.leftHandOffsetUnits : handResult.rightHandOffsetUnits);
		const NiPoint3 r = links.haveHoldPoint ? links.holdPoint - controller : NiPoint3{0.0f, 0.0f, 0.0f};
		throwVelocity = game::PointVelocity(v, w, r);
	}
	// Stowing (vr::StepStow): an item let go at the chest or the belly
	// goes into the inventory, taken as activating it would take it, once
	// the engine has let go of it. It is not thrown on the way.
	const vr::HandPose& holding = state.holdLeft ? frame.left : frame.right;
	vr::StowInput stowIn;
	stowIn.allowed = active && !menuIsUp && frame.inWorld;
	stowIn.keyDown = links.grabKey;
	stowIn.heldRef = game::GrabbedRef();
	stowIn.heldIsItem = stowIn.heldRef != 0 && game::RefIsItem(stowIn.heldRef, nullptr);
	bool heldIsPoison = false;
	stowIn.heldIsEdible = stowIn.heldIsItem && game::RefIsEdible(stowIn.heldRef, &heldIsPoison);
	NoteHeldForHints(state, stowIn, heldIsPoison);
	stowIn.handValid = holding.valid && frame.headValid;
	float toMouth = -1.0f;
	bool nearMouth = false;
	if (stowIn.handValid) {
		// At the drawn hand, not the controller (DrawnHandTracking).
		const NiPoint3 drawn = DrawnHandTracking(holding, !state.holdLeft, config.hands);
		stowIn.handRelative = vr::BodyRelative(frame.head, frame.headPosition, drawn);
		stowIn.potionPlace = PotionPlaceOfHeld(config, stowIn, heldIsPoison);
		const NiPoint3 mouth = vr::MouthInTracking(frame.head, frame.headPosition, config.hands.stow);
		toMouth = math::Sqrt((drawn - mouth).LengthSquared());
		nearMouth = vr::AtMouth(drawn, mouth, config.hands.stow);
		// The held thing itself, wherever the hand holds it: its bound's
		// surface from the mouth, both in the world (the camera is the
		// eyes; the mouth below and ahead of them in the head's frame).
		float thingToMouth = -1.0f;
		NiPoint3 centre{};
		float radius = 0.0f;
		const float upm = config.tracker.unitsPerMetre;
		if (stowIn.heldIsEdible && shared.cyclopeanCameraValid && upm > 0.0f &&
		    game::RefWorldBound(stowIn.heldRef, centre, radius)) {
			const NiPoint3 mouthWorld =
				shared.cyclopeanCamera.pos +
				shared.cyclopeanCamera.rot * NiPoint3{0.0f, config.hands.stow.mouthForward * upm,
				                                                -config.hands.stow.mouthDown * upm};
			const float gap = math::Sqrt((centre - mouthWorld).LengthSquared()) - radius;
			thingToMouth = (gap > 0.0f ? gap : 0.0f) / upm;
		}
		nearMouth = vr::ThingAtMouth(nearMouth, thingToMouth, config.hands.stow);
		stowIn.handAtMouth = nearMouth;
		if (test::HandScriptMarkedThisFrame() && state.eatLinesLeft > 0) {
			--state.eatLinesLeft;
			const NiPoint3 off = drawn - mouth;
			OBVR_LOG("Eat: at the mark - the %s drawn hand %.2f m from the mouth (%.2f %.2f %.2f in tracking, x "
			         "right, y up, z back), holding %08X (edible %d, poison %d), the thing's bound %.2f m from "
			         "it (-1 unread), at the mouth %d",
			         state.holdLeft ? "left" : "right", static_cast<double>(toMouth), static_cast<double>(off.x),
			         static_cast<double>(off.y), static_cast<double>(off.z), stowIn.heldRef,
			         stowIn.heldIsEdible ? 1 : 0, heldIsPoison ? 1 : 0, static_cast<double>(thingToMouth),
			         nearMouth ? 1 : 0);
		}
	}
	stowIn.dt = deltaSeconds;
	const vr::StowVerdict stow = vr::StepStow(state.stow, stowIn, config.hands.stow);
	// The spot, while an item is held and the ring is switched on
	// ([Hands] StowSpotVisible, off by default); lit while the hand is in
	// it. While it is being placed, UpdateStowPlacing draws it.
	const bool placing = game::StowPlaceActive();
	if (!placing && vr::StowRingShown(false, config.hands.stow.spotVisible, stow.showSpot) &&
	    frame.headValid) {
		if (state.stowSpotView.lit != stow.atBody) {
			state.stowSpotView.lit = stow.atBody;
			++state.stowSpotRevision;
		}
		const NiPoint3 at =
			vr::StowSpotInTracking(frame.head, frame.headPosition, config.hands.stow);
		state.stowSpotLayer.Show(backend, render::GetGameDevice(),
		                     StowSpotPose(at, frame.headPosition),
		                     2.0f * config.hands.stow.radius, state.stowSpotRevision,
		                     ui::PaintStowSpotFor, &state.stowSpotView);
	} else if (!placing) {
		state.stowSpotLayer.Hide(backend);
	}
	const bool stowing = stow.waiting || stow.take != 0 || links.weaponPullTaking || links.handBackTaking;
	// The held object pushes what it meets: its body driven to the hand
	// (GrabPhysics.h). Not while it is being stowed - it goes into the
	// inventory, not into the chest.
	game::StepGrabPhysics(true, stowing ? 0.0f : config.hands.throwStrength, throwValid,
	                      throwVelocity, deltaSeconds,
	                      config.hands.heldObjectsPush && !stowing);
	links.throwHits(config, deltaSeconds, active && !menuIsUp && frame.inWorld);
	// The hand at the mouth with something to eat: a short buzz, and said.
	if (!stowIn.keyDown) {
		state.eatWasAtMouth = false;
		state.eatPoisonRef = 0;
	} else {
		if (stow.atMouth != state.eatWasAtMouth) {
			state.eatWasAtMouth = stow.atMouth;
			if (stow.atMouth) {
				backend.Pulse(vr::HandDeviceForRole(!state.holdLeft, links.rolesSwapped),
				                                         0.04f, 160.0f, 0.6f);
			}
			if (state.eatLinesLeft > 0) {
				--state.eatLinesLeft;
				OBVR_LOG("Eat: the held %08X is %s the mouth (the drawn hand %.2f m from it)%s", stowIn.heldRef,
				         stow.atMouth ? "at" : "away from", static_cast<double>(toMouth),
				         stow.atMouth ? " - letting go eats it" : "");
			}
		}
		if (heldIsPoison && nearMouth && state.eatPoisonRef != stowIn.heldRef) {
			state.eatPoisonRef = stowIn.heldRef;
			if (state.eatLinesLeft > 0) {
				--state.eatLinesLeft;
				OBVR_LOG("Eat: the held %08X is a poison at the mouth - not drunk (from the inventory the game "
				         "puts a poison on the weapon); let go it drops",
				         stowIn.heldRef);
			}
		}
	}
	if (!stowIn.keyDown) {
		state.stowWasAtBody = false;  // let go: said by the lines below instead
	} else if (stow.atBody != state.stowWasAtBody) {
		state.stowWasAtBody = stow.atBody;
		if (state.stowLinesLeft > 0) {
			--state.stowLinesLeft;
			OBVR_LOG("Stow: the held item is %s the body (the drawn hand at %.2f right, %.2f forward, "
			         "%.2f up)",
			         stow.atBody ? "at" : "away from",
			         static_cast<double>(stowIn.handRelative.x),
			         static_cast<double>(stowIn.handRelative.y),
			         static_cast<double>(stowIn.handRelative.z));
		}
	}
	if (stow.take != 0 && stow.eat) {
		// Let go at the mouth: taken as activating it takes it, then eaten
		// or drunk as the inventory does it (game::EatItem).
		const UInt32 base = game::RefBaseForm(stow.take);
		const UInt8 type = game::RefBaseFormType(stow.take);
		float health = 0.0f, healthBase = 0.0f, fatigue = 0.0f, fatigueBase = 0.0f;
		const bool readBefore =
			game::ReadPlayerHealth(health, healthBase) && game::ReadPlayerFatigue(fatigue, fatigueBase);
		UInt32 owner = 0;
		links.forgetTaken(stow.take);
		// Taken without the pickup sound: the eat's own sound plays, the
		// chew or the swallow the inventory's use plays (Nadi, 2026-10-25:
		// "beim essen muss der ess sound kommen der beim aktivieren auch
		// kommt") - not the pickup's on top of it.
		const game::TakeResult taken = game::TakeIntoInventoryQuietly(stow.take, &owner);
		const bool eaten = taken == game::TakeResult::Taken && game::EatItem(base);
		const char* const sound = eaten ? game::ItemUseSoundName(base) : nullptr;
		state.eatAfterSeconds = eaten ? 1.0f : state.eatAfterSeconds;
		state.meals += eaten ? 1 : 0;
		if (state.eatLinesLeft > 0) {
			--state.eatLinesLeft;
			char ownerText[32] = "no owner";
			if (owner != 0) {
				std::snprintf(ownerText, sizeof(ownerText), "owner %08X", owner);
			}
			char before[96] = "";
			if (readBefore) {
				std::snprintf(before, sizeof(before), "; health %.0f of %.0f, fatigue %.0f of %.0f before",
				              static_cast<double>(health), static_cast<double>(healthBase),
				              static_cast<double>(fatigue), static_cast<double>(fatigueBase));
			}
			const UInt32 formId =
				mem::LooksLikeObjectAddress(base) ? *reinterpret_cast<const UInt32*>(base + addr::kFormIdOffset) : 0u;
			char soundText[64] = "";
			if (eaten) {
				std::snprintf(soundText, sizeof(soundText), " with the game's %s", sound != nullptr ? sound : "sound (its name unread)");
			}
			OBVR_LOG("Eat: let go at the mouth - %08X (form %08X, %s) %s quietly (%s), %s%s%s", stow.take, formId,
			         type == addr::kFormTypePotion ? "a potion" : "an ingredient", game::TakeResultName(taken),
			         ownerText,
			         eaten ? (type == addr::kFormTypePotion ? "drunk" : "eaten")
			               : "NOT eaten - the game's use was not called",
			         soundText, before);
		}
	} else if (stow.take != 0) {
		TakeStowed(state, links, stow);
	} else if ((stow.notItem || stow.gaveUp) && state.stowLinesLeft > 0) {
		--state.stowLinesLeft;
		OBVR_LOG("Stow: let go at the body - %s",
		         stow.notItem ? "not an item, dropped as usual"
		                      : "the engine kept holding it, nothing taken");
	}
}

void StepMealAfter(HeldState& state, float deltaSeconds) {
	if (state.eatAfterSeconds >= 0.0f) {
		state.eatAfterSeconds -= deltaSeconds;
		if (state.eatAfterSeconds < 0.0f) {
			float health = 0.0f, healthBase = 0.0f, fatigue = 0.0f, fatigueBase = 0.0f;
			if (game::ReadPlayerHealth(health, healthBase) && game::ReadPlayerFatigue(fatigue, fatigueBase) &&
			    state.eatLinesLeft > 0) {
				--state.eatLinesLeft;
				OBVR_LOG("Eat: a second on - health %.0f of %.0f, fatigue %.0f of %.0f", static_cast<double>(health),
				         static_cast<double>(healthBase), static_cast<double>(fatigue),
				         static_cast<double>(fatigueBase));
			}
		}
	}
}

void UpdateStowPlacing(HeldState& state, Config& config, vr::OpenVRBackend& backend, const vr::HandModeFrame& frame,
                       float deltaSeconds) {
	vr::StowPlaceInput in;
	in.command = static_cast<vr::StowPlaceCommand>(game::TakeStowPlaceCommand());
	in.rightValid = frame.right.valid && frame.headValid;
	in.leftValid = frame.left.valid && frame.headValid;
	if (frame.headValid) {
		in.right = vr::BodyRelative(frame.head, frame.headPosition, DrawnHandTracking(frame.right, true, config.hands));
		in.left = vr::BodyRelative(frame.head, frame.headPosition, DrawnHandTracking(frame.left, false, config.hands));
	}
	in.rightGrip = frame.right.valid && vr::GripDown(frame.right.buttonsPressed);
	in.leftGrip = frame.left.valid && vr::GripDown(frame.left.buttonsPressed);
	// The right stick up and down sizes the circle, past the dead zone.
	if (frame.right.valid) {
		const float y = frame.right.thumbY;
		const float dead = config.hands.stickDeadZone;
		in.resize = y > dead ? (y - dead) / (1.0f - dead) : (y < -dead ? (y + dead) / (1.0f - dead) : 0.0f);
	}
	in.dtSeconds = deltaSeconds;
	vr::StowSettings& stow = config.hands.stow;
	const vr::StowPlaceVerdict v = vr::StepStowPlace(state.stowPlace, in, stow);
	game::NoteStowPlaceActive(v.active);
	if (in.command == vr::StowPlaceCommand::Start) {
		OBVR_LOG("Stow place: started - the ring at %.2f right, %.2f forward, %.2f up",
		         static_cast<double>(stow.centreRight), static_cast<double>(stow.centreForward),
		         static_cast<double>(stow.centreUp));
	}
	if (in.command == vr::StowPlaceCommand::ResetStays && v.active) {
		OBVR_LOG("Stow place: reset to the default - the ring at %.2f right, %.2f forward, %.2f up, %.2f m "
		         "across, still up",
		         static_cast<double>(v.spot.x), static_cast<double>(v.spot.y), static_cast<double>(v.spot.z),
		         static_cast<double>(2.0f * v.radius));
	}
	if (v.grabbed || v.dropped) {
		OBVR_LOG("Stow place: the ring %s at %.2f right, %.2f forward, %.2f up",
		         v.grabbed ? "taken" : "let go", static_cast<double>(v.spot.x),
		         static_cast<double>(v.spot.y), static_cast<double>(v.spot.z));
	}
	if (v.active || v.save || v.restore) {
		// The running settings follow the circle, so it is drawn - and would
		// stow - where and as large as it is being put.
		stow.centreRight = v.spot.x;
		stow.centreForward = v.spot.y;
		stow.centreUp = v.spot.z;
		if (stow.radius != v.radius) {
			stow.radius = v.radius;
			++state.stowSpotRevision;
		}
	}
	if (v.save) {
		const struct {
			const char* key;
			float value;
		} entries[] = {{"StowRight", v.spot.x}, {"StowForward", v.spot.y}, {"StowUp", v.spot.z}, {"StowRadius", v.radius}};
		bool saved = true;
		for (const auto& entry : entries) {
			char value[32];
			std::snprintf(value, sizeof(value), "%.2f", static_cast<double>(entry.value));
			saved = SaveSetting("Hands", entry.key, value) && saved;
		}
		OBVR_LOG("Stow place: kept at %.2f right, %.2f forward, %.2f up, %.2f m across - the circle hidden again%s",
		         static_cast<double>(v.spot.x), static_cast<double>(v.spot.y),
		         static_cast<double>(v.spot.z), static_cast<double>(2.0f * v.radius), saved ? "" : " - COULD NOT SAVE the INI");
	} else if (v.restore) {
		OBVR_LOG("Stow place: cancelled - the spot back at %.2f right, %.2f forward, %.2f up",
		         static_cast<double>(v.spot.x), static_cast<double>(v.spot.y),
		         static_cast<double>(v.spot.z));
	}
	if (v.active && frame.headValid) {
		if (state.stowSpotView.lit != v.dragging) {
			state.stowSpotView.lit = v.dragging;
			++state.stowSpotRevision;
		}
		const NiPoint3 at = vr::StowSpotInTracking(frame.head, frame.headPosition, stow);
		state.stowSpotLayer.Show(backend, render::GetGameDevice(), StowSpotPose(at, frame.headPosition),
		                     2.0f * stow.radius, state.stowSpotRevision, ui::PaintStowSpotFor,
		                     &state.stowSpotView);
	} else if (v.save || v.restore) {
		state.stowSpotLayer.Hide(backend);
	}
}

}  // namespace obvr::camera::hands
