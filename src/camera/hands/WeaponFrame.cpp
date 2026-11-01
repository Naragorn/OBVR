#include "camera/hands/WeaponFrame.h"

#include <cstdio>

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/HolsterSlotRecord.h"
#include "game/HolsterSlots.h"
#include "game/MeleeHits.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/PlayerAim.h"
#include "game/QuickKeys.h"
#include "game/TorchFire.h"
#include "game/TorchKeep.h"
#include "vr/Holster.h"
#include "vr/OneHandTwoHander.h"
#include "vr/WeaponWeight.h"

namespace obvr::camera::hands {

void StepWeaponFrame(WeaponFrameState& state, const HandStep& step, vr::HandModeFrame& frame,
                     const WeaponFrameLinks& links) {
	const Config& config = step.config;
	const bool active = step.active;
	const bool menuIsUp = step.menuIsUp;
	// The weapon slot, and the last weapon of each kind seen in it: what a
	// reach to the hip or a shoulder draws when another kind is in hand. In
	// menus too, so a weapon picked in the inventory counts; only with the
	// player in the world.
	{
		SInt32 weaponType = 0;
		UInt8* const weaponForm = frame.inWorld ? game::EquippedWeaponForm(&weaponType) : nullptr;
		frame.equipped =
			weaponForm == nullptr ? vr::EquippedKind::Nothing : vr::KindOfWeaponType(weaponType);
		state.equippedForBody = frame.equipped;
		state.equippedWeaponForm = frame.equipped != vr::EquippedKind::Nothing ? weaponForm : nullptr;
		// A weapon equipped in the inventory and kept at no place goes to its
		// kind's place on its side, or another free one (vr::PlaceForUnkept).
		UInt32 (&places)[vr::kHolsterSlotCount] = game::HolsterSlotIds();
		const UInt32 weaponId = game::FormIdOf(state.equippedWeaponForm);
		if (frame.inWorld && weaponId != 0 && game::SlotKeeping(places, weaponId) == vr::HolsterSlot::None) {
			vr::SlotItem kept[vr::kHolsterSlotCount] = {};
			for (UInt32 i = 1; i < vr::kHolsterSlotCount; ++i) {
				kept[i] = game::SlotItemOfForm(game::HolsterSlotForm(places[i]));
			}
			const vr::SlotItem item = vr::SlotItemOf(frame.equipped);
			const vr::HolsterSlot home = vr::PlaceForUnkept(
				item, vr::DefaultSlotFor(item, config.hands.holster, config.hands.leftHanded), kept);
			if (home != vr::HolsterSlot::None) {
				game::KeepAtSlot(places, home, weaponId);
				if (links.holster.linesLeft > 0) {
					--links.holster.linesLeft;
					char name[64] = "?";
					game::ReadFormFullName(reinterpret_cast<UInt32>(state.equippedWeaponForm), name, sizeof(name));
					OBVR_LOG("Holster: the %s, equipped and kept at no place, is kept at %s", name, vr::SlotWords(home));
				}
			}
		}
		frame.equippedSlot = game::SlotKeeping(places, weaponId);
		// A torch may join a drawn two-handed blade or blunt (TorchKeep), not a
		// staff - so the holster gives one back with the claymore out.
		frame.torchBesideTwoHander = config.hands.torchWithTwoHander && game::TorchKeepInstalled() &&
		                             frame.equipped == vr::EquippedKind::TwoHand && !vr::IsStaffWeaponType(weaponType);
		// The drawn weapon's weight, for its lag (vr/WeaponWeight.h): a swung
		// weapon's own - not the fists', which have no form, not a staff's,
		// which is not swung by its weight, not a bow's, which is aimed.
		// Logged once per draw, with the lag it makes at the slider's setting.
		// Exhausted, the lag is made heavier (StepExhaustion above).
		frame.exhaustionEase = links.exhaustionEase;
		frame.exhaustionFactor = links.exhaustionFactor;
		float weight = 0.0f;
		const bool weighed = frame.meleeHeld && weaponForm != nullptr && !vr::IsStaffWeaponType(weaponType) &&
		                     game::WeaponWeightOf(weaponForm, &weight);
		// A two-hander that is swung, not a staff (vr/OneHandTwoHander.h).
		frame.twoHanderSwung = weighed && vr::IsSwungTwoHanderType(weaponType);
		if (weighed) {
			frame.weaponWeight = weight;
			if (weaponForm != state.weighed) {
				state.weighed = weaponForm;
				char name[64] = "";
				game::ReadFormFullName(reinterpret_cast<UInt32>(weaponForm), name, sizeof(name));
				const vr::WeaponLagTuning lag = vr::WeaponLagFor(weight, config.hands.weaponWeightPercent, false,
				                                                 config.hands.weaponSwingThrough);
				// A two-hander's weight in one hand and in both (vr/OneHandTwoHander.h).
				char twoHander[160] = "";
				if (frame.twoHanderSwung) {
					const vr::WeaponLagTuning one = vr::WeaponLagHeavier(
						lag, vr::PercentMore(config.hands.oneHandTwoHander.weightPercent));
					const vr::WeaponLagTuning both = vr::WeaponLagFor(weight, config.hands.weaponWeightPercent, true,
					                                                  config.hands.weaponSwingThrough);
					std::snprintf(twoHander, sizeof(twoHander),
					              "; a two-hander: in one hand %.0f ms, cap %.2f m / %.0f degrees, in both %.0f ms",
					              static_cast<double>(one.timeConstantSeconds * 1000.0f),
					              static_cast<double>(one.capMetres),
					              static_cast<double>(one.capRadians * math::kRadiansToDegrees),
					              static_cast<double>(both.timeConstantSeconds * 1000.0f));
				}
				OBVR_LOG("Weapon weight: %s %.1f, time constant %.0f ms at %.0f %%, cap %.2f m / %.0f degrees, %s%s",
				         name, static_cast<double>(weight), static_cast<double>(lag.timeConstantSeconds * 1000.0f),
				         static_cast<double>(config.hands.weaponWeightPercent), static_cast<double>(lag.capMetres),
				         static_cast<double>(lag.capRadians * math::kRadiansToDegrees),
				         lag.swingThrough ? "swinging through" : "the plain lag", twoHander);
			}
		} else {
			state.weighed = nullptr;
		}
	}
	// The other hand's shield or torch, worn (the shield first should the
	// engine ever let both be worn), and where one is stowed. A change is
	// logged with both readings, so the log shows what the engine lets the
	// other hand carry together.
	{
		UInt8* const shield = frame.inWorld ? game::WornShieldForm() : nullptr;
		UInt8* const torch = frame.inWorld ? game::WornTorchForm() : nullptr;
		// A torch newly equipped - in the inventory, by a hotkey, from a place
		// or the world - for the torch's grace (game/TorchRelease.h): not the
		// one a load brings, which was the player's before.
		{
			if (game::LoadingThreadActive() || (menuIsUp && game::TopVisibleMenu() == game::kMenuIdLoading)) {
				state.loadGap = true;
			} else if (frame.inWorld) {
				if (!state.loadGap && torch != nullptr && torch != state.torchSeen) {
					state.torchJustEquipped = true;
				}
				state.loadGap = false;
				state.torchSeen = torch;
			}
		}
		frame.offHand = shield != nullptr  ? vr::OffHandItem::Shield
		                : torch != nullptr ? vr::OffHandItem::Torch
		                                   : vr::OffHandItem::Nothing;
		const UInt32 (&places)[vr::kHolsterSlotCount] = game::HolsterSlotIds();
		for (UInt32 i = 1; i < vr::kHolsterSlotCount; ++i) {
			UInt8* const form = frame.inWorld ? game::HolsterSlotForm(places[i]) : nullptr;
			frame.slotItem[i] = game::SlotItemOfForm(form);
			frame.slotWorn[i] = form != nullptr && (vr::WeaponHandItem(frame.slotItem[i]) || frame.slotItem[i] == vr::SlotItem::Bow
			                                            ? form == state.equippedWeaponForm
			                                            : game::PlayerWearsItem(form));
		}
		if (frame.inWorld && (shield != state.shieldWas || torch != state.torchWas)) {
			state.shieldWas = shield;
			state.torchWas = torch;
			if (active && links.holster.offHandLinesLeft > 0) {
				--links.holster.offHandLinesLeft;
				char shieldName[64] = "none";
				char torchName[64] = "none";
				if (shield != nullptr) {
					game::ReadFormFullName(reinterpret_cast<UInt32>(shield), shieldName, sizeof(shieldName));
				}
				if (torch != nullptr) {
					game::ReadFormFullName(reinterpret_cast<UInt32>(torch), torchName, sizeof(torchName));
				}
				OBVR_LOG("Hands: the other hand carries - shield %s, torch %s", shieldName, torchName);
			}
			state.torchWatch = torch != nullptr ? 4 : 0;
		}
		// A torch come into the hand: its first-person model's shapes and
		// their render state, the frame it came and three after
		// (game/FlameStateLogic.h).
		if (state.torchWatch > 0 && active) {
			--state.torchWatch;
			game::FlameShapeStates states;
			game::ReadFirstPersonTorchShapeStates(states);
			char what[64];
			std::snprintf(what, sizeof(what), "in the hand, %u frame(s) after it came, the shapes of",
			              3 - state.torchWatch);
			links.logTorchShapeStates(what, reinterpret_cast<UInt32>(torch), states);
		}
	}
}

}  // namespace obvr::camera::hands
