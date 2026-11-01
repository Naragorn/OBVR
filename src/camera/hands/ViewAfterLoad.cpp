#include "camera/hands/ViewAfterLoad.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/GameAddresses.h"
#include "game/MeleeHits.h"
#include "game/MenuPause.h"
#include "game/MenuType.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"

namespace obvr::camera::hands {

void StepViewAfterLoad(ViewAfterLoadState& state, const HandStep& step, const ViewAfterLoadLinks& links) {
	const Config& config = step.config;
	const float dt = step.dt;
	const bool menuIsUp = step.menuIsUp;
		const UInt16 sitMountBytes = game::PlayerSitMountBytes();
		const UInt8 sitSleepState = game::PlayerSitSleepState();
		const SInt32 knockState = game::PlayerKnockState();
		FirstPersonAfterLoadInput in;
		in.enabled = config.hands.firstPersonAfterLoad;
		in.fullVr = true;
		in.loading = game::LoadingThreadActive() || (menuIsUp && game::TopVisibleMenu() == game::kMenuIdLoading);
		in.inWorld = game::PlayerInWorld();
		in.menuIsUp = menuIsUp;
		in.thirdPerson = links.thirdPerson();
		in.engineOwned = EngineOwnsThirdPerson(sitMountBytes, sitSleepState, knockState);
		in.dead = game::PlayerIsDead();
		// Said once per load, at the frame the view is read (the first in
		// the world without a menu): every input, for the run that found
		// no switch where one was due (load-third-person.txt, 13:51).
		{
			if (in.loading) {
				state.loadSaid = false;
			} else if (state.afterLoad.pending && in.inWorld && !in.menuIsUp && !state.loadSaid &&
			           state.afterLoadLines > 0) {
				state.loadSaid = true;
				--state.afterLoadLines;
				// The chargen byte for the record only: it read 1 on the
				// harness save (13:55), a character long past the chargen.
				OBVR_LOG("Hands: after the load - the view read: third person %d, the engine's own %d (sit/mount bytes "
				         "%04X, sit/sleep state %u, knock state %d), dead %d, the setting %d, the chargen byte %d "
				         "(frame %u)",
				         in.thirdPerson ? 1 : 0, in.engineOwned ? 1 : 0, sitMountBytes, sitSleepState, knockState,
				         in.dead ? 1 : 0, in.enabled ? 1 : 0, game::PlayerInChargen() ? 1 : 0, links.frameCount);
			}
		}
		if (StepFirstPersonAfterLoad(state.afterLoad, in)) {
			auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
			if (mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(player))) {
				using ToggleCameraFn = void(__fastcall*)(UInt8* self, void* edx, UInt8 firstPerson);
				reinterpret_cast<ToggleCameraFn>(addr::kToggleCamera)(player, nullptr, 1);
				if (state.afterLoadLines > 0) {
					--state.afterLoadLines;
					OBVR_LOG("Hands: the save was made in third person - put into first person once after the load "
					         "(sit/mount bytes %04X, sit/sleep state %u, knock state %d; the view now %s, frame %u)",
					         sitMountBytes, sitSleepState, knockState,
					         links.thirdPerson() ? "third person" : "first person", links.frameCount);
				}
			}
		}
		// A knockdown begun in first person ends in first person: the
		// engine's own falls switch back by themselves, a mod's can leave
		// the view in third person (Oblivion Combat Extender, 2026-10-29).
		FirstPersonAfterKnockdownInput knock;
		knock.enabled = config.hands.firstPersonAfterKnockdown;
		knock.fullVr = true;
		knock.knockState = knockState;
		knock.thirdPerson = in.thirdPerson;
		knock.engineOwned = EngineOwnsThirdPerson(sitMountBytes, sitSleepState, 0);
		knock.menuIsUp = menuIsUp;
		knock.dead = in.dead;
		knock.dtSeconds = dt;
		const bool wasDown = state.afterKnockdown.down;
		const bool switchBack = StepFirstPersonAfterKnockdown(state.afterKnockdown, knock);
		if (!wasDown && state.afterKnockdown.down && state.afterKnockdownLines > 0) {
			--state.afterKnockdownLines;
			OBVR_LOG("Hands: knocked down (knock state %d) - begun in %s, the view now %s (frame %u)", knockState,
			         state.afterKnockdown.beganFirstPerson ? "first person" : "third person",
			         in.thirdPerson ? "third person" : "first person", links.frameCount);
		} else if (wasDown && !state.afterKnockdown.down && !switchBack &&
		           state.afterKnockdownLines > 0) {
			--state.afterKnockdownLines;
			OBVR_LOG("Hands: up after the knockdown - left as it is, the view %s (dead %d, the setting %d; frame %u)",
			         in.thirdPerson ? "third person" : "first person", in.dead ? 1 : 0, knock.enabled ? 1 : 0,
			         links.frameCount);
		}
		if (switchBack) {
			auto* const player = *reinterpret_cast<UInt8* const*>(addr::kPlayerPointer);
			if (mem::LooksLikeObjectAddress(reinterpret_cast<UInt32>(player))) {
				using ToggleCameraFn = void(__fastcall*)(UInt8* self, void* edx, UInt8 firstPerson);
				reinterpret_cast<ToggleCameraFn>(addr::kToggleCamera)(player, nullptr, 1);
				if (state.afterKnockdownLines > 0) {
					--state.afterKnockdownLines;
					OBVR_LOG("Hands: up after the knockdown and still in third person %.1f s on - put into first "
					         "person (sit/mount bytes %04X, sit/sleep state %u; the view now %s, frame %u)",
					         static_cast<double>(kFirstPersonAfterKnockdownSeconds), sitMountBytes, sitSleepState,
					         links.thirdPerson() ? "third person" : "first person", links.frameCount);
				}
			}
		}
}

}  // namespace obvr::camera::hands
