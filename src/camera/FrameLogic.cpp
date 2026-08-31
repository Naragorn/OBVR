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
	if (!visibility.onlyWhenNeeded || visibility.thirdPerson) {
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

bool AimPitchWanted(bool enabled, bool headsetConnected, bool isThirdPerson, bool menuIsUp) {
	return enabled && headsetConnected && !isThirdPerson && !menuIsUp;
}

bool AimYawWanted(bool enabled, bool headsetConnected, bool isThirdPerson, bool menuIsUp,
                  bool attacking) {
	return AimPitchWanted(enabled, headsetConnected, isThirdPerson, menuIsUp) && attacking;
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

}  // namespace obvr::camera
