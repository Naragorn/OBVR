#include "camera/hands/Gestures.h"

#include "core/AddressSpace.h"
#include "core/Log.h"
#include "game/ActorRoster.h"
#include "game/CombatRing.h"
#include "game/CrosshairTarget.h"
#include "game/EngineInput.h"
#include "game/HandBones.h"
#include "game/Lead.h"
#include "game/MeleeHits.h"
#include "game/NearbyItems.h"
#include "game/PlayerAim.h"
#include "game/PlayerTeleport.h"
#include "test/HandScriptRuntime.h"
#include "vr/LaserGeometry.h"

namespace obvr::camera::hands {
namespace {

inline constexpr UInt32 kYieldHoldFrames = 30;
inline constexpr UInt32 kYieldActivateFrom = 20;   // frames left when activate goes down
inline constexpr UInt32 kYieldActivateTo = 14;     // ... and comes up
inline constexpr float kYieldReadySeconds = 3.0f;  // the fists given up on after this
inline constexpr float kYieldEnemyUnits = 420.0f;  // 6 m
inline constexpr float kYieldEnemyCos = 0.866f;    // 30 degrees from the head's forward

}  // namespace

void UpdateYield(GesturesState& state, const HandStep& step, const HandShared& shared,
                 const vr::HandModeFrame& frame, bool active) {
	const Config& config = step.config;
	const float dt = step.dt;
	vr::HandModeResult& hand = shared.hand;
	NiPoint3 enemy{};
	bool enemyAhead = false;
	void* ahead = nullptr;  // whoever is under the head ray, in combat or not
	if (active && shared.cyclopeanCameraValid) {
		const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
		const NiPoint3 forward{camRot.data[0][1], camRot.data[1][1], camRot.data[2][1]};
		ahead = game::ActorUnderRay(shared.cyclopeanCamera.pos, forward, kYieldEnemyUnits, kYieldEnemyCos,
		                            &enemy);
		enemyAhead = ahead != nullptr && game::ActorInCombat(ahead);
	}
	if (!active) {
		state.yield = vr::YieldState{};
		state.yieldShadow = vr::YieldState{};
		return;
	}
	// Each hand as the gesture reads it (vr/Yield.h): open by its middle,
	// ring and little fingers (vr::YieldHandCurl) - unless it grips a drawn
	// weapon (holdsWeapon: a blade, a blunt weapon or a staff in the weapon
	// hand, the bow in the bow hand; the tester, 2026-10-12: "egal ob mit
	// oder ohne waffe oder schild", and 2026-10-16: his sword hand's curl of
	// 1.00 stood in the way 37 times of 44); its place relative to the head
	// in metres - the camera-relative offset (x right, y ahead, z up), the x
	// outward from the body for either hand; its palm's share to the front
	// (vr::YieldPalmAhead, the controller's side the fingers close over - the
	// physical side, whichever role the hand plays), asked of the gripping
	// hand; and the hand mode's swing.
	const float perMetre = config.tracker.unitsPerMetre > 0.0f ? config.tracker.unitsPerMetre : 1.0f;
	SInt32 weaponType = -1;
	const bool weaponDrawn = game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
	const vr::EquippedKind drawnKind = weaponDrawn && game::EquippedWeaponForm(&weaponType) != nullptr
	                                       ? vr::KindOfWeaponType(weaponType)
	                                       : vr::EquippedKind::Nothing;
	const bool weaponHandGrips = drawnKind == vr::EquippedKind::OneHand || drawnKind == vr::EquippedKind::TwoHand;
	const bool bowHandGrips = drawnKind == vr::EquippedKind::Bow;
	vr::YieldInput in;
	in.dt = dt;
	in.enemyAhead = enemyAhead;
	for (int side = 0; side < 2; ++side) {
		const bool right = side == 0;  // the hand's role: the weapon hand first
		const bool physicalRight = right != config.hands.leftHanded;
		vr::YieldHandInput& h = in.hand[side];
		h.tracked = right ? (frame.right.valid && hand.rightHandValid) : (frame.left.valid && hand.leftHandValid);
		h.curl = right ? (hand.rightCurlValid ? vr::YieldHandCurl(hand.rightCurl) : 1.0f)
		               : (hand.leftCurlValid ? vr::YieldHandCurl(hand.leftCurl) : 1.0f);
		h.holdsWeapon = right ? weaponHandGrips : bowHandGrips;
		const NiPoint3& off = right ? hand.rightHandOffsetUnits : hand.leftHandOffsetUnits;
		h.position = NiPoint3{(right ? off.x : -off.x) / perMetre, off.y / perMetre, off.z / perMetre};
		h.swinging = right ? hand.swingActive : hand.leftSwingActive;
		h.palmAhead =
			vr::YieldPalmAhead(right ? hand.rightHandRotation : hand.leftHandRotation, physicalRight);
	}
	// The rocking watched on its own as well: when it completes while the
	// gesture did not count, the log says what stood in the way - the
	// tester's "yield ging nicht" came with no yield line at all, 2026-10-07.
	// Outside a fight only the first few: the tester's run of 2026-10-12
	// spent all twenty lines on rockings before his first fight, and the
	// fights after logged nothing at all.
	const bool rocked = vr::StepYield(state.yieldShadow, vr::YieldRockingAlone(in));
	const bool fired = vr::StepYield(state.yield, in);
	// Fighting: the player in combat, someone in combat before the head, or
	// the ring counting those fighting the player - the harness's bandit
	// attacked an untouchable player whose own combat state read "not
	// fighting" (2026-10-12).
	const bool fighting = game::PlayerInCombat() || enemyAhead || game::CombatRingFighting() > 0;
	UInt32& rockLines = fighting ? state.yieldRockLines : state.yieldRockQuietLines;
	if (rocked && !fired && rockLines > 0) {
		--rockLines;
		char why[240] = {};
		vr::YieldReasons(state.yield.lastReset | state.yield.waiting, why, sizeof(why));
		OBVR_LOG("Yield: the hands rocked, but no yield - %s (hand curls %.2f/%.2f, palms to the front %.2f/%.2f, "
		         "under the eyes %.2f/%.2f m; under the head: %08X, in combat %d; the player %s)",
		         why, static_cast<double>(in.hand[0].curl), static_cast<double>(in.hand[1].curl),
		         static_cast<double>(in.hand[0].palmAhead), static_cast<double>(in.hand[1].palmAhead),
		         static_cast<double>(-in.hand[0].position.z), static_cast<double>(-in.hand[1].position.z),
		         reinterpret_cast<UInt32>(ahead), ahead != nullptr && game::ActorInCombat(ahead) ? 1 : 0,
		         fighting ? "fighting" : "not fighting");
	}
	// The pose on its own, twice a second, for the headset's numbers
	// (2026-10-16, vr/Yield.h): with both hands shown - open, or gripping a
	// drawn weapon - and up before someone in combat, what each palm reads to
	// the front and which way the drawn fingers point (the pinned hand bone's
	// x in the world, its share up). What an open hand's controller reads for
	// the palm is what the tester's next session has to settle.
	{
		static float s_poseClock = 0.5f;
		static UInt32 s_poseLines = 60;
		bool posed = in.enemyAhead;
		for (const vr::YieldHandInput& h : in.hand) {
			posed = posed && h.tracked && (h.holdsWeapon || h.curl < vr::kYieldOpenCurl) &&
			        h.position.z > -vr::kYieldBelowHeadMetres && h.position.y > vr::kYieldAheadMetres;
		}
		if (!posed) {
			s_poseClock = 0.5f;  // the first line the moment the pose is taken
		} else if (s_poseLines > 0) {
			s_poseClock += dt > 0.0f && dt < 1.0f ? dt : 0.0f;
			if (s_poseClock >= 0.5f) {
				s_poseClock = 0.0f;
				--s_poseLines;
				float fingersUp[2] = {0.0f, 0.0f};
				for (int side = 0; side < 2; ++side) {
					NiMatrix33 rot;
					NiPoint3 pos;
					if (game::ReadHandBoneWorld(side == 0, rot, pos)) {
						fingersUp[side] = rot.data[2][0];  // the bone's x runs along the fingers
					}
				}
				OBVR_LOG("Yield: the pose - palms to the front %.2f/%.2f, the fingers' share up %.2f/%.2f, under the "
				         "eyes %.2f/%.2f m, ahead %.2f/%.2f m, curls %.2f/%.2f; the right hand %s, the left %s",
				         static_cast<double>(in.hand[0].palmAhead), static_cast<double>(in.hand[1].palmAhead),
				         static_cast<double>(fingersUp[0]), static_cast<double>(fingersUp[1]),
				         static_cast<double>(-in.hand[0].position.z), static_cast<double>(-in.hand[1].position.z),
				         static_cast<double>(in.hand[0].position.y), static_cast<double>(in.hand[1].position.y),
				         static_cast<double>(in.hand[0].curl), static_cast<double>(in.hand[1].curl),
				         in.hand[0].holdsWeapon ? "gripping a weapon" : "open",
				         in.hand[1].holdsWeapon ? "gripping the bow" : "open");
			}
		}
	}
	if (test::HandScriptMarkedThisFrame()) {
		char why[240] = {};
		vr::YieldReasons(state.yield.lastReset | state.yield.waiting, why, sizeof(why));
		OBVR_LOG("Yield: at the mark - reversals %u (right %u, left %u), the last reset by: %s; hand curls %.2f/%.2f, "
		         "palms to the front %.2f/%.2f, under the eyes %.2f/%.2f m, ahead %.2f/%.2f m; under the head: %08X, in "
		         "combat %d",
		         state.yield.reversals, state.yield.hand[0].reversals, state.yield.hand[1].reversals, why,
		         static_cast<double>(in.hand[0].curl), static_cast<double>(in.hand[1].curl),
		         static_cast<double>(in.hand[0].palmAhead), static_cast<double>(in.hand[1].palmAhead),
		         static_cast<double>(-in.hand[0].position.z), static_cast<double>(-in.hand[1].position.z),
		         static_cast<double>(in.hand[0].position.y), static_cast<double>(in.hand[1].position.y),
		         reinterpret_cast<UInt32>(ahead), enemyAhead ? 1 : 0);
	}
	if (fired) {
		state.yieldTarget = enemy;
		state.yieldActor = reinterpret_cast<UInt32>(ahead);
		const bool readied = game::ReadPlayerWeaponState() == game::WeaponState::Drawn;
		state.yieldDrive = YieldDrive{};
		if (readied) {
			state.yieldDrive.phase = YieldPhase::Blocking;
			state.yieldDrive.framesLeft = kYieldHoldFrames;
		} else {
			state.yieldDrive.phase = YieldPhase::Readying;
			state.yieldDrive.raisedFists = true;
			state.yieldDrive.clickWanted = true;
		}
		if (state.yieldLines > 0) {
			--state.yieldLines;
			OBVR_LOG("Yield: the open hands rocked at someone in combat - %s, then block held and activate pressed on "
			         "them at %.0f %.0f %.0f (%s)",
			         readied ? "the weapon or the fists already up" : "the fists raised first for the engine's block",
			         static_cast<double>(enemy.x), static_cast<double>(enemy.y), static_cast<double>(enemy.z),
			         frame.meleeHeld ? "a weapon in hand" : (readied ? "the fists up" : "the hands empty"));
		}
	}
}

void StepYieldDrive(GesturesState& state, const HandShared& shared, float deltaSeconds) {
	vr::HandModeResult& hand = shared.hand;
if (state.yieldDrive.phase == YieldPhase::Readying) {
	state.yieldDrive.readySeconds += deltaSeconds;
	const bool up = game::ReadPlayerWeaponState() == game::WeaponState::Drawn &&
	                game::ReadPlayerAction() != vr::kPlayerActionEquipWeapon;
	if (up) {
		state.yieldDrive.phase = YieldPhase::Blocking;
		state.yieldDrive.framesLeft = kYieldHoldFrames;
		if (state.yieldLines > 0) {
			--state.yieldLines;
			OBVR_LOG("Yield: the fists are up after %.2f s - block held now", static_cast<double>(state.yieldDrive.readySeconds));
		}
	} else if (state.yieldDrive.readySeconds > kYieldReadySeconds) {
		state.yieldDrive = YieldDrive{};
		if (state.yieldLines > 0) {
			--state.yieldLines;
			OBVR_LOG("Yield: given up - the fists did not come up within %.1f s (weapon state %d, action %d)",
			         static_cast<double>(kYieldReadySeconds), static_cast<int>(game::ReadPlayerWeaponState()),
			         game::ReadPlayerAction());
		}
	}
} else if (state.yieldDrive.phase == YieldPhase::Blocking) {
	hand.controls.block = true;
	// The pick's target where the attacker is now.
	NiPoint3 centre{};
	float radius = 0.0f;
	if (mem::LooksLikeObjectAddress(state.yieldActor) && game::RefWorldBound(state.yieldActor, centre, radius)) {
		state.yieldTarget = centre;
	}
	if (state.yieldDrive.framesLeft <= kYieldActivateFrom && state.yieldDrive.framesLeft > kYieldActivateTo) {
		hand.controls.activate = true;
		if (state.yieldDrive.framesLeft == kYieldActivateFrom && state.yieldLines > 0) {
			// What the engine makes of the block by then, and whom the
			// pick has: the yield needs the player blocking and the
			// attacker under the crosshair (UESP Oblivion:Controls).
			--state.yieldLines;
			const game::CrosshairTarget target = game::ReadCrosshairTarget();
			OBVR_LOG("Yield: activate goes down after %u frames of block - the player's action %d, the crosshair "
			         "on %08X",
			         static_cast<unsigned>(kYieldHoldFrames - kYieldActivateFrom), game::ReadPlayerAction(),
			         target.haveRef ? target.refAddress : 0u);
		}
	}
	if (--state.yieldDrive.framesLeft == 0) {
		const bool lower = state.yieldDrive.raisedFists;
		state.yieldDrive = YieldDrive{};
		state.yieldDrive.clickWanted = lower;  // the fists raised for it lowered again
		if (lower && state.yieldLines > 0) {
			--state.yieldLines;
			OBVR_LOG("Yield: done - the fists raised for it are lowered again");
		}
	}
}
}

bool YieldPicking(const GesturesState& state) { return state.yieldDrive.phase == YieldPhase::Blocking; }

void UpdateInsult(GesturesState& state, const HandStep& step, const HandShared& shared,
                  const vr::HandModeFrame& frame, bool active) {
	const Config& config = step.config;
	const float dt = step.dt;
	vr::HandModeResult& hand = shared.hand;
	const vr::HandSettings& hands = config.hands;
	if (!active || !hands.middleFinger || !shared.cyclopeanCameraValid) {
		state.insult[0] = game::InsultState{};
		state.insult[1] = game::InsultState{};
		return;
	}
	const NiMatrix33& camRot = shared.cyclopeanCamera.rot;
	const NiPoint3& camPos = shared.cyclopeanCamera.pos;
	for (int side = 0; side < 2; ++side) {
		const bool left = side == 1;
		const bool tracked = left ? (frame.left.valid && hand.leftHandValid) : (frame.right.valid && hand.rightHandValid);
		const bool gesture = tracked && game::InsultGesture(left ? hand.leftCurlValid : hand.rightCurlValid,
		                                                    left ? hand.leftCurl : hand.rightCurl);
		void* actor = nullptr;
		if (gesture) {
			float pitch = 0.0f;
			float yaw = 0.0f;
			shared.laserAngles(hands, left, pitch, yaw);
			const vr::LaserWorldRay ray = vr::HandLaserWorldRay(
				camRot, camPos, left ? hand.leftHandRotation : hand.rightHandRotation,
				left ? hand.leftHandOffsetUnits : hand.rightHandOffsetUnits, pitch, yaw, hands.laserOriginMetres,
				config.tracker.unitsPerMetre,
				vr::LaserOffsetLocal(hands.laserOffsetRightMetres, hands.laserOffsetUpMetres, left));
			actor = game::ActorUnderRay(ray.origin, ray.direction, game::kInsultReachUnits, game::kInsultConeCos,
			                            nullptr);
			// No need to aim the laser at them (the tester, 2026-10-07:
			// "mittelfinger muss aber auch gehen ohne dass ich mit dem pointer
			// auf npcs zeigen muss"): whoever is ahead of the head within 60
			// degrees, else the nearest living one within 3 m.
			if (actor == nullptr) {
				const NiPoint3 forward{camRot.data[0][1], camRot.data[1][1], camRot.data[2][1]};
				actor = game::ActorUnderRay(camPos, forward, game::kInsultReachUnits, 0.5f, nullptr);
			}
			if (actor == nullptr) {
				actor = game::LivingActorAt(camPos, 0.0f, 210.0f, nullptr);
			}
		}
		if (game::StepInsult(state.insult[side], gesture, actor, dt)) {
			game::ChangeDisposition(actor, -hands.middleFingerDisposition);
			if (state.insultLines > 0) {
				--state.insultLines;
				OBVR_LOG("Insult: the %s hand's middle finger at %08X - their disposition -%.0f", left ? "left" : "right",
				         reinterpret_cast<UInt32>(actor), static_cast<double>(hands.middleFingerDisposition));
			}
		}
	}
}

}  // namespace obvr::camera::hands
