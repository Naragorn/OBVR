#include "camera/hands/KeptOnBodyStep.h"

#include "core/Log.h"
#include "game/HolsterSlotRecord.h"
#include "game/HolsterSlots.h"
#include "game/KeptOnBody.h"
#include "test/HandScriptRuntime.h"
#include "vr/SheathMirror.h"

namespace obvr::camera::hands {

namespace {

// About a second at the headset's rate: a weapon sold or given away in a
// menu leaves the hip within it.
constexpr UInt32 kRecountFrames = 90;

struct KeptFrame {
	UInt32 countedIds[vr::kHolsterSlotCount] = {};
	UInt32 countedEquipped = 0;
	bool counted = false;
	UInt32 recountIn = 0;
	vr::SlotItem item[vr::kHolsterSlotCount] = {};
	bool inPack[vr::kHolsterSlotCount] = {};
	game::KeptShown said[game::kKeptHipCount] = {game::KeptShown::Off, game::KeptShown::Off};
	UInt32 lines = 40;
};

KeptFrame& Frame() {
	static KeptFrame s_frame;
	return s_frame;
}

// What the hips' places keep and whether the pack holds it, read again when
// the places or the slot change and every kRecountFrames.
void Recount(KeptFrame& f, const UInt32 (&ids)[vr::kHolsterSlotCount], UInt32 equipped) {
	bool same = f.counted && equipped == f.countedEquipped;
	for (UInt32 i = 0; i < vr::kHolsterSlotCount; ++i) {
		same = same && ids[i] == f.countedIds[i];
	}
	if (same && f.recountIn > 0) {
		--f.recountIn;
		return;
	}
	for (UInt32 i = 0; i < vr::kHolsterSlotCount; ++i) {
		const bool hip = vr::SlotIsHip(static_cast<vr::HolsterSlot>(i));
		UInt8* const form = hip && ids[i] != 0 ? game::HolsterSlotForm(ids[i]) : nullptr;
		f.item[i] = game::SlotItemOfForm(form);
		f.inPack[i] = form != nullptr && game::PlayerItemCount(form) > 0;
		f.countedIds[i] = ids[i];
	}
	f.countedEquipped = equipped;
	f.counted = true;
	f.recountIn = kRecountFrames;
}

}  // namespace

void StepKeptOnBodyFrame(const Config& config, const UInt8* equippedWeaponForm, vr::EquippedKind equippedForBody) {
	KeptFrame& f = Frame();
	const vr::HandSettings& hands = config.hands;
	game::KeptOnBodyInput in;
	in.enabled = config.fullVrMode && hands.enabled && hands.holster.enabled && hands.holster.showKept;
	const UInt32 (&ids)[vr::kHolsterSlotCount] = game::HolsterSlotIds();
	in.equippedWeapon = game::FormIdOf(equippedWeaponForm);
	if (in.enabled) {
		Recount(f, ids, in.equippedWeapon);
		for (UInt32 i = 0; i < vr::kHolsterSlotCount; ++i) {
			in.kept[i] = ids[i];
			in.item[i] = f.item[i];
			in.inPack[i] = f.inPack[i];
		}
		const vr::SheathMirror mirror =
			vr::SheathMirrorFor(hands.holster, hands.archery.quiverZone.x, hands.leftHanded, equippedForBody,
			                    game::SlotKeeping(ids, in.equippedWeapon));
		in.gameSheathHip = game::GameSheathHip(equippedForBody, mirror.side);
	} else {
		f.counted = false;
	}
	for (UInt32 hip = 0; hip < game::kKeptHipCount; ++hip) {
		const vr::HolsterSlot slot = game::KeptHipSlot(hip);
		const game::KeptShown verdict = game::KeptShownAt(in, slot);
		if (verdict != f.said[hip] && f.lines > 0) {
			--f.lines;
			OBVR_LOG("Kept on body: the %s hip - %08X %s", hip == 0 ? "left" : "right", in.kept[static_cast<UInt32>(slot)],
			         game::KeptShownName(verdict));
		}
		f.said[hip] = verdict;
	}
	game::StepKeptOnBody(game::KeptOnBodyFor(in));
	if (test::HandScriptMarkedThisFrame()) {
		game::LogKeptOnBody(test::HandScriptMarkName());
	}
}

}  // namespace obvr::camera::hands
