#include "camera/FrameLogic.h"

#include "core/MathFns.h"

namespace obvr::camera {

bool KeyEdge::Update(bool isDown) {
	const bool isEdge = isDown && !m_wasDown;
	m_wasDown = isDown;
	return isEdge;
}

void KeyEdge::Reset() { m_wasDown = false; }

PovEvent State::ObservePointOfView(bool thirdPerson) {
	if (!sawCameraNode) {
		sawCameraNode = true;
		isThirdPerson = thirdPerson;
		return PovEvent::FirstPass;
	}

	if (thirdPerson == isThirdPerson) {
		return PovEvent::Unchanged;
	}

	isThirdPerson = thirdPerson;
	return PovEvent::Switched;
}

float FrameClock::Tick(long long nowTicks, long long ticksPerSecond) {
	const long long lastTicks = m_lastTicks;
	const bool hadLast = m_hasLast;

	m_lastTicks = nowTicks;
	m_hasLast = true;

	if (!hadLast || ticksPerSecond <= 0) {
		return 0.0f;
	}

	const long long elapsed = nowTicks - lastTicks;
	if (elapsed <= 0) {
		// The counter is monotonic, so this means the caller handed in a
		// stale reading. Reporting a negative frame time would run the
		// smoothing backwards.
		return 0.0f;
	}

	const float seconds =
		static_cast<float>(static_cast<double>(elapsed) / static_cast<double>(ticksPerSecond));

	return seconds > kMaxDeltaSeconds ? kMaxDeltaSeconds : seconds;
}

void FrameClock::Reset() {
	m_lastTicks = 0;
	m_hasLast = false;
}

bool IsDue(UInt32 frameCount, UInt32 interval) {
	if (interval == 0) {
		return false;
	}
	return (frameCount % interval) == 0;
}

bool IsLeftEyeFrame(UInt32 frameCount) {
	// Even frames to the left eye. Which one goes first does not matter, only
	// that the two places asking get the same answer for the same frame.
	return (frameCount & 1u) == 0u;
}

bool FirstPassDrawsLeftEye(bool swapEyeOrder) { return !swapEyeOrder; }

bool BackBufferEyeIsLeft(bool isLeftEye, bool backBufferIsThisFrame) {
	return backBufferIsThisFrame ? isLeftEye : !isLeftEye;
}

float ScaledEyeHalfSeparation(float halfUnits, float scale) {
	// Written as a negated comparison on purpose: NaN fails every comparison,
	// so this one line rejects NaN, zero and negatives together.
	if (!(scale > 0.0f)) {
		return halfUnits;
	}
	if (scale < kMinEyeSeparationScale) {
		scale = kMinEyeSeparationScale;
	}
	if (scale > kMaxEyeSeparationScale) {
		scale = kMaxEyeSeparationScale;
	}
	return halfUnits * scale;
}

FrameDelivery DeliverFrame(bool hadCameraPass, bool menuIsUp, bool menusInWorld,
                           bool haveHeldEyes, UInt32 worldlessStreak) {
	// Nothing drew a world, so there is no viewpoint to claim and no stereo
	// pair to hold. Videos, loading screens and the main menu land here, and
	// they land here whatever menusInWorld says.
	if (!hadCameraPass && !menuIsUp) {
		// Except for stray frames: the seam that closes a menu, the gap a
		// dialogue's exit transition leaves - the world missing for a frame
		// or two, not gone. Screening those is a flash of the flat picture in
		// the middle of otherwise smooth play, and it fills the eye copies
		// with the letterboxed layout, which is where black bars come from.
		// The streak cap is what lets a real video through. See the header.
		if (haveHeldEyes && worldlessStreak < kWorldlessBridgeFrames) {
			return FrameDelivery::HeldStereo;
		}
		return FrameDelivery::Cinema;
	}

	if (!menuIsUp) {
		return FrameDelivery::Stereo;
	}

	if (!menusInWorld) {
		return FrameDelivery::Cinema;
	}

	// A menu in the world, on a frame where Oblivion redrew what is behind it.
	if (hadCameraPass) {
		return FrameDelivery::Stereo;
	}

	// A menu in the world on a frame where it did not. Holding is the whole
	// point: switching to the cinema screen here is the flicker.
	//
	// Unless there is nothing to hold - the main menu, or a menu opened before
	// the first pair was ever captured. Then the screen is the only picture
	// available, and a picture beats the test pattern.
	return haveHeldEyes ? FrameDelivery::HeldStereo : FrameDelivery::Cinema;
}

bool MenusCanReachTheWorld(bool menusInWorld, bool hudOverlay) {
	return menusInWorld && hudOverlay;
}

bool LiveMenuBackgroundCanRun(bool enabled, bool renderToHeadset, bool showMenus,
                              bool headsetConnected, bool dualPass,
                              bool menusInWorld, bool hudOverlay,
                              bool hudBetweenPasses, bool cameraStandIn) {
	return enabled && renderToHeadset && showMenus && headsetConnected && dualPass &&
	       menusInWorld && hudOverlay && hudBetweenPasses && cameraStandIn;
}

MenuFrameDressing MenuDressingForFrame(FrameDelivery delivery, bool menuIsUp,
                                       bool liveStereoFrame,
                                       UInt32 framesSinceMenuOpened,
                                       bool shadeEnabled, bool singleBorderEnabled) {
	if (!menuIsUp) {
		return MenuFrameDressing{};
	}

	if (delivery == FrameDelivery::HeldStereo) {
		return MenuDressingWanted(framesSinceMenuOpened)
		           ? MenuFrameDressing{shadeEnabled, singleBorderEnabled}
		           : MenuFrameDressing{};
	}

	if (delivery == FrameDelivery::Stereo && liveStereoFrame) {
		return MenuFrameDressing{shadeEnabled, false};
	}

	return MenuFrameDressing{};
}

bool MenuWorldProbeWanted(bool probeEnabled, FrameDelivery delivery, bool menuIsUp,
                          UInt32 attemptsLeft) {
	return probeEnabled && delivery != FrameDelivery::Stereo && menuIsUp &&
	       attemptsLeft > 0;
}

bool WorldControlProbeWanted(bool probeEnabled, bool menuIsUp, bool hadCameraPass,
                             UInt32 attemptsLeft) {
	return probeEnabled && !menuIsUp && hadCameraPass && attemptsLeft > 0;
}

bool MenuFrameNeedsCameraStandIn(bool enabled, bool stereoDual, bool menuIsUp,
                                 bool headsetConnected, bool haveCameraBase,
                                 bool alreadyRanThisFrame, bool engineDrewThisFrame) {
	return enabled && stereoDual && menuIsUp && headsetConnected && haveCameraBase &&
	       !alreadyRanThisFrame && !engineDrewThisFrame;
}

EyeStep StereoEyeStep(float half, bool firstIsLeft) {
	// Left is negative along the head's x axis, so the first eye takes the
	// sign of the eye it is, and the step to the other one is the whole
	// separation back the other way.
	const float first = firstIsLeft ? -half : half;
	return EyeStep{first, -2.0f * first};
}

bool CrosshairWanted(const CrosshairVisibility& visibility) {
	if (!visibility.enabled || !visibility.worldFrame || visibility.menuIsUp) {
		return false;
	}
	const bool onlyWhenNeeded = visibility.thirdPerson ? visibility.onlyWhenNeededThirdPerson
	                                                   : visibility.onlyWhenNeeded;
	if (!onlyWhenNeeded) {
		return true;
	}
	return visibility.somethingAimedAt || visibility.weaponDrawn;
}

CrosshairPlacement PlaceCrosshair(float distanceMetres, float sizeAtOneMetre) {
	float distance = distanceMetres;
	if (distance < kCrosshairNearestMetres) {
		distance = kCrosshairNearestMetres;
	} else if (distance > kCrosshairFarthestMetres) {
		distance = kCrosshairFarthestMetres;
	}

	float size = sizeAtOneMetre;
	if (size < kCrosshairSmallestAtOneMetre) {
		size = kCrosshairSmallestAtOneMetre;
	} else if (size > kCrosshairLargestAtOneMetre) {
		size = kCrosshairLargestAtOneMetre;
	}

	// Multiplied after the clamps, not before: clamping the width instead
	// would tie the two together, so a distance the player raised would come
	// back as a crosshair that also changed size.
	return CrosshairPlacement{distance, size * distance};
}

bool BorrowedCrosshairWanted(bool thirdPerson, bool enabled, bool somethingAimedAt,
                             bool sneaking) {
	if (!thirdPerson || !enabled) {
		return false;
	}
	return !somethingAimedAt && !sneaking;
}

bool CrosshairTargetReadWanted(bool dynamicDepth, bool onlyWhenNeeded,
                               bool probeEnabled, bool thirdPersonBorrowing) {
	return dynamicDepth || onlyWhenNeeded || probeEnabled || thirdPersonBorrowing;
}

UInt32 CrosshairSourcePixels(UInt32 believedHeight, float sharePercent) {
	if (believedHeight == 0) {
		return 0;
	}

	float share = sharePercent;
	if (!(share > kCrosshairSourceSmallestShare)) {
		// Written so a NaN lands here rather than passing through: it fails the
		// comparison, and a NaN carried into the rectangle would give a square
		// with no corners and a lift that silently does nothing.
		share = kCrosshairSourceSmallestShare;
	} else if (share > kCrosshairSourceLargestShare) {
		share = kCrosshairSourceLargestShare;
	}

	UInt32 pixels = static_cast<UInt32>(static_cast<float>(believedHeight) * share / 100.0f);

	// Even, because the square is centred by halving it. An odd size would sit
	// half a pixel off centre, which is invisible on its own and exactly the
	// sort of thing that leaves a one-pixel line of the old crosshair behind.
	pixels &= ~1u;

	// A floor, so a tiny picture still lifts something rather than nothing. The
	// caller treats zero as "do not lift", and that answer is reserved for a
	// height of zero, which means the size is not known yet.
	if (pixels < 8) {
		pixels = 8;
	}
	return pixels;
}

float CrosshairDepth(const CrosshairDepthInput& input) {
	if (!input.haveTarget) {
		return input.fallbackMetres;
	}

	// Nonsense units would divide a number of units by a number that is not a
	// scale and produce a depth in nothing at all. The INI is written by hand.
	if (!(input.unitsPerMetre > 0.0f)) {
		return input.fallbackMetres;
	}

	// The gaze comes out of a rotation matrix and is orthonormal in practice,
	// but it is normalised rather than assumed to be: an unnormalised gaze
	// scales the projection silently, which would read as the crosshair sitting
	// at a plausible but consistently wrong depth - the hardest kind of fault
	// to notice from inside a headset.
	const float gazeLengthSquared = input.gazeDirection.LengthSquared();
	if (!(gazeLengthSquared > 1.0e-6f)) {
		return input.fallbackMetres;
	}
	const float gazeLength = math::Sqrt(gazeLengthSquared);

	const NiPoint3 toTarget = input.targetPosition - input.cameraPosition;
	const float alongGaze = (toTarget.x * input.gazeDirection.x +
	                         toTarget.y * input.gazeDirection.y +
	                         toTarget.z * input.gazeDirection.z) / gazeLength;

	// Behind the camera. Reachable in third person, where the reference under
	// the crosshair can be nearer the camera than the player is, and reachable
	// for a frame whenever the reference outlives the look that found it. A
	// negative depth would put the quad behind the wearer's head.
	//
	// Zero is refused with it, and deliberately: a target exactly in the eye
	// plane is not a depth the crosshair can be placed at either.
	if (!(alongGaze > 0.0f)) {
		return input.fallbackMetres;
	}

	return alongGaze / input.unitsPerMetre;
}

bool LayoutProbeDue(bool probeEnabled, UInt32 presentedFrame, UInt32 lastProbeFrame) {
	return probeEnabled && presentedFrame - lastProbeFrame >= kLayoutProbeFrameGap;
}

bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp, bool menusInWorld) {
	return frameOpen && armed && (!menuIsUp || menusInWorld);
}

