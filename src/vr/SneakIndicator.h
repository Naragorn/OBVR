#pragma once

#include "core/ChoiceWord.h"
#include "core/MathFns.h"
#include "core/Types.h"
#include "game/NiMath.h"

namespace obvr::vr {

// Sneaking without the crosshair (Nadi, 2026-10-28: "auch hier crosshair WEG
// das sneak auge soll nun mittig oben ... als fallback bauen wir 1. als
// default 4. (vignette hat selbe farbe beim heller werden wie 1.) im
// onboarding und settings sollen dann user entscheiden").
//
// The game draws its sneak eye in the reticle's place: the HUDReticle tile
// "hudreticle_sneak" (hud_reticle.xml, Menus\Icons\icon_small_eye.dds),
// whose alpha the game sets - 0 standing, 50 sneaking unseen, up to 255 seen
// (the harness's tile tree, 2026-10-28). In Full VR the crosshair no longer
// shows it; one of two takes its place ([Hands] SneakIndicator):
//
//   * vignette (the default): a ring at the edge of the view in the eye's
//     own gold, its strength the eye's alpha - faint while unseen, bright
//     once seen. Meta's guidance keeps status off the head
//     (VRC.Quest.Functional.10: "Headlocked menus and UI elements are
//     generally uncomfortable ... should be avoided for ... status
//     information"); a vignette is the edge of the view, not a thing in it.
//   * eye: the game's eye itself on its own quad above the view, following
//     the head lazily - inside a dead zone it stays, outside it eases after
//     ("loosely follow the user using smoothing", the same guidance).
enum class SneakIndicator : UInt8 {
	Vignette = 0,
	Eye = 1,
};
inline constexpr UInt32 kSneakIndicatorCount = 2;
inline constexpr const char* kSneakIndicatorNames[kSneakIndicatorCount] = {"vignette", "eye"};
inline constexpr SneakIndicator kSneakIndicatorDefault = SneakIndicator::Vignette;

inline bool ParseSneakIndicator(const char* text, SneakIndicator& out) {
	UInt32 index = 0;
	if (!MatchChoiceWord(text, kSneakIndicatorNames, kSneakIndicatorCount, index)) {
		return false;
	}
	out = static_cast<SneakIndicator>(index);
	return true;
}

inline SneakIndicator SneakIndicatorFromIndex(float value) {
	if (!(value > -0.5f) || !(value < static_cast<float>(kSneakIndicatorCount) - 0.5f)) {
		return kSneakIndicatorDefault;
	}
	return static_cast<SneakIndicator>(static_cast<int>(value + 0.5f));
}

// The eye's gold, as the capture shows it (its brightest pixels' mean,
// 79:73:50 at alpha 50, 2026-10-28), at full brightness.
inline constexpr float kSneakEyeRed = 1.0f;
inline constexpr float kSneakEyeGreen = 0.924f;
inline constexpr float kSneakEyeBlue = 0.633f;

// The eye's alpha while sneaking unseen (the tile tree, 2026-10-28) - taken
// when the tile cannot be read, so sneaking is still shown.
inline constexpr float kSneakEyeUnknownAlpha = 50.0f;

struct SneakIndicatorInput {
	bool fullVr = false;
	bool firstPerson = false;
	bool sneaking = false;
	bool menuUp = false;
	bool worldFrame = false;
	// hudreticle_sneak's alpha, 0..255; NaN or negative: not read.
	float eyeAlpha = 0.0f;
};

struct SneakIndicatorVerdict {
	bool vignette = false;
	float vignetteStrength = 0.0f;  // 0..maxStrength
	bool eye = false;               // the eye's quad above the view
};

inline SneakIndicatorVerdict StepSneakIndicator(const SneakIndicatorInput& in, SneakIndicator choice,
                                                float maxStrength) {
	SneakIndicatorVerdict v;
	if (!in.fullVr || !in.firstPerson || !in.sneaking || in.menuUp || !in.worldFrame) {
		return v;
	}
	if (choice == SneakIndicator::Eye) {
		v.eye = true;
		return v;
	}
	if (!(maxStrength >= 0.0f)) {
		return v;
	}
	float alpha = in.eyeAlpha >= 0.0f ? in.eyeAlpha : kSneakEyeUnknownAlpha;
	if (alpha > 255.0f) {
		alpha = 255.0f;
	}
	v.vignette = true;
	v.vignetteStrength = maxStrength * (alpha / 255.0f);
	return v;
}

// A unit direction `up`-wards of `forward` by `degrees` (both unit, at right
// angles): where the eye is wanted above the view.
inline NiPoint3 AboveView(const NiPoint3& forward, const NiPoint3& up, float degrees) {
	const float r = degrees * (3.14159265f / 180.0f);
	const float c = math::Cos(r);
	const float s = math::Sin(r);
	return NiPoint3{forward.x * c + up.x * s, forward.y * c + up.y * s, forward.z * c + up.z * s};
}

// One frame of the eye's lazy follow: `current` stays while within
// deadDegrees of `target`, and otherwise eases towards it at `rate` per
// second (exponential, so a long frame arrives and never overshoots).
// No usable current direction: the target at once. No target: the current.
inline NiPoint3 StepLazyDirection(const NiPoint3& current, const NiPoint3& target, float deadDegrees, float rate,
                                  float dtSeconds) {
	const float targetLength = math::Sqrt(target.LengthSquared());
	if (!(targetLength > 1.0e-6f)) {
		return current;
	}
	const NiPoint3 t = target * (1.0f / targetLength);
	const float currentLength = math::Sqrt(current.LengthSquared());
	if (!(currentLength > 1.0e-6f)) {
		return t;
	}
	const NiPoint3 c = current * (1.0f / currentLength);
	const float cosine = c.x * t.x + c.y * t.y + c.z * t.z;
	if (cosine >= math::Cos(deadDegrees * (3.14159265f / 180.0f))) {
		return c;
	}
	float k = rate * (dtSeconds > 0.0f ? dtSeconds : 0.0f);
	k = 1.0f - math::Exp(-k);
	const NiPoint3 mixed = c + (t - c) * k;
	const float mixedLength = math::Sqrt(mixed.LengthSquared());
	return mixedLength > 1.0e-6f ? mixed * (1.0f / mixedLength) : t;
}

}  // namespace obvr::vr
