#include "camera/hands/Holster.h"

#include <cstdio>
#include <cstring>

#include "camera/hands/OverlayPose.h"
#include "core/Log.h"
#include "game/HandAdjust.h"
#include "game/HolsterSlotRecord.h"
#include "game/HolsterSlots.h"
#include "game/MeleeHits.h"
#include "game/QuickKeys.h"
#include "platform/PluginPath.h"
#include "render/GameDevice.h"
#include "test/HandScriptRuntime.h"
#include "vr/HandInput.h"

namespace obvr::camera::hands {
namespace {

constexpr float kHolsterHintSeconds = 2.5f;

}  // namespace

void StepHolsterReach(HolsterState& state, const HandShared& shared, const vr::HandModeFrame& frame,
                      UInt8* equippedWeaponForm, bool rolesSwapped) {
	const vr::HandModeResult& hand = shared.hand;
	const vr::HolsterVerdict& h = hand.holster;
	UInt32 (&places)[vr::kHolsterSlotCount] = game::HolsterSlotIds();
	const auto nameOf = [](const UInt8* form, char* out, UInt32 size) {
		std::snprintf(out, size, "%s", "?");
		if (form != nullptr) {
			game::ReadFormFullName(reinterpret_cast<UInt32>(form), out, size);
		}
	};
	char name[64] = "";
	// The weapon kept at the place reached for: equipped first, drawn
	// once the game shows it (vr::StepHolster); one that cannot be
	// equipped is forgotten there.
	if (h.equip != vr::HolsterSlot::None) {
		const UInt32 at = static_cast<UInt32>(h.equip);
		UInt8* const form = game::HolsterSlotForm(places[at]);
		if (form == nullptr || !game::EquipWeaponForm(form)) {
			places[at] = 0;
		}
	}
	// The drawn (or kept-nowhere) weapon is kept at the place it was put.
	if (h.assign != vr::HolsterSlot::None && equippedWeaponForm != nullptr) {
		game::KeepAtSlot(places, h.assign, game::FormIdOf(equippedWeaponForm));
	}
	if (h.gaveUp && h.gaveUpAt != vr::HolsterSlot::None) {
		nameOf(game::HolsterSlotForm(places[static_cast<UInt32>(h.gaveUpAt)]), name, sizeof(name));
		places[static_cast<UInt32>(h.gaveUpAt)] = 0;
		if (state.linesLeft > 0) {
			--state.linesLeft;
			OBVR_LOG("Holster: the %s at %s never showed in the hand (no longer in the pack?) - forgotten there",
			         name, vr::SlotWords(h.gaveUpAt));
		}
	}
	// The other hand's shield or torch at a place (vr::StepHolster):
	// stowed - kept at the place, taken off into the inventory - or taken
	// back from there, with the game's item sound; each watched until the
	// item shows it.
	{
		const auto say = [&state](const char* text, const char* item, const char* place) {
			if (state.offHandLinesLeft > 0) {
				--state.offHandLinesLeft;
				OBVR_LOG("Holster: the other hand's %s %s %s", item, text, place);
			}
		};
		if (h.offHandStow != vr::HolsterSlot::None) {
			const UInt32 at = static_cast<UInt32>(h.offHandStow);
			UInt8* const shield = game::WornShieldForm();
			UInt8* const form = shield != nullptr ? shield : game::WornTorchForm();
			nameOf(form, name, sizeof(name));
			bool sent = false;
			if (form != nullptr) {
				game::KeepAtSlot(places, h.offHandStow, game::FormIdOf(form));
				sent = game::UnequipItemForm(form, false);
			}
			say(sent ? "taken off - stowing it at" : "could NOT be taken off - nothing stowed at", name,
			    vr::SlotWords(h.offHandStow));
			if (!sent) {
				places[at] = 0;
			}
		}
		if (h.offHandTake != vr::HolsterSlot::None) {
			const UInt32 at = static_cast<UInt32>(h.offHandTake);
			UInt8* const form = game::HolsterSlotForm(places[at]);
			nameOf(form, name, sizeof(name));
			const bool sent = form != nullptr && game::EquipItemForm(form, false);
			say(sent ? "equipped - taking it back from" : "could NOT be equipped - forgotten at", name,
			    vr::SlotWords(h.offHandTake));
			if (!sent) {
				places[at] = 0;
			}
		}
		if (h.offHandRefused != vr::HolsterSlot::None) {
			nameOf(game::HolsterSlotForm(places[static_cast<UInt32>(h.offHandRefused)]), name, sizeof(name));
			say("stays - a two-hander or a bow is drawn, it goes back first; at", name, vr::SlotWords(h.offHandRefused));
		}
		if (h.offHandDone != vr::HolsterSlot::None) {
			const UInt32 at = static_cast<UInt32>(h.offHandDone);
			nameOf(game::HolsterSlotForm(places[at]), name, sizeof(name));
			say(h.offHandDoneTaking ? "is equipped again, taken back from" : "is unequipped, stowed at", name,
			    vr::SlotWords(h.offHandDone));
			if (h.offHandDoneTaking) {
				places[at] = 0;
			}
		}
		if (h.offHandGaveUp != vr::HolsterSlot::None) {
			const UInt32 at = static_cast<UInt32>(h.offHandGaveUp);
			nameOf(game::HolsterSlotForm(places[at]), name, sizeof(name));
			say(h.offHandDoneTaking ? "was never equipped again (no longer in the pack?) - forgotten at"
			                        : "was never unequipped - forgotten at",
			    name, vr::SlotWords(h.offHandGaveUp));
			places[at] = 0;
		}
	}
	const bool unequipped = hand.fist.unequip && game::UnequipWeapon();
	if ((hand.fist.changed || hand.fist.gaveUp) && state.linesLeft > 0) {
		--state.linesLeft;
		if (hand.fist.gaveUp) {
			OBVR_LOG("Fist: the weapon never left the slot - fists given up");
		} else {
			OBVR_LOG("Fist: the weapon hand %s%s", hand.fist.closed ? "closed" : "opened",
			         hand.fist.unequip
			             ? (unequipped ? " - the sheathed weapon taken off for the fists"
			                           : " - the sheathed weapon could NOT be taken off")
			         : hand.fist.readyClick
			             ? (hand.fist.closed ? " - fists up" : " - fists down")
			             : " - nothing to ready");
		}
	}
	if (hand.fist.readyClick && !hand.fist.changed && state.linesLeft > 0) {
		--state.linesLeft;
		OBVR_LOG("Fist: the slot is empty - fists up");
	}
	// A grip that closed at no place: where the hand was and how far from
	// the nearest place, for a reach that found nothing (Nadi,
	// left-handed, 2026-10-20: "waffe ziehen mit links nicht" - his log
	// had no Holster line at all).
	if ((h.rightMissed || h.leftMissed) && state.missLinesLeft > 0) {
		--state.missLinesLeft;
		OBVR_LOG("Holster: the %s grip closed at %.2f right, %.2f forward, %.2f up from the head - at no place: the "
		         "nearest, %s, is %.2f m off; left-handed %d",
		         h.rightMissed ? "weapon hand's" : "other hand's", static_cast<double>(h.missedAt.x),
		         static_cast<double>(h.missedAt.y), static_cast<double>(h.missedAt.z), vr::SlotWords(h.missedNearest),
		         static_cast<double>(h.missedOff), rolesSwapped ? 1 : 0);
	}
	if (h.gesture != vr::HolsterSlot::None && h.outcome != vr::SlotOutcome::None && state.linesLeft > 0) {
		--state.linesLeft;
		// The kind, by what the reach was about: the item kept there, or
		// the one in the hand.
		const vr::SlotItem kept = frame.slotItem[static_cast<UInt32>(h.gesture)];
		const vr::SlotItem carried =
			h.gestureLeft ? (frame.equipped == vr::EquippedKind::Bow && frame.weaponSeen == vr::WeaponSeen::Drawn
			                     ? vr::SlotItem::Bow
			                     : vr::SlotItemOf(frame.offHand))
			              : (frame.weaponSeen == vr::WeaponSeen::Drawn ? vr::SlotItemOf(frame.equipped)
			                                                           : vr::SlotItem::Nothing);
		const vr::SlotItem about = carried != vr::SlotItem::Nothing ? carried
		                           : kept != vr::SlotItem::Nothing  ? kept
		                                                            : vr::SlotItemOf(frame.equipped);
		const char* const what = about == vr::SlotItem::OneHand   ? "the one-handed weapon"
		                         : about == vr::SlotItem::TwoHand ? "the two-handed weapon"
		                         : about == vr::SlotItem::Bow     ? "the bow"
		                         : about == vr::SlotItem::Shield  ? "the shield"
		                         : about == vr::SlotItem::Torch   ? "the torch"
		                                                          : "nothing";
		const char* const did = h.outcome == vr::SlotOutcome::Draw        ? "drawing"
		                        : h.outcome == vr::SlotOutcome::EquipDraw ? "equipping the one kept there, then drawing"
		                        : h.outcome == vr::SlotOutcome::Sheathe   ? "sheathing"
		                        : h.outcome == vr::SlotOutcome::OtherDrawn
		                            ? "another weapon is drawn, it goes back first"
		                            : vr::SlotOutcomeWords(h.outcome);
		nameOf(game::HolsterSlotForm(places[static_cast<UInt32>(h.gesture)]), name, sizeof(name));
		OBVR_LOG("Holster: reach for %s - %s (the %s at %s, which keeps %s)", what, did,
		         h.gestureLeft ? "other hand" : "weapon hand", vr::SlotWords(h.gesture),
		         places[static_cast<UInt32>(h.gesture)] != 0 ? name : "nothing");
	}
	if (h.gaveUp && h.gaveUpAt == vr::HolsterSlot::None && state.linesLeft > 0) {
		--state.linesLeft;
		OBVR_LOG("Holster: the weapon equipped never showed in the hand - draw given up");
	}
}

void StepHolsterSign(HolsterState& state, const HandStep& step, const HandShared& shared,
                     const vr::HandModeFrame& frame, float deltaSeconds) {
	const Config& config = step.config;
	vr::OpenVRBackend& backend = step.backend;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	const vr::HandModeResult& hand = shared.hand;
	const vr::HolsterVerdict& hv = hand.holster;
	const UInt32 (&places)[vr::kHolsterSlotCount] = game::HolsterSlotIds();
	const bool bowOut = frame.equipped == vr::EquippedKind::Bow && frame.weaponSeen == vr::WeaponSeen::Drawn;
	const auto carriedBy = [&](bool otherHand) {
		if (otherHand) {
			return bowOut ? vr::SlotItem::Bow : vr::SlotItemOf(frame.offHand);
		}
		const vr::SlotItem drawnItem = vr::SlotItemOf(frame.equipped);
		return frame.weaponSeen == vr::WeaponSeen::Drawn && vr::WeaponHandItem(drawnItem) ? drawnItem
		                                                                                   : vr::SlotItem::Nothing;
	};
	if (hv.gesture != vr::HolsterSlot::None && hv.outcome != vr::SlotOutcome::None) {
		const char* const hint = vr::SlotHint(hv.outcome, frame.slotItem[static_cast<UInt32>(hv.gesture)],
		                                      carriedBy(hv.gestureLeft));
		if (hint != nullptr) {
			state.hintSlot = hv.gesture;
			state.hintLeft = hv.gestureLeft;
			state.hintSeconds = kHolsterHintSeconds;
			std::snprintf(state.hint, sizeof(state.hint), "%s", hint);
		}
	}
	state.hintSeconds -= deltaSeconds == deltaSeconds && deltaSeconds > 0.0f ? deltaSeconds : 0.0f;
	// The hand the hint is for while it stays at that place; else the
	// weapon hand's place, else the other's.
	const vr::HolsterSlot hintHandAt = state.hintLeft ? hv.leftSlot : hv.rightSlot;
	const bool hintShown = state.hintSeconds > 0.0f && hintHandAt == state.hintSlot;
	// Which hand's place, after a dwell, one hand owning it, faded
	// (vr/HolsterSign.h).
	vr::SignTimingInput timing;
	timing.rightSlot = hv.rightSlot;
	timing.leftSlot = hv.leftSlot;
	timing.hint = hintShown;
	timing.hintSlot = state.hintSlot;
	timing.hintLeft = state.hintLeft;
	timing.allowed = config.hands.holster.enabled && config.hands.holster.labels && active && !menuIsUp &&
	                 frame.inWorld && frame.headValid;
	timing.seconds = deltaSeconds;
	const vr::SignTimingOutput sign = vr::StepSignTiming(state.signTiming, timing);
	const bool otherHand = sign.left;
	const vr::HolsterSlot slot = sign.slot;
	// What the sign would say: the kept thing's name; at an empty place
	// "Holster weapon" (or bow, shield, torch) when the hand carries what
	// the place would take, else nothing - an empty place with nothing to
	// put there shows no sign (Nadi, 2026-10-28; vr/Holster.h
	// EmptyPlaceSign, HolsterSignWanted).
	ui::HolsterSignView view;
	bool showSign = false;
	if (sign.visible) {
		const UInt32 at = static_cast<UInt32>(slot);
		UInt8* const kept = game::HolsterSlotForm(places[at]);
		const vr::SlotItem carried = carriedBy(otherHand);
		const bool keptIsCarried =
			otherHand && !bowOut ? frame.slotWorn[at]
			                     : slot == frame.equippedSlot && carried != vr::SlotItem::Nothing;
		const vr::SlotOutcome before =
			vr::SignOutcomeFor(otherHand, slot, carried, frame.slotItem[at], keptIsCarried);
		const char* const emptyText = kept == nullptr ? vr::EmptyPlaceSign(carried, before) : nullptr;
		if (kept != nullptr) {
			game::ReadFormFullName(reinterpret_cast<UInt32>(kept), view.item, sizeof(view.item));
		} else if (emptyText != nullptr) {
			std::snprintf(view.item, sizeof(view.item), "%s", emptyText);
		}
		const char* hint = hintShown ? state.hint : vr::SlotHint(before, frame.slotItem[at], carried);
		if (hint != nullptr) {
			std::snprintf(view.hint, sizeof(view.hint), "%s", hint);
		}
		showSign = vr::HolsterSignWanted(kept != nullptr, hint, emptyText);
	}
	if (showSign) {
		if (std::strcmp(view.item, state.signView.item) != 0 ||
		    std::strcmp(view.hint, state.signView.hint) != 0) {
			state.signView = view;
			++state.signRevision;
		}
		const NiPoint3 target = vr::SignPlaceInView(frame.head, frame.headPosition, slot);
		state.signAt = vr::FollowSign(state.signAt, target, deltaSeconds, !state.signLayer.IsVisible());
		if (state.signLayer.Show(backend, render::GetGameDevice(),
		                            hands::StowSpotPose(state.signAt, frame.headPosition), ui::kHolsterSignMetres,
		                            state.signRevision, ui::PaintHolsterSignFor, &state.signView)) {
			state.signLayer.SetAlpha(backend, sign.alpha);
		}
		if (state.signSaidRevision != state.signRevision && state.signLines > 0) {
			--state.signLines;
			state.signSaidRevision = state.signRevision;
			OBVR_LOG("Holster sign: at %s - \"%s\"%s%s%s", vr::SlotWords(slot), state.signView.item,
			         state.signView.hint[0] != '\0' ? " / \"" : "", state.signView.hint,
			         state.signView.hint[0] != '\0' ? "\"" : "");
		}
	} else {
		state.signLayer.Hide(backend);
	}
}

void StepHolsterReaches(HolsterState& state, const HandStep& step, const HandShared& shared,
                        const vr::HandModeFrame& frame, UInt8* equippedWeaponForm, bool rolesSwapped,
                        InventoryHold& hold) {
	StepHolsterReach(state, shared, frame, equippedWeaponForm, rolesSwapped);
	StepPotionBelt(state.potionBelt, step, shared, frame, hold);
}

bool UpdateHolsterFit(HolsterState& state, Config& config, vr::OpenVRBackend& backend,
                      const vr::HandModeFrame& frame, bool allowed) {
	UpdateHolsterPlacing(state, config, backend, frame);
	vr::HolsterFitInput in;
	if (test::TakeHandScriptAction("holster_fit")) {
		state.fitRequested = true;
	}
	// Asked for by a hand script or the old panel menu (the native settings'
	// row opens the circles since 2026-11-09): it starts once the menu has
	// closed and the player stands in the world.
	in.start = state.fitRequested && allowed;
	state.fitRequested = state.fitRequested && !in.start;
	in.weaponValid = frame.right.valid && frame.headValid;
	in.otherValid = frame.left.valid && frame.headValid;
	if (frame.headValid) {
		in.weaponRelative = vr::BodyRelative(frame.head, frame.headPosition, frame.right.position);
		in.otherRelative = vr::BodyRelative(frame.head, frame.headPosition, frame.left.position);
	}
	in.weaponTrigger = frame.right.valid && frame.right.trigger > 0.5f;
	in.otherTrigger = frame.left.valid && frame.left.trigger > 0.5f;
	const bool leftB = frame.left.valid && vr::ButtonBDown(frame.left.buttonsPressed);
	const bool rightB = frame.right.valid && vr::ButtonBDown(frame.right.buttonsPressed);
	in.cancel = (leftB && !state.fitWasLeftB) || (rightB && !state.fitWasRightB) || !allowed;
	state.fitWasLeftB = leftB;
	state.fitWasRightB = rightB;
	in.leftHanded = config.hands.leftHanded;
	const vr::HolsterFitVerdict v = vr::StepHolsterFit(state.fit, in);

	if (v.stepChanged) {
		++state.fitRevision;
		state.fitText = ui::GuidePanelText{};
		state.fitText.title = "Weapon places";
		if (v.step == vr::HolsterFitStep::OneHand) {
			state.fitText.lines[0] = "1. One-handed weapons: hold the weapon hand";
			state.fitText.lines[1] = "   where they hang, pull its trigger.";
			state.fitText.lines[3] = "A menu button cancels.";
			vr::openvr::HmdMatrix34 head{};
			if (backend.GetRenderPoseMatrix(head)) {
				vr::LevelPose(head);
				state.fitPose = vr::OverlayPoseAhead(head, 0.8f);
			}
			OBVR_LOG("Holster fit: started - the one-handed place first");
		} else if (v.step == vr::HolsterFitStep::TwoHand) {
			state.fitText.lines[0] = "2. Two-handed weapons and staffs: the weapon";
			state.fitText.lines[1] = "   hand where they sit, pull its trigger.";
			state.fitText.lines[3] = "A menu button cancels.";
			OBVR_LOG("Holster fit: the one-handed place taken - now the two-handed one");
		} else if (v.step == vr::HolsterFitStep::Bow) {
			state.fitText.lines[0] = "3. Your bow: hold the other hand";
			state.fitText.lines[1] = "   where it should be, pull its trigger.";
			state.fitText.lines[3] = "A menu button cancels.";
			OBVR_LOG("Holster fit: the two-handed place taken - now the bow's");
		}
	}
	if (v.cancelled) {
		OBVR_LOG("Holster fit: cancelled - the places stay as they were");
	}
	if (v.finished) {
		vr::HolsterSettings& h = config.hands.holster;
		h.oneHandZone = v.oneHand;
		h.twoHandZone = v.twoHand;
		h.bowZone = v.bow;
		// A chosen side follows the hand: the place goes on the side it was
		// fitted on (vr::FittedSide); auto stays auto.
		const bool leftHanded = config.hands.leftHanded;
		h.oneHandSide = vr::FittedSide(h.oneHandSide, v.oneHand.x, leftHanded);
		h.twoHandSide = vr::FittedSide(h.twoHandSide, v.twoHand.x, leftHanded);
		h.bowSide = vr::FittedSide(h.bowSide, v.bow.x, leftHanded);
		const struct {
			const char* key;
			vr::HolsterSide side;
		} sides[] = {{"HolsterOneHandSide", h.oneHandSide},
		             {"HolsterTwoHandSide", h.twoHandSide},
		             {"HolsterBowSide", h.bowSide}};
		struct Entry {
			const char* key;
			float value;
		};
		const Entry entries[] = {
			{"HolsterOneHandX", v.oneHand.x}, {"HolsterOneHandForward", v.oneHand.y},
			{"HolsterOneHandUp", v.oneHand.z}, {"HolsterTwoHandX", v.twoHand.x},
			{"HolsterTwoHandForward", v.twoHand.y}, {"HolsterTwoHandUp", v.twoHand.z},
			{"HolsterBowX", v.bow.x}, {"HolsterBowForward", v.bow.y},
			{"HolsterBowUp", v.bow.z},
		};
		bool saved = true;
		for (const Entry& entry : entries) {
			char value[32];
			std::snprintf(value, sizeof(value), "%.2f", static_cast<double>(entry.value));
			saved = SaveSetting("Hands", entry.key, value) && saved;
		}
		for (const auto& entry : sides) {
			saved = SaveSetting("Hands", entry.key, vr::kHolsterSideNames[static_cast<UInt32>(entry.side)]) && saved;
		}
		OBVR_LOG("Holster fit: done - one-handed at %.2f %.2f %.2f, two-handed at %.2f %.2f %.2f, "
		         "bow at %.2f %.2f %.2f (right, forward, up from the eyes)%s",
		         static_cast<double>(v.oneHand.x), static_cast<double>(v.oneHand.y),
		         static_cast<double>(v.oneHand.z), static_cast<double>(v.twoHand.x),
		         static_cast<double>(v.twoHand.y), static_cast<double>(v.twoHand.z),
		         static_cast<double>(v.bow.x), static_cast<double>(v.bow.y),
		         static_cast<double>(v.bow.z), saved ? "" : " - COULD NOT SAVE the INI");
	}
	if (v.active) {
		state.fitLayer.Show(backend, render::GetGameDevice(), state.fitPose, 0.6f,
		                       state.fitRevision, ui::PaintGuidePanelFor, &state.fitText);
		if (test::HandScriptMarkedThisFrame()) {
			char name[96];
			char path[512];
			std::snprintf(name, sizeof(name), "OBVR-HolsterFit-%s.bmp", test::HandScriptMarkName());
			if (platform::BuildGamePath(name, path, sizeof(path)) && state.fitLayer.SaveBmp(path)) {
				OBVR_LOG("HandScript: the weapon places panel saved as %s", name);
			}
		}
	} else {
		state.fitLayer.Hide(backend);
	}
	return v.active || v.finished || v.cancelled;
}

namespace {

// The places Keep keeps, into the INI: the hips (HolsterOneHand*), the
// back's right (HolsterTwoHand*) and its left (HolsterBow*). False when the
// INI could not be written.
bool SaveHolsterPlaces(const vr::HolsterSettings& h) {
	struct Entry {
		const char* key;
		float value;
	};
	const Entry entries[] = {
		{"HolsterOneHandX", h.oneHandZone.x},  {"HolsterOneHandForward", h.oneHandZone.y},
		{"HolsterOneHandUp", h.oneHandZone.z}, {"HolsterOneHandRadius", h.oneHandRadius},
		{"HolsterTwoHandX", h.twoHandZone.x},  {"HolsterTwoHandForward", h.twoHandZone.y},
		{"HolsterTwoHandUp", h.twoHandZone.z}, {"HolsterTwoHandRadius", h.twoHandRadius},
		{"HolsterBowX", h.bowZone.x},          {"HolsterBowForward", h.bowZone.y},
		{"HolsterBowUp", h.bowZone.z},         {"HolsterBowRadius", h.bowRadius},
	};
	bool saved = true;
	for (const Entry& entry : entries) {
		char value[32];
		std::snprintf(value, sizeof(value), "%.2f", static_cast<double>(entry.value));
		saved = SaveSetting("Hands", entry.key, value) && saved;
	}
	return saved;
}

void LogPlaces(const char* what, const vr::HolsterPlaces& p, const char* tail) {
	OBVR_LOG("Holster place: %s - right hip at %.2f %.2f %.2f, left hip at %.2f %.2f %.2f (%.2f wide), the back's "
	         "left at %.2f %.2f %.2f (%.2f), its right at %.2f %.2f %.2f (%.2f) - right, forward, up from the eyes%s",
	         what, static_cast<double>(p.at[1].x), static_cast<double>(p.at[1].y), static_cast<double>(p.at[1].z),
	         static_cast<double>(p.at[0].x), static_cast<double>(p.at[0].y), static_cast<double>(p.at[0].z),
	         static_cast<double>(2.0f * p.radius[1]), static_cast<double>(p.at[2].x), static_cast<double>(p.at[2].y),
	         static_cast<double>(p.at[2].z), static_cast<double>(2.0f * p.radius[2]), static_cast<double>(p.at[3].x),
	         static_cast<double>(p.at[3].y), static_cast<double>(p.at[3].z), static_cast<double>(2.0f * p.radius[3]),
	         tail);
}

}  // namespace

void UpdateHolsterPlacing(HolsterState& state, Config& config, vr::OpenVRBackend& backend,
                          const vr::HandModeFrame& frame) {
	vr::HolsterPlaceInput in;
	in.command = static_cast<vr::HolsterPlaceCommand>(game::TakeHolsterPlaceCommand());
	in.rightValid = frame.right.valid && frame.headValid;
	in.leftValid = frame.left.valid && frame.headValid;
	if (frame.headValid) {
		in.right = vr::BodyRelative(frame.head, frame.headPosition, frame.right.position);
		in.left = vr::BodyRelative(frame.head, frame.headPosition, frame.left.position);
	}
	in.rightGrip = frame.right.valid && vr::GripDown(frame.right.buttonsPressed);
	in.leftGrip = frame.left.valid && vr::GripDown(frame.left.buttonsPressed);
	// The right stick up and down sizes the circle held, past the dead zone.
	if (frame.right.valid) {
		const float y = frame.right.thumbY;
		const float dead = config.hands.stickDeadZone;
		in.resize = y > dead ? (y - dead) / (1.0f - dead) : (y < -dead ? (y + dead) / (1.0f - dead) : 0.0f);
	}
	in.dtSeconds = frame.dtSeconds;
	if (in.command == vr::HolsterPlaceCommand::Reset) {
		float eyes = 0.0f;
		const bool known = backend.ReadStandingEyeHeight(eyes);
		in.reset = vr::ResetPlaces(vr::PlaceScaleFor(known, eyes));
	}
	vr::HolsterSettings& h = config.hands.holster;
	const vr::HolsterPlaceVerdict v = vr::StepHolsterPlace(state.place, in, h);
	game::NoteHolsterPlaceActive(v.active);
	if (in.command == vr::HolsterPlaceCommand::Start && v.active) {
		LogPlaces("the circles up", v.places, "");
	}
	if (v.grabbed || v.dropped) {
		const NiPoint3& at = v.places.at[v.circle];
		OBVR_LOG("Holster place: the %s circle %s at %.2f right, %.2f forward, %.2f up",
		         vr::kHolsterPlaceNames[v.circle], v.grabbed ? "taken" : "let go", static_cast<double>(at.x),
		         static_cast<double>(at.y), static_cast<double>(at.z));
	}
	if (v.reset) {
		LogPlaces("reset to the T-pose's places", v.places, "");
	}
	// The running settings follow the circles, so the places are where the
	// circles are drawn - and Cancel puts them back.
	if (v.active || v.save || v.restore) {
		vr::ApplyPlaces(h, v.places);
	}
	if (v.save) {
		const bool saved = SaveHolsterPlaces(h);
		LogPlaces("kept", v.places, saved ? " - the circles hidden" : " - COULD NOT SAVE the INI");
	} else if (v.restore) {
		LogPlaces("cancelled, back", v.places, "");
	}
	if (!v.active || !frame.headValid) {
		for (ui::CanvasOverlay& layer : state.placeLayers) {
			layer.Hide(backend);
		}
		return;
	}
	bool repaint = false;
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		ui::StowSpotView& view = state.placeViews[i];
		repaint = repaint || view.lit != v.lit[i] || view.name != vr::kHolsterPlaceNames[i];
		view.lit = v.lit[i];
		view.name = vr::kHolsterPlaceNames[i];
	}
	if (repaint) {
		++state.placeRevision;
	}
	for (UInt32 i = 0; i < vr::kHolsterPlaceCount; ++i) {
		const NiPoint3 at = vr::BodyToTracking(frame.head, frame.headPosition, v.places.at[i]);
		state.placeLayers[i].Show(backend, render::GetGameDevice(), StowSpotPose(at, frame.headPosition),
		                          2.0f * v.places.radius[i], state.placeRevision, ui::PaintStowSpotFor,
		                          &state.placeViews[i]);
	}
	if (test::HandScriptMarkedThisFrame()) {
		char name[96];
		char path[512];
		std::snprintf(name, sizeof(name), "OBVR-HolsterPlace-%s.bmp", test::HandScriptMarkName());
		if (platform::BuildGamePath(name, path, sizeof(path)) && state.placeLayers[1].SaveBmp(path)) {
			OBVR_LOG("HandScript: the right hip's circle saved as %s", name);
		}
	}
}

}  // namespace obvr::camera::hands