bool WantsHudRedirect(FrameDelivery delivery) {
	// One question, asked once. The layer may be taken out of the frame
	// exactly when the frame is not the thing being shown - and the cinema
	// screen shows the frame, so on those the layer has to stay in it.
	//
	// This used to be its own three-argument decision, and the two drifted the
	// moment the redirect learned that menu frames carry no camera pass: the
	// main menu has no captured pair to hold, so it falls back to the cinema
	// screen, while the redirect went on taking its 2D layer away. The menu was
	// then in neither place, and the game looked hung at the title screen.
	//
	// Derived from the delivery instead, that cannot happen again: there is no
	// second rule left to disagree with the first.
	return delivery != FrameDelivery::Cinema;
}

UInt32 SweepProbeStage(UInt32 sceneCall, UInt32 configured) {
	if (configured != kProbeSweep) {
		return configured;
	}
	if (sceneCall < kSweepFirstBand) {
		return kProbeSinglePass;
	}
	if (sceneCall < kSweepSecondBand) {
		return 1;
	}
	if (sceneCall < kSweepThirdBand) {
		return kProbeSinglePass;
	}
	return 0;
}

bool WantsSecondScenePass(bool frameOpen, bool armed, bool menuIsUp, bool menusInWorld,
                          UInt32 probeRung) {
	return probeRung != kProbeSinglePass &&
	       WantsSecondScenePass(frameOpen, armed, menuIsUp, menusInWorld);
}

