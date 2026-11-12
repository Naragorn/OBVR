#include "camera/hands/SlapTap.h"

#include <cstring>

#include "core/Log.h"
#include "core/MathFns.h"
#include "game/CrosshairTarget.h"
#include "game/GameAddresses.h"
#include "game/InputRoute.h"
#include "game/Lead.h"
#include "game/PlayerAim.h"
#include "game/Shove.h"
#include "game/ShoveLogic.h"
#include "core/AddressSpace.h"
#include "platform/Win32Min.h"

namespace obvr::camera::hands {

namespace {

// The frames the log watches the mod's state after a tap, the tap's own
// frames included.
constexpr UInt32 kSlapTapWatchFrames = 24;
constexpr UInt32 kMapScanToKey = 1;  // MAPVK_VSC_TO_VK
// Oblivion's heading: zero at north, clockwise; a reference's rotZ at +0x28
// and its position at +0x2C (xOBSE GameObjects.h, TESObjectREFR).
constexpr UInt32 kRefRotZOffset = 0x28;
// The mod's slap wants the two origins within 51 units (its script's
// `getDistance < 51`); a hand at a face from an arm's length stands the
// body further off (the tester's log, 2026-10-07: 69 and 45 units, the mod
// following only on the 45). For the tap's watch the slapped one's origin
// is set this close along the line to the player when it is further - the
// mod's own positioner then puts them 50 units before the player anyway,
// and the engine's movement writes their place back otherwise.
constexpr float kSlapModReachUnits = 45.0f;

}  // namespace

void StepSlapTap(SlapTapState& state, const HandStep& step, TurnBodyFn turnBody) {
	const Config& config = step.config;
	UInt32 slapped = 0;
	NiPoint3 centre{};
	if (game::TakeSlapGrabTap(slapped, centre)) {
		state.frames = game::kSlapGrabTapTotalFrames;
		state.target = centre;
		state.actor = slapped;
	}
	state.pressed = game::SlapGrabTapPressed(state.frames);
	if (state.frames > 0) {
		state.watch = kSlapTapWatchFrames;
		// The body turned to face them for the tap: the mod wants the
		// player's heading within 25 degrees of the slapped one ("I can't
		// slap %po from this side - I need to face %po"), and in Full VR
		// the body's heading is the walk's or the gaze's, not the hand's
		// (the tester, 2026-10-07: "das klappt in vr nicht so gut weil ich
		// das eig tue"). Oblivion's heading: zero at north, clockwise.
		NiPoint3 feet{};
		if (game::PlayerWorldPosition(feet)) {
			const float dx = state.target.x - feet.x;
			const float dy = state.target.y - feet.y;
			if (dx * dx + dy * dy > 1.0f) {
				turnBody(math::Atan2(dx, dy));
			}
		}
	}
	const bool watchLogged = state.watch > 0 && state.lines > 0;
	if (state.watch > 0) {
		--state.watch;
		// The slap's noise with the mod on (game/Shove.h): the mod's
		// own with its sequence, OBVR's once the watch is over without
		// it.
		game::StepSlapNoise(state.watch == 0);
	}
	if (watchLogged) {
		// Evidence for the mod's side (the tester, 2026-10-07: "slaps
		// gehen nicht mehr" with the tap logged, then "slap klappte gar
		// nicht" with the key down every frame): whom the pick has, the
		// key as the game's layout sees it (the scan code's key - on a
		// German layout the Z scan code is the Y key), and the mod's own
		// state from its quest variables (game/Shove.h) - its grab quest
		// running, its count of the grab's frames (sNPCGrab: 1 on the
		// down-edge with an NPC under the crosshair, then counted up while
		// held, 0 again once its tap-slap or pickpocket branch ran), and
		// whom it took (rGrabbedItem).
		--state.lines;
		const game::CrosshairTarget target = game::ReadCrosshairTarget();
		const UInt32 scan = game::UsScanCode(config.handKeys.grab);
		const UInt32 layoutKey = scan != 0 ? MapVirtualKeyA(scan, kMapScanToKey) : config.handKeys.grab;
		const UInt8 modIndex = game::PutItInItsPlaceIndex();
		double npcGrab = -1.0;
		double grabbing = -1.0;
		double grabbed = 0.0;
		double enabled = -1.0;
		double slapper = -1.0;
		game::ReadModQuestVar(modIndex, game::kPiiiPGrabQuestLow, game::kPiiiPVarNpcGrab, npcGrab);
		game::ReadModQuestVar(modIndex, game::kPiiiPGrabQuestLow, game::kPiiiPVarIsGrabbing, grabbing);
		game::ReadModQuestVar(modIndex, game::kPiiiPGrabQuestLow, game::kPiiiPVarGrabbedItem, grabbed);
		game::ReadModQuestVar(modIndex, game::kPiiiPVarsQuestLow, game::kPiiiPVarEnabled, enabled);
		game::ReadModQuestVar(modIndex, game::kPiiiPVarsQuestLow, game::kPiiiPVarSlapper, slapper);
		UInt32 grabbedId = 0;
		std::memcpy(&grabbedId, &grabbed, sizeof(grabbedId));
		OBVR_LOG("Shove: the grab tap, %u frame(s) left - the grab %s this frame, the key (layout %02X) down %d, the "
		         "crosshair on %08X (the slapped one %08X), the player in combat %d; the mod: grab quest %s, "
		         "sNPCGrab %.0f, sIsGrabbing %.0f, rGrabbedItem %08X, sEnabled %.0f, sSlapper %.0f",
		         state.frames, state.pressed ? "down" : "not yet", layoutKey,
		         (GetAsyncKeyState(static_cast<int>(layoutKey)) & 0x8000) != 0 ? 1 : 0,
		         target.haveRef ? target.refAddress : 0u, state.actor,
		         game::ActorInCombat(*reinterpret_cast<void* const*>(addr::kPlayerPointer)) ? 1 : 0,
		         game::ModQuestActive(modIndex, game::kPiiiPGrabQuestLow) ? "running" : "NOT running", npcGrab,
		         grabbing, grabbedId, enabled, slapper);
	}
	if (state.frames > 0) {
		--state.frames;
	}
}

void HoldSlapFacing(SlapTapState& state, TurnBodyFn turnBody) {
	if (state.watch == 0 || !mem::LooksLikeObjectAddress(state.actor)) {
		return;
	}
	NiPoint3 feet{};
	if (!game::PlayerWorldPosition(feet)) {
		return;
	}
	const float* const npcPos = reinterpret_cast<const float*>(state.actor + addr::kRefPositionOffset);
	const float dx = npcPos[0] - feet.x;
	const float dy = npcPos[1] - feet.y;
	const float distance = math::Sqrt(dx * dx + dy * dy);
	if (!(distance > 1.0f)) {
		return;
	}
	float* const npcYaw = reinterpret_cast<float*>(state.actor + kRefRotZOffset);
	if (state.watch == kSlapTapWatchFrames - 1 && state.facingLines > 0) {
		--state.facingLines;
		game::PlayerRotation player{};
		const bool haveYaw = game::ReadPlayerRotation(player);
		const float toNpc = math::Atan2(dx, dy);
		const float toPlayer = math::Atan2(-dx, -dy);
		OBVR_LOG("Shove: facing for the mod - the player %.0f deg off the slapped one, they %.0f deg off the player, "
		         "%.0f units apart (the mod wants 25, 18 and 51) - both turned to face for %u frames",
		         static_cast<double>(haveYaw ? math::WrapAngle(player.yaw - toNpc) * 57.2958f : 0.0f),
		         static_cast<double>(math::WrapAngle(*npcYaw - toPlayer) * 57.2958f), static_cast<double>(distance),
		         kSlapTapWatchFrames);
	}
	turnBody(math::Atan2(dx, dy));
	*npcYaw = math::Atan2(-dx, -dy);
	if (distance > kSlapModReachUnits + 1.0f) {
		float* const npcPosWrite = reinterpret_cast<float*>(state.actor + addr::kRefPositionOffset);
		const float share = kSlapModReachUnits / distance;
		npcPosWrite[0] = feet.x + dx * share;
		npcPosWrite[1] = feet.y + dy * share;
	}
}

}  // namespace obvr::camera::hands