bool DeliversDualEyes(bool stereoDual, bool sceneHooked, UInt32 probeRung) {
	return stereoDual && sceneHooked && probeRung != kProbeSinglePass;
}

bool AimPitchWanted(bool enabled, bool headsetConnected, bool isThirdPerson,
                    bool thirdPersonAllowed, bool menuIsUp, bool attacking) {
	if (!enabled || !headsetConnected || menuIsUp) {
		return false;
	}
	// First person writes on every frame; third person only while aiming, and
	// only with the switch on - see the header for the measurement behind it.
	return !isThirdPerson || (thirdPersonAllowed && attacking);
}

bool AimYawWanted(bool enabled, bool headsetConnected, bool isThirdPerson,
                  bool thirdPersonAllowed, bool menuIsUp, bool attacking) {
	if (!enabled || !headsetConnected || menuIsUp || !attacking) {
		return false;
	}
	return !isThirdPerson || thirdPersonAllowed;
}

float ChaseStep(float chased, float target, float rate) {
	// The shortest way round, so a turn that wraps past the half circle is
	// still closed by the short arc rather than the long one.
	return math::WrapAngle(chased + math::WrapAngle(target - chased) * rate);
}

float MeasuredChaseRate(const ChaseRateInput& input) {
	if (!input.haveBefore) {
		return input.fallbackRate;
	}
	const float remaining = math::WrapAngle(input.target - input.cameraBefore);
	if (remaining > -kChaseMinRemainingRadians && remaining < kChaseMinRemainingRadians) {
		return input.fallbackRate;
	}
	const float moved = math::WrapAngle(input.cameraNow - input.cameraBefore);
	const float rate = moved / remaining;
	// A camera that moved the other way, or past its target, is something
	// other than the easing - a script, a collision, a wrap. Neither is a
	// share of the aim.
	if (rate < 0.0f) {
		return 0.0f;
	}
	return rate > 1.0f ? 1.0f : rate;
}

float AimCameraShare(bool isThirdPerson, float bodyOffset, float chased) {
	return isThirdPerson ? chased : bodyOffset;
}

float PlayerYawForGaze(float engineYaw, float stepRadians) {
	float turned = engineYaw - stepRadians;

	// Into 0..2pi. Written as loops rather than as a remainder because the
	// input is one turn out at the very most - the step is never more than half
	// a circle - and a remainder on a negative float is where sign conventions
	// differ between compilers.
	while (turned < 0.0f) {
		turned += math::kTwoPi;
	}
	while (turned >= math::kTwoPi) {
		turned -= math::kTwoPi;
	}
	return turned;
}

float AimYawRemaining(float headYaw, float bodyOffset) {
	return math::WrapAngle(headYaw - bodyOffset);
}

bool AimTurnDue(AimTurnMode mode, bool attackHeld, bool attackWasHeld, bool attackInProgress) {
	if (mode == AimTurnMode::WhileAiming) {
		return attackHeld;
	}

	// OnShot. The window opens on the release - the frame the shot is started -
	// and stays open while the attack runs, because that is when the arrow is
	// made and the heading it is made with is the one it flies along.
	//
	// Held is deliberately NOT included. The whole point is that the draw
	// leaves the body alone, so somebody can hold a bow at full stretch,
	// looking one way and walking another, for as long as they like.
	if (attackHeld) {
		return false;
	}
	return (attackWasHeld && !attackHeld) || attackInProgress;
}

bool AimReturnWanted(bool enabled, bool headsetConnected, bool menuIsUp, bool attackHeld,
                     float secondsSinceRelease, bool attackInProgress, bool aimStanding) {
	if (!enabled || !headsetConnected) {
		return false;
	}

	// Nothing to give back. Also the ordinary case for almost every frame, so
	// it is checked before anything else that could be surprising.
	if (!aimStanding) {
		return false;
	}

	// Still aiming, or never was. A negative count means the control is held or
	// the last release has already been dealt with.
	if (attackHeld || secondsSinceRelease < 0.0f) {
		return false;
	}

	// Not into a menu. Writing the player's heading while the world is paused
	// is the same refusal the aiming half makes, and for the same reason: the
	// engine is not going to rebuild the camera from it until play resumes, so
	// the compensation and the heading would sit disagreeing until it did.
	if (menuIsUp) {
		return false;
	}

	// THE WAIT. The arrow has not left yet, and the heading it leaves along is
	// whatever the heading is at that moment - so nothing may move until the
	// game says the attack is done.
	//
	// The limit is what keeps a wrong action value from disabling the feature
	// rather than merely delaying it. It is generous on purpose: it should
	// never be what ends the wait during ordinary play, and if the log says it
	// is, the action values are what to look at.
	if (attackInProgress && secondsSinceRelease < kAimReturnLimitSeconds) {
		return false;
	}
	return true;
}

bool YawWriteLanded(float wroteYaw, float engineYawNow, float stepTaken) {
	const float step = stepTaken < 0.0f ? -stepTaken : stepTaken;
	if (step < kYawLandingMinStep) {
		return true;
	}

	// How far the field has moved away from what OBVR left in it. The wrap is
	// what keeps a heading either side of north from reading as most of a
	// circle apart.
	const float moved = math::WrapAngle(engineYawNow - wroteYaw);
	const float distance = moved < 0.0f ? -moved : moved;

	// Half the step as the dividing line, rather than a fixed tolerance. The
	// two outcomes are "still here" and "back where it was", and those are
	// exactly `step` apart - so half of it separates them with the most room on
	// either side, whatever the size of the step. It also leaves the mouse's
	// own movement on the correct side: a mouse moving fast enough to cross
	// that line within one frame is turning faster than the step it is being
	// compared against.
	return distance < step * 0.5f;
}

float PlayerPitchForGaze(float viewSinPitch) {
	const float radians = -math::Asin(viewSinPitch);
	if (radians > kAimPitchLimitRadians) {
		return kAimPitchLimitRadians;
	}
	if (radians < -kAimPitchLimitRadians) {
		return -kAimPitchLimitRadians;
	}
	return radians;
}

CastWindow NextCastWindow(const CastWindow& current, const CastWindowInput& input,
                          float minimumSeconds) {
	if (!input.enabled) {
		return CastWindow{};
	}

	CastWindow next = current;

	// The press. The only moment about a cast that is certain, so it is the
	// only thing allowed to open the window.
	if (input.castHeld && !input.castWasHeld) {
		next.open = true;
		next.secondsOpen = 0.0f;
		return next;
	}

	if (!next.open) {
		return next;
	}

	next.secondsOpen += input.deltaSeconds;

	// The limit, checked before anything can argue with it. A flag stuck true
	// cannot hold the body turned past this.
	if (next.secondsOpen >= kCastWindowLimitSeconds) {
		return CastWindow{};
	}

	// Below the insurance minimum the window stays open whatever else says,
	// because a cast key is a tap and the flag may say nothing at all.
	if (next.secondsOpen < minimumSeconds) {
		return next;
	}

	// Past it, something has to be holding it open: the key still down, or the
	// action field saying the spell has not left yet. The moment it reads
	// FollowThrough instead, the spell has gone and the body is free - which is
	// the same line the bow's window is drawn on, and for the same reason.
	if (input.castHeld || input.spellStillLeaving) {
		return next;
	}

	return CastWindow{};
}

float NextReleaseClock(float current, const ReleaseClockInput& input) {
	// Still aiming. Held, or the key window turning the body itself - either
	// way nothing has been let go of yet.
	if (input.attackHeld || input.castTurning) {
		return kReleaseClockIdle;
	}
	// Let go. The cast half reaches here two ways: the key window closing, back
	// when the window itself turned the body, and a spell's turn coming due -
	// which is the moment the body is handed to the machinery that holds the
	// turn until the action field says the spell has left.
	// is what hands the body back to the bow's machinery.
	if (input.attackWasHeld || (input.castWasOpen && input.castFeedsClock) ||
	    input.castTurnDue) {
		return 0.0f;
	}

	if (current >= 0.0f) {
		return current + input.deltaSeconds;
	}

	return kReleaseClockIdle;
}

NiPoint3 AimArcCorrection(const AimArcInput& input) {
	if (!input.centreKnown || input.bodyOffset == 0.0f) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}

	// The arm: from the point the body turns about to where the eye stands,
	// flattened, because the turn is about the vertical and cannot lift or
	// lower anything.
	const float armX = input.cameraPosition.x - input.turnCentre.x;
	const float armY = input.cameraPosition.y - input.turnCentre.y;

	// A camera standing on the axis is not swung by the turn and needs nothing
	// putting back. Worth its own exit rather than falling out of the
	// arithmetic, because it is also what a centre read from a torn-down actor
	// would look like, and doing nothing is the right answer to both.
	if (armX == 0.0f && armY == 0.0f) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}

	// Backwards by the offset - the same rotation the base gets, written the
	// same way so the two can only ever be wrong together.
	const float cosine = math::Cos(input.bodyOffset);
	const float sine = -math::Sin(input.bodyOffset);

	const float turnedX = armX * cosine - armY * sine;
	const float turnedY = armX * sine + armY * cosine;

	return NiPoint3{turnedX - armX, turnedY - armY, 0.0f};
}


CastArmDecision NextCastArm(const CastArm& current, const CastArmInput& input) {
	CastArmDecision decision;
	decision.next = current;

	// A new cast rearms, whatever was standing. Two casts cannot overlap - the
	// animation of the first has to end before the second can start - so the
	// later one is simply the one being watched.
	if (input.castBegan) {
		decision.next = CastArm{};
		decision.next.seconds = 0.0f;
		return decision;
	}

	if (decision.next.seconds < 0.0f) {
		return decision;
	}

	decision.next.seconds += input.deltaSeconds;

	// The safety rail first, so a cast that never reports an end cannot leave
	// this armed for the rest of the run.
	if (decision.next.seconds > input.limitSeconds) {
		decision.missed = !decision.next.turned;
		decision.next = CastArm{};
		return decision;
	}

	if (input.actionIsAttack) {
		decision.next.sawAttack = true;
		if (!decision.next.turned && decision.next.seconds >= input.turnAfterSeconds) {
			decision.turnNow = true;
			decision.next.turned = true;
		}
		return decision;
	}

	// Not attacking. Either the animation has not started yet - the field reads
	// None for a frame or two after the cast - or it has ended, and only having
	// seen Attack tells the two apart.
	if (decision.next.sawAttack) {
		decision.measuredSeconds = decision.next.seconds;
		decision.missed = !decision.next.turned;
		decision.next = CastArm{};
	}
	return decision;
}

AimPitchHoldDecision NextAimPitchHold(const AimPitchHold& current,
                                      const AimPitchHoldInput& input) {
	AimPitchHoldDecision decision;
	decision.next = current;

	// The mouse's own tilt. While the field is held, this frame's movement is
	// whatever the engine added to what was last there; otherwise the field
	// is the mouse's and nothing else.
	if (current.held) {
		decision.next.mouseTilt =
			current.mouseTilt + (input.enginePitch - current.fieldNow);
	} else {
		decision.next.mouseTilt = input.enginePitch;
	}

	if (input.writeGaze) {
		decision.write = true;
		decision.value = input.gazePitch;
		decision.next.held = true;
		decision.next.fieldNow = input.gazePitch;
	} else if (current.held && input.returnDue) {
		decision.write = true;
		decision.value = decision.next.mouseTilt;
		decision.next.held = false;
		decision.next.fieldNow = decision.next.mouseTilt;
	} else {
		decision.next.fieldNow = input.enginePitch;
	}

	decision.offset = decision.next.held ? decision.next.fieldNow - decision.next.mouseTilt : 0.0f;
	return decision;
}

NiPoint3 AimTiltCorrection(const AimTiltInput& input) {
	if (!input.centreKnown || input.pitchShare == 0.0f) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}

	// Where the camera stands off the player's axis, and how far.
	const float armX = input.cameraPosition.x - input.feet.x;
	const float armY = input.cameraPosition.y - input.feet.y;
	const float horizontal = math::Sqrt(armX * armX + armY * armY);
	if (horizontal <= 0.0f) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}

	// The camera's own pitch, positive up, and the sphere read back from it:
	// the horizontal distance is r cos(pitch), so r follows, and the pivot is
	// r sin(pitch) above the camera when the camera looks up from below.
	const float pitch = math::Asin(input.cameraSinPitch);
	const float cosine = math::Cos(pitch);
	if (cosine < kAimTiltMinCosine) {
		return NiPoint3{0.0f, 0.0f, 0.0f};
	}
	const float radius = horizontal / cosine;
	const float pivotZ = input.cameraPosition.z + radius * math::Sin(pitch);

	// Where the camera would stand had it eased towards the mouse's tilt
	// alone. The share is in the engine's convention (positive down) and the
	// camera's pitch is positive up, so taking the share out ADDS it.
	const float wanted = pitch + input.pitchShare;
	const float wantedHorizontal = radius * math::Cos(wanted);
	const float wantedZ = pivotZ - radius * math::Sin(wanted);

	const float scale = wantedHorizontal / horizontal;
	return NiPoint3{input.feet.x + armX * scale - input.cameraPosition.x,
	                input.feet.y + armY * scale - input.cameraPosition.y,
	                wantedZ - input.cameraPosition.z};
}

bool AimTurnOwnsAction(bool aimAtSource, SInt32 action) {
	// Nothing at all once the source aim is in place: the bow, the swing and
	// the cast are all read inside the one call it wraps. Otherwise the set
	// IsShotUnreleased has always used - written here as plain numbers so this
	// stays a function over values: 2 Attack, 4 AttackBow, 5 ArrowAttached.
	// FollowThrough (3) is never in it; the projectile has gone by then.
	if (aimAtSource) {
		return false;
	}
	return action == 2 || action == 4 || action == 5;
}

bool AimAtSourceWanted(bool enabled, bool aimAtSource, bool headsetConnected, bool menuIsUp,
                       bool viewAimed) {
	return enabled && aimAtSource && headsetConnected && !menuIsUp && viewAimed;
}

bool AimSourceSwapDue(bool wanted, bool isPlayer, SInt32 action) {
	if (!wanted || !isPlayer) {
		return false;
	}
	// Attack (2) is the swing and the cast alike; the bow's release key comes
	// with the arrow still on the string (5). AttackBow (4) is the draw, whose
	// keys make nothing.
	return action == 2 || action == 5;
}

ThirdPersonAimVisualDecision NextThirdPersonAimVisual(
	const ThirdPersonAimVisualState& current, const ThirdPersonAimVisualInput& input) {
	ThirdPersonAimVisualDecision decision;

	// Written as positive tests so NaN is refused as surely as a negative
	// percentage. A value above 100 is accepted but clamped: an INI typo must
	// not twist the skeleton farther than the gaze itself.
	if (!input.enabled || !input.aimAtSource || !input.headsetConnected ||
	    !input.isThirdPerson || !input.thirdPersonAllowed || input.menuIsUp ||
	    !(input.percent > 0.0f)) {
		return decision;
	}

	decision.next = current;

	// A ready weapon owns the pose continuously, so a bow never straightens
	// between shots and a sword is already facing the gaze before the swing.
	// Controls cover the frame before HighProcess has caught up, especially for
	// magic, and the action field keeps short presses alive through animation.
	const bool live = input.bodyWithoutWeapon || input.weaponDrawn ||
	                  input.attackHeld || input.castActive ||
	                  input.action == 2 || input.action == 4 || input.action == 5;
	if (live) {
		decision.next.active = true;
		decision.next.gazeYaw = input.gazeYaw;
		decision.next.gazePitch = -input.playerPitch;
	} else if (input.action != 3 || !decision.next.active) {
		// FollowThrough (3) keeps the last live direction. Every other action,
		// and a follow-through first observed without its attack, resets.
		decision.next = ThirdPersonAimVisualState{};
	}

	if (!decision.next.active) {
		return decision;
	}

	const float fraction = input.percent >= 100.0f ? 1.0f : input.percent * 0.01f;
	decision.write = true;
	decision.yaw = decision.next.gazeYaw * fraction;
	decision.pitch = decision.next.gazePitch * fraction;
	return decision;
}

bool ThirdPersonHeadVisualWanted(bool enabled, bool headFollowsGaze,
	                             bool headsetConnected, bool isThirdPerson,
	                             bool thirdPersonAllowed, bool menuIsUp) {
	return enabled && headFollowsGaze && headsetConnected && isThirdPerson &&
	       thirdPersonAllowed && !menuIsUp;
}

bool LooksLikeReturnAddress(UInt32 value, UInt32 textStart, UInt32 textEnd,
                            const UInt8 preceding[6]) {
	if (value < textStart || value >= textEnd) {
		return false;
	}
	// call rel32: E8 xx xx xx xx, five bytes.
	if (preceding[1] == 0xE8) {
		return true;
	}
	// call reg: FF D0..D7, two bytes.
	if (preceding[4] == 0xFF && preceding[5] >= 0xD0 && preceding[5] <= 0xD7) {
		return true;
	}
	// call [reg+disp8]: FF 50..57 xx, and call [sib]: FF 14 xx, three bytes.
	if (preceding[3] == 0xFF &&
	    ((preceding[4] >= 0x50 && preceding[4] <= 0x57) || preceding[4] == 0x14)) {
		return true;
	}
	// call [reg+disp32]: FF 90..97 + 4, and call [disp32]: FF 15 + 4, six bytes.
	if (preceding[0] == 0xFF &&
	    ((preceding[1] >= 0x90 && preceding[1] <= 0x97) || preceding[1] == 0x15)) {
		return true;
	}
	return false;
}

}  // namespace obvr::camera
